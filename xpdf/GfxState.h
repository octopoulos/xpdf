//========================================================================
//
// GfxState.h
//
// Copyright 1996-2016 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "Object.h"
#include "Function.h"

class Array;
class GfxFont;
class PDFRectangle;
class GfxDeviceNColorSpace;
class GfxSeparationColorSpace;
class GfxShading;
class GfxState;

//------------------------------------------------------------------------
// GfxBlendMode
//------------------------------------------------------------------------

enum GfxBlendMode
{
	gfxBlendNormal,
	gfxBlendMultiply,
	gfxBlendScreen,
	gfxBlendOverlay,
	gfxBlendDarken,
	gfxBlendLighten,
	gfxBlendColorDodge,
	gfxBlendColorBurn,
	gfxBlendHardLight,
	gfxBlendSoftLight,
	gfxBlendDifference,
	gfxBlendExclusion,
	gfxBlendHue,
	gfxBlendSaturation,
	gfxBlendColor,
	gfxBlendLuminosity
};

//------------------------------------------------------------------------
// GfxRenderingIntent
//------------------------------------------------------------------------

enum GfxRenderingIntent
{
	gfxRenderingIntentAbsoluteColorimetric,
	gfxRenderingIntentRelativeColorimetric,
	gfxRenderingIntentSaturation,
	gfxRenderingIntentPerceptual
};

#define gfxNumRenderingIntents 4

//------------------------------------------------------------------------
// GfxColorComp
//------------------------------------------------------------------------

// 16.16 fixed point color component
typedef int GfxColorComp;

#define gfxColorComp1 0x10000

static inline GfxColorComp dblToCol(double x)
{
	return (GfxColorComp)(x * gfxColorComp1);
}

static inline double colToDbl(GfxColorComp x)
{
	return (double)x / (double)gfxColorComp1;
}

static inline GfxColorComp byteToCol(uint8_t x)
{
	// (x / 255) << 16  =  (0.0000000100000001... * x) << 16
	//                  =  ((x << 8) + (x) + (x >> 8) + ...)
	//                  =  (x << 8) + (x) + (x >> 7) [for rounding]
	return (GfxColorComp)((x << 8) + x + (x >> 7));
}

static inline GfxColorComp wordToCol(uint16_t x)
{
	// (x / 65535) << 16  =  (0.0000000000000001... * x) << 16
	//                    =  x + (x >> 15) [for rounding]
	return (GfxColorComp)(x + (x >> 15));
}

static inline uint8_t colToByte(GfxColorComp x)
{
	// 255 * x + 0.5  =  256 * x - x + 0.5
	//                =  [256 * (x << 16) - (x << 16) + (1 << 15)] >> 16
	return (uint8_t)(((x << 8) - x + 0x8000) >> 16);
}

static inline uint16_t colToWord(GfxColorComp x)
{
	// 65535 * x + 0.5  =  65536 * x - x + 0.5
	//                  =  [65536 * (x << 16) - (x << 16) + (1 << 15)] >> 16
	return (uint16_t)(((x << 16) - x + 0x8000) >> 16);
}

//------------------------------------------------------------------------
// GfxColor
//------------------------------------------------------------------------

#define gfxColorMaxComps funcMaxOutputs

struct GfxColor
{
	GfxColorComp c[gfxColorMaxComps] = {}; //
};

//------------------------------------------------------------------------
// GfxGray
//------------------------------------------------------------------------

typedef GfxColorComp GfxGray;

//------------------------------------------------------------------------
// GfxRGB
//------------------------------------------------------------------------

struct GfxRGB
{
	GfxColorComp r, g, b;
};

//------------------------------------------------------------------------
// GfxCMYK
//------------------------------------------------------------------------

struct GfxCMYK
{
	GfxColorComp c, m, y, k;
};

//------------------------------------------------------------------------
// GfxColorSpace
//------------------------------------------------------------------------

// NB: The nGfxColorSpaceModes constant and the gfxColorSpaceModeNames
// array defined in GfxState.cc must match this enum.
enum GfxColorSpaceMode
{
	csDeviceGray,
	csCalGray,
	csDeviceRGB,
	csCalRGB,
	csDeviceCMYK,
	csLab,
	csICCBased,
	csIndexed,
	csSeparation,
	csDeviceN,
	csPattern
};

class GfxColorSpace
{
public:
	GfxColorSpace();
	virtual ~GfxColorSpace();
	virtual GfxColorSpace*    copy()    = 0;
	virtual GfxColorSpaceMode getMode() = 0;

	// Construct a color space.
	// Returns nullptr if unsuccessful.
	static GfxColorSpace* parse(Object* csObj, int recursion = 0);

	// Construct a simple color space.
	// The <mode> argument can be csDeviceGray, csDeviceRGB, or csDeviceCMYK.
	static GfxColorSpace* create(GfxColorSpaceMode mode);

	// Convert to gray, RGB, or CMYK.
	virtual void getGray(GfxColor* color, GfxGray* gray, GfxRenderingIntent ri) = 0;
	virtual void getRGB(GfxColor* color, GfxRGB* rgb, GfxRenderingIntent ri)    = 0;
	virtual void getCMYK(GfxColor* color, GfxCMYK* cmyk, GfxRenderingIntent ri) = 0;

	// Return the number of color components.
	virtual int getNComps() = 0;

	// Get this color space's default color.
	virtual void getDefaultColor(GfxColor* color) = 0;

	// Return the default ranges for each component, assuming an image with a max pixel value of <maxImgPixel>.
	virtual void getDefaultRanges(double* decodeLow, double* decodeRange, int maxImgPixel);

	// Returns true if painting operations in this color space never mark the page (e.g., the "None" colorant).
	virtual bool isNonMarking() { return false; }

	// Return the color space's overprint mask.
	uint32_t getOverprintMask() { return overprintMask; }

	// Return true if this color space object is the result of substituting a DefaultGray/RGB/CMYK color space for DeviceGray/RGB/CMYK.
	bool isDefaultColorSpace() { return defaultColorSpace; }

	// Return the number of color space modes
	static int getNumColorSpaceModes();

	// Return the name of the <idx>th color space mode.
	static const char* getColorSpaceModeName(int idx);

protected:
	uint32_t overprintMask     = 0;     //
	bool     defaultColorSpace = false; //
};

//------------------------------------------------------------------------
// GfxDeviceGrayColorSpace
//------------------------------------------------------------------------

class GfxDeviceGrayColorSpace : public GfxColorSpace
{
public:
	GfxDeviceGrayColorSpace();
	virtual ~GfxDeviceGrayColorSpace();
	virtual GfxColorSpace* copy();

	virtual GfxColorSpaceMode getMode() { return csDeviceGray; }

	virtual void getGray(GfxColor* color, GfxGray* gray, GfxRenderingIntent ri);
	virtual void getRGB(GfxColor* color, GfxRGB* rgb, GfxRenderingIntent ri);
	virtual void getCMYK(GfxColor* color, GfxCMYK* cmyk, GfxRenderingIntent ri);

	virtual int getNComps() { return 1; }

	virtual void getDefaultColor(GfxColor* color);

private:
};

//------------------------------------------------------------------------
// GfxCalGrayColorSpace
//------------------------------------------------------------------------

class GfxCalGrayColorSpace : public GfxColorSpace
{
public:
	GfxCalGrayColorSpace();
	virtual ~GfxCalGrayColorSpace();
	virtual GfxColorSpace* copy();

	virtual GfxColorSpaceMode getMode() { return csCalGray; }

	// Construct a CalGray color space.
	// Returns nullptr if unsuccessful.
	static GfxColorSpace* parse(Array* arr, int recursion);

	virtual void getGray(GfxColor* color, GfxGray* gray, GfxRenderingIntent ri);
	virtual void getRGB(GfxColor* color, GfxRGB* rgb, GfxRenderingIntent ri);
	virtual void getCMYK(GfxColor* color, GfxCMYK* cmyk, GfxRenderingIntent ri);

	virtual int getNComps() { return 1; }

	virtual void getDefaultColor(GfxColor* color);

	// CalGray-specific access.
	double getWhiteX() { return whiteX; }

	double getWhiteY() { return whiteY; }

	double getWhiteZ() { return whiteZ; }

	double getBlackX() { return blackX; }

	double getBlackY() { return blackY; }

	double getBlackZ() { return blackZ; }

	double getGamma() { return gamma; }

private:
	double whiteX = 0;
	double whiteY = 0;
	double whiteZ = 0; // white point
	double blackX = 0;
	double blackY = 0;
	double blackZ = 0; // black point
	double gamma  = 0; // gamma value
};

//------------------------------------------------------------------------
// GfxDeviceRGBColorSpace
//------------------------------------------------------------------------

class GfxDeviceRGBColorSpace : public GfxColorSpace
{
public:
	GfxDeviceRGBColorSpace();
	virtual ~GfxDeviceRGBColorSpace();
	virtual GfxColorSpace* copy();

	virtual GfxColorSpaceMode getMode() { return csDeviceRGB; }

	virtual void getGray(GfxColor* color, GfxGray* gray, GfxRenderingIntent ri);
	virtual void getRGB(GfxColor* color, GfxRGB* rgb, GfxRenderingIntent ri);
	virtual void getCMYK(GfxColor* color, GfxCMYK* cmyk, GfxRenderingIntent ri);

	virtual int getNComps() { return 3; }

	virtual void getDefaultColor(GfxColor* color);

private:
};

//------------------------------------------------------------------------
// GfxCalRGBColorSpace
//------------------------------------------------------------------------

class GfxCalRGBColorSpace : public GfxColorSpace
{
public:
	GfxCalRGBColorSpace();
	virtual ~GfxCalRGBColorSpace();
	virtual GfxColorSpace* copy();

	virtual GfxColorSpaceMode getMode() { return csCalRGB; }

	// Construct a CalRGB color space.
	// Returns nullptr if unsuccessful.
	static GfxColorSpace* parse(Array* arr, int recursion);

	virtual void getGray(GfxColor* color, GfxGray* gray, GfxRenderingIntent ri);
	virtual void getRGB(GfxColor* color, GfxRGB* rgb, GfxRenderingIntent ri);
	virtual void getCMYK(GfxColor* color, GfxCMYK* cmyk, GfxRenderingIntent ri);

	virtual int getNComps() { return 3; }

	virtual void getDefaultColor(GfxColor* color);

	// CalRGB-specific access.
	double getWhiteX() { return whiteX; }

	double getWhiteY() { return whiteY; }

	double getWhiteZ() { return whiteZ; }

	double getBlackX() { return blackX; }

	double getBlackY() { return blackY; }

	double getBlackZ() { return blackZ; }

	double getGammaR() { return gammaR; }

	double getGammaG() { return gammaG; }

	double getGammaB() { return gammaB; }

	double* getMatrix() { return mat; }

private:
	double whiteX = 0;  //
	double whiteY = 0;  //
	double whiteZ = 0;  // white point
	double blackX = 0;  //
	double blackY = 0;  //
	double blackZ = 0;  // black point
	double gammaR = 0;  //
	double gammaG = 0;  //
	double gammaB = 0;  // gamma values
	double mat[9] = {}; // ABC -> XYZ transform matrix
};

//------------------------------------------------------------------------
// GfxDeviceCMYKColorSpace
//------------------------------------------------------------------------

class GfxDeviceCMYKColorSpace : public GfxColorSpace
{
public:
	GfxDeviceCMYKColorSpace();
	virtual ~GfxDeviceCMYKColorSpace();
	virtual GfxColorSpace* copy();

	virtual GfxColorSpaceMode getMode() { return csDeviceCMYK; }

	virtual void getGray(GfxColor* color, GfxGray* gray, GfxRenderingIntent ri);
	virtual void getRGB(GfxColor* color, GfxRGB* rgb, GfxRenderingIntent ri);
	virtual void getCMYK(GfxColor* color, GfxCMYK* cmyk, GfxRenderingIntent ri);

	virtual int getNComps() { return 4; }

	virtual void getDefaultColor(GfxColor* color);

private:
};

//------------------------------------------------------------------------
// GfxLabColorSpace
//------------------------------------------------------------------------

class GfxLabColorSpace : public GfxColorSpace
{
public:
	GfxLabColorSpace();
	virtual ~GfxLabColorSpace();
	virtual GfxColorSpace* copy();

	virtual GfxColorSpaceMode getMode() { return csLab; }

	// Construct a Lab color space.  Returns nullptr if unsuccessful.
	static GfxColorSpace* parse(Array* arr, int recursion);

	virtual void getGray(GfxColor* color, GfxGray* gray, GfxRenderingIntent ri);
	virtual void getRGB(GfxColor* color, GfxRGB* rgb, GfxRenderingIntent ri);
	virtual void getCMYK(GfxColor* color, GfxCMYK* cmyk, GfxRenderingIntent ri);

	virtual int getNComps() { return 3; }

	virtual void getDefaultColor(GfxColor* color);

	virtual void getDefaultRanges(double* decodeLow, double* decodeRange, int maxImgPixel);

	// Lab-specific access.
	double getWhiteX() { return whiteX; }

	double getWhiteY() { return whiteY; }

	double getWhiteZ() { return whiteZ; }

	double getBlackX() { return blackX; }

	double getBlackY() { return blackY; }

	double getBlackZ() { return blackZ; }

	double getAMin() { return aMin; }

	double getAMax() { return aMax; }

	double getBMin() { return bMin; }

	double getBMax() { return bMax; }

private:
	double whiteX = 0; //
	double whiteY = 0; //
	double whiteZ = 0; // white point
	double blackX = 0; //
	double blackY = 0; //
	double blackZ = 0; // black point
	double aMin   = 0; //
	double aMax   = 0; //
	double bMin   = 0; //
	double bMax   = 0; // range for the a and b components
	double kr     = 0; //
	double kg     = 0; //
	double kb     = 0; // gamut mapping mulitpliers
};

//------------------------------------------------------------------------
// GfxICCBasedColorSpace
//------------------------------------------------------------------------

class GfxICCBasedColorSpace : public GfxColorSpace
{
public:
	GfxICCBasedColorSpace(int nCompsA, GfxColorSpace* altA, Ref* iccProfileStreamA);
	virtual ~GfxICCBasedColorSpace();
	virtual GfxColorSpace* copy();

	virtual GfxColorSpaceMode getMode() { return csICCBased; }

	// Construct an ICCBased color space.  Returns nullptr if unsuccessful.
	static GfxColorSpace* parse(Array* arr, int recursion);

	virtual void getGray(GfxColor* color, GfxGray* gray, GfxRenderingIntent ri);
	virtual void getRGB(GfxColor* color, GfxRGB* rgb, GfxRenderingIntent ri);
	virtual void getCMYK(GfxColor* color, GfxCMYK* cmyk, GfxRenderingIntent ri);

	virtual int getNComps() { return nComps; }

	virtual void getDefaultColor(GfxColor* color);

	virtual void getDefaultRanges(double* decodeLow, double* decodeRange, int maxImgPixel);

	// ICCBased-specific access.
	GfxColorSpace* getAlt() { return alt; }

	Ref getICCProfileStreamRef() { return iccProfileStream; }

private:
	int            nComps           = 0;       // number of color components (1, 3, or 4)
	GfxColorSpace* alt              = nullptr; // alternate color space
	double         rangeMin[4]      = {};      // min values for each component
	double         rangeMax[4]      = {};      // max values for each component
	Ref            iccProfileStream = {};      // the ICC profile
};

//------------------------------------------------------------------------
// GfxIndexedColorSpace
//------------------------------------------------------------------------

class GfxIndexedColorSpace : public GfxColorSpace
{
public:
	GfxIndexedColorSpace(GfxColorSpace* baseA, int indexHighA);
	virtual ~GfxIndexedColorSpace();
	virtual GfxColorSpace* copy();

	virtual GfxColorSpaceMode getMode() { return csIndexed; }

	// Construct an Indexed color space.  Returns nullptr if unsuccessful.
	static GfxColorSpace* parse(Array* arr, int recursion);

	virtual void getGray(GfxColor* color, GfxGray* gray, GfxRenderingIntent ri);
	virtual void getRGB(GfxColor* color, GfxRGB* rgb, GfxRenderingIntent ri);
	virtual void getCMYK(GfxColor* color, GfxCMYK* cmyk, GfxRenderingIntent ri);

	virtual int getNComps() { return 1; }

	virtual void getDefaultColor(GfxColor* color);

	virtual void getDefaultRanges(double* decodeLow, double* decodeRange, int maxImgPixel);

	// Indexed-specific access.
	GfxColorSpace* getBase() { return base; }

	int getIndexHigh() { return indexHigh; }

	uint8_t* getLookup() { return lookup; }

	GfxColor* mapColorToBase(GfxColor* color, GfxColor* baseColor);

private:
	GfxColorSpace* base      = nullptr; // base color space
	int            indexHigh = 0;       // max pixel value
	uint8_t*       lookup    = nullptr; // lookup table
};

//------------------------------------------------------------------------
// GfxSeparationColorSpace
//------------------------------------------------------------------------

class GfxSeparationColorSpace : public GfxColorSpace
{
public:
	GfxSeparationColorSpace(const std::string& nameA, GfxColorSpace* altA, Function* funcA);
	virtual ~GfxSeparationColorSpace();
	virtual GfxColorSpace* copy();

	virtual GfxColorSpaceMode getMode() { return csSeparation; }

	// Construct a Separation color space.  Returns nullptr if unsuccessful.
	static GfxColorSpace* parse(Array* arr, int recursion);

	virtual void getGray(GfxColor* color, GfxGray* gray, GfxRenderingIntent ri);
	virtual void getRGB(GfxColor* color, GfxRGB* rgb, GfxRenderingIntent ri);
	virtual void getCMYK(GfxColor* color, GfxCMYK* cmyk, GfxRenderingIntent ri);

	virtual int getNComps() { return 1; }

	virtual void getDefaultColor(GfxColor* color);

	virtual bool isNonMarking() { return nonMarking; }

	// Separation-specific access.
	std::string_view getName() { return name; }

	GfxColorSpace* getAlt() { return alt; }

	Function* getFunc() { return func; }

private:
	GfxSeparationColorSpace(const std::string& nameA, GfxColorSpace* altA, Function* funcA, bool nonMarkingA, uint32_t overprintMaskA);

	std::string    name       = "";      // colorant name
	GfxColorSpace* alt        = nullptr; // alternate color space
	Function*      func       = nullptr; // tint transform (into alternate color space)
	bool           nonMarking = false;   //
};

//------------------------------------------------------------------------
// GfxDeviceNColorSpace
//------------------------------------------------------------------------

class GfxDeviceNColorSpace : public GfxColorSpace
{
public:
	GfxDeviceNColorSpace(int nCompsA, std::string* namesA, GfxColorSpace* alt, Function* func, Object* attrsA);
	virtual ~GfxDeviceNColorSpace();
	virtual GfxColorSpace* copy();

	virtual GfxColorSpaceMode getMode() { return csDeviceN; }

	// Construct a DeviceN color space.  Returns nullptr if unsuccessful.
	static GfxColorSpace* parse(Array* arr, int recursion);

	virtual void getGray(GfxColor* color, GfxGray* gray, GfxRenderingIntent ri);
	virtual void getRGB(GfxColor* color, GfxRGB* rgb, GfxRenderingIntent ri);
	virtual void getCMYK(GfxColor* color, GfxCMYK* cmyk, GfxRenderingIntent ri);

	virtual int getNComps() { return nComps; }

	virtual void getDefaultColor(GfxColor* color);

	virtual bool isNonMarking() { return nonMarking; }

	// DeviceN-specific access.
	std::string getColorantName(int i) { return names[i]; }

	GfxColorSpace* getAlt() { return alt; }

	Function* getTintTransformFunc() { return func; }

	Object* getAttrs() { return &attrs; }

private:
	GfxDeviceNColorSpace(int nCompsA, std::string* namesA, GfxColorSpace* alt, Function* func, Object* attrsA, bool nonMarkingA, uint32_t overprintMaskA);

	int            nComps                  = 0;       // number of components
	std::string    names[gfxColorMaxComps] = {};      // colorant names
	GfxColorSpace* alt                     = nullptr; // alternate color space
	Function*      func                    = nullptr; // tint transform (into alternate color space)
	Object         attrs                   = {};      //
	bool           nonMarking              = false;   //
};

//------------------------------------------------------------------------
// GfxPatternColorSpace
//------------------------------------------------------------------------

class GfxPatternColorSpace : public GfxColorSpace
{
public:
	GfxPatternColorSpace(GfxColorSpace* underA);
	virtual ~GfxPatternColorSpace();
	virtual GfxColorSpace* copy();

	virtual GfxColorSpaceMode getMode() { return csPattern; }

	// Construct a Pattern color space.  Returns nullptr if unsuccessful.
	static GfxColorSpace* parse(Array* arr, int recursion);

	virtual void getGray(GfxColor* color, GfxGray* gray, GfxRenderingIntent ri);
	virtual void getRGB(GfxColor* color, GfxRGB* rgb, GfxRenderingIntent ri);
	virtual void getCMYK(GfxColor* color, GfxCMYK* cmyk, GfxRenderingIntent ri);

	virtual int getNComps() { return 0; }

	virtual void getDefaultColor(GfxColor* color);

	// Pattern-specific access.
	GfxColorSpace* getUnder() { return under; }

private:
	GfxColorSpace* under = nullptr; // underlying color space (for uncolored patterns)
};

//------------------------------------------------------------------------
// GfxPattern
//------------------------------------------------------------------------

class GfxPattern
{
public:
	GfxPattern(int typeA);
	virtual ~GfxPattern();

	static GfxPattern* parse(Object* objRef, Object* obj);

	virtual GfxPattern* copy() = 0;

	int getType() { return type; }

private:
	int type = 0;
};

//------------------------------------------------------------------------
// GfxTilingPattern
//------------------------------------------------------------------------

class GfxTilingPattern : public GfxPattern
{
public:
	static GfxTilingPattern* parse(Object* patObjRef, Object* patObj);
	virtual ~GfxTilingPattern();

	virtual GfxPattern* copy();

	int getPaintType() { return paintType; }

	int getTilingType() { return tilingType; }

	double* getBBox() { return bbox; }

	double getXStep() { return xStep; }

	double getYStep() { return yStep; }

	Dict* getResDict()
	{
		return resDict.isDict() ? resDict.getDict() : (Dict*)nullptr;
	}

	double* getMatrix() { return matrix; }

	Object* getContentStreamRef() { return &contentStreamRef; }

	bool usesBlendMode(XRef* xref);

private:
	GfxTilingPattern(int paintTypeA, int tilingTypeA, double* bboxA, double xStepA, double yStepA, Object* resDictA, double* matrixA, Object* contentStreamA);
	bool scanResourcesForBlendMode(Object* resDict2, char* scannedObjs, XRef* xref);
	bool scanExtGStateForBlendMode(Object* gsObj, char* scannedObjs, XRef* xref);
	bool scanSoftMaskForBlendMode(Object* softMaskObj, char* scannedObjs, XRef* xref);
	bool scanPatternForBlendMode(Object* patternObj, char* scannedObjs, XRef* xref);
	bool scanXObjectForBlendMode(Object* xObj, char* scannedObjs, XRef* xref);

	int    paintType        = 0;  //
	int    tilingType       = 0;  //
	double bbox[4]          = {}; //
	double xStep            = 0;  //
	double yStep            = 0;  //
	Object resDict          = {}; //
	double matrix[6]        = {}; //
	Object contentStreamRef = {}; //
};

//------------------------------------------------------------------------
// GfxShadingPattern
//------------------------------------------------------------------------

class GfxShadingPattern : public GfxPattern
{
public:
	static GfxShadingPattern* parse(Object* patObj);
	virtual ~GfxShadingPattern();

	virtual GfxPattern* copy();

	GfxShading* getShading() { return shading; }

	double* getMatrix() { return matrix; }

private:
	GfxShadingPattern(GfxShading* shadingA, double* matrixA);

	GfxShading* shading   = nullptr;
	double      matrix[6] = {};
};

//------------------------------------------------------------------------
// GfxShading
//------------------------------------------------------------------------

class GfxShading
{
public:
	GfxShading(int typeA);
	GfxShading(GfxShading* shading);
	virtual ~GfxShading();

	static GfxShading* parse(Object* obj);

	virtual GfxShading* copy() = 0;

	int getType() { return type; }

	GfxColorSpace* getColorSpace() { return colorSpace; }

	GfxColor* getBackground() { return &background; }

	bool getHasBackground() { return hasBackground; }

	void getBBox(double* xMinA, double* yMinA, double* xMaxA, double* yMaxA)
	{
		*xMinA = xMin;
		*yMinA = yMin;
		*xMaxA = xMax;
		*yMaxA = yMax;
	}

	bool getHasBBox() { return hasBBox; }

protected:
	bool init(Dict* dict);

	int            type          = 0;       //
	GfxColorSpace* colorSpace    = nullptr; //
	GfxColor       background    = {};      //
	bool           hasBackground = false;   //
	double         xMin          = 0;       //
	double         yMin          = 0;       //
	double         xMax          = 0;       //
	double         yMax          = 0;       //
	bool           hasBBox       = false;   //
};

//------------------------------------------------------------------------
// GfxFunctionShading
//------------------------------------------------------------------------

class GfxFunctionShading : public GfxShading
{
public:
	GfxFunctionShading(double x0A, double y0A, double x1A, double y1A, double* matrixA, Function** funcsA, int nFuncsA);
	GfxFunctionShading(GfxFunctionShading* shading);
	virtual ~GfxFunctionShading();

	static GfxFunctionShading* parse(Dict* dict);

	virtual GfxShading* copy();

	void getDomain(double* x0A, double* y0A, double* x1A, double* y1A)
	{
		*x0A = x0;
		*y0A = y0;
		*x1A = x1;
		*y1A = y1;
	}

	double* getMatrix() { return matrix; }

	int getNFuncs() { return nFuncs; }

	Function* getFunc(int i) { return funcs[i]; }

	void getColor(double x, double y, GfxColor* color);

private:
	double    x0                      = 0;  //
	double    y0                      = 0;  //
	double    x1                      = 0;  //
	double    y1                      = 0;  //
	double    matrix[6]               = {}; //
	Function* funcs[gfxColorMaxComps] = {}; //
	int       nFuncs                  = 0;  //
};

//------------------------------------------------------------------------
// GfxAxialShading
//------------------------------------------------------------------------

class GfxAxialShading : public GfxShading
{
public:
	GfxAxialShading(double x0A, double y0A, double x1A, double y1A, double t0A, double t1A, Function** funcsA, int nFuncsA, bool extend0A, bool extend1A);
	GfxAxialShading(GfxAxialShading* shading);
	virtual ~GfxAxialShading();

	static GfxAxialShading* parse(Dict* dict);

	virtual GfxShading* copy();

	void getCoords(double* x0A, double* y0A, double* x1A, double* y1A)
	{
		*x0A = x0;
		*y0A = y0;
		*x1A = x1;
		*y1A = y1;
	}

	double getDomain0() { return t0; }

	double getDomain1() { return t1; }

	bool getExtend0() { return extend0; }

	bool getExtend1() { return extend1; }

	int getNFuncs() { return nFuncs; }

	Function* getFunc(int i) { return funcs[i]; }

	void getColor(double t, GfxColor* color);

private:
	double    x0                      = 0;     //
	double    y0                      = 0;     //
	double    x1                      = 0;     //
	double    y1                      = 0;     //
	double    t0                      = 0;     //
	double    t1                      = 0;     //
	Function* funcs[gfxColorMaxComps] = {};    //
	int       nFuncs                  = 0;     //
	bool      extend0                 = false; //
	bool      extend1                 = false; //
};

//------------------------------------------------------------------------
// GfxRadialShading
//------------------------------------------------------------------------

class GfxRadialShading : public GfxShading
{
public:
	GfxRadialShading(double x0A, double y0A, double r0A, double x1A, double y1A, double r1A, double t0A, double t1A, Function** funcsA, int nFuncsA, bool extend0A, bool extend1A);
	GfxRadialShading(GfxRadialShading* shading);
	virtual ~GfxRadialShading();

	static GfxRadialShading* parse(Dict* dict);

	virtual GfxShading* copy();

	void getCoords(double* x0A, double* y0A, double* r0A, double* x1A, double* y1A, double* r1A)
	{
		*x0A = x0;
		*y0A = y0;
		*r0A = r0;
		*x1A = x1;
		*y1A = y1;
		*r1A = r1;
	}

	double getDomain0() { return t0; }

	double getDomain1() { return t1; }

	bool getExtend0() { return extend0; }

	bool getExtend1() { return extend1; }

	int getNFuncs() { return nFuncs; }

	Function* getFunc(int i) { return funcs[i]; }

	void getColor(double t, GfxColor* color);

private:
	double    x0                      = 0;     //
	double    y0                      = 0;     //
	double    r0                      = 0;     //
	double    x1                      = 0;     //
	double    y1                      = 0;     //
	double    r1                      = 0;     //
	double    t0                      = 0;     //
	double    t1                      = 0;     //
	Function* funcs[gfxColorMaxComps] = {};    //
	int       nFuncs                  = 0;     //
	bool      extend0                 = false; //
	bool      extend1                 = false; //
};

//------------------------------------------------------------------------
// GfxGouraudTriangleShading
//------------------------------------------------------------------------

struct GfxGouraudVertex
{
	double x                       = 0;  //
	double y                       = 0;  //
	double color[gfxColorMaxComps] = {}; //
};

class GfxGouraudTriangleShading : public GfxShading
{
public:
	GfxGouraudTriangleShading(int typeA, GfxGouraudVertex* verticesA, int nVerticesA, int (*trianglesA)[3], int nTrianglesA, int nCompsA, Function** funcsA, int nFuncsA);
	GfxGouraudTriangleShading(GfxGouraudTriangleShading* shading);
	virtual ~GfxGouraudTriangleShading();

	static GfxGouraudTriangleShading* parse(int typeA, Dict* dict, Stream* str);

	virtual GfxShading* copy();

	int getNComps() { return nComps; }

	int getNTriangles() { return nTriangles; }

	void getTriangle(int i, double* x0, double* y0, double* color0, double* x1, double* y1, double* color1, double* x2, double* y2, double* color2);
	void getBBox(double* xMinA, double* yMinA, double* xMaxA, double* yMaxA);
	void getColor(double* in, GfxColor* out);

private:
	GfxGouraudVertex* vertices        = nullptr; //
	int               nVertices       = 0;       //
	int (*triangles)[3]               = {};      //
	int       nTriangles              = 0;       //
	Function* funcs[gfxColorMaxComps] = {};      //
	int       nComps                  = 0;       // number of color components (1 if nFuncs > 0)
	int       nFuncs                  = 0;       //
};

//------------------------------------------------------------------------
// GfxPatchMeshShading
//------------------------------------------------------------------------

struct GfxPatch
{
	double x[4][4]                       = {}; //
	double y[4][4]                       = {}; //
	double color[2][2][gfxColorMaxComps] = {}; //
};

class GfxPatchMeshShading : public GfxShading
{
public:
	GfxPatchMeshShading(int typeA, GfxPatch* patchesA, int nPatchesA, int nCompsA, Function** funcsA, int nFuncsA);
	GfxPatchMeshShading(GfxPatchMeshShading* shading);
	virtual ~GfxPatchMeshShading();

	static GfxPatchMeshShading* parse(int typeA, Dict* dict, Stream* str);

	virtual GfxShading* copy();

	int getNComps() { return nComps; }

	int getNPatches() { return nPatches; }

	GfxPatch* getPatch(int i) { return &patches[i]; }

	void getBBox(double* xMinA, double* yMinA, double* xMaxA, double* yMaxA);
	void getColor(double* in, GfxColor* out);

private:
	GfxPatch* patches                 = nullptr; //
	int       nPatches                = 0;       //
	Function* funcs[gfxColorMaxComps] = {};      //
	int       nComps                  = 0;       // number of color components (1 if nFuncs > 0)
	int       nFuncs                  = 0;       //
};

//------------------------------------------------------------------------
// GfxImageColorMap
//------------------------------------------------------------------------

class GfxImageColorMap
{
public:
	// Constructor.
	GfxImageColorMap(int bitsA, Object* decode, GfxColorSpace* colorSpaceA, int maxAllowedBits = 8);

	// Destructor.
	~GfxImageColorMap();

	// Return a copy of this color map.
	GfxImageColorMap* copy() { return new GfxImageColorMap(this); }

	// Is color map valid?
	bool isOk() { return ok; }

	// Get the color space.
	GfxColorSpace* getColorSpace() { return colorSpace; }

	// Get stream decoding info.
	int getNumPixelComps() { return nComps; }

	int getBits() { return bits; }

	// Get decode table.
	double getDecodeLow(int i) { return decodeLow[i]; }

	double getDecodeHigh(int i) { return decodeLow[i] + decodeRange[i]; }

	// Convert an image pixel to a color.
	void getGray(uint8_t* x, GfxGray* gray, GfxRenderingIntent ri);
	void getRGB(uint8_t* x, GfxRGB* rgb, GfxRenderingIntent ri);
	void getCMYK(uint8_t* x, GfxCMYK* cmyk, GfxRenderingIntent ri);
	void getColor(uint8_t* x, GfxColor* color);

	// Convert a line of <n> pixels to 8-bit colors.
	void getGrayByteLine(uint8_t* in, uint8_t* out, int n, GfxRenderingIntent ri);
	void getRGBByteLine(uint8_t* in, uint8_t* out, int n, GfxRenderingIntent ri);
	void getCMYKByteLine(uint8_t* in, uint8_t* out, int n, GfxRenderingIntent ri);

private:
	GfxImageColorMap(GfxImageColorMap* colorMap);

	GfxColorSpace* colorSpace                    = nullptr; // the image color space
	int            bits                          = 0;       // bits per component
	int            nComps                        = 0;       // number of components in a pixel
	GfxColorSpace* colorSpace2                   = nullptr; // secondary color space
	int            nComps2                       = 0;       // number of components in colorSpace2
	GfxColorComp*  lookup[gfxColorMaxComps]      = {};      // lookup table
	GfxColorComp*  lookup2[gfxColorMaxComps]     = {};      // optimized case lookup table
	double         decodeLow[gfxColorMaxComps]   = {};      // minimum values for each component
	double         decodeRange[gfxColorMaxComps] = {};      // max - min value for each component
	bool           ok                            = false;   //
};

//------------------------------------------------------------------------
// GfxSubpath and GfxPath
//------------------------------------------------------------------------

class GfxSubpath
{
public:
	// Constructor.
	GfxSubpath(double x1, double y1);

	// Destructor.
	~GfxSubpath();

	// Copy.
	GfxSubpath* copy() { return new GfxSubpath(this); }

	// Get points.
	int getNumPoints() { return n; }

	double getX(int i) { return x[i]; }

	double getY(int i) { return y[i]; }

	bool getCurve(int i) { return curve[i]; }

	// Get last point.
	double getLastX() { return x[n - 1]; }

	double getLastY() { return y[n - 1]; }

	// Add a line segment.
	void lineTo(double x1, double y1);

	// Add a Bezier curve.
	void curveTo(double x1, double y1, double x2, double y2, double x3, double y3);

	// Close the subpath.
	void close();

	bool isClosed() { return closed; }

	// Add (<dx>, <dy>) to each point in the subpath.
	void offset(double dx, double dy);

private:
	double* x      = nullptr; //
	double* y      = nullptr; // points
	bool*   curve  = nullptr; // curve[i] => point i is a control point for a Bezier curve
	int     n      = 0;       // number of points
	int     size   = 0;       // size of x/y arrays
	bool    closed = false;   // set if path is closed

	GfxSubpath(GfxSubpath* subpath);
};

class GfxPath
{
public:
	// Constructor.
	GfxPath();

	// Destructor.
	~GfxPath();

	// Copy.
	GfxPath* copy()
	{
		return new GfxPath(justMoved, firstX, firstY, subpaths, n, size);
	}

	// Is there a current point?
	bool isCurPt() { return n > 0 || justMoved; }

	// Is the path non-empty, i.e., is there at least one segment?
	bool isPath() { return n > 0; }

	// Get subpaths.
	int getNumSubpaths() { return n; }

	GfxSubpath* getSubpath(int i) { return subpaths[i]; }

	// Get last point on last subpath.
	double getLastX() { return subpaths[n - 1]->getLastX(); }

	double getLastY() { return subpaths[n - 1]->getLastY(); }

	// Get the current point
	double getCurX();
	double getCurY();

	// Move the current point.
	void moveTo(double x, double y);

	// Add a segment to the last subpath.
	void lineTo(double x, double y);

	// Add a Bezier curve to the last subpath
	void curveTo(double x1, double y1, double x2, double y2, double x3, double y3);

	// Close the last subpath.
	void close();

	// Append <path> to <this>.
	void append(GfxPath* path);

	// Add (<dx>, <dy>) to each point in the path.
	void offset(double dx, double dy);

private:
	bool         justMoved = false;   // set if a new subpath was just started
	double       firstX    = 0;       //
	double       firstY    = 0;       // first point in new subpath
	GfxSubpath** subpaths  = nullptr; // subpaths
	int          n         = 0;       // number of subpaths
	int          size      = 0;       // size of subpaths array

	GfxPath(bool justMoved1, double firstX1, double firstY1, GfxSubpath** subpaths1, int n1, int size1);
};

//------------------------------------------------------------------------
// GfxState
//------------------------------------------------------------------------

class GfxState
{
public:
	// Construct a default GfxState, for a device with resolution <hDPI>
	// x <vDPI>, page box <pageBox>, page rotation <rotateA>, and
	// coordinate system specified by <upsideDown>.
	GfxState(double hDPIA, double vDPIA, PDFRectangle* pageBox, int rotateA, bool upsideDown);

	// Destructor.
	~GfxState();

	// Copy.
	GfxState* copy(bool copyPath = false)
	{
		return new GfxState(this, copyPath);
	}

	// Accessors.
	double getHDPI() { return hDPI; }

	double getVDPI() { return vDPI; }

	double* getCTM() { return ctm; }

	double getX1() { return px1; }

	double getY1() { return py1; }

	double getX2() { return px2; }

	double getY2() { return py2; }

	double getPageWidth() { return pageWidth; }

	double getPageHeight() { return pageHeight; }

	int getRotate() { return rotate; }

	GfxColor* getFillColor() { return &fillColor; }

	GfxColor* getStrokeColor() { return &strokeColor; }

	void getFillGray(GfxGray* gray)
	{
		fillColorSpace->getGray(&fillColor, gray, renderingIntent);
	}

	void getStrokeGray(GfxGray* gray)
	{
		strokeColorSpace->getGray(&strokeColor, gray, renderingIntent);
	}

	void getFillRGB(GfxRGB* rgb)
	{
		fillColorSpace->getRGB(&fillColor, rgb, renderingIntent);
	}

	void getStrokeRGB(GfxRGB* rgb)
	{
		strokeColorSpace->getRGB(&strokeColor, rgb, renderingIntent);
	}

	void getFillCMYK(GfxCMYK* cmyk)
	{
		fillColorSpace->getCMYK(&fillColor, cmyk, renderingIntent);
	}

	void getStrokeCMYK(GfxCMYK* cmyk)
	{
		strokeColorSpace->getCMYK(&strokeColor, cmyk, renderingIntent);
	}

	GfxColorSpace* getFillColorSpace() { return fillColorSpace; }

	GfxColorSpace* getStrokeColorSpace() { return strokeColorSpace; }

	GfxPattern* getFillPattern() { return fillPattern; }

	GfxPattern* getStrokePattern() { return strokePattern; }

	GfxBlendMode getBlendMode() { return blendMode; }

	double getFillOpacity() { return fillOpacity; }

	double getStrokeOpacity() { return strokeOpacity; }

	bool getFillOverprint() { return fillOverprint; }

	bool getStrokeOverprint() { return strokeOverprint; }

	int getOverprintMode() { return overprintMode; }

	GfxRenderingIntent getRenderingIntent() { return renderingIntent; }

	Function** getTransfer() { return transfer; }

	double getLineWidth() { return lineWidth; }

	void getLineDash(double** dash, int* length, double* start)
	{
		*dash   = lineDash;
		*length = lineDashLength;
		*start  = lineDashStart;
	}

	double getFlatness() { return flatness; }

	int getLineJoin() { return lineJoin; }

	int getLineCap() { return lineCap; }

	double getMiterLimit() { return miterLimit; }

	bool getStrokeAdjust() { return strokeAdjust; }

	GfxFont* getFont() { return font; }

	double getFontSize() { return fontSize; }

	double* getTextMat() { return textMat; }

	double getCharSpace() { return charSpace; }

	double getWordSpace() { return wordSpace; }

	double getHorizScaling() { return horizScaling; }

	double getLeading() { return leading; }

	double getRise() { return rise; }

	int getRender() { return render; }

	GfxPath* getPath() { return path; }

	void setPath(GfxPath* pathA);

	double getCurX() { return curX; }

	double getCurY() { return curY; }

	void getClipBBox(double* xMin, double* yMin, double* xMax, double* yMax)
	{
		*xMin = clipXMin;
		*yMin = clipYMin;
		*xMax = clipXMax;
		*yMax = clipYMax;
	}

	void getUserClipBBox(double* xMin, double* yMin, double* xMax, double* yMax);

	double getLineX() { return lineX; }

	double getLineY() { return lineY; }

	bool getIgnoreColorOps() { return ignoreColorOps; }

	// Is there a current point/path?
	bool isCurPt() { return path->isCurPt(); }

	bool isPath() { return path->isPath(); }

	// Transforms.
	void transform(double x1, double y1, double* x2, double* y2)
	{
		*x2 = ctm[0] * x1 + ctm[2] * y1 + ctm[4];
		*y2 = ctm[1] * x1 + ctm[3] * y1 + ctm[5];
	}

	void transformDelta(double x1, double y1, double* x2, double* y2)
	{
		*x2 = ctm[0] * x1 + ctm[2] * y1;
		*y2 = ctm[1] * x1 + ctm[3] * y1;
	}

	void textTransform(double x1, double y1, double* x2, double* y2)
	{
		*x2 = textMat[0] * x1 + textMat[2] * y1 + textMat[4];
		*y2 = textMat[1] * x1 + textMat[3] * y1 + textMat[5];
	}

	void textTransformDelta(double x1, double y1, double* x2, double* y2)
	{
		*x2 = textMat[0] * x1 + textMat[2] * y1;
		*y2 = textMat[1] * x1 + textMat[3] * y1;
	}

	double transformWidth(double w);

	double getTransformedLineWidth()
	{
		return transformWidth(lineWidth);
	}

	double getTransformedFontSize();
	void   getFontTransMat(double* m11, double* m12, double* m21, double* m22);

	// Change state parameters.
	void setCTM(double a, double b, double c, double d, double e, double f);
	void concatCTM(double a, double b, double c, double d, double e, double f);
	void shiftCTM(double tx, double ty);
	void setFillColorSpace(GfxColorSpace* colorSpace);
	void setStrokeColorSpace(GfxColorSpace* colorSpace);

	void setFillColor(GfxColor* color) { fillColor = *color; }

	void setStrokeColor(GfxColor* color) { strokeColor = *color; }

	void setFillPattern(GfxPattern* pattern);
	void setStrokePattern(GfxPattern* pattern);

	void setBlendMode(GfxBlendMode mode) { blendMode = mode; }

	void setFillOpacity(double opac) { fillOpacity = opac; }

	void setStrokeOpacity(double opac) { strokeOpacity = opac; }

	void setFillOverprint(bool op) { fillOverprint = op; }

	void setStrokeOverprint(bool op) { strokeOverprint = op; }

	void setOverprintMode(int opm) { overprintMode = opm; }

	void setRenderingIntent(GfxRenderingIntent ri) { renderingIntent = ri; }

	void setTransfer(Function** funcs);

	void setLineWidth(double width) { lineWidth = width; }

	void setLineDash(double* dash, int length, double start);

	void setFlatness(double flatness1) { flatness = flatness1; }

	void setLineJoin(int lineJoin1) { lineJoin = lineJoin1; }

	void setLineCap(int lineCap1) { lineCap = lineCap1; }

	void setMiterLimit(double limit) { miterLimit = limit; }

	void setStrokeAdjust(bool sa) { strokeAdjust = sa; }

	void setFont(GfxFont* fontA, double fontSizeA)
	{
		font     = fontA;
		fontSize = fontSizeA;
	}

	void setTextMat(double a, double b, double c, double d, double e, double f)
	{
		textMat[0] = a;
		textMat[1] = b;
		textMat[2] = c;
		textMat[3] = d;
		textMat[4] = e;
		textMat[5] = f;
	}

	void setCharSpace(double space)
	{
		charSpace = space;
	}

	void setWordSpace(double space)
	{
		wordSpace = space;
	}

	void setHorizScaling(double scale)
	{
		horizScaling = 0.01 * scale;
	}

	void setLeading(double leadingA)
	{
		leading = leadingA;
	}

	void setRise(double riseA)
	{
		rise = riseA;
	}

	void setRender(int renderA)
	{
		render = renderA;
	}

	// Add to path.
	void moveTo(double x, double y)
	{
		path->moveTo(curX = x, curY = y);
	}

	void lineTo(double x, double y)
	{
		path->lineTo(curX = x, curY = y);
	}

	void curveTo(double x1, double y1, double x2, double y2, double x3, double y3)
	{
		path->curveTo(x1, y1, x2, y2, curX = x3, curY = y3);
	}

	void closePath()
	{
		path->close();
		curX = path->getLastX();
		curY = path->getLastY();
	}

	void clearPath();

	// Update clip region.
	void clip();
	void clipToStrokePath();
	void clipToRect(double xMin, double yMin, double xMax, double yMax);

	void resetDevClipRect(double xMin, double yMin, double xMax, double yMax)
	{
		clipXMin = xMin;
		clipYMin = yMin;
		clipXMax = xMax;
		clipYMax = yMax;
	}

	// Text position.
	void textSetPos(double tx, double ty)
	{
		lineX = tx;
		lineY = ty;
	}

	void textMoveTo(double tx, double ty)
	{
		lineX = tx;
		lineY = ty;
		textTransform(tx, ty, &curX, &curY);
	}

	void textShift(double tx, double ty);
	void shift(double dx, double dy);

	// Ignore color operators (in cached/uncolored Type 3 chars, and uncolored tiling patterns).
	// Cached Type 3 char status.
	void setIgnoreColorOps(bool ignore) { ignoreColorOps = ignore; }

	// Push/pop GfxState on/off stack.
	GfxState* save();
	GfxState* restore();

	bool hasSaves() { return saved != nullptr; }

	// Misc
	bool parseBlendMode(Object* obj, GfxBlendMode* mode);

private:
	double             hDPI             = 0;                            //
	double             vDPI             = 0;                            // resolution
	double             ctm[6]           = {};                           // coord transform matrix
	double             px1              = 0;                            //
	double             py1              = 0;                            //
	double             px2              = 0;                            //
	double             py2              = 0;                            // page corners (user coords)
	double             pageWidth        = 0;                            //
	double             pageHeight       = 0;                            // page size (pixels)
	int                rotate           = 0;                            // page rotation angle
	GfxColorSpace*     fillColorSpace   = nullptr;                      // fill color space
	GfxColorSpace*     strokeColorSpace = nullptr;                      // stroke color space
	GfxColor           fillColor        = {};                           // fill color
	GfxColor           strokeColor      = {};                           // stroke color
	GfxPattern*        fillPattern      = nullptr;                      // fill pattern
	GfxPattern*        strokePattern    = nullptr;                      // stroke pattern
	GfxBlendMode       blendMode        = gfxBlendNormal;               // transparency blend mode
	double             fillOpacity      = 0;                            // fill opacity
	double             strokeOpacity    = 0;                            // stroke opacity
	bool               fillOverprint    = false;                        // fill overprint
	bool               strokeOverprint  = false;                        // stroke overprint
	int                overprintMode    = 0;                            // overprint mode ("OPM")
	GfxRenderingIntent renderingIntent  = gfxRenderingIntentPerceptual; // rendering intent
	Function*          transfer[4]      = {};                           // transfer function (entries may be: all nullptr = identity; last three nullptr = single function; all four non-nullptr = R,G,B,gray functions)
	double             lineWidth        = 0;                            // line width
	double*            lineDash         = nullptr;                      // line dash
	int                lineDashLength   = 0;                            //
	double             lineDashStart    = 0;                            //
	double             flatness         = 0;                            // curve flatness
	int                lineJoin         = 0;                            // line join style
	int                lineCap          = 0;                            // line cap style
	double             miterLimit       = 0;                            // line miter limit
	bool               strokeAdjust     = false;                        // stroke adjustment
	GfxFont*           font             = nullptr;                      // font
	double             fontSize         = 0;                            // font size
	double             textMat[6]       = {};                           // text matrix
	double             charSpace        = 0;                            // character spacing
	double             wordSpace        = 0;                            // word spacing
	double             horizScaling     = 0;                            // horizontal scaling
	double             leading          = 0;                            // text leading
	double             rise             = 0;                            // text rise
	int                render           = 0;                            // text rendering mode
	GfxPath*           path             = nullptr;                      // array of path elements
	double             curX             = 0;                            //
	double             curY             = 0;                            // current point (user coords)
	double             lineX            = 0;                            //
	double             lineY            = 0;                            // start of current text line (text coords)
	double             clipXMin         = 0;                            //
	double             clipYMin         = 0;                            // bounding box for clip region
	double             clipXMax         = 0;                            //
	double             clipYMax         = 0;                            //
	bool               ignoreColorOps   = false;                        // ignore color ops (in cached/uncolored Type 3 chars, and uncolored tiling patterns)
	GfxState*          saved            = nullptr;                      // next GfxState on stack

	GfxState(GfxState* state, bool copyPath);
};
