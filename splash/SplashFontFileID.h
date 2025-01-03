//========================================================================
//
// SplashFontFileID.h
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>

//------------------------------------------------------------------------
// SplashFontFileID
//------------------------------------------------------------------------

class SplashFontFileID
{
public:
	SplashFontFileID();
	virtual ~SplashFontFileID();
	virtual bool matches(SplashFontFileID* id) = 0;
};
