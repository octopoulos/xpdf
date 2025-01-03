//========================================================================
//
// Outline.h
//
// Copyright 2002-2013 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "Object.h"
#include "CharTypes.h"

class GList;
class XRef;
class LinkAction;
class TextString;

//------------------------------------------------------------------------

class Outline
{
public:
	Outline(Object* outlineObj, XRef* xref);
	~Outline();

	GList* getItems() { return items; }

private:
	GList* items; // nullptr if document has no outline [OutlineItem]
};

//------------------------------------------------------------------------

class OutlineItem
{
public:
	OutlineItem(Object* itemRefA, Dict* dict, OutlineItem* parentA, XRef* xrefA);
	~OutlineItem();

	static GList* readItemList(Object* firstItemRef, Object* lastItemRef, OutlineItem* parentA, XRef* xrefA);

	void open();
	void close();

	Unicode* getTitle();
	int      getTitleLength();

	TextString* getTitleTextString() { return title; }

	LinkAction* getAction() { return action; }

	bool isOpen() { return startsOpen; }

	bool hasKids() { return firstRef.isRef(); }

	GList* getKids() { return kids; }

	OutlineItem* getParent() { return parent; }

private:
	friend class PDFDoc;

	XRef*        xref;       //
	TextString*  title;      // may be nullptr
	LinkAction*  action;     //
	Object       itemRef;    //
	Object       firstRef;   //
	Object       lastRef;    //
	Object       nextRef;    //
	bool         startsOpen; //
	int          pageNum;    // page number (used by PDFDoc::getOutlineTargetPage)
	GList*       kids;       // nullptr unless this item is open [OutlineItem]
	OutlineItem* parent;     //
};
