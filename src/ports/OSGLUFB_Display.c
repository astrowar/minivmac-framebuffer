/*
	OSGLUFB_Display.c

	Framebuffer display and rendering module for Linux Framebuffer port.
*/

#include "OSGLUFB_Common.h"

/* Forward declaration of global scale factor from Main.c */
extern double g_scale_factor;
extern int g_offset_x;
extern int g_offset_y;
extern blnr g_fill_enabled;
extern ui32 g_fill_color;

/* Framebuffer variables */
LOCALVAR int fb_fd = -1;
LOCALVAR ui3p fb_buffer = NULL;
LOCALVAR int fb_width = 0;
LOCALVAR int fb_height = 0;
LOCALVAR int fb_bps = 0;
LOCALVAR struct fb_var_screeninfo fb_var;
LOCALVAR struct fb_fix_screeninfo fb_fix;

/* Layout tracking: background is cleared only when these change */
LOCALVAR int fb_layout_offset_x = 0x7FFFFFFF;
LOCALVAR int fb_layout_offset_y = 0x7FFFFFFF;
LOCALVAR int fb_layout_scaled_w = 0;
LOCALVAR int fb_layout_scaled_h = 0;
LOCALVAR blnr fb_layout_center = falseblnr;
LOCALVAR ui32 fb_layout_fill_color = 0;
LOCALVAR blnr fb_layout_fill_enabled = falseblnr;
LOCALVAR blnr fb_first_draw = trueblnr; /* Force clear on first frame */

/* Test mode framebuffer buffer - defined here for module-local access */
LOCALVAR ui3p test_fb_buffer = NULL;

#define FB_INIT_SUCCESS 1
#define FB_INIT_FAILED  0

LOCALINLINEPROC GetRotatedDimensions(
	int src_width,
	int src_height,
	int *rot_width,
	int *rot_height)
{
	if ((g_rotate_degrees == 90) || (g_rotate_degrees == 270)) {
		*rot_width = src_height;
		*rot_height = src_width;
	} else {
		*rot_width = src_width;
		*rot_height = src_height;
	}
}

LOCALINLINEPROC RotatedToSource(
	int rot_x,
	int rot_y,
	int src_width,
	int src_height,
	int *src_x,
	int *src_y)
{
	switch (g_rotate_degrees) {
		case 90:
			*src_x = rot_y;
			*src_y = src_height - 1 - rot_x;
			break;
		case 180:
			*src_x = src_width - 1 - rot_x;
			*src_y = src_height - 1 - rot_y;
			break;
		case 270:
			*src_x = src_width - 1 - rot_y;
			*src_y = rot_x;
			break;
		case 0:
		default:
			*src_x = rot_x;
			*src_y = rot_y;
			break;
	}
}

LOCALINLINEFUNC int MonoSourceBitAt(
	ui3p draw_buf,
	int src_line_bytes,
	int src_width,
	int src_height,
	int rot_x,
	int rot_y)
{
	int src_x;
	int src_y;
	int src_byte_idx;

	RotatedToSource(rot_x, rot_y, src_width, src_height, &src_x, &src_y);

	if ((src_x < 0) || (src_x >= src_width)
		|| (src_y < 0) || (src_y >= src_height))
	{
		return 0;
	}

	src_byte_idx = src_y * src_line_bytes;
	return (draw_buf[src_byte_idx + (src_x >> 3)] >> (7 - (src_x & 7))) & 1;
}

/* --- Framebuffer initialization --- */

GLOBALOSGLUFUNC blnr fb_init(void)
{
	if (g_test_mode) {
		/* Test mode: use memory buffer */
		log_printf("OSGLUFB: Test mode - using %dx%d memory buffer\n",
			TEST_FB_WIDTH, TEST_FB_HEIGHT);
		fb_width = TEST_FB_WIDTH;
		fb_height = TEST_FB_HEIGHT;
		fb_bps = 32;
		test_fb_buffer = (ui3p)malloc(fb_width * fb_height * 4);
		if (test_fb_buffer == NULL) {
			log_printf("OSGLUFB: Failed to allocate test buffer\n");
			return falseblnr;
		}
		fb_fix.line_length = fb_width * 4;
		fb_buffer = test_fb_buffer;
		fb_layout_offset_x = 0x7FFFFFFF; /* force clear on first frame */
		log_printf("OSGLUFB: Test buffer allocated at %p\n", (void*)test_fb_buffer);
		return trueblnr;
	}

	/* Try to open /dev/fb0 with retries (10ms delay, up to 10 seconds) */
	int retry_count = 0;
	const int max_retries = 1000; /* 1000 * 10ms = 10 second */
	while ((fb_fd = open("/dev/fb0", O_RDWR)) < 0) {
		/* If device not found, fail immediately */
		if (errno == ENOENT) {
			log_printf("OSGLUFB: /dev/fb0 not found\n");
			return falseblnr;
		}
		retry_count++;
		if (retry_count >= max_retries) {
			if (errno == EACCES) {
				//log_printf("OSGLUFB: Permission denied accessing /dev/fb0 after %d retries\n", max_retries);
				//log_printf("Make sure you are running as root or have appropriate permissions.\n");
			} else {
				log_printf("OSGLUFB: Cannot open /dev/fb0 after %d retries: %s\n", max_retries, strerror(errno));
			}
			return falseblnr;
		}
		if (errno == EACCES) {
			log_printf("OSGLUFB: Permission denied (attempt %d/%d), retrying...\n", retry_count, max_retries);
		} else {
			log_printf("OSGLUFB: Cannot open /dev/fb0 (attempt %d/%d), retrying... (%s)\n",
				retry_count, max_retries, strerror(errno));
		}
		usleep(10000); /* 10ms */
	}

	if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &fb_var) < 0) {
		log_printf("OSGLUFB: Failed to get framebuffer info: %s\n",
			strerror(errno));
		close(fb_fd);
		fb_fd = -1;
		return falseblnr;
	}

	if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &fb_fix) < 0) {
		log_printf("OSGLUFB: Failed to get fix info: %s\n",
			strerror(errno));
		close(fb_fd);
		fb_fd = -1;
		return falseblnr;
	}

	fb_width = fb_var.xres;
	fb_height = fb_var.yres;
	fb_bps = fb_var.bits_per_pixel;

	fb_buffer = (ui3p)mmap(NULL, fb_fix.smem_len, PROT_READ | PROT_WRITE,
		MAP_SHARED, fb_fd, 0);
	if (fb_buffer == (ui3p)MAP_FAILED) {
		fb_buffer = NULL;
		log_printf("OSGLUFB: Failed to mmap framebuffer: %s\n",
			strerror(errno));
		close(fb_fd);
		fb_fd = -1;
		return falseblnr;
	}

	fb_layout_offset_x = 0x7FFFFFFF; /* force clear on first frame */

	return trueblnr;
}

/* --- Framebuffer shutdown --- */

GLOBALOSGLUPROC fb_shutdown(void)
{
	if (g_test_mode && test_fb_buffer != NULL) {
		free(test_fb_buffer);
		test_fb_buffer = NULL;
	} else if (fb_buffer != NULL) {
		munmap(fb_buffer, fb_fix.smem_len);
		fb_buffer = NULL;
	}
	fb_layout_offset_x = 0x7FFFFFFF; /* force re-clear on next open */
	fb_layout_offset_y = 0x7FFFFFFF;
	if (fb_fd >= 0) {
		close(fb_fd);
		fb_fd = -1;
	}
}

/* --- Test mode: render framebuffer to console --- */

LOCALPROC fb_test_render(void)
{
	if (!g_test_mode || fb_buffer == NULL) {
		return;
	}

	/* Clear screen */
	log_printf("fb_test_render called\n");
	printf("\033[2J\033[H");
	printf("\n");
	printf("Test mode - Press Ctrl+C to exit\n");
	printf("Keyboard: any key | Mouse: mouse moves to screen\n");

	/* Read keyboard input */
	char keybuf[16];
	ssize_t n = read(STDIN_FILENO, keybuf, sizeof(keybuf) - 1);
	if (n > 0) {
		keybuf[n] = 0;
		printf("Key pressed: ");
		for (int i = 0; i < n; i++) printf("%02x ", (unsigned char)keybuf[i]);
		printf("\n");
		fflush(stdout);
	}

	/* Read mouse events from /dev/input */
	FILE *mouse_fp = fopen("/dev/input/mice", "rb");
	if (mouse_fp) {
		unsigned char mbuf[3];
		n = fread(mbuf, 1, 3, mouse_fp);
		if (n == 3) {
			int dx = (mbuf[1] | ((mbuf[2] & 0x40) ? -0x100 : 0)) - 128;
			int dy = (mbuf[2] & 0x80) ? -128 : mbuf[2];
			int buttons = ((mbuf[0] & 0x01) ? 1 : 0) | ((mbuf[0] & 0x02) ? 2 : 0) | ((mbuf[0] & 0x04) ? 4 : 0);
			printf("Mouse: dx=%d dy=%d buttons=%d\n", dx, dy, buttons);
			fflush(stdout);
		}
		fclose(mouse_fp);
	}

	/* Render as ASCII art (scaled down for readability) */
	int scale = 2;
	int render_width = fb_width / scale;
	int render_height = fb_height / scale;

	for (int y = 0; y < render_height; y++) {
		for (int x = 0; x < render_width; x++) {
			int src_x = x * scale;
			int src_y = y * scale;
			ui32 pixel = ((ui32 *)fb_buffer)[src_y * fb_width + src_x];

			/* Check if pixel is black (top bit set in ARGB) */
			if ((pixel & 0xFF000000) != 0) {
				printf("██");  /* Black */
			} else {
				printf("  ");  /* White */
			}
		}
		printf("\n");
	}

	printf("\n");
	fflush(stdout);
}

/* --- Fill framebuffer with color (for fill mode) --- */

LOCALPROC fb_fill_with_color(ui32 color)
{
	if (fb_buffer == NULL) {
		return;
	}

	switch (fb_bps) {
		case 32: {
			ui32 *fb_ptr = (ui32 *)fb_buffer;
			int total_pixels = fb_width * fb_height;
			for (int i = 0; i < total_pixels; i++) {
				fb_ptr[i] = color;
			}
			break;
		}
		case 24: {
			ui8 *fb_ptr = (ui8 *)fb_buffer;
			int total_pixels = fb_width * fb_height;
			for (int i = 0; i < total_pixels; i++) {
				fb_ptr[i * 3 + 0] = (color >> 16) & 0xFF;
				fb_ptr[i * 3 + 1] = (color >> 8) & 0xFF;
				fb_ptr[i * 3 + 2] = color & 0xFF;
			}
			break;
		}
		case 16: {
			ui16 *fb_ptr = (ui16 *)fb_buffer;
			int total_pixels = fb_width * fb_height;
			for (int i = 0; i < total_pixels; i++) {
				fb_ptr[i] = ((color >> 10) & 0x1F) << 11 |
					((color >> 5) & 0x1F) << 5 |
					(color & 0x1F);
			}
			break;
		}
		case 15: {
			ui16 *fb_ptr = (ui16 *)fb_buffer;
			int total_pixels = fb_width * fb_height;
			for (int i = 0; i < total_pixels; i++) {
				fb_ptr[i] = ((color >> 10) & 0x1F) << 10 |
					((color >> 5) & 0x1F) << 5 |
					(color & 0x1F);
			}
			break;
		}
	}
}

/* --- Internal: draw to buffer (common code) --- */

LOCALPROC fb_draw_to_buffer(void)
{
	if (fb_buffer == NULL) {
		return;
	}

	ui3p draw_buf = GetCurDrawBuff();
	if (draw_buf == NULL) {
		/* Screen buffer not available - draw blank screen */
		memset(fb_buffer, 0xFF, fb_width * fb_height * 4);
		return;
	}

#if 0 != vMacScreenDepth
	if (UseColorMode && ColorModeWorks) {
		ui3b *clut_r = (ui3b *)CLUT_reds;
		ui3b *clut_g = (ui3b *)CLUT_greens;
		ui3b *clut_b = (ui3b *)CLUT_blues;

		int src_line_bytes = vMacScreenBitWidth / 8;
		int src_width = vMacScreenWidth;
		int src_height = vMacScreenHeight;
		int rot_width;
		int rot_height;

		GetRotatedDimensions(src_width, src_height, &rot_width, &rot_height);

		for (si4b y = 0; y < fb_height; y++) {
			int rot_y = (y * rot_height) / fb_height;
			ui8 *fb_row = (ui8 *)fb_buffer + y * fb_fix.line_length;

			for (si4b x = 0; x < fb_width; x++) {
				int rot_x = (x * rot_width) / fb_width;
				ui8 color_idx = (ui8)MonoSourceBitAt(
					draw_buf, src_line_bytes, src_width, src_height, rot_x, rot_y);

				if (color_idx < CLUT_size) {
					switch (fb_bps) {
						case 32:
							((ui32 *)fb_row)[x] =
								((ui32)clut_r[color_idx] << 16) |
								((ui32)clut_g[color_idx] << 8) |
								clut_b[color_idx];
							break;
						case 24:
							fb_row[x * 3 + 0] = clut_r[color_idx];
							fb_row[x * 3 + 1] = clut_g[color_idx];
							fb_row[x * 3 + 2] = clut_b[color_idx];
							break;
						case 16:
							((ui16 *)fb_row)[x] =
								((ui16)(clut_r[color_idx] >> 3) << 11) |
								((ui16)(clut_g[color_idx] >> 2) << 5) |
								((ui16)(clut_b[color_idx] >> 3));
							break;
						case 15:
							((ui16 *)fb_row)[x] =
								((ui16)(clut_r[color_idx] >> 3) << 10) |
								((ui16)(clut_g[color_idx] >> 3) << 5) |
								((ui16)(clut_b[color_idx] >> 3));
							break;
					}
				}
			}
		}
	} else
#endif
	{
		int src_line_bytes = vMacScreenMonoByteWidth;
		int src_width = vMacScreenWidth;
		int src_height = vMacScreenHeight;
		int rot_width;
		int rot_height;
		blnr center_no_scale;
		int dst_x0 = 0;
		int dst_y0 = 0;

		GetRotatedDimensions(src_width, src_height, &rot_width, &rot_height);

		/* Apply scale factor */
		int scaled_width = (int)(rot_width * g_scale_factor);
		int scaled_height = (int)(rot_height * g_scale_factor);

		center_no_scale = (fb_width >= scaled_width) && (fb_height >= scaled_height);

		if (center_no_scale) {
			dst_x0 = (fb_width - scaled_width) / 2 + g_offset_x;
			dst_y0 = (fb_height - scaled_height) / 2 + g_offset_y;
		} else {
			dst_x0 = g_offset_x;
			dst_y0 = g_offset_y;
		}

		/* Fill background with fill color if enabled, only when layout changes */
		if (fb_first_draw
			|| (fb_layout_offset_x != g_offset_x)
			|| (fb_layout_offset_y != g_offset_y)
			|| (fb_layout_scaled_w != scaled_width)
			|| (fb_layout_scaled_h != scaled_height)
			|| (fb_layout_center != center_no_scale)
			|| (fb_layout_fill_enabled != g_fill_enabled)
			|| (fb_layout_fill_color != g_fill_color))
		{
			if (g_fill_enabled) {
				fb_fill_with_color(g_fill_color);
			} else {
				memset(fb_buffer, 0x00, fb_fix.line_length * fb_height);
			}
			fb_first_draw = falseblnr;
			fb_layout_offset_x = g_offset_x;
			fb_layout_offset_y = g_offset_y;
			fb_layout_scaled_w = scaled_width;
			fb_layout_scaled_h = scaled_height;
			fb_layout_center = center_no_scale;
			fb_layout_fill_enabled = g_fill_enabled;
			fb_layout_fill_color = g_fill_color;
		}

		switch (fb_bps) {
			case 32: {
				ui32 *fb_ptr = (ui32 *)fb_buffer;
				int fb_pitch = fb_fix.line_length / 4;

				if (center_no_scale) {
					for (si4b y = 0; y < scaled_height; ++y) {
						int src_y = (y * rot_height) / scaled_height;
						int dst_y = y + dst_y0;
						if (dst_y < 0 || dst_y >= fb_height) { continue; }

						for (si4b x = 0; x < scaled_width; ++x) {
							int src_x = (x * rot_width) / scaled_width;
							int src_bit = MonoSourceBitAt(
								draw_buf, src_line_bytes, src_width, src_height, src_x, src_y);
							int dst_x = x + dst_x0;
							if (dst_x < 0 || dst_x >= fb_width) { continue; }

							fb_ptr[dst_y * fb_pitch + dst_x] = src_bit ? 0x00000000 : 0x00FFFFFF;
						}
					}
				} else {
					for (si4b y = 0; y < fb_height; y++) {
						int src_y = (y * rot_height) / fb_height;
						int dst_y = y + dst_y0;
						if (dst_y < 0 || dst_y >= fb_height) { continue; }

						for (si4b x = 0; x < fb_width; x++) {
							int src_x = (x * rot_width) / fb_width;
							int src_bit = MonoSourceBitAt(
								draw_buf, src_line_bytes, src_width, src_height,
								src_x, src_y);
							int dst_x = x + dst_x0;
							if (dst_x < 0 || dst_x >= fb_width) { continue; }

							fb_ptr[dst_y * fb_pitch + dst_x] = src_bit ? 0x00000000 : 0x00FFFFFF;
						}
					}
				}
				break;
			}

			case 24: {
				ui8 *fb_ptr = (ui8 *)fb_buffer;
				int fb_pitch = fb_fix.line_length;

				if (center_no_scale) {
					for (si4b y = 0; y < scaled_height; ++y) {
						int src_y = (y * rot_height) / scaled_height;
						int dst_y = y + dst_y0;
						if (dst_y < 0 || dst_y >= fb_height) { continue; }
						ui8 *row_ptr = fb_ptr + dst_y * fb_pitch;

						for (si4b x = 0; x < scaled_width; ++x) {
							int src_x = (x * rot_width) / scaled_width;
							int src_bit = MonoSourceBitAt(
								draw_buf, src_line_bytes, src_width, src_height, src_x, src_y);
							int dst_x = x + dst_x0;
							if (dst_x < 0 || dst_x >= fb_width) { continue; }

							row_ptr[dst_x * 3 + 0] = src_bit ? 0x00 : 0xFF;
							row_ptr[dst_x * 3 + 1] = src_bit ? 0x00 : 0xFF;
							row_ptr[dst_x * 3 + 2] = src_bit ? 0x00 : 0xFF;
						}
					}
				} else {
					for (si4b y = 0; y < fb_height; y++) {
						int src_y = (y * rot_height) / fb_height;
						int dst_y = y + dst_y0;
						if (dst_y < 0 || dst_y >= fb_height) { continue; }
						ui8 *row_ptr = fb_ptr + dst_y * fb_pitch;

						for (si4b x = 0; x < fb_width; x++) {
							int src_x = (x * rot_width) / fb_width;
							int src_bit = MonoSourceBitAt(
								draw_buf, src_line_bytes, src_width, src_height,
								src_x, src_y);
							int dst_x = x + dst_x0;
							if (dst_x < 0 || dst_x >= fb_width) { continue; }

							row_ptr[dst_x * 3 + 0] = src_bit ? 0x00 : 0xFF;
							row_ptr[dst_x * 3 + 1] = src_bit ? 0x00 : 0xFF;
							row_ptr[dst_x * 3 + 2] = src_bit ? 0x00 : 0xFF;
						}
					}
				}
				break;
			}

			case 16: {
				ui16 *fb_ptr = (ui16 *)fb_buffer;
				int fb_pitch = fb_fix.line_length / 2;

				if (center_no_scale) {
					for (si4b y = 0; y < scaled_height; ++y) {
						int src_y = (y * rot_height) / scaled_height;
						int dst_y = y + dst_y0;
						if (dst_y < 0 || dst_y >= fb_height) { continue; }

						for (si4b x = 0; x < scaled_width; ++x) {
							int src_x = (x * rot_width) / scaled_width;
							int src_bit = MonoSourceBitAt(
								draw_buf, src_line_bytes, src_width, src_height, src_x, src_y);
							ui16 color = src_bit ? 0x0000 : 0xFFFF;
							int dst_x = x + dst_x0;
							if (dst_x < 0 || dst_x >= fb_width) { continue; }

							fb_ptr[dst_y * fb_pitch + dst_x] = color;
						}
					}
				} else {
					for (si4b y = 0; y < fb_height; y++) {
						int src_y = (y * rot_height) / fb_height;
						int dst_y = y + dst_y0;
						if (dst_y < 0 || dst_y >= fb_height) { continue; }

						for (si4b x = 0; x < fb_width; x++) {
							int src_x = (x * rot_width) / fb_width;
							int src_bit = MonoSourceBitAt(
								draw_buf, src_line_bytes, src_width, src_height,
								src_x, src_y);
							ui16 color = src_bit ? 0x0000 : 0xFFFF;
							int dst_x = x + dst_x0;
							if (dst_x < 0 || dst_x >= fb_width) { continue; }

							fb_ptr[dst_y * fb_pitch + dst_x] = color;
						}
					}
				}
				break;
			}

			case 15: {
				ui16 *fb_ptr = (ui16 *)fb_buffer;
				int fb_pitch = fb_fix.line_length / 2;

				if (center_no_scale) {
					for (si4b y = 0; y < scaled_height; ++y) {
						int src_y = (y * rot_height) / scaled_height;
						int dst_y = y + dst_y0;
						if (dst_y < 0 || dst_y >= fb_height) { continue; }

						for (si4b x = 0; x < scaled_width; ++x) {
							int src_x = (x * rot_width) / scaled_width;
							int src_bit = MonoSourceBitAt(
								draw_buf, src_line_bytes, src_width, src_height, src_x, src_y);
							ui16 color = src_bit ? 0x0000 : 0x7FFF;
							int dst_x = x + dst_x0;
							if (dst_x < 0 || dst_x >= fb_width) { continue; }

							fb_ptr[dst_y * fb_pitch + dst_x] = color;
						}
					}
				} else {
					for (si4b y = 0; y < fb_height; y++) {
						int src_y = (y * rot_height) / fb_height;
						int dst_y = y + dst_y0;
						if (dst_y < 0 || dst_y >= fb_height) { continue; }

						for (si4b x = 0; x < fb_width; x++) {
							int src_x = (x * rot_width) / fb_width;
							int src_bit = MonoSourceBitAt(
								draw_buf, src_line_bytes, src_width, src_height,
								src_x, src_y);
							ui16 color = src_bit ? 0x0000 : 0x7FFF;
							int dst_x = x + dst_x0;
							if (dst_x < 0 || dst_x >= fb_width) { continue; }

							fb_ptr[dst_y * fb_pitch + dst_x] = color;
						}
					}
				}
				break;
			}
		}
	}
}

/* --- Public: draw to screen --- */

GLOBALOSGLUPROC fb_draw(void)
{

	fb_draw_to_buffer();

	if (g_test_mode) {
		return;
	}

	if (fb_buffer != NULL && fb_fd >= 0) {
		/* Copy to framebuffer */
		void *dst = fb_buffer;
		ui3p src = GetCurDrawBuff();
		if (src != NULL) {
			/* Use memcpy for simplicity - could optimize with ioctl */
			/* For now, just update the buffer which should be visible */
		}
	}
}

GLOBALOSGLUFUNC blnr fb_dump_snapshot(char *path)
{
	FILE *fp;
	si4b x;
	si4b y;

	if ((NULL == fb_buffer) || (NULL == path)) {
		return falseblnr;
	}

	fp = fopen(path, "wb");
	if (NULL == fp) {
		return falseblnr;
	}

	(void)fprintf(fp, "P6\n%d %d\n255\n", fb_width, fb_height);

	for (y = 0; y < fb_height; ++y) {
		for (x = 0; x < fb_width; ++x) {
			ui3b rgb[3];

			if (32 == fb_bps) {
				ui32 px = ((ui32 *)fb_buffer)[y * fb_width + x];
				rgb[0] = (ui3b)((px >> 16) & 0xFF);
				rgb[1] = (ui3b)((px >> 8) & 0xFF);
				rgb[2] = (ui3b)(px & 0xFF);
			} else {
				rgb[0] = 0x00;
				rgb[1] = 0x00;
				rgb[2] = 0x00;
			}

			(void)fwrite(rgb, 1, 3, fp);
		}
	}

	(void)fclose(fp);
	return trueblnr;
}