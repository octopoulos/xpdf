//========================================================================
//
// Parser.cc
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include <stddef.h>
#include <string.h>
#include "gmempp.h"
#include "Object.h"
#include "Array.h"
#include "Dict.h"
#include "Decrypt.h"
#include "Parser.h"
#include "XRef.h"
#include "Error.h"

// Max number of nested objects.
// This is used to catch infinite loops in the object structure.
#define recursionLimit 500

Parser::Parser(XRef* xrefA, Lexer* lexerA, bool allowStreamsA)
{
	xref         = xrefA;
	lexer        = lexerA;
	allowStreams = allowStreamsA;
	lexer->getObj(&buf1);
	lexer->getObj(&buf2);
}

Parser::~Parser()
{
	buf1.free();
	buf2.free();
	delete lexer;
}

Object* Parser::getObj(Object* obj, bool simpleOnly, uint8_t* fileKey, CryptAlgorithm encAlgorithm, int keyLength, int objNum, int objGen, int recursion)
{
	char*          key;
	Stream*        str;
	Object         obj2;
	int            num;
	DecryptStream* decrypt;
	std::string    s;
	int            c;

	// refill buffer after inline image data
	if (inlineImg == 2)
	{
		buf1.free();
		buf2.free();
		lexer->getObj(&buf1);
		lexer->getObj(&buf2);
		inlineImg = 0;
	}

	// array
	if (!simpleOnly && recursion < recursionLimit && buf1.isCmd("["))
	{
		shift();
		obj->initArray(xref);
		while (!buf1.isCmd("]") && !buf1.isEOF())
			obj->arrayAdd(getObj(&obj2, false, fileKey, encAlgorithm, keyLength, objNum, objGen, recursion + 1));
		if (buf1.isEOF())
			error(errSyntaxError, getPos(), "End of file inside array");
		shift();

		// dictionary or stream
	}
	else if (!simpleOnly && recursion < recursionLimit && buf1.isCmd("<<"))
	{
		shift();
		obj->initDict(xref);
		while (!buf1.isCmd(">>") && !buf1.isEOF())
		{
			if (!buf1.isName())
			{
				error(errSyntaxError, getPos(), "Dictionary key must be a name object");
				shift();
			}
			else
			{
				key = copyString(buf1.getName());
				shift();
				if (buf1.isEOF() || buf1.isError())
				{
					gfree(key);
					break;
				}
				obj->dictAdd(key, getObj(&obj2, false, fileKey, encAlgorithm, keyLength, objNum, objGen, recursion + 1));
			}
		}
		if (buf1.isEOF())
			error(errSyntaxError, getPos(), "End of file inside dictionary");
		// stream objects are not allowed inside content streams or object streams
		if (allowStreams && buf2.isCmd("stream"))
		{
			if ((str = makeStream(obj, fileKey, encAlgorithm, keyLength, objNum, objGen, recursion + 1)))
			{
				obj->initStream(str);
			}
			else
			{
				obj->free();
				obj->initError();
			}
		}
		else
		{
			shift();
		}

		// indirect reference or integer
	}
	else if (buf1.isInt())
	{
		num = buf1.getInt();
		shift();
		if (buf1.isInt() && buf2.isCmd("R"))
		{
			obj->initRef(num, buf1.getInt());
			shift();
			shift();
		}
		else
		{
			obj->initInt(num);
		}

		// string
	}
	else if (buf1.isString() && fileKey)
	{
		s = buf1.getString();
		std::string s2;
		obj2.initNull();
		decrypt = new DecryptStream(new MemStream(s.data(), 0, s.size(), &obj2), fileKey, encAlgorithm, keyLength, objNum, objGen);
		decrypt->reset();
		while ((c = decrypt->getChar()) != EOF)
			s2 += (char)c;
		delete decrypt;
		obj->initString(s2);
		shift();

		// simple object
	}
	else
	{
		buf1.copy(obj);
		shift();
	}

	return obj;
}

Stream* Parser::makeStream(Object* dict, uint8_t* fileKey, CryptAlgorithm encAlgorithm, int keyLength, int objNum, int objGen, int recursion)
{
	// get stream start position
	lexer->skipToNextLine();
	Stream* curStr = lexer->getStream();
	if (!curStr) return nullptr;
	int64_t pos = curStr->getPos();

	bool    haveLength = false;
	int64_t length     = 0;
	int64_t endPos;

	// check for length in damaged file
	if (xref && xref->getStreamEnd(pos, &endPos))
	{
		length     = endPos - pos;
		haveLength = true;

		// get length from the stream object
	}
	else
	{
		Object obj;
		dict->dictLookup("Length", &obj, recursion);
		if (obj.isInt())
		{
			length     = (int64_t)(uint32_t)obj.getInt();
			haveLength = true;
		}
		else
		{
			error(errSyntaxError, getPos(), "Missing or invalid 'Length' attribute in stream");
		}
		obj.free();
	}

	// in badly damaged PDF files, we can run off the end of the input stream immediately after the "stream" token
	if (!lexer->getStream()) return nullptr;

	// copy the base stream (Lexer will free stream objects when it gets to end of stream
	// -- which can happen in the shift() calls below)
	BaseStream* baseStr = (BaseStream*)lexer->getStream()->getBaseStream()->copy();

	// 'Length' attribute is missing -- search for 'endstream'
	if (!haveLength)
	{
		bool foundEndstream = false;
		char endstreamBuf[8];
		if ((curStr = lexer->getStream()))
		{
			int c;
			while ((c = curStr->getChar()) != EOF)
			{
				if (c == 'e' && curStr->getBlock(endstreamBuf, 8) == 8 && !memcmp(endstreamBuf, "ndstream", 8))
				{
					length         = curStr->getPos() - 9 - pos;
					foundEndstream = true;
					break;
				}
			}
		}
		if (!foundEndstream)
		{
			error(errSyntaxError, getPos(), "Couldn't find 'endstream' for stream");
			delete baseStr;
			return nullptr;
		}
	}

	// make new base stream
	Stream* str = baseStr->makeSubStream(pos, true, length, dict);

	// look for the 'endstream' marker
	if (haveLength)
	{
		// skip over stream data
		lexer->setPos(pos + length);

		// check for 'endstream'
		// NB: we never reuse the Parser object to parse objects after a stream,
		// and we could (if the PDF file is damaged) be in the middle of binary data at this point,
		// so we check the stream data directly for 'endstream', rather than calling shift() to parse objects
		bool foundEndstream = false;
		char endstreamBuf[8];
		if ((curStr = lexer->getStream()))
		{
			// skip up to 100 whitespace chars
			int c;
			for (int i = 0; i < 100; ++i)
			{
				c = curStr->getChar();
				if (!Lexer::isSpace(c))
					break;
			}
			if (c == 'e')
			{
				if (curStr->getBlock(endstreamBuf, 8) == 8 && !memcmp(endstreamBuf, "ndstream", 8))
					foundEndstream = true;
			}
		}
		if (!foundEndstream)
		{
			error(errSyntaxError, getPos(), "Missing 'endstream'");
			// kludge for broken PDF files: just add 5k to the length, and hope it's enough
			// (dict is now owned by str, so we need to copy it before deleting str)
			Object obj;
			dict->copy(&obj);
			delete str;
			length += 5000;
			str = baseStr->makeSubStream(pos, true, length, &obj);
		}
	}

	// free the copied base stream
	delete baseStr;

	// handle decryption
	if (fileKey)
	{
		// the 'Crypt' filter is used to mark unencrypted metadata streams
		//~ this should also check for an empty DecodeParams entry
		bool   encrypted = true;
		Object obj;
		dict->dictLookup("Filter", &obj, recursion);
		if (obj.isName("Crypt"))
		{
			encrypted = false;
		}
		else if (obj.isArray() && obj.arrayGetLength() >= 1)
		{
			Object obj2;
			if (obj.arrayGet(0, &obj2)->isName("Crypt"))
				encrypted = false;
			obj2.free();
		}
		obj.free();
		if (encrypted)
			str = new DecryptStream(str, fileKey, encAlgorithm, keyLength, objNum, objGen);
	}

	// get filters
	str = str->addFilters(dict, recursion);

	return str;
}

void Parser::shift()
{
	if (inlineImg > 0)
	{
		if (inlineImg < 2)
		{
			++inlineImg;
		}
		else
		{
			// in a damaged content stream, if 'ID' shows up in the middle of a dictionary, we need to reset
			inlineImg = 0;
		}
	}
	else if (buf2.isCmd("ID"))
	{
		lexer->skipChar(); // skip char after 'ID' command
		inlineImg = 1;
	}
	buf1.free();
	buf1 = buf2;
	if (inlineImg > 0) // don't buffer inline image data
		buf2.initNull();
	else
		lexer->getObj(&buf2);
}
