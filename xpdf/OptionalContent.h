//========================================================================
//
// OptionalContent.h
//
// Copyright 2008-2013 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "Object.h"
#include "CharTypes.h"

class GList;
class PDFDoc;
class TextString;
class XRef;
class OptionalContentGroup;
class OCDisplayNode;

//------------------------------------------------------------------------

class OptionalContent
{
public:
	OptionalContent(PDFDoc* doc);
	~OptionalContent();

	// Walk the list of optional content groups.
	int                   getNumOCGs();
	OptionalContentGroup* getOCG(int idx);

	// Find an OCG by indirect reference.
	OptionalContentGroup* findOCG(Ref* ref);

	// Get the root node of the optional content group display tree
	// (which does not necessarily include all of the OCGs).
	OCDisplayNode* getDisplayRoot() { return display; }

	// Evaluate an optional content object -- either an OCG or an OCMD.
	// If <obj> is a valid OCG or OCMD, sets *<visible> and returns
	// true; otherwise returns false.
	bool evalOCObject(Object* obj, bool* visible);

private:
	bool evalOCVisibilityExpr(Object* expr, int recursion);

	XRef*          xref    = nullptr; //
	GList*         ocgs    = nullptr; // all OCGs [OptionalContentGroup]
	OCDisplayNode* display = nullptr; // root node of display tree
};

//------------------------------------------------------------------------

// Values from the optional content usage dictionary.
enum OCUsageState
{
	ocUsageOn,
	ocUsageOff,
	ocUsageUnset
};

//------------------------------------------------------------------------

class OptionalContentGroup
{
public:
	static OptionalContentGroup* parse(Ref* refA, Object* obj);
	~OptionalContentGroup();

	bool matches(Ref* refA);

	Unicode* getName();
	int      getNameLength();

	OCUsageState getViewState() { return viewState; }

	OCUsageState getPrintState() { return printState; }

	bool getState() { return state; }

	void setState(bool stateA) { state = stateA; }

	bool getInViewUsageAppDict() { return inViewUsageAppDict; }

	void setInViewUsageAppDict() { inViewUsageAppDict = true; }

private:
	OptionalContentGroup(Ref* refA, TextString* nameA, OCUsageState viewStateA, OCUsageState printStateA);

	Ref          ref                = {};           //
	TextString*  name               = nullptr;      //
	OCUsageState viewState          = ocUsageUnset; // suggested state when viewing
	OCUsageState printState         = ocUsageUnset; // suggested state when printing
	bool         state              = false;        // current state (on/off)
	bool         inViewUsageAppDict = false;        // true if this OCG is listed in a usage app dict with Event=View

	friend class OCDisplayNode;
};

//------------------------------------------------------------------------

class OCDisplayNode
{
public:
	static OCDisplayNode* parse(Object* obj, OptionalContent* oc, XRef* xref, int recursion = 0);
	OCDisplayNode();
	~OCDisplayNode();

	Unicode* getName();
	int      getNameLength();

	OptionalContentGroup* getOCG() { return ocg; }

	int            getNumChildren();
	OCDisplayNode* getChild(int idx);

	OCDisplayNode* getParent() { return parent; }

private:
	OCDisplayNode(const std::string& nameA);
	OCDisplayNode(OptionalContentGroup* ocgA);
	void   addChild(OCDisplayNode* child);
	void   addChildren(GList* childrenA);
	GList* takeChildren();

	TextString*           name     = nullptr; // display name
	OptionalContentGroup* ocg      = nullptr; // nullptr for display labels
	OCDisplayNode*        parent   = nullptr; // parent node; nullptr at root
	GList*                children = nullptr; // nullptr if there are no children [OCDisplayNode]
};
