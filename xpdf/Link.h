//========================================================================
//
// Link.h
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "Object.h"

class Array;
class Dict;

//------------------------------------------------------------------------
// LinkAction
//------------------------------------------------------------------------

enum LinkActionKind
{
	actionGoTo,       // go to destination
	actionGoToR,      // go to destination in new file
	actionLaunch,     // launch app (or open document)
	actionURI,        // URI
	actionNamed,      // named action
	actionMovie,      // movie action
	actionJavaScript, // run JavaScript
	actionSubmitForm, // submit form
	actionHide,       // hide annotation
	actionUnknown     // anything else
};

class LinkAction
{
public:
	// Destructor.
	virtual ~LinkAction() {}

	// Was the LinkAction created successfully?
	virtual bool isOk() = 0;

	// Check link action type.
	virtual LinkActionKind getKind() = 0;

	// Parse a destination (old-style action) name, string, or array.
	static LinkAction* parseDest(Object* obj);

	// Parse an action dictionary.
	static LinkAction* parseAction(Object* obj, const std::string& baseURI = nullptr);

	// Extract a file name from a file specification (string or
	// dictionary).
	static std::string getFileSpecName(Object* fileSpecObj);
};

//------------------------------------------------------------------------
// LinkDest
//------------------------------------------------------------------------

enum LinkDestKind
{
	destXYZ,
	destFit,
	destFitH,
	destFitV,
	destFitR,
	destFitB,
	destFitBH,
	destFitBV
};

class LinkDest
{
public:
	// Build a LinkDest from the array.
	LinkDest(Array* a);

	// Copy a LinkDest.
	LinkDest* copy() { return new LinkDest(this); }

	// Was the LinkDest created successfully?
	bool isOk() { return ok; }

	// Accessors.
	LinkDestKind getKind() { return kind; }

	bool isPageRef() { return pageIsRef; }

	int getPageNum() { return pageNum; }

	Ref getPageRef() { return pageRef; }

	double getLeft() { return left; }

	double getBottom() { return bottom; }

	double getRight() { return right; }

	double getTop() { return top; }

	double getZoom() { return zoom; }

	bool getChangeLeft() { return changeLeft; }

	bool getChangeTop() { return changeTop; }

	bool getChangeZoom() { return changeZoom; }

private:
	LinkDestKind kind      = destXYZ; // destination type
	bool         pageIsRef = false;   // is the page a reference or number?

	union
	{
		Ref pageRef; // reference to page
		int pageNum; // one-relative page number
	};

	double left       = 0;     //
	double bottom     = 0;     // position
	double right      = 0;     //
	double top        = 0;     //
	double zoom       = 0;     // zoom factor
	bool   changeLeft = false; //
	bool   changeTop  = false; // which position components to change:
	bool   changeZoom = false; // destXYZ uses all three; destFitH/BH use changeTop; destFitV/BV use changeLeft
	bool   ok         = false; // set if created successfully

	LinkDest(LinkDest* dest);
};

//------------------------------------------------------------------------
// LinkGoTo
//------------------------------------------------------------------------

class LinkGoTo : public LinkAction
{
public:
	// Build a LinkGoTo from a destination (dictionary, name, or string).
	LinkGoTo(Object* destObj);

	// Destructor.
	virtual ~LinkGoTo();

	// Was the LinkGoTo created successfully?
	virtual bool isOk() { return dest || namedDest.size(); }

	// Accessors.
	virtual LinkActionKind getKind() { return actionGoTo; }

	LinkDest* getDest() { return dest; }

	std::string getNamedDest() { return namedDest; }

private:
	LinkDest*   dest      = nullptr; // regular destination (nullptr for remote link with bad destination)
	std::string namedDest = "";      // named destination (only one of dest and and namedDest may be non-nullptr)
};

//------------------------------------------------------------------------
// LinkGoToR
//------------------------------------------------------------------------

class LinkGoToR : public LinkAction
{
public:
	// Build a LinkGoToR from a file spec (dictionary) and destination
	// (dictionary, name, or string).
	LinkGoToR(Object* fileSpecObj, Object* destObj);

	// Destructor.
	virtual ~LinkGoToR();

	// Was the LinkGoToR created successfully?
	virtual bool isOk() { return fileName.size() && (dest || namedDest.size()); }

	// Accessors.
	virtual LinkActionKind getKind() { return actionGoToR; }

	std::string getFileName() { return fileName; }

	LinkDest* getDest() { return dest; }

	std::string getNamedDest() { return namedDest; }

private:
	std::string fileName  = "";      // file name
	LinkDest*   dest      = nullptr; // regular destination (nullptr for remote link with bad destination)
	std::string namedDest = "";      // named destination (only one of dest and and namedDest may be non-nullptr)
};

//------------------------------------------------------------------------
// LinkLaunch
//------------------------------------------------------------------------

class LinkLaunch : public LinkAction
{
public:
	// Build a LinkLaunch from an action dictionary.
	LinkLaunch(Object* actionObj);

	// Destructor.
	virtual ~LinkLaunch();

	// Was the LinkLaunch created successfully?
	virtual bool isOk() { return !fileName.empty(); }

	// Accessors.
	virtual LinkActionKind getKind() { return actionLaunch; }

	std::string getFileName() { return fileName; }

	std::string getParams() { return params; }

private:
	std::string fileName = ""; // file name
	std::string params   = ""; // parameters
};

//------------------------------------------------------------------------
// LinkURI
//------------------------------------------------------------------------

class LinkURI : public LinkAction
{
public:
	// Build a LinkURI given the URI (string) and base URI.
	LinkURI(Object* uriObj, const std::string& baseURI);

	// Destructor.
	virtual ~LinkURI();

	// Was the LinkURI created successfully?
	virtual bool isOk() { return !uri.empty(); }

	// Accessors.
	virtual LinkActionKind getKind() { return actionURI; }

	std::string getURI() { return uri; }

private:
	std::string uri = ""; // the URI
};

//------------------------------------------------------------------------
// LinkNamed
//------------------------------------------------------------------------

class LinkNamed : public LinkAction
{
public:
	// Build a LinkNamed given the action name.
	LinkNamed(Object* nameObj);

	virtual ~LinkNamed();

	// Was the LinkNamed created successfully?
	virtual bool isOk() { return !name.empty(); }

	// Accessors.
	virtual LinkActionKind getKind() { return actionNamed; }

	std::string getName() { return name; }

private:
	std::string name = ""; //
};

//------------------------------------------------------------------------
// LinkMovie
//------------------------------------------------------------------------

class LinkMovie : public LinkAction
{
public:
	LinkMovie(Object* annotObj, Object* titleObj);

	virtual ~LinkMovie();

	// Was the LinkMovie created successfully?
	virtual bool isOk() { return annotRef.num >= 0 || title.size(); }

	// Accessors.
	virtual LinkActionKind getKind() { return actionMovie; }

	bool hasAnnotRef() { return annotRef.num >= 0; }

	Ref* getAnnotRef() { return &annotRef; }

	std::string getTitle() { return title; }

private:
	Ref         annotRef = {}; //
	std::string title    = ""; //
};

//------------------------------------------------------------------------
// LinkJavaScript
//------------------------------------------------------------------------

class LinkJavaScript : public LinkAction
{
public:
	LinkJavaScript(Object* jsObj);

	virtual ~LinkJavaScript();

	// Was the LinkJavaScript created successfully?
	virtual bool isOk() { return !js.empty(); }

	// Accessors.
	virtual LinkActionKind getKind() { return actionJavaScript; }

	std::string getJS() { return js; }

private:
	std::string js = ""; //
};

//------------------------------------------------------------------------
// LinkSubmitForm
//------------------------------------------------------------------------

class LinkSubmitForm : public LinkAction
{
public:
	LinkSubmitForm(Object* urlObj, Object* fieldsObj, Object* flagsObj);

	virtual ~LinkSubmitForm();

	// Was the LinkSubmitForm created successfully?
	virtual bool isOk() { return !url.empty(); }

	// Accessors.
	virtual LinkActionKind getKind() { return actionSubmitForm; }

	std::string getURL() { return url; }

	Object* getFields() { return &fields; }

	int getFlags() { return flags; }

private:
	std::string url    = ""; //
	Object      fields = {}; //
	int         flags  = 0;  //
};

//------------------------------------------------------------------------
// LinkHide
//------------------------------------------------------------------------

class LinkHide : public LinkAction
{
public:
	LinkHide(Object* fieldsObj, Object* hideFlagObj);

	virtual ~LinkHide();

	// Was the LinkHide created successfully?
	virtual bool isOk() { return !fields.isNull(); }

	// Accessors.
	virtual LinkActionKind getKind() { return actionHide; }

	Object* getFields() { return &fields; }

	bool getHideFlag() { return hideFlag; }

private:
	Object fields   = {};    //
	bool   hideFlag = false; //
};

//------------------------------------------------------------------------
// LinkUnknown
//------------------------------------------------------------------------

class LinkUnknown : public LinkAction
{
public:
	// Build a LinkUnknown with the specified action type.
	LinkUnknown(std::string_view actionA);

	// Destructor.
	virtual ~LinkUnknown();

	// Was the LinkUnknown create successfully?
	virtual bool isOk() { return !action.empty(); }

	// Accessors.
	virtual LinkActionKind getKind() { return actionUnknown; }

	std::string getAction() { return action; }

private:
	std::string action = ""; // action subtype
};

//------------------------------------------------------------------------
// Link
//------------------------------------------------------------------------

class Link
{
public:
	// Construct a link, given its dictionary.
	Link(Dict* dict, const std::string& baseURI);

	// Destructor.
	~Link();

	// Was the link created successfully?
	bool isOk() { return ok; }

	// Check if point is inside the link rectangle.
	bool inRect(double x, double y)
	{
		return x1 <= x && x <= x2 && y1 <= y && y <= y2;
	}

	// Get action.
	LinkAction* getAction() { return action; }

	// Get the link rectangle.
	void getRect(double* xa1, double* ya1, double* xa2, double* ya2)
	{
		*xa1 = x1;
		*ya1 = y1;
		*xa2 = x2;
		*ya2 = y2;
	}

private:
	double      x1     = 0;       //
	double      y1     = 0;       // lower left corner
	double      x2     = 0;       //
	double      y2     = 0;       // upper right corner
	LinkAction* action = nullptr; // action
	bool        ok     = false;   // is link valid?
};

//------------------------------------------------------------------------
// Links
//------------------------------------------------------------------------

class Links
{
public:
	// Extract links from array of annotations.
	Links(Object* annots, const std::string& baseURI);

	// Destructor.
	~Links();

	// Iterate through list of links.
	int getNumLinks() { return numLinks; }

	Link* getLink(int i) { return links[i]; }

	// If point <x>,<y> is in a link, return the associated action;
	// else return nullptr.
	LinkAction* find(double x, double y);

	// Return true if <x>,<y> is in a link.
	bool onLink(double x, double y);

private:
	Link** links    = nullptr; //
	int    numLinks = 0;       //
};
