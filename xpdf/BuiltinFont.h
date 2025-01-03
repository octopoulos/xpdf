//========================================================================
//
// BuiltinFont.h
//
// Copyright 2001-2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>

struct BuiltinFont;
class BuiltinFontWidths;

//------------------------------------------------------------------------

struct BuiltinFont
{
	const char*        name           = nullptr; //
	const char**       defaultBaseEnc = nullptr; //
	short              missingWidth   = 0;       //
	short              ascent         = 0;       //
	short              descent        = 0;       //
	short              bbox[4]        = {};      //
	BuiltinFontWidths* widths         = nullptr; //
};

//------------------------------------------------------------------------

struct BuiltinFontWidth
{
	const char*       name  = nullptr; //
	uint16_t          width = 0;       //
	BuiltinFontWidth* next  = nullptr; //
};

class BuiltinFontWidths
{
public:
	BuiltinFontWidths(BuiltinFontWidth* widths, int sizeA);
	~BuiltinFontWidths();
	bool getWidth(const char* name, uint16_t* width);

private:
	int hash(const char* name);

	BuiltinFontWidth** tab  = nullptr; //
	int                size = 0;       //
};
