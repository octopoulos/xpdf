//========================================================================
//
// SplashGlyphBitmap.h
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>

//------------------------------------------------------------------------
// SplashGlyphBitmap
//------------------------------------------------------------------------

struct SplashGlyphBitmap
{
	int      x;        // offset and size of glyph
	int      y;        //
	int      w;        //
	int      h;        //
	bool     aa;       // anti-aliased: true means 8-bit alpha bitmap; false means 1-bit
	uint8_t* data;     // bitmap data
	bool     freeData; // true if data memory should be freed
};
