//========================================================================
//
// SplashBitmap.h
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include <stdio.h>
#include <limits.h>
// older compilers won't define SIZE_MAX in stdint.h without this
#ifndef __STDC_LIMIT_MACROS
#	define __STDC_LIMIT_MACROS 1
#endif
#include <stdint.h>
#include "SplashTypes.h"

//------------------------------------------------------------------------

// ssize_t isn't well-defined, so define our own
#if SIZE_MAX == UINT_MAX
typedef int SplashBitmapRowSize;
#	define SplashBitmapRowSizeMax INT_MAX
#else
typedef long long SplashBitmapRowSize;
#	define SplashBitmapRowSizeMax LLONG_MAX
#endif

//------------------------------------------------------------------------
// SplashBitmap
//------------------------------------------------------------------------

class SplashBitmap
{
public:
	// Create a new bitmap.
	// It will have <widthA> x <heightA> pixels in color mode <modeA>.
	// Rows will be padded out to a multiple of <rowPad> bytes.
	// If <topDown> is false, the bitmap will be stored upside-down,
	// i.e., with the last row first in memory.
	SplashBitmap(int widthA, int heightA, int rowPad, SplashColorMode modeA, bool alphaA, bool topDown, SplashBitmap* parentA);

	~SplashBitmap();

	int getWidth() { return width; }

	int getHeight() { return height; }

	SplashBitmapRowSize getRowSize() { return rowSize; }

	size_t getAlphaRowSize() { return alphaRowSize; }

	SplashColorMode getMode() { return mode; }

	SplashColorPtr getDataPtr() { return data; }

	uint8_t* getAlphaPtr() { return alpha; }

	SplashError writePNMFile(const char* fileName);
	SplashError writePNMFile(FILE* f);
	SplashError writeAlphaPGMFile(const char* fileName);

	void    getPixel(int x, int y, SplashColorPtr pixel);
	uint8_t getAlpha(int x, int y);

	// Caller takes ownership of the bitmap data.
	// The SplashBitmap object is no longer valid
	// -- the next call should be to the destructor.
	SplashColorPtr takeAlpha();
	SplashColorPtr takeData();

private:
	int                 width           = 0;              //
	int                 height          = 0;              // size of bitmap
	SplashBitmapRowSize rowSize         = 0;              // size of one row of data, in bytes - negative for bottom-up bitmaps
	size_t              alphaRowSize    = 0;              // size of one row of alpha, in bytes
	SplashColorMode     mode            = splashModeRGB8; // color mode
	SplashColorPtr      data            = nullptr;        // pointer to row zero of the color data
	uint8_t*            alpha           = nullptr;        // pointer to row zero of the alpha data (always top-down)
	SplashBitmap*       parent          = nullptr;        // save the last-allocated (large) bitmap data and reuse if possible
	SplashColorPtr      oldData         = nullptr;        //
	uint8_t*            oldAlpha        = nullptr;        //
	SplashBitmapRowSize oldRowSize      = 0;              //
	size_t              oldAlphaRowSize = 0;              //
	int                 oldHeight       = 0;              //

	friend class Splash;
};
