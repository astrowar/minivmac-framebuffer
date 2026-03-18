/*
	OSGLUFB_Drives.c

	Drive/disk image and ROM loading module for Linux Framebuffer port.
*/

#include "OSGLUFB_Common.h"

/* --- Drives --- */

#define NotAfileRef NULL

LOCALVAR blnr ROM_loaded = falseblnr;

LOCALVAR FILE *Drives[NumDrives];

/* Armazena os caminhos dos discos para re-inserção após reboot */
LOCALVAR char *DiskPaths[NumDrives];

LOCALPROC DiskInsertNotify(tDrive Drive_No, blnr locked)
{
	vSonyInsertedMask |= ((ui5b)1 << Drive_No);
	if (!locked) {
		vSonyWritableMask |= ((ui5b)1 << Drive_No);
	}
}

LOCALPROC DiskEjectedNotify(tDrive Drive_No)
{
	vSonyInsertedMask &= ~((ui5b)1 << Drive_No);
	vSonyWritableMask &= ~((ui5b)1 << Drive_No);
}

LOCALFUNC blnr FirstFreeDisk(tDrive *Drive_No)
{
	tDrive i;
	for (i = 0; i < NumDrives; ++i) {
		if (0 == (vSonyInsertedMask & ((ui5b)1 << i))) {
			if (NULL != Drive_No) {
				*Drive_No = i;
			}
			return trueblnr;
		}
	}
	return falseblnr;
}

LOCALPROC MacMsg(char *briefMsg, char *longMsg, blnr fatal)
{
	(void)fatal;
	if (NULL != briefMsg) {
		fprintf(stderr, "%s\n", briefMsg);
	}
	if (NULL != longMsg) {
		fprintf(stderr, "%s\n", longMsg);
	}
}

LOCALPROC InitDrives(void)
{
	tDrive i;

	for (i = 0; i < NumDrives; ++i) {
		Drives[i] = NotAfileRef;
		DiskPaths[i] = NULL;
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

	/* Libera o caminho do disco */
	if (DiskPaths[Drive_No] != NULL) {
		free(DiskPaths[Drive_No]);
		DiskPaths[Drive_No] = NULL;
	}

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
		/* Too many images - just fail silently */
	} else {
		Drives[Drive_No] = refnum;
		DiskInsertNotify(Drive_No, locked);

		/* Salva o caminho do disco para re-inserção após reboot */
		{
			ui5b L = strlen(drivepath);
			char *p = malloc(L + 1);
			if (p != NULL) {
				(void) memcpy(p, drivepath, L + 1);
				DiskPaths[Drive_No] = p;
			}
		}

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
			/* Could show error here */
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

/* --- ROM loading --- */

LOCALFUNC tMacErr LoadMacRomFrom(char *path)
{
	ui5r CheckSum;
	ui5r CheckSumActual;
	ui5r i;
	ui3p p;
	tMacErr err = mnvm_miscErr;
	if (NULL == path) {
		return mnvm_fnfErr;
	}
	FILE *f = fopen(path, "rb");
	if (f != NULL) {
		size_t n = fread(ROM, 1, kROM_Size, f);
		fclose(f);
		if (n == kROM_Size) {
			CheckSum =
				((ui5r)ROM[0] << 24)
				| ((ui5r)ROM[1] << 16)
				| ((ui5r)ROM[2] << 8)
				| (ui5r)ROM[3];

			if ((CheckSum != kRomCheckSum1)
				&& (CheckSum != kRomCheckSum2)
				&& (CheckSum != kRomCheckSum3))
			{
				fprintf(stderr,
					"OSGLUFB: Unsupported ROM checksum 0x%08lX for %s\n",
					(unsigned long)CheckSum, path);
				err = mnvm_miscErr;
			} else {
				CheckSumActual = 0;
				p = ROM + 4;
				for (i = (kCheckSumRom_Size - 4) >> 1; i != 0; --i) {
					CheckSumActual += ((ui5r)p[0] << 8) | (ui5r)p[1];
					p += 2;
				}

				if (CheckSum != CheckSumActual) {
					fprintf(stderr,
						"OSGLUFB: Corrupted ROM checksum for %s (header=0x%08lX calc=0x%08lX)\n",
						path,
						(unsigned long)CheckSum,
						(unsigned long)CheckSumActual);
					err = mnvm_miscErr;
				} else {
					ROM_loaded = trueblnr;
					err = mnvm_noErr;
				}
			}
		} else {
			err = mnvm_miscErr;
		}
	} else {
		err = mnvm_fnfErr;
	}
	return err;
}

GLOBALOSGLUFUNC blnr LoadMacRom(void)
{
	tMacErr err;

	if (! ROM_loaded) {
		if (rom_path != NULL) {
			err = LoadMacRomFrom(rom_path);
			if (mnvm_noErr == err) {
				return trueblnr;
			}
		}

		err = LoadMacRomFrom(RomFileName);
		if (mnvm_noErr == err) {
			return trueblnr;
		}

		return falseblnr;
	}
	return trueblnr;
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

/* Re-insere discos que foram ejectados (para boot/reboot) */
GLOBALOSGLUFUNC blnr ReInsertEjectedDisks(void)
{
	tDrive i;
	blnr anyInserted = falseblnr;

	for (i = 0; i < NumDrives; ++i) {
		if (DiskPaths[i] != NULL && !vSonyIsInserted(i)) {
			/* Tenta re-inserir o disco do caminho salvo */
			if (Sony_Insert1(DiskPaths[i], trueblnr)) {
				anyInserted = trueblnr;
			}
		}
	}

	return anyInserted;
}

GLOBALOSGLUFUNC blnr LoadInitialImages(void)
{
	/* Primeiro tenta re-inserir discos ejectados */
	ReInsertEjectedDisks();

	/* Se não houver discos inseridos, tenta carregar disk?.dsk */
	if (! AnyDiskInserted()) {
		int i;

		for (i = 1; Sony_InsertIth(i); ++i) {
		}
	}

	return trueblnr;
}

GLOBALOSGLUFUNC blnr InsertDiskImage(char *path)
{
	if (NULL == path) {
		return falseblnr;
	}

	return Sony_Insert1(path, falseblnr);
}