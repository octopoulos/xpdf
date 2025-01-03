//========================================================================
//
// SplashFontEngine.h
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>

class GList;
class SplashFTFontEngine;
class SplashDTFontEngine;
class SplashDT4FontEngine;
class SplashFontFile;
class SplashFontFileID;
class SplashFont;

//------------------------------------------------------------------------

#define splashFontCacheSize 16

#if HAVE_FREETYPE_H
#	define splashFTNoHinting (1 << 0)
#endif

//------------------------------------------------------------------------
// SplashFontEngine
//------------------------------------------------------------------------

class SplashFontEngine
{
public:
	// Create a font engine.
	SplashFontEngine(
#if HAVE_FREETYPE_H
	    bool     enableFreeType,
	    uint32_t freeTypeFlags,
#endif
	    bool aa);

	~SplashFontEngine();

	// Get a font file from the cache.
	// Returns nullptr if there is no matching entry in the cache.
	SplashFontFile* getFontFile(SplashFontFileID* id);

	// Returns true if [id] refers to a bad font file, i.e., if one of the loadXXXFont functions has returned nullptr for that ID.
	bool checkForBadFontFile(SplashFontFileID* id);

	// Load fonts - these create new SplashFontFile objects.
	SplashFontFile* loadType1Font(SplashFontFileID* idA, LOAD_FONT_ARGS_DEFS(), const char** enc);
	SplashFontFile* loadType1CFont(SplashFontFileID* idA, LOAD_FONT_ARGS_DEFS(), int* codeToGID, const char** enc);
	SplashFontFile* loadOpenTypeT1CFont(SplashFontFileID* idA, LOAD_FONT_ARGS_DEFS(), int* codeToGID, const char** enc);
	SplashFontFile* loadCIDFont(SplashFontFileID* idA, LOAD_FONT_ARGS_DEFS(), int* codeToGID, int codeToGIDLen);
	SplashFontFile* loadOpenTypeCFFFont(SplashFontFileID* idA, LOAD_FONT_ARGS_DEFS(), int* codeToGID, int codeToGIDLen);
	SplashFontFile* loadTrueTypeFont(SplashFontFileID* idA, LOAD_FONT_ARGS_DEFS(), int fontNum, int* codeToGID, int codeToGIDLen, const char* fontName);

	// Get a font - this does a cache lookup first, and if not found, creates a new SplashFont object and adds it to the cache.
	// The matrix, mat = textMat * ctm:
	//    [ mat[0] mat[1] ]
	//    [ mat[2] mat[3] ]
	// specifies the font transform in PostScript style:
	//    [x' y'] = [x y] * mat
	// Note that the Splash y axis points downward.
	SplashFont* getFont(SplashFontFile* fontFile, SplashCoord* textMat, SplashCoord* ctm);

private:
	SplashFont* fontCache[splashFontCacheSize] = {};      //
	GList*      badFontFiles                   = nullptr; // [SplashFontFileID]

#if HAVE_FREETYPE_H
	SplashFTFontEngine* ftEngine = nullptr;
#endif
};
