package main

import "core:fmt"
import cam "camera"
import rl "vendor:raylib"

main :: proc() {
	format: cam.Format = { width = 800, height = 600 }
	format.pixelformat = .YUYV
	if !cam.open("/dev/video0", &format, .MMAP) {
		fmt.eprintln("Could not open camera")
		return
	}
	defer cam.close()

	assert(format.pixelformat == .YUYV)

	rl.InitWindow(i32(format.width), i32(format.height), "Odin linux webcam")
	defer rl.CloseWindow()

	if !cam.begin() {
		fmt.eprintln("Could not start camera")
	}
	defer cam.end()

	frame: rl.Texture
	for !rl.WindowShouldClose() {
		surf: cam.Surface
		if cam.get_frame(&surf, nil) {
			assert(surf.pixelformat == .RGB24)
			if rl.IsTextureValid(frame) {
				rl.UpdateTexture(frame, surf.data)
			} else {
				frame = rl.LoadTextureFromImage({
					data = surf.data,
					width = i32(surf.width),
					height = i32(surf.height),
					format = .UNCOMPRESSED_R8G8B8,
					mipmaps = 1,
				})
			}
		}

		rl.BeginDrawing()
		rl.ClearBackground(rl.BLACK)
		rl.DrawTexture(frame, 0, 0, rl.WHITE)
		rl.EndDrawing()
	}
}
