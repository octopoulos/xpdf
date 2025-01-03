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
	int      x        = 0;       // offset and size of glyph
	int      y        = 0;       //
	int      w        = 0;       //
	int      h        = 0;       //
	bool     aa       = false;   // anti-aliased: true means 8-bit alpha bitmap; false means 1-bit
	uint8_t* data     = nullptr; // bitmap data
	bool     freeData = false;   // true if data memory should be freed
};
