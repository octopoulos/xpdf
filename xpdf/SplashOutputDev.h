//========================================================================
//
// SplashOutputDev.h
//
// Copyright 2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "SplashTypes.h"
#include "config.h"
#include "OutputDev.h"
#include "GfxState.h"

class Gfx8BitFont;
class SplashBitmap;
class Splash;
class SplashPath;
class SplashPattern;
class SplashFontEngine;
class SplashFont;
class T3FontCache;
struct T3FontCacheTag;
struct T3GlyphStack;
struct SplashTransparencyGroup;

//------------------------------------------------------------------------

// number of Type 3 fonts to cache
#define splashOutT3FontCacheSize 8

//------------------------------------------------------------------------
// SplashOutputDev
//------------------------------------------------------------------------

class SplashOutputDev : public OutputDev
{
public:
	// Constructor.
	SplashOutputDev(SplashColorMode colorModeA, int bitmapRowPadA, bool reverseVideoA, SplashColorPtr paperColorA, bool bitmapTopDownA = true, bool allowAntialiasA = true);

	// Destructor.
	virtual ~SplashOutputDev();

	//----- get info about output device

	// Does this device use upside-down coordinates?
	// (Upside-down means (0,0) is the top left corner of the page.)
	virtual bool upsideDown() { return bitmapTopDown ^ bitmapUpsideDown; }

	// Does this device use drawChar() or drawString()?
	virtual bool useDrawChar() { return true; }

	// Does this device use tilingPatternFill()?  If this returns false,
	// tiling pattern fills will be reduced to a series of other drawing
	// operations.
	virtual bool useTilingPatternFill() { return true; }

	// Does this device use beginType3Char/endType3Char?  Otherwise,
	// text in Type 3 fonts will be drawn with drawChar/drawString.
	virtual bool interpretType3Chars() { return true; }

	//----- initialization and control

	// Start a page.
	virtual void startPage(int pageNum, GfxState* state);

	// End a page.
	virtual void endPage();

	//----- save/restore graphics state
	virtual void saveState(GfxState* state);
	virtual void restoreState(GfxState* state);

	//----- update graphics state
	virtual void updateAll(GfxState* state);
	virtual void updateCTM(GfxState* state, double m11, double m12, double m21, double m22, double m31, double m32);
	virtual void updateLineDash(GfxState* state);
	virtual void updateFlatness(GfxState* state);
	virtual void updateLineJoin(GfxState* state);
	virtual void updateLineCap(GfxState* state);
	virtual void updateMiterLimit(GfxState* state);
	virtual void updateLineWidth(GfxState* state);
	virtual void updateStrokeAdjust(GfxState* state);
	virtual void updateFillColor(GfxState* state);
	virtual void updateStrokeColor(GfxState* state);
	virtual void updateBlendMode(GfxState* state);
	virtual void updateFillOpacity(GfxState* state);
	virtual void updateStrokeOpacity(GfxState* state);
	virtual void updateRenderingIntent(GfxState* state);
	virtual void updateTransfer(GfxState* state);

	//----- update text state
	virtual void updateFont(GfxState* state);

	//----- path painting
	virtual void stroke(GfxState* state);
	virtual void fill(GfxState* state);
	virtual void eoFill(GfxState* state);
	virtual void tilingPatternFill(GfxState* state, Gfx* gfx, Object* strRef, int paintType, int tilingType, Dict* resDict, double* mat, double* bbox, int x0, int y0, int x1, int y1, double xStep, double yStep);
	virtual bool shadedFill(GfxState* state, GfxShading* shading);

	//----- path clipping
	virtual void clip(GfxState* state);
	virtual void eoClip(GfxState* state);
	virtual void clipToStrokePath(GfxState* state);

	//----- text drawing
	virtual void drawChar(GfxState* state, double x, double y, double dx, double dy, double originX, double originY, CharCode code, int nBytes, Unicode* u, int uLen, bool fill, bool stroke, bool makePath);
	virtual void fillTextPath(GfxState* state);
	virtual void strokeTextPath(GfxState* state);
	virtual void clipToTextPath(GfxState* state);
	virtual void clipToTextStrokePath(GfxState* state);
	virtual void clearTextPath(GfxState* state);
	virtual void addTextPathToSavedClipPath(GfxState* state);
	virtual void clipToSavedClipPath(GfxState* state);
	virtual bool beginType3Char(GfxState* state, double x, double y, double dx, double dy, CharCode code, Unicode* u, int uLen);
	virtual void endType3Char(GfxState* state);
	virtual void endTextObject(GfxState* state);

	//----- image drawing
	virtual void drawImageMask(GfxState* state, Object* ref, Stream* str, int width, int height, bool invert, bool inlineImg, bool interpolate);
	virtual void setSoftMaskFromImageMask(GfxState* state, Object* ref, Stream* str, int width, int height, bool invert, bool inlineImg, bool interpolate);
	virtual void drawImage(GfxState* state, Object* ref, Stream* str, int width, int height, GfxImageColorMap* colorMap, int* maskColors, bool inlineImg, bool interpolate);
	virtual void drawMaskedImage(GfxState* state, Object* ref, Stream* str, int width, int height, GfxImageColorMap* colorMap, Object* maskRef, Stream* maskStr, int maskWidth, int maskHeight, bool maskInvert, bool interpolate);
	virtual void drawSoftMaskedImage(GfxState* state, Object* ref, Stream* str, int width, int height, GfxImageColorMap* colorMap, Object* maskRef, Stream* maskStr, int maskWidth, int maskHeight, GfxImageColorMap* maskColorMap, double* matte, bool interpolate);

	//----- Type 3 font operators
	virtual void type3D0(GfxState* state, double wx, double wy);
	virtual void type3D1(GfxState* state, double wx, double wy, double llx, double lly, double urx, double ury);

	//----- transparency groups and soft masks
	virtual bool beginTransparencyGroup(GfxState* state, double* bbox, GfxColorSpace* blendingColorSpace, bool isolated, bool knockout, bool forSoftMask);
	virtual void endTransparencyGroup(GfxState* state);
	virtual void paintTransparencyGroup(GfxState* state, double* bbox);
	virtual void setSoftMask(GfxState* state, double* bbox, bool alpha, Function* transferFunc, GfxColor* backdropColor);
	virtual void clearSoftMask(GfxState* state);

	//----- special access

	// Called to indicate that a new PDF document has been loaded.
	void startDoc(XRef* xrefA);

	void setStartPageCallback(void (*cbk)(void* data), void* data)
	{
		startPageCbk     = cbk;
		startPageCbkData = data;
	}

	void setPaperColor(SplashColorPtr paperColorA);

	bool isReverseVideo() { return reverseVideo; }

	void setReverseVideo(bool reverseVideoA) { reverseVideo = reverseVideoA; }

	// Get the bitmap and its size.
	SplashBitmap* getBitmap() { return bitmap; }

	int getBitmapWidth();
	int getBitmapHeight();

	// Returns the last rasterized bitmap, transferring ownership to the
	// caller.
	SplashBitmap* takeBitmap();

	// Set this flag to true to generate an upside-down bitmap (useful
	// for Windows BMP files).
	void setBitmapUpsideDown(bool f) { bitmapUpsideDown = f; }

	// Setting this to true disables the final composite (with the
	// opaque paper color), resulting in transparent output.
	void setNoComposite(bool f) { noComposite = f; }

	// Get the Splash object.
	Splash* getSplash() { return splash; }

	// Get the modified region.
	void getModRegion(int* xMin, int* yMin, int* xMax, int* yMax);

	// Clear the modified region.
	void clearModRegion();

	// Set the Splash fill color.
	void setFillColor(int r, int g, int b);

	// Get a font object for a Base-14 font, using the Latin-1 encoding.
	SplashFont* getFont(const std::string& name, SplashCoord* textMatA);

	SplashFont* getCurrentFont() { return font; }

	// If <skipTextA> is true, don't draw horizontal text.
	// If <skipRotatedTextA> is true, don't draw rotated (non-horizontal) text.
	void setSkipText(bool skipHorizTextA, bool skipRotatedTextA)
	{
		skipHorizText   = skipHorizTextA;
		skipRotatedText = skipRotatedTextA;
	}

	int getNestCount() { return nestCount; }

	// Get the screen parameters.
	SplashScreenParams* getScreenParams() { return &screenParams; }

private:
	void           setupScreenParams(double hDPI, double vDPI);
	SplashPattern* getColor(GfxGray gray);
	SplashPattern* getColor(GfxRGB* rgb);
#if SPLASH_CMYK
	SplashPattern* getColor(GfxCMYK* cmyk);
#endif
	void getColor(GfxGray gray, SplashColorPtr color);
	void getColor(GfxRGB* rgb, SplashColorPtr color);
#if SPLASH_CMYK
	void getColor(GfxCMYK* cmyk, SplashColorPtr color);
#endif
	void        setOverprintMask(GfxState* state, GfxColorSpace* colorSpace, bool overprintFlag, int overprintMode, GfxColor* singleColor);
	SplashPath* convertPath(GfxState* state, GfxPath* path, bool dropEmptySubpaths);
	void        doUpdateFont(GfxState* state);
	void        drawType3Glyph(GfxState* state, T3FontCache* t3Font, T3FontCacheTag* tag, uint8_t* data);
	static bool imageMaskSrc(void* data, uint8_t* line);
	static bool imageSrc(void* data, SplashColorPtr colorLine, uint8_t* alphaLine);
	static bool alphaImageSrc(void* data, SplashColorPtr line, uint8_t* alphaLine);
	static bool maskedImageSrc(void* data, SplashColorPtr line, uint8_t* alphaLine);
	static bool softMaskMatteImageSrc(void* data, SplashColorPtr colorLine, uint8_t* alphaLine);
	std::string makeImageTag(Object* ref, GfxRenderingIntent ri, GfxColorSpace* colorSpace);
	void        reduceImageResolution(Stream* str, double* mat, int* width, int* height);
	void        clearMaskRegion(GfxState* state, Splash* maskSplash, double xMin, double yMin, double xMax, double yMax);
	void        copyState(Splash* oldSplash, bool copyColors);
#if 1 //~tmp: turn off anti-aliasing temporarily
	void setInShading(bool sh);
#endif

	SplashColorMode          colorMode                             = splashModeRGB8; //
	int                      bitmapRowPad                          = 0;              //
	bool                     bitmapTopDown                         = false;          //
	bool                     bitmapUpsideDown                      = false;          //
	bool                     noComposite                           = false;          //
	bool                     allowAntialias                        = false;          //
	bool                     vectorAntialias                       = false;          //
	bool                     reverseVideo                          = false;          // reverse video mode
	bool                     reverseVideoInvertImages              = false;          //
	SplashColor              paperColor                            = {};             // paper color
	SplashScreenParams       screenParams                          = {};             //
	bool                     skipHorizText                         = false;          //
	bool                     skipRotatedText                       = false;          //
	XRef*                    xref                                  = nullptr;        // xref table for current document
	SplashBitmap*            bitmap                                = nullptr;        //
	Splash*                  splash                                = nullptr;        //
	SplashFontEngine*        fontEngine                            = nullptr;        //
	T3FontCache*             t3FontCache[splashOutT3FontCacheSize] = {};             // Type 3 font cache
	int                      nT3Fonts                              = 0;              // number of valid entries in t3FontCache
	T3GlyphStack*            t3GlyphStack                          = nullptr;        // Type 3 glyph context stack
	SplashFont*              font                                  = nullptr;        // current font
	bool                     needFontUpdate                        = false;          // set when the font needs to be updated
	SplashPath*              savedTextPath                         = nullptr;        // path built for text string
	SplashPath*              savedClipPath                         = nullptr;        // clipping path built with text object
	SplashTransparencyGroup* transpGroupStack                      = nullptr;        // transparency group stack
	int                      nestCount                             = 0;              //

	void (*startPageCbk)(void* data) = nullptr; //
	void* startPageCbkData           = nullptr; //
};
