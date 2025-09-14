#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#define CAMERA_IMPLEMENTATION
#include "camera.h"

#define LUMINANCE(r, g, b) (0.2126f*r + 0.7152f*g + 0.0722f*b)

#define ANSI_HIDE_CURSOR "\x1b[?25l"
#define ANSI_SHOW_CURSOR "\x1b[?25h"
#define ANSI_CLEAR_SCREEN "\x1b[H\x1b[2J"

unsigned short term_rows = 20;
unsigned short term_cols = 28;
fd_set fds;

void update_term_size(Cam_Format fmt)
{
    unsigned short width = fmt.width, height = fmt.height;
    struct winsize w;
    ioctl(1, TIOCGWINSZ, &w);
    term_rows = w.ws_row - 1;
    term_cols = w.ws_row * width / height;
}

bool should_quit(struct timeval *tv)
{
    FD_ZERO(&fds);
    FD_SET(0, &fds);

    int ret = select(1, &fds, NULL, NULL, tv);
    if (ret < 0) {
        if (errno == EINTR) return false;
        fprintf(stderr, "select: %s\n", strerror(errno));
        return true;
    } else if (ret > 0 && getchar() == EOF) return true;

    return false;
}

void handle_interupt_signal(int sig)
{
    (void)sig;

    // escape codes to reset terminal
    printf("\x1b[m");
    printf(ANSI_SHOW_CURSOR);
    printf(ANSI_CLEAR_SCREEN);

    // these just return false if camera is not open
    camera_end();
    camera_close();
    exit(0);
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <sorted ascii table>\n\n"
                "example:\n\t%s \".',:;xlxokXdO0KN\"", argv[0], argv[0]);
        return 1;
    }
    char *ascii_table = argv[1];
    size_t ascii_table_len = strlen(ascii_table) - 1;

    Cam_Format fmt = {0};
    fmt.pixelformat = V4L2_PIX_FMT_YUYV;
    camera_set_log_level(CAM_NONE);
    camera_open(NULL, &fmt, IO_METHOD_MMAP);

    update_term_size(fmt);
    signal(SIGINT, handle_interupt_signal);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 30000;

    camera_begin();
    printf(ANSI_HIDE_CURSOR);
    while (!should_quit(&tv)) {
        Cam_Surface surf;
        if (!camera_get_frame(&surf, &tv)) continue;
        assert(surf.pixelformat == CAM_PIX_FMT_RGB24);

        unsigned char *data = (unsigned char *)surf.data;
        printf(ANSI_CLEAR_SCREEN);
        for (size_t row = 0; row < term_rows; row++) {
            for (size_t col = 0; col < term_cols; col++) {
                size_t pixy = (row * surf.height/term_rows);
                size_t pixx = (col * surf.width/term_cols);
                size_t idx = 3 * (pixy*surf.width + pixx);

                unsigned char r = data[idx], g = data[idx+1], b = data[idx+2];
                size_t ascii_idx = ascii_table_len * (unsigned char)LUMINANCE(r, g, b)/255;
                char c = ascii_table[ascii_idx];
                printf("\x1b[38;2;%u;%u;%um%c%c", r, g, b, c, c);
            }
            printf("\n");
        }
        printf("\x1b[m");
    }
    // escape codes to reset terminal
    printf("\x1b[m");
    printf(ANSI_SHOW_CURSOR);
    printf(ANSI_CLEAR_SCREEN);

    camera_end();
    camera_close();

    return 0;
}
