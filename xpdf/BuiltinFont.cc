//========================================================================
//
// BuiltinFont.cc
//
// Copyright 2001-2003 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include <stdlib.h>
#include <string.h>
#include "gmem.h"
#include "gmempp.h"
#include "FontEncodingTables.h"
#include "BuiltinFont.h"

//------------------------------------------------------------------------

BuiltinFontWidths::BuiltinFontWidths(BuiltinFontWidth* widths, int sizeA)
{
	size = sizeA;
	tab  = (BuiltinFontWidth**)gmallocn(size, sizeof(BuiltinFontWidth*));
	for (int i = 0; i < size; ++i)
		tab[i] = nullptr;
	for (int i = 0; i < sizeA; ++i)
	{
		const int h    = hash(widths[i].name);
		widths[i].next = tab[h];
		tab[h]         = &widths[i];
	}
}

BuiltinFontWidths::~BuiltinFontWidths()
{
	gfree(tab);
}

bool BuiltinFontWidths::getWidth(const char* name, uint16_t* width)
{
	BuiltinFontWidth* p;

	const int h = hash(name);
	for (p = tab[h]; p; p = p->next)
	{
		if (!strcmp(p->name, name))
		{
			*width = p->width;
			return true;
		}
	}
	*width = 0;
	return false;
}

int BuiltinFontWidths::hash(const char* name)
{
	const char* p;
	uint32_t    h = 0;
	for (p = name; *p; ++p)
		h = 17 * h + (int)(*p & 0xff);
	return (int)(h % size);
}
