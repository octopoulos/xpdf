//========================================================================
//
// SplashXPath.h
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "SplashTypes.h"

class SplashPath;
class SplashClip;
struct SplashXPathPoint;
struct SplashPathHint;

//------------------------------------------------------------------------

#define splashMaxCurveSplits (1 << 10)

//------------------------------------------------------------------------
// SplashXPathSeg
//------------------------------------------------------------------------

struct SplashXPathSeg
{
	SplashCoord x0    = 0;
	SplashCoord y0    = 0; // first endpoint (y0 <= y1)
	SplashCoord x1    = 0;
	SplashCoord y1    = 0; // second endpoint
	SplashCoord dxdy  = 0; // slope: delta-x / delta-y
	SplashCoord dydx  = 0; // slope: delta-y / delta-x
	int         count = 0; // EO/NZWN counter increment

	//----- used by SplashXPathScanner
	int             iy   = 0; //
	SplashCoord     sx0  = 0;
	SplashCoord     sx1  = 0;       //
	SplashCoord     mx   = 0;       //
	SplashXPathSeg* prev = nullptr; //
	SplashXPathSeg* next = nullptr; //

#if HAVE_STD_SORT

	static bool cmpMX(const SplashXPathSeg& s0, const SplashXPathSeg& s1)
	{
		if (s0.iy != s1.iy) return s0.iy < s1.iy;
		return s0.mx < s1.mx;
	}

	static bool cmpY(const SplashXPathSeg& seg0, const SplashXPathSeg& seg1)
	{
		return seg0.y0 < seg1.y0;
	}

#else

	static int cmpMX(const void* p0, const void* p1)
	{
		SplashXPathSeg* s0 = (SplashXPathSeg*)p0;
		SplashXPathSeg* s1 = (SplashXPathSeg*)p1;
		SplashCoord     cmp;

		if (s0->iy != s1->iy) return s0->iy - s1->iy;
		cmp = s0->mx - s1->mx;
		return (cmp < 0) ? -1 : ((cmp > 0) ? 1 : 0);
	}

	static int cmpY(const void* seg0, const void* seg1)
	{
		SplashCoord cmp;

		cmp = ((SplashXPathSeg*)seg0)->y0 - ((SplashXPathSeg*)seg1)->y0;
		return (cmp > 0) ? 1 : ((cmp < 0) ? -1 : 0);
	}

#endif
};

//------------------------------------------------------------------------
// SplashXPath
//------------------------------------------------------------------------

class SplashXPath
{
public:
	// Expands (converts to segments) and flattens (converts curves to lines) <path>.
	// Transforms all points from user space to device space, via <matrix>.
	// If <closeSubpaths> is true, closes all open subpaths.
	SplashXPath(SplashPath* path, SplashCoord* matrix, SplashCoord flatness, bool closeSubpaths, bool simplify, SplashStrokeAdjustMode strokeAdjMode, SplashClip* clip);

	// Copy an expanded path.
	SplashXPath* copy() { return new SplashXPath(this); }

	~SplashXPath();

	int getXMin() { return xMin; }

	int getXMax() { return xMax; }

	int getYMin() { return yMin; }

	int getYMax() { return yMax; }

private:
	SplashXPath(SplashXPath* xPath);

	static void clampCoords(SplashCoord* x, SplashCoord* y);
	void        transform(SplashCoord* matrix, SplashCoord xi, SplashCoord yi, SplashCoord* xo, SplashCoord* yo);
	bool        strokeAdjust(SplashXPathPoint* pts, SplashPathHint* hints, int nHints, SplashStrokeAdjustMode strokeAdjMode, SplashClip* clip);
	void        grow(int nSegs);
	void        addCurve(SplashCoord x0, SplashCoord y0, SplashCoord x1, SplashCoord y1, SplashCoord x2, SplashCoord y2, SplashCoord x3, SplashCoord y3, SplashCoord flatness, bool first, bool last, bool end0, bool end1);
	void        mergeSegments(int first);
	void        addSegment(SplashCoord x0, SplashCoord y0, SplashCoord x1, SplashCoord y1);
	void        finishSegments();

	SplashXPathSeg* segs   = nullptr; //
	int             length = 0;       //
	int             size   = 0;       // length and size of segs array
	int             xMin   = 0;       //
	int             xMax   = 0;       //
	int             yMin   = 0;       //
	int             yMax   = 0;       //
	bool            isRect = false;   //
	SplashCoord     rectX0 = 0;       //
	SplashCoord     rectY0 = 0;       //
	SplashCoord     rectX1 = 0;       //
	SplashCoord     rectY1 = 0;       //

	friend class SplashXPathScanner;
	friend class SplashClip;
	friend class Splash;
};
