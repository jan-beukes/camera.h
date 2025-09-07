#include <raylib.h>
#define CAMERA_IMPLEMENTATION
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

