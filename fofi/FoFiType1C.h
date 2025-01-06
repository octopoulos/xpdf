//========================================================================
//
// FoFiType1C.h
//
// Copyright 1999-2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "FoFiBase.h"

//------------------------------------------------------------------------

struct Type1CIndex
{
	int pos      = 0; // absolute position in file
	int len      = 0; // length (number of entries)
	int offSize  = 0; // offset size
	int startPos = 0; // position of start of index data - 1
	int endPos   = 0; // position one byte past end of the index
};

struct Type1CIndexVal
{
	int pos = 0; // absolute position in file
	int len = 0; // length, in bytes
};

struct Type1CTopDict
{
	int    firstOp            = 0;     //
	int    versionSID         = 0;     //
	int    noticeSID          = 0;     //
	int    copyrightSID       = 0;     //
	int    fullNameSID        = 0;     //
	int    familyNameSID      = 0;     //
	int    weightSID          = 0;     //
	int    isFixedPitch       = 0;     //
	double italicAngle        = 0;     //
	double underlinePosition  = 0;     //
	double underlineThickness = 0;     //
	int    paintType          = 0;     //
	int    charstringType     = 0;     //
	double fontMatrix[6]      = {};    //
	bool   hasFontMatrix      = false; // CID fonts are allowed to put their FontMatrix in the FD instead of the top dict
	int    uniqueID           = 0;     //
	double fontBBox[4]        = {};    //
	double strokeWidth        = 0;     //
	int    charsetOffset      = 0;     //
	int    encodingOffset     = 0;     //
	int    charStringsOffset  = 0;     //
	int    privateSize        = 0;     //
	int    privateOffset      = 0;     //

	// CIDFont entries
	int registrySID    = 0; //
	int orderingSID    = 0; //
	int supplement     = 0; //
	int fdArrayOffset  = 0; //
	int fdSelectOffset = 0; //
};

#define type1CMaxBlueValues 14
#define type1CMaxOtherBlues 10
#define type1CMaxStemSnap   12

struct Type1CPrivateDict
{
	double fontMatrix[6]                         = {};    //
	bool   hasFontMatrix                         = false; //
	int    blueValues[type1CMaxBlueValues]       = {};    //
	int    nBlueValues                           = 0;     //
	int    otherBlues[type1CMaxOtherBlues]       = {};    //
	int    nOtherBlues                           = 0;     //
	int    familyBlues[type1CMaxBlueValues]      = {};    //
	int    nFamilyBlues                          = 0;     //
	int    familyOtherBlues[type1CMaxOtherBlues] = {};    //
	int    nFamilyOtherBlues                     = 0;     //
	double blueScale                             = 0;     //
	int    blueShift                             = 0;     //
	int    blueFuzz                              = 0;     //
	double stdHW                                 = 0;     //
	bool   hasStdHW                              = false; //
	double stdVW                                 = 0;     //
	bool   hasStdVW                              = false; //
	double stemSnapH[type1CMaxStemSnap]          = {};    //
	int    nStemSnapH                            = 0;     //
	double stemSnapV[type1CMaxStemSnap]          = {};    //
	int    nStemSnapV                            = 0;     //
	bool   forceBold                             = false; //
	bool   hasForceBold                          = false; //
	double forceBoldThreshold                    = 0;     //
	int    languageGroup                         = 0;     //
	double expansionFactor                       = 0;     //
	int    initialRandomSeed                     = 0;     //
	int    subrsOffset                           = 0;     //
	double defaultWidthX                         = 0;     //
	bool   defaultWidthXInt                      = false; //
	double nominalWidthX                         = 0;     //
	bool   nominalWidthXInt                      = false; //
};

// operand kind
enum Type1COpKind
{
	type1COpOperator,
	type1COpInteger,
	type1COpFloat,
	type1COpRational
};

// operand
struct Type1COp
{
	Type1COpKind kind = type1COpOperator; //

	union
	{
		int    op;    // type1COpOperator
		int    intgr; // type1COpInteger
		double flt;   // type1COpFloat

		struct
		{
			int num, den; // type1COpRational
		} rat;
	};

	bool   isZero();
	bool   isNegative();
	int    toInt();
	double toFloat();
};

struct Type1CEexecBuf
{
	FoFiOutputFunc outputFunc   = nullptr; //
	void*          outputStream = nullptr; //
	bool           ascii        = false;   // ASCII encoding?
	uint16_t       r1           = 0;       // eexec encryption key
	int            line         = 0;       // number of eexec chars left on current line
};

//------------------------------------------------------------------------
// FoFiType1C
//------------------------------------------------------------------------

class FoFiType1C : public FoFiBase
{
public:
	// Create a FoFiType1C object from a memory buffer.
	static FoFiType1C* make(const char* fileA, size_t lenA);

	// Create a FoFiType1C object from a file on disk.
	static FoFiType1C* load(const char* fileName);

	virtual ~FoFiType1C();

	// Return the font name.
	std::string_view getName();

	// Return the encoding, as an array of 256 names (any of which may be nullptr).
	// This is only useful with 8-bit fonts.
	const VEC_STR& getEncoding();

	// Get the glyph names.
	int getNumGlyphs() { return nGlyphs; }

	std::string getGlyphName(int gid);

	// Returns a hash mapping glyph names to GIDs.
	// This is only useful with 8-bit fonts.
	UMAP_STR_INT getNameToGIDMap();

	// Return the mapping from CIDs to GIDs, and return the number of
	// CIDs in *<nCIDs>.  This is only useful for CID fonts.
	int* getCIDToGIDMap(int* nCIDs);

	// Return the font matrix as an array of six numbers.
	void getFontMatrix(double* mat);

	// Convert to a Type 1 font, suitable for embedding in a PostScript file.
	// This is only useful with 8-bit fonts.
	// If <newEncoding> is not nullptr, it will be used in place of the encoding in the Type 1C font.
	// If <ascii> is true the eexec section will be hex-encoded, otherwise it will be left as binary data.  If <psName> is non-nullptr, it will be used as the PostScript font name.
	void convertToType1(const std::string& psName0, const VEC_STR& newEncoding, bool ascii, FoFiOutputFunc outputFunc, void* outputStream);

	// Convert to a Type 0 CIDFont, suitable for embedding in a PostScript file.
	// <psName> will be used as the PostScript font name.
	// There are three cases for the CID-to-GID mapping:
	// (1) if <codeMap> is non-nullptr, then it is the CID-to-GID mapping
	// (2) if <codeMap> is nullptr and this is a CID CFF font, then the font's internal CID-to-GID mapping is used
	// (3) is <codeMap> is nullptr and this is an 8-bit CFF font, then the identity CID-to-GID mapping is used
	void convertToCIDType0(const std::string& psName, int* codeMap, int nCodes, FoFiOutputFunc outputFunc, void* outputStream);

	// Convert to a Type 0 (but non-CID) composite font, suitable for embedding in a PostScript file.
	// <psName> will be used as the PostScript font name.
	// There are three cases for the CID-to-GID mapping:
	// (1) if <codeMap> is non-nullptr, then it is the CID-to-GID mapping
	// (2) if <codeMap> is nullptr and this is a CID CFF font, then the font's internal CID-to-GID mapping is used
	// (3) is <codeMap> is nullptr and this is an 8-bit CFF font, then the identity CID-to-GID mapping is used
	void convertToType0(const std::string& psName, int* codeMap, int nCodes, FoFiOutputFunc outputFunc, void* outputStream);

	// Write an OpenType file, encapsulating the CFF font.
	// <widths> provides the glyph widths (in design units) for <nWidths> glyphs.
	// The cmap table must be supplied by the caller.
	void convertToOpenType(FoFiOutputFunc outputFunc, void* outputStream, int nWidths, uint16_t* widths, uint8_t* cmapTable, int cmapTableLen);

private:
	FoFiType1C(const char* fileA, size_t lenA, bool freeFileDataA);
	void     eexecCvtGlyph(Type1CEexecBuf* eb, const char* glyphName, int offset, int nBytes, Type1CIndex* subrIdx, Type1CPrivateDict* pDict);
	void     cvtGlyph(int offset, int nBytes, std::string& charBuf, Type1CIndex* subrIdx, Type1CPrivateDict* pDict, bool top, int recursion);
	void     cvtGlyphWidth(bool useOp, std::string& charBuf, Type1CPrivateDict* pDict);
	void     cvtNum(Type1COp op, std::string& charBuf);
	void     eexecWrite(Type1CEexecBuf* eb, const char* s);
	void     eexecWriteCharstring(Type1CEexecBuf* eb, uint8_t* s, size_t n);
	void     writePSString(std::string_view sv, FoFiOutputFunc outputFunc, void* outputStream);
	uint32_t computeOpenTypeTableChecksum(uint8_t* data, int length);
	bool     parse();
	void     readTopDict();
	void     readFD(int offset, int length, Type1CPrivateDict* pDict);
	void     readPrivateDict(int offset, int length, Type1CPrivateDict* pDict);
	void     readFDSelect();
	void     buildEncoding();
	bool     readCharset();
	int      getOp(int pos, bool charstring, bool* ok);
	int      getDeltaIntArray(int* arr, int maxLen);
	int      getDeltaFPArray(double* arr, int maxLen);
	void     getIndex(int pos, Type1CIndex* idx, bool* ok);
	void     getIndexVal(Type1CIndex* idx, int i, Type1CIndexVal* val, bool* ok);
	char*    getString(int sid, char* buf, bool* ok);

	std::string        name           = "";      //
	VEC_STR            encoding       = {};      //
	Type1CIndex        nameIdx        = {};      //
	Type1CIndex        topDictIdx     = {};      //
	Type1CIndex        stringIdx      = {};      //
	Type1CIndex        gsubrIdx       = {};      //
	Type1CIndex        charStringsIdx = {};      //
	Type1CTopDict      topDict        = {};      //
	Type1CPrivateDict* privateDicts   = nullptr; //
	int                nGlyphs        = 0;       //
	int                nFDs           = 0;       //
	uint8_t*           fdSelect       = nullptr; //
	uint16_t*          charset        = nullptr; //
	int                gsubrBias      = 0;       //
	bool               parsedOk       = false;   //
	Type1COp           ops[49]        = {};      // operands and operator
	int                nOps           = 0;       // number of operands
	int                nHints         = 0;       // number of hints for the current glyph
	bool               firstOp        = false;   // true if we haven't hit the first op yet
	bool               openPath       = false;   // true if there is an unclosed path
};
