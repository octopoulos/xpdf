//========================================================================
//
// FoFiType1.h
//
// Copyright 1999-2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "FoFiBase.h"

//------------------------------------------------------------------------
// FoFiType1
//------------------------------------------------------------------------

class FoFiType1 : public FoFiBase
{
public:
	// Create a FoFiType1 object from a memory buffer.
	static FoFiType1* make(char* fileA, int lenA);

	// Create a FoFiType1 object from a file on disk.
	static FoFiType1* load(char* fileName);

	virtual ~FoFiType1();

	// Return the font name.
	std::string_view getName();

	// Return the encoding, as an array of 256 names (any of which may be nullptr).
	char** getEncoding();

	// Return the font matrix as an array of six numbers.
	void getFontMatrix(double* mat);

	// Write a version of the Type 1 font file with a new encoding.
	void writeEncoded(const char** newEncoding, FoFiOutputFunc outputFunc, void* outputStream);

private:
	FoFiType1(char* fileA, int lenA, bool freeFileDataA);

	char* getNextLine(char* line);
	void  parse();
	void  undoPFB();

	std::string name          = "";      //
	char**      encoding      = nullptr; //
	double      fontMatrix[6] = {};      //
	bool        parsed        = false;   //
};
