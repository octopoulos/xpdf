//========================================================================
//
// GlobalParams.h
//
// Copyright 2001-2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include <stdio.h>
#ifdef _WIN32
#	include <windows.h>
#endif
#include "CharTypes.h"
#include "UnicodeMap.h"

#if MULTITHREADED
#	include "GMutex.h"
#endif

class GList;
class GHash;
class NameToCharCode;
class CharCodeToUnicode;
class CharCodeToUnicodeCache;
class UnicodeMapCache;
class UnicodeRemapping;
class CMap;
class CMapCache;
struct XpdfSecurityHandler;
class GlobalParams;
class SysFontList;

//------------------------------------------------------------------------

// The global parameters object.
extern std::shared_ptr<GlobalParams> globalParams;

//------------------------------------------------------------------------

enum SysFontType
{
	sysFontPFA,
	sysFontPFB,
	sysFontTTF,
	sysFontTTC,
	sysFontOTF
};

//------------------------------------------------------------------------

class PSFontParam16
{
public:
	std::string name;       // PDF font name for psResidentFont16; char collection name for psResidentFontCC
	int         wMode;      // writing mode (0=horiz, 1=vert)
	std::string psFontName; // PostScript font name
	std::string encoding;   // encoding

	PSFontParam16(const std::string& nameA, int wModeA, const std::string& psFontNameA, const std::string& encodingA);
	~PSFontParam16();
};

//------------------------------------------------------------------------

enum PSLevel
{
	psLevel1,
	psLevel1Sep,
	psLevel2,
	psLevel2Gray,
	psLevel2Sep,
	psLevel3,
	psLevel3Gray,
	psLevel3Sep,
};

//------------------------------------------------------------------------

enum EndOfLineKind
{
	eolUnix, // LF
	eolDOS,  // CR+LF
	eolMac   // CR
};

//------------------------------------------------------------------------

enum StrokeAdjustMode
{
	strokeAdjustOff,
	strokeAdjustNormal,
	strokeAdjustCAD
};

//------------------------------------------------------------------------

enum ScreenType
{
	screenUnset,
	screenDispersed,
	screenClustered,
	screenStochasticClustered
};

//------------------------------------------------------------------------

struct Base14FontInfo
{
	Base14FontInfo(const std::string& fileNameA, int fontNumA, double obliqueA)
	{
		fileName = fileNameA;
		fontNum  = fontNumA;
		oblique  = obliqueA;
	}

	std::string fileName;
	int         fontNum;
	double      oblique;
};

class KeyBinding
{
public:
	int     code;    // 0x20 .. 0xfe = ASCII, >=0x10000 = special keys, mouse buttons, etc. (xpdfKeyCode* symbols)
	int     mods;    // modifiers (xpdfKeyMod* symbols, or-ed together)
	int     context; // context (xpdfKeyContext* symbols, or-ed together)
	VEC_STR cmds;    // list of commands [GString]

	KeyBinding(int codeA, int modsA, int contextA, const char* cmd0);
	KeyBinding(int codeA, int modsA, int contextA, const char* cmd0, const char* cmd1);
	KeyBinding(int codeA, int modsA, int contextA, const VEC_STR& cmdsA);
	~KeyBinding();
};

#define xpdfKeyCodeTab                0x1000
#define xpdfKeyCodeReturn             0x1001
#define xpdfKeyCodeEnter              0x1002
#define xpdfKeyCodeBackspace          0x1003
#define xpdfKeyCodeEsc                0x1004
#define xpdfKeyCodeInsert             0x1005
#define xpdfKeyCodeDelete             0x1006
#define xpdfKeyCodeHome               0x1007
#define xpdfKeyCodeEnd                0x1008
#define xpdfKeyCodePgUp               0x1009
#define xpdfKeyCodePgDn               0x100a
#define xpdfKeyCodeLeft               0x100b
#define xpdfKeyCodeRight              0x100c
#define xpdfKeyCodeUp                 0x100d
#define xpdfKeyCodeDown               0x100e
#define xpdfKeyCodeF1                 0x1100
#define xpdfKeyCodeF35                0x1122
#define xpdfKeyCodeMousePress1        0x2001
#define xpdfKeyCodeMousePress2        0x2002
#define xpdfKeyCodeMousePress3        0x2003
#define xpdfKeyCodeMousePress4        0x2004
#define xpdfKeyCodeMousePress5        0x2005
#define xpdfKeyCodeMousePress6        0x2006
#define xpdfKeyCodeMousePress7        0x2007
// ...
#define xpdfKeyCodeMousePress32       0x2020
#define xpdfKeyCodeMouseRelease1      0x2101
#define xpdfKeyCodeMouseRelease2      0x2102
#define xpdfKeyCodeMouseRelease3      0x2103
#define xpdfKeyCodeMouseRelease4      0x2104
#define xpdfKeyCodeMouseRelease5      0x2105
#define xpdfKeyCodeMouseRelease6      0x2106
#define xpdfKeyCodeMouseRelease7      0x2107
// ...
#define xpdfKeyCodeMouseRelease32     0x2120
#define xpdfKeyCodeMouseClick1        0x2201
#define xpdfKeyCodeMouseClick2        0x2202
#define xpdfKeyCodeMouseClick3        0x2203
#define xpdfKeyCodeMouseClick4        0x2204
#define xpdfKeyCodeMouseClick5        0x2205
#define xpdfKeyCodeMouseClick6        0x2206
#define xpdfKeyCodeMouseClick7        0x2207
// ...
#define xpdfKeyCodeMouseClick32       0x2220
#define xpdfKeyCodeMouseDoubleClick1  0x2301
#define xpdfKeyCodeMouseDoubleClick2  0x2302
#define xpdfKeyCodeMouseDoubleClick3  0x2303
#define xpdfKeyCodeMouseDoubleClick4  0x2304
#define xpdfKeyCodeMouseDoubleClick5  0x2305
#define xpdfKeyCodeMouseDoubleClick6  0x2306
#define xpdfKeyCodeMouseDoubleClick7  0x2307
// ...
#define xpdfKeyCodeMouseDoubleClick32 0x2320
#define xpdfKeyCodeMouseTripleClick1  0x2401
#define xpdfKeyCodeMouseTripleClick2  0x2402
#define xpdfKeyCodeMouseTripleClick3  0x2403
#define xpdfKeyCodeMouseTripleClick4  0x2404
#define xpdfKeyCodeMouseTripleClick5  0x2405
#define xpdfKeyCodeMouseTripleClick6  0x2406
#define xpdfKeyCodeMouseTripleClick7  0x2407
// ...
#define xpdfKeyCodeMouseTripleClick32 0x2420
#define xpdfKeyModNone                0
#define xpdfKeyModShift               (1 << 0)
#define xpdfKeyModCtrl                (1 << 1)
#define xpdfKeyModAlt                 (1 << 2)
#define xpdfKeyContextAny             0
#define xpdfKeyContextFullScreen      (1 << 0)
#define xpdfKeyContextWindow          (2 << 0)
#define xpdfKeyContextContinuous      (1 << 2)
#define xpdfKeyContextSinglePage      (2 << 2)
#define xpdfKeyContextOverLink        (1 << 4)
#define xpdfKeyContextOffLink         (2 << 4)
#define xpdfKeyContextOutline         (1 << 6)
#define xpdfKeyContextMainWin         (2 << 6)
#define xpdfKeyContextScrLockOn       (1 << 8)
#define xpdfKeyContextScrLockOff      (2 << 8)

//------------------------------------------------------------------------

class PopupMenuCmd
{
public:
	std::string label; // label for display in the menu
	VEC_STR     cmds;  // list of commands [GString]

	PopupMenuCmd(const std::string& labelA, const VEC_STR& cmdsA);
	~PopupMenuCmd();
};

//------------------------------------------------------------------------

#ifdef _WIN32
struct XpdfWin32ErrorInfo
{
	const char* func; // last Win32 API function call to fail
	DWORD       code; // error code returned by that function
};
#endif

//------------------------------------------------------------------------

class GlobalParams
{
private:
	std::mutex mtx;

public:
	// Initialize the global parameters by attempting to read a config file.
	GlobalParams(const char* cfgFileName);

	~GlobalParams();

	void setBaseDir(const char* dir);
	void setupBaseFonts(const char* dir);

	void parseLine(char* buf, const std::string& fileName, int line);

	//----- accessors

	CharCode getMacRomanCharCode(char* charName);

	std::string           getBaseDir();
	Unicode               mapNameToUnicode(const char* charName);
	UnicodeMap*           getResidentUnicodeMap(const std::string& encodingName);
	FILE*                 getUnicodeMapFile(const std::string& encodingName);
	FILE*                 findCMapFile(const std::string& collection, const std::string& cMapName);
	FILE*                 findToUnicodeFile(const std::string& name);
	UnicodeRemapping*     getUnicodeRemapping();
	std::filesystem::path findFontFile(const std::string& fontName);
	std::filesystem::path findBase14FontFile(const std::string& fontName, int* fontNum, double* oblique);
	std::filesystem::path findSystemFontFile(const std::string& fontName, SysFontType* type, int* fontNum);
	std::string           findCCFontFile(const std::string& collection);
	int                   getPSPaperWidth();
	int                   getPSPaperHeight();
	void                  getPSImageableArea(int* llx, int* lly, int* urx, int* ury);
	bool                  getPSDuplex();
	bool                  getPSCrop();
	bool                  getPSUseCropBoxAsPage();
	bool                  getPSExpandSmaller();
	bool                  getPSShrinkLarger();
	bool                  getPSCenter();
	PSLevel               getPSLevel();
	std::string           getPSResidentFont(const std::string& fontName);
	VEC_STR               getPSResidentFonts();
	PSFontParam16*        getPSResidentFont16(const std::string& fontName, int wMode);
	PSFontParam16*        getPSResidentFontCC(const std::string& collection, int wMode);
	bool                  getPSEmbedType1();
	bool                  getPSEmbedTrueType();
	bool                  getPSEmbedCIDPostScript();
	bool                  getPSEmbedCIDTrueType();
	bool                  getPSFontPassthrough();
	bool                  getPSPreload();
	bool                  getPSOPI();
	bool                  getPSASCIIHex();
	bool                  getPSLZW();
	bool                  getPSUncompressPreloadedImages();
	double                getPSMinLineWidth();
	double                getPSRasterResolution();
	bool                  getPSRasterMono();
	int                   getPSRasterSliceSize();
	bool                  getPSAlwaysRasterize();
	bool                  getPSNeverRasterize();
	std::string           getTextEncodingName();
	VEC_STR               getAvailableTextEncodings();
	EndOfLineKind         getTextEOL();
	bool                  getTextPageBreaks();
	bool                  getTextKeepTinyChars();
	std::string           getInitialZoom();
	int                   getDefaultFitZoom();
	double                getZoomScaleFactor();
	VEC_STR               getZoomValues();
	std::string           getInitialDisplayMode();
	bool                  getInitialToolbarState();
	bool                  getInitialSidebarState();
	int                   getInitialSidebarWidth();
	std::string           getInitialSelectMode();
	int                   getMaxTileWidth();
	int                   getMaxTileHeight();
	int                   getTileCacheSize();
	int                   getWorkerThreads();
	bool                  getEnableFreeType();
	bool                  getDisableFreeTypeHinting();
	bool                  getAntialias();
	bool                  getVectorAntialias();
	bool                  getImageMaskAntialias();
	bool                  getAntialiasPrinting();
	StrokeAdjustMode      getStrokeAdjust();
	ScreenType            getScreenType();
	int                   getScreenSize();
	int                   getScreenDotRadius();
	double                getScreenGamma();
	double                getScreenBlackThreshold();
	double                getScreenWhiteThreshold();
	double                getMinLineWidth();
	bool                  getEnablePathSimplification();
	bool                  getDrawAnnotations();
	bool                  getDrawFormFields();
	bool                  getEnableXFA();

	bool getOverprintPreview() { return overprintPreview; }

	std::string getPaperColor();
	std::string getMatteColor();
	std::string getFullScreenMatteColor();
	std::string getSelectionColor();
	bool        getReverseVideoInvertImages();
	bool        getAllowLinksToChangeZoom();

	std::string getLaunchCommand() { return launchCommand; }

	std::string getMovieCommand() { return movieCommand; }

	std::string             getDefaultPrinter();
	bool                    getMapNumericCharNames();
	bool                    getMapUnknownCharNames();
	bool                    getMapExtTrueTypeFontsViaUnicode();
	bool                    getUseTrueTypeUnicodeMapping();
	bool                    getIgnoreWrongSizeToUnicode();
	bool                    isDroppedFont(const char* fontName);
	bool                    getSeparateRotatedText();
	VEC_STR                 getKeyBinding(int code, int mods, int context);
	std::vector<KeyBinding> getAllKeyBindings();
	int                     getNumPopupMenuCmds();
	PopupMenuCmd*           getPopupMenuCmd(int idx);
	std::string             getPagesFile();
	std::string             getTabStateFile();
	std::string             getSessionFile();
	bool                    getSaveSessionOnQuit();
	bool                    getSavePageNumbers();
	bool                    getPrintCommands();
	bool                    getPrintStatusInfo();
	bool                    getErrQuiet();
	std::string             getDebugLogFile();
	void                    debugLogPrintf(const char* fmt, ...);

	CharCodeToUnicode* getCIDToUnicode(const std::string& collection);
	CharCodeToUnicode* getUnicodeToUnicode(const std::string& fontName);
	UnicodeMap*        getUnicodeMap(const std::string& encodingName);
	CMap*              getCMap(const std::string& collection, const std::string& cMapName);
	UnicodeMap*        getTextEncoding();

	//----- functions to set parameters

	void addUnicodeRemapping(Unicode in, Unicode* out, int len);
	void addFontFile(const std::string& fontName, const std::string& path);
	bool setPSPaperSize(const char* size);
	void setPSPaperWidth(int width);
	void setPSPaperHeight(int height);
	void setPSImageableArea(int llx, int lly, int urx, int ury);
	void setPSDuplex(bool duplex);
	void setPSCrop(bool crop);
	void setPSUseCropBoxAsPage(bool crop);
	void setPSExpandSmaller(bool expand);
	void setPSShrinkLarger(bool shrink);
	void setPSCenter(bool center);
	void setPSLevel(PSLevel level);
	void setPSEmbedType1(bool embed);
	void setPSEmbedTrueType(bool embed);
	void setPSEmbedCIDPostScript(bool embed);
	void setPSEmbedCIDTrueType(bool embed);
	void setPSFontPassthrough(bool passthrough);
	void setPSPreload(bool preload);
	void setPSOPI(bool opi);
	void setPSASCIIHex(bool hex);
	void setTextEncoding(const char* encodingName);
	bool setTextEOL(const char* s);
	void setTextPageBreaks(bool pageBreaks);
	void setTextKeepTinyChars(bool keep);
	void setInitialZoom(const char* s);
	void setDefaultFitZoom(int z);
	bool setEnableFreeType(const char* s);
	bool setAntialias(const char* s);
	bool setVectorAntialias(const char* s);
	void setScreenType(ScreenType t);
	void setScreenSize(int size);
	void setScreenDotRadius(int r);
	void setScreenGamma(double gamma);
	void setScreenBlackThreshold(double thresh);
	void setScreenWhiteThreshold(double thresh);
	void setDrawFormFields(bool draw);
	void setOverprintPreview(bool preview);
	void setMapNumericCharNames(bool map);
	void setMapUnknownCharNames(bool map);
	void setMapExtTrueTypeFontsViaUnicode(bool map);
	void setTabStateFile(const char* tabStateFileA);
	void setSessionFile(const char* sessionFileA);
	void setPrintCommands(bool printCommandsA);
	void setPrintStatusInfo(bool printStatusInfoA);
	void setErrQuiet(bool errQuietA);

#ifdef _WIN32
	void                setWin32ErrorInfo(const char* func, DWORD code);
	XpdfWin32ErrorInfo* getWin32ErrorInfo();
#endif

	static const char* defaultTextEncoding;

private:
	void        setDataDirVar();
	void        createDefaultKeyBindings();
	void        initStateFilePaths();
	void        parseFile(const std::string& fileName, FILE* f);
	VEC_STR     parseLineTokens(char* buf, const std::string& fileName, int line);
	void        parseNameToUnicode(const VEC_STR& tokens, const std::string& fileName, int line);
	void        parseCIDToUnicode(const VEC_STR& tokens, const std::string& fileName, int line);
	void        parseUnicodeToUnicode(const VEC_STR& tokens, const std::string& fileName, int line);
	void        parseUnicodeMap(const VEC_STR& tokens, const std::string& fileName, int line);
	void        parseCMapDir(const VEC_STR& tokens, const std::string& fileName, int line);
	void        parseToUnicodeDir(const VEC_STR& tokens, const std::string& fileName, int line);
	void        parseUnicodeRemapping(const VEC_STR& tokens, const std::string& fileName, int line);
	void        parseFontFile(const VEC_STR& tokens, const std::string& fileName, int line);
	void        parseFontDir(const VEC_STR& tokens, const std::string& fileName, int line);
	void        parseFontFileCC(const VEC_STR& tokens, const std::string& fileName, int line);
	void        parsePSPaperSize(const VEC_STR& tokens, const std::string& fileName, int line);
	void        parsePSImageableArea(const VEC_STR& tokens, const std::string& fileName, int line);
	void        parsePSLevel(const VEC_STR& tokens, const std::string& fileName, int line);
	void        parsePSResidentFont(const VEC_STR& tokens, const std::string& fileName, int line);
	void        parsePSResidentFont16(const VEC_STR& tokens, const std::string& fileName, int line);
	void        parsePSResidentFontCC(const VEC_STR& tokens, const std::string& fileName, int line);
	void        parseTextEOL(const VEC_STR& tokens, const std::string& fileName, int line);
	void        parseStrokeAdjust(const VEC_STR& tokens, const std::string& fileName, int line);
	void        parseScreenType(const VEC_STR& tokens, const std::string& fileName, int line);
	void        parseDropFont(const VEC_STR& tokens, const std::string& fileName, int line);
	void        parseBind(const VEC_STR& tokens, const std::string& fileName, int line);
	void        parseUnbind(const VEC_STR& tokens, const std::string& fileName, int line);
	bool        parseKey(const std::string& modKeyStr, const std::string& contextStr, int* code, int* mods, int* context, const char* cmdName, const VEC_STR& tokens, const std::string& fileName, int line);
	void        parsePopupMenuCmd(const VEC_STR& tokens, const std::string& fileName, int line);
	void        parseZoomScaleFactor(const VEC_STR& tokens, const std::string& fileName, int line);
	void        parseZoomValues(const VEC_STR& tokens, const std::string& fileName, int line);
	void        parseYesNo(const char* cmdName, std::atomic<bool>* flag, const VEC_STR& tokens, const std::string& fileName, int line);
	bool        parseYesNo2(const char* token, std::atomic<bool>* flag);
	void        parseString(const char* cmdName, std::string* s, const VEC_STR& tokens, const std::string& fileName, int line);
	void        parseInteger(const char* cmdName, std::atomic<int>* val, const VEC_STR& tokens, const std::string& fileName, int line);
	void        parseFloat(const char* cmdName, std::atomic<double>* val, const VEC_STR& tokens, const std::string& fileName, int line);
	UnicodeMap* getUnicodeMap2(const std::string& encodingName);

	//----- static tables

	NameToCharCode*         // mapping from char name to
	    macRomanReverseMap; // MacRomanEncoding index

	//----- meta settings

	std::string  baseDir;        // base directory - for plugins, etc.
	UMAP_STR_STR configFileVars; // variables for use in the config file [GString]

	//----- user-modifiable settings

	NameToCharCode*                   nameToUnicode;                 // mapping from char name to Unicode
	UMAP_STR_STR                      cidToUnicodes;                 // files for mappings from char collections to Unicode, indexed by collection name [GString]
	UMAP_STR_STR                      unicodeToUnicodes;             // files for Unicode-to-Unicode mappings, indexed by font name pattern [GString]
	UMAP<std::string, UnicodeMap>     residentUnicodeMaps;           // mappings from Unicode to char codes, indexed by encoding name [UnicodeMap]
	UMAP_STR_STR                      unicodeMaps;                   // files for mappings from Unicode to char codes, indexed by encoding name [GString]
	UMAP<std::string, VEC_STR>        cMapDirs;                      // list of CMap dirs, indexed by collection name [GList[GString]]
	VEC_STR                           toUnicodeDirs;                 // list of ToUnicode CMap dirs [GString]
	UnicodeRemapping*                 unicodeRemapping;              // Unicode remapping for text output
	UMAP_STR_STR                      fontFiles;                     // font files: font name mapped to path [GString]
	VEC_STR                           fontDirs;                      // list of font dirs [GString]
	UMAP_STR_STR                      ccFontFiles;                   // character collection font files: collection name mapped to path [GString]
	UMAP<std::string, Base14FontInfo> base14SysFonts;                // Base-14 system font files: font name mapped to path [Base14FontInfo]
	SysFontList*                      sysFonts;                      // system fonts
	std::atomic<int>                  psPaperWidth;                  // paper size, in PostScript points, for
	std::atomic<int>                  psPaperHeight;                 // PostScript output
	std::atomic<int>                  psImageableLLX;                // imageable area, in PostScript points,
	std::atomic<int>                  psImageableLLY;                // for PostScript output
	std::atomic<int>                  psImageableURX;                //
	std::atomic<int>                  psImageableURY;                //
	std::atomic<bool>                 psCrop;                        // crop PS output to CropBox
	std::atomic<bool>                 psUseCropBoxAsPage;            // use CropBox as page size
	std::atomic<bool>                 psExpandSmaller;               // expand smaller pages to fill paper
	std::atomic<bool>                 psShrinkLarger;                // shrink larger pages to fit paper
	std::atomic<bool>                 psCenter;                      // center pages on the paper
	std::atomic<bool>                 psDuplex;                      // enable duplexing in PostScript?
	PSLevel                           psLevel;                       // PostScript level to generate
	UMAP_STR_STR                      psResidentFonts;               // 8-bit fonts resident in printer: PDF font name mapped to PS font name [GString]
	std::vector<PSFontParam16>        psResidentFonts16;             // 16-bit fonts resident in printer: PDF font name mapped to font info [PSFontParam16]
	std::vector<PSFontParam16>        psResidentFontsCC;             // 16-bit character collection fonts resident in printer: collection name mapped to font info [PSFontParam16]
	std::atomic<bool>                 psEmbedType1;                  // embed Type 1 fonts?
	std::atomic<bool>                 psEmbedTrueType;               // embed TrueType fonts?
	std::atomic<bool>                 psEmbedCIDPostScript;          // embed CID PostScript fonts?
	std::atomic<bool>                 psEmbedCIDTrueType;            // embed CID TrueType fonts?
	std::atomic<bool>                 psFontPassthrough;             // pass all fonts through as-is?
	std::atomic<bool>                 psPreload;                     // preload PostScript images and forms into memory
	std::atomic<bool>                 psOPI;                         // generate PostScript OPI comments?
	std::atomic<bool>                 psASCIIHex;                    // use ASCIIHex instead of ASCII85?
	std::atomic<bool>                 psLZW;                         // false to use RLE instead of LZW
	std::atomic<bool>                 psUncompressPreloadedImages;   // uncompress all preloaded images
	std::atomic<double>               psMinLineWidth;                // minimum line width for PostScript output
	std::atomic<double>               psRasterResolution;            // PostScript rasterization resolution (dpi)
	std::atomic<bool>                 psRasterMono;                  // true to do PostScript rasterization in monochrome (gray); false to do it in color (RGB/CMYK)
	std::atomic<int>                  psRasterSliceSize;             // maximum size (pixels) of PostScript rasterization slice
	std::atomic<bool>                 psAlwaysRasterize;             // force PostScript rasterization
	std::atomic<bool>                 psNeverRasterize;              // prevent PostScript rasterization
	std::string                       textEncoding;                  // encoding (unicodeMap) to use for text output
	EndOfLineKind                     textEOL;                       // type of EOL marker to use for text output
	std::atomic<bool>                 textPageBreaks;                // insert end-of-page markers?
	std::atomic<bool>                 textKeepTinyChars;             // keep all characters in text output
	std::string                       initialZoom;                   // initial zoom level
	std::atomic<int>                  defaultFitZoom;                // default zoom factor if initialZoom is 'page' or 'width'
	std::atomic<double>               zoomScaleFactor;               // displayed zoom values are scaled by this
	VEC_STR                           zoomValues;                    // zoom values for the combo box
	std::string                       initialDisplayMode;            // initial display mode (single, continuous, etc.)
	std::atomic<bool>                 initialToolbarState;           // initial toolbar state - open (true) or closed (false)
	std::atomic<bool>                 initialSidebarState;           // initial sidebar state - open (true) or closed (false)
	std::atomic<int>                  initialSidebarWidth;           // initial sidebar width
	std::string                       initialSelectMode;             // initial selection mode (block or linear)
	std::atomic<int>                  maxTileWidth;                  // maximum rasterization tile width
	std::atomic<int>                  maxTileHeight;                 // maximum rasterization tile height
	std::atomic<int>                  tileCacheSize;                 // number of rasterization tiles in cache
	std::atomic<int>                  workerThreads;                 // number of rasterization worker threads
	std::atomic<bool>                 enableFreeType;                // FreeType enable flag
	std::atomic<bool>                 disableFreeTypeHinting;        // FreeType hinting disable flag
	std::atomic<bool>                 antialias;                     // font anti-aliasing enable flag
	std::atomic<bool>                 vectorAntialias;               // vector anti-aliasing enable flag
	std::atomic<bool>                 imageMaskAntialias;            // image mask anti-aliasing enable flag
	std::atomic<bool>                 antialiasPrinting;             // allow anti-aliasing when printing
	StrokeAdjustMode                  strokeAdjust;                  // stroke adjustment mode
	ScreenType                        screenType;                    // halftone screen type
	std::atomic<int>                  screenSize;                    // screen matrix size
	std::atomic<int>                  screenDotRadius;               // screen dot radius
	std::atomic<double>               screenGamma;                   // screen gamma correction
	std::atomic<double>               screenBlackThreshold;          // screen black clamping threshold
	std::atomic<double>               screenWhiteThreshold;          // screen white clamping threshold
	std::atomic<double>               minLineWidth;                  // minimum line width
	std::atomic<bool>                 enablePathSimplification;      // enable path simplification
	std::atomic<bool>                 drawAnnotations;               // draw annotations or not
	std::atomic<bool>                 drawFormFields;                // draw form fields or not
	std::atomic<bool>                 enableXFA;                     // enable XFA form parsing
	std::atomic<bool>                 overprintPreview;              // enable overprint preview
	std::string                       paperColor;                    // paper (page background) color
	std::string                       matteColor;                    // matte (background outside of page) color
	std::string                       fullScreenMatteColor;          // matte color in full-screen mode
	std::string                       selectionColor;                // selection color
	std::atomic<bool>                 reverseVideoInvertImages;      // invert images in reverse video mode
	std::atomic<bool>                 allowLinksToChangeZoom;        // allow clicking on a link to change the zoom
	std::string                       launchCommand;                 // command executed for 'launch' links
	std::string                       movieCommand;                  // command executed for movie annotations
	std::string                       defaultPrinter;                // default printer (for interactive printing from the viewer)
	std::atomic<bool>                 mapNumericCharNames;           // map numeric char names (from font subsets)?
	std::atomic<bool>                 mapUnknownCharNames;           // map unknown char names?
	std::atomic<bool>                 mapExtTrueTypeFontsViaUnicode; // map char codes to GID via Unicode for external TrueType fonts?
	std::atomic<bool>                 useTrueTypeUnicodeMapping;     // use the Unicode cmaps in TrueType fonts, rather than the PDF ToUnicode mapping
	std::atomic<bool>                 ignoreWrongSizeToUnicode;      // ignore ToUnicode CMaps if their size (8-bit vs 16-bit) doesn't match the font
	UMAP_STR_INT                      droppedFonts;                  // dropped fonts [int]
	std::atomic<bool>                 separateRotatedText;           // separate text at each rotation
	std::vector<KeyBinding>           keyBindings;                   // key & mouse button bindings [KeyBinding]
	std::vector<PopupMenuCmd>         popupMenuCmds;                 // popup menu commands [PopupMenuCmd]
	std::string                       pagesFile;                     // path for the page number save file
	std::string                       tabStateFile;                  // path for the tab state save file
	std::string                       sessionFile;                   // path for the session save file
	std::atomic<bool>                 saveSessionOnQuit;             // save session info when xpdf is quit
	std::atomic<bool>                 savePageNumbers;               // save page number when file is closed and restore page number when opened
	std::atomic<bool>                 printCommands;                 // print the drawing commands
	std::atomic<bool>                 printStatusInfo;               // print status info for each page
	std::atomic<bool>                 errQuiet;                      // suppress error messages?
	std::string                       debugLogFile;                  // path for debug log file

	CharCodeToUnicodeCache* cidToUnicodeCache;
	CharCodeToUnicodeCache* unicodeToUnicodeCache;
	UnicodeMapCache*        unicodeMapCache;
	CMapCache*              cMapCache;

#if MULTITHREADED
	GMutex mutex;
	GMutex unicodeMapCacheMutex;
	GMutex cMapCacheMutex;
#endif
#ifdef _WIN32
	DWORD tlsWin32ErrorInfo; // TLS index for error info
#endif
};
