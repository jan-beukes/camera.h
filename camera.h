/* 
    Simple header-only wrapper around the v4l2 api for working with camera devices on linux.
    most of the code used to be able to capture frames comes from this example:
    https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/capture.c.html

   Do this:
      #define CAMERA_IMPLEMENTATION
   before you include this file in *one* C or C++ file to create the implementation.

// Webcam capture example using raylib:

#include <raylib.h>
#include "camera.h"

int main(void)
{
    Cam_Format fmt = { 0 };
    fmt.width = 1280;
    fmt.height = 720;
    fmt.pixelformat = V4L2_PIX_FMT_MJPEG;

    // camera_open will set fmt, our values are not guaranteed to work
    if (!camera_open(NULL, &fmt, 0)) return 1;
    assert(fmt.pixelformat == V4L2_PIX_FMT_MJPEG);

    SetTraceLogLevel(LOG_WARNING);
    InitWindow(fmt.width, fmt.height, "video for linux capture");
    if (!camera_begin()) return 1;

    Texture frame;
    while (!WindowShouldClose()) {
        Cam_Buffer buf;
        if (camera_get_frame(&buf, NULL)) {
            // this allocates every frame, ideally we have a single
            // output buffer where we can write the contents of buf
            // to for format conversion
            Image img = LoadImageFromMemory(".JPG", buf.ptr, buf.length);
            if (IsTextureValid(frame)) {
                UpdateTexture(frame, img.data);
            } else {
                frame = LoadTextureFromImage(img);
            }
            UnloadImage(img);
        }

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexture(frame, 0, 0, WHITE);
        EndDrawing();
    }

    camera_end();
    camera_close();
    CloseWindow();
    return 0;
}

TODO:
- Add framerate options

- Built in format conversions for YUYV and MJPEG to rgb24 surface.
    Then add seperate camera_get_frame(Cam_Surface *surf) and
    camera_get_frame_raw(Cam_Buffer *buf) functions.
    If the format could not be converted it could just log 
    CAM_ERROR("Could not convert format to rgb, Use camera_get_frame_raw").

- Access to more data from camera devices (list devices, get device info, etc.)

*/

#ifndef CAMERA_H
#define CAMERA_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>

#ifdef __cplusplus
extern "C" {
#endif

// NOTE check the pixel format with the V4L2_PIX_FMT_XXX macros
typedef struct v4l2_pix_format Cam_Format;

typedef struct {
    void *ptr;
    size_t length;
} Cam_Buffer;

enum cam_log_level {
    CAM_LOG_INFO,
    CAM_LOG_WARN,
    CAM_LOG_ERROR,
    CAM_LOG_NONE,
};

enum cam_io_method {
    IO_METHOD_MMAP,
    IO_METHOD_READ,
};

// Initialize the camera device.
// if NULL is provided for device, it open's /dev/video0.
// if fmt is provided it tries to set the format to the specified values.
// NOTE: fmt WILL be set by this function to the actual format
bool camera_open(const char *device, Cam_Format *fmt,
        enum cam_io_method io);
bool camera_begin();
// try to get the next frame and set buf.
// this uses select to wait for a read, if timeout is not NULL it will be used
bool camera_get_frame(Cam_Buffer *buf, struct timeval *timeout);
bool camera_end();
bool camera_close();
void set_min_log_level(enum cam_log_level level);

#ifdef __cplusplus
}
#endif

/************************
* CAMERA IMPLEMENTATION
*************************/
#ifdef CAMERA_IMPLEMENTATION

#define __CLEAR(x) memset(&(x), 0, sizeof(x))

static struct {
    const char *dev_name;
    enum cam_io_method io;
    enum cam_log_level minloglevel;
    int fd;
    Cam_Buffer *buffers;
    unsigned int n_buffers;
    int out_buf;
    Cam_Format pix_fmt;
} _cam_state = {
    .dev_name = "/dev/video0",
    .io = IO_METHOD_MMAP,
    .minloglevel = CAM_LOG_INFO,
    .fd = -1,
};


#define CAM_INFO(fmt, ...) if (_cam_state.minloglevel <= CAM_LOG_INFO) \
    printf("[INFO] "fmt"\n", ##__VA_ARGS__)

#define CAM_WARN(fmt, ...) if (_cam_state.minloglevel <= CAM_LOG_WARN) \
    fprintf(stderr, "[WARN] "fmt"\n", ##__VA_ARGS__)

#define CAM_ERROR(fmt, ...) if (_cam_state.minloglevel <= CAM_LOG_ERROR) \
    fprintf(stderr, "[ERROR] "fmt"\n", ##__VA_ARGS__)


void set_min_log_level(enum cam_log_level level)
{
    _cam_state.minloglevel = level;
}

static int _xioctl(int fh, int request, void *arg)
{
    int r;
    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);

    return r;
}

static bool _read_frame(Cam_Buffer *cam_buf)
{
    struct v4l2_buffer buf;
    unsigned int i;

    switch (_cam_state.io) {
    case IO_METHOD_READ:
        if (read(_cam_state.fd, _cam_state.buffers[0].ptr, 
                    _cam_state.buffers[0].length) == -1) {
            switch (errno) {
                case EAGAIN:
                    break;

                case EIO:
                    /* Could ignore EIO, see spec. */
                default:
                    CAM_ERROR("reading from device");
            }
            return false;
        }

        *cam_buf = _cam_state.buffers[0];
        break;

    case IO_METHOD_MMAP:
        __CLEAR(buf);

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if (_xioctl(_cam_state.fd, VIDIOC_DQBUF, &buf) == -1) {
            switch (errno) {
                case EAGAIN:
                    break;

                case EIO:
                    /* Could ignore EIO, see spec. */
                default:
                    CAM_ERROR("VIDIOC_DQBUF");
            }
            return false;
        }

        assert(buf.index < _cam_state.n_buffers);

        *cam_buf = _cam_state.buffers[buf.index];

        if (_xioctl(_cam_state.fd, VIDIOC_QBUF, &buf) == -1) {
            CAM_ERROR("VIDIOC_QBUF");
            return false;
        }
        break;
    }

    return true;
}

static void _init_io_read(unsigned int buffer_size)
{
    _cam_state.buffers = calloc(1, sizeof(*_cam_state.buffers));

    _cam_state.buffers[0].length = buffer_size;
    _cam_state.buffers[0].ptr = malloc(buffer_size);
}

static bool _init_io_mmap(void)
{
    struct v4l2_requestbuffers req;

    __CLEAR(req);

    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (_xioctl(_cam_state.fd, VIDIOC_REQBUFS, &req) == -1) {
        if (errno == EINVAL) {
            CAM_ERROR("%s does not support memory mapping",
                    _cam_state.dev_name);
            return false;
        } else {
            CAM_ERROR("VIDIOC_REQBUFS");
            return false;
        }
    }

    if (req.count < 2) {
        CAM_ERROR("Insufficient buffer memory on %s", _cam_state.dev_name);
        return false;
    }

    _cam_state.buffers = calloc(req.count, sizeof(*_cam_state.buffers));

    for (_cam_state.n_buffers = 0; _cam_state.n_buffers < req.count;
            ++_cam_state.n_buffers) {
        struct v4l2_buffer buf;

        __CLEAR(buf);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = _cam_state.n_buffers;

        if (_xioctl(_cam_state.fd, VIDIOC_QUERYBUF, &buf) == -1) {
            CAM_ERROR("VIDIOC_QUERYBUF");
            return false;
        }

        _cam_state.buffers[_cam_state.n_buffers].length = buf.length;
        _cam_state.buffers[_cam_state.n_buffers].ptr = mmap(
                NULL, /* start anywhere */
                buf.length,
                PROT_READ | PROT_WRITE, /* required */
                MAP_SHARED,  /* recommended */
                _cam_state.fd, 
                buf.m.offset
                );

        if (_cam_state.buffers[_cam_state.n_buffers].ptr == MAP_FAILED) {
            CAM_ERROR("mmap");
            return false;
        }
    }

    return true;
}

bool camera_open(const char *device, Cam_Format *pix_fmt, enum cam_io_method io)
{
    struct stat st;

    if (!pix_fmt) {
        CAM_ERROR("pix_fmt cannot be NULL");
        return false;
    }
    if (device)
        _cam_state.dev_name = device;

    if (stat(_cam_state.dev_name, &st) == -1) {
        CAM_ERROR("Cannot identify '%s':  %s",
                _cam_state.dev_name, strerror(errno));
        return false;
    }

    if (!S_ISCHR(st.st_mode)) {
        CAM_ERROR("%s is not device", _cam_state.dev_name);
        return false;
    }

    _cam_state.fd = open(_cam_state.dev_name,
            O_RDWR /* required */ | O_NONBLOCK, 0);
    if (_cam_state.fd == -1) {
        CAM_ERROR("Cannot open '%s': %s", _cam_state.dev_name,
                strerror(errno));
        return false;
    }
    
    // Camera Initialization
    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format fmt;
    unsigned int min;

    _cam_state.io = io;

    if (_xioctl(_cam_state.fd, VIDIOC_QUERYCAP, &cap) == -1) {
        if (errno == EINVAL) {
            CAM_ERROR("%s is not V4L2 device", _cam_state.dev_name);
            return false;
        } else {
            CAM_ERROR("VIDIOC_QUERYCAP");
            return false;
        }
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        CAM_ERROR("%s is not video capture device", _cam_state.dev_name);
        return false;
    }

    switch (_cam_state.io) {
        case IO_METHOD_READ:
            if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
                CAM_ERROR("%s does not support read i/o",
                _cam_state.dev_name);
                return false;
            }
            break;

        case IO_METHOD_MMAP:
            if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
                CAM_ERROR("%s does not support streaming i/o",
                        _cam_state.dev_name);
                return false;
            }
            break;
    }

    /* Select video input, video standard and tune here. */
    __CLEAR(cropcap);
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (_xioctl(_cam_state.fd, VIDIOC_CROPCAP, &cropcap) == 0) {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect; /* reset to default */

        if (_xioctl(_cam_state.fd, VIDIOC_S_CROP, &crop) == -1) {
            switch (errno) {
                case EINVAL:
                    /* Cropping not supported. */
                    break;
                default:
                    /* Errors ignored. */
                    break;
                }
        }
    }

    __CLEAR(fmt);

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (pix_fmt->width || pix_fmt->height || pix_fmt->pixelformat ||
        pix_fmt->colorspace) {
        fmt.fmt.pix = *pix_fmt;
        /* NOTE VIDIOC_S_FMT may change width and height. */
        if (_xioctl(_cam_state.fd, VIDIOC_S_FMT, &fmt) == -1) {
            CAM_ERROR("VIDIOC_S_FMT");
            return false;
        }
    } else {
        /* Preserve original settings as set by v4l2-ctl for example */
        if (_xioctl(_cam_state.fd, VIDIOC_G_FMT, &fmt) == -1) {
            CAM_ERROR("VIDIOC_G_FMT");
            return false;
        }
    }
    _cam_state.pix_fmt = fmt.fmt.pix;
    *pix_fmt = fmt.fmt.pix;

    /* Buggy driver paranoia. */
    min = fmt.fmt.pix.width * 2;
    if (fmt.fmt.pix.bytesperline < min)
        fmt.fmt.pix.bytesperline = min;
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min)
        fmt.fmt.pix.sizeimage = min;

    bool ret = true;
    switch (_cam_state.io) {
        case IO_METHOD_READ:
            _init_io_read(_cam_state.pix_fmt.sizeimage);
            break;
        case IO_METHOD_MMAP:
            ret = _init_io_mmap();
            break;
    }

    CAM_INFO("Device '%s' opened", _cam_state.dev_name);
    // extract the 4cc from pixelformat
    char fmt_name[5];
    memcpy(fmt_name, &pix_fmt->pixelformat, sizeof(pix_fmt->pixelformat));
    fmt_name[4] = '\0';
    CAM_INFO("Format: %dx%d %s", pix_fmt->width, pix_fmt->height, fmt_name);

    return ret;
}
bool camera_begin()
{
    unsigned int i;
    enum v4l2_buf_type type;

    switch (_cam_state.io) {
        case IO_METHOD_READ:
            /* Nothing to do. */
            break;

        case IO_METHOD_MMAP:
            for (i = 0; i < _cam_state.n_buffers; ++i) {
                struct v4l2_buffer buf;

                __CLEAR(buf);
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = i;

                if (_xioctl(_cam_state.fd, VIDIOC_QBUF, &buf) == -1) {
                    CAM_ERROR("VIDIOC_QBUF");
                    return false;
                }
            }
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (_xioctl(_cam_state.fd, VIDIOC_STREAMON, &type) == -1) {
                CAM_ERROR("VIDIOC_STREAMON");
                return false;
            }
            break;
    }

    return true;
}

#define DEFUALT_TV_US 30000
bool camera_get_frame(Cam_Buffer *buf, struct timeval *timeout)
{
    fd_set fds;
    struct timeval tv;
    int r;

    if (!buf) return false;

    FD_ZERO(&fds);
        FD_SET(_cam_state.fd, &fds);

    if (timeout) {
        tv = *timeout;
    } else {
        tv.tv_sec = 0;
        tv.tv_usec = DEFUALT_TV_US;
    }

    r = select(_cam_state.fd + 1, &fds, NULL, NULL, &tv);
    if (r == -1) {
        CAM_ERROR("select");
        return false;
    }
    if (r == 0) return false;

    return _read_frame(buf);
}

bool camera_end()
{
    enum v4l2_buf_type type;

    switch (_cam_state.io) {
        case IO_METHOD_READ:
            /* Nothing to do. */
            break;
        case IO_METHOD_MMAP:
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (_xioctl(_cam_state.fd, VIDIOC_STREAMOFF, &type) == -1) {
                CAM_ERROR("VIDIOC_STREAMOFF");
                return false;
            }
            break;
    }

    return true;
}

bool camera_close()
{
    unsigned int i;

    switch (_cam_state.io) {
        case IO_METHOD_READ:
            free(_cam_state.buffers[0].ptr);
            break;

        case IO_METHOD_MMAP:
            for (i = 0; i < _cam_state.n_buffers; ++i)
                if (-1 == munmap(_cam_state.buffers[i].ptr,
                            _cam_state.buffers[i].length)) {
                    CAM_ERROR("munmap");
                    return false;
                }
            break;
    }

    free(_cam_state.buffers);

    // close device
    if (close(_cam_state.fd) == -1) {
        CAM_ERROR("close");
        return false;
    }

    _cam_state.fd = -1;
    return true;
}

#endif // CAMERA_IMPLEMENTATION
#endif // CAMERA_H
