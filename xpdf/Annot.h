//========================================================================
//
// Annot.h
//
// Copyright 2000-2022 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "Object.h" // Ref

class XRef;
class Catalog;
class Gfx;
class GfxFontDict;
class PDFDoc;
class PageAnnots;

//------------------------------------------------------------------------
// AnnotBorderStyle
//------------------------------------------------------------------------

enum AnnotBorderType
{
	annotBorderSolid,
	annotBorderDashed,
	annotBorderBeveled,
	annotBorderInset,
	annotBorderUnderlined
};

class AnnotBorderStyle
{
public:
	AnnotBorderStyle(AnnotBorderType typeA, double widthA, double* dashA, int dashLengthA, double* colorA, int nColorCompsA);
	~AnnotBorderStyle();

	AnnotBorderType getType() { return type; }

	double getWidth() { return width; }

	void getDash(double** dashA, int* dashLengthA)
	{
		*dashA       = dash;
		*dashLengthA = dashLength;
	}

	int getNumColorComps() { return nColorComps; }

	double* getColor() { return color; }

private:
	AnnotBorderType type;
	double          width;
	double*         dash;
	int             dashLength;
	double          color[4];
	int             nColorComps;
};

//------------------------------------------------------------------------

enum AnnotLineEndType
{
	annotLineEndNone,
	annotLineEndSquare,
	annotLineEndCircle,
	annotLineEndDiamond,
	annotLineEndOpenArrow,
	annotLineEndClosedArrow,
	annotLineEndButt,
	annotLineEndROpenArrow,
	annotLineEndRClosedArrow,
	annotLineEndSlash
};

//------------------------------------------------------------------------
// Annot
//------------------------------------------------------------------------

class Annot
{
public:
	Annot(PDFDoc* docA, Dict* dict, Ref* refA);
	~Annot();

	bool isOk() { return ok; }

	void draw(Gfx* gfx, bool printing);

	std::string getType() { return type; }

	double getXMin() { return xMin; }

	double getYMin() { return yMin; }

	double getXMax() { return xMax; }

	double getYMax() { return yMax; }

	Object* getObject(Object* obj);

	// Check if point is inside the annotation rectangle.
	bool inRect(double x, double y)
	{
		return xMin <= x && x <= xMax && yMin <= y && y <= yMax;
	}

	// Get appearance object.
	Object* getAppearance(Object* obj) { return appearance.fetch(xref, obj); }

	AnnotBorderStyle* getBorderStyle() { return borderStyle; }

	bool match(Ref* refA)
	{
		return ref.num == refA->num && ref.gen == refA->gen;
	}

	void generateAnnotAppearance(Object* annotObj);

private:
	void             generateLineAppearance(Object* annotObj);
	void             generatePolyLineAppearance(Object* annotObj);
	void             generatePolygonAppearance(Object* annotObj);
	void             generateFreeTextAppearance(Object* annotObj);
	void             setLineStyle(AnnotBorderStyle* bs, double* lineWidth);
	void             setStrokeColor(double* color, int nComps);
	bool             setFillColor(Object* colorObj);
	AnnotLineEndType parseLineEndType(Object* obj);
	void             adjustLineEndpoint(AnnotLineEndType lineEnd, double x, double y, double dx, double dy, double w, double* tx, double* ty);
	void             drawLineArrow(AnnotLineEndType lineEnd, double x, double y, double dx, double dy, double w, bool fill);
	void             drawCircle(double cx, double cy, double r, const char* cmd);
	void             drawCircleTopLeft(double cx, double cy, double r);
	void             drawCircleBottomRight(double cx, double cy, double r);
	void             drawText(const std::string& text, const std::string& da, int quadding, double margin, int rot);

	PDFDoc*           doc;             //
	XRef*             xref;            // the xref table for this PDF file
	Ref               ref;             // object ref identifying this annotation
	std::string       type;            // annotation type
	std::string       appearanceState; // appearance state name
	Object            appearance;      // a reference to the Form XObject stream for the normal appearance
	std::string       appearBuf;       //
	double            xMin;            //
	double            yMin;            // annotation rectangle
	double            xMax;            //
	double            yMax;            //
	uint32_t          flags;           //
	AnnotBorderStyle* borderStyle;     //
	Object            ocObj;           // optional content entry
	bool              ok;              //
};

//------------------------------------------------------------------------
// Annots
//------------------------------------------------------------------------

class Annots
{
public:
	Annots(PDFDoc* docA);

	~Annots();

	// Iterate over annotations on a specific page.
	int    getNumAnnots(int page);
	Annot* getAnnot(int page, int idx);

	// If point (<x>,<y>) is in an annotation, return the associated
	// annotation (or annotation index); else return nullptr (or -1).
	Annot* find(int page, double x, double y);
	int    findIdx(int page, double x, double y);

	// Add an annotation [annotObj] on page [page].
	void add(int page, Object* annotObj);

	// Generate an appearance stream for any non-form-field annotation
	// on the specified page that is missing an appearance.
	void generateAnnotAppearances(int page);

private:
	void loadAnnots(int page);
	void loadFormFieldRefs();

	PDFDoc*      doc;               //
	PageAnnots** pageAnnots;        // list of annots for each page
	int          formFieldRefsSize; // number of entries in formFieldRefs[]
	char*        formFieldRefs;     // set of AcroForm field refs
};
