//========================================================================
//
// SplashPath.h
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "SplashTypes.h"

//------------------------------------------------------------------------
// SplashPathPoint
//------------------------------------------------------------------------

struct SplashPathPoint
{
	SplashCoord x, y;
};

//------------------------------------------------------------------------
// SplashPath.flags
//------------------------------------------------------------------------

// first point on each subpath sets this flag
#define splashPathFirst 0x01

// last point on each subpath sets this flag
#define splashPathLast 0x02

// if the subpath is closed, its first and last points must be
// identical, and must set this flag
#define splashPathClosed 0x04

// curve control points set this flag
#define splashPathCurve 0x08

//------------------------------------------------------------------------
// SplashPathHint
//------------------------------------------------------------------------

struct SplashPathHint
{
	int  ctrl0         = 0;     //
	int  ctrl1         = 0;     //
	int  firstPt       = 0;     //
	int  lastPt        = 0;     //
	bool projectingCap = false; //
};

//------------------------------------------------------------------------
// SplashPath
//------------------------------------------------------------------------

class SplashPath
{
public:
	// Create an empty path.
	SplashPath();

	// Copy a path.
	SplashPath* copy() { return new SplashPath(this); }

	~SplashPath();

	// Append <path> to <this>.
	void append(SplashPath* path);

	// Start a new subpath.
	SplashError moveTo(SplashCoord x, SplashCoord y);

	// Add a line segment to the last subpath.
	SplashError lineTo(SplashCoord x, SplashCoord y);

	// Add a third-order (cubic) Bezier curve segment to the last
	// subpath.
	SplashError curveTo(SplashCoord x1, SplashCoord y1, SplashCoord x2, SplashCoord y2, SplashCoord x3, SplashCoord y3);

	// Close the last subpath, adding a line segment if necessary.  If
	// <force> is true, this adds a line segment even if the current
	// point is equal to the first point in the subpath.
	SplashError close(bool force = false);

	// Add a stroke adjustment hint.  The controlling segments are
	// <ctrl0> and <ctrl1> (where segments are identified by their first
	// point), and the points to be adjusted are <firstPt> .. <lastPt>.
	// <projectingCap> is true if the points are part of a projecting
	// line cap.
	void addStrokeAdjustHint(int ctrl0, int ctrl1, int firstPt, int lastPt, bool projectingCap = false);

	// Add (<dx>, <dy>) to every point on this path.
	void offset(SplashCoord dx, SplashCoord dy);

	// Get the points on the path.
	int getLength() { return length; }

	void getPoint(int i, SplashCoord* x, SplashCoord* y, uint8_t* f)
	{
		*x = pts[i].x;
		*y = pts[i].y;
		*f = flags[i];
	}

	// Get the current point.
	bool getCurPt(SplashCoord* x, SplashCoord* y);

	// Returns true if the path contains one or more zero length
	// subpaths.
	bool containsZeroLengthSubpaths();

private:
	SplashPath(SplashPath* path);
	void grow(int nPts);

	bool noCurrentPoint() { return curSubpath == length; }

	bool onePointSubpath() { return curSubpath == length - 1; }

	bool openSubpath() { return curSubpath < length - 1; }

	SplashPathPoint* pts         = nullptr; // array of points
	uint8_t*         flags       = nullptr; // array of flags
	int              length      = 0;       // length/size of the pts and flags arrays
	int              size        = 0;       //
	int              curSubpath  = 0;       // index of first point in last subpath
	SplashPathHint*  hints       = nullptr; // list of hints
	int              hintsLength = 0;       //
	int              hintsSize   = 0;       //

	friend class SplashXPath;
	friend class Splash;
};
