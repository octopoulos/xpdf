//========================================================================
//
// Splash.h
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "SplashTypes.h"
#include "SplashClip.h"

class Splash;
class SplashBitmap;
struct SplashGlyphBitmap;
class SplashState;
class SplashPattern;
class SplashScreen;
class SplashPath;
class SplashXPath;
class SplashFont;
struct SplashPipe;
struct SplashDrawImageMaskRowData;
class ImageScaler;
typedef void (Splash::*SplashDrawImageMaskRowFunc)(SplashDrawImageMaskRowData* data, uint8_t* maskData, int x, int y, int width);
struct SplashDrawImageRowData;
typedef void (Splash::*SplashDrawImageRowFunc)(SplashDrawImageRowData* data, uint8_t* colorData, uint8_t* alphaData, int x, int y, int width);

//------------------------------------------------------------------------

// Retrieves the next line of pixels in an image mask.
// Normally, fills in *<line> and returns true.
// If the image stream is exhausted, returns false.
typedef bool (*SplashImageMaskSource)(void* data, uint8_t* pixel);

// Retrieves the next line of pixels in an image.
// Normally, fills in *<line> and returns true.
// If the image stream is exhausted, returns false.
typedef bool (*SplashImageSource)(void* data, SplashColorPtr colorLine, uint8_t* alphaLine);

//------------------------------------------------------------------------

enum SplashPipeResultColorCtrl
{
	splashPipeResultColorNoAlphaBlendMono,
	splashPipeResultColorNoAlphaBlendRGB,
#if SPLASH_CMYK
	splashPipeResultColorNoAlphaBlendCMYK,
#endif
	splashPipeResultColorAlphaNoBlendMono,
	splashPipeResultColorAlphaNoBlendRGB,
#if SPLASH_CMYK
	splashPipeResultColorAlphaNoBlendCMYK,
#endif
	splashPipeResultColorAlphaBlendMono,
	splashPipeResultColorAlphaBlendRGB
#if SPLASH_CMYK
	    ,
	splashPipeResultColorAlphaBlendCMYK
#endif
};

//------------------------------------------------------------------------

// Transparency group destination bitmap initialization control.
enum SplashGroupDestInitMode
{
	splashGroupDestPreInit,  // dest is already initialized
	splashGroupDestInitZero, // initialize to zero (isolated group)
	splashGroupDestInitCopy  // copy backdrop (non-isolated group)
};

//------------------------------------------------------------------------
// SplashImageCache
//------------------------------------------------------------------------

// This holds a cached image, and is shared by multiple Splash objects
// in the same thread.
class SplashImageCache
{
public:
	SplashImageCache();
	~SplashImageCache();
	bool match(const std::string& aTag, int aWidth, int aHeight, SplashColorMode aMode, bool aAlpha, bool aInterpolate);
	void reset(const std::string& aTag, int aWidth, int aHeight, SplashColorMode aMode, bool aAlpha, bool aInterpolate);
	void incRefCount();
	void decRefCount();

	std::string     tag         = "";              //
	int             width       = 0;               //
	int             height      = 0;               //
	SplashColorMode mode        = splashModeMono1; //
	bool            alpha       = false;           //
	bool            interpolate = false;           //
	uint8_t*        colorData   = nullptr;         //
	uint8_t*        alphaData   = nullptr;         //
	int             refCount    = 0;               //
};

//------------------------------------------------------------------------
// Splash
//------------------------------------------------------------------------

class Splash
{
public:
	// Create a new rasterizer object.
	Splash(SplashBitmap* bitmapA, bool vectorAntialiasA, SplashImageCache* imageCacheA, SplashScreenParams* screenParams = nullptr);
	Splash(SplashBitmap* bitmapA, bool vectorAntialiasA, SplashImageCache* imageCacheA, SplashScreen* screenA);

	~Splash();

	//----- state read

	SplashCoord*           getMatrix();
	SplashPattern*         getStrokePattern();
	SplashPattern*         getFillPattern();
	SplashScreen*          getScreen();
	SplashBlendFunc        getBlendFunc();
	SplashCoord            getStrokeAlpha();
	SplashCoord            getFillAlpha();
	SplashCoord            getLineWidth();
	int                    getLineCap();
	int                    getLineJoin();
	SplashCoord            getMiterLimit();
	SplashCoord            getFlatness();
	SplashCoord*           getLineDash();
	int                    getLineDashLength();
	SplashCoord            getLineDashPhase();
	SplashStrokeAdjustMode getStrokeAdjust();
	SplashClip*            getClip();
	SplashBitmap*          getSoftMask();
	bool                   getInNonIsolatedGroup();
	bool                   getInKnockoutGroup();

	//----- state write

	void        setMatrix(SplashCoord* matrix);
	void        setStrokePattern(SplashPattern* strokeColor);
	void        setFillPattern(SplashPattern* fillColor);
	void        setScreen(SplashScreen* screen);
	void        setBlendFunc(SplashBlendFunc func);
	void        setStrokeAlpha(SplashCoord alpha);
	void        setFillAlpha(SplashCoord alpha);
	void        setLineWidth(SplashCoord lineWidth);
	void        setLineCap(int lineCap);
	void        setLineJoin(int lineJoin);
	void        setMiterLimit(SplashCoord miterLimit);
	void        setFlatness(SplashCoord flatness);
	// the <lineDash> array will be copied
	void        setLineDash(SplashCoord* lineDash, int lineDashLength, SplashCoord lineDashPhase);
	void        setStrokeAdjust(SplashStrokeAdjustMode strokeAdjust);
	// NB: uses transformed coordinates.
	void        clipResetToRect(SplashCoord x0, SplashCoord y0, SplashCoord x1, SplashCoord y1);
	// NB: uses transformed coordinates.
	SplashError clipToRect(SplashCoord x0, SplashCoord y0, SplashCoord x1, SplashCoord y1);
	// NB: uses untransformed coordinates.
	SplashError clipToPath(SplashPath* path, bool eo);
	void        setSoftMask(SplashBitmap* softMask, bool deleteBitmap = true);
	void        setInTransparencyGroup(SplashBitmap* groupBackBitmapA, int groupBackXA, int groupBackYA, SplashGroupDestInitMode groupDestInitModeA, bool nonIsolated, bool knockout);
	void        forceDeferredInit(int y, int h);
	bool        checkTransparentRect(int x, int y, int w, int h);
	void        setTransfer(uint8_t* red, uint8_t* green, uint8_t* blue, uint8_t* gray);
	void        setOverprintMask(uint32_t overprintMask);
	void        setEnablePathSimplification(bool en);

	//----- state save/restore

	void        saveState();
	SplashError restoreState();

	//----- drawing operations

	// Fill the bitmap with <color>.
	// This is not subject to clipping.
	void clear(SplashColorPtr color, uint8_t alpha = 0x00);

	// Stroke a path using the current stroke pattern.
	SplashError stroke(SplashPath* path);

	// Fill a path using the current fill pattern.
	SplashError fill(SplashPath* path, bool eo);

	// Draw a character, using the current fill pattern.
	SplashError fillChar(SplashCoord x, SplashCoord y, int c, SplashFont* font);

	// Draw a glyph, using the current fill pattern.
	// This function does not free any data, i.e., it ignores glyph->freeData.
	SplashError fillGlyph(SplashCoord x, SplashCoord y, SplashGlyphBitmap* glyph);

	// Draws an image mask using the fill color.
	// This will read <h> lines of <w> pixels from <src>, starting with the top line.
	// "1" pixels will be drawn with the current fill color;
	// "0" pixels are transparent.
	// The matrix:
	//    [ mat[0] mat[1] 0 ]
	//    [ mat[2] mat[3] 0 ]
	//    [ mat[4] mat[5] 1 ]
	// maps a unit square to the desired destination for the image, in PostScript style:
	//    [x' y' 1] = [x y 1] * mat
	// Note that the Splash y axis points downward, and the image source is assumed to produce pixels in raster order, starting from the top line.
	// If [interpolate] is false, no filtering is done when upsampling.
	// If [antialias] is false, no filtering is done when upsampling (overriding the [interpolate] flag), and threshold filtering is done when downsampling.
	SplashError fillImageMask(const std::string& imageTag, SplashImageMaskSource src, void* srcData, int w, int h, SplashCoord* mat, bool glyphMode, bool interpolate, bool antialias);

	// Draw an image.
	// This will read <h> lines of <w> pixels from <src>, starting with the top line.
	// These pixels are assumed to be in the source mode, <srcMode>.
	// If <srcAlpha> is true, the alpha values returned by <src> are used; otherwise they are ignored.
	// The following combinations of source and target modes are supported:
	//    source       target
	//    ------       ------
	//    Mono8        Mono1   -- with dithering
	//    Mono8        Mono8
	//    RGB8         RGB8
	//    BGR8         RGB8
	//    CMYK8        CMYK8
	// The matrix behaves as for fillImageMask.
	SplashError drawImage(const std::string& imageTag, SplashImageSource src, void* srcData, SplashColorMode srcMode, bool srcAlpha, int w, int h, SplashCoord* mat, bool interpolate);

	// Composite a rectangular region from <src> onto this Splash object.
	SplashError composite(SplashBitmap* src, int xSrc, int ySrc, int xDest, int yDest, int w, int h, bool noClip, bool nonIsolated);

	// Composite a rectangular region from <src> onto this Splash object, using <srcOverprintMaskBitmap> as the overprint mask per pixel.
	// This is only supported for CMYK and DeviceN bitmaps.
	SplashError compositeWithOverprint(SplashBitmap* src, uint32_t* srcOverprintMaskBitmap, int xSrc, int ySrc, int xDest, int yDest, int w, int h, bool noClip, bool nonIsolated);

	// Composite this Splash object onto a background color.
	// The background alpha is assumed to be 1.
	void compositeBackground(SplashColorPtr color);

	// Copy a rectangular region from <src> onto the bitmap belonging to this Splash object.
	// The destination alpha values are all set to zero.
	SplashError blitTransparent(SplashBitmap* src, int xSrc, int ySrc, int xDest, int yDest, int w, int h);

	// Copy a rectangular region from the bitmap belonging to this Splash object to <dest>.
	// The alpha values are corrected for a non-isolated group.
	SplashError blitCorrectedAlpha(SplashBitmap* dest, int xSrc, int ySrc, int xDest, int yDest, int w, int h);

	//----- misc

	// Construct a path for a stroke, given the path to be stroked and the line width <w>.
	// All other stroke parameters are taken from the current state.
	// If <flatten> is true, this function will first flatten the path and handle the linedash.
	SplashPath* makeStrokePath(SplashPath* path, SplashCoord w, int lineCap, int lineJoin, bool flatten = true);

	// Reduce the size of a rectangle as much as possible by moving any edges that are completely outside the clip region.
	// Returns the clipping status of the resulting rectangle.
	SplashClipResult limitRectToClipRect(int* xMin, int* yMin, int* xMax, int* yMax);

	// Return the associated bitmap.
	SplashBitmap* getBitmap() { return bitmap; }

	// Enable writing the per-pixel overprint mask to a separate bitmap.
	void setOverprintMaskBitmap(uint32_t* overprintMaskBitmapA)
	{
		overprintMaskBitmap = overprintMaskBitmapA;
	}

	// Set the minimum line width.
	void setMinLineWidth(SplashCoord w) { minLineWidth = w; }

	// Get a bounding box which includes all modifications since the last call to clearModRegion.
	void getModRegion(int* xMin, int* yMin, int* xMax, int* yMax)
	{
		*xMin = modXMin;
		*yMin = modYMin;
		*xMax = modXMax;
		*yMax = modYMax;
	}

	// Clear the modified region bounding box.
	void clearModRegion();

	// Get clipping status for the last drawing operation subject to clipping.
	SplashClipResult getClipRes() { return opClipRes; }

	// Toggle debug mode on or off.
	void setDebugMode(bool debugModeA) { debugMode = debugModeA; }

	SplashImageCache* getImageCache() { return imageCache; }

#if 1 //~tmp: turn off anti-aliasing temporarily
	void setInShading(bool sh) { inShading = sh; }
#endif

private:
	void pipeInit(SplashPipe* pipe, SplashPattern* pattern, uint8_t aInput, bool usesShape, bool nonIsolatedGroup, bool usesSrcOverprint = false);
	void pipeRun(SplashPipe* pipe, int x0, int x1, int y, uint8_t* shapePtr, SplashColorPtr cSrcPtr);
	void pipeRunSimpleMono1(SplashPipe* pipe, int x0, int x1, int y, uint8_t* shapePtr, SplashColorPtr cSrcPtr);
	void pipeRunSimpleMono8(SplashPipe* pipe, int x0, int x1, int y, uint8_t* shapePtr, SplashColorPtr cSrcPtr);
	void pipeRunSimpleRGB8(SplashPipe* pipe, int x0, int x1, int y, uint8_t* shapePtr, SplashColorPtr cSrcPtr);
	void pipeRunSimpleBGR8(SplashPipe* pipe, int x0, int x1, int y, uint8_t* shapePtr, SplashColorPtr cSrcPtr);
#if SPLASH_CMYK
	void pipeRunSimpleCMYK8(SplashPipe* pipe, int x0, int x1, int y, uint8_t* shapePtr, SplashColorPtr cSrcPtr);
#endif
	void pipeRunShapeMono1(SplashPipe* pipe, int x0, int x1, int y, uint8_t* shapePtr, SplashColorPtr cSrcPtr);
	void pipeRunShapeMono8(SplashPipe* pipe, int x0, int x1, int y, uint8_t* shapePtr, SplashColorPtr cSrcPtr);
	void pipeRunShapeRGB8(SplashPipe* pipe, int x0, int x1, int y, uint8_t* shapePtr, SplashColorPtr cSrcPtr);
	void pipeRunShapeBGR8(SplashPipe* pipe, int x0, int x1, int y, uint8_t* shapePtr, SplashColorPtr cSrcPtr);
#if SPLASH_CMYK
	void pipeRunShapeCMYK8(SplashPipe* pipe, int x0, int x1, int y, uint8_t* shapePtr, SplashColorPtr cSrcPtr);
#endif
	void pipeRunShapeNoAlphaMono8(SplashPipe* pipe, int x0, int x1, int y, uint8_t* shapePtr, SplashColorPtr cSrcPtr);
	void pipeRunAAMono1(SplashPipe* pipe, int x0, int x1, int y, uint8_t* shapePtr, SplashColorPtr cSrcPtr);
	void pipeRunAAMono8(SplashPipe* pipe, int x0, int x1, int y, uint8_t* shapePtr, SplashColorPtr cSrcPtr);
	void pipeRunAARGB8(SplashPipe* pipe, int x0, int x1, int y, uint8_t* shapePtr, SplashColorPtr cSrcPtr);
	void pipeRunAABGR8(SplashPipe* pipe, int x0, int x1, int y, uint8_t* shapePtr, SplashColorPtr cSrcPtr);
#if SPLASH_CMYK
	void pipeRunAACMYK8(SplashPipe* pipe, int x0, int x1, int y, uint8_t* shapePtr, SplashColorPtr cSrcPtr);
#endif
	void pipeRunSoftMaskMono8(SplashPipe* pipe, int x0, int x1, int y, uint8_t* shapePtr, SplashColorPtr cSrcPtr);
	void pipeRunSoftMaskRGB8(SplashPipe* pipe, int x0, int x1, int y, uint8_t* shapePtr, SplashColorPtr cSrcPtr);
	void pipeRunSoftMaskBGR8(SplashPipe* pipe, int x0, int x1, int y, uint8_t* shapePtr, SplashColorPtr cSrcPtr);
#if SPLASH_CMYK
	void pipeRunSoftMaskCMYK8(SplashPipe* pipe, int x0, int x1, int y, uint8_t* shapePtr, SplashColorPtr cSrcPtr);
#endif
	void pipeRunNonIsoMono8(SplashPipe* pipe, int x0, int x1, int y, uint8_t* shapePtr, SplashColorPtr cSrcPtr);
	void pipeRunNonIsoRGB8(SplashPipe* pipe, int x0, int x1, int y, uint8_t* shapePtr, SplashColorPtr cSrcPtr);
	void pipeRunNonIsoBGR8(SplashPipe* pipe, int x0, int x1, int y, uint8_t* shapePtr, SplashColorPtr cSrcPtr);
#if SPLASH_CMYK
	void pipeRunNonIsoCMYK8(SplashPipe* pipe, int x0, int x1, int y, uint8_t* shapePtr, SplashColorPtr cSrcPtr);
#endif
	void         useDestRow(int y);
	void         copyGroupBackdropRow(int y);
	void         transform(SplashCoord* matrix, SplashCoord xi, SplashCoord yi, SplashCoord* xo, SplashCoord* yo);
	void         updateModX(int x);
	void         updateModY(int y);
	void         strokeNarrow(SplashPath* path);
	void         drawStrokeSpan(SplashPipe* pipe, int x0, int x1, int y, bool noClip);
	void         strokeWide(SplashPath* path, SplashCoord w, int lineCap, int lineJoin);
	SplashPath*  flattenPath(SplashPath* path, SplashCoord* matrix, SplashCoord flatness);
	void         flattenCurve(SplashCoord x0, SplashCoord y0, SplashCoord x1, SplashCoord y1, SplashCoord x2, SplashCoord y2, SplashCoord x3, SplashCoord y3, SplashCoord* matrix, SplashCoord flatness2, SplashPath* fPath);
	SplashPath*  makeDashedPath(SplashPath* xPath);
	SplashError  fillWithPattern(SplashPath* path, bool eo, SplashPattern* pattern, SplashCoord alpha);
	SplashPath*  tweakFillPath(SplashPath* path);
	bool         pathAllOutside(SplashPath* path, bool stroke);
	SplashError  fillGlyph2(int x0, int y0, SplashGlyphBitmap* glyph);
	void         getImageBounds(SplashCoord xyMin, SplashCoord xyMax, int* xyMinI, int* xyMaxI);
	void         drawImageMaskArbitraryNoInterp(uint8_t* scaledMask, SplashDrawImageMaskRowData* dd, SplashDrawImageMaskRowFunc drawRowFunc, SplashCoord* invMat, int scaledWidth, int scaledHeight, int xMin, int yMin, int xMax, int yMax);
	void         drawImageMaskArbitraryInterp(uint8_t* scaledMask, SplashDrawImageMaskRowData* dd, SplashDrawImageMaskRowFunc drawRowFunc, SplashCoord* invMat, int scaledWidth, int scaledHeight, int xMin, int yMin, int xMax, int yMax);
	void         mirrorImageMaskRow(uint8_t* maskIn, uint8_t* maskOut, int width);
	void         drawImageMaskRowNoClip(SplashDrawImageMaskRowData* data, uint8_t* maskData, int x, int y, int width);
	void         drawImageMaskRowClipNoAA(SplashDrawImageMaskRowData* data, uint8_t* maskData, int x, int y, int width);
	void         drawImageMaskRowClipAA(SplashDrawImageMaskRowData* data, uint8_t* maskData, int x, int y, int width);
	ImageScaler* getImageScaler(const std::string& imageTag, SplashImageSource src, void* srcData, int w, int h, int nComps, int scaledWidth, int scaledHeight, SplashColorMode srcMode, bool srcAlpha, bool interpolate);
	void         getScaledImage(const std::string& imageTag, SplashImageSource src, void* srcData, int w, int h, int nComps, int scaledWidth, int scaledHeight, SplashColorMode srcMode, bool srcAlpha, bool interpolate, uint8_t** scaledColor, uint8_t** scaledAlpha, bool* freeScaledImage);
	void         drawImageArbitraryNoInterp(uint8_t* scaledColor, uint8_t* scaledAlpha, SplashDrawImageRowData* dd, SplashDrawImageRowFunc drawRowFunc, SplashCoord* invMat, int scaledWidth, int scaledHeight, int xMin, int yMin, int xMax, int yMax, int nComps, bool srcAlpha);
	void         drawImageArbitraryInterp(uint8_t* scaledColor, uint8_t* scaledAlpha, SplashDrawImageRowData* dd, SplashDrawImageRowFunc drawRowFunc, SplashCoord* invMat, int scaledWidth, int scaledHeight, int xMin, int yMin, int xMax, int yMax, int nComps, bool srcAlpha);
	void         mirrorImageRow(uint8_t* colorIn, uint8_t* alphaIn, uint8_t* colorOut, uint8_t* alphaOut, int width, int nComps, bool srcAlpha);
	void         drawImageRowNoClipNoAlpha(SplashDrawImageRowData* data, uint8_t* colorData, uint8_t* alphaData, int x, int y, int width);
	void         drawImageRowNoClipAlpha(SplashDrawImageRowData* data, uint8_t* colorData, uint8_t* alphaData, int x, int y, int width);
	void         drawImageRowClipNoAlphaNoAA(SplashDrawImageRowData* data, uint8_t* colorData, uint8_t* alphaData, int x, int y, int width);
	void         drawImageRowClipNoAlphaAA(SplashDrawImageRowData* data, uint8_t* colorData, uint8_t* alphaData, int x, int y, int width);
	void         drawImageRowClipAlphaNoAA(SplashDrawImageRowData* data, uint8_t* colorData, uint8_t* alphaData, int x, int y, int width);
	void         drawImageRowClipAlphaAA(SplashDrawImageRowData* data, uint8_t* colorData, uint8_t* alphaData, int x, int y, int width);
	void         dumpPath(SplashPath* path);
	void         dumpXPath(SplashXPath* path);

	static SplashPipeResultColorCtrl pipeResultColorNoAlphaBlend[];
	static SplashPipeResultColorCtrl pipeResultColorAlphaNoBlend[];
	static SplashPipeResultColorCtrl pipeResultColorAlphaBlend[];
	static int                       pipeNonIsoGroupCorrection[];

	SplashBitmap*           bitmap              = nullptr;                //
	int                     bitmapComps         = 0;                      //
	SplashState*            state               = nullptr;                //
	uint8_t*                scanBuf             = nullptr;                //
	uint8_t*                scanBuf2            = nullptr;                //
	SplashBitmap*           groupBackBitmap     = nullptr;                // for transparency groups, this is the bitmap containing the alpha0/color0 values
	int                     groupBackX          = 0;                      //
	int                     groupBackY          = 0;                      // offset within groupBackBitmap
	SplashGroupDestInitMode groupDestInitMode   = splashGroupDestPreInit; //
	int                     groupDestInitYMin   = 0;                      //
	int                     groupDestInitYMax   = 0;                      //
	uint32_t*               overprintMaskBitmap = nullptr;                //
	SplashCoord             minLineWidth        = 0;                      //
	int                     modXMin             = 0;                      //
	int                     modYMin             = 0;                      //
	int                     modXMax             = 0;                      //
	int                     modYMax             = 0;                      //
	SplashClipResult        opClipRes           = splashClipAllInside;    //
	bool                    vectorAntialias     = false;                  //
	bool                    inShading           = false;                  //
	bool                    debugMode           = false;                  //
	SplashImageCache*       imageCache          = nullptr;                //
};
