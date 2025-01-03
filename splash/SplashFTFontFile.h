//========================================================================
//
// SplashFTFontFile.h
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>

#if HAVE_FREETYPE_H
#	include <ft2build.h>
#	include FT_FREETYPE_H
#	include "SplashFontFile.h"

class SplashFontFileID;
class SplashFTFontEngine;

//------------------------------------------------------------------------
// SplashFTFontFile
//------------------------------------------------------------------------

class SplashFTFontFile : public SplashFontFile
{
public:
	static SplashFontFile* loadType1Font(SplashFTFontEngine* engineA, SplashFontFileID* idA, SplashFontType fontTypeA, LOAD_FONT_ARGS_DEFS(A), const char** encA);
	static SplashFontFile* loadCIDFont(SplashFTFontEngine* engineA, SplashFontFileID* idA, SplashFontType fontTypeA, LOAD_FONT_ARGS_DEFS(A), int* codeToGIDA, int codeToGIDLenA);
	static SplashFontFile* loadTrueTypeFont(SplashFTFontEngine* engineA, SplashFontFileID* idA, SplashFontType fontTypeA, LOAD_FONT_ARGS_DEFS(A), int fontNum, int* codeToGIDA, int codeToGIDLenA);

	virtual ~SplashFTFontFile();

	// Create a new SplashFTFont, i.e., a scaled instance of this font
	// file.
	virtual SplashFont* makeFont(SplashCoord* mat, SplashCoord* textMat);

private:
	SplashFTFontFile(SplashFTFontEngine* engineA, SplashFontFileID* idA, SplashFontType fontTypeA, LOAD_FONT_ARGS_DEFS(A), FT_Face faceA, int* codeToGIDA, int codeToGIDLenA);

	SplashFTFontEngine* engine;       //
	FT_Face             face;         //
	int*                codeToGID;    //
	int                 codeToGIDLen; //

	friend class SplashFTFont;
};

#endif // HAVE_FREETYPE_H
