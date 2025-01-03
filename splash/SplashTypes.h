//========================================================================
//
// SplashTypes.h
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>

//------------------------------------------------------------------------
// coordinates
//------------------------------------------------------------------------

#if USE_FIXEDPOINT
#	include "FixedPoint.h"
typedef FixedPoint SplashCoord;
#else
typedef double SplashCoord;
#endif

//------------------------------------------------------------------------
// antialiasing
//------------------------------------------------------------------------

#define splashAASize 4

//------------------------------------------------------------------------
// colors
//------------------------------------------------------------------------

enum SplashColorMode
{
	splashModeMono1, // 1 bit per component, 8 pixels per byte, MSbit is on the left
	splashModeMono8, // 1 byte per component, 1 byte per pixel
	splashModeRGB8,  // 1 byte per component, 3 bytes per pixel: RGBRGB...
	splashModeBGR8   // 1 byte per component, 3 bytes per pixel: BGRBGR...

#if SPLASH_CMYK
	    ,
	splashModeCMYK8 // 1 byte per component, 4 bytes per pixel: CMYKCMYK...
#endif
};

// number of components in each color mode
// (defined in SplashState.cc)
extern int splashColorModeNComps[];

// max number of components in any SplashColor
#define splashMaxColorComps 3
#if SPLASH_CMYK
#	undef splashMaxColorComps
#	define splashMaxColorComps 4
#endif

typedef uint8_t  SplashColor[splashMaxColorComps];
typedef uint8_t* SplashColorPtr;

// RGB8
static inline uint8_t splashRGB8R(SplashColorPtr rgb8) { return rgb8[0]; }

static inline uint8_t splashRGB8G(SplashColorPtr rgb8) { return rgb8[1]; }

static inline uint8_t splashRGB8B(SplashColorPtr rgb8) { return rgb8[2]; }

// BGR8
static inline uint8_t splashBGR8R(SplashColorPtr bgr8) { return bgr8[2]; }

static inline uint8_t splashBGR8G(SplashColorPtr bgr8) { return bgr8[1]; }

static inline uint8_t splashBGR8B(SplashColorPtr bgr8) { return bgr8[0]; }

#if SPLASH_CMYK
// CMYK8
static inline uint8_t splashCMYK8C(SplashColorPtr cmyk8) { return cmyk8[0]; }

static inline uint8_t splashCMYK8M(SplashColorPtr cmyk8) { return cmyk8[1]; }

static inline uint8_t splashCMYK8Y(SplashColorPtr cmyk8) { return cmyk8[2]; }

static inline uint8_t splashCMYK8K(SplashColorPtr cmyk8) { return cmyk8[3]; }
#endif

static inline void splashColorCopy(SplashColorPtr dest, SplashColorPtr src)
{
	dest[0] = src[0];
	dest[1] = src[1];
	dest[2] = src[2];
#if SPLASH_CMYK
	dest[3] = src[3];
#endif
}

static inline void splashColorXor(SplashColorPtr dest, SplashColorPtr src)
{
	dest[0] ^= src[0];
	dest[1] ^= src[1];
	dest[2] ^= src[2];
#if SPLASH_CMYK
	dest[3] ^= src[3];
#endif
}

//------------------------------------------------------------------------
// blend functions
//------------------------------------------------------------------------

typedef void (*SplashBlendFunc)(SplashColorPtr src, SplashColorPtr dest, SplashColorPtr blend, SplashColorMode cm);

//------------------------------------------------------------------------
// stroke adjustment mode
//------------------------------------------------------------------------

enum SplashStrokeAdjustMode
{
	splashStrokeAdjustOff,
	splashStrokeAdjustNormal,
	splashStrokeAdjustCAD
};

//------------------------------------------------------------------------
// screen parameters
//------------------------------------------------------------------------

enum SplashScreenType
{
	splashScreenDispersed,
	splashScreenClustered,
	splashScreenStochasticClustered
};

struct SplashScreenParams
{
	SplashScreenType type           = splashScreenDispersed; //
	int              size           = 0;                     //
	int              dotRadius      = 0;                     //
	SplashCoord      gamma          = 0;                     //
	SplashCoord      blackThreshold = 0;                     //
	SplashCoord      whiteThreshold = 0;                     //
};

//------------------------------------------------------------------------
// error results
//------------------------------------------------------------------------

typedef int SplashError;
