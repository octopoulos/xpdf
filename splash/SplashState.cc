//========================================================================
//
// SplashState.cc
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include <string.h>
#include "gmem.h"
#include "gmempp.h"
#include "SplashPattern.h"
#include "SplashScreen.h"
#include "SplashClip.h"
#include "SplashBitmap.h"
#include "SplashState.h"

//------------------------------------------------------------------------
// SplashState
//------------------------------------------------------------------------

// number of components in each color mode
int splashColorModeNComps[] = {
	1, 1, 3, 3
#if SPLASH_CMYK
	,
	4
#endif
};

SplashState::SplashState(int width, int height, bool vectorAntialias, SplashScreenParams* screenParams)
{
	SplashColor color;

	matrix[0] = 1;
	matrix[1] = 0;
	matrix[2] = 0;
	matrix[3] = 1;
	matrix[4] = 0;
	matrix[5] = 0;
	memset(&color, 0, sizeof(SplashColor));
	strokePattern      = new SplashSolidColor(color);
	fillPattern        = new SplashSolidColor(color);
	screen             = new SplashScreen(screenParams);
	blendFunc          = nullptr;
	strokeAlpha        = 1;
	fillAlpha          = 1;
	lineWidth          = 1;
	lineCap            = splashLineCapButt;
	lineJoin           = splashLineJoinMiter;
	miterLimit         = 10;
	flatness           = 1;
	lineDash           = nullptr;
	lineDashLength     = 0;
	lineDashPhase      = 0;
	strokeAdjust       = splashStrokeAdjustOff;
	clip               = new SplashClip(0, 0, width, height);
	clipIsShared       = false;
	softMask           = nullptr;
	deleteSoftMask     = false;
	inNonIsolatedGroup = false;
	inKnockoutGroup    = false;
#if SPLASH_CMYK
	rgbTransferR  = (uint8_t*)gmalloc(8 * 256);
	rgbTransferG  = rgbTransferR + 256;
	rgbTransferB  = rgbTransferG + 256;
	grayTransfer  = rgbTransferB + 256;
	cmykTransferC = grayTransfer + 256;
	cmykTransferM = cmykTransferC + 256;
	cmykTransferY = cmykTransferM + 256;
	cmykTransferK = cmykTransferY + 256;
#else
	rgbTransferR = (uint8_t*)gmalloc(4 * 256);
	rgbTransferG = rgbTransferR + 256;
	rgbTransferB = rgbTransferG + 256;
	grayTransfer = rgbTransferB + 256;
#endif
	for (int i = 0; i < 256; ++i)
	{
		rgbTransferR[i] = (uint8_t)i;
		rgbTransferG[i] = (uint8_t)i;
		rgbTransferB[i] = (uint8_t)i;
		grayTransfer[i] = (uint8_t)i;
#if SPLASH_CMYK
		cmykTransferC[i] = (uint8_t)i;
		cmykTransferM[i] = (uint8_t)i;
		cmykTransferY[i] = (uint8_t)i;
		cmykTransferK[i] = (uint8_t)i;
#endif
	}
	transferIsShared         = false;
	overprintMask            = 0xffffffff;
	enablePathSimplification = false;
	next                     = nullptr;
}

SplashState::SplashState(int width, int height, bool vectorAntialias, SplashScreen* screenA)
{
	SplashColor color;

	matrix[0] = 1;
	matrix[1] = 0;
	matrix[2] = 0;
	matrix[3] = 1;
	matrix[4] = 0;
	matrix[5] = 0;
	memset(&color, 0, sizeof(SplashColor));
	strokePattern      = new SplashSolidColor(color);
	fillPattern        = new SplashSolidColor(color);
	screen             = screenA->copy();
	blendFunc          = nullptr;
	strokeAlpha        = 1;
	fillAlpha          = 1;
	lineWidth          = 1;
	lineCap            = splashLineCapButt;
	lineJoin           = splashLineJoinMiter;
	miterLimit         = 10;
	flatness           = 1;
	lineDash           = nullptr;
	lineDashLength     = 0;
	lineDashPhase      = 0;
	strokeAdjust       = splashStrokeAdjustOff;
	clip               = new SplashClip(0, 0, width, height);
	clipIsShared       = false;
	softMask           = nullptr;
	deleteSoftMask     = false;
	inNonIsolatedGroup = false;
	inKnockoutGroup    = false;
#if SPLASH_CMYK
	rgbTransferR  = (uint8_t*)gmalloc(8 * 256);
	rgbTransferG  = rgbTransferR + 256;
	rgbTransferB  = rgbTransferG + 256;
	grayTransfer  = rgbTransferB + 256;
	cmykTransferC = grayTransfer + 256;
	cmykTransferM = cmykTransferC + 256;
	cmykTransferY = cmykTransferM + 256;
	cmykTransferK = cmykTransferY + 256;
#else
	rgbTransferR = (uint8_t*)gmalloc(4 * 256);
	rgbTransferG = rgbTransferR + 256;
	rgbTransferB = rgbTransferG + 256;
	grayTransfer = rgbTransferB + 256;
#endif
	for (int i = 0; i < 256; ++i)
	{
		rgbTransferR[i] = (uint8_t)i;
		rgbTransferG[i] = (uint8_t)i;
		rgbTransferB[i] = (uint8_t)i;
		grayTransfer[i] = (uint8_t)i;
#if SPLASH_CMYK
		cmykTransferC[i] = (uint8_t)i;
		cmykTransferM[i] = (uint8_t)i;
		cmykTransferY[i] = (uint8_t)i;
		cmykTransferK[i] = (uint8_t)i;
#endif
	}
	transferIsShared         = false;
	overprintMask            = 0xffffffff;
	enablePathSimplification = false;
	next                     = nullptr;
}

SplashState::SplashState(SplashState* state)
{
	memcpy(matrix, state->matrix, 6 * sizeof(SplashCoord));
	strokePattern = state->strokePattern->copy();
	fillPattern   = state->fillPattern->copy();
	screen        = state->screen->copy();
	blendFunc     = state->blendFunc;
	strokeAlpha   = state->strokeAlpha;
	fillAlpha     = state->fillAlpha;
	lineWidth     = state->lineWidth;
	lineCap       = state->lineCap;
	lineJoin      = state->lineJoin;
	miterLimit    = state->miterLimit;
	flatness      = state->flatness;
	if (state->lineDash)
	{
		lineDashLength = state->lineDashLength;
		lineDash       = (SplashCoord*)gmallocn(lineDashLength, sizeof(SplashCoord));
		memcpy(lineDash, state->lineDash, lineDashLength * sizeof(SplashCoord));
	}
	else
	{
		lineDash       = nullptr;
		lineDashLength = 0;
	}
	lineDashPhase      = state->lineDashPhase;
	strokeAdjust       = state->strokeAdjust;
	clip               = state->clip;
	clipIsShared       = true;
	softMask           = state->softMask;
	deleteSoftMask     = false;
	inNonIsolatedGroup = state->inNonIsolatedGroup;
	inKnockoutGroup    = state->inKnockoutGroup;
	rgbTransferR       = state->rgbTransferR;
	rgbTransferG       = state->rgbTransferG;
	rgbTransferB       = state->rgbTransferB;
	grayTransfer       = state->grayTransfer;
#if SPLASH_CMYK
	cmykTransferC = state->cmykTransferC;
	cmykTransferM = state->cmykTransferM;
	cmykTransferY = state->cmykTransferY;
	cmykTransferK = state->cmykTransferK;
#endif
	transferIsShared         = true;
	overprintMask            = state->overprintMask;
	enablePathSimplification = state->enablePathSimplification;
	next                     = nullptr;
}

SplashState::~SplashState()
{
	delete strokePattern;
	delete fillPattern;
	delete screen;
	gfree(lineDash);
	if (!clipIsShared)
		delete clip;
	if (!transferIsShared)
		gfree(rgbTransferR);
	if (deleteSoftMask && softMask)
		delete softMask;
}

void SplashState::setStrokePattern(SplashPattern* strokePatternA)
{
	delete strokePattern;
	strokePattern = strokePatternA;
}

void SplashState::setFillPattern(SplashPattern* fillPatternA)
{
	delete fillPattern;
	fillPattern = fillPatternA;
}

void SplashState::setScreen(SplashScreen* screenA)
{
	delete screen;
	screen = screenA;
}

void SplashState::setLineDash(SplashCoord* lineDashA, int lineDashLengthA, SplashCoord lineDashPhaseA)
{
	gfree(lineDash);
	lineDashLength = lineDashLengthA;
	if (lineDashLength > 0)
	{
		lineDash = (SplashCoord*)gmallocn(lineDashLength, sizeof(SplashCoord));
		memcpy(lineDash, lineDashA, lineDashLength * sizeof(SplashCoord));
	}
	else
	{
		lineDash = nullptr;
	}
	lineDashPhase = lineDashPhaseA;
}

bool SplashState::lineDashContainsZeroLengthDashes()
{
	if (lineDashLength == 0)
		return false;

	// if the line dash array has an odd number of elements, we need to
	// check all of the elements; if the length is even, we only need to
	// check even-number elements
	if (lineDashLength & 1)
	{
		for (int i = 0; i < lineDashLength; ++i)
			if (lineDash[i] == 0)
				return true;
	}
	else
	{
		for (int i = 0; i < lineDashLength; i += 2)
			if (lineDash[i] == 0)
				return true;
	}
	return false;
}

void SplashState::clipResetToRect(SplashCoord x0, SplashCoord y0, SplashCoord x1, SplashCoord y1)
{
	if (clipIsShared)
	{
		clip         = clip->copy();
		clipIsShared = false;
	}
	clip->resetToRect(x0, y0, x1, y1);
}

SplashError SplashState::clipToRect(SplashCoord x0, SplashCoord y0, SplashCoord x1, SplashCoord y1)
{
	if (clipIsShared)
	{
		clip         = clip->copy();
		clipIsShared = false;
	}
	return clip->clipToRect(x0, y0, x1, y1);
}

SplashError SplashState::clipToPath(SplashPath* path, bool eo)
{
	if (clipIsShared)
	{
		clip         = clip->copy();
		clipIsShared = false;
	}
	return clip->clipToPath(path, matrix, flatness, eo, enablePathSimplification, strokeAdjust);
}

void SplashState::setSoftMask(SplashBitmap* softMaskA, bool deleteBitmap)
{
	if (deleteSoftMask)
		delete softMask;
	softMask       = softMaskA;
	deleteSoftMask = deleteBitmap;
}

void SplashState::setTransfer(uint8_t* red, uint8_t* green, uint8_t* blue, uint8_t* gray)
{
	if (transferIsShared)
	{
#if SPLASH_CMYK
		rgbTransferR  = (uint8_t*)gmalloc(8 * 256);
		rgbTransferG  = rgbTransferR + 256;
		rgbTransferB  = rgbTransferG + 256;
		grayTransfer  = rgbTransferB + 256;
		cmykTransferC = grayTransfer + 256;
		cmykTransferM = cmykTransferC + 256;
		cmykTransferY = cmykTransferM + 256;
		cmykTransferK = cmykTransferY + 256;
#else
		rgbTransferR = (uint8_t*)gmalloc(4 * 256);
		rgbTransferG = rgbTransferR + 256;
		rgbTransferB = rgbTransferG + 256;
		grayTransfer = rgbTransferB + 256;
#endif
		transferIsShared = false;
	}
	memcpy(rgbTransferR, red, 256);
	memcpy(rgbTransferG, green, 256);
	memcpy(rgbTransferB, blue, 256);
	memcpy(grayTransfer, gray, 256);
#if SPLASH_CMYK
	for (int i = 0; i < 256; ++i)
	{
		cmykTransferC[i] = (uint8_t)(255 - rgbTransferR[255 - i]);
		cmykTransferM[i] = (uint8_t)(255 - rgbTransferG[255 - i]);
		cmykTransferY[i] = (uint8_t)(255 - rgbTransferB[255 - i]);
		cmykTransferK[i] = (uint8_t)(255 - grayTransfer[255 - i]);
	}
#endif
}
