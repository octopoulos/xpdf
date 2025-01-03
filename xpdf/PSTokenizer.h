//========================================================================
//
// PSTokenizer.h
//
// Copyright 2002-2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>

//------------------------------------------------------------------------

class PSTokenizer
{
public:
	PSTokenizer(int (*getCharFuncA)(void*), void* dataA);
	~PSTokenizer();

	// Get the next PostScript token.  Returns false at end-of-stream.
	bool getToken(char* buf, int size, int* length);

private:
	int lookChar();
	int getChar();

	int (*getCharFunc)(void*); //
	void* data;                //
	int   charBuf;             //
};
