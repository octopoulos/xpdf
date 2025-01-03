//========================================================================
//
// GfxFont.h
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "Object.h"
#include "CharTypes.h"

struct Base14FontMapEntry;
class CharCodeToUnicode;
class CMap;
class Dict;
class FNVHash;
class FoFiTrueType;
class FoFiType1C;
struct GfxFontCIDWidths;
class GfxFontDictEntry;
class GHash;
class GList;

//------------------------------------------------------------------------
// GfxFontType
//------------------------------------------------------------------------

enum GfxFontType
{
	//----- Gfx8BitFont
	fontUnknownType,
	fontType1,
	fontType1C,
	fontType1COT,
	fontType3,
	fontTrueType,
	fontTrueTypeOT,
	//----- GfxCIDFont
	fontCIDType0,
	fontCIDType0C,
	fontCIDType0COT,
	fontCIDType2,
	fontCIDType2OT
};

//------------------------------------------------------------------------
// GfxFontCIDWidths
//------------------------------------------------------------------------

struct GfxFontCIDWidthExcep
{
	CID    first = 0; // this record applies to
	CID    last  = 0; // CIDs <first>..<last>
	double width = 0; // char width
};

struct GfxFontCIDWidthExcepV
{
	CID    first  = 0; // this record applies to
	CID    last   = 0; // CIDs <first>..<last>
	double height = 0; // char height
	double vx     = 0; //
	double vy     = 0; // origin position
};

struct GfxFontCIDWidths
{
	double                 defWidth  = 0;       // default char width
	double                 defHeight = 0;       // default char height
	double                 defVY     = 0;       // default origin position
	GfxFontCIDWidthExcep*  exceps    = nullptr; // exceptions
	int                    nExceps   = 0;       // number of valid entries in exceps
	GfxFontCIDWidthExcepV* excepsV   = nullptr; // exceptions for vertical font
	int                    nExcepsV  = 0;       // number of valid entries in excepsV
};

//------------------------------------------------------------------------
// GfxFontLoc
//------------------------------------------------------------------------

enum GfxFontLocType
{
	gfxFontLocEmbedded, // font embedded in PDF file
	gfxFontLocExternal, // external font file
	gfxFontLocResident  // font resident in PS printer
};

class GfxFontLoc
{
public:
	GfxFontLoc();
	~GfxFontLoc();

	GfxFontLocType        locType   = gfxFontLocEmbedded; //
	GfxFontType           fontType  = fontUnknownType;    //
	Ref                   embFontID = {};                 // embedded stream obj ID (if locType == gfxFontLocEmbedded)
	std::filesystem::path path      = {};                 // font file path (if locType == gfxFontLocExternal); PS font name (if locType == gfxFontLocResident)
	int                   fontNum   = 0;                  // for TrueType collections and Mac dfonts (if locType == gfxFontLocExternal)
	double                oblique   = 0;                  // sheer factor to oblique this font (used when substituting a plain font for an oblique font)
	std::string           encoding  = "";                 // PS font encoding, only for 16-bit fonts (if locType == gfxFontLocResident)
	int                   wMode     = 0;                  // writing mode, only for 16-bit fonts (if locType == gfxFontLocResident)
	int                   substIdx  = -1;                 // substitute font index (if locType == gfxFontLocExternal, and a Base-14 substitution was made)
};

//------------------------------------------------------------------------
// GfxFont
//------------------------------------------------------------------------

#define fontFixedWidth (1 << 0)
#define fontSerif      (1 << 1)
#define fontSymbolic   (1 << 2)
#define fontItalic     (1 << 6)
#define fontBold       (1 << 18)

class GfxFont
{
public:
	// Build a GfxFont object.
	static GfxFont* makeFont(XRef* xref, std::string_view tagA, Ref idA, Dict* fontDict);

	// Create a simple default font, to substitute for an undefined font object.
	static GfxFont* makeDefaultFont(XRef* xref);

	GfxFont(std::string_view tagA, Ref idA, std::string_view nameA, GfxFontType typeA, Ref embFontIDA);

	virtual ~GfxFont();

	bool isOk() { return ok; }

	// Get font tag.
	std::string getTag() { return tag; }

	// Get font dictionary ID.
	Ref* getID() { return &id; }

	// Does this font match the tag?
	bool matches(char* tagA) { return tag == tagA; }

	// Get the original font name (ignornig any munging that might have been done to map to a canonical Base-14 font name).
	std::string getName() { return name; }

	// Get font type.
	GfxFontType getType() { return type; }

	virtual bool isCIDFont() { return false; }

	// Get embedded font ID, i.e., a ref for the font file stream.
	// Returns false if there is no embedded font.
	bool getEmbeddedFontID(Ref* embID)
	{
		*embID = embFontID;
		return embFontID.num >= 0;
	}

	// Get the PostScript font name for the embedded font.  Returns
	// nullptr if there is no embedded font.
	std::string getEmbeddedFontName() { return embFontName; }

	// Get font descriptor flags.
	int getFlags() { return flags; }

	bool isFixedWidth() { return flags & fontFixedWidth; }

	bool isSerif() { return flags & fontSerif; }

	bool isSymbolic() { return flags & fontSymbolic; }

	bool isItalic() { return flags & fontItalic; }

	bool isBold() { return flags & fontBold; }

	// Return the font matrix.
	double* getFontMatrix() { return fontMat; }

	// Return the font bounding box.
	double* getFontBBox() { return fontBBox; }

	// Return the ascent and descent values.
	double getAscent() { return ascent; }

	double getDescent() { return descent; }

	double getDeclaredAscent() { return declaredAscent; }

	// Return the writing mode (0=horizontal, 1=vertical).
	virtual int getWMode() { return 0; }

	// Locate the font file for this font.  If <ps> is true, includes PS printer-resident fonts.
	// Returns nullptr on failure.
	GfxFontLoc* locateFont(XRef* xref, bool ps);

	// Locate a Base-14 font file for a specified font name.
	static GfxFontLoc* locateBase14Font(const std::string& base14Name);

	// Read an embedded font file into a buffer.
	char* readEmbFontFile(XRef* xref, int* len);

	// Get the next char from a string <s> of <len> bytes, returning the char <code>, its Unicode mapping <u>, its displacement vector (<dx>, <dy>), and its origin offset vector (<ox>, <oy>).
	// <uSize> is the number of entries available in <u>, and <uLen> is set to the number actually used.  Returns the number of bytes used by the char code.
	virtual int getNextChar(const char* s, int len, CharCode* code, Unicode* u, int uSize, int* uLen, double* dx, double* dy, double* ox, double* oy) = 0;

	// Returns true if this font is likely to be problematic when converting text to Unicode.
	virtual bool problematicForUnicode() = 0;

protected:
	static GfxFontType getFontType(XRef* xref, Dict* fontDict, Ref* embID);
	void               readFontDescriptor(XRef* xref, Dict* fontDict);
	CharCodeToUnicode* readToUnicodeCMap(Dict* fontDict, int nBits, CharCodeToUnicode* ctu);
	static GfxFontLoc* getExternalFont(const std::filesystem::path& path, int fontNum, double oblique, bool cid);

	std::string tag            = "";              // PDF font tag
	Ref         id             = {};              // reference (used as unique ID)
	std::string name           = "";              // font name
	GfxFontType type           = fontUnknownType; // type of font
	int         flags          = 0;               // font descriptor flags
	std::string embFontName    = "";              // name of embedded font
	Ref         embFontID      = {};              // ref to embedded font file stream
	double      fontMat[6]     = {};              // font matrix
	double      fontBBox[4]    = {};              // font bounding box
	double      missingWidth   = 0;               // "default" width
	double      ascent         = 0;               // max height above baseline
	double      descent        = 0;               // max depth below baseline
	double      declaredAscent = 0;               // ascent value, before munging
	bool        hasToUnicode   = false;           // true if the font has a ToUnicode map
	bool        ok             = false;           //
};

//------------------------------------------------------------------------
// Gfx8BitFont
//------------------------------------------------------------------------

class Gfx8BitFont : public GfxFont
{
public:
	Gfx8BitFont(XRef* xref, std::string_view tagA, Ref idA, std::string_view nameA, GfxFontType typeA, Ref embFontIDA, Dict* fontDict);

	virtual ~Gfx8BitFont();

	virtual int getNextChar(const char* s, int len, CharCode* code, Unicode* u, int uSize, int* uLen, double* dx, double* dy, double* ox, double* oy);

	// Return the encoding.
	char** getEncoding() { return enc; }

	// Return the Unicode map.
	CharCodeToUnicode* getToUnicode();

	// Return the character name associated with <code>.
	char* getCharName(int code) { return enc[code]; }

	// Returns true if the PDF font specified an encoding.
	bool getHasEncoding() { return hasEncoding; }

	// Returns true if the PDF font specified MacRomanEncoding.
	bool getUsesMacRomanEnc() { return usesMacRomanEnc; }

	// Get width of a character.
	double getWidth(uint8_t c) { return widths[c]; }

	// Return a char code-to-GID mapping for the provided font file.
	// (This is only useful for TrueType fonts.)
	int* getCodeToGIDMap(FoFiTrueType* ff);

	// Return a char code-to-GID mapping for the provided font file.
	// (This is only useful for Type1C fonts.)
	int* getCodeToGIDMap(FoFiType1C* ff);

	// Return the Type 3 CharProc dictionary, or nullptr if none.
	Dict* getCharProcs();

	// Return the Type 3 CharProc for the character associated with <code>.
	Object* getCharProc(int code, Object* proc);
	Object* getCharProcNF(int code, Object* proc);

	// Return the Type 3 Resources dictionary, or nullptr if none.
	Dict* getResources();

	virtual bool problematicForUnicode();

private:
	Base14FontMapEntry* base14               = nullptr; // for Base-14 fonts only; nullptr otherwise
	char*               enc[256]             = {};      // char code --> char name
	char                encFree[256]         = {};      // boolean for each char name: if set, the string is malloc'ed
	CharCodeToUnicode*  ctu                  = nullptr; // char code --> Unicode
	bool                hasEncoding          = false;   //
	bool                usesMacRomanEnc      = false;   //
	bool                baseEncFromFontFile  = false;   //
	bool                usedNumericHeuristic = false;   //
	double              widths[256]          = {};      // character widths
	Object              charProcs            = {};      // Type 3 CharProcs dictionary
	Object              resources            = {};      // Type 3 Resources dictionary

	friend class GfxFont;
};

//------------------------------------------------------------------------
// GfxCIDFont
//------------------------------------------------------------------------

class GfxCIDFont : public GfxFont
{
public:
	GfxCIDFont(XRef* xref, std::string_view tagA, Ref idA, std::string_view nameA, GfxFontType typeA, Ref embFontIDA, Dict* fontDict);

	virtual ~GfxCIDFont();

	virtual bool isCIDFont() { return true; }

	virtual int getNextChar(const char* s, int len, CharCode* code, Unicode* u, int uSize, int* uLen, double* dx, double* dy, double* ox, double* oy);

	// Return the writing mode (0=horizontal, 1=vertical).
	virtual int getWMode();

	// Return the Unicode map.
	CharCodeToUnicode* getToUnicode();

	// Get the collection name (<registry>-<ordering>).
	std::string getCollection();

	// Return the horizontal width for <cid>.
	double getWidth(CID cid);

	// Return the CID-to-GID mapping table.  These should only be called if type is fontCIDType2.
	int* getCIDToGID() { return cidToGID; }

	int getCIDToGIDLen() { return cidToGIDLen; }

	// Returns true if this font uses the Identity-H encoding (cmap), and the Adobe-Identity character collection, and does not have a CIDToGIDMap.
	// When this is true for a CID TrueType font, Adobe appears to treat char codes as raw GIDs.
	bool usesIdentityEncoding() { return identityEnc; }

	virtual bool problematicForUnicode();

private:
	void readTrueTypeUnicodeMapping(XRef* xref);
	void getHorizontalMetrics(CID cid, double* w);
	void getVerticalMetrics(CID cid, double* h, double* vx, double* vy);

	std::string        collection          = "";      // collection name
	CMap*              cMap                = nullptr; // char code --> CID
	CharCodeToUnicode* ctu                 = nullptr; // CID/char code --> Unicode
	bool               ctuUsesCharCode     = true;    // true: ctu maps char code to Unicode; false: ctu maps CID to Unicode
	GfxFontCIDWidths   widths              = {};      // character widths
	int*               cidToGID            = nullptr; // CID --> GID mapping (for embedded TrueType fonts)
	int                cidToGIDLen         = 0;       //
	bool               hasKnownCollection  = false;   //
	bool               hasIdentityCIDToGID = false;   //
	bool               identityEnc         = false;   //
};

//------------------------------------------------------------------------
// GfxFontDict
//------------------------------------------------------------------------

class GfxFontDictEntry
{
public:
	GfxFontDictEntry(Ref refA, Object* fontObjA);
	~GfxFontDictEntry();
	void load(GfxFont* fontA);

	bool     loaded  = false;   //
	Ref      ref     = {};      //
	Object   fontObj = {};      // valid if unloaded
	GfxFont* font    = nullptr; // valid if loaded
};

class GfxFontDict
{
public:
	// Build the font dictionary, given the PDF font dictionary.
	GfxFontDict(XRef* xrefA, Ref* fontDictRef, Dict* fontDict);

	// Destructor.
	~GfxFontDict();

	// Get the specified font.
	GfxFont* lookup(const char* tag);
	GfxFont* lookupByRef(Ref ref);

	// Iterative access.
	int      getNumFonts();
	GfxFont* getFont(int i);

private:
	friend class GfxFont;

	void        loadAll();
	void        load(const char* tag, GfxFontDictEntry* entry);
	static int  hashFontObject(Object* obj);
	static void hashFontObject1(Object* obj, FNVHash* h);

	XRef*                               xref        = nullptr; //
	GHash*                              fonts2      = nullptr; // hash table of fonts, mapping from tag to GfxFontDictEntry; this may contain duplicates, i.e., two tags that map to the same font
	GList*                              uniqueFonts = nullptr; // list of all unique font objects (no dups) that have been loaded
	UMAP<std::string, GfxFontDictEntry> fonts       = {};      //
};
