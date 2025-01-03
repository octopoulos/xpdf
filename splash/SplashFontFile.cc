//========================================================================
//
// SplashFontFile.cc
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include <stdio.h>
#ifndef _WIN32
#	include <unistd.h>
#endif
#include "gmempp.h"
#include "SplashFontFile.h"
#include "SplashFontFileID.h"

#ifdef VMS
#	if (__VMS_VER < 70000000)
extern "C" int unlink(char* filename);
#	endif
#endif

//------------------------------------------------------------------------
// SplashFontFile
//------------------------------------------------------------------------

SplashFontFile::SplashFontFile(SplashFontFileID* idA, SplashFontType fontTypeA, LOAD_FONT_ARGS_DEFS(A))
{
	id       = idA;
	fontType = fontTypeA;
#if LOAD_FONTS_FROM_MEM
	fontBuf = fontBufA;
#else
	fileName   = fileNameA;
	deleteFile = deleteFileA;
#endif
	refCnt = 0;
}

SplashFontFile::~SplashFontFile()
{
#if LOAD_FONTS_FROM_MEM
	;
#else
	if (deleteFile)
		unlink(fileName.c_str());
#endif
	delete id;
}

void SplashFontFile::incRefCnt()
{
#if MULTITHREADED
	gAtomicIncrement(&refCnt);
#else
	++refCnt;
#endif
}

void SplashFontFile::decRefCnt()
{
	bool done;

#if MULTITHREADED
	done = gAtomicDecrement(&refCnt) == 0;
#else
	done = --refCnt == 0;
#endif
	if (done)
		delete this;
}
