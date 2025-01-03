//========================================================================
//
// Array.cc
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include <stdlib.h>
#include <stddef.h>
#include "gmem.h"
#include "gmempp.h"
#include "Object.h"
#include "Array.h"

//------------------------------------------------------------------------
// Array
//------------------------------------------------------------------------

Array::Array(XRef* xrefA)
{
	xref = xrefA;
	ref  = 1;
}

Array::~Array()
{
	for (int i = 0; i < elems.size(); ++i)
		elems[i].free();
}

void Array::add(Object* elem)
{
	elems.push_back(*elem);
}

Object* Array::get(int i, Object* obj, int recursion)
{
	if (i < 0 || i >= elems.size())
	{
#ifdef DEBUG_OBJECT_MEM
		abort();
#else
		return obj->initNull();
#endif
	}
	return elems[i].fetch(xref, obj, recursion);
}

Object* Array::getNF(int i, Object* obj)
{
	if (i < 0 || i >= elems.size())
	{
#ifdef DEBUG_OBJECT_MEM
		abort();
#else
		return obj->initNull();
#endif
	}
	return elems[i].copy(obj);
}
