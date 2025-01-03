//========================================================================
//
// SplashScreen.cc
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_STD_SORT
#	include <algorithm>
#endif
#include "gmem.h"
#include "gmempp.h"
#include "SplashMath.h"
#include "SplashScreen.h"

//------------------------------------------------------------------------

static SplashScreenParams defaultParams = {
	splashScreenDispersed, // type
	2,                     // size
	2,                     // dotRadius
	1.0,                   // gamma
	0.0,                   // blackThreshold
	1.0                    // whiteThreshold
};

//------------------------------------------------------------------------

struct SplashScreenPoint
{
	int x    = 0;
	int y    = 0;
	int dist = 0;
};

#if HAVE_STD_SORT

struct cmpDistancesFunctor
{
	bool operator()(const SplashScreenPoint& p0, const SplashScreenPoint& p1)
	{
		return p0.dist < p1.dist;
	}
};

#else // HAVE_STD_SORT

static int cmpDistances(const void* p0, const void* p1)
{
	return ((SplashScreenPoint*)p0)->dist - ((SplashScreenPoint*)p1)->dist;
}

#endif

//------------------------------------------------------------------------
// SplashScreen
//------------------------------------------------------------------------

// If <clustered> is true, this generates a 45 degree screen using a
// circular dot spot function.  DPI = resolution / ((size / 2) *
// sqrt(2)).  If <clustered> is false, this generates an optimal
// threshold matrix using recursive tesselation.  Gamma correction
// (gamma = 1 / 1.33) is also computed here.
SplashScreen::SplashScreen(SplashScreenParams* params)
{
	uint8_t u;
	int     black, white;

	if (!params)
		params = &defaultParams;

	// size must be a power of 2, and at least 2
	for (size = 2, log2Size = 1; size < params->size; size <<= 1, ++log2Size)
		;

	switch (params->type)
	{
	case splashScreenDispersed:
		mat = (uint8_t*)gmallocn(size * size, sizeof(uint8_t));
		buildDispersedMatrix(size / 2, size / 2, 1, size / 2, 1);
		break;

	case splashScreenClustered:
		mat = (uint8_t*)gmallocn(size * size, sizeof(uint8_t));
		buildClusteredMatrix();
		break;

	case splashScreenStochasticClustered:
		// size must be at least 2*r
		while (size < (params->dotRadius << 1))
		{
			size <<= 1;
			++log2Size;
		}
		mat = (uint8_t*)gmallocn(size * size, sizeof(uint8_t));
		buildSCDMatrix(params->dotRadius);
		break;
	}

	sizeM1 = size - 1;

	// do gamma correction and compute minVal/maxVal
	minVal = 255;
	maxVal = 0;
	black  = splashRound((SplashCoord)255.0 * params->blackThreshold);
	if (black < 1)
		black = 1;
	white = splashRound((SplashCoord)255.0 * params->whiteThreshold);
	if (white > 255)
		white = 255;
	for (int i = 0; i < size * size; ++i)
	{
		u = (uint8_t)splashRound((SplashCoord)255.0 * splashPow((SplashCoord)mat[i] / 255.0, params->gamma));
		if (u < black)
			u = (uint8_t)black;
		else if (u >= white)
			u = (uint8_t)white;
		mat[i] = u;
		if (u < minVal)
			minVal = u;
		else if (u > maxVal)
			maxVal = u;
	}
}

void SplashScreen::buildDispersedMatrix(int i, int j, int val, int delta, int offset)
{
	if (delta == 0)
	{
		// map values in [1, size^2] --> [1, 255]
		mat[(i << log2Size) + j] = (uint8_t)(1 + (254 * (val - 1)) / (size * size - 1));
	}
	else
	{
		buildDispersedMatrix(i, j, val, delta / 2, 4 * offset);
		buildDispersedMatrix((i + delta) % size, (j + delta) % size, val + offset, delta / 2, 4 * offset);
		buildDispersedMatrix((i + delta) % size, j, val + 2 * offset, delta / 2, 4 * offset);
		buildDispersedMatrix((i + 2 * delta) % size, (j + delta) % size, val + 3 * offset, delta / 2, 4 * offset);
	}
}

void SplashScreen::buildClusteredMatrix()
{
	SplashCoord  u, v;

	const int size2 = size >> 1;

	// initialize the threshold matrix
	for (int y = 0; y < size; ++y)
		for (int x = 0; x < size; ++x)
			mat[(y << log2Size) + x] = 0;

	// build the distance matrix
	SplashCoord* dist = (SplashCoord*)gmallocn(size * size2, sizeof(SplashCoord));
	for (int y = 0; y < size2; ++y)
	{
		for (int x = 0; x < size2; ++x)
		{
			if (x + y < size2 - 1)
			{
				u = (SplashCoord)x + 0.5 - 0;
				v = (SplashCoord)y + 0.5 - 0;
			}
			else
			{
				u = (SplashCoord)x + 0.5 - (SplashCoord)size2;
				v = (SplashCoord)y + 0.5 - (SplashCoord)size2;
			}
			dist[y * size2 + x] = u * u + v * v;
		}
	}
	for (int y = 0; y < size2; ++y)
	{
		for (int x = 0; x < size2; ++x)
		{
			if (x < y)
			{
				u = (SplashCoord)x + 0.5 - 0;
				v = (SplashCoord)y + 0.5 - (SplashCoord)size2;
			}
			else
			{
				u = (SplashCoord)x + 0.5 - (SplashCoord)size2;
				v = (SplashCoord)y + 0.5 - 0;
			}
			dist[(size2 + y) * size2 + x] = u * u + v * v;
		}
	}

	// build the threshold matrix
	int x1 = 0;
	int y1 = 0; // make gcc happy
	for (int i = 0; i < size * size2; ++i)
	{
		SplashCoord d = -1;
		for (int y = 0; y < size; ++y)
		{
			for (int x = 0; x < size2; ++x)
			{
				if (mat[(y << log2Size) + x] == 0 && dist[y * size2 + x] > d)
				{
					x1 = x;
					y1 = y;
					d  = dist[y1 * size2 + x1];
				}
			}
		}
		// map values in [0, 2*size*size2-1] --> [1, 255]
		uint8_t val                = (uint8_t)(1 + (254 * (2 * i)) / (2 * size * size2 - 1));
		mat[(y1 << log2Size) + x1] = val;
		val                        = (uint8_t)(1 + (254 * (2 * i + 1)) / (2 * size * size2 - 1));
		if (y1 < size2)
			mat[((y1 + size2) << log2Size) + x1 + size2] = val;
		else
			mat[((y1 - size2) << log2Size) + x1 + size2] = val;
	}

	gfree(dist);
}

// Compute the distance between two points on a toroid.
int SplashScreen::distance(int x0, int y0, int x1, int y1)
{
	const int dx0 = abs(x0 - x1);
	const int dx1 = size - dx0;
	const int dx  = dx0 < dx1 ? dx0 : dx1;
	const int dy0 = abs(y0 - y1);
	const int dy1 = size - dy0;
	const int dy  = dy0 < dy1 ? dy0 : dy1;
	return dx * dx + dy * dy;
}

// Algorithm taken from:
// Victor Ostromoukhov and Roger D. Hersch, "Stochastic Clustered-Dot
// Dithering" in Color Imaging: Device-Independent Color, Color
// Hardcopy, and Graphic Arts IV, SPIE Vol. 3648, pp. 496-505, 1999.
void SplashScreen::buildSCDMatrix(int r)
{
	//~ this should probably happen somewhere else
	srand(123);

	// generate the random space-filling curve
	SplashScreenPoint* pts = (SplashScreenPoint*)gmallocn(size * size, sizeof(SplashScreenPoint));
	{
		int i = 0;
		for (int y = 0; y < size; ++y)
		{
			for (int x = 0; x < size; ++x)
			{
				pts[i].x = x;
				pts[i].y = y;
				++i;
			}
		}
	}
	for (int i = 0; i < size * size; ++i)
	{
		const int j = i + (int)((double)(size * size - i) * (double)rand() / ((double)RAND_MAX + 1.0));
		const int x = pts[i].x;
		const int y = pts[i].y;
		pts[i].x    = pts[j].x;
		pts[i].y    = pts[j].y;
		pts[j].x    = x;
		pts[j].y    = y;
	}

	// construct the circle template
	char* tmpl = (char*)gmallocn((r + 1) * (r + 1), sizeof(char));
	for (int y = 0; y <= r; ++y)
		for (int x = 0; x <= r; ++x)
			tmpl[y * (r + 1) + x] = (x * y <= r * r) ? 1 : 0;

	// mark all grid cells as free
	char* grid = (char*)gmallocn(size * size, sizeof(char));
	for (int y = 0; y < size; ++y)
		for (int x = 0; x < size; ++x)
			grid[(y << log2Size) + x] = 0;

	// walk the space-filling curve, adding dots
	int                dotsLen  = 0;
	int                dotsSize = 32;
	SplashScreenPoint* dots     = (SplashScreenPoint*)gmallocn(dotsSize, sizeof(SplashScreenPoint));
	for (int i = 0; i < size * size; ++i)
	{
		const int x = pts[i].x;
		const int y = pts[i].y;
		if (!grid[(y << log2Size) + x])
		{
			if (dotsLen == dotsSize)
			{
				dotsSize *= 2;
				dots = (SplashScreenPoint*)greallocn(dots, dotsSize, sizeof(SplashScreenPoint));
			}
			dots[dotsLen++] = pts[i];
			for (int yy = 0; yy <= r; ++yy)
			{
				const int y0 = (y + yy) % size;
				const int y1 = (y - yy + size) % size;
				for (int xx = 0; xx <= r; ++xx)
				{
					if (tmpl[yy * (r + 1) + xx])
					{
						const int x0                = (x + xx) % size;
						const int x1                = (x - xx + size) % size;
						grid[(y0 << log2Size) + x0] = 1;
						grid[(y0 << log2Size) + x1] = 1;
						grid[(y1 << log2Size) + x0] = 1;
						grid[(y1 << log2Size) + x1] = 1;
					}
				}
			}
		}
	}

	gfree(tmpl);
	gfree(grid);

	// assign each cell to a dot, compute distance to center of dot
	int* region = (int*)gmallocn(size * size, sizeof(int));
	int* dist   = (int*)gmallocn(size * size, sizeof(int));
	for (int y = 0; y < size; ++y)
	{
		for (int x = 0; x < size; ++x)
		{
			int iMin = 0;
			int dMin = distance(dots[0].x, dots[0].y, x, y);
			for (int i = 1; i < dotsLen; ++i)
			{
				const int d = distance(dots[i].x, dots[i].y, x, y);
				if (d < dMin)
				{
					iMin = i;
					dMin = d;
				}
			}
			region[(y << log2Size) + x] = iMin;
			dist[(y << log2Size) + x]   = dMin;
		}
	}

	// compute threshold values
	for (int i = 0; i < dotsLen; ++i)
	{
		int n = 0;
		for (int y = 0; y < size; ++y)
		{
			for (int x = 0; x < size; ++x)
			{
				if (region[(y << log2Size) + x] == i)
				{
					pts[n].x    = x;
					pts[n].y    = y;
					pts[n].dist = distance(dots[i].x, dots[i].y, x, y);
					++n;
				}
			}
		}
#if HAVE_STD_SORT
		std::sort(pts, pts + n, cmpDistancesFunctor());
#else
		qsort(pts, n, sizeof(SplashScreenPoint), &cmpDistances);
#endif
		for (int j = 0; j < n; ++j)
		{
			// map values in [0 .. n-1] --> [255 .. 1]
			mat[(pts[j].y << log2Size) + pts[j].x] = (uint8_t)(255 - (254 * j) / (n - 1));
		}
	}

	gfree(pts);
	gfree(region);
	gfree(dist);
	gfree(dots);
}

SplashScreen::SplashScreen(SplashScreen* screen)
{
	size     = screen->size;
	sizeM1   = screen->sizeM1;
	log2Size = screen->log2Size;
	mat      = (uint8_t*)gmallocn(size * size, sizeof(uint8_t));
	memcpy(mat, screen->mat, size * size * sizeof(uint8_t));
	minVal = screen->minVal;
	maxVal = screen->maxVal;
}

SplashScreen::~SplashScreen()
{
	gfree(mat);
}
