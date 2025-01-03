//========================================================================
//
// ShadingImage.h
//
// Convert shading patterns to bitmaps.
//
// Copyright 2020 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "SplashTypes.h"

class GfxState;
class GfxShading;
class SplashBitmap;
class Splash;

class ShadingImage
{
public:
	// Generate a [mode] bitmap for [shading], using the clipping and
	// CTM info from [state].  Returns the bitmap and sets [xOut],[yOut]
	// to the upper-left corner of the region in device space.  Returns
	// nullptr if the clip region is empty.
	static SplashBitmap* generateBitmap(GfxState* state, GfxShading* shading, SplashColorMode mode, bool reverseVideo, Splash* parentSplash, SplashBitmap* parentBitmap, int* xOut, int* yOut);

private:
	static SplashBitmap* generateFunctionBitmap(GfxState* state, GfxFunctionShading* shading, SplashColorMode mode, bool reverseVideo, Splash* parentSplash, SplashBitmap* parentBitmap, int* xOut, int* yOut);
	static SplashBitmap* generateAxialBitmap(GfxState* state, GfxAxialShading* shading, SplashColorMode mode, bool reverseVideo, Splash* parentSplash, SplashBitmap* parentBitmap, int* xOut, int* yOut);
	static SplashBitmap* generateRadialBitmap(GfxState* state, GfxRadialShading* shading, SplashColorMode mode, bool reverseVideo, Splash* parentSplash, SplashBitmap* parentBitmap, int* xOut, int* yOut);
	static SplashBitmap* generateGouraudTriangleBitmap(
	    GfxState*                  state,
	    GfxGouraudTriangleShading* shading,
	    SplashColorMode            mode,
	    bool                       reverseVideo,
	    Splash*                    parentSplash,
	    SplashBitmap*              parentBitmap,
	    int* xOut, int* yOut);
	static void          gouraudFillTriangle(GfxState* state, SplashBitmap* bitmap, SplashColorMode mode, bool reverseVideo, int xMin, int yMin, int xMax, int yMax, double x0, double y0, double* color0, double x1, double y1, double* color1, double x2, double y2, double* color2, GfxGouraudTriangleShading* shading);
	static SplashBitmap* generatePatchMeshBitmap(GfxState* state, GfxPatchMeshShading* shading, SplashColorMode mode, bool reverseVideo, Splash* parentSplash, SplashBitmap* parentBitmap, int* xOut, int* yOut);
	static void          fillPatch(GfxState* state, Splash* splash, SplashColorMode mode, bool reverseVideo, int xMin, int yMin, GfxPatch* patch, GfxPatchMeshShading* shading, int depth);
	static void          computeShadingColor(GfxState* state, SplashColorMode mode, bool reverseVideo, GfxColor* color, SplashColorPtr sColor);
	static void          transformBBox(GfxState* state, double uxMin, double uyMin, double uxMax, double uyMax, double* dxMin, double* dyMin, double* dxMax, double* dyMax);
};
