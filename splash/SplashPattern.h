//========================================================================
//
// SplashPattern.h
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "SplashTypes.h"

class SplashScreen;

//------------------------------------------------------------------------
// SplashPattern
//------------------------------------------------------------------------

class SplashPattern
{
public:
	SplashPattern();

	virtual SplashPattern* copy() = 0;

	virtual ~SplashPattern();

	// Return the color value for a specific pixel.
	virtual void getColor(int x, int y, SplashColorPtr c) = 0;

	// Returns true if this pattern object will return the same color
	// value for all pixels.
	virtual bool isStatic() = 0;

private:
};

//------------------------------------------------------------------------
// SplashSolidColor
//------------------------------------------------------------------------

class SplashSolidColor : public SplashPattern
{
public:
	SplashSolidColor(SplashColorPtr colorA);

	virtual SplashPattern* copy() { return new SplashSolidColor(color); }

	virtual ~SplashSolidColor();

	virtual void getColor(int x, int y, SplashColorPtr c);

	virtual bool isStatic() { return true; }

private:
	SplashColor color = {}; //
};
