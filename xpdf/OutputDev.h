//========================================================================
//
// OutputDev.h
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "CharTypes.h"
#include "Object.h" // Object, Ref

class Catalog;
class Dict;
class Function;
class Gfx;
struct GfxColor;
class GfxColorSpace;
class GfxImageColorMap;
class GfxShading;
class GfxState;
class Link;
class Links;
class Page;
class Stream;

//------------------------------------------------------------------------
// OutputDev
//------------------------------------------------------------------------

class OutputDev
{
public:
	// Constructor.
	OutputDev() {}

	// Destructor.
	virtual ~OutputDev() {}

	//----- get info about output device

	// Does this device use upside-down coordinates?
	// (Upside-down means (0,0) is the top left corner of the page.)
	virtual bool upsideDown() = 0;

	// Does this device use drawChar() or drawString()?
	virtual bool useDrawChar() = 0;

	// Does this device use tilingPatternFill()?  If this returns false,
	// tiling pattern fills will be reduced to a series of other drawing
	// operations.
	virtual bool useTilingPatternFill() { return false; }

	// Does this device use drawForm()?  If this returns false,
	// form-type XObjects will be interpreted (i.e., unrolled).
	virtual bool useDrawForm() { return false; }

	// Does this device use beginType3Char/endType3Char?  Otherwise,
	// text in Type 3 fonts will be drawn with drawChar/drawString.
	virtual bool interpretType3Chars() = 0;

	// Does this device need non-text content?
	virtual bool needNonText() { return true; }

	// Does this device require incCharCount to be called for text on
	// non-shown layers?
	virtual bool needCharCount() { return false; }

	//----- initialization and control

	// Set default transform matrix.
	virtual void setDefaultCTM(double* ctm);

	// Check to see if a page slice should be displayed.  If this
	// returns false, the page display is aborted.  Typically, an
	// OutputDev will use some alternate means to display the page
	// before returning false.
	virtual bool checkPageSlice(Page* page, double hDPI, double vDPI, int rotate, bool useMediaBox, bool crop, int sliceX, int sliceY, int sliceW, int sliceH, bool printing, bool (*abortCheckCbk)(void* data) = nullptr, void* abortCheckCbkData = nullptr)
	{
		return true;
	}

	// Start a page.
	virtual void startPage(int pageNum, GfxState* state) {}

	// End a page.
	virtual void endPage() {}

	// Start/end a sub-page stream (Form XObject, annotation).
	virtual void startStream(Ref streamRef, GfxState* state) {}

	virtual void endStream(Ref streamRef) {}

	//----- coordinate conversion

	// Convert between device and user coordinates.
	virtual void cvtDevToUser(double dx, double dy, double* ux, double* uy);
	virtual void cvtUserToDev(double ux, double uy, double* dx, double* dy);
	virtual void cvtUserToDev(double ux, double uy, int* dx, int* dy);

	double* getDefCTM() { return defCTM; }

	double* getDefICTM() { return defICTM; }

	//----- save/restore graphics state
	virtual void saveState(GfxState* state) {}

	virtual void restoreState(GfxState* state) {}

	//----- update graphics state
	virtual void updateAll(GfxState* state);

	virtual void updateCTM(GfxState* state, double m11, double m12, double m21, double m22, double m31, double m32) {}

	virtual void updateLineDash(GfxState* state) {}

	virtual void updateFlatness(GfxState* state) {}

	virtual void updateLineJoin(GfxState* state) {}

	virtual void updateLineCap(GfxState* state) {}

	virtual void updateMiterLimit(GfxState* state) {}

	virtual void updateLineWidth(GfxState* state) {}

	virtual void updateStrokeAdjust(GfxState* state) {}

	virtual void updateFillColorSpace(GfxState* state) {}

	virtual void updateStrokeColorSpace(GfxState* state) {}

	virtual void updateFillColor(GfxState* state) {}

	virtual void updateStrokeColor(GfxState* state) {}

	virtual void updateBlendMode(GfxState* state) {}

	virtual void updateFillOpacity(GfxState* state) {}

	virtual void updateStrokeOpacity(GfxState* state) {}

	virtual void updateFillOverprint(GfxState* state) {}

	virtual void updateStrokeOverprint(GfxState* state) {}

	virtual void updateOverprintMode(GfxState* state) {}

	virtual void updateRenderingIntent(GfxState* state) {}

	virtual void updateTransfer(GfxState* state) {}

	//----- update text state
	virtual void updateFont(GfxState* state) {}

	virtual void updateTextMat(GfxState* state) {}

	virtual void updateCharSpace(GfxState* state) {}

	virtual void updateRender(GfxState* state) {}

	virtual void updateRise(GfxState* state) {}

	virtual void updateWordSpace(GfxState* state) {}

	virtual void updateHorizScaling(GfxState* state) {}

	virtual void updateTextPos(GfxState* state) {}

	virtual void updateTextShift(GfxState* state, double shift) {}

	//----- path painting
	virtual void stroke(GfxState* state) {}

	virtual void fill(GfxState* state) {}

	virtual void eoFill(GfxState* state) {}

	virtual void fillStroke(GfxState* state, bool eo);

	virtual void tilingPatternFill(GfxState* state, Gfx* gfx, Object* strRef, int paintType, int tilingType, Dict* resDict, double* mat, double* bbox, int x0, int y0, int x1, int y1, double xStep, double yStep) {}

	virtual bool shadedFill(GfxState* state, GfxShading* shading)
	{
		return false;
	}

	//----- path clipping
	virtual void clip(GfxState* state) {}

	virtual void eoClip(GfxState* state) {}

	virtual void clipToStrokePath(GfxState* state) {}

	//----- text drawing
	virtual void beginStringOp(GfxState* state) {}

	virtual void endStringOp(GfxState* state) {}

	virtual void beginString(GfxState* state, const std::string& s) {}

	virtual void endString(GfxState* state) {}

	virtual void drawChar(GfxState* state, double x, double y, double dx, double dy, double originX, double originY, CharCode code, int nBytes, Unicode* u, int uLen, bool fill, bool stroke, bool makePath) {}

	virtual void drawString(GfxState* state, const std::string& s, bool fill, bool stroke, bool makePath) {}

	virtual void fillTextPath(GfxState* state) {}

	virtual void strokeTextPath(GfxState* state) {}

	virtual void clipToTextPath(GfxState* state) {}

	virtual void clipToTextStrokePath(GfxState* state) {}

	virtual void clearTextPath(GfxState* state) {}

	virtual void addTextPathToSavedClipPath(GfxState* state) {}

	virtual void clipToSavedClipPath(GfxState* state) {}

	virtual bool beginType3Char(GfxState* state, double x, double y, double dx, double dy, CharCode code, Unicode* u, int uLen);

	virtual void endType3Char(GfxState* state) {}

	virtual void endTextObject(GfxState* state) {}

	virtual void incCharCount(int nChars) {}

	virtual void beginActualText(GfxState* state, Unicode* u, int uLen) {}

	virtual void endActualText(GfxState* state) {}

	//----- image drawing
	virtual void drawImageMask(GfxState* state, Object* ref, Stream* str, int width, int height, bool invert, bool inlineImg, bool interpolate);
	virtual void setSoftMaskFromImageMask(GfxState* state, Object* ref, Stream* str, int width, int height, bool invert, bool inlineImg, bool interpolate);
	virtual void drawImage(GfxState* state, Object* ref, Stream* str, int width, int height, GfxImageColorMap* colorMap, int* maskColors, bool inlineImg, bool interpolate);
	virtual void drawMaskedImage(GfxState* state, Object* ref, Stream* str, int width, int height, GfxImageColorMap* colorMap, Object* maskRef, Stream* maskStr, int maskWidth, int maskHeight, bool maskInvert, bool interpolate);
	virtual void drawSoftMaskedImage(GfxState* state, Object* ref, Stream* str, int width, int height, GfxImageColorMap* colorMap, Object* maskRef, Stream* maskStr, int maskWidth, int maskHeight, GfxImageColorMap* maskColorMap, double* matte, bool interpolate);

#if OPI_SUPPORT
	//----- OPI functions
	virtual void opiBegin(GfxState* state, Dict* opiDict);
	virtual void opiEnd(GfxState* state, Dict* opiDict);
#endif

	//----- Type 3 font operators
	virtual void type3D0(GfxState* state, double wx, double wy) {}

	virtual void type3D1(GfxState* state, double wx, double wy, double llx, double lly, double urx, double ury) {}

	//----- form XObjects
	virtual void drawForm(Ref id) {}

	//----- PostScript XObjects
	virtual void psXObject(Stream* psStream, Stream* level1Stream) {}

	//----- transparency groups and soft masks
	virtual bool beginTransparencyGroup(GfxState* state, double* bbox, GfxColorSpace* blendingColorSpace, bool isolated, bool knockout, bool forSoftMask) { return true; }

	virtual void endTransparencyGroup(GfxState* state) {}

	virtual void paintTransparencyGroup(GfxState* state, double* bbox) {}

	virtual void setSoftMask(GfxState* state, double* bbox, bool alpha, Function* transferFunc, GfxColor* backdropColor) {}

	virtual void clearSoftMask(GfxState* state) {}

	//----- links
	virtual void processLink(Link* link) {}

	//----- structure tree
	virtual void beginStructureItem(const char* tag, int mcid, Dict* dict) {}

	virtual void endStructureItem() {}

private:
	double defCTM[6]  = {}; // default coordinate transform matrix
	double defICTM[6] = {}; // inverse of default CTM
};
