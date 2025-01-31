//========================================================================
//
// SplashPattern.cc
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include "gmempp.h"
#include "SplashMath.h"
#include "SplashScreen.h"
#include "SplashPattern.h"

//------------------------------------------------------------------------
// SplashPattern
//------------------------------------------------------------------------

SplashPattern::SplashPattern()
{
}

SplashPattern::~SplashPattern()
{
}

//------------------------------------------------------------------------
// SplashSolidColor
//------------------------------------------------------------------------

SplashSolidColor::SplashSolidColor(SplashColorPtr colorA)
{
	splashColorCopy(color, colorA);
}

SplashSolidColor::~SplashSolidColor()
{
}

void SplashSolidColor::getColor(int x, int y, SplashColorPtr c)
{
	splashColorCopy(c, color);
}
