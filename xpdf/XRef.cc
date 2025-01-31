//========================================================================
//
// XRef.cc
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include "gmem.h"
#include "gmempp.h"
#include "gfile.h"
#include "Object.h"
#include "Stream.h"
#include "Lexer.h"
#include "Parser.h"
#include "Dict.h"
#include "Error.h"
#include "ErrorCodes.h"
#include "XRef.h"

//------------------------------------------------------------------------

#define xrefSearchSize 1024 // read this many bytes at end of file to look for 'startxref'

//------------------------------------------------------------------------
// Permission bits
//------------------------------------------------------------------------

#define permPrint    (1 << 2)
#define permChange   (1 << 3)
#define permCopy     (1 << 4)
#define permNotes    (1 << 5)
#define defPermFlags 0xfffc

//------------------------------------------------------------------------
// XRefPosSet
//------------------------------------------------------------------------

class XRefPosSet
{
public:
	XRefPosSet();
	~XRefPosSet();
	void add(int64_t pos);
	bool check(int64_t pos);

	int getLength() { return len; }

	int64_t get(int idx) { return tab[idx]; }

private:
	int find(int64_t pos);

	int64_t* tab  = nullptr; //
	int      size = 0;       //
	int      len  = 0;       //
};

XRefPosSet::XRefPosSet()
{
	size = 16;
	len  = 0;
	tab  = (int64_t*)gmallocn(size, sizeof(int64_t));
}

XRefPosSet::~XRefPosSet()
{
	gfree(tab);
}

void XRefPosSet::add(int64_t pos)
{
	int i;

	i = find(pos);
	if (i < len && tab[i] == pos)
		return;
	if (len == size)
	{
		if (size > INT_MAX / 2)
			gMemError("Integer overflow in XRefPosSet::add()");
		size *= 2;
		tab = (int64_t*)greallocn(tab, size, sizeof(int64_t));
	}
	if (i < len)
		memmove(&tab[i + 1], &tab[i], (len - i) * sizeof(int64_t));
	tab[i] = pos;
	++len;
}

bool XRefPosSet::check(int64_t pos)
{
	int i;

	i = find(pos);
	return i < len && tab[i] == pos;
}

int XRefPosSet::find(int64_t pos)
{
	int a, b, m;

	a = -1;
	b = len;
	// invariant: tab[a] < pos < tab[b]
	while (b - a > 1)
	{
		m = (a + b) / 2;
		if (tab[m] < pos)
			a = m;
		else if (tab[m] > pos)
			b = m;
		else
			return m;
	}
	return b;
}

//------------------------------------------------------------------------
// ObjectStream
//------------------------------------------------------------------------

class ObjectStream
{
public:
	// Create an object stream, using object number <objStrNum>,
	// generation 0.
	ObjectStream(XRef* xref, int objStrNumA, int recursion);

	bool isOk() { return ok; }

	~ObjectStream();

	// Return the object number of this object stream.
	int getObjStrNum() { return objStrNum; }

	// Get the <objIdx>th object from this stream, which should be
	// object number <objNum>, generation 0.
	Object* getObject(int objIdx, int objNum, Object* obj);

private:
	int     objStrNum = 0;       // object number of the object stream
	int     nObjects  = 0;       // number of objects in the stream
	Object* objs      = nullptr; // the objects (length = nObjects)
	int*    objNums   = nullptr; // the object numbers (length = nObjects)
	bool    ok        = false;   //
};

ObjectStream::ObjectStream(XRef* xref, int objStrNumA, int recursion)
{
	Stream* str;
	Lexer*  lexer;
	Parser* parser;
	int*    offsets;
	Object  objStr, obj1, obj2;
	int     first;

	objStrNum = objStrNumA;
	nObjects  = 0;
	objs      = nullptr;
	objNums   = nullptr;
	ok        = false;

	if (!xref->fetch(objStrNum, 0, &objStr, recursion)->isStream())
		goto err1;

	if (!objStr.streamGetDict()->lookup("N", &obj1)->isInt())
	{
		obj1.free();
		goto err1;
	}
	nObjects = obj1.getInt();
	obj1.free();
	if (nObjects <= 0)
		goto err1;

	if (!objStr.streamGetDict()->lookup("First", &obj1)->isInt())
	{
		obj1.free();
		goto err1;
	}
	first = obj1.getInt();
	obj1.free();
	if (first < 0)
		goto err1;

	// this is an arbitrary limit to avoid integer overflow problems
	// in the 'new Object[nObjects]' call (Acrobat apparently limits
	// object streams to 100-200 objects)
	if (nObjects > 1000000)
	{
		error(errSyntaxError, -1, "Too many objects in an object stream");
		goto err1;
	}
	objs    = new Object[nObjects];
	objNums = (int*)gmallocn(nObjects, sizeof(int));
	offsets = (int*)gmallocn(nObjects, sizeof(int));

	// parse the header: object numbers and offsets
	objStr.streamReset();
	obj1.initNull();
	str    = new EmbedStream(objStr.getStream(), &obj1, true, first);
	lexer  = new Lexer(xref, str);
	parser = new Parser(xref, lexer, false);
	for (int i = 0; i < nObjects; ++i)
	{
		parser->getObj(&obj1, true);
		parser->getObj(&obj2, true);
		if (!obj1.isInt() || !obj2.isInt())
		{
			obj1.free();
			obj2.free();
			delete parser;
			gfree(offsets);
			goto err2;
		}
		objNums[i] = obj1.getInt();
		offsets[i] = obj2.getInt();
		obj1.free();
		obj2.free();
		if (objNums[i] < 0 || offsets[i] < 0 || (i > 0 && offsets[i] < offsets[i - 1]))
		{
			delete parser;
			gfree(offsets);
			goto err2;
		}
	}
	lexer->skipToEOF();
	delete parser;

	// skip to the first object - this generally shouldn't be needed,
	// because offsets[0] is normally 0, but just in case...
	if (offsets[0] > 0)
		objStr.getStream()->discardChars(offsets[0]);

	// parse the objects
	for (int i = 0; i < nObjects; ++i)
	{
		obj1.initNull();
		if (i == nObjects - 1)
			str = new EmbedStream(objStr.getStream(), &obj1, false, 0);
		else
			str = new EmbedStream(objStr.getStream(), &obj1, true, offsets[i + 1] - offsets[i]);
		lexer  = new Lexer(xref, str);
		parser = new Parser(xref, lexer, false);
		parser->getObj(&objs[i]);
		lexer->skipToEOF();
		delete parser;
	}

	gfree(offsets);
	ok = true;

err2:
	objStr.streamClose();
err1:
	objStr.free();
}

ObjectStream::~ObjectStream()
{
	if (objs)
	{
		for (int i = 0; i < nObjects; ++i) objs[i].free();
		delete[] objs;
	}
	gfree(objNums);
}

Object* ObjectStream::getObject(int objIdx, int objNum, Object* obj)
{
	if (objIdx < 0 || objIdx >= nObjects || objNum != objNums[objIdx])
		obj->initNull();
	else
		objs[objIdx].copy(obj);
	return obj;
}

//------------------------------------------------------------------------
// XRef
//------------------------------------------------------------------------

XRef::XRef(BaseStream* strA, bool repair)
{
	ok = true;

	for (int i = 0; i < objStrCacheSize; ++i)
	{
		objStrs[i]       = nullptr;
		objStrLastUse[i] = 0;
	}
	permFlags = defPermFlags;

	for (int i = 0; i < xrefCacheSize; ++i)
		cache[i].num = -1;

#if MULTITHREADED
	gInitMutex(&objStrsMutex);
	gInitMutex(&cacheMutex);
#endif

	str   = strA;
	start = str->getStart();

	// if the 'repair' flag is set, try to reconstruct the xref table
	if (repair)
	{
		if (!(ok = constructXRef()))
		{
			errCode = errDamaged;
			return;
		}
		repaired = true;

		// if the 'repair' flag is not set, read the xref table
	}
	else
	{
		// read the trailer
		int64_t pos = getStartXref();
		if (pos == 0)
		{
			errCode = errDamaged;
			ok      = false;
			return;
		}

		// read the xref table
		{
			XRefPosSet posSet;
			while (readXRef(&pos, &posSet, false))
				;
			xrefTablePosLen = posSet.getLength();
			xrefTablePos    = (int64_t*)gmallocn(xrefTablePosLen, sizeof(int64_t));
			for (int i = 0; i < xrefTablePosLen; ++i)
				xrefTablePos[i] = posSet.get(i);
		}
		if (!ok)
		{
			errCode = errDamaged;
			return;
		}
	}

	// get the root dictionary (catalog) object
	Object obj;
	trailerDict.dictLookupNF("Root", &obj);
	if (obj.isRef())
	{
		rootNum = obj.getRefNum();
		rootGen = obj.getRefGen();
		obj.free();
	}
	else
	{
		obj.free();
		if (!(ok = constructXRef()))
		{
			errCode = errDamaged;
			return;
		}
	}

	// now set the trailer dictionary's xref pointer so we can fetch indirect objects from it
	trailerDict.getDict()->setXRef(this);
}

XRef::~XRef()
{
	for (int i = 0; i < xrefCacheSize; ++i)
		if (cache[i].num >= 0)
			cache[i].obj.free();
	gfree(entries);
	trailerDict.free();
	if (xrefTablePos) gfree(xrefTablePos);
	if (streamEnds) gfree(streamEnds);
	for (int i = 0; i < objStrCacheSize; ++i)
		if (objStrs[i])
			delete objStrs[i];
#if MULTITHREADED
	gDestroyMutex(&objStrsMutex);
	gDestroyMutex(&cacheMutex);
#endif
}

// Read the 'startxref' position.
int64_t XRef::getStartXref()
{
	char  buf[xrefSearchSize + 1];
	char* p;
	int   n, i;

	// read last xrefSearchSize bytes
	str->setPos(xrefSearchSize, -1);
	n      = str->getBlock(buf, xrefSearchSize);
	buf[n] = '\0';

	// find startxref
	for (i = n - 9; i >= 0; --i)
		if (!strncmp(&buf[i], "startxref", 9))
			break;
	if (i < 0)
		return 0;
	for (p = &buf[i + 9]; isspace(*p & 0xff); ++p)
		;
	lastXRefPos      = strToFileOffset(p);
	lastStartxrefPos = str->getPos() - n + i;

	return lastXRefPos;
}

// Read one xref table section.  Also reads the associated trailer
// dictionary, and returns the prev pointer (if any).  The [hybrid]
// flag is true when following the XRefStm link in a hybrid-reference
// file.
bool XRef::readXRef(int64_t* pos, XRefPosSet* posSet, bool hybrid)
{
	Parser* parser;
	Object  obj;
	bool    more;
	char    buf[100];
	int     n, i;

	// check for a loop in the xref tables
	if (posSet->check(*pos))
	{
		error(errSyntaxWarning, -1, "Infinite loop in xref table");
		return false;
	}
	posSet->add(*pos);

	// the xref data should either be "xref ..." (for an xref table) or "nn gg obj << ... >> stream ..." (for an xref stream);
	// possibly preceded by whitespace
	str->setPos(start + *pos);
	n = str->getBlock(buf, 100);
	for (i = 0; i < n && Lexer::isSpace(buf[i]); ++i)
		;

	// parse an old-style xref table
	if (!hybrid && i + 4 < n && buf[i] == 'x' && buf[i + 1] == 'r' && buf[i + 2] == 'e' && buf[i + 3] == 'f' && Lexer::isSpace(buf[i + 4]))
	{
		more = readXRefTable(pos, i + 5, posSet);

		// parse an xref stream
	}
	else
	{
		obj.initNull();
		parser = new Parser(nullptr, new Lexer(nullptr, str->makeSubStream(start + *pos, false, 0, &obj)), true);
		if (!parser->getObj(&obj, true)->isInt())
			goto err;
		obj.free();
		if (!parser->getObj(&obj, true)->isInt())
			goto err;
		obj.free();
		if (!parser->getObj(&obj, true)->isCmd("obj"))
			goto err;
		obj.free();
		if (!parser->getObj(&obj)->isStream())
			goto err;
		more = readXRefStream(obj.getStream(), pos, hybrid);
		obj.free();
		delete parser;
	}

	return more;

err:
	obj.free();
	delete parser;
	ok = false;
	return false;
}

bool XRef::readXRefTable(int64_t* pos, int offset, XRefPosSet* posSet)
{
	XRefEntry entry;
	Parser*   parser;
	Object    obj, obj2;
	char      buf[6];
	int64_t   off, pos2;
	bool      more;
	int       first, n, digit, newSize, gen, c;

	str->setPos(start + *pos + offset);

	while (1)
	{
		do
		{
			c = str->getChar();
		}
		while (Lexer::isSpace(c));
		if (c == 't')
		{
			if (str->getBlock(buf, 6) != 6 || memcmp(buf, "railer", 6)) goto err1;
			break;
		}
		if (c < '0' || c > '9') goto err1;
		first = 0;
		do
		{
			digit = c - '0';
			if (first > (INT_MAX - digit) / 10)
				goto err1;
			first = (first * 10) + digit;
			c     = str->getChar();
		}
		while (c >= '0' && c <= '9');
		if (!Lexer::isSpace(c))
			goto err1;
		do
		{
			c = str->getChar();
		}
		while (Lexer::isSpace(c));
		n = 0;
		do
		{
			digit = c - '0';
			if (n > (INT_MAX - digit) / 10) goto err1;
			n = (n * 10) + digit;
			c = str->getChar();
		}
		while (c >= '0' && c <= '9');
		if (!Lexer::isSpace(c)) goto err1;
		if (first > INT_MAX - n) goto err1;
		if (first + n > size)
		{
			for (newSize = size ? 2 * size : 1024; first + n > newSize && newSize > 0; newSize <<= 1)
				;
			if (newSize < 0)
				goto err1;
			entries = (XRefEntry*)greallocn(entries, newSize, sizeof(XRefEntry));
			for (int i = size; i < newSize; ++i)
			{
				entries[i].offset = (int64_t)-1;
				entries[i].type   = xrefEntryFree;
			}
			size = newSize;
		}
		for (int i = first; i < first + n; ++i)
		{
			do
			{
				c = str->getChar();
			}
			while (Lexer::isSpace(c));
			off = 0;
			do
			{
				off = (off * 10) + (c - '0');
				c   = str->getChar();
			}
			while (c >= '0' && c <= '9');
			if (!Lexer::isSpace(c))
				goto err1;
			entry.offset = off;
			do
			{
				c = str->getChar();
			}
			while (Lexer::isSpace(c));
			gen = 0;
			do
			{
				gen = (gen * 10) + (c - '0');
				c   = str->getChar();
			}
			while (c >= '0' && c <= '9');
			if (!Lexer::isSpace(c))
				goto err1;
			entry.gen = gen;
			do
			{
				c = str->getChar();
			}
			while (Lexer::isSpace(c));
			if (c == 'n')
				entry.type = xrefEntryUncompressed;
			else if (c == 'f')
				entry.type = xrefEntryFree;
			else
				goto err1;
			c = str->getChar();
			if (!Lexer::isSpace(c))
				goto err1;
			if (entries[i].offset == (int64_t)-1)
			{
				entries[i] = entry;
				// PDF files of patents from the IBM Intellectual Property
				// Network have a bug: the xref table claims to start at 1
				// instead of 0.
				if (i == 1 && first == 1 && entries[1].offset == 0 && entries[1].gen == 65535 && entries[1].type == xrefEntryFree)
				{
					i = first         = 0;
					entries[0]        = entries[1];
					entries[1].offset = (int64_t)-1;
				}
				if (i > last)
					last = i;
			}
		}
	}

	// read the trailer dictionary
	obj.initNull();
	parser = new Parser(nullptr, new Lexer(nullptr, str->makeSubStream(str->getPos(), false, 0, &obj)), true);
	parser->getObj(&obj);
	delete parser;
	if (!obj.isDict())
	{
		obj.free();
		goto err1;
	}

	// get the 'Prev' pointer
	//~ this can be a 64-bit int (?)
	obj.getDict()->lookupNF("Prev", &obj2);
	if (obj2.isInt())
	{
		*pos = (int64_t)(uint32_t)obj2.getInt();
		more = true;
	}
	else if (obj2.isRef())
	{
		// certain buggy PDF generators generate "/Prev NNN 0 R" instead
		// of "/Prev NNN"
		*pos = (int64_t)(uint32_t)obj2.getRefNum();
		more = true;
	}
	else
	{
		more = false;
	}
	obj2.free();

	// save the first trailer dictionary
	if (trailerDict.isNone())
		obj.copy(&trailerDict);

	// check for an 'XRefStm' key
	//~ this can be a 64-bit int (?)
	if (obj.getDict()->lookup("XRefStm", &obj2)->isInt())
	{
		pos2 = (int64_t)(uint32_t)obj2.getInt();
		readXRef(&pos2, posSet, true);
		if (!ok)
		{
			obj2.free();
			obj.free();
			goto err1;
		}
	}
	obj2.free();

	obj.free();
	return more;

err1:
	ok = false;
	return false;
}

bool XRef::readXRefStream(Stream* xrefStr, int64_t* pos, bool hybrid)
{
	Dict*  dict;
	int    w[3];
	bool   more;
	Object obj, obj2, idx;
	int    newSize, first, n;

	dict = xrefStr->getDict();

	if (!dict->lookupNF("Size", &obj)->isInt())
		goto err1;
	newSize = obj.getInt();
	obj.free();
	if (newSize < 0)
		goto err1;
	if (newSize > size)
	{
		entries = (XRefEntry*)greallocn(entries, newSize, sizeof(XRefEntry));
		for (int i = size; i < newSize; ++i)
		{
			entries[i].offset = (int64_t)-1;
			entries[i].type   = xrefEntryFree;
		}
		size = newSize;
	}

	if (!dict->lookupNF("W", &obj)->isArray() || obj.arrayGetLength() < 3)
		goto err1;
	for (int i = 0; i < 3; ++i)
	{
		if (!obj.arrayGet(i, &obj2)->isInt())
		{
			obj2.free();
			goto err1;
		}
		w[i] = obj2.getInt();
		obj2.free();
	}
	obj.free();
	if (w[0] < 0 || w[0] > 8 || w[1] < 0 || w[1] > 8 || w[2] < 0 || w[2] > 8)
		goto err0;

	xrefStr->reset();
	dict->lookupNF("Index", &idx);
	if (idx.isArray())
	{
		for (int i = 0; i + 1 < idx.arrayGetLength(); i += 2)
		{
			if (!idx.arrayGet(i, &obj)->isInt())
			{
				idx.free();
				goto err1;
			}
			first = obj.getInt();
			obj.free();
			if (!idx.arrayGet(i + 1, &obj)->isInt())
			{
				idx.free();
				goto err1;
			}
			n = obj.getInt();
			obj.free();
			if (first < 0 || n < 0 || !readXRefStreamSection(xrefStr, w, first, n))
			{
				idx.free();
				goto err0;
			}
		}
	}
	else if (!readXRefStreamSection(xrefStr, w, 0, newSize))
	{
		idx.free();
		goto err0;
	}
	idx.free();

	//~ this can be a 64-bit int (?)
	dict->lookupNF("Prev", &obj);
	if (obj.isInt())
	{
		*pos = (int64_t)(uint32_t)obj.getInt();
		more = true;
	}
	else
	{
		more = false;
	}
	obj.free();
	if (trailerDict.isNone())
		trailerDict.initDict(dict);

	return more;

err1:
	obj.free();
err0:
	ok = false;
	return false;
}

bool XRef::readXRefStreamSection(Stream* xrefStr, int* w, int first, int n)
{
	long long type, gen, offset;
	int       c, newSize, j;

	if (first + n < 0) return false;
	if (first + n > size)
	{
		for (newSize = size ? 2 * size : 1024; first + n > newSize && newSize > 0; newSize <<= 1)
			;
		if (newSize < 0) return false;
		entries = (XRefEntry*)greallocn(entries, newSize, sizeof(XRefEntry));
		for (int i = size; i < newSize; ++i)
		{
			entries[i].offset = (int64_t)-1;
			entries[i].type   = xrefEntryFree;
		}
		size = newSize;
	}
	for (int i = first; i < first + n; ++i)
	{
		if (w[0] == 0)
		{
			type = 1;
		}
		else
		{
			for (type = 0, j = 0; j < w[0]; ++j)
			{
				if ((c = xrefStr->getChar()) == EOF) return false;
				type = (type << 8) + c;
			}
		}
		for (offset = 0, j = 0; j < w[1]; ++j)
		{
			if ((c = xrefStr->getChar()) == EOF) return false;
			offset = (offset << 8) + c;
		}
		if (offset < 0 || offset > GFILEOFFSET_MAX) return false;
		for (gen = 0, j = 0; j < w[2]; ++j)
		{
			if ((c = xrefStr->getChar()) == EOF) return false;
			gen = (gen << 8) + c;
		}
		// some PDF generators include a free entry with gen=0xffffffff
		if ((gen < 0 || gen > INT_MAX) && type != 0) return false;
		if (entries[i].offset == (int64_t)-1)
		{
			switch (type)
			{
			case 0:
				entries[i].offset = (int64_t)offset;
				entries[i].gen    = (int)gen;
				entries[i].type   = xrefEntryFree;
				break;
			case 1:
				entries[i].offset = (int64_t)offset;
				entries[i].gen    = (int)gen;
				entries[i].type   = xrefEntryUncompressed;
				break;
			case 2:
				entries[i].offset = (int64_t)offset;
				entries[i].gen    = (int)gen;
				entries[i].type   = xrefEntryCompressed;
				break;
			default: return false;
			}
			if (i > last)
				last = i;
		}
	}

	return true;
}

// Attempt to construct an xref table for a damaged file.
bool XRef::constructXRef()
{
	int* streamObjNums     = nullptr;
	int  streamObjNumsLen  = 0;
	int  streamObjNumsSize = 0;
	int  lastObjNum        = -1;
	rootNum                = -1;
	int streamEndsSize     = 0;
	streamEndsLen          = 0;
	char buf[4096 + 1];
	str->reset();
	int64_t bufPos      = start;
	char*   p           = buf;
	char*   end         = buf;
	bool    startOfLine = true;
	bool    space       = true;
	bool    eof         = false;
	while (1)
	{
		if (end - p < 256 && !eof)
		{
			memcpy(buf, p, end - p);
			bufPos += p - buf;
			p     = buf + (end - p);
			int n = (int)(buf + 4096 - p);
			int m = str->getBlock(p, n);
			end   = p + m;
			*end  = '\0';
			p     = buf;
			eof   = m < n;
		}
		if (p == end && eof)
			break;
		if (startOfLine && !strncmp(p, "trailer", 7))
		{
			constructTrailerDict((int64_t)(bufPos + (p + 7 - buf)));
			p += 7;
			startOfLine = false;
			space       = false;
		}
		else if (startOfLine && !strncmp(p, "endstream", 9))
		{
			if (streamEndsLen == streamEndsSize)
			{
				streamEndsSize += 64;
				streamEnds = (int64_t*)greallocn(streamEnds, streamEndsSize, sizeof(int64_t));
			}
			streamEnds[streamEndsLen++] = (int64_t)(bufPos + (p - buf));
			p += 9;
			startOfLine = false;
			space       = false;
		}
		else if (space && *p >= '0' && *p <= '9')
		{
			p           = constructObjectEntry(p, (int64_t)(bufPos + (p - buf)), &lastObjNum);
			startOfLine = false;
			space       = false;
		}
		else if (p[0] == '>' && p[1] == '>')
		{
			p += 2;
			startOfLine = false;
			space       = false;
			// skip any PDF whitespace except for '\0'
			while (*p == '\t' || *p == '\n' || *p == '\x0c' || *p == '\r' || *p == ' ')
			{
				if (*p == '\n' || *p == '\r')
					startOfLine = true;
				space = true;
				++p;
			}
			if (!strncmp(p, "stream", 6))
			{
				if (lastObjNum >= 0)
				{
					if (streamObjNumsLen == streamObjNumsSize)
					{
						streamObjNumsSize += 64;
						streamObjNums = (int*)greallocn(streamObjNums, streamObjNumsSize, sizeof(int));
					}
					streamObjNums[streamObjNumsLen++] = lastObjNum;
				}
				p += 6;
				startOfLine = false;
				space       = false;
			}
		}
		else
		{
			if (*p == '\n' || *p == '\r')
			{
				startOfLine = true;
				space       = true;
			}
			else if (Lexer::isSpace(*p & 0xff))
			{
				space = true;
			}
			else
			{
				startOfLine = false;
				space       = false;
			}
			++p;
		}
	}

	// read each stream object, check for xref or object stream
	for (int i = 0; i < streamObjNumsLen; ++i)
	{
		Object obj;
		fetch(streamObjNums[i], entries[streamObjNums[i]].gen, &obj);
		if (obj.isStream())
		{
			Dict*  dict = obj.streamGetDict();
			Object type;
			dict->lookup("Type", &type);
			if (type.isName("XRef"))
				saveTrailerDict(dict, true);
			else if (type.isName("ObjStm"))
				constructObjectStreamEntries(&obj, streamObjNums[i]);
			type.free();
		}
		obj.free();
	}

	gfree(streamObjNums);

	// if the file is encrypted, then any objects fetched here will be
	// incorrect (because decryption is not yet enabled), so clear the
	// cache to avoid that problem
	for (int i = 0; i < xrefCacheSize; ++i)
	{
		if (cache[i].num >= 0)
		{
			cache[i].obj.free();
			cache[i].num = -1;
		}
	}

	if (rootNum < 0)
	{
		error(errSyntaxError, -1, "Couldn't find trailer dictionary");
		return false;
	}
	return true;
}

// Attempt to construct a trailer dict at [pos] in the stream.
void XRef::constructTrailerDict(int64_t pos)
{
	Object newTrailerDict, obj;
	obj.initNull();
	Parser* parser = new Parser(nullptr, new Lexer(nullptr, str->makeSubStream(pos, false, 0, &obj)), false);
	parser->getObj(&newTrailerDict);
	if (newTrailerDict.isDict())
		saveTrailerDict(newTrailerDict.getDict(), false);
	newTrailerDict.free();
	delete parser;
}

// If [dict] "looks like" a trailer dict (i.e., has a Root entry),
// save it as the trailer dict.
void XRef::saveTrailerDict(Dict* dict, bool isXRefStream)
{
	Object obj;
	dict->lookupNF("Root", &obj);
	if (obj.isRef())
	{
		int newRootNum = obj.getRefNum();
		// the xref stream scanning code runs after all objects are found,
		// so we can check for a valid root object number at that point
		if (!isXRefStream || newRootNum <= last)
		{
			rootNum = newRootNum;
			rootGen = obj.getRefGen();
			if (!trailerDict.isNone())
				trailerDict.free();
			trailerDict.initDict(dict);
		}
	}
	obj.free();
}

// Look for an object header ("nnn ggg obj") at [p].
// The first character at *[p] is a digit.  [pos] is the position of *[p].
char* XRef::constructObjectEntry(char* p, int64_t pos, int* objNum)
{
	// we look for non-end-of-line space characters here, to deal with situations like:
	//    nnn          <-- garbage digits on a line
	//    nnn nnn obj  <-- actual object
	// and we also ignore '\0' (because it's used to terminate the buffer in this damage-scanning code)
	int num = 0;
	do
	{
		num = (num * 10) + (*p - '0');
		++p;
	}
	while (*p >= '0' && *p <= '9' && num < 100000000);
	if (*p != '\t' && *p != '\x0c' && *p != ' ')
		return p;
	do
	{
		++p;
	}
	while (*p == '\t' || *p == '\x0c' || *p == ' ');
	if (!(*p >= '0' && *p <= '9'))
		return p;
	int gen = 0;
	do
	{
		gen = (gen * 10) + (*p - '0');
		++p;
	}
	while (*p >= '0' && *p <= '9' && gen < 100000000);
	if (*p != '\t' && *p != '\x0c' && *p != ' ')
		return p;
	do
	{
		++p;
	}
	while (*p == '\t' || *p == '\x0c' || *p == ' ');
	if (strncmp(p, "obj", 3))
		return p;

	if (constructXRefEntry(num, gen, pos - start, xrefEntryUncompressed))
		*objNum = num;

	return p;
}

// Read the header from an object stream, and add xref entries for all
// of its objects.
void XRef::constructObjectStreamEntries(Object* objStr, int objStrObjNum)
{
	Object obj1, obj2;

	// get the object count
	if (!objStr->streamGetDict()->lookup("N", &obj1)->isInt())
	{
		obj1.free();
		return;
	}
	int nObjects = obj1.getInt();
	obj1.free();
	if (nObjects <= 0 || nObjects > 1000000)
		return;

	// parse the header: object numbers and offsets
	Parser* parser = new Parser(nullptr, new Lexer(nullptr, objStr->getStream()->copy()), false);
	for (int i = 0; i < nObjects; ++i)
	{
		parser->getObj(&obj1, true);
		parser->getObj(&obj2, true);
		if (obj1.isInt() && obj2.isInt())
		{
			int num = obj1.getInt();
			if (num >= 0 && num < 1000000)
				constructXRefEntry(num, i, objStrObjNum, xrefEntryCompressed);
		}
		obj2.free();
		obj1.free();
	}
	delete parser;
}

bool XRef::constructXRefEntry(int num, int gen, int64_t pos, XRefEntryType type)
{
	if (num >= size)
	{
		int newSize = (num + 1 + 255) & ~255;
		if (newSize < 0) return false;
		entries = (XRefEntry*)greallocn(entries, newSize, sizeof(XRefEntry));
		for (int i = size; i < newSize; ++i)
		{
			entries[i].offset = (int64_t)-1;
			entries[i].type   = xrefEntryFree;
		}
		size = newSize;
	}

	if (entries[num].type == xrefEntryFree || gen >= entries[num].gen)
	{
		entries[num].offset = pos;
		entries[num].gen    = gen;
		entries[num].type   = type;
		if (num > last)
			last = num;
	}

	return true;
}

void XRef::setEncryption(int permFlagsA, bool ownerPasswordOkA, uint8_t* fileKeyA, int keyLengthA, int encVersionA, CryptAlgorithm encAlgorithmA)
{
	encrypted       = true;
	permFlags       = permFlagsA;
	ownerPasswordOk = ownerPasswordOkA;
	if (keyLengthA <= 32)
		keyLength = keyLengthA;
	else
		keyLength = 32;
	for (int i = 0; i < keyLength; ++i)
		fileKey[i] = fileKeyA[i];
	encVersion   = encVersionA;
	encAlgorithm = encAlgorithmA;
}

bool XRef::getEncryption(int* permFlagsA, bool* ownerPasswordOkA, int* keyLengthA, int* encVersionA, CryptAlgorithm* encAlgorithmA)
{
	if (!encrypted) return false;
	*permFlagsA       = permFlags;
	*ownerPasswordOkA = ownerPasswordOk;
	*keyLengthA       = keyLength;
	*encVersionA      = encVersion;
	*encAlgorithmA    = encAlgorithm;
	return true;
}

bool XRef::okToPrint(bool ignoreOwnerPW)
{
	return (!ignoreOwnerPW && ownerPasswordOk) || (permFlags & permPrint);
}

bool XRef::okToChange(bool ignoreOwnerPW)
{
	return (!ignoreOwnerPW && ownerPasswordOk) || (permFlags & permChange);
}

bool XRef::okToCopy(bool ignoreOwnerPW)
{
	return (!ignoreOwnerPW && ownerPasswordOk) || (permFlags & permCopy);
}

bool XRef::okToAddNotes(bool ignoreOwnerPW)
{
	return (!ignoreOwnerPW && ownerPasswordOk) || (permFlags & permNotes);
}

Object* XRef::fetch(int num, int gen, Object* obj, int recursion)
{
	XRefEntry*     e;
	Parser*        parser;
	Object         obj1, obj2, obj3;
	XRefCacheEntry tmp;

	// check for bogus ref - this can happen in corrupted PDF files
	if (num < 0 || num >= size) goto err;

		// check the cache
#if MULTITHREADED
	gLockMutex(&cacheMutex);
#endif
	if (cache[0].num == num && cache[0].gen == gen)
	{
		cache[0].obj.copy(obj);
#if MULTITHREADED
		gUnlockMutex(&cacheMutex);
#endif
		return obj;
	}
	for (int i = 1; i < xrefCacheSize; ++i)
	{
		if (cache[i].num == num && cache[i].gen == gen)
		{
			tmp = cache[i];
			for (int j = i; j > 0; --j)
				cache[j] = cache[j - 1];
			cache[0] = tmp;
			cache[0].obj.copy(obj);
#if MULTITHREADED
			gUnlockMutex(&cacheMutex);
#endif
			return obj;
		}
	}
#if MULTITHREADED
	gUnlockMutex(&cacheMutex);
#endif

	e = &entries[num];
	switch (e->type)
	{
	case xrefEntryUncompressed:
		if (e->gen != gen) goto err;
		obj1.initNull();
		parser = new Parser(this, new Lexer(this, str->makeSubStream(start + e->offset, false, 0, &obj1)), true);
		parser->getObj(&obj1, true);
		parser->getObj(&obj2, true);
		parser->getObj(&obj3, true);
		if (!obj1.isInt() || obj1.getInt() != num || !obj2.isInt() || obj2.getInt() != gen || !obj3.isCmd("obj"))
		{
			obj1.free();
			obj2.free();
			obj3.free();
			delete parser;
			goto err;
		}
		parser->getObj(obj, false, encrypted ? fileKey : (uint8_t*)nullptr, encAlgorithm, keyLength, num, gen, recursion);
		obj1.free();
		obj2.free();
		obj3.free();
		delete parser;
		break;

	case xrefEntryCompressed:
#if 0 // Adobe apparently ignores the generation number on compressed objects
		if (gen != 0) {
			goto err;
		}
#endif
		if (e->offset >= (int64_t)size || entries[e->offset].type != xrefEntryUncompressed)
		{
			error(errSyntaxError, -1, "Invalid object stream");
			goto err;
		}
		if (!getObjectStreamObject((int)e->offset, e->gen, num, obj, recursion))
			goto err;
		break;

	default:
		goto err;
	}

	// put the new object in the cache, throwing away the oldest object
	// currently in the cache
#if MULTITHREADED
	gLockMutex(&cacheMutex);
#endif
	if (cache[xrefCacheSize - 1].num >= 0)
		cache[xrefCacheSize - 1].obj.free();
	for (int i = xrefCacheSize - 1; i > 0; --i)
		cache[i] = cache[i - 1];
	cache[0].num = num;
	cache[0].gen = gen;
	obj->copy(&cache[0].obj);
#if MULTITHREADED
	gUnlockMutex(&cacheMutex);
#endif

	return obj;

err:
	return obj->initNull();
}

bool XRef::getObjectStreamObject(int objStrNum, int objIdx, int objNum, Object* obj, int recursion)
{
	// check for a cached ObjectStream
#if MULTITHREADED
	gLockMutex(&objStrsMutex);
#endif
	ObjectStream* objStr = getObjectStreamFromCache(objStrNum);
	bool          found  = false;
	if (objStr)
	{
		objStr->getObject(objIdx, objNum, obj);
		cleanObjectStreamCache();
		found = true;
	}
#if MULTITHREADED
	gUnlockMutex(&objStrsMutex);
#endif
	if (found) return true;

	// load a new ObjectStream
	objStr = new ObjectStream(this, objStrNum, recursion);
	if (!objStr->isOk())
	{
		delete objStr;
		return false;
	}
	objStr->getObject(objIdx, objNum, obj);
#if MULTITHREADED
	gLockMutex(&objStrsMutex);
#endif
	addObjectStreamToCache(objStr);
	cleanObjectStreamCache();
#if MULTITHREADED
	gUnlockMutex(&objStrsMutex);
#endif
	return true;
}

// NB: objStrsMutex must be locked when calling this function.
ObjectStream* XRef::getObjectStreamFromCache(int objStrNum)
{
	// check the MRU entry in the cache
	if (objStrs[0] && objStrs[0]->getObjStrNum() == objStrNum)
	{
		ObjectStream* objStr = objStrs[0];
		objStrLastUse[0]     = objStrTime++;
		return objStr;
	}

	// check the rest of the cache
	for (int i = 1; i < objStrCacheLength; ++i)
	{
		if (objStrs[i] && objStrs[i]->getObjStrNum() == objStrNum)
		{
			ObjectStream* objStr = objStrs[i];
			for (int j = i; j > 0; --j)
			{
				objStrs[j]       = objStrs[j - 1];
				objStrLastUse[j] = objStrLastUse[j - 1];
			}
			objStrs[0]       = objStr;
			objStrLastUse[0] = objStrTime++;
			return objStr;
		}
	}

	return nullptr;
}

// NB: objStrsMutex must be locked when calling this function.
void XRef::addObjectStreamToCache(ObjectStream* objStr)
{
	// add to the cache
	if (objStrCacheLength == objStrCacheSize)
	{
		delete objStrs[objStrCacheSize - 1];
		--objStrCacheLength;
	}
	for (int j = objStrCacheLength; j > 0; --j)
	{
		objStrs[j]       = objStrs[j - 1];
		objStrLastUse[j] = objStrLastUse[j - 1];
	}
	++objStrCacheLength;
	objStrs[0]       = objStr;
	objStrLastUse[0] = objStrTime++;
}

// If the oldest (least recently used) entry in the object stream cache is more than objStrCacheTimeout accesses old
// (hasn't been used in the last objStrCacheTimeout accesses), eject it from the cache.
// NB: objStrsMutex must be locked when calling this function.
void XRef::cleanObjectStreamCache()
{
	// NB: objStrTime and objStrLastUse[] are unsigned ints,
	// so the mod-2^32 arithmetic makes the subtraction work out, even if the time wraps around.
	if (objStrCacheLength > 1 && objStrTime - objStrLastUse[objStrCacheLength - 1] > objStrCacheTimeout)
	{
		delete objStrs[objStrCacheLength - 1];
		objStrs[objStrCacheLength - 1] = nullptr;
		--objStrCacheLength;
	}
}

Object* XRef::getDocInfo(Object* obj)
{
	return trailerDict.dictLookup("Info", obj);
}

// Added for the pdftex project.
Object* XRef::getDocInfoNF(Object* obj)
{
	return trailerDict.dictLookupNF("Info", obj);
}

bool XRef::getStreamEnd(int64_t streamStart, int64_t* streamEnd)
{
	int a, b, m;

	if (streamEndsLen == 0 || streamStart > streamEnds[streamEndsLen - 1])
		return false;

	a = -1;
	b = streamEndsLen - 1;
	// invariant: streamEnds[a] < streamStart <= streamEnds[b]
	while (b - a > 1)
	{
		m = (a + b) / 2;
		if (streamStart <= streamEnds[m])
			b = m;
		else
			a = m;
	}
	*streamEnd = streamEnds[b];
	return true;
}

int64_t XRef::strToFileOffset(char* s)
{
	int64_t x, d;
	char*   p;

	x = 0;
	for (p = s; *p && isdigit(*p & 0xff); ++p)
	{
		d = *p - '0';
		if (x > (GFILEOFFSET_MAX - d) / 10)
			break;
		x = 10 * x + d;
	}
	return x;
}
