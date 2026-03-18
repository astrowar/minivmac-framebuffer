/*
	OSGLUFB_Common.h

	Common types, includes, and utilities for Linux Framebuffer port.
	Shared header for all OSGLUFB modules.
*/

#ifndef OSGLUFB_COMMON_H
#define OSGLUFB_COMMON_H

/* Standard includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <termios.h>

/* Project includes */
#include "OSDISPLAY.h"
#include "OSGCOMUI.h"
#include "OSGCOMUD.h"
/* COMOSGLU.h is included in modules that need it */

/* Type definitions */
#ifndef ui8
typedef unsigned char ui8;
#endif
#ifndef ui16
typedef unsigned short ui16;
#endif
#ifndef ui32
typedef unsigned int ui32;
#endif

/* Test mode constants */
#define TEST_FB_WIDTH 640
#define TEST_FB_HEIGHT  480

/* Forward declaration for GetCurDrawBuff */
GLOBALOSGLUFUNC ui3p GetCurDrawBuff(void);

/* Other forward declarations needed by modules */
GLOBALOSGLUPROC Keyboard_UpdateKeyMap2(ui3r key, blnr isDown);

/* Split module exported functions */
GLOBALOSGLUFUNC blnr fb_init(void);
GLOBALOSGLUPROC fb_shutdown(void);
GLOBALOSGLUPROC fb_draw(void);
GLOBALOSGLUFUNC blnr fb_dump_snapshot(char *path);
GLOBALOSGLUPROC setup_kbd_raw(void);
GLOBALOSGLUPROC restore_kbd(void);
GLOBALOSGLUPROC fb_poll_input(void);
GLOBALOSGLUFUNC blnr OSGLUFB_PostKey(ui3r key, blnr down);
GLOBALOSGLUFUNC blnr OSGLUFB_PostMouseDelta(si4r dh, si4r dv);
GLOBALOSGLUFUNC blnr OSGLUFB_PostMouseButton(blnr down);
GLOBALOSGLUFUNC blnr LoadMacRom(void);
GLOBALOSGLUFUNC blnr LoadInitialImages(void);
GLOBALOSGLUFUNC blnr InsertDiskImage(char *path);
GLOBALOSGLUFUNC blnr ReInsertEjectedDisks(void);
EXPORTFUNC void ToggleWantFullScreen(void);
EXPORTFUNC tMacErr NativeTextToMacRomanPbuf(char *x, tPbuf *r);

/* Path separator */
#ifdef _WIN32
#define MyPathSep '\\'
#else
#define MyPathSep '/'
#endif

/* --- Simple utilities --- */

GLOBALOSGLUPROC MyMoveBytes(anyp srcPtr, anyp destPtr, si5b byteCount);

GLOBALOSGLUFUNC tMacErr ChildPath(char *x, char *y, char **r);
GLOBALOSGLUPROC MyMayFree(char *p);

/* --- Debug logging --- */

#if dbglog_HAVE

#ifndef dbglog_ToStdErr
#define dbglog_ToStdErr 0
#endif

#if ! dbglog_ToStdErr
LOCALVAR FILE *dbglog_File;
#endif

LOCALFUNC blnr dbglog_open0(void);
LOCALPROC dbglog_write0(char *s, uimr L);
LOCALPROC dbglog_close0(void);

#endif /* dbglog_HAVE */

/* --- Environment variables shared by split modules --- */

extern char *d_arg;
extern char *n_arg;
extern char *rom_path;
extern blnr g_test_mode;
extern int g_rotate_degrees;

#if CanGetAppPath
extern char *app_parent;
extern char *pref_dir;
#endif

/* --- Shutdown/reset hook --- */
/* This function is called when the emulated Mac shuts down or resets */
GLOBALOSGLUPROC OnMacShutdown(void);

#endif /* OSGLUFB_COMMON_H */