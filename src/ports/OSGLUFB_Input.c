/*
	OSGLUFB_Input.c

	Keyboard input handling module for Linux Framebuffer port.
*/

#include "OSGLUFB_Common.h"
#include "../event_reader/input_reader.h"
#include <linux/input.h>
#include <time.h>

/* Keyboard variables */
LOCALVAR int mouse_fd = -1;
LOCALVAR blnr kbd_raw_mode = falseblnr;
LOCALVAR blnr host_mouse_button_down = falseblnr;
LOCALVAR ui3b mouse_packet_buf[3];
LOCALVAR ui3r mouse_packet_have = 0;
LOCALVAR blnr mouse_log_enabled = falseblnr;
LOCALVAR si4r mouse_pending_dx = 0;
LOCALVAR si4r mouse_pending_dy = 0;
LOCALVAR blnr mouse_pending_button_valid = falseblnr;
LOCALVAR blnr mouse_pending_button_state = falseblnr;

/* Keyboard device reader */
LOCALVAR input_reader_t kbd_reader;
LOCALVAR time_t kbd_last_scan_time = 0;

/* Keyboard hotplug scan interval in seconds */
#define KBD_SCAN_INTERVAL_SEC 10

LOCALFUNC blnr KbdPathExists(char paths[MAX_KEYBOARDS][MAX_NAME_LEN], int count, const char *path)
{
	int i;

	for (i = 0; i < count; ++i) {
		if (0 == strcmp(paths[i], path)) {
			return trueblnr;
		}
	}

	return falseblnr;
}

LOCALFUNC blnr KbdReaderHasPath(const input_reader_t *reader, const char *path)
{
	int i;

	for (i = 0; i < reader->count; ++i) {
		if (0 == strcmp(reader->keyboards[i].path, path)) {
			return trueblnr;
		}
	}

	return falseblnr;
}

LOCALPROC MouseLogPacket(ui3b b0, ui3b b1, ui3b b2,
	si4b dx, si4b dy, blnr left_down)
{
	if (!mouse_log_enabled) {
		return;
	}

	fprintf(stderr,
		"OSGLUFB: mouse packet raw=[%02X %02X %02X] dx=%ld dy=%ld left=%s\n",
		(unsigned int)b0,
		(unsigned int)b1,
		(unsigned int)b2,
		(long)dx,
		(long)dy,
		left_down ? "down" : "up");
}

LOCALPROC QueueKeyEvent(ui3r mac_key, blnr is_down)
{
	if (OSGLUFB_PostKey(mac_key, is_down)) {
	} else if (mouse_log_enabled) {
		fprintf(stderr, "OSGLUFB: key event dropped (queue full), key=%u down=%u\n",
			(unsigned int)mac_key, (unsigned int)is_down);
	}
}

LOCALFUNC blnr QueueMouseButton(blnr down)
{
	if (down == host_mouse_button_down) {
		return trueblnr;
	}

	if (OSGLUFB_PostMouseButton(down)) {
		if (mouse_log_enabled) {
			fprintf(stderr, "OSGLUFB: mouse button event queued: %s\n",
				down ? "press" : "release");
		}
		host_mouse_button_down = down;
		return trueblnr;
	} else if (mouse_log_enabled) {
		fprintf(stderr, "OSGLUFB: mouse button event dropped (queue full): %s\n",
			down ? "press" : "release");
	}

	return falseblnr;
}

LOCALFUNC blnr QueueMouseDelta(si4r dh, si4r dv)
{
	if ((dh != 0) || (dv != 0)) {
		if (OSGLUFB_PostMouseDelta(dh, dv)) {
			if (mouse_log_enabled) {
				fprintf(stderr, "OSGLUFB: mouse move event queued: dx=%ld dy=%ld\n",
					(long)dh, (long)dv);
			}
			return trueblnr;
		} else if (mouse_log_enabled) {
			fprintf(stderr,
				"OSGLUFB: mouse move event dropped (queue full): dx=%ld dy=%ld\n",
				(long)dh, (long)dv);
		}
		return falseblnr;
	}

	return trueblnr;
}

/* --- Setup raw keyboard mode --- */

/* Reinitialize keyboard device reader (for hotplug support) */
LOCALPROC KbdRescan(void)
{
	char old_paths[MAX_KEYBOARDS][MAX_NAME_LEN];
	int old_count = 0;
	int i;
	blnr init_ok;

	for (i = 0; (i < kbd_reader.count) && (i < MAX_KEYBOARDS); ++i) {
		strncpy(old_paths[i], kbd_reader.keyboards[i].path, MAX_NAME_LEN - 1);
		old_paths[i][MAX_NAME_LEN - 1] = '\0';
		++old_count;
	}

	input_reader_cleanup(&kbd_reader);
	init_ok = (input_reader_init(&kbd_reader) >= 0) ? trueblnr : falseblnr;

	if (!init_ok) {
		kbd_reader.count = 0;
	}

	for (i = 0; i < kbd_reader.count; ++i) {
		if (!KbdPathExists(old_paths, old_count, kbd_reader.keyboards[i].path)) {
			fprintf(stderr, "OSGLUFB: teclado adicionado: %s (%s)\n",
				kbd_reader.keyboards[i].name[0] ? kbd_reader.keyboards[i].name : "sem nome",
				kbd_reader.keyboards[i].path);
		}
	}

	for (i = 0; i < old_count; ++i) {
		if (!KbdReaderHasPath(&kbd_reader, old_paths[i])) {
			fprintf(stderr, "OSGLUFB: teclado removido: %s\n", old_paths[i]);
		}
	}
}

/* Update keyboard list periodically */
LOCALPROC KbdScanUpdate(void)
{
	if (kbd_raw_mode) {
		time_t now = time(NULL);

		if ((kbd_last_scan_time == 0)
			|| ((now - kbd_last_scan_time) >= KBD_SCAN_INTERVAL_SEC))
		{
			kbd_last_scan_time = now;
			KbdRescan();
		}
	}
}

GLOBALOSGLUPROC setup_kbd_raw(void)
{
	/* Skip keyboard setup in test mode - not needed for test mode */
	if (g_test_mode) {
		return;
	}

	/* Initialize keyboard device reader using dev/input */
	if (input_reader_init(&kbd_reader) < 0) {
		fprintf(stderr, "OSGLUFB: nenhum teclado detectado no init; hotplug ativo\n");
	}
	kbd_raw_mode = trueblnr;
	kbd_last_scan_time = 0;

	mouse_fd = open("/dev/input/mice", O_RDONLY | O_NONBLOCK);
	{
		char *mouse_log_env = getenv("OSGLUFB_MOUSE_LOG");
		if ((mouse_log_env != NULL) && (0 == strcmp(mouse_log_env, "1"))) {
			mouse_log_enabled = trueblnr;
		}
	}
	if (mouse_fd < 0) {
		(void)strerror(errno);
	}
}

/* --- Restore original keyboard mode --- */

GLOBALOSGLUPROC restore_kbd(void)
{
	if (kbd_raw_mode) {
		input_reader_cleanup(&kbd_reader);
		kbd_raw_mode = falseblnr;
	}
	kbd_last_scan_time = 0;
	if (mouse_fd >= 0) {
		close(mouse_fd);
		mouse_fd = -1;
	}
	host_mouse_button_down = falseblnr;
	mouse_packet_have = 0;
	mouse_pending_dx = 0;
	mouse_pending_dy = 0;
	mouse_pending_button_valid = falseblnr;
	mouse_pending_button_state = falseblnr;
}

/* --- Poll for keyboard input --- */

/* Map Linux input scan codes to Mac keys */
LOCALFUNC ui3r LinuxScanCode2MacKey(uint16_t scan_code)
{
	/* Function keys (EV_KEY scan codes) */
	switch (scan_code) {
		case KEY_F1:   return MKC_F1;
		case KEY_F2:   return MKC_F2;
		case KEY_F3:   return MKC_F3;
		case KEY_F4:   return MKC_F4;
		case KEY_F5:   return MKC_F5;
		case KEY_F6:   return MKC_F6;
		case KEY_F7:   return MKC_F7;
		case KEY_F8:   return MKC_F8;
		case KEY_F9:   return MKC_F9;
		case KEY_F10:  return MKC_F10;
		case KEY_F11:  return MKC_F11;
		case KEY_F12:  return MKC_F12;

		/* Special keys */
		case KEY_ENTER: return MKC_Return;
		case KEY_TAB:   return MKC_Tab;
		case KEY_ESC:   return MKC_formac_Escape;
		case KEY_BACKSPACE: return MKC_BackSpace;

		/* Letters */
		case KEY_A: return MKC_A;
		case KEY_B: return MKC_B;
		case KEY_C: return MKC_C;
		case KEY_D: return MKC_D;
		case KEY_E: return MKC_E;
		case KEY_F: return MKC_F;
		case KEY_G: return MKC_G;
		case KEY_H: return MKC_H;
		case KEY_I: return MKC_I;
		case KEY_J: return MKC_J;
		case KEY_K: return MKC_K;
		case KEY_L: return MKC_L;
		case KEY_M: return MKC_M;
		case KEY_N: return MKC_N;
		case KEY_O: return MKC_O;
		case KEY_P: return MKC_P;
		case KEY_Q: return MKC_Q;
		case KEY_R: return MKC_R;
		case KEY_S: return MKC_S;
		case KEY_T: return MKC_T;
		case KEY_U: return MKC_U;
		case KEY_V: return MKC_V;
		case KEY_W: return MKC_W;
		case KEY_X: return MKC_X;
		case KEY_Y: return MKC_Y;
		case KEY_Z: return MKC_Z;

		/* Numbers */
		case KEY_1: return MKC_1;
		case KEY_2: return MKC_2;
		case KEY_3: return MKC_3;
		case KEY_4: return MKC_4;
		case KEY_5: return MKC_5;
		case KEY_6: return MKC_6;
		case KEY_7: return MKC_7;
		case KEY_8: return MKC_8;
		case KEY_9: return MKC_9;
		case KEY_0: return MKC_0;

		/* Symbols */
		case KEY_MINUS: return MKC_Minus;
		case KEY_EQUAL: return MKC_Equal;
		case KEY_LEFTBRACE: return MKC_LeftBracket;
		case KEY_RIGHTBRACE: return MKC_RightBracket;
		case KEY_BACKSLASH: return MKC_formac_BackSlash;
		case KEY_SEMICOLON: return MKC_SemiColon;
		case KEY_APOSTROPHE: return MKC_SingleQuote;
		case KEY_GRAVE: return MKC_formac_Grave;
		case KEY_COMMA: return MKC_Comma;
		case KEY_DOT: return MKC_Period;
		case KEY_SLASH: return MKC_Slash;

		/* Space */
		case KEY_SPACE: return MKC_Space;

		default:
			return 0;
	}
}

GLOBALOSGLUPROC fb_poll_input(void)
{
	ui3b buf[16];
	ssize_t n;
	si4r mouse_acc_dx = 0;
	si4r mouse_acc_dy = 0;
	blnr mouse_button_seen = falseblnr;
	blnr mouse_button_latest = host_mouse_button_down;
	blnr mouse_got_new_packet = falseblnr;
	uint16_t key_code;

	if (g_test_mode) {
		/* In test mode, skip host input integration */
		return;
	}

	/* Periodically rescan for keyboard devices (hotplug support) */
	KbdScanUpdate();

	/* Read keyboard events using event_reader */
	int key_result = input_reader_read_key(&kbd_reader, &key_code);
	if (key_result > 0) {
		/* F10: force quit */
		if (key_code == KEY_F10) {
			fprintf(stderr, "force quit\n");
			fflush(stderr);
			exit(0);
		}

		/* Convert scan code to Mac key and queue event */
		ui3r mac_key = LinuxScanCode2MacKey(key_code);
		if (mac_key != 0) {
			QueueKeyEvent(mac_key, trueblnr);
			QueueKeyEvent(mac_key, falseblnr);
		}
	}

	if (mouse_fd >= 0) {
		while ((n = read(mouse_fd, buf, sizeof(buf))) > 0) {
			size_t j;
			for (j = 0; j < (size_t)n; ++j) {
				if ((mouse_packet_have == 0) && ((buf[j] & 0x08) == 0)) {
					if (mouse_log_enabled) {
						fprintf(stderr,
							"OSGLUFB: mouse desync byte dropped: %02X\n",
							(unsigned int)buf[j]);
					}
					continue;
				}

				mouse_packet_buf[mouse_packet_have++] = buf[j];
				if (mouse_packet_have == 3) {
					si4b dx = (si4b)(signed char)mouse_packet_buf[1];
					si4b dy = - (si4b)(signed char)mouse_packet_buf[2];
					blnr left_down = (mouse_packet_buf[0] & 0x01) ? trueblnr : falseblnr;
					mouse_got_new_packet = trueblnr;

					MouseLogPacket(
						mouse_packet_buf[0],
						mouse_packet_buf[1],
						mouse_packet_buf[2],
						dx,
						dy,
						left_down);

					if (mouse_log_enabled && (dx == 0) && (dy == 0)
						&& (left_down == host_mouse_button_down))
					{
						fprintf(stderr, "OSGLUFB: mouse noop packet\n");
					}

					mouse_acc_dx += (si4r)dx;
					mouse_acc_dy += (si4r)dy;
					mouse_button_seen = trueblnr;
					mouse_button_latest = left_down;
					mouse_packet_have = 0;
				}
			}
		}
		if ((n < 0) && (errno != EAGAIN) && (errno != EWOULDBLOCK)) {
			fprintf(stderr, "OSGLUFB: Mouse read error: %s\n", strerror(errno));
		}

		if ((mouse_acc_dx != 0) || (mouse_acc_dy != 0)) {
			mouse_pending_dx += mouse_acc_dx;
			mouse_pending_dy += mouse_acc_dy;
			if (mouse_pending_dx > 8192) {
				mouse_pending_dx = 8192;
			} else if (mouse_pending_dx < -8192) {
				mouse_pending_dx = -8192;
			}
			if (mouse_pending_dy > 8192) {
				mouse_pending_dy = 8192;
			} else if (mouse_pending_dy < -8192) {
				mouse_pending_dy = -8192;
			}
		}

		if (mouse_button_seen) {
			mouse_pending_button_valid = trueblnr;
			mouse_pending_button_state = mouse_button_latest;
		}

		if ((mouse_pending_dx != 0) || (mouse_pending_dy != 0)) {
			if (mouse_log_enabled && mouse_got_new_packet) {
				fprintf(stderr,
					"OSGLUFB: mouse coalesced move queued: dx=%ld dy=%ld\n",
					(long)mouse_pending_dx,
					(long)mouse_pending_dy);
			}
			if (QueueMouseDelta(mouse_pending_dx, mouse_pending_dy)) {
				mouse_pending_dx = 0;
				mouse_pending_dy = 0;
			}
		}

		if (mouse_pending_button_valid) {
			if (QueueMouseButton(mouse_pending_button_state)) {
				mouse_pending_button_valid = falseblnr;
			}
		}
	}
}