//========================================================================
//
// GList.h
//
// Copyright 2001-2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>

//------------------------------------------------------------------------
// GList
//------------------------------------------------------------------------

class GList
{
public:
	// Create an empty list.
	GList();

	// Create an empty list with space for <size1> elements.
	GList(int sizeA);

	// Destructor - does not free pointed-to objects.
	~GList();

	//----- general

	// Get the number of elements.
	int getLength() { return length; }

	// Returns a (shallow) copy of this list.
	GList* copy();

	//----- ordered list support

	// Return the <i>th element.
	// Assumes 0 <= i < length.
	void* get(int i) { return data[i]; }

	// Replace the <i>th element.
	// Assumes 0 <= i < length.
	void put(int i, void* p) { data[i] = p; }

	// Append an element to the end of the list.
	void append(void* p);

	// Append another list to the end of this one.
	void append(GList* list);

	// Insert an element at index <i>.
	// Assumes 0 <= i <= length.
	void insert(int i, void* p);

	// Deletes and returns the element at index <i>.
	// Assumes 0 <= i < length.
	void* del(int i);

	// Sort the list accoring to the given comparison function.
	// NB: this sorts an array of pointers, so the pointer args need to
	// be double-dereferenced.
	void sort(int (*cmp)(const void* ptr1, const void* ptr2));

	// Sort <n> items starting at <first>.
	void sort(int first, int n, int (*cmp)(const void* ptr1, const void* ptr2));

	// Reverse the list.
	void reverse();

	//----- control

	// Set allocation increment to <inc>.  If inc > 0, that many
	// elements will be allocated every time the list is expanded.
	// If inc <= 0, the list will be doubled in size.
	void setAllocIncr(int incA) { inc = incA; }

private:
	void expand();
	void shrink();

	void** data   = nullptr; // the list elements
	int    size   = 8;       // size of data array
	int    length = 0;       // number of elements on list
	int    inc    = 0;       // allocation increment
};

#define deleteGList(list, T)                            \
	do {                                                \
		GList* _list = (list);                          \
		{                                               \
			int _i;                                     \
			for (_i = 0; _i < _list->getLength(); ++_i) \
			{                                           \
				delete (T*)_list->get(_i);              \
			}                                           \
			delete _list;                               \
		}                                               \
	}                                                   \
	while (0)
