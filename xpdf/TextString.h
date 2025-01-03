//========================================================================
//
// TextString.h
//
// Copyright 2011-2013 Glyph & Cog, LLC
//
// Represents a PDF "text string", which can either be a UTF-16BE
// string (with a leading byte order marker), or an 8-bit string in
// PDFDocEncoding.
//
//========================================================================

#pragma once

#include <aconf.h>
#include "CharTypes.h"

//------------------------------------------------------------------------

class TextString
{
public:
	// Create an empty TextString.
	TextString();

	// Create a TextString from a PDF text string.
	TextString(const std::string& s);

	// Copy a TextString.
	TextString(TextString* s);

	~TextString();

	// Append a Unicode character or PDF text string to this TextString.
	TextString* append(Unicode c);
	TextString* append(const std::string& s);

	// Insert a Unicode character, sequence of Unicode characters, or
	// PDF text string in this TextString.
	TextString* insert(int idx, Unicode c);
	TextString* insert(int idx, Unicode* u2, int n);
	TextString* insert(int idx, const std::string& s);

	// Get the Unicode characters in the TextString.
	int getLength() { return len; }

	Unicode* getUnicode() { return u; }

	// Create a PDF text string from a TextString.
	std::string toPDFTextString();

	// Convert a TextString to UTF-8.
	std::string toUTF8();

private:
	void expand(int delta);

	Unicode* u    = nullptr; // NB: not null-terminated
	int      len  = 0;       //
	int      size = 0;       //
};
