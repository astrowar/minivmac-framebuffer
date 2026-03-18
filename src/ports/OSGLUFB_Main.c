/*
	OSGLUFB_Main.c

	Main entry point and stubs for Linux Framebuffer port.
*/

#include "OSGLUFB_Common.h"
#include "COMOSGLU.h"
#include "PBUFSTDC.h"
#include "PROGMAIN.h"
#include <limits.h>
#include <sys/stat.h>
#include <sys/time.h>

/* Hook para quando o Mac reinicia/shutdown */
GLOBALOSGLUPROC OnMacShutdown(void)
{
	fprintf(stderr, "\n=== MAC SHUTDOWN/RESET DETECTED ===\n");
}

char *d_arg = NULL;
char *n_arg = NULL;
char *rom_path = NULL;
blnr g_test_mode = falseblnr;
int g_rotate_degrees = 0;
double g_scale_factor = 1.0;
int g_offset_x = 0;
int g_offset_y = 0;

#if CanGetAppPath
char *app_parent = NULL;
char *pref_dir = NULL;
#endif

LOCALVAR char *test_snapshot_path = "frame300.ppm";
LOCALVAR si5b test_duration_usec = 10 * 1000 * 1000;
LOCALVAR si5b test_start_usec = 0;
LOCALVAR blnr test_snapshot_done = falseblnr;
LOCALVAR ui5b TrueEmulatedTime = 0;
LOCALVAR char *cmd_disk_paths[NumDrives];
LOCALVAR int cmd_disk_count = 0;
LOCALVAR int frame_skip = 0;
LOCALVAR int frame_skip_count = 0;

LOCALFUNC blnr PathLooksLikeROM(char *path)
{
	struct stat st;

	if (NULL == path) {
		return falseblnr;
	}

	if (0 != stat(path, &st)) {
		return falseblnr;
	}

	if (!S_ISREG(st.st_mode)) {
		return falseblnr;
	}

	return ((ui5b)st.st_size == (ui5b)kROM_Size) ? trueblnr : falseblnr;
}

LOCALPROC ReserveAllocAll(void)
{
	ReserveAllocOneBlock(&ROM, kROM_Size, 5, falseblnr);

	ReserveAllocOneBlock(&screencomparebuff,
		vMacScreenNumBytes, 5, trueblnr);
#if WantScalingBuff
	ReserveAllocOneBlock(&ScalingBuff,
		ScalingBuffsz, 5, falseblnr);
#endif
#if WantScalingTabl
	ReserveAllocOneBlock(&ScalingTabl,
		ScalingTablsz, 5, falseblnr);
#endif

	EmulationReserveAlloc();
}

LOCALFUNC blnr AllocMyMemory(void)
{
	uimr n;
	blnr IsOk = falseblnr;

	ReserveAllocOffset = 0;
	ReserveAllocBigBlock = nullpr;
	ReserveAllocAll();
	n = ReserveAllocOffset;
	ReserveAllocBigBlock = (ui3p)calloc(1, n);
	if (NULL == ReserveAllocBigBlock) {
		fprintf(stderr, "OSGLUFB: Out of memory allocating %lu bytes\n",
			(unsigned long)n);
	} else {
		ReserveAllocOffset = 0;
		ReserveAllocAll();
		if (n == ReserveAllocOffset) {
			IsOk = trueblnr;
		}
	}

	return IsOk;
}

LOCALPROC UnallocMyMemory(void)
{
	if (nullpr != ReserveAllocBigBlock) {
		free((char *)ReserveAllocBigBlock);
		ReserveAllocBigBlock = nullpr;
	}
}

/* Forward declarations from other modules */
GLOBALOSGLUFUNC blnr fb_init(void);
GLOBALOSGLUPROC fb_shutdown(void);
GLOBALOSGLUPROC setup_kbd_raw(void);
GLOBALOSGLUPROC restore_kbd(void);
GLOBALOSGLUFUNC blnr LoadMacRom(void);
GLOBALOSGLUFUNC blnr LoadInitialImages(void);

GLOBALOSGLUFUNC ui3p GetCurDrawBuff(void)
{
	return screencomparebuff;
}

LOCALFUNC si5b GetNowUsec(void)
{
	struct timeval tv;
	(void)gettimeofday(&tv, NULL);
	return ((si5b)tv.tv_sec * 1000000) + (si5b)tv.tv_usec;
}

GLOBALOSGLUPROC Keyboard_UpdateKeyMap2(ui3r key, blnr isDown)
{
	Keyboard_UpdateKeyMap(key, isDown);
}

LOCALFUNC blnr EventPostBegin(blnr *saved_recover)
{
	*saved_recover = MyEvtQNeedRecover;
	MyEvtQNeedRecover = falseblnr;
	return trueblnr;
}

LOCALFUNC blnr EventPostEnd(blnr saved_recover)
{
	blnr ok = !MyEvtQNeedRecover;
	MyEvtQNeedRecover = saved_recover || MyEvtQNeedRecover;
	return ok;
}

GLOBALOSGLUFUNC blnr OSGLUFB_PostKey(ui3r key, blnr down)
{
	blnr saved_recover;
	(void)EventPostBegin(&saved_recover);
	Keyboard_UpdateKeyMap(key, down);
	return EventPostEnd(saved_recover);
}

GLOBALOSGLUFUNC blnr OSGLUFB_PostMouseDelta(si4r dh, si4r dv)
{
	blnr saved_recover;
	(void)EventPostBegin(&saved_recover);
	MyMousePositionSetDelta((ui4r)dh, (ui4r)dv);
	return EventPostEnd(saved_recover);
}

GLOBALOSGLUFUNC blnr OSGLUFB_PostMouseButton(blnr down)
{
	blnr saved_recover;
	(void)EventPostBegin(&saved_recover);
	MyMouseButtonSet(down);
	return EventPostEnd(saved_recover);
}

GLOBALOSGLUPROC WarnMsgUnsupportedDisk(void)
{
	fprintf(stderr, "OSGLUFB: Unsupported disk\n");
}

GLOBALOSGLUFUNC blnr ExtraTimeNotOver(void)
{
	return falseblnr;
}

GLOBALOSGLUPROC DoneWithDrawingForTick(void)
{
	blnr should_draw = trueblnr;

	if (frame_skip > 0) {
		if (frame_skip_count < frame_skip) {
			should_draw = falseblnr;
			++frame_skip_count;
		} else {
			frame_skip_count = 0;
		}
	}

	if (should_draw) {
		fb_draw();
	}

	if (g_test_mode && !test_snapshot_done) {
		si5b elapsed = GetNowUsec() - test_start_usec;
		if (elapsed >= test_duration_usec) {
			if (!should_draw) {
				fb_draw();
			}
			if (fb_dump_snapshot(test_snapshot_path)) {
				fprintf(stderr, "OSGLUFB: Test snapshot written to %s\n", test_snapshot_path);
			} else {
				fprintf(stderr, "OSGLUFB: Failed to write test snapshot %s\n", test_snapshot_path);
			}
			test_snapshot_done = trueblnr;
			ForceMacOff = trueblnr;
		}
	}
}

GLOBALOSGLUPROC WaitForNextTick(void)
{
	fb_poll_input();
	usleep(16666);
	++TrueEmulatedTime;
	OnTrueTime = TrueEmulatedTime;
}

/* --- Stub functions --- */

EXPORTFUNC void ToggleWantFullScreen(void)
{
	/* Stub: do nothing */
	(void)0;
}

EXPORTFUNC tMacErr NativeTextToMacRomanPbuf(char *x, tPbuf *r)
{
	(void)x;
	(void)r;
	return mnvm_miscErr;
}

/* --- Sound stubs --- */

#if MySoundEnabled

LOCALVAR int MySound_File = -1;
LOCALVAR int MySound_SampRate = 44100;
LOCALVAR int MySound_NumChannels = 2;
LOCALVAR int MySound_SampSize = 16;

EXPORTOSGLUFUNC tpSoundSamp MySound_BeginWrite(ui4r n, ui4r *actL)
{
	(void)n;
	if (actL != NULL) {
		*actL = 0;
	}
	return NULL;
}

EXPORTOSGLUPROC MySound_EndWrite(ui4r actL)
{
	(void)actL;
}

#endif /* MySoundEnabled */

/* --- Text clip exchange --- */

#if IncludeHostTextClipExchange

EXPORTOSGLUFUNC tMacErr HTCEexport(tPbuf i)
{
	(void)i;
	return mnvm_miscErr;
}

EXPORTOSGLUFUNC tMacErr HTCEimport(tPbuf *r)
{
	(void)r;
	return mnvm_miscErr;
}

#endif /* IncludeHostTextClipExchange */

/* --- Usage --- */

LOCALPROC Usage(void)
{
	fprintf(stderr, "Usage: minivmac [options] [rom] [disk]\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -r rom        Specify ROM file\n");
	fprintf(stderr, "  -d dir        Specify disk directory\n");
	fprintf(stderr, "  --skip n      Skip n frames between draws\n");
	fprintf(stderr, "  --rotate deg  Rotate output (0, 90, 180, 270)\n");
	fprintf(stderr, "  --scale f     Scale factor (e.g., 1.15)\n");
	fprintf(stderr, "  --offset-x n  Horizontal offset in pixels (positive = right)\n");
	fprintf(stderr, "  --offset-y n  Vertical offset in pixels (positive = down)\n");
	fprintf(stderr, "  -h            Show this help\n");
	fprintf(stderr, "  --test        Run in test mode\n");
}

/* --- Main entry point --- */

int main(int argc, char *argv[])
{
	int i;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (strcmp(argv[i], "--test") == 0) {
				g_test_mode = trueblnr;
				continue;
			} else if (strcmp(argv[i], "--skip") == 0) {
				long parsed;
				char *endp;

				if (++i >= argc) {
					Usage();
					return 1;
				}

				errno = 0;
				parsed = strtol(argv[i], &endp, 10);
				if ((errno != 0) || (endp == argv[i]) || (*endp != '\0')
					|| (parsed < 0) || (parsed > INT_MAX))
				{
					fprintf(stderr, "OSGLUFB: invalid --skip value: %s\n", argv[i]);
					Usage();
					return 1;
				}

				frame_skip = (int)parsed;
				continue;
			} else if (strcmp(argv[i], "--rotate") == 0) {
				long parsed;
				char *endp;

				if (++i >= argc) {
					Usage();
					return 1;
				}

				errno = 0;
				parsed = strtol(argv[i], &endp, 10);
				if ((errno != 0) || (endp == argv[i]) || (*endp != '\0')) {
					fprintf(stderr, "OSGLUFB: invalid --rotate value: %s\n", argv[i]);
					Usage();
					return 1;
				}

				parsed %= 360;
				if (parsed < 0) {
					parsed += 360;
				}

				if (!((parsed == 0) || (parsed == 90)
					|| (parsed == 180) || (parsed == 270)))
				{
					fprintf(stderr,
						"OSGLUFB: --rotate only accepts 0, 90, 180 or 270\n");
					Usage();
					return 1;
				}

				g_rotate_degrees = (int)parsed;
				continue;
			} else if (strcmp(argv[i], "--scale") == 0) {
				double parsed;
				char *endp;

				if (++i >= argc) {
					Usage();
					return 1;
				}

				errno = 0;
				parsed = strtod(argv[i], &endp);
				if ((errno != 0) || (endp == argv[i]) || (*endp != '\0')) {
					fprintf(stderr, "OSGLUFB: invalid --scale value: %s\n", argv[i]);
					Usage();
					return 1;
				}

				if (parsed <= 0.0) {
					fprintf(stderr, "OSGLUFB: --scale must be positive\n");
					Usage();
					return 1;
				}

				g_scale_factor = parsed;
				continue;
			} else if (strcmp(argv[i], "--offset-x") == 0) {
				long parsed;
				char *endp;

				if (++i >= argc) {
					Usage();
					return 1;
				}

				errno = 0;
				parsed = strtol(argv[i], &endp, 10);
				if ((errno != 0) || (endp == argv[i]) || (*endp != '\0')) {
					fprintf(stderr, "OSGLUFB: invalid --offset-x value: %s\n", argv[i]);
					Usage();
					return 1;
				}

				g_offset_x = (int)parsed;
				continue;
			} else if (strcmp(argv[i], "--offset-y") == 0) {
				long parsed;
				char *endp;

				if (++i >= argc) {
					Usage();
					return 1;
				}

				errno = 0;
				parsed = strtol(argv[i], &endp, 10);
				if ((errno != 0) || (endp == argv[i]) || (*endp != '\0')) {
					fprintf(stderr, "OSGLUFB: invalid --offset-y value: %s\n", argv[i]);
					Usage();
					return 1;
				}

				g_offset_y = (int)parsed;
				continue;
			} else if (strcmp(argv[i], "--snapshot") == 0) {
				if (++i >= argc) {
					Usage();
					return 1;
				}
				test_snapshot_path = argv[i];
				continue;
			}

			switch (argv[i][1]) {
			case 'r':
				if (++i >= argc) {
					Usage();
					return 1;
				}
				rom_path = argv[i];
				break;
			case 'd':
				if (++i >= argc) {
					Usage();
					return 1;
				}
				d_arg = argv[i];
				break;
			case 'h':
				Usage();
				return 0;
			default:
				Usage();
				return 1;
			}
		} else {
			if ((rom_path == NULL) && PathLooksLikeROM(argv[i])) {
				rom_path = argv[i];
			} else if (cmd_disk_count < NumDrives) {
				cmd_disk_paths[cmd_disk_count++] = argv[i];
			}
		}
	}

	if (!fb_init()) {
		fprintf(stderr, "OSGLUFB: Framebuffer initialization failed\n");
		return 1;
	}

	if (!AllocMyMemory()) {
		fprintf(stderr, "OSGLUFB: Memory allocation failed\n");
		fb_shutdown();
		return 1;
	}

	setup_kbd_raw();
	frame_skip_count = 0;
	test_start_usec = GetNowUsec();

	if (!LoadMacRom()) {
		fprintf(stderr, "OSGLUFB: Failed to load ROM\n");
		restore_kbd();
		fb_shutdown();
		UnallocMyMemory();
		return 1;
	}
	if (cmd_disk_count > 0) {
		int k;
		for (k = 0; k < cmd_disk_count; ++k) {
			InsertDiskImage(cmd_disk_paths[k]);
		}
	} else {
		LoadInitialImages();
	}

	ProgramMain();

	restore_kbd();
	fb_shutdown();
	UnallocMyMemory();

	return 0;
}