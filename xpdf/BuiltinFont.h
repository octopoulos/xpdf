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
	const char*        name;
	const char**       defaultBaseEnc;
	short              missingWidth;
	short              ascent;
	short              descent;
	short              bbox[4];
	BuiltinFontWidths* widths;
};

//------------------------------------------------------------------------

struct BuiltinFontWidth
{
	const char*       name;
	uint16_t          width;
	BuiltinFontWidth* next;
};

class BuiltinFontWidths
{
public:
	BuiltinFontWidths(BuiltinFontWidth* widths, int sizeA);
	~BuiltinFontWidths();
	bool getWidth(const char* name, uint16_t* width);

private:
	int hash(const char* name);

	BuiltinFontWidth** tab;
	int                size;
};
