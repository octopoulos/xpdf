//========================================================================
//
// Array.h
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>

#if MULTITHREADED
#	include "GMutex.h"
#endif
#include "Object.h"

class XRef;

//------------------------------------------------------------------------
// Array
//------------------------------------------------------------------------

class Array
{
public:
	// Constructor.
	Array(XRef* xrefA);

	// Destructor.
	~Array();

	// Reference counting.
#if MULTITHREADED
	long incRef() { return gAtomicIncrement(&ref); }

	long decRef() { return gAtomicDecrement(&ref); }
#else
	long incRef() { return ++ref; }

	long decRef() { return --ref; }
#endif

	// Get number of elements.
	int getLength() { return TO_INT(elems.size()); }

	// Add an element.
	void add(Object* elem);

	// Accessors.
	Object* get(int i, Object* obj, int recursion = 0);
	Object* getNF(int i, Object* obj);

private:
	XRef*               xref;   // the xref table for this PDF file
	std::vector<Object> elems;  // array of elements
#if MULTITHREADED
	GAtomicCounter ref; // reference count
#else
	long ref; // reference count
#endif
};
