//========================================================================
//
// Page.h
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "Object.h"

class Dict;
class PDFDoc;
class XRef;
class OutputDev;
class Links;

//------------------------------------------------------------------------

class PDFRectangle
{
public:
	double x1 = 0; //
	double y1 = 0; //
	double x2 = 0; //
	double y2 = 0; //

	PDFRectangle() { x1 = y1 = x2 = y2 = 0; }

	PDFRectangle(double x1A, double y1A, double x2A, double y2A)
	{
		x1 = x1A;
		y1 = y1A;
		x2 = x2A;
		y2 = y2A;
	}

	bool isValid() { return x1 != 0 || y1 != 0 || x2 != 0 || y2 != 0; }

	void clipTo(PDFRectangle* rect);
};

//------------------------------------------------------------------------
// PageAttrs
//------------------------------------------------------------------------

class PageAttrs
{
public:
	// Construct a new PageAttrs object by merging a dictionary
	// (of type Pages or Page) into another PageAttrs object.  If
	// <attrs> is nullptr, uses defaults.
	PageAttrs(PageAttrs* attrs, Dict* dict, XRef* xref);

	// Construct a new PageAttrs object for an empty page (only used
	// when there is an error in the page tree).
	PageAttrs();

	// Destructor.
	~PageAttrs();

	// Accessors.
	PDFRectangle* getMediaBox() { return &mediaBox; }

	PDFRectangle* getCropBox() { return &cropBox; }

	bool isCropped() { return haveCropBox; }

	PDFRectangle* getBleedBox() { return &bleedBox; }

	PDFRectangle* getTrimBox() { return &trimBox; }

	PDFRectangle* getArtBox() { return &artBox; }

	int getRotate() { return rotate; }

	std::string getLastModified()
	{
		return lastModified.isString() ? lastModified.getString() : "";
	}

	Dict* getBoxColorInfo()
	{
		return boxColorInfo.isDict() ? boxColorInfo.getDict() : (Dict*)nullptr;
	}

	Dict* getGroup()
	{
		return group.isDict() ? group.getDict() : (Dict*)nullptr;
	}

	Stream* getMetadata()
	{
		return metadata.isStream() ? metadata.getStream() : (Stream*)nullptr;
	}

	Dict* getPieceInfo()
	{
		return pieceInfo.isDict() ? pieceInfo.getDict() : (Dict*)nullptr;
	}

	Dict* getSeparationInfo()
	{
		return separationInfo.isDict()
		    ? separationInfo.getDict()
		    : (Dict*)nullptr;
	}

	double getUserUnit() { return userUnit; }

	Dict* getResourceDict()
	{
		return resources.isDict() ? resources.getDict() : (Dict*)nullptr;
	}

	// Clip all other boxes to the MediaBox.
	void clipBoxes();

private:
	bool readBox(Dict* dict, const char* key, PDFRectangle* box);

	PDFRectangle mediaBox       = {};    //
	PDFRectangle cropBox        = {};    //
	bool         haveCropBox    = false; //
	PDFRectangle bleedBox       = {};    //
	PDFRectangle trimBox        = {};    //
	PDFRectangle artBox         = {};    //
	int          rotate         = 0;     //
	Object       lastModified   = {};    //
	Object       boxColorInfo   = {};    //
	Object       group          = {};    //
	Object       metadata       = {};    //
	Object       pieceInfo      = {};    //
	Object       separationInfo = {};    //
	double       userUnit       = 1;     //
	Object       resources      = {};    //
};

//------------------------------------------------------------------------
// Page
//------------------------------------------------------------------------

class Page
{
public:
	// Constructor.
	Page(PDFDoc* docA, int numA, Dict* pageDict, PageAttrs* attrsA);

	// Create an empty page (only used when there is an error in the
	// page tree).
	Page(PDFDoc* docA, int numA);

	// Destructor.
	~Page();

	// Is page valid?
	bool isOk() { return ok; }

	// Get page parameters.
	int getNum() { return num; }

	PageAttrs* getAttrs() { return attrs; }

	PDFRectangle* getMediaBox() { return attrs->getMediaBox(); }

	PDFRectangle* getCropBox() { return attrs->getCropBox(); }

	bool isCropped() { return attrs->isCropped(); }

	double getMediaWidth()
	{
		return attrs->getMediaBox()->x2 - attrs->getMediaBox()->x1;
	}

	double getMediaHeight()
	{
		return attrs->getMediaBox()->y2 - attrs->getMediaBox()->y1;
	}

	double getCropWidth()
	{
		return attrs->getCropBox()->x2 - attrs->getCropBox()->x1;
	}

	double getCropHeight()
	{
		return attrs->getCropBox()->y2 - attrs->getCropBox()->y1;
	}

	PDFRectangle* getBleedBox() { return attrs->getBleedBox(); }

	PDFRectangle* getTrimBox() { return attrs->getTrimBox(); }

	PDFRectangle* getArtBox() { return attrs->getArtBox(); }

	int getRotate() { return attrs->getRotate(); }

	std::string getLastModified() { return attrs->getLastModified(); }

	Dict* getBoxColorInfo() { return attrs->getBoxColorInfo(); }

	Dict* getGroup() { return attrs->getGroup(); }

	Stream* getMetadata() { return attrs->getMetadata(); }

	Dict* getPieceInfo() { return attrs->getPieceInfo(); }

	Dict* getSeparationInfo() { return attrs->getSeparationInfo(); }

	double getUserUnit() { return attrs->getUserUnit(); }

	// Get resource dictionary.
	Dict* getResourceDict() { return attrs->getResourceDict(); }

	// Get annotations array.
	Object* getAnnots(Object* obj) { return annots.fetch(xref, obj); }

	// Return a list of links.
	Links* getLinks();

	// Get contents.
	Object* getContents(Object* obj) { return contents.fetch(xref, obj); }

	// Get the page's thumbnail image.
	Object* getThumbnail(Object* obj) { return thumbnail.fetch(xref, obj); }

	// Display a page.
	void display(OutputDev* out, double hDPI, double vDPI, int rotate, bool useMediaBox, bool crop, bool printing, bool (*abortCheckCbk)(void* data) = nullptr, void* abortCheckCbkData = nullptr);

	// Display part of a page.
	void displaySlice(OutputDev* out, double hDPI, double vDPI, int rotate, bool useMediaBox, bool crop, int sliceX, int sliceY, int sliceW, int sliceH, bool printing, bool (*abortCheckCbk)(void* data) = nullptr, void* abortCheckCbkData = nullptr);

	void makeBox(double hDPI, double vDPI, int rotate, bool useMediaBox, bool upsideDown, double sliceX, double sliceY, double sliceW, double sliceH, PDFRectangle* box, bool* crop);

	void processLinks(OutputDev* out);

	// Get the page's default CTM.
	void getDefaultCTM(double* ctm, double hDPI, double vDPI, int rotate, bool useMediaBox, bool upsideDown);

private:
	PDFDoc*    doc       = nullptr; //
	XRef*      xref      = nullptr; // the xref table for this PDF file
	int        num       = 0;       // page number
	PageAttrs* attrs     = nullptr; // page attributes
	Object     annots    = {};      // annotations array
	Object     contents  = {};      // page contents
	Object     thumbnail = {};      // reference to thumbnail image
	bool       ok        = false;   // true if page is valid
};
