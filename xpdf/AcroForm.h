//========================================================================
//
// AcroForm.h
//
// Copyright 2012 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "CharTypes.h" // Unicode
#include "Object.h"    // Ref

class AcroFormField;
class Catalog;
class Dict;
class Gfx;
class GfxFont;
class GfxFontDict;
class GList;
class Object;
class PDFDoc;
class TextString;
class XFAField;
class XFAFieldBarcodeInfo;
class XFAScanner;

//------------------------------------------------------------------------

class AcroForm
{
public:
	static AcroForm* load(PDFDoc* docA, Catalog* catalog, Object* acroFormObjA);

	~AcroForm();

	const char* getType();

	void draw(int pageNum, Gfx* gfx, bool printing);

	int            getNumFields();
	AcroFormField* getField(int idx);
	AcroFormField* findField(int pg, double x, double y);
	int            findFieldIdx(int pg, double x, double y);

private:
	AcroForm(PDFDoc* docA, Object* acroFormObjA);
	void buildAnnotPageList(Catalog* catalog);
	int  lookupAnnotPage(Object* annotRef);
	void scanField(Object* fieldRef, char* touchedObjs);

	PDFDoc*     doc             = nullptr; //
	Object      acroFormObj     = {};      //
	bool        needAppearances = false;   //
	GList*      annotPages      = nullptr; // [AcroFormAnnotPage]
	GList*      fields          = nullptr; // [AcroFormField]
	XFAScanner* xfaScanner      = nullptr; //
	bool        isStaticXFA     = false;   //

	friend class AcroFormField;
};

//------------------------------------------------------------------------

enum AcroFormFieldType
{
	acroFormFieldPushbutton,
	acroFormFieldRadioButton,
	acroFormFieldCheckbox,
	acroFormFieldFileSelect,
	acroFormFieldMultilineText,
	acroFormFieldText,
	acroFormFieldBarcode,
	acroFormFieldComboBox,
	acroFormFieldListBox,
	acroFormFieldSignature
};

class AcroFormField
{
public:
	static AcroFormField* load(AcroForm* acroFormA, Object* fieldRefA);

	~AcroFormField();

	int         getPageNum();
	const char* getType();
	Unicode*    getName(int* length);
	Unicode*    getValue(int* length);
	void        getBBox(double* llx, double* lly, double* urx, double* ury);
	void        getFont(Ref* fontID, double* fontSize);
	void        getColor(double* red, double* green, double* blue);
	int         getMaxLen();

	Object* getResources(Object* res);

	AcroFormFieldType getAcroFormFieldType() { return type; }

	Object* getFieldRef(Object* ref);
	Object* getValueObj(Object* val);
	Object* getParentRef(Object* parent);

	bool getTypeFromParent() { return typeFromParent; }

private:
	AcroFormField(AcroForm* acroFormA, Object* fieldRefA, Object* fieldObjA, AcroFormFieldType typeA, TextString* nameA, uint32_t flagsA, bool typeFromParentA, XFAField* xfaFieldA);
	Ref         findFontName(const char* fontTag);
	void        draw(int pageNum, Gfx* gfx, bool printing);
	void        drawAnnot(int pageNum, Gfx* gfx, bool printing, Object* annotRef, Object* annotObj);
	void        drawExistingAppearance(Gfx* gfx, Dict* annot, double xMin, double yMin, double xMax, double yMax);
	void        drawNewAppearance(Gfx* gfx, Dict* annot, double xMin, double yMin, double xMax, double yMax);
	void        setColor(Array* a, bool fill, int adjust, std::string& appearBuf);
	void        drawText(const std::string& text, const std::string& da, GfxFontDict* fontDict, bool multiline, int comb, int quadding, int vAlign, bool txField, bool forceZapfDingbats, int rot, double x, double y, double width, double height, double border, bool whiteBackground, std::string& appearBuf);
	void        drawListBox(const VEC_STR& texts, bool* selection, int nOptions, int topIdx, const std::string& da, GfxFontDict* fontDict, int quadding, double xMin, double yMin, double xMax, double yMax, double border, std::string& appearBuf);
	void        getNextLine(const std::string& text, int start, GfxFont* font, double fontSize, double wMax, int* end, double* width, int* next);
	void        drawCircle(double cx, double cy, double r, const char* cmd, std::string& appearBuf);
	void        drawCircleTopLeft(double cx, double cy, double r, std::string& appearBuf);
	void        drawCircleBottomRight(double cx, double cy, double r, std::string& appearBuf);
	void        drawBarcode(const std::string& value, const std::string& da, GfxFontDict* fontDict, int rot, double xMin, double yMin, double xMax, double yMax, XFAFieldBarcodeInfo* barcodeInfo, std::string& appearBuf);
	VEC_STR     tokenize(const std::string& s);
	Object*     getAnnotObj(Object* annotObj);
	Object*     getAnnotResources(Dict* annot, Object* res);
	void        buildDefaultResourceDict(Object* dr);
	Object*     fieldLookup(const char* key, Object* obj);
	Object*     fieldLookup(Dict* dict, const char* key, Object* obj);
	Unicode*    utf8ToUnicode(const std::string& s, int* unicodeLength);
	std::string unicodeToLatin1(Unicode* u, int unicodeLength);
	bool        unicodeStringEqual(Unicode* u, int unicodeLength, const std::string& s);
	bool        unicodeStringEqual(Unicode* u, int unicodeLength, const char* s);
	std::string pictureFormatDateTime(const std::string& value, const std::string& picture);
	std::string pictureFormatNumber(const std::string& value, const std::string& picture);
	std::string pictureFormatText(const std::string& value, const std::string& picture);
	bool        isValidInt(const std::string& s, int start, int len);
	int         convertInt(const std::string& s, int start, int len);

	AcroForm*         acroForm       = nullptr;           //
	Object            fieldRef       = {};                //
	Object            fieldObj       = {};                //
	AcroFormFieldType type           = acroFormFieldText; //
	TextString*       name           = nullptr;           //
	uint32_t          flags          = 0;                 //
	bool              typeFromParent = false;             //
	XFAField*         xfaField       = nullptr;           //

	friend class AcroForm;
};
