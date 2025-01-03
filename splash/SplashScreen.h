//========================================================================
//
// SplashScreen.h
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "SplashTypes.h"

//------------------------------------------------------------------------
// SplashScreen
//------------------------------------------------------------------------

typedef uint8_t* SplashScreenCursor;

class SplashScreen
{
public:
	SplashScreen(SplashScreenParams* params);
	SplashScreen(SplashScreen* screen);
	~SplashScreen();

	SplashScreen* copy() { return new SplashScreen(this); }

	// Return the computed pixel value (0=black, 1=white) for the gray
	// level <value> at (<x>, <y>).
	int test(int x, int y, uint8_t value)
	{
		int xx, yy;
		xx = x & sizeM1;
		yy = y & sizeM1;
		return value < mat[(yy << log2Size) + xx] ? 0 : 1;
	}

	// To do a series of tests with the same y value, call
	// getTestCursor(y), and then call testWithCursor(cursor, x, value)
	// for each x.
	SplashScreenCursor getTestCursor(int y)
	{
		int yy;
		yy = y & sizeM1;
		return &mat[yy << log2Size];
	}

	int testWithCursor(SplashScreenCursor cursor, int x, uint8_t value)
	{
		int xx = x & sizeM1;
		return value >= cursor[xx];
	}

	// Returns true if value is above the white threshold or below the
	// black threshold, i.e., if the corresponding halftone will be
	// solid white or black.
	bool isStatic(uint8_t value) { return value < minVal || value >= maxVal; }

private:
	void buildDispersedMatrix(int i, int j, int val, int delta, int offset);
	void buildClusteredMatrix();
	int  distance(int x0, int y0, int x1, int y1);
	void buildSCDMatrix(int r);

	uint8_t* mat      = nullptr; // threshold matrix
	int      size     = 0;       // size of the threshold matrix
	int      sizeM1   = 0;       // size - 1
	int      log2Size = 0;       // log2(size)
	uint8_t  minVal   = 0;       // any pixel value below minVal generates solid black
	uint8_t  maxVal   = 0;       // any pixel value above maxVal generates solid white
};
