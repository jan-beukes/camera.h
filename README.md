# camera.h - Simple linux camera api

A simple header-only library to easily work with webcams/cameras through v4l2.

This will never give as much control as using the API (or libv4l) directly.

## Example use with raylib:
```c
#include <raylib.h>
#define CAMERA_IMPLEMENTATION
#include "camera.h"

int main(void)
{
    Cam_Format fmt = {0};
    fmt.width = 640, fmt.height = 480;
    fmt.pixelformat = V4L2_PIX_FMT_YUYV;

    // camera_open will set fmt, our values are not guaranteed to work
    if (!camera_open(NULL, &fmt, 0)) return 1;
    assert(fmt.pixelformat == V4L2_PIX_FMT_YUYV);

    SetTraceLogLevel(LOG_WARNING);
    InitWindow(fmt.width, fmt.height, "video for linux capture");
    if (!camera_begin()) return 1;

    Texture frame;
    while (!WindowShouldClose()) {
        Cam_Surface surf;
        if (camera_get_frame(&surf, NULL)) {
            assert(surf.pixelformat == CAM_PIX_FMT_RGB24);
            if (IsTextureValid(frame)) {
                UpdateTexture(frame, surf.data);
            } else {
                Image img = {
                    .data = surf.data,
                    .width = surf.width,
                    .height = surf.height,
                    .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8,
                    .mipmaps = 1,
                };
                frame = LoadTextureFromImage(img);
            }
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
```
