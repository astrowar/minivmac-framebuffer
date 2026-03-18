/*
	OSGLUFB.c

	Operating System GLUe for Linux Framebuffer

	All operating system dependent code for the
	Linux Framebuffer should go here.
*/

#include "OSGCOMUI.h"
#include "OSGCOMUD.h"

/* Add missing type definitions for ui8, ui16 and ui32 */
#ifndef ui8
typedef unsigned char ui8;
#endif
#ifndef ui16
typedef unsigned short ui16;
#endif
#ifndef ui32
typedef unsigned int ui32;
#endif

#ifdef WantOSGLUFB

/* --- some simple utilities --- */

GLOBALOSGLUPROC MyMoveBytes(anyp srcPtr, anyp destPtr, si5b byteCount)
{
	(void) memcpy((char *)destPtr, (char *)srcPtr, byteCount);
}

/* --- control mode and internationalization --- */

#define NeedCell2PlainAsciiMap 1

#include "INTLCHAR.h"

/* --- environment variables --- */

LOCALVAR char *d_arg = NULL;

#if CanGetAppPath
LOCALVAR char *app_parent = NULL;
#endif

#define MyPathSep '/'

LOCALFUNC tMacErr ChildPath(char *x, char *y, char **r)
{
	tMacErr err = mnvm_miscErr;
	int nx = strlen(x);
	int ny = strlen(y);
	{
		if ((nx > 0) && (MyPathSep == x[nx - 1])) {
			--nx;
		}
		{
			int nr = nx + 1 + ny;
			char *p = malloc(nr + 1);
			if (p != NULL) {
				char *p2 = p;
				(void) memcpy(p2, x, nx);
				p2 += nx;
				*p2++ = MyPathSep;
				(void) memcpy(p2, y, ny);
				p2 += ny;
				*p2 = 0;
				*r = p;
				err = mnvm_noErr;
			}
		}
	}

	return err;
}

LOCALPROC MyMayFree(char *p)
{
	if (NULL != p) {
		free(p);
	}
}

/* --- sending debugging info to file --- */

#if dbglog_HAVE

#ifndef dbglog_ToStdErr
#define dbglog_ToStdErr 0
#endif

#if ! dbglog_ToStdErr
LOCALVAR FILE *dbglog_File = NULL;
#endif

LOCALFUNC blnr dbglog_open0(void)
{
#if dbglog_ToStdErr
	return trueblnr;
#else
#if CanGetAppPath
	if (NULL == app_parent)
#endif
	{
		dbglog_File = fopen("dbglog.txt", "w");
	}
#if CanGetAppPath
	else {
		char *t = NULL;

		if (mnvm_noErr == ChildPath(app_parent, "dbglog.txt", &t)) {
			dbglog_File = fopen(t, "w");
		}

		MyMayFree(t);
	}
#endif

	return (NULL != dbglog_File);
#endif
}

LOCALPROC dbglog_write0(char *s, uimr L)
{
#if dbglog_ToStdErr
	(void) fwrite(s, 1, L, stderr);
#else
	if (dbglog_File != NULL) {
		(void) fwrite(s, 1, L, dbglog_File);
	}
#endif
}

LOCALPROC dbglog_close0(void)
{
#if ! dbglog_ToStdErr
	if (dbglog_File != NULL) {
		fclose(dbglog_File);
		dbglog_File = NULL;
	}
#endif
}

#endif

/* --- information about the environment --- */

#define WantColorTransValid 0

#include "COMOSGLU.h"

#include "PBUFSTDC.h"

#include "CONTROLM.h"

/* --- text translation --- */

#if IncludePbufs
LOCALFUNC tMacErr NativeTextToMacRomanPbuf(char *x, tPbuf *r);
#endif

LOCALPROC NativeStrFromCStr(char *r, char *s)
{
	ui3b ps[ClStrMaxLength];
	int i;
	int L;

	ClStrFromSubstCStr(&L, ps, s);

	for (i = 0; i < L; ++i) {
		r[i] = Cell2PlainAsciiMap[ps[i]];
	}

	r[L] = 0;
}

/* --- drives --- */

#define NotAfileRef NULL

LOCALVAR FILE *Drives[NumDrives];

LOCALPROC InitDrives(void)
{
	tDrive i;

	for (i = 0; i < NumDrives; ++i) {
		Drives[i] = NotAfileRef;
	}
}

GLOBALOSGLUFUNC tMacErr vSonyTransfer(blnr IsWrite, ui3p Buffer,
	tDrive Drive_No, ui5r Sony_Start, ui5r Sony_Count,
	ui5r *Sony_ActCount)
{
	tMacErr err = mnvm_miscErr;
	FILE *refnum = Drives[Drive_No];
	ui5r NewSony_Count = 0;

	if (0 == fseek(refnum, Sony_Start, SEEK_SET)) {
		if (IsWrite) {
			NewSony_Count = fwrite(Buffer, 1, Sony_Count, refnum);
		} else {
			NewSony_Count = fread(Buffer, 1, Sony_Count, refnum);
		}

		if (NewSony_Count == Sony_Count) {
			err = mnvm_noErr;
		}
	}

	if (nullpr != Sony_ActCount) {
		*Sony_ActCount = NewSony_Count;
	}

	return err;
}

GLOBALOSGLUFUNC tMacErr vSonyGetSize(tDrive Drive_No, ui5r *Sony_Count)
{
	tMacErr err = mnvm_miscErr;
	FILE *refnum = Drives[Drive_No];
	long v;

	if (0 == fseek(refnum, 0, SEEK_END)) {
		v = ftell(refnum);
		if (v >= 0) {
			*Sony_Count = v;
			err = mnvm_noErr;
		}
	}

	return err;
}

#if IncludeSonyGetName || IncludeSonyNew
LOCALVAR char *DriveNames[NumDrives];
#endif

LOCALFUNC tMacErr vSonyEject0(tDrive Drive_No, blnr deleteit)
{
	FILE *refnum = Drives[Drive_No];

	DiskEjectedNotify(Drive_No);

	fclose(refnum);
	Drives[Drive_No] = NotAfileRef;

#if IncludeSonyGetName || IncludeSonyNew
	{
		char *s = DriveNames[Drive_No];
		if (NULL != s) {
			if (deleteit) {
				remove(s);
			}
			free(s);
			DriveNames[Drive_No] = NULL;
		}
	}
#endif

	return mnvm_noErr;
}

GLOBALOSGLUFUNC tMacErr vSonyEject(tDrive Drive_No)
{
	return vSonyEject0(Drive_No, falseblnr);
}

#if IncludeSonyNew
GLOBALOSGLUFUNC tMacErr vSonyEjectDelete(tDrive Drive_No)
{
	return vSonyEject0(Drive_No, trueblnr);
}
#endif

LOCALPROC UnInitDrives(void)
{
	tDrive i;

	for (i = 0; i < NumDrives; ++i) {
		if (vSonyIsInserted(i)) {
			(void) vSonyEject(i);
		}
	}
}

#if IncludeSonyGetName
GLOBALOSGLUFUNC tMacErr vSonyGetName(tDrive Drive_No, tPbuf *r)
{
	char *drivepath = DriveNames[Drive_No];
	if (NULL == drivepath) {
		return mnvm_miscErr;
	} else {
		char *s = strrchr(drivepath, MyPathSep);
		if (NULL == s) {
			s = drivepath;
		} else {
			++s;
		}
		return NativeTextToMacRomanPbuf(s, r);
	}
}
#endif

LOCALFUNC blnr Sony_Insert0(FILE *refnum, blnr locked,
	char *drivepath)
{
	tDrive Drive_No;
	blnr IsOk = falseblnr;

	if (! FirstFreeDisk(&Drive_No)) {
		MacMsg(kStrTooManyImagesTitle, kStrTooManyImagesMessage,
			falseblnr);
	} else {
		Drives[Drive_No] = refnum;
		DiskInsertNotify(Drive_No, locked);

#if IncludeSonyGetName || IncludeSonyNew
		{
			ui5b L = strlen(drivepath);
			char *p = malloc(L + 1);
			if (p != NULL) {
				(void) memcpy(p, drivepath, L + 1);
			}
			DriveNames[Drive_No] = p;
		}
#endif

		IsOk = trueblnr;
	}

	if (! IsOk) {
		fclose(refnum);
	}

	return IsOk;
}

LOCALFUNC blnr Sony_Insert1(char *drivepath, blnr silentfail)
{
	blnr locked = falseblnr;
	FILE *refnum = fopen(drivepath, "rb+");
	if (NULL == refnum) {
		locked = trueblnr;
		refnum = fopen(drivepath, "rb");
	}
	if (NULL == refnum) {
		if (! silentfail) {
			MacMsg(kStrOpenFailTitle, kStrOpenFailMessage, falseblnr);
		}
	} else {
		return Sony_Insert0(refnum, locked, drivepath);
	}
	return falseblnr;
}

#if IncludeSonyNew
LOCALFUNC blnr WriteZero(FILE *refnum, ui5b L)
{
#define ZeroBufferSize 2048
	ui5b i;
	ui3b buffer[ZeroBufferSize];

	memset(&buffer, 0, ZeroBufferSize);

	while (L > 0) {
		i = (L > ZeroBufferSize) ? ZeroBufferSize : L;
		if (fwrite(buffer, 1, i, refnum) != i) {
			return falseblnr;
		}
		L -= i;
	}
	return trueblnr;
}
#endif

#if IncludeSonyNew
LOCALPROC MakeNewDisk0(ui5b L, char *drivepath)
{
	blnr IsOk = falseblnr;
	FILE *refnum = fopen(drivepath, "wb+");
	if (NULL == refnum) {
		MacMsg(kStrOpenFailTitle, kStrOpenFailMessage, falseblnr);
	} else {
		if (WriteZero(refnum, L)) {
			IsOk = Sony_Insert0(refnum, falseblnr, drivepath);
			refnum = NULL;
		}
		if (refnum != NULL) {
			fclose(refnum);
		}
		if (! IsOk) {
			(void) remove(drivepath);
		}
	}
}
#endif

#if IncludeSonyNew
LOCALPROC MakeNewDisk(ui5b L, char *drivename)
{
	char *d =
#if CanGetAppPath
		(NULL == d_arg) ? app_parent :
#endif
		d_arg;

	if (NULL == d) {
		MakeNewDisk0(L, drivename);
	} else {
		tMacErr err;
		char *t = NULL;
		char *t2 = NULL;

		if (mnvm_noErr == (err = ChildPath(d, "out", &t)))
		if (mnvm_noErr == (err = ChildPath(t, drivename, &t2)))
		{
			MakeNewDisk0(L, t2);
		}

		MyMayFree(t2);
		MyMayFree(t);
	}
}
#endif

#if IncludeSonyNew
LOCALPROC MakeNewDiskAtDefault(ui5b L)
{
	char s[ClStrMaxLength + 1];

	NativeStrFromCStr(s, "untitled.dsk");
	MakeNewDisk(L, s);
}
#endif

/* --- ROM --- */

LOCALFUNC tMacErr LoadMacRomFrom(char *path)
{
	tMacErr err;
	FILE *ROM_File;
	int File_Size;

	ROM_File = fopen(path, "rb");
	if (NULL == ROM_File) {
		err = mnvm_fnfErr;
	} else {
		File_Size = fread(ROM, 1, kROM_Size, ROM_File);
		if (kROM_Size != File_Size) {
			if (feof(ROM_File)) {
				MacMsgOverride(kStrShortROMTitle,
					kStrShortROMMessage);
				err = mnvm_eofErr;
			} else {
				MacMsgOverride(kStrNoReadROMTitle,
					kStrNoReadROMMessage);
				err = mnvm_miscErr;
			}
		} else {
			err = ROM_IsValid();
		}
		fclose(ROM_File);
	}

	return err;
}

LOCALFUNC blnr Sony_Insert1a(char *drivepath, blnr silentfail)
{
	blnr v;

	if (! ROM_loaded) {
		v = (mnvm_noErr == LoadMacRomFrom(drivepath));
	} else {
		v = Sony_Insert1(drivepath, silentfail);
	}

	return v;
}

LOCALFUNC blnr Sony_Insert2(char *s)
{
	char *d =
#if CanGetAppPath
		(NULL == d_arg) ? app_parent :
#endif
		d_arg;
	blnr IsOk = falseblnr;

	if (NULL == d) {
		IsOk = Sony_Insert1(s, trueblnr);
	} else {
		char *t;

		if (mnvm_noErr == ChildPath(d, s, &t)) {
			IsOk = Sony_Insert1(t, trueblnr);
			free(t);
		}
	}

	return IsOk;
}

LOCALFUNC blnr Sony_InsertIth(int i)
{
	blnr v;

	if ((i > 9) || ! FirstFreeDisk(nullpr)) {
		v = falseblnr;
	} else {
		char s[] = "disk?.dsk";

		s[4] = '0' + i;

		v = Sony_Insert2(s);
	}

	return v;
}

LOCALFUNC blnr LoadInitialImages(void)
{
	if (! AnyDiskInserted()) {
		int i;

		for (i = 1; Sony_InsertIth(i); ++i) {
		}
	}

	return trueblnr;
}

/* --- framebuffer display --- */

LOCALVAR int fb_fd = -1;
LOCALVAR struct fb_var_screeninfo fb_var;
LOCALVAR struct fb_fix_screeninfo fb_fix;
LOCALVAR ui3p fb_buffer = NULL;
LOCALVAR si4b fb_width = 0;
LOCALVAR si4b fb_height = 0;
LOCALVAR si4b fb_bps = 0;

LOCALPROC fb_init(void)
{
	fb_fd = open("/dev/fb0", O_RDWR);
	if (fb_fd < 0) {
		fprintf(stderr, "OSGLUFB: Cannot open /dev/fb0\n");
		return;
	}

	if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &fb_var) < 0) {
		fprintf(stderr, "OSGLUFB: Failed to get framebuffer info: %s\n",
			strerror(errno));
		close(fb_fd);
		fb_fd = -1;
		return;
	}

	if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &fb_fix) < 0) {
		fprintf(stderr, "OSGLUFB: Failed to get fix info: %s\n",
			strerror(errno));
		close(fb_fd);
		fb_fd = -1;
		return;
	}

	fb_width = fb_var.xres;
	fb_height = fb_var.yres;
	fb_bps = fb_var.bits_per_pixel;

	fb_buffer = (ui3p)mmap(NULL, fb_fix.smem_len, PROT_READ | PROT_WRITE,
		MAP_SHARED, fb_fd, 0);
	if (fb_buffer == (ui3p)MAP_FAILED) {
		fb_buffer = NULL;
		fprintf(stderr, "OSGLUFB: Failed to mmap framebuffer: %s\n",
			strerror(errno));
		close(fb_fd);
		fb_fd = -1;
		return;
	}

	fprintf(stderr, "OSGLUFB: Framebuffer %dx%d, %dbpp, %d bytes/line\n",
		fb_width, fb_height, fb_bps, fb_fix.line_length);
}

LOCALPROC fb_shutdown(void)
{
	if (fb_buffer != NULL) {
		munmap(fb_buffer, fb_fix.smem_len);
		fb_buffer = NULL;
	}
	if (fb_fd >= 0) {
		close(fb_fd);
		fb_fd = -1;
	}
}

LOCALPROC DoScreen_OutputFrame(ui3p screencurrentbuff)
{
	if (fb_fd < 0 || fb_buffer == NULL) {
		return;
	}

	ui3b *src = screencurrentbuff;
	int src_line_bytes = vMacScreenMonoByteWidth;
	int src_width = vMacScreenWidth;
	int src_height = vMacScreenHeight;
	int fb_linesize = fb_fix.line_length;
	si4b y, x;

	/* Clear to white background */
	memset(fb_buffer, 0xFF, fb_fix.smem_len);

	/* Handle different framebuffer bit depths */
	if (fb_bps == 8) {
		/* 8-bit: simple direct mapping */
		for (y = 0; y < fb_height; y++) {
			int src_y = (y * src_height) / fb_height;
			int src_byte_idx = src_y * src_line_bytes;

			for (x = 0; x < fb_width; x++) {
				int src_x = (x * src_width) / fb_width;
				int src_bit = (src[src_byte_idx + (src_x / 8)] >> (7 - (src_x & 7))) & 1;
				int fb_offset = y * fb_linesize + x;

				if (fb_buffer != NULL && fb_offset < (int)fb_fix.smem_len) {
					fb_buffer[fb_offset] = src_bit ? 0x00 : 0xFF; /* Black on white */
				}
			}
		}
	} else if (fb_bps == 16 || fb_bps == 32) {
		/* 16/32-bit: map to RGB565 or ARGB8888 */
		if (fb_bps == 16) {
			ui16 *fb = (ui16 *)fb_buffer;
			for (y = 0; y < fb_height; y++) {
				int src_y = (y * src_height) / fb_height;
				int src_byte_idx = src_y * src_line_bytes;

				for (x = 0; x < fb_width; x++) {
					int src_x = (x * src_width) / fb_width;
					int src_bit = (src[src_byte_idx + (src_x / 8)] >> (7 - (src_x & 7))) & 1;
					int fb_offset = y * fb_linesize / 2 + x;

					if (fb != NULL && fb_offset < (int)fb_fix.smem_len / 2) {
						/* RGB565: black pixel (0x0000) or white (0xFFFF) */
						fb[fb_offset] = src_bit ? 0x0000 : 0xFFFF;
					}
				}
			}
		} else {
			ui32 *fb = (ui32 *)fb_buffer;
			for (y = 0; y < fb_height; y++) {
				int src_y = (y * src_height) / fb_height;
				int src_byte_idx = src_y * src_line_bytes;

				for (x = 0; x < fb_width; x++) {
					int src_x = (x * src_width) / fb_width;
					int src_bit = (src[src_byte_idx + (src_x / 8)] >> (7 - (src_x & 7))) & 1;
					int fb_offset = y * fb_linesize / 4 + x;

					if (fb != NULL && fb_offset < (int)fb_fix.smem_len / 4) {
						/* ARGB8888: black or white */
						fb[fb_offset] = src_bit ? 0xFF000000 : 0xFFFFFFFF;
					}
				}
			}
		}
	} else if (fb_bps == 24) {
		/* 24-bit: RGB888 */
		ui8 *fb = (ui8 *)fb_buffer;
		for (y = 0; y < fb_height; y++) {
			int src_y = (y * src_height) / fb_height;
			int src_byte_idx = src_y * src_line_bytes;

			for (x = 0; x < fb_width; x++) {
				int src_x = (x * src_width) / fb_width;
				int src_bit = (src[src_byte_idx + (src_x / 8)] >> (7 - (src_x & 7))) & 1;
				int fb_offset = y * fb_linesize + x * 3;

				if (fb != NULL && fb_offset < (int)fb_fix.smem_len) {
					ui8 white = 0xFF, black = 0x00;
					fb[fb_offset] = src_bit ? black : white;       /* R */
					fb[fb_offset + 1] = src_bit ? black : white;   /* G */
					fb[fb_offset + 2] = src_bit ? black : white;   /* B */
				}
			}
		}
	}

	/* Flush the framebuffer to ensure display update */
	if (msync(fb_buffer, fb_fix.smem_len, MS_ASYNC) != 0) {
		/* msync may fail on some systems, ignore error */
	}
}

/* --- input handling --- */

LOCALVAR int kbd_fd = -1;
LOCALVAR blnr kbd_raw_mode = falseblnr;
LOCALVAR struct termios kbd_original;

LOCALPROC setup_kbd_raw(void)
{
	struct termios t;

	kbd_fd = open("/dev/tty", O_RDWR);
	if (kbd_fd < 0) {
		kbd_fd = open("/dev/stdin", O_RDWR);
		if (kbd_fd < 0) {
			return;
		}
	}

	if (tcgetattr(kbd_fd, &t) < 0) {
		close(kbd_fd);
		kbd_fd = -1;
		return;
	}

	kbd_original = t;

	t.c_lflag &= ~(ECHO | ICANON | ISIG);
	t.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	t.c_oflag &= ~OPOST;
	t.c_cflag |= (CS8);
	t.c_cc[VMIN] = 1;
	t.c_cc[VTIME] = 0;

	tcsetattr(kbd_fd, TCSAFLUSH, &t);
	kbd_raw_mode = trueblnr;
}

LOCALPROC restore_kbd(void)
{
	if (kbd_raw_mode && kbd_fd >= 0) {
		tcsetattr(kbd_fd, TCSAFLUSH, &kbd_original);
		kbd_raw_mode = falseblnr;
	}
	if (kbd_fd >= 0) {
		close(kbd_fd);
		kbd_fd = -1;
	}
}

/* --- sound --- */

#if MySoundEnabled

EXPORTOSGLUFUNC tpSoundSamp MySound_BeginWrite(ui4r n, ui4r *actL)
{
	*actL = 0;
	return NULL;
}

EXPORTOSGLUPROC MySound_EndWrite(ui4r actL)
{
	(void)actL;
}

#else

EXPORTOSGLUFUNC tpSoundSamp MySound_BeginWrite(ui4r n, ui4r *actL)
{
	*actL = 0;
	return NULL;
}

EXPORTOSGLUPROC MySound_EndWrite(ui4r actL)
{
	(void)actL;
}

#endif

/* --- required exports (stubs) --- */

EXPORTOSGLUFUNC blnr ExtraTimeNotOver(void)
{
	return falseblnr;
}

EXPORTOSGLUPROC DoneWithDrawingForTick(void)
{
	if (fb_fd >= 0) {
		DoScreen_OutputFrame(GetCurDrawBuff());
	}
}

EXPORTOSGLUPROC WaitForNextTick(void)
{
	/* Simple busy-wait loop - replace with proper timing */
}

/* --- text translation stub --- */

#if IncludePbufs
LOCALFUNC tMacErr NativeTextToMacRomanPbuf(char *x, tPbuf *r)
{
	(void)x;
	(void)r;
	return mnvm_miscErr;
}
#endif /* IncludePbufs */

/* --- Host Text Clip Exchange stubs --- */

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
#endif

/* --- main entry point --- */

#include "PROGMAIN.h"

LOCALPROC Usage(void)
{
	fprintf(stderr, "Usage: minivmac [options] [rom] [disk...]\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -d disk      - insert disk image\n");
	fprintf(stderr, "  -h           - show this help\n");
}

int main(int argc, char *argv[])
{
	int i;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
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
			if (d_arg == NULL) {
				d_arg = argv[i];
			}
		}
	}

	fb_init();
	if (fb_fd < 0) {
		fprintf(stderr, "OSGLUFB: Framebuffer initialization failed\n");
		return 1;
	}

	/* Allocate memory for ROM */
	ROM = (ui3p)calloc(1, kROM_Size);
	if (NULL == ROM) {
		fprintf(stderr, "OSGLUFB: Out of memory allocating ROM\n");
		fb_shutdown();
		return 1;
	}

	setup_kbd_raw();

	ProgramMain();

	restore_kbd();
	fb_shutdown();

	return 0;
}

#endif /* WantOSGLUFB */