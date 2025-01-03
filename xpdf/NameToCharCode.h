//========================================================================
//
// NameToCharCode.h
//
// Copyright 2001-2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "CharTypes.h"

struct NameToCharCodeEntry;

//------------------------------------------------------------------------

class NameToCharCode
{
public:
	NameToCharCode();
	~NameToCharCode();

	void     add(const char* name, CharCode c);
	CharCode lookup(const char* name);

private:
	int hash(const char* name);

	NameToCharCodeEntry* tab  = nullptr; //
	int                  size = 0;       //
	int                  len  = 0;       //
};
