//========================================================================
//
// Parser.h
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "Lexer.h"

//------------------------------------------------------------------------
// Parser
//------------------------------------------------------------------------

class Parser
{
public:
	// Constructor.
	Parser(XRef* xrefA, Lexer* lexerA, bool allowStreamsA);

	// Destructor.
	~Parser();

	// Get the next object from the input stream.  If <simpleOnly> is
	// true, do not parse compound objects (arrays, dictionaries, or
	// streams).
	Object* getObj(Object* obj, bool simpleOnly = false, uint8_t* fileKey = nullptr, CryptAlgorithm encAlgorithm = cryptRC4, int keyLength = 0, int objNum = 0, int objGen = 0, int recursion = 0);

	// Get stream index (for arrays of streams).
	int getStreamIndex() { return lexer->getStreamIndex(); }

	// Get stream.
	Stream* getStream() { return lexer->getStream(); }

	// Get current position in file.
	GFileOffset getPos() { return lexer->getPos(); }

private:
	XRef*  xref;         // the xref table for this PDF file
	Lexer* lexer;        // input stream
	bool   allowStreams; // parse stream objects?
	Object buf1, buf2;   // next two tokens
	int    inlineImg;    // set when inline image data is encountered

	Stream* makeStream(Object* dict, uint8_t* fileKey, CryptAlgorithm encAlgorithm, int keyLength, int objNum, int objGen, int recursion);
	void    shift();
};
