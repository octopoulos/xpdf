//========================================================================
//
// UnicodeRemapping.h
//
// Sparse remapping of Unicode characters.
//
// Copyright 2018 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "CharTypes.h"

struct UnicodeRemappingString;

//------------------------------------------------------------------------

class UnicodeRemapping
{
public:
	// Create an empty (i.e., identity) remapping.
	UnicodeRemapping();

	~UnicodeRemapping();

	// Add a remapping for <in>.
	void addRemapping(Unicode in, Unicode* out, int len);

	// Add entries from the specified file to this UnicodeRemapping.
	void parseFile(const std::string& fileName);

	// Map <in> to zero or more (up to <size>) output characters in <out>.
	// Returns the number of output characters.
	int map(Unicode in, Unicode* out, int size);

private:
	int findSMap(Unicode u);

	Unicode                 page0[256]; //
	UnicodeRemappingString* sMap;       //
	int                     sMapLen;    //
	int                     sMapSize;   //
};
