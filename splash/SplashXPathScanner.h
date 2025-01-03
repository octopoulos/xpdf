//========================================================================
//
// SplashXPathScanner.h
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "SplashTypes.h"
#include "SplashXPath.h"

class GList;

//------------------------------------------------------------------------

// Set this to 0 for antialiasing with 16 levels of gray.
// Set it to 1 for (much slower) antialiasing with 256 levels of gray.
#define ANTIALIAS_256 0

//------------------------------------------------------------------------
// SplashXPathScanner
//------------------------------------------------------------------------

class SplashXPathScanner
{
public:
	// Create a new SplashXPathScanner object.  <xPathA> must be sorted.
	SplashXPathScanner(SplashXPath* xPathA, bool eo, int yMinA, int yMaxA);

	~SplashXPathScanner();

	// Compute shape values for a scan line.  Fills in line[] with shape
	// values for one scan line: ([x0, x1], y).  The values are in [0,
	// 255].  Also returns the min/max x positions with non-zero shape
	// values.
	void getSpan(uint8_t* line, int y, int x0, int x1, int* xMin, int* xMax);

	// Like getSpan(), but uses the values 0 and 255 only.  Writes 255
	// for all pixels which include non-zero area inside the path.
	void getSpanBinary(uint8_t* line, int y, int x0, int x1, int* xMin, int* xMax);

private:
	void insertSegmentBefore(SplashXPathSeg* s, SplashXPathSeg* sNext);
	void removeSegment(SplashXPathSeg* s);
	void moveSegmentAfter(SplashXPathSeg* s, SplashXPathSeg* sPrev);
	void reset(bool aa, bool aaChanged);
	void skip(int newYBottomI, bool aa);
	void advance(bool aa);
	void generatePixels(int x0, int x1, uint8_t* line, int* xMin, int* xMax);
	void generatePixelsBinary(int x0, int x1, uint8_t* line, int* xMin, int* xMax);
	void drawRectangleSpan(uint8_t* line, int y, int x0, int x1, int* xMin, int* xMax);
	void drawRectangleSpanBinary(uint8_t* line, int y, int x0, int x1, int* xMin, int* xMax);

	SplashXPath*    xPath     = nullptr; //
	int             eoMask    = 0;       //
	int             yMin      = 0;       //
	int             yMax      = 0;       //
	int             rectX0I   = 0;       //
	int             rectY0I   = 0;       //
	int             rectX1I   = 0;       //
	int             rectY1I   = 0;       //
	SplashXPathSeg  preSeg    = {};      //
	SplashXPathSeg  postSeg   = {};      //
	SplashXPathSeg* pre       = nullptr; //
	SplashXPathSeg* post      = nullptr; //
	bool            resetDone = false;   //
	bool            resetAA   = false;   //
	int             nextSeg   = 0;       //
	int             yTopI     = 0;       //
	int             yBottomI  = 0;       //
	SplashCoord     yTop      = 0;       //
	SplashCoord     yBottom   = 0;       //
};
