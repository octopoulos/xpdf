//========================================================================
//
// SplashFont.cc
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include <string.h>
#include "gmem.h"
#include "gmempp.h"
#include "SplashMath.h"
#include "SplashGlyphBitmap.h"
#include "SplashFontFile.h"
#include "SplashFont.h"

//------------------------------------------------------------------------

// font cache size parameters
#define splashFontCacheAssoc   8
#define splashFontCacheMaxSets 8
#define splashFontCacheSize    (128 * 1024)

//------------------------------------------------------------------------

struct SplashFontCacheTag
{
	int   c     = 0;
	short xFrac = 0;
	short yFrac = 0; // x and y fractions
	int   mru   = 0; // valid bit (0x80000000) and MRU index
	int   x     = 0;
	int   y     = 0;
	int   w     = 0;
	int   h     = 0; // offset and size of glyph
};

//------------------------------------------------------------------------
// SplashFont
//------------------------------------------------------------------------

SplashFont::SplashFont(SplashFontFile* fontFileA, SplashCoord* matA, SplashCoord* textMatA, bool aaA)
{
	fontFile = fontFileA;
	fontFile->incRefCnt();
	mat[0]     = matA[0];
	mat[1]     = matA[1];
	mat[2]     = matA[2];
	mat[3]     = matA[3];
	textMat[0] = textMatA[0];
	textMat[1] = textMatA[1];
	textMat[2] = textMatA[2];
	textMat[3] = textMatA[3];
	aa         = aaA;
}

void SplashFont::initCache()
{
	// this should be (max - min + 1), but we add some padding to
	// deal with rounding errors
	glyphW = xMax - xMin + 3;
	glyphH = yMax - yMin + 3;
	if (glyphW > 1000 || glyphH > 1000)
	{
		// if the glyphs are too large, don't cache them -- setting the
		// cache bitmap size to something tiny will cause getGlyph() to
		// fall back to the uncached case
		glyphW = glyphH = 0;
		glyphSize       = 0;
		cacheSets       = 0;
		cacheAssoc      = 0;
		return;
	}
	if (aa)
		glyphSize = glyphW * glyphH;
	else
		glyphSize = ((glyphW + 7) >> 3) * glyphH;

	// set up the glyph pixmap cache
	cacheAssoc = splashFontCacheAssoc;
	for (cacheSets = splashFontCacheMaxSets; cacheSets > 1 && glyphSize > splashFontCacheSize / (cacheSets * cacheAssoc); cacheSets >>= 1)
		;
	cache     = (uint8_t*)gmallocn(cacheSets * cacheAssoc, glyphSize);
	cacheTags = (SplashFontCacheTag*)gmallocn(cacheSets * cacheAssoc, sizeof(SplashFontCacheTag));
	for (int i = 0; i < cacheSets * cacheAssoc; ++i)
		cacheTags[i].mru = i & (cacheAssoc - 1);
}

SplashFont::~SplashFont()
{
	fontFile->decRefCnt();
	if (cache) gfree(cache);
	if (cacheTags) gfree(cacheTags);
}

bool SplashFont::getGlyph(int c, int xFrac, int yFrac, SplashGlyphBitmap* bitmap)
{
	SplashGlyphBitmap bitmap2;
	int               size;
	uint8_t*          p;
	int               i, k;

	// no fractional coordinates for large glyphs or non-anti-aliased
	// glyphs
	if (!aa || glyphH > 50)
		xFrac = yFrac = 0;

	// check the cache
	if (cache)
	{
		i = (c & (cacheSets - 1)) * cacheAssoc;
		for (int j = 0; j < cacheAssoc; ++j)
		{
			if ((cacheTags[i + j].mru & 0x80000000) && cacheTags[i + j].c == c && (int)cacheTags[i + j].xFrac == xFrac && (int)cacheTags[i + j].yFrac == yFrac)
			{
				bitmap->x = cacheTags[i + j].x;
				bitmap->y = cacheTags[i + j].y;
				bitmap->w = cacheTags[i + j].w;
				bitmap->h = cacheTags[i + j].h;
				for (k = 0; k < cacheAssoc; ++k)
					if (k != j && (cacheTags[i + k].mru & 0x7fffffff) < (cacheTags[i + j].mru & 0x7fffffff))
						++cacheTags[i + k].mru;
				cacheTags[i + j].mru = 0x80000000;
				bitmap->aa           = aa;
				bitmap->data         = cache + (i + j) * glyphSize;
				bitmap->freeData     = false;
				return true;
			}
		}
	}
	else
	{
		i = 0; // make gcc happy
	}

	// generate the glyph bitmap
	if (!makeGlyph(c, xFrac, yFrac, &bitmap2))
		return false;

	// if the glyph doesn't fit in the bounding box, return a temporary
	// uncached bitmap
	if (!cache || bitmap2.w > glyphW || bitmap2.h > glyphH)
	{
		*bitmap = bitmap2;
		return true;
	}

	// insert glyph pixmap in cache
	if (aa)
		size = bitmap2.w * bitmap2.h;
	else
		size = ((bitmap2.w + 7) >> 3) * bitmap2.h;
	p = nullptr; // make gcc happy
	for (int j = 0; j < cacheAssoc; ++j)
	{
		if ((cacheTags[i + j].mru & 0x7fffffff) == cacheAssoc - 1)
		{
			cacheTags[i + j].mru   = 0x80000000;
			cacheTags[i + j].c     = c;
			cacheTags[i + j].xFrac = (short)xFrac;
			cacheTags[i + j].yFrac = (short)yFrac;
			cacheTags[i + j].x     = bitmap2.x;
			cacheTags[i + j].y     = bitmap2.y;
			cacheTags[i + j].w     = bitmap2.w;
			cacheTags[i + j].h     = bitmap2.h;
			p                      = cache + (i + j) * glyphSize;
			memcpy(p, bitmap2.data, size);
		}
		else
		{
			++cacheTags[i + j].mru;
		}
	}
	*bitmap          = bitmap2;
	bitmap->data     = p;
	bitmap->freeData = false;
	if (bitmap2.freeData) gfree(bitmap2.data);
	return true;
}
