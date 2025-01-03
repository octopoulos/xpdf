//========================================================================
//
// SplashFont.h
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "SplashTypes.h"
#include "SplashMath.h"

struct SplashGlyphBitmap;
struct SplashFontCacheTag;
class SplashFontFile;
class SplashPath;

//------------------------------------------------------------------------

// Fractional positioning uses this many bits to the right of the
// decimal points.
#define splashFontFractionBits 2
#define splashFontFraction     (1 << splashFontFractionBits)
#define splashFontFractionMul  ((SplashCoord)1 / (SplashCoord)splashFontFraction)

//------------------------------------------------------------------------
// SplashFont
//------------------------------------------------------------------------

class SplashFont
{
public:
	SplashFont(SplashFontFile* fontFileA, SplashCoord* matA, SplashCoord* textMatA, bool aaA);

	// This must be called after the constructor, so that the subclass
	// constructor has a chance to compute the bbox.
	void initCache();

	virtual ~SplashFont();

	SplashFontFile* getFontFile() { return fontFile; }

	// Return true if <this> matches the specified font file and matrix.
	bool matches(SplashFontFile* fontFileA, SplashCoord* matA, SplashCoord* textMatA)
	{
		return fontFileA == fontFile && splashAbs(matA[0] - mat[0]) < 0.0001 && splashAbs(matA[1] - mat[1]) < 0.0001 && splashAbs(matA[2] - mat[2]) < 0.0001 && splashAbs(matA[3] - mat[3]) < 0.0001 && splashAbs(textMatA[0] - textMat[0]) < 0.0001 && splashAbs(textMatA[1] - textMat[1]) < 0.0001 && splashAbs(textMatA[2] - textMat[2]) < 0.0001 && splashAbs(textMatA[3] - textMat[3]) < 0.0001;
	}

	// Get a glyph - this does a cache lookup first, and if not found,
	// creates a new bitmap and adds it to the cache.  The <xFrac> and
	// <yFrac> values are splashFontFractionBits bits each, representing
	// the numerators of fractions in [0, 1), where the denominator is
	// splashFontFraction = 1 << splashFontFractionBits.  Subclasses
	// should override this to zero out xFrac and/or yFrac if they don't
	// support fractional coordinates.
	virtual bool getGlyph(int c, int xFrac, int yFrac, SplashGlyphBitmap* bitmap);

	// Rasterize a glyph.  The <xFrac> and <yFrac> values are the same
	// as described for getGlyph.
	virtual bool makeGlyph(int c, int xFrac, int yFrac, SplashGlyphBitmap* bitmap) = 0;

	// Return the path for a glyph.
	virtual SplashPath* getGlyphPath(int c) = 0;

	// Return the font transform matrix.
	SplashCoord* getMatrix() { return mat; }

	// Return the glyph bounding box.
	void getBBox(int* xMinA, int* yMinA, int* xMaxA, int* yMaxA)
	{
		*xMinA = xMin;
		*yMinA = yMin;
		*xMaxA = xMax;
		*yMaxA = yMax;
	}

protected:
	SplashFontFile*     fontFile   = nullptr; //
	SplashCoord         mat[4]     = {};      // font transform matrix (text space -> device space)
	SplashCoord         textMat[4] = {};      // text transform matrix (text space -> user space)
	bool                aa         = false;   // anti-aliasing
	int                 xMin       = 0;       //
	int                 yMin       = 0;       //
	int                 xMax       = 0;       //
	int                 yMax       = 0;       // glyph bounding box
	uint8_t*            cache      = nullptr; // glyph bitmap cache
	SplashFontCacheTag* cacheTags  = nullptr; // cache tags
	int                 glyphW     = 0;       //
	int                 glyphH     = 0;       // size of glyph bitmaps
	int                 glyphSize  = 0;       // size of glyph bitmaps, in bytes
	int                 cacheSets  = 0;       // number of sets in cache
	int                 cacheAssoc = 0;       // cache associativity (glyphs per set)
};
