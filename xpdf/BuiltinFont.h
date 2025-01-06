//========================================================================
//
// BuiltinFont.h
//
// Copyright 2001-2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>

struct BuiltinFontWidth
{
	std::string name  = ""; //
	int         width = 0;  //
};

struct BuiltinFont
{
	std::string  name           = ""; //
	VEC_STR      defaultBaseEnc = {}; //
	short        missingWidth   = 0;  //
	short        ascent         = 0;  //
	short        descent        = 0;  //
	short        bbox[4]        = {}; //
	UMAP_STR_INT widths         = {}; //
};
