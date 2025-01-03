//========================================================================
//
// SplashPath.cc
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include <string.h>
#include "gmem.h"
#include "gmempp.h"
#include "SplashErrorCodes.h"
#include "SplashPath.h"

//------------------------------------------------------------------------
// SplashPath
//------------------------------------------------------------------------

// A path can be in three possible states:
//
// 1. no current point -- zero or more finished subpaths
//    [curSubpath == length]
//
// 2. one point in subpath
//    [curSubpath == length - 1]
//
// 3. open subpath with two or more points
//    [curSubpath < length - 1]

SplashPath::SplashPath()
{
}

SplashPath::SplashPath(SplashPath* path)
{
	length = path->length;
	size   = path->size;
	pts    = (SplashPathPoint*)gmallocn(size, sizeof(SplashPathPoint));
	flags  = (uint8_t*)gmallocn(size, sizeof(uint8_t));
	memcpy(pts, path->pts, length * sizeof(SplashPathPoint));
	memcpy(flags, path->flags, length * sizeof(uint8_t));
	curSubpath = path->curSubpath;
	if (path->hints)
	{
		hintsLength = hintsSize = path->hintsLength;
		hints                   = (SplashPathHint*)gmallocn(hintsSize, sizeof(SplashPathHint));
		memcpy(hints, path->hints, hintsLength * sizeof(SplashPathHint));
	}
	else
	{
		hints       = nullptr;
		hintsLength = hintsSize = 0;
	}
}

SplashPath::~SplashPath()
{
	gfree(pts);
	gfree(flags);
	gfree(hints);
}

// Add space for <nPts> more points.
void SplashPath::grow(int nPts)
{
	if (length + nPts > size)
	{
		if (size == 0) size = 32;
		while (size < length + nPts) size *= 2;
		pts   = (SplashPathPoint*)greallocn(pts, size, sizeof(SplashPathPoint));
		flags = (uint8_t*)greallocn(flags, size, sizeof(uint8_t));
	}
}

void SplashPath::append(SplashPath* path)
{
	curSubpath = length + path->curSubpath;
	grow(path->length);
	for (int i = 0; i < path->length; ++i)
	{
		pts[length]   = path->pts[i];
		flags[length] = path->flags[i];
		++length;
	}
}

SplashError SplashPath::moveTo(SplashCoord x, SplashCoord y)
{
	if (onePointSubpath()) return splashErrBogusPath;
	grow(1);
	pts[length].x = x;
	pts[length].y = y;
	flags[length] = splashPathFirst | splashPathLast;
	curSubpath    = length++;
	return splashOk;
}

SplashError SplashPath::lineTo(SplashCoord x, SplashCoord y)
{
	if (noCurrentPoint()) return splashErrNoCurPt;
	flags[length - 1] &= (uint8_t)~splashPathLast;
	grow(1);
	pts[length].x = x;
	pts[length].y = y;
	flags[length] = splashPathLast;
	++length;
	return splashOk;
}

SplashError SplashPath::curveTo(SplashCoord x1, SplashCoord y1, SplashCoord x2, SplashCoord y2, SplashCoord x3, SplashCoord y3)
{
	if (noCurrentPoint()) return splashErrNoCurPt;
	flags[length - 1] &= (uint8_t)~splashPathLast;
	grow(3);
	pts[length].x = x1;
	pts[length].y = y1;
	flags[length] = splashPathCurve;
	++length;
	pts[length].x = x2;
	pts[length].y = y2;
	flags[length] = splashPathCurve;
	++length;
	pts[length].x = x3;
	pts[length].y = y3;
	flags[length] = splashPathLast;
	++length;
	return splashOk;
}

SplashError SplashPath::close(bool force)
{
	if (noCurrentPoint()) return splashErrNoCurPt;
	if (force || curSubpath == length - 1 || pts[length - 1].x != pts[curSubpath].x || pts[length - 1].y != pts[curSubpath].y)
		lineTo(pts[curSubpath].x, pts[curSubpath].y);
	flags[curSubpath] |= splashPathClosed;
	flags[length - 1] |= splashPathClosed;
	curSubpath = length;
	return splashOk;
}

void SplashPath::addStrokeAdjustHint(int ctrl0, int ctrl1, int firstPt, int lastPt, bool projectingCap)
{
	if (hintsLength == hintsSize)
	{
		hintsSize = hintsLength ? 2 * hintsLength : 8;
		hints     = (SplashPathHint*)greallocn(hints, hintsSize, sizeof(SplashPathHint));
	}
	hints[hintsLength].ctrl0         = ctrl0;
	hints[hintsLength].ctrl1         = ctrl1;
	hints[hintsLength].firstPt       = firstPt;
	hints[hintsLength].lastPt        = lastPt;
	hints[hintsLength].projectingCap = projectingCap;
	++hintsLength;
}

void SplashPath::offset(SplashCoord dx, SplashCoord dy)
{
	for (int i = 0; i < length; ++i)
	{
		pts[i].x += dx;
		pts[i].y += dy;
	}
}

bool SplashPath::getCurPt(SplashCoord* x, SplashCoord* y)
{
	if (noCurrentPoint()) return false;
	*x = pts[length - 1].x;
	*y = pts[length - 1].y;
	return true;
}

bool SplashPath::containsZeroLengthSubpaths()
{
	bool zeroLength = true; // make gcc happy
	for (int i = 0; i < length; ++i)
	{
		if (flags[i] & splashPathFirst)
		{
			zeroLength = true;
		}
		else
		{
			if (pts[i].x != pts[i - 1].x || pts[i].y != pts[i - 1].y)
				zeroLength = false;
			if (flags[i] & splashPathLast)
			{
				if (zeroLength) return true;
			}
		}
	}
	return false;
}
