//========================================================================
//
// PSOutputDev.h
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include <stddef.h>
#include "config.h"
#include "Object.h"
#include "GlobalParams.h"
#include "OutputDev.h"

#include "GfxFont.h"     // GfxFontType
#include "PSOutputDev.h" // PSFontFileLocation

class GHash;
class PDFDoc;
class XRef;
class Function;
class GfxPath;
class GfxFont;
class GfxColorSpace;
class GfxDeviceGrayColorSpace;
class GfxCalGrayColorSpace;
class GfxDeviceRGBColorSpace;
class GfxCalRGBColorSpace;
class GfxDeviceCMYKColorSpace;
class GfxLabColorSpace;
class GfxICCBasedColorSpace;
class GfxIndexedColorSpace;
class GfxSeparationColorSpace;
class GfxDeviceNColorSpace;
class GfxFunctionShading;
class GfxAxialShading;
class GfxRadialShading;
class PDFRectangle;
class PSOutCustomColor;
class PSOutputDev;

enum PSFontFileLocation
{
	psFontFileResident,
	psFontFileEmbedded,
	psFontFileExternal
};

class PSFontFileInfo
{
public:
	PSFontFileInfo(const std::string& psNameA, GfxFontType typeA, PSFontFileLocation locA);
	~PSFontFileInfo();

	std::string        psName       = "";                 // name under which font is defined
	GfxFontType        type         = fontUnknownType;    // font type
	PSFontFileLocation loc          = psFontFileResident; // font location
	Ref                embFontID    = {};                 // object ID for the embedded font file (for all embedded fonts)
	std::string        extFileName  = "";                 // external font file path (for all external fonts)
	std::string        encoding     = "";                 // encoding name (for resident CID fonts)
	int*               codeToGID    = nullptr;            // mapping from code/CID to GID (for TrueType, OpenType-TrueType, and CID OpenType-CFF fonts)
	int                codeToGIDLen = 0;                  // length of codeToGID array
};

using sPSFontFileInfo = std::shared_ptr<PSFontFileInfo>;

//------------------------------------------------------------------------
// PSOutputDev
//------------------------------------------------------------------------

enum PSOutMode
{
	psModePS,
	psModeEPS,
	psModeForm
};

enum PSFileType
{
	psFile,   // write to file
	psPipe,   // write to pipe
	psStdout, // write to stdout
	psGeneric // write to a generic stream
};

enum PSOutCustomCodeLocation
{
	psOutCustomDocSetup,
	psOutCustomPageSetup
};

typedef void (*PSOutputFunc)(void* stream, const char* data, size_t len);

typedef std::string (*PSOutCustomCodeCbk)(PSOutputDev* psOut, PSOutCustomCodeLocation loc, int n, void* data);

class PSOutputDev : public OutputDev
{
public:
	// Open a PostScript output file, and write the prolog.
	PSOutputDev(const char* fileName, PDFDoc* docA, int firstPageA, int lastPageA, PSOutMode modeA, int imgLLXA = 0, int imgLLYA = 0, int imgURXA = 0, int imgURYA = 0, bool manualCtrlA = false, PSOutCustomCodeCbk customCodeCbkA = nullptr, void* customCodeCbkDataA = nullptr, bool honorUserUnitA = false, bool fileNameIsUTF8 = false);

	// Open a PSOutputDev that will write to a generic stream.
	PSOutputDev(PSOutputFunc outputFuncA, void* outputStreamA, PDFDoc* docA, int firstPageA, int lastPageA, PSOutMode modeA, int imgLLXA = 0, int imgLLYA = 0, int imgURXA = 0, int imgURYA = 0, bool manualCtrlA = false, PSOutCustomCodeCbk customCodeCbkA = nullptr, void* customCodeCbkDataA = nullptr, bool honorUserUnitA = false);

	// Destructor -- writes the trailer and closes the file.
	virtual ~PSOutputDev();

	// Check if file was successfully created.
	virtual bool isOk() { return ok; }

	// Returns false if there have been any errors on the output stream.
	bool checkIO();

	//---- get info about output device

	// Does this device use upside-down coordinates?
	// (Upside-down means (0,0) is the top left corner of the page.)
	virtual bool upsideDown() { return false; }

	// Does this device use drawChar() or drawString()?
	virtual bool useDrawChar() { return false; }

	// Does this device use tilingPatternFill()?  If this returns false,
	// tiling pattern fills will be reduced to a series of other drawing
	// operations.
	virtual bool useTilingPatternFill() { return true; }

	// Does this device use drawForm()?  If this returns false,
	// form-type XObjects will be interpreted (i.e., unrolled).
	virtual bool useDrawForm() { return preload; }

	// Does this device use beginType3Char/endType3Char?  Otherwise,
	// text in Type 3 fonts will be drawn with drawChar/drawString.
	virtual bool interpretType3Chars() { return false; }

	//----- header/trailer (used only if manualCtrl is true)

	// Write the document-level header.
	void writeHeader(PDFRectangle* mediaBox, PDFRectangle* cropBox, int pageRotate);

	// Write the Xpdf procset.
	void writeXpdfProcset();

	// Write the document-level setup.
	void writeDocSetup(Catalog* catalog);

	// Write the trailer for the current page.
	void writePageTrailer();

	// Write the document trailer.
	void writeTrailer();

	//----- initialization and control

	// Check to see if a page slice should be displayed.  If this
	// returns false, the page display is aborted.  Typically, an
	// OutputDev will use some alternate means to display the page
	// before returning false.
	virtual bool checkPageSlice(Page* page, double hDPI, double vDPI, int rotate, bool useMediaBox, bool crop, int sliceX, int sliceY, int sliceW, int sliceH, bool printing, bool (*abortCheckCbk)(void* data) = nullptr, void* abortCheckCbkData = nullptr);

	// Start a page.
	virtual void startPage(int pageNum, GfxState* state);

	// End a page.
	virtual void endPage();

	//----- save/restore graphics state
	virtual void saveState(GfxState* state);
	virtual void restoreState(GfxState* state);

	//----- update graphics state
	virtual void updateCTM(GfxState* state, double m11, double m12, double m21, double m22, double m31, double m32);
	virtual void updateLineDash(GfxState* state);
	virtual void updateFlatness(GfxState* state);
	virtual void updateLineJoin(GfxState* state);
	virtual void updateLineCap(GfxState* state);
	virtual void updateMiterLimit(GfxState* state);
	virtual void updateLineWidth(GfxState* state);
	virtual void updateFillColorSpace(GfxState* state);
	virtual void updateStrokeColorSpace(GfxState* state);
	virtual void updateFillColor(GfxState* state);
	virtual void updateStrokeColor(GfxState* state);
	virtual void updateFillOverprint(GfxState* state);
	virtual void updateStrokeOverprint(GfxState* state);
	virtual void updateOverprintMode(GfxState* state);
	virtual void updateTransfer(GfxState* state);

	//----- update text state
	virtual void updateFont(GfxState* state);
	virtual void updateTextMat(GfxState* state);
	virtual void updateCharSpace(GfxState* state);
	virtual void updateRender(GfxState* state);
	virtual void updateRise(GfxState* state);
	virtual void updateWordSpace(GfxState* state);
	virtual void updateHorizScaling(GfxState* state);
	virtual void updateTextPos(GfxState* state);
	virtual void updateTextShift(GfxState* state, double shift);

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
	virtual void drawString(GfxState* state, const std::string& s, bool fill, bool stroke, bool makePath);
	virtual void fillTextPath(GfxState* state);
	virtual void strokeTextPath(GfxState* state);
	virtual void clipToTextPath(GfxState* state);
	virtual void clipToTextStrokePath(GfxState* state);
	virtual void clearTextPath(GfxState* state);
	virtual void addTextPathToSavedClipPath(GfxState* state);
	virtual void clipToSavedClipPath(GfxState* state);
	virtual void endTextObject(GfxState* state);

	//----- image drawing
	virtual void drawImageMask(GfxState* state, Object* ref, Stream* str, int width, int height, bool invert, bool inlineImg, bool interpolate);
	virtual void drawImage(GfxState* state, Object* ref, Stream* str, int width, int height, GfxImageColorMap* colorMap, int* maskColors, bool inlineImg, bool interpolate);
	virtual void drawMaskedImage(GfxState* state, Object* ref, Stream* str, int width, int height, GfxImageColorMap* colorMap, Object* maskRef, Stream* maskStr, int maskWidth, int maskHeight, bool maskInvert, bool interpolate);

#if OPI_SUPPORT
	//----- OPI functions
	virtual void opiBegin(GfxState* state, Dict* opiDict);
	virtual void opiEnd(GfxState* state, Dict* opiDict);
#endif

	//----- Type 3 font operators
	virtual void type3D0(GfxState* state, double wx, double wy);
	virtual void type3D1(GfxState* state, double wx, double wy, double llx, double lly, double urx, double ury);

	//----- form XObjects
	virtual void drawForm(Ref ref);

	//----- PostScript XObjects
	virtual void psXObject(Stream* psStream, Stream* level1Stream);

	//----- miscellaneous
	void setImageableArea(int imgLLXA, int imgLLYA, int imgURXA, int imgURYA)
	{
		imgLLX = imgLLXA;
		imgLLY = imgLLYA;
		imgURX = imgURXA;
		imgURY = imgURYA;
	}

	void setOffset(double x, double y)
	{
		tx0 = x;
		ty0 = y;
	}

	void setScale(double x, double y)
	{
		xScale0 = x;
		yScale0 = y;
	}

	void setRotate(int rotateA)
	{
		rotate0 = rotateA;
	}

	void setClip(double llx, double lly, double urx, double ury)
	{
		clipLLX0 = llx;
		clipLLY0 = lly;
		clipURX0 = urx;
		clipURY0 = ury;
	}

	void setExpandSmallPages(bool expand)
	{
		expandSmallPages = expand;
	}

	void setUnderlayCbk(void (*cbk)(PSOutputDev* psOut, void* data), void* data)
	{
		underlayCbk     = cbk;
		underlayCbkData = data;
	}

	void setOverlayCbk(void (*cbk)(PSOutputDev* psOut, void* data), void* data)
	{
		overlayCbk     = cbk;
		overlayCbkData = data;
	}

	void writePSChar(char c);
	void writePSBlock(const char* s, size_t len);
	void writePS(const char* s);

	template <typename... T>
	void writePSFmt(fmt::format_string<T...> fmt, T&&... args);

	void writePSString(std::string_view sv);
	void writePSName(const char* s);

private:
	void            init(PSOutputFunc outputFuncA, void* outputStreamA, PSFileType fileTypeA, PDFDoc* docA, int firstPageA, int lastPageA, PSOutMode modeA, int imgLLXA, int imgLLYA, int imgURXA, int imgURYA, bool manualCtrlA, bool honorUserUnitA);
	bool            checkIfPageNeedsToBeRasterized(int pg);
	void            setupResources(Dict* resDict);
	void            setupFonts(Dict* resDict);
	void            setupFont(GfxFont* font, Dict* parentResDict);
	sPSFontFileInfo setupEmbeddedType1Font(GfxFont* font, Ref* id);
	sPSFontFileInfo setupExternalType1Font(GfxFont* font, const std::string& fileName);
	sPSFontFileInfo setupEmbeddedType1CFont(GfxFont* font, Ref* id);
	sPSFontFileInfo setupEmbeddedOpenTypeT1CFont(GfxFont* font, Ref* id);
	sPSFontFileInfo setupExternalOpenTypeT1CFont(GfxFont* font, const std::string& fileName);
	sPSFontFileInfo setupEmbeddedTrueTypeFont(GfxFont* font, Ref* id);
	sPSFontFileInfo setupExternalTrueTypeFont(GfxFont* font, const std::string& fileName, int fontNum);
	sPSFontFileInfo setupEmbeddedCIDType0Font(GfxFont* font, Ref* id);
	sPSFontFileInfo setupEmbeddedCIDTrueTypeFont(GfxFont* font, Ref* id, bool needVerticalMetrics);
	sPSFontFileInfo setupExternalCIDTrueTypeFont(GfxFont* font, const std::string& fileName, int fontNum, bool needVerticalMetrics);
	sPSFontFileInfo setupEmbeddedOpenTypeCFFFont(GfxFont* font, Ref* id);
	sPSFontFileInfo setupExternalOpenTypeCFFFont(GfxFont* font, const std::string& fileName);
	sPSFontFileInfo setupType3Font(GfxFont* font, Dict* parentResDict);
	std::string     makePSFontName(GfxFont* font, Ref* id);
	std::string     fixType1Font(const std::string& font, int length1, int length2);
	bool            splitType1PFA(uint8_t* font, int fontSize, int length1, int length2, std::string& textSection, std::string& binSection);
	bool            splitType1PFB(uint8_t* font, int fontSize, std::string& textSection, std::string& binSection);
	std::string     asciiHexDecodeType1EexecSection(const std::string& in);
	bool            fixType1EexecSection(std::string& binSection, std::string& out);
	std::string     copyType1PFA(uint8_t* font, int fontSize);
	std::string     copyType1PFB(uint8_t* font, int fontSize);
	void            renameType1Font(std::string& font, const std::string& name);
	void            setupDefaultFont();
	void            setupImages(Dict* resDict);
	void            setupImage(Ref id, Stream* str, bool mask, Array* colorKeyMask);
	void            setupForms(Dict* resDict);
	void            setupForm(Object* strRef, Object* strObj);
	void            addProcessColor(double c, double m, double y, double k);
	void            addCustomColor(GfxState* state, GfxSeparationColorSpace* sepCS);
	void            addCustomColors(GfxState* state, GfxDeviceNColorSpace* devnCS);
	void            tilingPatternFillL1(GfxState* state, Gfx* gfx, Object* strRef, int paintType, int tilingType, Dict* resDict, double* mat, double* bbox, int x0, int y0, int x1, int y1, double xStep, double yStep);
	void            tilingPatternFillL2(GfxState* state, Gfx* gfx, Object* strRef, int paintType, int tilingType, Dict* resDict, double* mat, double* bbox, int x0, int y0, int x1, int y1, double xStep, double yStep);
	bool            functionShadedFill(GfxState* state, GfxFunctionShading* shading);
	bool            axialShadedFill(GfxState* state, GfxAxialShading* shading);
	bool            radialShadedFill(GfxState* state, GfxRadialShading* shading);
	void            doPath(GfxPath* path);
	void            doImageL1(Object* ref, GfxState* state, GfxImageColorMap* colorMap, bool invert, bool inlineImg, Stream* str, int width, int height, int len);
	void            doImageL1Sep(GfxState* state, GfxImageColorMap* colorMap, bool invert, bool inlineImg, Stream* str, int width, int height, int len);
	void            doImageL2(Object* ref, GfxState* state, GfxImageColorMap* colorMap, bool invert, bool inlineImg, Stream* str, int width, int height, int len, int* maskColors, Stream* maskStr, int maskWidth, int maskHeight, bool maskInvert);
	void            convertColorKeyMaskToClipRects(GfxImageColorMap* colorMap, Stream* str, int width, int height, int* maskColors);
	void            convertExplicitMaskToClipRects(Stream* maskStr, int maskWidth, int maskHeight, bool maskInvert);
	void            doImageL3(Object* ref, GfxState* state, GfxImageColorMap* colorMap, bool invert, bool inlineImg, Stream* str, int width, int height, int len, int* maskColors, Stream* maskStr, int maskWidth, int maskHeight, bool maskInvert);
	void            dumpColorSpaceL2(GfxState* state, GfxColorSpace* colorSpace, bool genXform, bool updateColors, bool map01);
	void            dumpDeviceGrayColorSpace(GfxDeviceGrayColorSpace* cs, bool genXform, bool updateColors, bool map01);
	void            dumpCalGrayColorSpace(GfxCalGrayColorSpace* cs, bool genXform, bool updateColors, bool map01);
	void            dumpDeviceRGBColorSpace(GfxDeviceRGBColorSpace* cs, bool genXform, bool updateColors, bool map01);
	void            dumpCalRGBColorSpace(GfxCalRGBColorSpace* cs, bool genXform, bool updateColors, bool map01);
	void            dumpDeviceCMYKColorSpace(GfxDeviceCMYKColorSpace* cs, bool genXform, bool updateColors, bool map01);
	void            dumpLabColorSpace(GfxLabColorSpace* cs, bool genXform, bool updateColors, bool map01);
	void            dumpICCBasedColorSpace(GfxState* state, GfxICCBasedColorSpace* cs, bool genXform, bool updateColors, bool map01);
	void            dumpIndexedColorSpace(GfxState* state, GfxIndexedColorSpace* cs, bool genXform, bool updateColors, bool map01);
	void            dumpSeparationColorSpace(GfxState* state, GfxSeparationColorSpace* cs, bool genXform, bool updateColors, bool map01);
	void            dumpDeviceNColorSpaceL2(GfxState* state, GfxDeviceNColorSpace* cs, bool genXform, bool updateColors, bool map01);
	void            dumpDeviceNColorSpaceL3(GfxState* state, GfxDeviceNColorSpace* cs, bool genXform, bool updateColors, bool map01);
	std::string     createDeviceNTintFunc(GfxDeviceNColorSpace* cs);
#if OPI_SUPPORT
	void opiBegin20(GfxState* state, Dict* dict);
	void opiBegin13(GfxState* state, Dict* dict);
	void opiTransform(GfxState* state, double x0, double y0, double* x1, double* y1);
	bool getFileSpec(Object* fileSpec, Object* fileName);
#endif
	void        cvtFunction(Function* func);
	std::string filterPSName(const std::string& name);
	void        writePSTextLine(const std::string& s);

	void        (*underlayCbk)(PSOutputDev* psOut, void* data)                                       = nullptr;
	void*       underlayCbkData                                                                      = nullptr;
	void        (*overlayCbk)(PSOutputDev* psOut, void* data)                                        = nullptr;
	void*       overlayCbkData                                                                       = nullptr;
	std::string (*customCodeCbk)(PSOutputDev* psOut, PSOutCustomCodeLocation loc, int n, void* data) = nullptr;
	void*       customCodeCbkData                                                                    = nullptr;

	PSLevel                            level             = psLevel2; // PostScript level
	PSOutMode                          mode              = psModePS; // PostScript mode (PS, EPS, form)
	int                                paperWidth        = 0;        // width of paper, in pts
	int                                paperHeight       = 0;        // height of paper, in pts
	bool                               paperMatch        = false;    // true if paper size is set to match each page
	int                                imgLLX            = 0;        // imageable area, in pts
	int                                imgLLY            = 0;        //
	int                                imgURX            = 0;        //
	int                                imgURY            = 0;        //
	bool                               preload           = false;    // load all images into memory, and predefine forms
	PSOutputFunc                       outputFunc        = nullptr;  //
	void*                              outputStream      = nullptr;  //
	PSFileType                         fileType          = psFile;   // file / pipe / stdout
	bool                               manualCtrl        = false;    //
	int                                seqPage           = 0;        // current sequential page number
	bool                               honorUserUnit     = false;    //
	PDFDoc*                            doc               = nullptr;  //
	XRef*                              xref              = nullptr;  // the xref table for this PDF file
	int                                firstPage         = 1;        // first output page
	int                                lastPage          = 0;        // last output page
	char*                              rasterizePage     = nullptr;  // boolean for each page - true if page needs to be rasterized
	GList*                             fontInfo          = nullptr;  // info for each font [PSFontInfo]
	UMAP<std::string, sPSFontFileInfo> fontFileInfo      = {};       // info for each font file [PSFontFileInfo]
	Ref*                               imgIDs            = nullptr;  // list of image IDs for in-memory images
	int                                imgIDLen          = 0;        // number of entries in imgIDs array
	int                                imgIDSize         = 0;        // size of imgIDs array
	Ref*                               formIDs           = nullptr;  // list of IDs for predefined forms
	int                                formIDLen         = 0;        // number of entries in formIDs array
	int                                formIDSize        = 0;        // size of formIDs array
	char*                              visitedResources  = nullptr;  // vector of resource objects already visited
	bool                               noStateChanges    = false;    // true if there have been no state changes since the last save
	GList*                             saveStack         = nullptr;  // "no state changes" flag for each pending save
	int                                numTilingPatterns = 0;        // current number of nested tiling patterns
	int                                nextFunc          = 0;        // next unique number to use for a function
	GList*                             paperSizes        = nullptr;  // list of used paper sizes, if paperMatch is true [PSOutPaperSize]
	double                             tx0               = -1;       //
	double                             ty0               = -1;       // global translation
	double                             xScale0           = 0;        //
	double                             yScale0           = 0;        // global scaling
	int                                rotate0           = -1;       // rotation angle (0, 90, 180, 270)
	double                             clipLLX0          = 0;        //
	double                             clipLLY0          = 0;        //
	double                             clipURX0          = -1;       //
	double                             clipURY0          = -1;       //
	bool                               expandSmallPages  = false;    // expand smaller pages to fill paper
	double                             tx                = 0;        //
	double                             ty                = 0;        // global translation for current page
	double                             xScale            = 0;        //
	double                             yScale            = 0;        // global scaling for current page
	int                                rotate            = 0;        // rotation angle for current page
	double                             epsX1             = 0;        //
	double                             epsY1             = 0;        // EPS bounding box (unrotated)
	double                             epsX2             = 0;        //
	double                             epsY2             = 0;        //
	std::string                        embFontList       = "";       // resource comments for embedded fonts
	int                                processColors     = 0;        // used process colors
	PSOutCustomColor*                  customColors      = nullptr;  // used custom colors
	bool                               haveSavedTextPath = false;    // set if text has been drawn with the 'makePath' argument
	bool                               haveSavedClipPath = false;    // set if the text path has been added to the saved clipping path (with addTextPathToSavedClipPath)
	bool                               inType3Char       = false;    // inside a Type 3 CharProc
	std::string                        t3String          = "";       // Type 3 content string
	double                             t3WX              = 0;        //
	double                             t3WY              = 0;        // Type 3 character parameters
	double                             t3LLX             = 0;        //
	double                             t3LLY             = 0;        //
	double                             t3URX             = 0;        //
	double                             t3URY             = 0;        //
	bool                               t3FillColorOnly   = false;    // operators should only use the fill color
	bool                               t3Cacheable       = false;    // cleared if char is not cacheable
	bool                               t3NeedsRestore    = false;    // set if a 'q' operator was issued
	bool                               ok                = false;    // set up ok?

#if OPI_SUPPORT
	int opi13Nest = 0; // nesting level of OPI 1.3 objects
	int opi20Nest = 0; // nesting level of OPI 2.0 objects
#endif

	friend class WinPDFPrinter;
};
