//========================================================================
//
// XFAScanner.h
//
// Copyright 2020 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>

class GHash;
class ZxElement;

//------------------------------------------------------------------------

enum XFAFieldLayoutHAlign
{
	xfaFieldLayoutHAlignLeft,
	xfaFieldLayoutHAlignCenter,
	xfaFieldLayoutHAlignRight
};

enum XFAFieldLayoutVAlign
{
	xfaFieldLayoutVAlignTop,
	xfaFieldLayoutVAlignMiddle,
	xfaFieldLayoutVAlignBottom
};

class XFAFieldLayoutInfo
{
public:
	XFAFieldLayoutInfo(XFAFieldLayoutHAlign hAlignA, XFAFieldLayoutVAlign vAlignA);

	XFAFieldLayoutHAlign hAlign = xfaFieldLayoutHAlignLeft; //
	XFAFieldLayoutVAlign vAlign = xfaFieldLayoutVAlignTop;  //
};

//------------------------------------------------------------------------

enum XFAFieldPictureSubtype
{
	xfaFieldPictureDateTime,
	xfaFieldPictureNumeric,
	xfaFieldPictureText
};

class XFAFieldPictureInfo
{
public:
	XFAFieldPictureInfo(XFAFieldPictureSubtype subtypeA, const std::string& formatA);
	~XFAFieldPictureInfo();

	XFAFieldPictureSubtype subtype = xfaFieldPictureDateTime; //
	std::string            format  = "";                      // picture format string
};

//------------------------------------------------------------------------

class XFAFieldBarcodeInfo
{
public:
	XFAFieldBarcodeInfo(const std::string& barcodeTypeA, double wideNarrowRatioA, double moduleWidthA, double moduleHeightA, int dataLengthA, int errorCorrectionLevelA, const std::string& textLocationA);
	~XFAFieldBarcodeInfo();

	std::string barcodeType          = ""; //
	double      wideNarrowRatio      = 0;  //
	double      moduleWidth          = 0;  //
	double      moduleHeight         = 0;  //
	int         dataLength           = 0;  //
	int         errorCorrectionLevel = 0;  //
	std::string textLocation         = ""; //
};

//------------------------------------------------------------------------

class XFAField
{
public:
	XFAField(const std::string& nameA, const std::string& fullNameA, const std::string& valueA, XFAFieldLayoutInfo* layoutInfoA, XFAFieldPictureInfo* pictureInfoA, XFAFieldBarcodeInfo* barcodeInfoA);
	~XFAField();

	// Get the field's value, or nullptr if it doesn't have a value.  Sets
	// *[length] to the length of the Unicode string.
	std::string getValue() { return value; }

	// Return a pointer to the field's picture formatting info object,
	// or nullptr if the field doesn't have picture formatting.
	XFAFieldPictureInfo* getPictureInfo() { return pictureInfo; }

	// Return a pointer to the field's layout info object, or nullptr if
	// the field doesn't have layout info.
	XFAFieldLayoutInfo* getLayoutInfo() { return layoutInfo; }

	// Return a pointer to the field's barcode info object, or nullptr if
	// the field isn't a barcode.
	XFAFieldBarcodeInfo* getBarcodeInfo() { return barcodeInfo; }

private:
	friend class XFAScanner;

	std::string          name        = "";      // UTF-8
	std::string          fullName    = "";      // UTF-8
	std::string          value       = "";      // UTF-8
	XFAFieldLayoutInfo*  layoutInfo  = nullptr; //
	XFAFieldPictureInfo* pictureInfo = nullptr; //
	XFAFieldBarcodeInfo* barcodeInfo = nullptr; //
};

//------------------------------------------------------------------------

class XFAScanner
{
public:
	static XFAScanner* load(Object* xfaObj);

	virtual ~XFAScanner();

	// Find an XFA field matchined the specified AcroForm field name.
	// Returns nullptr if there is no matching field.
	XFAField* findField(const std::string& acroFormFieldName);

private:
	XFAScanner();
	static std::string   readXFAStreams(Object* xfaObj);
	UMAP_STR_STR         scanFormValues(ZxElement* xmlRoot);
	void                 scanFormNode(ZxElement* elem, const std::string& fullName, UMAP_STR_STR& formValues);
	void                 scanNode(ZxElement* elem, const std::string& parentName, const std::string& parentFullName, GHash* nameIdx, GHash* fullNameIdx, const std::string& exclGroupName, ZxElement* xmlRoot, UMAP_STR_STR& formValues);
	void                 scanField(ZxElement* elem, const std::string& name, const std::string& fullName, const std::string& exclGroupName, ZxElement* xmlRoot, UMAP_STR_STR& formValues);
	std::string          getFieldValue(ZxElement* elem, const std::string& name, const std::string& fullName, const std::string& exclGroupName, ZxElement* xmlRoot, UMAP_STR_STR& formValues);
	std::string          getDatasetsValue(const char* partName, ZxElement* elem);
	XFAFieldLayoutInfo*  getFieldLayoutInfo(ZxElement* elem);
	XFAFieldPictureInfo* getFieldPictureInfo(ZxElement* elem);
	XFAFieldBarcodeInfo* getFieldBarcodeInfo(ZxElement* elem);
	double               getMeasurement(const std::string& s);
	std::string          getNodeName(ZxElement* elem);
	std::string          getNodeFullName(ZxElement* elem);
	bool                 nodeIsBindGlobal(ZxElement* elem);
	bool                 nodeIsBindNone(ZxElement* elem);

	// GHash* fields; // [XFAField]
	UMAP<std::string, XFAField> fields = {}; //
};
