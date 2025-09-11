/* 
    Simple header-only wrapper around the v4l2 api for working with camera devices on linux.
    most of the code used to be able to capture frames comes from this example:
    https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/capture.c.html

   Do this:
      #define CAMERA_IMPLEMENTATION
   before you include this file in *one* C or C++ file to create the implementation.

   Currently only a single global camera device can be handled at a time.
*/

#ifndef CAMERA_H
#define CAMERA_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
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

typedef enum {
    CAM_INFO,
    CAM_WARN,
    CAM_ERROR,
    CAM_NONE,
} Cam_LogLevel;

typedef enum {
    IO_METHOD_MMAP,
    IO_METHOD_READ,
} Cam_IoMethod;

#define CAM_PIX_FMT_RGB24 V4L2_PIX_FMT_RGB24 
// This is just a typedef when working with V4L2_PIX_FMT_*
typedef unsigned int Cam_PixelFormat;

// Mostly just a wrapper for v4l2_pix_format data so that all format info
// can be put into a single struct
typedef struct {
    size_t width;
    size_t height;
    size_t stride;
    size_t sizeimage;
    Cam_PixelFormat pixelformat;
    // TODO: unsigned int fps;
} Cam_Format;

typedef struct {
    void *data;
    size_t width;
    size_t height;
    // This will either be CAM_PIX_FMT_RGB24 or one of the V4L2_PIX_FMT_XXX
    Cam_PixelFormat pixelformat;
} Cam_Surface;

typedef struct {
    void *ptr;
    size_t length;
} Cam_Buffer;

// Initialize the camera device.
//
// if NULL is provided for device, it open's /dev/video0.
// if fmt is provided it tries to set the format to the specified values.
//
// NOTE: fmt WILL be set by this function to the actual format
bool camera_open(const char *device, Cam_Format *cam_fmt, Cam_IoMethod io);
bool camera_begin();

// try to load the next frame into surf.
//
// this uses select to wait for a read, if timeout is NULL it will wait for
// CAM_DEFAULT_TIMEOUT_US, this can be defined to any value before including
// camera.h
//
// If the obtained format of surf is one that can be automatically converted
// to rgb then it will be returned as CAM_PIX_FMT_RGB24.
// This can be disabled with #define CAM_NO_COVERT_TO_RGB
bool camera_get_frame(Cam_Surface *surf, struct timeval *timeout);
bool camera_get_frame_raw(Cam_Buffer *buf, struct timeval *timeout);

bool camera_end();
bool camera_close();
void set_min_log_level(Cam_LogLevel level);

#ifdef __cplusplus
}
#endif

/************************
* CAMERA IMPLEMENTATION
*************************/
#ifdef CAMERA_IMPLEMENTATION

#define __CLEAR(x) memset(&(x), 0, sizeof(x))

// Internal camera state
static struct {
    const char *dev_name;
    Cam_IoMethod io;
    int fd;
    Cam_Format fmt;
    struct v4l2_capability capability;

    Cam_Buffer *buffers;
    unsigned int n_buffers;
    unsigned char *rgb_buffer;
    size_t rgb_buffer_size;

    Cam_LogLevel min_log_level;
} _cam_state = {
    .dev_name = "/dev/video0",
    .io = IO_METHOD_MMAP,
    .fd = -1,
    .min_log_level = CAM_INFO,
};


void set_min_log_level(Cam_LogLevel level)
{
    _cam_state.min_log_level = level;
}


static void cam_log(Cam_LogLevel level, const char *fmt, va_list args)
{
    if (level < _cam_state.min_log_level) return;
    FILE *stream = stderr;

    switch (level) {
        case CAM_INFO:
            stream = stdout;
            fprintf(stream, "[INFO] ");
            break;
        case CAM_WARN:
            fprintf(stream, "[WARN] ");
            break;
        case CAM_ERROR:
            fprintf(stream, "[ERROR] ");
            break;
        default:
            // nada
    }

    vfprintf(stream, fmt, args);
    fprintf(stream, "\n");
}

static void cam_info(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    cam_log(CAM_INFO, fmt, args);
    va_end(args);
}

static void cam_warn(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    cam_log(CAM_WARN, fmt, args);
    va_end(args);

}

static void cam_error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    cam_log(CAM_ERROR, fmt, args);
    va_end(args);
}

static int _xioctl(int fh, unsigned int request, void *arg)
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
                    cam_error("reading from device");
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
                    cam_error("VIDIOC_DQBUF");
            }
            return false;
        }

        assert(buf.index < _cam_state.n_buffers);

        *cam_buf = _cam_state.buffers[buf.index];

        if (_xioctl(_cam_state.fd, VIDIOC_QBUF, &buf) == -1) {
            cam_error("VIDIOC_QBUF");
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
            cam_error("%s does not support memory mapping",
                    _cam_state.dev_name);
            return false;
        } else {
            cam_error("Could not request buffers");
            return false;
        }
    }

    if (req.count < 2) {
        cam_error("Insufficient buffer memory on %s", _cam_state.dev_name);
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
            cam_error("Could not query buffer");
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
            cam_error("mmap");
            return false;
        }
    }

    return true;
}

bool camera_open(const char *device, Cam_Format *cam_fmt, Cam_IoMethod io)
{
    struct stat st;

    if (!cam_fmt) {
        cam_error("cam_fmt cannot be NULL");
        return false;
    }

    if (device)
        _cam_state.dev_name = device;

    if (stat(_cam_state.dev_name, &st) == -1) {
        cam_error("Cannot identify '%s':  %s",
                _cam_state.dev_name, strerror(errno));
        return false;
    }

    if (!S_ISCHR(st.st_mode)) {
        cam_error("%s is not device", _cam_state.dev_name);
        return false;
    }

    _cam_state.fd = open(_cam_state.dev_name,
            O_RDWR /* required */ | O_NONBLOCK, 0);
    if (_cam_state.fd == -1) {
        cam_error("Cannot open '%s': %s", _cam_state.dev_name,
                strerror(errno));
        return false;
    }
    
    // Camera Initialization
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;

    _cam_state.io = io;

    if (_xioctl(_cam_state.fd, VIDIOC_QUERYCAP, &cap) == -1) {
        if (errno == EINVAL) {
            cam_error("%s is not V4L2 device", _cam_state.dev_name);
            return false;
        } else {
            cam_error("Could not query capabilities");
            return false;
        }
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        cam_error("%s is not video capture device", _cam_state.dev_name);
        return false;
    }

    _cam_state.capability = cap;

    switch (_cam_state.io) {
        case IO_METHOD_READ:
            if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
                cam_error("%s does not support read i/o",
                _cam_state.dev_name);
                return false;
            }
            break;

        case IO_METHOD_MMAP:
            if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
                cam_error("%s does not support streaming i/o",
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

    if (cam_fmt->width || cam_fmt->height || cam_fmt->pixelformat) {
        fmt.fmt.pix.width = cam_fmt->width;
        fmt.fmt.pix.height = cam_fmt->height;
        fmt.fmt.pix.pixelformat = cam_fmt->pixelformat;

        /* NOTE VIDIOC_S_FMT may change width and height. */
        if (_xioctl(_cam_state.fd, VIDIOC_S_FMT, &fmt) == -1) {
            cam_error("Could not set format for '%s'", _cam_state.dev_name);
            return false;
        }
    } else {
        /* Preserve original settings as set by v4l2-ctl for example */
        if (_xioctl(_cam_state.fd, VIDIOC_G_FMT, &fmt) == -1) {
            cam_error("Could not get format for '%s'", _cam_state.dev_name);
            return false;
        }
    }

    /* Buggy driver paranoia. */
    unsigned int min = fmt.fmt.pix.width * 2;
    if (fmt.fmt.pix.bytesperline < min)
        fmt.fmt.pix.bytesperline = min;
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min)
        fmt.fmt.pix.sizeimage = min;


    // store the camera's pixel format in global state and set the user's ptr
    // since VIDIOC_S_FMT might have set different values
    // TODO: set/get framerate
    _cam_state.fmt = (Cam_Format){
        .width = fmt.fmt.pix.width,
        .height = fmt.fmt.pix.height,
        .sizeimage = fmt.fmt.pix.sizeimage,
        .stride = fmt.fmt.pix.bytesperline, // idk if this is always valid
        .pixelformat = fmt.fmt.pix.pixelformat,
    };
    *cam_fmt = _cam_state.fmt;

#ifndef CAM_NO_COVERT_TO_RGB
    // XXX: this should be reallocated if changing format after input is
    //      ever implemented
    _cam_state.rgb_buffer_size = _cam_state.fmt.width *
        _cam_state.fmt.height * 3;
    _cam_state.rgb_buffer = malloc(_cam_state.rgb_buffer_size);
    memset(_cam_state.rgb_buffer, 0, _cam_state.rgb_buffer_size);
#endif

    // Allocate buffers and set initialize IO
    bool ret = true;
    switch (_cam_state.io) {
        case IO_METHOD_READ:
            _init_io_read(_cam_state.fmt.sizeimage);
            break;
        case IO_METHOD_MMAP:
            ret = _init_io_mmap();
            break;
    }


    cam_info("Device '%s' opened", _cam_state.dev_name);
    cam_info("Model: %s", cap.card);
    // extract the 4cc from pixelformat
    char fmt_name[5];
    memcpy(fmt_name, &cam_fmt->pixelformat, sizeof(cam_fmt->pixelformat));
    fmt_name[4] = '\0';
    cam_info("Format: %dx%d %s", cam_fmt->width, cam_fmt->height, fmt_name);

    switch (_cam_state.fmt.pixelformat) {
        case V4L2_PIX_FMT_YUYV:
            break;
        default:
            cam_warn("Can not convert %s to RGB24", fmt_name);
    }

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
                    cam_error("Could not query buffer");
                    return false;
                }
            }
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (_xioctl(_cam_state.fd, VIDIOC_STREAMON, &type) == -1) {
                cam_error("Could not turn on stream");
                return false;
            }
            break;
    }

    return true;
}

#ifndef CAM_DEFAULT_TIMEOUT_US
#define CAM_DEFAULT_TIMEOUT_US 33333
#endif
bool camera_get_frame_raw(Cam_Buffer *buf, struct timeval *timeout)
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
        tv.tv_usec = CAM_DEFAULT_TIMEOUT_US;
    }

    r = select(_cam_state.fd + 1, &fds, NULL, NULL, &tv);
    if (r == -1) {
        cam_error("select");
        return false;
    }
    if (r == 0) return false;

    return _read_frame(buf);
}

#define CLAMP(x) ((x) > 255 ? 255 : ((x) < 0 ? 0 : (x)))

// There is so little information about converting YUYV to rgb
// (or maybe search engines are just cooked)
// https://stackoverflow.com/a/72234444
static void yuyv_to_rgb(Cam_Surface *surf, Cam_Buffer *buf)
{
#define Y_OFFSET   16
#define UV_OFFSET 128
#define YUV2RGB_11  298
#define YUV2RGB_12   -1
#define YUV2RGB_13  409
#define YUV2RGB_22 -100
#define YUV2RGB_23 -210
#define YUV2RGB_32  519
#define YUV2RGB_33    0

    unsigned char *pixels   = _cam_state.rgb_buffer;
    unsigned char *yuyvdata = buf->ptr;
    int count = buf->length / 4; // 2 YUYV 16bit "pixels" per loop
    while (count--) {
        int y, u, v;
        int uv_r, uv_g, uv_b;

        u = yuyvdata[1] - UV_OFFSET;
        v = yuyvdata[3] - UV_OFFSET;
        uv_r = YUV2RGB_12*u + YUV2RGB_13*v;
        uv_g = YUV2RGB_22*u + YUV2RGB_23*v;
        uv_b = YUV2RGB_32*u + YUV2RGB_33*v;

        // 1st pixel
        y = YUV2RGB_11 * (yuyvdata[0] - Y_OFFSET);
        pixels[0] = CLAMP((y + uv_r) >> 8); // r
        pixels[1] = CLAMP((y + uv_g) >> 8); // g
        pixels[2] = CLAMP((y + uv_b) >> 8); // b
        pixels += 3;

        // 2nd pixel
        y = YUV2RGB_11*(yuyvdata[2] - Y_OFFSET);
        pixels[0] = CLAMP((y + uv_r) >> 8); // r
        pixels[1] = CLAMP((y + uv_g) >> 8); // g
        pixels[2] = CLAMP((y + uv_b) >> 8); // b
        pixels += 3;

        yuyvdata += 4;
    }

    surf->pixelformat = CAM_PIX_FMT_RGB24;
    surf->data = _cam_state.rgb_buffer;
}

bool camera_get_frame(Cam_Surface *surf, struct timeval *timeout)
{
    if (!surf) return false;

    Cam_Buffer buf = {0};
    if (!camera_get_frame_raw(&buf, timeout)) return false;

    surf->data = buf.ptr;
    surf->width = _cam_state.fmt.width;
    surf->height = _cam_state.fmt.height;
    surf->pixelformat = _cam_state.fmt.pixelformat;

#ifndef CAM_NO_COVERT_TO_RGB
    switch (surf->pixelformat) {
        case V4L2_PIX_FMT_YUYV: yuyv_to_rgb(surf, &buf); break;
        default:
            // Not supported
    }
#endif

    return true;
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
                cam_error("Could not turn off stream");
                return false;
            }
            break;
    }

    return true;
}

bool camera_close()
{
    unsigned int i;

    // free the buffers
    if (_cam_state.rgb_buffer)
        free(_cam_state.rgb_buffer);
    switch (_cam_state.io) {
        case IO_METHOD_READ:
            free(_cam_state.buffers[0].ptr);
            break;

        case IO_METHOD_MMAP:
            for (i = 0; i < _cam_state.n_buffers; ++i)
                if (-1 == munmap(_cam_state.buffers[i].ptr,
                            _cam_state.buffers[i].length)) {
                    cam_error("munmap");
                    return false;
                }
            break;
    }

    free(_cam_state.buffers);

    // close device
    if (close(_cam_state.fd) == -1) {
        cam_error("close");
        return false;
    }

    _cam_state.fd = -1;
    return true;
}

#endif // CAMERA_IMPLEMENTATION
#endif // CAMERA_H

/* 
    TODO:
    - Conversion for MJPG to RGB24.
    - Add framerate options
    - Change fmt while camera is open
        maybe VIDIOC_ENUM_FRAMESIZES, VIDIOC_ENUM_FMT, ...
    - multiple camera's by storing state in a Cam_Camera structure or something
*/
