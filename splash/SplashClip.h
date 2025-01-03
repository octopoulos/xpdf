//========================================================================
//
// SplashClip.h
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "SplashTypes.h"
#include "SplashMath.h"

class SplashPath;
class SplashXPath;
class SplashXPathScanner;
class SplashBitmap;

//------------------------------------------------------------------------

enum SplashClipResult
{
	splashClipAllInside,
	splashClipAllOutside,
	splashClipPartial
};

//------------------------------------------------------------------------
// SplashClip
//------------------------------------------------------------------------

class SplashClip
{
public:
	// Create a clip, for the given rectangle.
	SplashClip(int hardXMinA, int hardYMinA, int hardXMaxA, int hardYMaxA);

	// Copy a clip.
	SplashClip* copy() { return new SplashClip(this); }

	~SplashClip();

	// Reset the clip to a rectangle.
	void resetToRect(SplashCoord x0, SplashCoord y0, SplashCoord x1, SplashCoord y1);

	// Intersect the clip with a rectangle.
	SplashError clipToRect(SplashCoord x0, SplashCoord y0, SplashCoord x1, SplashCoord y1);

	// Interesect the clip with <path>.
	SplashError clipToPath(SplashPath* path, SplashCoord* matrix, SplashCoord flatness, bool eoA, bool enablePathSimplification, SplashStrokeAdjustMode strokeAdjust);

	// Tests a rectangle against the clipping region.  Returns one of:
	//   - splashClipAllInside if the entire rectangle is inside the clipping region, i.e., all pixels in the rectangle are visible
	//   - splashClipAllOutside if the entire rectangle is outside the clipping region, i.e., all the pixels in the rectangle are clipped
	//   - splashClipPartial if the rectangle is part inside and part outside the clipping region
	SplashClipResult testRect(int rectXMin, int rectYMin, int rectXMax, int rectYMax, SplashStrokeAdjustMode strokeAdjust);

	// Clip a scan line.  Modifies line[] by multiplying with clipping shape values for one scan line: ([x0, x1], y).
	void clipSpan(uint8_t* line, int y, int x0, int x1, SplashStrokeAdjustMode strokeAdjust);

	// Like clipSpan(), but uses the values 0 and 255 only.
	// Returns true if there are any non-zero values in the result (i.e., returns false if the entire line is clipped out).
	bool clipSpanBinary(uint8_t* line, int y, int x0, int x1, SplashStrokeAdjustMode strokeAdjust);

	// Get the rectangle part of the clip region.
	SplashCoord getXMin() { return xMin; }

	SplashCoord getXMax() { return xMax; }

	SplashCoord getYMin() { return yMin; }

	SplashCoord getYMax() { return yMax; }

	// Get the rectangle part of the clip region, in integer coordinates.
	int getXMinI(SplashStrokeAdjustMode strokeAdjust);
	int getXMaxI(SplashStrokeAdjustMode strokeAdjust);
	int getYMinI(SplashStrokeAdjustMode strokeAdjust);
	int getYMaxI(SplashStrokeAdjustMode strokeAdjust);

	// Get the number of arbitrary paths used by the clip region.
	int getNumPaths();

	// Return true if the clip path is a simple rectangle.
	bool getIsSimple() { return isSimple; }

private:
	SplashClip(SplashClip* clip);
	void grow(int nPaths);
	void updateIntBounds(SplashStrokeAdjustMode strokeAdjust);

	int                    hardXMin;              //
	int                    hardYMin;              // coordinates cannot fall outside of
	int                    hardXMax;              //
	int                    hardYMax;              // [hardXMin, hardXMax), [hardYMin, hardYMax)
	SplashCoord            xMin;                  //
	SplashCoord            yMin;                  // current clip bounding rectangle
	SplashCoord            xMax;                  //
	SplashCoord            yMax;                  //
	int                    xMinI;                 //
	int                    yMinI;                 // integer clip bounding rectangle (these coordinates are adjusted if stroke adjustment is enabled)
	int                    xMaxI;                 //
	int                    yMaxI;                 //
	bool                   intBoundsValid;        // true if xMinI, etc. are valid
	SplashStrokeAdjustMode intBoundsStrokeAdjust; // value of strokeAdjust used to compute xMinI, etc.
	SplashXPath**          paths;                 //
	uint8_t*               eo;                    //
	SplashXPathScanner**   scanners;              //
	int                    length;                //
	int                    size;                  //
	bool                   isSimple;              //
	SplashClip*            prev;                  //
	uint8_t*               buf;                   //
};
