//========================================================================
//
// Object.h
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include <stdio.h>
#include <string.h>
#include "gmem.h"
#include "gfile.h"
#if MULTITHREADED
#	include "GMutex.h"
#endif

class XRef;
class Array;
class Dict;
class Stream;

//------------------------------------------------------------------------
// Ref
//------------------------------------------------------------------------

struct Ref
{
	int num = 0; // object number
	int gen = 0; // generation number
};

//------------------------------------------------------------------------
// object types
//------------------------------------------------------------------------

enum ObjType
{
	// simple objects
	objBool,   // boolean
	objInt,    // integer
	objReal,   // real
	objString, // string
	objName,   // name
	objNull,   // null

	// complex objects
	objArray,  // array
	objDict,   // dictionary
	objStream, // stream
	objRef,    // indirect reference

	// special objects
	objCmd,   // command name
	objError, // error return from Lexer
	objEOF,   // end of file return from Lexer
	objNone,  // uninitialized object
};

#define numObjTypes 14 // total number of object types

//------------------------------------------------------------------------
// Object
//------------------------------------------------------------------------

#ifdef DEBUG_OBJECT_MEM
#	if MULTITHREADED
#		define initObj(t) gAtomicIncrement(&numAlloc[type = t])
#	else
#		define initObj(t) ++numAlloc[type = t]
#	endif
#else
#	define initObj(t) type = t
#endif

class Object
{
public:
	// Default constructor.
	Object()
	    : type(objNone)
	{
	}

	~Object() {}

	// Initialize an object.
	Object* initBool(bool boolnA)
	{
		initObj(objBool);
		booln = boolnA;
		return this;
	}

	Object* initInt(int intgA)
	{
		initObj(objInt);
		intg = intgA;
		return this;
	}

	Object* initReal(double realA)
	{
		initObj(objReal);
		real = realA;
		return this;
	}

	Object* initString(const std::string& stringA)
	{
		initObj(objString);
		string = new std::string(stringA);
		return this;
	}

	Object* initName(const char* nameA)
	{
		initObj(objName);
		name = copyString(nameA);
		return this;
	}

	Object* initNull()
	{
		initObj(objNull);
		return this;
	}

	Object* initArray(XRef* xref);
	Object* initDict(XRef* xref);
	Object* initDict(Dict* dictA);
	Object* initStream(Stream* streamA);

	Object* initRef(int numA, int genA)
	{
		initObj(objRef);
		ref.num = numA;
		ref.gen = genA;
		return this;
	}

	Object* initCmd(char* cmdA)
	{
		initObj(objCmd);
		cmd = copyString(cmdA);
		return this;
	}

	Object* initError()
	{
		initObj(objError);
		return this;
	}

	Object* initEOF()
	{
		initObj(objEOF);
		return this;
	}

	// Copy an object.
	Object* copy(Object* obj);

	// If object is a Ref, fetch and return the referenced object.
	// Otherwise, return a copy of the object.
	Object* fetch(XRef* xref, Object* obj, int recursion = 0);

	// Free object contents.
	void free();

	// Type checking.
	ObjType getType() { return type; }

	bool isBool() { return type == objBool; }

	bool isInt() { return type == objInt; }

	bool isReal() { return type == objReal; }

	bool isNum() { return type == objInt || type == objReal; }

	bool isString() { return type == objString; }

	bool isName() { return type == objName; }

	bool isNull() { return type == objNull; }

	bool isArray() { return type == objArray; }

	bool isDict() { return type == objDict; }

	bool isStream() { return type == objStream; }

	bool isRef() { return type == objRef; }

	bool isCmd() { return type == objCmd; }

	bool isError() { return type == objError; }

	bool isEOF() { return type == objEOF; }

	bool isNone() { return type == objNone; }

	// Special type checking.
	bool isName(const char* nameA)
	{
		return type == objName && !strcmp(name, nameA);
	}

	bool isDict(const char* dictType);
	bool isStream(char* dictType);

	bool isCmd(const char* cmdA)
	{
		return type == objCmd && !strcmp(cmd, cmdA);
	}

	// Accessors.  NB: these assume object is of correct type.
	bool getBool() { return booln; }

	int getInt() { return intg; }

	double getReal() { return real; }

	double getNum() { return type == objInt ? (double)intg : real; }

	std::string getString() { return *string; }

	char* getName() { assert(name != nullptr); return name; }

	Array* getArray() { return array; }

	Dict* getDict() { return dict; }

	Stream* getStream() { return stream; }

	Ref getRef() { return ref; }

	int getRefNum() { return ref.num; }

	int getRefGen() { return ref.gen; }

	char* getCmd() { return cmd; }

	// Array accessors.
	int     arrayGetLength();
	void    arrayAdd(Object* elem);
	Object* arrayGet(int i, Object* obj, int recursion = 0);
	Object* arrayGetNF(int i, Object* obj);

	// Dict accessors.
	int     dictGetLength();
	void    dictAdd(char* key, Object* val);
	bool    dictIs(const char* dictType);
	Object* dictLookup(const char* key, Object* obj, int recursion = 0);
	Object* dictLookupNF(const char* key, Object* obj);
	char*   dictGetKey(int i);
	Object* dictGetVal(int i, Object* obj);
	Object* dictGetValNF(int i, Object* obj);

	// Stream accessors.
	bool    streamIs(char* dictType);
	void    streamReset();
	void    streamClose();
	int     streamGetChar();
	int     streamLookChar();
	int     streamGetBlock(char* blk, int size);
	char*   streamGetLine(char* buf, int size);
	int64_t streamGetPos();
	void    streamSetPos(int64_t pos, int dir = 0);
	Dict*   streamGetDict();

	// Output.
	const char* getTypeName();
	void        print(FILE* f = stdout);

	// Memory testing.
	static void memCheck(FILE* f);

private:
	ObjType type = objNone; // object type

	union
	{                        // value for each type:
		bool         booln;  // boolean
		int          intg;   // integer
		double       real;   // real
		char*        name;   // name
		Array*       array;  // array
		Dict*        dict;   // dictionary
		Stream*      stream; // stream
		std::string* string; // string
		Ref          ref;    // indirect reference
		char*        cmd;    // command
	};

#ifdef DEBUG_OBJECT_MEM
	static REFCNT_TYPE numAlloc[numObjTypes] = {}; // number of each type of object currently allocated
#endif
};

//------------------------------------------------------------------------
// Array accessors.
//------------------------------------------------------------------------

#include "Array.h"

inline int Object::arrayGetLength()
{
	return array->getLength();
}

inline void Object::arrayAdd(Object* elem)
{
	array->add(elem);
}

inline Object* Object::arrayGet(int i, Object* obj, int recursion)
{
	return array->get(i, obj, recursion);
}

inline Object* Object::arrayGetNF(int i, Object* obj)
{
	return array->getNF(i, obj);
}

//------------------------------------------------------------------------
// Dict accessors.
//------------------------------------------------------------------------

#include "Dict.h"

inline int Object::dictGetLength()
{
	return dict->getLength();
}

inline void Object::dictAdd(char* key, Object* val)
{
	dict->add(key, val);
}

inline bool Object::dictIs(const char* dictType)
{
	return dict->is(dictType);
}

inline bool Object::isDict(const char* dictType)
{
	return type == objDict && dictIs(dictType);
}

inline Object* Object::dictLookup(const char* key, Object* obj, int recursion)
{
	return dict->lookup(key, obj, recursion);
}

inline Object* Object::dictLookupNF(const char* key, Object* obj)
{
	return dict->lookupNF(key, obj);
}

inline char* Object::dictGetKey(int i)
{
	return dict->getKey(i);
}

inline Object* Object::dictGetVal(int i, Object* obj)
{
	return dict->getVal(i, obj);
}

inline Object* Object::dictGetValNF(int i, Object* obj)
{
	return dict->getValNF(i, obj);
}

//------------------------------------------------------------------------
// Stream accessors.
//------------------------------------------------------------------------

#include "Stream.h"

inline bool Object::streamIs(char* dictType)
{
	return stream->getDict()->is(dictType);
}

inline bool Object::isStream(char* dictType)
{
	return type == objStream && streamIs(dictType);
}

inline void Object::streamReset()
{
	stream->reset();
}

inline void Object::streamClose()
{
	stream->close();
}

inline int Object::streamGetChar()
{
	return stream->getChar();
}

inline int Object::streamLookChar()
{
	return stream->lookChar();
}

inline int Object::streamGetBlock(char* blk, int size)
{
	return stream->getBlock(blk, size);
}

inline char* Object::streamGetLine(char* buf, int size)
{
	return stream->getLine(buf, size);
}

inline int64_t Object::streamGetPos()
{
	return stream->getPos();
}

inline void Object::streamSetPos(int64_t pos, int dir)
{
	stream->setPos(pos, dir);
}

inline Dict* Object::streamGetDict()
{
	return stream->getDict();
}
