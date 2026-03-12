/* 
    Simple header-only wrapper around the v4l2 api for working with camera devices on linux.
    most of the code used to be able to capture frames comes from this example:
    https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/capture.c.html

   Do this:
      #define CAMERA_IMPLEMENTATION
   before you include this file in *one* C or C++ file to create the implementation.

   Currently only a single global camera device can be handled at a time.
*/
package camera

import "core:c"

foreign import lib "camera.o"
_ :: lib

LogLevel :: enum u32 {
	INFO  = 0,
	WARN  = 1,
	ERROR = 2,
	NONE  = 3,
}

IoMethod :: enum u32 {
	MMAP = 0,
	READ = 1,
}

// Mostly just a wrapper for v4l2_pix_format data so that all format info
// can be put into a single struct
Format :: struct {
	width:       c.size_t,
	height:      c.size_t,
	stride:      c.size_t,
	sizeimage:   c.size_t,
	pixelformat: Pixel_Format,
}

Surface :: struct {
	data:   rawptr,
	width:  c.size_t,
	height: c.size_t,

	// This will either be CAM_PIX_FMT_RGB24 or one of the V4L2_PIX_FMT_XXX
	pixelformat: Pixel_Format,
}

Buffer :: struct {
	ptr:    rawptr,
	length: c.size_t,
}

@(default_calling_convention="c", link_prefix="camera_")
foreign lib {
	// Initialize the camera device.
	//
	// if NULL is provided for device, it opens /dev/video0.
	// if fmt is provided it tries to set the format to the specified values.
	//
	// NOTE: fmt WILL be set by this function to a valid format.
	// if the fields width, height or pixelformat are non-zero then it
	// will attempt to set the format using this structure
	open  :: proc(device: cstring, cam_fmt: ^Format, io: IoMethod) -> bool ---
	begin :: proc() -> bool ---

	// try to load the next frame into surf.
	//
	// this uses select to wait for a read, if timeout is NULL it will wait for
	// CAM_DEFAULT_TIMEOUT_US, this can be defined to any value before including
	// camera.h
	//
	// If the obtained format of surf is one that can be automatically converted
	// to rgb then it will be returned as CAM_PIX_FMT_RGB24.
	// This can be disabled with #define CAM_NO_COVERT_TO_RGB
	get_frame     :: proc(surf: ^Surface, timeout: ^timeval) -> bool ---
	get_frame_raw :: proc(buf: ^Buffer, timeout: ^timeval) -> bool ---
	end           :: proc() -> bool ---
	close         :: proc() -> bool ---
	set_log_level :: proc(level: LogLevel) ---
}

import "core:sys/posix"

timeval :: posix.timeval

Pixel_Format :: enum(u32) {
	// Pixel formats. FourCC from linux/videodev2.h
	RGB332 = ('R' | 'G' << 8 | 'B' << 16 | '1' << 24), /*  8  RGB-3-3-2     */
	RGB444 = ('R' | '4' << 8 | '4' << 16 | '4' << 24), /* 16  xxxxrrrr ggggbbbb */
	ARGB444 = ('A' | 'R' << 8 | '1' << 16 | '2' << 24), /* 16  aaaarrrr ggggbbbb */
	XRGB444 = ('X' | 'R' << 8 | '1' << 16 | '2' << 24), /* 16  xxxxrrrr ggggbbbb */
	RGBA444 = ('R' | 'A' << 8 | '1' << 16 | '2' << 24), /* 16  rrrrgggg bbbbaaaa */
	RGBX444 = ('R' | 'X' << 8 | '1' << 16 | '2' << 24), /* 16  rrrrgggg bbbbxxxx */
	ABGR444 = ('A' | 'B' << 8 | '1' << 16 | '2' << 24), /* 16  aaaabbbb ggggrrrr */
	XBGR444 = ('X' | 'B' << 8 | '1' << 16 | '2' << 24), /* 16  xxxxbbbb ggggrrrr */
	BGRA444 = ('G' | 'A' << 8 | '1' << 16 | '2' << 24), /* 16  bbbbgggg rrrraaaa */
	BGRX444 = ('B' | 'X' << 8 | '1' << 16 | '2' << 24), /* 16  bbbbgggg rrrrxxxx */
	RGB555 =  ('R' | 'G' << 8 | 'B' << 16 | 'O' << 24), /* 16  RGB-5-5-5     */
	ARGB555 = ('A' | 'R' << 8 | '1' << 16 | '5' << 24), /* 16  ARGB-1-5-5-5  */
	XRGB555 = ('X' | 'R' << 8 | '1' << 16 | '5' << 24), /* 16  XRGB-1-5-5-5  */
	RGBA555 = ('R' | 'A' << 8 | '1' << 16 | '5' << 24), /* 16  RGBA-5-5-5-1  */
	RGBX555 = ('R' | 'X' << 8 | '1' << 16 | '5' << 24), /* 16  RGBX-5-5-5-1  */
	ABGR555 = ('A' | 'B' << 8 | '1' << 16 | '5' << 24), /* 16  ABGR-1-5-5-5  */
	XBGR555 = ('X' | 'B' << 8 | '1' << 16 | '5' << 24), /* 16  XBGR-1-5-5-5  */
	BGRA555 = ('B' | 'A' << 8 | '1' << 16 | '5' << 24), /* 16  BGRA-5-5-5-1  */
	BGRX555 = ('B' | 'X' << 8 | '1' << 16 | '5' << 24), /* 16  BGRX-5-5-5-1  */
	RGB565 =  ('R' | 'G' << 8 | 'B' << 16 | 'P' << 24), /* 16  RGB-5-6-5     */
	RGB555X = ('R' | 'G' << 8 | 'B' << 16 | 'Q' << 24), /* 16  RGB-5-5-5 BE  */
	ARGB555X = ('A' | 'R' << 8 | '1' << 16 | '5' << 24), /* 16  ARGB-5-5-5 BE */
	XRGB555X = ('X' | 'R' << 8 | '1' << 16 | '5' << 24), /* 16  XRGB-5-5-5 BE */
	RGB565X = ('R' | 'G' << 8 | 'B' << 16 | 'R' << 24), /* 16  RGB-5-6-5 BE  */

	/* RGB formats (3 or 4 bytes per pixel) */
	BGR666 =  ('B' | 'G' << 8 | 'R' << 16 | 'H' << 24), /* 18  BGR-6-6-6	  */
	BGR24 =   ('B' | 'G' << 8 | 'R' << 16 | '3' << 24), /* 24  BGR-8-8-8     */
	RGB24 =   ('R' | 'G' << 8 | 'B' << 16 | '3' << 24), /* 24  RGB-8-8-8     */
	BGR32 =   ('B' | 'G' << 8 | 'R' << 16 | '4' << 24), /* 32  BGR-8-8-8-8   */
	ABGR32 =  ('A' | 'R' << 8 | '2' << 16 | '4' << 24), /* 32  BGRA-8-8-8-8  */
	XBGR32 =  ('X' | 'R' << 8 | '2' << 16 | '4' << 24), /* 32  BGRX-8-8-8-8  */
	BGRA32 =  ('R' | 'A' << 8 | '2' << 16 | '4' << 24), /* 32  ABGR-8-8-8-8  */
	BGRX32 =  ('R' | 'X' << 8 | '2' << 16 | '4' << 24), /* 32  XBGR-8-8-8-8  */
	RGB32 =   ('R' | 'G' << 8 | 'B' << 16 | '4' << 24), /* 32  RGB-8-8-8-8   */
	RGBA32 =  ('A' | 'B' << 8 | '2' << 16 | '4' << 24), /* 32  RGBA-8-8-8-8  */
	RGBX32 =  ('X' | 'B' << 8 | '2' << 16 | '4' << 24), /* 32  RGBX-8-8-8-8  */
	ARGB32 =  ('B' | 'A' << 8 | '2' << 16 | '4' << 24), /* 32  ARGB-8-8-8-8  */
	XRGB32 =  ('B' | 'X' << 8 | '2' << 16 | '4' << 24), /* 32  XRGB-8-8-8-8  */
	RGBX1010102 = ('R' | 'X' << 8 | '3' << 16 | '0' << 24), /* 32  RGBX-10-10-10-2 */
	RGBA1010102 = ('R' | 'A' << 8 | '3' << 16 | '0' << 24), /* 32  RGBA-10-10-10-2 */
	ARGB2101010 = ('A' | 'R' << 8 | '3' << 16 | '0' << 24), /* 32  ARGB-2-10-10-10 */

	/* RGB formats (6 or 8 bytes per pixel) */
	BGR48_12 =    ('B' | '3' << 8 | '1' << 16 | '2' << 24), /* 48  BGR 12-bit per component */
	BGR48 =       ('B' | 'G' << 8 | 'R' << 16 | '6' << 24), /* 48  BGR 16-bit per component */
	RGB48 =       ('R' | 'G' << 8 | 'B' << 16 | '6' << 24), /* 48  RGB 16-bit per component */
	ABGR64_12 =   ('B' | '4' << 8 | '1' << 16 | '2' << 24), /* 64  BGRA 12-bit per component */

	/* Grey formats */
	GREY =    ('G' | 'R' << 8 | 'E' << 16 | 'Y' << 24), /*  8  Greyscale     */
	Y4 =      ('Y' | '0' << 8 | '4' << 16 | ' ' << 24), /*  4  Greyscale     */
	Y6 =      ('Y' | '0' << 8 | '6' << 16 | ' ' << 24), /*  6  Greyscale     */
	Y10 =     ('Y' | '1' << 8 | '0' << 16 | ' ' << 24), /* 10  Greyscale     */
	Y12 =     ('Y' | '1' << 8 | '2' << 16 | ' ' << 24), /* 12  Greyscale     */
	Y012 =    ('Y' | '0' << 8 | '1' << 16 | '2' << 24), /* 12  Greyscale     */
	Y14 =     ('Y' | '1' << 8 | '4' << 16 | ' ' << 24), /* 14  Greyscale     */
	Y16 =     ('Y' | '1' << 8 | '6' << 16 | ' ' << 24), /* 16  Greyscale     */
	Y16_BE =  ('Y' | '1' << 8 | '6' << 16 | ' ' << 24), /* 16  Greyscale BE  */

	/* Grey bit-packed formats */
	Y10BPACK =    ('Y' | '1' << 8 | '0' << 16 | 'B' << 24), /* 10  Greyscale bit-packed */
	Y10P =    ('Y' | '1' << 8 | '0' << 16 | 'P' << 24), /* 10  Greyscale, MIPI RAW10 packed */
	IPU3_Y10 = 	('i' | 'p' << 8 | '3' << 16 | 'y' << 24), /* IPU3 packed 10-bit greyscale */
	Y12P =    ('Y' | '1' << 8 | '2' << 16 | 'P' << 24), /* 12  Greyscale, MIPI RAW12 packed */
	Y14P =    ('Y' | '1' << 8 | '4' << 16 | 'P' << 24), /* 14  Greyscale, MIPI RAW14 packed */

	/* Palette formats */
	PAL8 =    ('P' | 'A' << 8 | 'L' << 16 | '8' << 24), /*  8  8-bit palette */

	/* Chrominance formats */
	UV8 =     ('U' | 'V' << 8 | '8' << 16 | ' ' << 24), /*  8  UV 4:4 */

	/* Luminance+Chrominance formats */
	YUYV =    ('Y' | 'U' << 8 | 'Y' << 16 | 'V' << 24), /* 16  YUV 4:2:2     */
	YYUV =    ('Y' | 'Y' << 8 | 'U' << 16 | 'V' << 24), /* 16  YUV 4:2:2     */
	YVYU =    ('Y' | 'V' << 8 | 'Y' << 16 | 'U' << 24), /* 16 YVU 4:2:2 */
	UYVY =    ('U' | 'Y' << 8 | 'V' << 16 | 'Y' << 24), /* 16  YUV 4:2:2     */
	VYUY =    ('V' | 'Y' << 8 | 'U' << 16 | 'Y' << 24), /* 16  YUV 4:2:2     */
	Y41P =    ('Y' | '4' << 8 | '1' << 16 | 'P' << 24), /* 12  YUV 4:1:1     */
	YUV444 =  ('Y' | '4' << 8 | '4' << 16 | '4' << 24), /* 16  xxxxyyyy uuuuvvvv */
	YUV555 =  ('Y' | 'U' << 8 | 'V' << 16 | 'O' << 24), /* 16  YUV-5-5-5     */
	YUV565 =  ('Y' | 'U' << 8 | 'V' << 16 | 'P' << 24), /* 16  YUV-5-6-5     */
	YUV24 =   ('Y' | 'U' << 8 | 'V' << 16 | '3' << 24), /* 24  YUV-8-8-8     */
	YUV32 =   ('Y' | 'U' << 8 | 'V' << 16 | '4' << 24), /* 32  YUV-8-8-8-8   */
	AYUV32 =  ('A' | 'Y' << 8 | 'U' << 16 | 'V' << 24), /* 32  AYUV-8-8-8-8  */
	XYUV32 =  ('X' | 'Y' << 8 | 'U' << 16 | 'V' << 24), /* 32  XYUV-8-8-8-8  */
	VUYA32 =  ('V' | 'U' << 8 | 'Y' << 16 | 'A' << 24), /* 32  VUYA-8-8-8-8  */
	VUYX32 =  ('V' | 'U' << 8 | 'Y' << 16 | 'X' << 24), /* 32  VUYX-8-8-8-8  */
	YUVA32 =  ('Y' | 'U' << 8 | 'V' << 16 | 'A' << 24), /* 32  YUVA-8-8-8-8  */
	YUVX32 =  ('Y' | 'U' << 8 | 'V' << 16 | 'X' << 24), /* 32  YUVX-8-8-8-8  */
	M420 =    ('M' | '4' << 8 | '2' << 16 | '0' << 24), /* 12  YUV 4:2:0 2 lines y, 1 line uv interleaved */
	YUV48_12 =    ('Y' | '3' << 8 | '1' << 16 | '2' << 24), /* 48  YUV 4:4:4 12-bit per component */

	/*
	 * YCbCr packed format. For each Y2xx format, xx bits of valid data occupy the MSBs
	 * of the 16 bit components, and 16-xx bits of zero padding occupy the LSBs.
	 */
	Y210 =    ('Y' | '2' << 8 | '1' << 16 | '0' << 24), /* 32  YUYV 4:2:2 */
	Y212 =    ('Y' | '2' << 8 | '1' << 16 | '2' << 24), /* 32  YUYV 4:2:2 */
	Y216 =    ('Y' | '2' << 8 | '1' << 16 | '6' << 24), /* 32  YUYV 4:2:2 */

	/* two planes -- one Y, one Cr + Cb interleaved  */
	NV12 =    ('N' | 'V' << 8 | '1' << 16 | '2' << 24), /* 12  Y/CbCr 4:2:0  */
	NV21 =    ('N' | 'V' << 8 | '2' << 16 | '1' << 24), /* 12  Y/CrCb 4:2:0  */
	NV15 =    ('N' | 'V' << 8 | '1' << 16 | '5' << 24), /* 15  Y/CbCr 4:2:0 10-bit packed */
	NV16 =    ('N' | 'V' << 8 | '1' << 16 | '6' << 24), /* 16  Y/CbCr 4:2:2  */
	NV61 =    ('N' | 'V' << 8 | '6' << 16 | '1' << 24), /* 16  Y/CrCb 4:2:2  */
	NV20 =    ('N' | 'V' << 8 | '2' << 16 | '0' << 24), /* 20  Y/CbCr 4:2:2 10-bit packed */
	NV24 =    ('N' | 'V' << 8 | '2' << 16 | '4' << 24), /* 24  Y/CbCr 4:4:4  */
	NV42 =    ('N' | 'V' << 8 | '4' << 16 | '2' << 24), /* 24  Y/CrCb 4:4:4  */
	P010 =    ('P' | '0' << 8 | '1' << 16 | '0' << 24), /* 24  Y/CbCr 4:2:0 10-bit per component */
	P012 =    ('P' | '0' << 8 | '1' << 16 | '2' << 24), /* 24  Y/CbCr 4:2:0 12-bit per component */

	/* two non contiguous planes - one Y, one Cr + Cb interleaved  */
	NV12M =   ('N' | 'M' << 8 | '1' << 16 | '2' << 24), /* 12  Y/CbCr 4:2:0  */
	NV21M =   ('N' | 'M' << 8 | '2' << 16 | '1' << 24), /* 21  Y/CrCb 4:2:0  */
	NV16M =   ('N' | 'M' << 8 | '1' << 16 | '6' << 24), /* 16  Y/CbCr 4:2:2  */
	NV61M =   ('N' | 'M' << 8 | '6' << 16 | '1' << 24), /* 16  Y/CrCb 4:2:2  */
	P012M =   ('P' | 'M' << 8 | '1' << 16 | '2' << 24), /* 24  Y/CbCr 4:2:0 12-bit per component */

	/* three planes - Y Cb, Cr */
	YUV410 =  ('Y' | 'U' << 8 | 'V' << 16 | '9' << 24), /*  9  YUV 4:1:0     */
	YVU410 =  ('Y' | 'V' << 8 | 'U' << 16 | '9' << 24), /*  9  YVU 4:1:0     */
	YUV411P = ('4' | '1' << 8 | '1' << 16 | 'P' << 24), /* 12  YVU411 planar */
	YUV420 =  ('Y' | 'U' << 8 | '1' << 16 | '2' << 24), /* 12  YUV 4:2:0     */
	YVU420 =  ('Y' | 'V' << 8 | '1' << 16 | '2' << 24), /* 12  YVU 4:2:0     */
	YUV422P = ('4' | '2' << 8 | '2' << 16 | 'P' << 24), /* 16  YVU422 planar */

	/* three non contiguous planes - Y, Cb, Cr */
	YUV420M = ('Y' | 'M' << 8 | '1' << 16 | '2' << 24), /* 12  YUV420 planar */
	YVU420M = ('Y' | 'M' << 8 | '2' << 16 | '1' << 24), /* 12  YVU420 planar */
	YUV422M = ('Y' | 'M' << 8 | '1' << 16 | '6' << 24), /* 16  YUV422 planar */
	YVU422M = ('Y' | 'M' << 8 | '6' << 16 | '1' << 24), /* 16  YVU422 planar */
	YUV444M = ('Y' | 'M' << 8 | '2' << 16 | '4' << 24), /* 24  YUV444 planar */
	YVU444M = ('Y' | 'M' << 8 | '4' << 16 | '2' << 24), /* 24  YVU444 planar */

	/* Tiled YUV formats */
	NV12_4L4 = ('V' | 'T' << 8 | '1' << 16 | '2' << 24),   /* 12  Y/CbCr 4:2:0  4x4 tiles */
	NV12_16L16 = ('H' | 'M' << 8 | '1' << 16 | '2' << 24), /* 12  Y/CbCr 4:2:0 16x16 tiles */
	NV12_32L32 = ('S' | 'T' << 8 | '1' << 16 | '2' << 24), /* 12  Y/CbCr 4:2:0 32x32 tiles */
	NV15_4L4 = ('V' | 'T' << 8 | '1' << 16 | '5' << 24), /* 15 Y/CbCr 4:2:0 10-bit 4x4 tiles */
	P010_4L4 = ('T' | '0' << 8 | '1' << 16 | '0' << 24), /* 12  Y/CbCr 4:2:0 10-bit 4x4 macroblocks */
	NV12_8L128 =       ('A' | 'T' << 8 | '1' << 16 | '2' << 24), /* Y/CbCr 4:2:0 8x128 tiles */
	NV12_10BE_8L128 =  ('A' | 'X' << 8 | '1' << 16 | '2' << 24), /* Y/CbCr 4:2:0 10-bit 8x128 tiles */

	/* Tiled YUV formats, non contiguous planes */
	NV12MT =  ('T' | 'M' << 8 | '1' << 16 | '2' << 24), /* 12  Y/CbCr 4:2:0 64x32 tiles */
	NV12MT_16X16 = ('V' | 'M' << 8 | '1' << 16 | '2' << 24), /* 12  Y/CbCr 4:2:0 16x16 tiles */
	NV12M_8L128 =      ('N' | 'A' << 8 | '1' << 16 | '2' << 24), /* Y/CbCr 4:2:0 8x128 tiles */
	NV12M_10BE_8L128 = ('N' | 'T' << 8 | '1' << 16 | '2' << 24), /* Y/CbCr 4:2:0 10-bit 8x128 tiles */

	/* Bayer formats - see http://www.siliconimaging.com/RGB%20Bayer.htm */
	SBGGR8 =  ('B' | 'A' << 8 | '8' << 16 | '1' << 24), /*  8  BGBG.. GRGR.. */
	SGBRG8 =  ('G' | 'B' << 8 | 'R' << 16 | 'G' << 24), /*  8  GBGB.. RGRG.. */
	SGRBG8 =  ('G' | 'R' << 8 | 'B' << 16 | 'G' << 24), /*  8  GRGR.. BGBG.. */
	SRGGB8 =  ('R' | 'G' << 8 | 'G' << 16 | 'B' << 24), /*  8  RGRG.. GBGB.. */
	SBGGR10 = ('B' | 'G' << 8 | '1' << 16 | '0' << 24), /* 10  BGBG.. GRGR.. */
	SGBRG10 = ('G' | 'B' << 8 | '1' << 16 | '0' << 24), /* 10  GBGB.. RGRG.. */
	SGRBG10 = ('B' | 'A' << 8 | '1' << 16 | '0' << 24), /* 10  GRGR.. BGBG.. */
	SRGGB10 = ('R' | 'G' << 8 | '1' << 16 | '0' << 24), /* 10  RGRG.. GBGB.. */
		/* 10bit raw bayer packed, 5 bytes for every 4 pixels */
	SBGGR10P = ('p' | 'B' << 8 | 'A' << 16 | 'A' << 24),
	SGBRG10P = ('p' | 'G' << 8 | 'A' << 16 | 'A' << 24),
	SGRBG10P = ('p' | 'g' << 8 | 'A' << 16 | 'A' << 24),
	SRGGB10P = ('p' | 'R' << 8 | 'A' << 16 | 'A' << 24),
		/* 10bit raw bayer a-law compressed to 8 bits */
	SBGGR10ALAW8 = ('a' | 'B' << 8 | 'A' << 16 | '8' << 24),
	SGBRG10ALAW8 = ('a' | 'G' << 8 | 'A' << 16 | '8' << 24),
	SGRBG10ALAW8 = ('a' | 'g' << 8 | 'A' << 16 | '8' << 24),
	SRGGB10ALAW8 = ('a' | 'R' << 8 | 'A' << 16 | '8' << 24),
		/* 10bit raw bayer DPCM compressed to 8 bits */
	SBGGR10DPCM8 = ('b' | 'B' << 8 | 'A' << 16 | '8' << 24),
	SGBRG10DPCM8 = ('b' | 'G' << 8 | 'A' << 16 | '8' << 24),
	SGRBG10DPCM8 = ('B' | 'D' << 8 | '1' << 16 | '0' << 24),
	SRGGB10DPCM8 = ('b' | 'R' << 8 | 'A' << 16 | '8' << 24),
	SBGGR12 = ('B' | 'G' << 8 | '1' << 16 | '2' << 24), /* 12  BGBG.. GRGR.. */
	SGBRG12 = ('G' | 'B' << 8 | '1' << 16 | '2' << 24), /* 12  GBGB.. RGRG.. */
	SGRBG12 = ('B' | 'A' << 8 | '1' << 16 | '2' << 24), /* 12  GRGR.. BGBG.. */
	SRGGB12 = ('R' | 'G' << 8 | '1' << 16 | '2' << 24), /* 12  RGRG.. GBGB.. */
		/* 12bit raw bayer packed, 3 bytes for every 2 pixels */
	SBGGR12P = ('p' | 'B' << 8 | 'C' << 16 | 'C' << 24),
	SGBRG12P = ('p' | 'G' << 8 | 'C' << 16 | 'C' << 24),
	SGRBG12P = ('p' | 'g' << 8 | 'C' << 16 | 'C' << 24),
	SRGGB12P = ('p' | 'R' << 8 | 'C' << 16 | 'C' << 24),
	SBGGR14 = ('B' | 'G' << 8 | '1' << 16 | '4' << 24), /* 14  BGBG.. GRGR.. */
	SGBRG14 = ('G' | 'B' << 8 | '1' << 16 | '4' << 24), /* 14  GBGB.. RGRG.. */
	SGRBG14 = ('G' | 'R' << 8 | '1' << 16 | '4' << 24), /* 14  GRGR.. BGBG.. */
	SRGGB14 = ('R' | 'G' << 8 | '1' << 16 | '4' << 24), /* 14  RGRG.. GBGB.. */
		/* 14bit raw bayer packed, 7 bytes for every 4 pixels */
	SBGGR14P = ('p' | 'B' << 8 | 'E' << 16 | 'E' << 24),
	SGBRG14P = ('p' | 'G' << 8 | 'E' << 16 | 'E' << 24),
	SGRBG14P = ('p' | 'g' << 8 | 'E' << 16 | 'E' << 24),
	SRGGB14P = ('p' | 'R' << 8 | 'E' << 16 | 'E' << 24),
	SBGGR16 = ('B' | 'Y' << 8 | 'R' << 16 | '2' << 24), /* 16  BGBG.. GRGR.. */
	SGBRG16 = ('G' | 'B' << 8 | '1' << 16 | '6' << 24), /* 16  GBGB.. RGRG.. */
	SGRBG16 = ('G' | 'R' << 8 | '1' << 16 | '6' << 24), /* 16  GRGR.. BGBG.. */
	SRGGB16 = ('R' | 'G' << 8 | '1' << 16 | '6' << 24), /* 16  RGRG.. GBGB.. */

	/* HSV formats */
	HSV24 = ('H' | 'S' << 8 | 'V' << 16 | '3' << 24),
	HSV32 = ('H' | 'S' << 8 | 'V' << 16 | '4' << 24),

	/* compressed formats */
	MJPEG =    ('M' | 'J' << 8 | 'P' << 16 | 'G' << 24), /* Motion-JPEG   */
	JPEG =     ('J' | 'P' << 8 | 'E' << 16 | 'G' << 24), /* JFIF JPEG     */
	DV =       ('d' | 'v' << 8 | 's' << 16 | 'd' << 24), /* 1394          */
	MPEG =     ('M' | 'P' << 8 | 'E' << 16 | 'G' << 24), /* MPEG-1/2/4 Multiplexed */
	H264 =     ('H' | '2' << 8 | '6' << 16 | '4' << 24), /* H264 with start codes */
	H264_NO_SC = ('A' | 'V' << 8 | 'C' << 16 | '1' << 24), /* H264 without start codes */
	H264_MVC = ('M' | '2' << 8 | '6' << 16 | '4' << 24), /* H264 MVC */
	H263 =     ('H' | '2' << 8 | '6' << 16 | '3' << 24), /* H263          */
	MPEG1 =    ('M' | 'P' << 8 | 'G' << 16 | '1' << 24), /* MPEG-1 ES     */
	MPEG2 =    ('M' | 'P' << 8 | 'G' << 16 | '2' << 24), /* MPEG-2 ES     */
	MPEG2_SLICE = ('M' | 'G' << 8 | '2' << 16 | 'S' << 24), /* MPEG-2 parsed slice data */
	MPEG4 =    ('M' | 'P' << 8 | 'G' << 16 | '4' << 24), /* MPEG-4 part 2 ES */
	XVID =     ('X' | 'V' << 8 | 'I' << 16 | 'D' << 24), /* Xvid           */
	VC1_ANNEX_G = ('V' | 'C' << 8 | '1' << 16 | 'G' << 24), /* SMPTE 421M Annex G compliant stream */
	VC1_ANNEX_L = ('V' | 'C' << 8 | '1' << 16 | 'L' << 24), /* SMPTE 421M Annex L compliant stream */
	VP8 =      ('V' | 'P' << 8 | '8' << 16 | '0' << 24), /* VP8 */
	VP8_FRAME = ('V' | 'P' << 8 | '8' << 16 | 'F' << 24), /* VP8 parsed frame */
	VP9 =      ('V' | 'P' << 8 | '9' << 16 | '0' << 24), /* VP9 */
	VP9_FRAME = ('V' | 'P' << 8 | '9' << 16 | 'F' << 24), /* VP9 parsed frame */
	HEVC =     ('H' | 'E' << 8 | 'V' << 16 | 'C' << 24), /* HEVC aka H.265 */
	FWHT =     ('F' | 'W' << 8 | 'H' << 16 | 'T' << 24), /* Fast Walsh Hadamard Transform (vicodec) */
	FWHT_STATELESS =     ('S' | 'F' << 8 | 'W' << 16 | 'H' << 24), /* Stateless FWHT (vicodec) */
	H264_SLICE = ('S' | '2' << 8 | '6' << 16 | '4' << 24), /* H264 parsed slices */
	HEVC_SLICE = ('S' | '2' << 8 | '6' << 16 | '5' << 24), /* HEVC parsed slices */
	AV1_FRAME = ('A' | 'V' << 8 | '1' << 16 | 'F' << 24), /* AV1 parsed frame */
	SPK =      ('S' | 'P' << 8 | 'K' << 16 | '0' << 24), /* Sorenson Spark */
	RV30 =     ('R' | 'V' << 8 | '3' << 16 | '0' << 24), /* RealVideo 8 */
	RV40 =     ('R' | 'V' << 8 | '4' << 16 | '0' << 24), /* RealVideo 9 & 10 */

	/*  Vendor-specific formats   */
	CPIA1 =    ('C' | 'P' << 8 | 'I' << 16 | 'A' << 24), /* cpia1 YUV */
	WNVA =     ('W' | 'N' << 8 | 'V' << 16 | 'A' << 24), /* Winnov hw compress */
	SN9C10X =  ('S' | '9' << 8 | '1' << 16 | '0' << 24), /* SN9C10x compression */
	SN9C20X_I420 = ('S' | '9' << 8 | '2' << 16 | '0' << 24), /* SN9C20x YUV 4:2:0 */
	PWC1 =     ('P' | 'W' << 8 | 'C' << 16 | '1' << 24), /* pwc older webcam */
	PWC2 =     ('P' | 'W' << 8 | 'C' << 16 | '2' << 24), /* pwc newer webcam */
	ET61X251 = ('E' | '6' << 8 | '2' << 16 | '5' << 24), /* ET61X251 compression */
	SPCA501 =  ('S' | '5' << 8 | '0' << 16 | '1' << 24), /* YUYV per line */
	SPCA505 =  ('S' | '5' << 8 | '0' << 16 | '5' << 24), /* YYUV per line */
	SPCA508 =  ('S' | '5' << 8 | '0' << 16 | '8' << 24), /* YUVY per line */
	SPCA561 =  ('S' | '5' << 8 | '6' << 16 | '1' << 24), /* compressed GBRG bayer */
	PAC207 =   ('P' | '2' << 8 | '0' << 16 | '7' << 24), /* compressed BGGR bayer */
	MR97310A = ('M' | '3' << 8 | '1' << 16 | '0' << 24), /* compressed BGGR bayer */
	JL2005BCD = ('J' | 'L' << 8 | '2' << 16 | '0' << 24), /* compressed RGGB bayer */
	SN9C2028 = ('S' | 'O' << 8 | 'N' << 16 | 'X' << 24), /* compressed GBRG bayer */
	SQ905C =   ('9' | '0' << 8 | '5' << 16 | 'C' << 24), /* compressed RGGB bayer */
	PJPG =     ('P' | 'J' << 8 | 'P' << 16 | 'G' << 24), /* Pixart 73xx JPEG */
	OV511 =    ('O' | '5' << 8 | '1' << 16 | '1' << 24), /* ov511 JPEG */
	OV518 =    ('O' | '5' << 8 | '1' << 16 | '8' << 24), /* ov518 JPEG */
	STV0680 =  ('S' | '6' << 8 | '8' << 16 | '0' << 24), /* stv0680 bayer */
	TM6000 =   ('T' | 'M' << 8 | '6' << 16 | '0' << 24), /* tm5600/tm60x0 */
	CIT_YYVYUY = ('C' | 'I' << 8 | 'T' << 16 | 'V' << 24), /* one line of Y then 1 line of VYUY */
	KONICA420 =  ('K' | 'O' << 8 | 'N' << 16 | 'I' << 24), /* YUV420 planar in blocks of 256 pixels */
	JPGL = ('J' | 'P' << 8 | 'G' << 16 | 'L' << 24), /* JPEG-Lite */
	SE401 =      ('S' | '4' << 8 | '0' << 16 | '1' << 24), /* se401 janggu compressed rgb */
	S5C_UYVY_JPG = ('S' | '5' << 8 | 'C' << 16 | 'I' << 24), /* S5C73M3 interleaved UYVY/JPEG */
	Y8I =      ('Y' | '8' << 8 | 'I' << 16 | ' ' << 24), /* Greyscale 8-bit L/R interleaved */
	Y12I =     ('Y' | '1' << 8 | '2' << 16 | 'I' << 24), /* Greyscale 12-bit L/R interleaved */
	Y16I =     ('Y' | '1' << 8 | '6' << 16 | 'I' << 24), /* Greyscale 16-bit L/R interleaved */
	Z16 =      ('Z' | '1' << 8 | '6' << 16 | ' ' << 24), /* Depth data 16-bit */
	MT21C =    ('M' | 'T' << 8 | '2' << 16 | '1' << 24), /* Mediatek compressed block mode  */
	MM21 =     ('M' | 'M' << 8 | '2' << 16 | '1' << 24), /* Mediatek 8-bit block mode, two non-contiguous planes */
	MT2110T =  ('M' | 'T' << 8 | '2' << 16 | 'T' << 24), /* Mediatek 10-bit block tile mode */
	MT2110R =  ('M' | 'T' << 8 | '2' << 16 | 'R' << 24), /* Mediatek 10-bit block raster mode */
	INZI =     ('I' | 'N' << 8 | 'Z' << 16 | 'I' << 24), /* Intel Planar Greyscale 10-bit and Depth 16-bit */
	CNF4 =     ('C' | 'N' << 8 | 'F' << 16 | '4' << 24), /* Intel 4-bit packed depth confidence information */
	HI240 =    ('H' | 'I' << 8 | '2' << 16 | '4' << 24), /* BTTV 8-bit dithered RGB */
	QC08C =    ('Q' | '0' << 8 | '8' << 16 | 'C' << 24), /* Qualcomm 8-bit compressed */
	QC10C =    ('Q' | '1' << 8 | '0' << 16 | 'C' << 24), /* Qualcomm 10-bit compressed */
	AJPG =     ('A' | 'J' << 8 | 'P' << 16 | 'G' << 24), /* Aspeed JPEG */
	HEXTILE =  ('H' | 'X' << 8 | 'T' << 16 | 'L' << 24), /* Hextile compressed */

	/* 10bit raw packed, 32 bytes for every 25 pixels, last LSB 6 bits unused */
	IPU3_SBGGR10 = ('i' | 'p' << 8 | '3' << 16 | 'b' << 24), /* IPU3 packed 10-bit BGGR bayer */
	IPU3_SGBRG10 = ('i' | 'p' << 8 | '3' << 16 | 'g' << 24), /* IPU3 packed 10-bit GBRG bayer */
	IPU3_SGRBG10 = ('i' | 'p' << 8 | '3' << 16 | 'G' << 24), /* IPU3 packed 10-bit GRBG bayer */
	IPU3_SRGGB10 = ('i' | 'p' << 8 | '3' << 16 | 'r' << 24), /* IPU3 packed 10-bit RGGB bayer */

	/* Raspberry Pi PiSP compressed formats. */
	PISP_COMP1_RGGB = ('P' | 'C' << 8 | '1' << 16 | 'R' << 24), /* PiSP 8-bit mode 1 compressed RGGB bayer */
	PISP_COMP1_GRBG = ('P' | 'C' << 8 | '1' << 16 | 'G' << 24), /* PiSP 8-bit mode 1 compressed GRBG bayer */
	PISP_COMP1_GBRG = ('P' | 'C' << 8 | '1' << 16 | 'g' << 24), /* PiSP 8-bit mode 1 compressed GBRG bayer */
	PISP_COMP1_BGGR = ('P' | 'C' << 8 | '1' << 16 | 'B' << 24), /* PiSP 8-bit mode 1 compressed BGGR bayer */
	PISP_COMP1_MONO = ('P' | 'C' << 8 | '1' << 16 | 'M' << 24), /* PiSP 8-bit mode 1 compressed monochrome */
	PISP_COMP2_RGGB = ('P' | 'C' << 8 | '2' << 16 | 'R' << 24), /* PiSP 8-bit mode 2 compressed RGGB bayer */
	PISP_COMP2_GRBG = ('P' | 'C' << 8 | '2' << 16 | 'G' << 24), /* PiSP 8-bit mode 2 compressed GRBG bayer */
	PISP_COMP2_GBRG = ('P' | 'C' << 8 | '2' << 16 | 'g' << 24), /* PiSP 8-bit mode 2 compressed GBRG bayer */
	PISP_COMP2_BGGR = ('P' | 'C' << 8 | '2' << 16 | 'B' << 24), /* PiSP 8-bit mode 2 compressed BGGR bayer */
	PISP_COMP2_MONO = ('P' | 'C' << 8 | '2' << 16 | 'M' << 24), /* PiSP 8-bit mode 2 compressed monochrome */

	/* Renesas RZ/V2H CRU packed formats. 64-bit units with contiguous pixels */
	RAW_CRU10 = ('C' | 'R' << 8 | '1' << 16 | '0' << 24),
	RAW_CRU12 = ('C' | 'R' << 8 | '1' << 16 | '2' << 24),
	RAW_CRU14 = ('C' | 'R' << 8 | '1' << 16 | '4' << 24),
	RAW_CRU20 = ('C' | 'R' << 8 | '2' << 16 | '0' << 24),

}

