//========================================================================
//
// TextOutputDev.h
//
// Copyright 1997-2012 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include <stdio.h>
#include "GfxFont.h"
#include "OutputDev.h"
#include "TextOutputDev.h"

class GList;
class UnicodeMap;
class UnicodeRemapping;

class TextBlock;
class TextChar;
class TextGaps;
class TextLink;
class TextPage;

//------------------------------------------------------------------------

typedef void (*TextOutputFunc)(void* stream, const char* text, size_t len);

//------------------------------------------------------------------------
// TextOutputControl
//------------------------------------------------------------------------

enum TextOutputMode
{
	textOutReadingOrder,  // format into reading order
	textOutPhysLayout,    // maintain original physical layout
	textOutSimpleLayout,  // simple one-column physical layout
	textOutSimple2Layout, // simple one-column physical layout
	textOutTableLayout,   // similar to PhysLayout, but optimized for tables
	textOutLinePrinter,   // strict fixed-pitch/height layout
	textOutRawOrder       // keep text in content stream order
};

enum TextOutputOverlapHandling
{
	textOutIgnoreOverlaps, // no special handling for overlaps
	textOutAppendOverlaps, // append overlapping text to main text
	textOutDiscardOverlaps // discard overlapping text
};

class TextOutputControl
{
public:
	TextOutputControl();

	~TextOutputControl() {}

	TextOutputMode            mode                 = textOutReadingOrder;   // formatting mode
	double                    fixedPitch           = 0;                     // if this is non-zero, assume fixed-pitch characters with this width (only relevant for PhysLayout, Table, and LinePrinter modes)
	double                    fixedLineSpacing     = 0;                     // fixed line spacing (only relevant for LinePrinter mode)
	bool                      html                 = false;                 // enable extra processing for HTML
	bool                      clipText             = false;                 // separate clipped text and add it back in after forming columns
	bool                      discardDiagonalText  = false;                 // discard all text that's not close to 0/90/180/270 degrees
	bool                      discardRotatedText   = false;                 // discard all text that's not horizontal (0 degrees)
	bool                      discardInvisibleText = false;                 // discard all invisible characters
	bool                      discardClippedText   = false;                 // discard all clipped characters
	bool                      splitRotatedWords    = false;                 // do not combine horizontal and non-horizontal chars in a single word
	TextOutputOverlapHandling overlapHandling      = textOutIgnoreOverlaps; // how to handle overlapping text
	bool                      separateLargeChars   = true;                  // separate "large" characters from "regular" characters
	bool                      insertBOM            = false;                 // insert a Unicode BOM at the start of the text output
	double                    marginLeft           = 0;                     // characters outside the margins are discarded
	double                    marginRight          = 0;                     //
	double                    marginTop            = 0;                     //
	double                    marginBottom         = 0;                     //
};

//------------------------------------------------------------------------
// TextFontInfo
//------------------------------------------------------------------------

class TextFontInfo
{
public:
	// Create a TextFontInfo for the current font in [state].
	TextFontInfo(GfxState* state);

	// Create a dummy TextFontInfo.
	TextFontInfo();

	~TextFontInfo();

	bool matches(GfxState* state);

	// Get the font name (which may be "").
	std::string getFontName() { return fontName; }

	// Get font descriptor flags.
	bool isFixedWidth() { return flags & fontFixedWidth; }

	bool isSerif() { return flags & fontSerif; }

	bool isSymbolic() { return flags & fontSymbolic; }

	bool isItalic() { return flags & fontItalic; }

	bool isBold() { return flags & fontBold; }

	// Get the width of the 'm' character, if available.
	double getMWidth() { return mWidth; }

	double getAscent() { return ascent; }

	double getDescent() { return descent; }

	Ref getFontID() { return fontID; }

private:
	Ref         fontID   = {}; //
	std::string fontName = ""; //
	int         flags    = 0;  //
	double      mWidth   = 0;  //
	double      ascent   = 0;  //
	double      descent  = 0;  //
	double      scale    = 0;  //

	friend class TextLine;
	friend class TextPage;
	friend class TextWord;
	friend class HTMLGen;
};

//------------------------------------------------------------------------
// TextWord
//------------------------------------------------------------------------

class TextWord
{
public:
	TextWord(GList* chars, int start, int lenA, int rotA, char rotatedA, int dirA, bool spaceAfterA);
	~TextWord();

	TextWord* copy() { return new TextWord(this); }

	// Get the TextFontInfo object associated with this word.
	TextFontInfo* getFontInfo() { return font; }

	int getLength() { return len; }

	Unicode getChar(int idx) { return text[idx]; }

	std::string getText();

	std::string getFontName() { return font->fontName; }

	void getColor(double* r, double* g, double* b)
	{
		*r = colorR;
		*g = colorG;
		*b = colorB;
	}

	bool isInvisible() { return invisible; }

	void getBBox(double* xMinA, double* yMinA, double* xMaxA, double* yMaxA)
	{
		*xMinA = xMin;
		*yMinA = yMin;
		*xMaxA = xMax;
		*yMaxA = yMax;
	}

	void getCharBBox(int charIdx, double* xMinA, double* yMinA, double* xMaxA, double* yMaxA);

	double getFontSize() { return fontSize; }

	int getRotation() { return rot; }

	bool isRotated() { return (bool)rotated; }

	int getCharPos() { return charPos[0]; }

	int getCharLen() { return charPos[len] - charPos[0]; }

	int getDirection() { return dir; }

	bool getSpaceAfter() { return spaceAfter; }

	double getBaseline();

	bool isUnderlined() { return underlined; }

	std::string getLinkURI();

private:
	TextWord(TextWord* word);
	static int cmpYX(const void* p1, const void* p2);
	static int cmpCharPos(const void* p1, const void* p2);

	double        xMin      = 0;       //
	double        xMax      = 0;       // bounding box x coordinates
	double        yMin      = 0;       //
	double        yMax      = 0;       // bounding box y coordinates
	Unicode*      text      = nullptr; // the text
	int*          charPos   = nullptr; // character position (within content stream) of each char (plus one extra entry for the last char)
	double*       edge      = nullptr; // "near" edge x or y coord of each char (plus one extra entry for the last char)
	int           len       = 0;       // number of characters
	TextFontInfo* font      = nullptr; // font information
	double        fontSize  = 0;       // font size
	TextLink*     link      = nullptr; //
	double        colorR    = 0;       // word color
	double        colorG    = 0;       //
	double        colorB    = 0;       //
	bool          invisible = false;   // set for invisible text (render mode 3)

	// group the byte-size fields to minimize object size
	uint8_t rot        = 0; // rotation, multiple of 90 degrees (0, 1, 2, or 3)
	char    rotated    = 0; // set if this word is non-horizontal
	char    dir        = 0; // character direction (+1 = left-to-right; -1 = right-to-left; 0 = neither)
	char    spaceAfter = 0; // set if there is a space between this word and the next word on the line
	char    underlined = 0; //

	friend class TextBlock;
	friend class TextLine;
	friend class TextPage;
};

//------------------------------------------------------------------------
// TextLine
//------------------------------------------------------------------------

class TextLine
{
public:
	TextLine(GList* wordsA, double xMinA, double yMinA, double xMaxA, double yMaxA, double fontSizeA);
	~TextLine();

	double getXMin() { return xMin; }

	double getYMin() { return yMin; }

	double getXMax() { return xMax; }

	double getYMax() { return yMax; }

	double getBaseline();

	int getRotation() { return rot; }

	GList* getWords() { return words; }

	Unicode* getUnicode() { return text; }

	int getLength() { return len; }

	double getEdge(int idx) { return edge[idx]; }

	bool getHyphenated() { return hyphenated; }

private:
	static int cmpX(const void* p1, const void* p2);

	GList*   words      = nullptr; // [TextWord]
	int      rot        = 0;       // rotation, multiple of 90 degrees (0, 1, 2, or 3)
	double   xMin       = 0;       //
	double   xMax       = 0;       // bounding box x coordinates
	double   yMin       = 0;       //
	double   yMax       = 0;       // bounding box y coordinates
	double   fontSize   = 0;       // main (max) font size for this line
	Unicode* text       = nullptr; // Unicode text of the line, including spaces between words
	double*  edge       = nullptr; // "near" edge x or y coord of each char (plus one extra entry for the last char)
	int      len        = 0;       // number of Unicode chars
	bool     hyphenated = false;   // set if last char is a hyphen
	int      px         = 0;       // x offset (in characters, relative to containing column) in physical layout mode
	int      pw         = 0;       // line width (in characters) in physical layout mode

	friend class TextSuperLine;
	friend class TextPage;
	friend class TextParagraph;
};

//------------------------------------------------------------------------
// TextParagraph
//------------------------------------------------------------------------

class TextParagraph
{
public:
	TextParagraph(GList* linesA, bool dropCapA);
	~TextParagraph();

	// Get the list of TextLine objects.
	GList* getLines() { return lines; }

	bool hasDropCap() { return dropCap; }

	double getXMin() { return xMin; }

	double getYMin() { return yMin; }

	double getXMax() { return xMax; }

	double getYMax() { return yMax; }

private:
	GList* lines   = nullptr; // [TextLine]
	bool   dropCap = false;   // paragraph starts with a drop capital
	double xMin    = 0;       //
	double xMax    = 0;       // bounding box x coordinates
	double yMin    = 0;       //
	double yMax    = 0;       // bounding box y coordinates

	friend class TextPage;
};

//------------------------------------------------------------------------
// TextColumn
//------------------------------------------------------------------------

class TextColumn
{
public:
	TextColumn(GList* paragraphsA, double xMinA, double yMinA, double xMaxA, double yMaxA);
	~TextColumn();

	// Get the list of TextParagraph objects.
	GList* getParagraphs() { return paragraphs; }

	double getXMin() { return xMin; }

	double getYMin() { return yMin; }

	double getXMax() { return xMax; }

	double getYMax() { return yMax; }

	int getRotation();

private:
	static int cmpX(const void* p1, const void* p2);
	static int cmpY(const void* p1, const void* p2);
	static int cmpPX(const void* p1, const void* p2);

	GList* paragraphs = nullptr; // [TextParagraph]
	double xMin       = 0;       //
	double xMax       = 0;       // bounding box x coordinates
	double yMin       = 0;       //
	double yMax       = 0;       // bounding box y coordinates
	int    px         = 0;       //
	int    py         = 0;       // x, y position (in characters) in physical layout mode
	int    pw         = 0;
	int    ph         = 0; // column width, height (in characters) in physical layout mode

	friend class TextPage;
};

//------------------------------------------------------------------------
// TextWordList
//------------------------------------------------------------------------

class TextWordList
{
public:
	TextWordList(GList* wordsA, bool primaryLRA);

	~TextWordList();

	// Return the number of words on the list.
	int getLength();

	// Return the <idx>th word from the list.
	TextWord* get(int idx);

	// Returns true if primary direction is left-to-right, or false if
	// right-to-left.
	bool getPrimaryLR() { return primaryLR; }

private:
	GList* words     = nullptr; // [TextWord]
	bool   primaryLR = false;   //
};

//------------------------------------------------------------------------
// TextPosition
//------------------------------------------------------------------------

// Position within a TextColumn tree.  The position is in column
// [colIdx], paragraph [parIdx], line [lineIdx], before character
// [charIdx].
class TextPosition
{
public:
	TextPosition()
	    : colIdx(0)
	    , parIdx(0)
	    , lineIdx(0)
	    , charIdx(0)
	{
	}

	TextPosition(int colIdxA, int parIdxA, int lineIdxA, int charIdxA)
	    : colIdx(colIdxA)
	    , parIdx(parIdxA)
	    , lineIdx(lineIdxA)
	    , charIdx(charIdxA)
	{
	}

	int operator==(TextPosition pos);
	int operator!=(TextPosition pos);
	int operator<(TextPosition pos);
	int operator>(TextPosition pos);

	int colIdx, parIdx, lineIdx, charIdx;
};

//------------------------------------------------------------------------
// TextPage
//------------------------------------------------------------------------

class TextPage
{
public:
	TextPage(TextOutputControl* controlA);
	~TextPage();

	// Write contents of page to a stream.
	void write(void* outputStream, TextOutputFunc outputFunc);

	// Find a string.
	// If <startAtTop> is true, starts looking at the top of the page;
	// else if <startAtLast> is true, starts looking immediately after the last find result;
	// else starts looking at <xMin>,<yMin>.
	// If <stopAtBottom> is true, stops looking at the bottom of the page;
	// else if <stopAtLast> is true, stops looking just before the last find result;
	// else stops looking at` <xMax>,<yMax>.
	bool findText(Unicode* s, int len, bool startAtTop, bool stopAtBottom, bool startAtLast, bool stopAtLast, bool caseSensitive, bool backward, bool wholeWord, double* xMin, double* yMin, double* xMax, double* yMax);

	// Get the text which is inside the specified rectangle.
	// Multi-line text always includes end-of-line markers at the end of each line.
	// If <forceEOL> is true, an end-of-line marker will be appended to single-line text as well.
	std::string getText(double xMin, double yMin, double xMax, double yMax, bool forceEOL = false);

	// Find a string by character position and length.
	// If found, sets the text bounding rectangle and returns true; otherwise returns false.
	bool findCharRange(int pos, int length, double* xMin, double* yMin, double* xMax, double* yMax);

	// Returns true if x,y falls inside a column.
	bool checkPointInside(double x, double y);

	// Find a point inside a column.  Returns false if x,y fall outside all columns.
	bool findPointInside(double x, double y, TextPosition* pos);

	// Find a point in the nearest column.  Returns false only if there are no columns.
	bool findPointNear(double x, double y, TextPosition* pos);

	// Find the start and end of a word inside a column.  Returns false if x,y fall outside all columns.
	bool findWordPoints(double x, double y, TextPosition* startPos, TextPosition* endPos);

	// Find the start and end of a line inside a column.  Returns false if x,y fall outside all columns.
	bool findLinePoints(double x, double y, TextPosition* startPos, TextPosition* endPos);

	// Get the upper point of a TextPosition.
	void convertPosToPointUpper(TextPosition* pos, double* x, double* y);

	// Get the lower point of a TextPosition.
	void convertPosToPointLower(TextPosition* pos, double* x, double* y);

	// Get the upper left corner of the line containing a TextPosition.
	void convertPosToPointLeftEdge(TextPosition* pos, double* x, double* y);

	// Get the lower right corner of the line containing a TextPosition.
	void convertPosToPointRightEdge(TextPosition* pos, double* x, double* y);

	// Get the upper right corner of a column.
	void getColumnUpperRight(int colIdx, double* x, double* y);

	// Get the lower left corner of a column.
	void getColumnLowerLeft(int colIdx, double* x, double* y);

	// Create and return a list of TextColumn objects.
	GList* makeColumns();

	// Get the list of all TextFontInfo objects used on this page.
	std::vector<TextFontInfo> getFonts() { return fonts; }

	// Build a flat word list, in the specified ordering.
	TextWordList* makeWordList();

	// Build a word list containing only words inside the specified rectangle.
	TextWordList* makeWordListForRect(double xMin, double yMin, double xMax, double yMax);

	// Get the primary rotation of text on the page.
	int getPrimaryRotation() { return primaryRot; }

	// Returns true if the primary character direction is left-to-right, false if it is right-to-left.
	bool primaryDirectionIsLR();

	// Get the counter values.
	int getNumVisibleChars() { return nVisibleChars; }

	int getNumInvisibleChars() { return nInvisibleChars; }

	int getNumRemovedDupChars() { return nRemovedDupChars; }

	// Returns true if any of the fonts used on this page are likely to be problematic when converting text to Unicode.
	bool problematicForUnicode() { return problematic; }

	// Add a 'special' character to this TextPage.  This is currently used by pdftohtml to insert markers for form fields.
	void addSpecialChar(double xMin, double yMin, double xMax, double yMax, int rot, TextFontInfo* font, double fontSize, Unicode u);

	// Remove characters that fall inside a region.
	void removeChars(double xMin, double yMin, double xMax, double yMax, double xOverlapThresh, double yOverlapThresh);

private:
	void startPage(GfxState* state);
	void clear();
	void updateFont(GfxState* state);
	void addChar(GfxState* state, double x, double y, double dx, double dy, CharCode c, int nBytes, Unicode* u, int uLen);
	void incCharCount(int nChars);
	void beginActualText(GfxState* state, Unicode* u, int uLen);
	void endActualText(GfxState* state);
	void addUnderline(double x0, double y0, double x1, double y1);
	void addLink(double xMin, double yMin, double xMax, double yMax, Link* link);

	// output
	void writeReadingOrder(void* outputStream, TextOutputFunc outputFunc, UnicodeMap* uMap, char* space, int spaceLen, char* eol, int eolLen);
	void writePhysLayout(void* outputStream, TextOutputFunc outputFunc, UnicodeMap* uMap, char* space, int spaceLen, char* eol, int eolLen);
	void writeSimpleLayout(void* outputStream, TextOutputFunc outputFunc, UnicodeMap* uMap, char* space, int spaceLen, char* eol, int eolLen);
	void writeSimple2Layout(void* outputStream, TextOutputFunc outputFunc, UnicodeMap* uMap, char* space, int spaceLen, char* eol, int eolLen);
	void writeLinePrinter(void* outputStream, TextOutputFunc outputFunc, UnicodeMap* uMap, char* space, int spaceLen, char* eol, int eolLen);
	void writeRaw(void* outputStream, TextOutputFunc outputFunc, UnicodeMap* uMap, char* space, int spaceLen, char* eol, int eolLen);
	void encodeFragment(Unicode* text, int len, UnicodeMap* uMap, bool primaryLR, std::string& s);
	bool unicodeEffectiveTypeLOrNum(Unicode u, Unicode left, Unicode right);
	bool unicodeEffectiveTypeR(Unicode u, Unicode left, Unicode right);

	// analysis
	int         rotateChars(GList* charsA);
	void        rotateCharsToZero(GList* charsA);
	void        rotateUnderlinesAndLinks(int rot);
	void        unrotateChars(GList* charsA, int rot);
	void        unrotateCharsFromZero(GList* charsA);
	void        unrotateColumnsFromZero(GList* columns);
	void        unrotateColumns(GList* columns, int rot);
	void        unrotateWords(GList* words, int rot);
	bool        checkPrimaryLR(GList* charsA);
	void        removeDuplicates(GList* charsA, int rot);
	GList*      separateOverlappingText(GList* charsA);
	TextColumn* buildOverlappingTextColumn(GList* overlappingChars);
	TextBlock*  splitChars(GList* charsA);
	TextBlock*  split(GList* charsA, int rot);
	GList*      getChars(GList* charsA, double xMin, double yMin, double xMax, double yMax);
	void        findGaps(GList* charsA, int rot, double* xMinOut, double* yMinOut, double* xMaxOut, double* yMaxOut, double* avgFontSizeOut, double* minFontSizeOut, GList* splitLines, TextGaps* horizGaps, TextGaps* vertGaps);
	void        mergeSplitLines(GList* charsA, int rot, GList* splitLines);
	void        tagBlock(TextBlock* blk);
	void        insertLargeChars(GList* largeChars, TextBlock* blk);
	void        insertLargeCharsInFirstLeaf(GList* largeChars, TextBlock* blk);
	void        insertLargeCharInLeaf(TextChar* ch, TextBlock* blk);
	void        insertIntoTree(TextBlock* subtree, TextBlock* primaryTree, bool doReorder);
	void        reorderBlocks(TextBlock* blk);
	void        insertColumnIntoTree(TextBlock* column, TextBlock* tree);
	void        insertClippedChars(GList* clippedChars, TextBlock* tree);
	TextBlock*  findClippedCharLeaf(TextChar* ch, TextBlock* tree);
	GList*      buildColumns(TextBlock* tree, bool primaryLR);
	void        buildColumns2(TextBlock* blk, GList* columns, bool primaryLR);
	TextColumn* buildColumn(TextBlock* tree);
	double      getLineIndent(TextLine* line, TextBlock* blk);
	double      getAverageLineSpacing(GList* lines);
	double      getLineSpacing(TextLine* line0, TextLine* line1);
	void        buildLines(TextBlock* blk, GList* lines, bool splitSuperLines);
	GList*      buildSimple2Columns(GList* charsA);
	GList*      buildSimple2Lines(GList* charsA, int rot);
	TextLine*   buildLine(TextBlock* blk);
	TextLine*   buildLine(GList* charsA, int rot, double xMin, double yMin, double xMax, double yMax);
	void        getLineChars(TextBlock* blk, GList* charsA);
	double      computeWordSpacingThreshold(GList* charsA, int rot);
	void        adjustCombiningChars(GList* charsA, int rot);
	int         getCharDirection(TextChar* ch);
	int         getCharDirection(TextChar* ch, TextChar* left, TextChar* right);
	int         assignPhysLayoutPositions(GList* columns);
	void        assignLinePhysPositions(GList* columns);
	void        computeLinePhysWidth(TextLine* line, UnicodeMap* uMap);
	int         assignColumnPhysPositions(GList* columns);
	void        buildSuperLines(TextBlock* blk, GList* superLines);
	void        assignSimpleLayoutPositions(GList* superLines, UnicodeMap* uMap);
	void        generateUnderlinesAndLinks(GList* columns);
	void        findPointInColumn(TextColumn* col, double x, double y, TextPosition* pos);
	void        buildFindCols();

	// debug
#if 0 //~debug
	void dumpChars(GList *charsA);
	void dumpTree(TextBlock *tree, int indent = 0);
	void dumpColumns(GList *columns, bool dumpWords = false);
	void dumpUnderlines();
#endif

	// word list
	TextWordList* makeWordListForChars(GList* charList);

	TextOutputControl         control          = {};      // formatting parameters
	UnicodeRemapping*         remapping        = nullptr; //
	Unicode*                  uBuf             = nullptr; //
	int                       uBufSize         = 0;       //
	double                    pageWidth        = 0;       //
	double                    pageHeight       = 0;       // width and height of current page
	int                       charPos          = 0;       // next character position (within content stream)
	TextFontInfo*             curFont          = nullptr; // current font
	double                    curFontSize      = 0;       // current font size
	int                       curRot           = 0;       // current rotation
	bool                      diagonal         = false;   // set if rotation is not close to 0/90/180/270 degrees
	bool                      rotated          = false;   // set if text is not horizontal (0 degrees)
	int                       nTinyChars       = 0;       // number of "tiny" chars seen so far
	Unicode*                  actualText       = nullptr; // current "ActualText" span
	int                       actualTextLen    = 0;       //
	double                    actualTextX0     = 0;       //
	double                    actualTextY0     = 0;       //
	double                    actualTextX1     = 0;       //
	double                    actualTextY1     = 0;       //
	int                       actualTextNBytes = 0;       //
	GList*                    chars            = nullptr; // [TextChar]
	std::vector<TextFontInfo> fonts            = {};      // all font info objects used on this page [TextFontInfo]
	int                       primaryRot       = 0;       // primary rotation
	GList*                    underlines       = nullptr; // [TextUnderline]
	GList*                    links            = nullptr; // [TextLink]
	int                       nVisibleChars    = 0;       // number of visible chars on the page
	int                       nInvisibleChars  = 0;       // number of invisible chars on the page
	int                       nRemovedDupChars = 0;       // number of duplicate chars removed
	GList*                    findCols         = nullptr; // text used by the findText**/findPoint** functions [TextColumn]
	double                    lastFindXMin     = 0;       // coordinates of the last "find" result
	double                    lastFindYMin     = 0;       //
	bool                      haveLastFind     = false;   //
	bool                      problematic      = false;   // true if any of the fonts used on this page are marked as problematic for Unicode conversion

	friend class TextOutputDev;
};

//------------------------------------------------------------------------
// TextOutputDev
//------------------------------------------------------------------------

class TextOutputDev : public OutputDev
{
public:
	// Open a text output file.
	// If <fileName> is nullptr, no file is written (this is useful, e.g., for searching text).
	// If <physLayoutA> is true, the original physical layout of the text is maintained.
	// If <rawOrder> is true, the text is kept in content stream order.
	TextOutputDev(const char* fileName, TextOutputControl* controlA, bool append, bool fileNameIsUTF8 = false);

	// Create a TextOutputDev which will write to a generic stream.
	// If <physLayoutA> is true, the original physical layout of the text is maintained.
	// If <rawOrder> is true, the text is kept in content stream order.
	TextOutputDev(TextOutputFunc func, void* stream, TextOutputControl* controlA);

	// Destructor.
	virtual ~TextOutputDev();

	// Check if file was successfully created.
	virtual bool isOk() { return ok; }

	//---- get info about output device

	// Does this device use upside-down coordinates?
	// (Upside-down means (0,0) is the top left corner of the page.)
	virtual bool upsideDown() { return true; }

	// Does this device use drawChar() or drawString()?
	virtual bool useDrawChar() { return true; }

	// Does this device use beginType3Char/endType3Char?
	// Otherwise, text in Type 3 fonts will be drawn with drawChar/drawString.
	virtual bool interpretType3Chars() { return false; }

	// Does this device need non-text content?
	virtual bool needNonText() { return false; }

	// Does this device require incCharCount to be called for text on non-shown layers?
	virtual bool needCharCount() { return true; }

	//----- initialization and control

	// Start a page.
	virtual void startPage(int pageNum, GfxState* state);

	// End a page.
	virtual void endPage();

	//----- save/restore graphics state
	virtual void restoreState(GfxState* state);

	//----- update text state
	virtual void updateFont(GfxState* state);

	//----- text drawing
	virtual void beginString(GfxState* state, const std::string& s);
	virtual void endString(GfxState* state);
	virtual void drawChar(GfxState* state, double x, double y, double dx, double dy, double originX, double originY, CharCode c, int nBytes, Unicode* u, int uLen, bool fill, bool stroke, bool makePath);
	virtual void incCharCount(int nChars);
	virtual void beginActualText(GfxState* state, Unicode* u, int uLen);
	virtual void endActualText(GfxState* state);

	//----- path painting
	virtual void stroke(GfxState* state);
	virtual void fill(GfxState* state);
	virtual void eoFill(GfxState* state);

	//----- link borders
	virtual void processLink(Link* link);

	//----- special access

	// Find a string.
	// If <startAtTop> is true, starts looking at the top of the page;
	// else if <startAtLast> is true, starts looking immediately after the last find result;
	// else starts looking at <xMin>,<yMin>.
	// If <stopAtBottom> is true, stops looking at the bottom of the page;
	// else if <stopAtLast> is true, stops looking just before the last find result;
	// else stops looking at <xMax>,<yMax>.
	bool findText(Unicode* s, int len, bool startAtTop, bool stopAtBottom, bool startAtLast, bool stopAtLast, bool caseSensitive, bool backward, bool wholeWord, double* xMin, double* yMin, double* xMax, double* yMax);

	// Get the text which is inside the specified rectangle.
	std::string getText(double xMin, double yMin, double xMax, double yMax);

	// Find a string by character position and length.
	// If found, sets the text bounding rectangle and returns true; otherwise returns false.
	bool findCharRange(int pos, int length, double* xMin, double* yMin, double* xMax, double* yMax);

	// Build a flat word list, in content stream order (if this->rawOrder is true),
	// physical layout order (if this->physLayout is true and this->rawOrder is false),
	// or reading order (if both flags are false).
	TextWordList* makeWordList();

	// Build a word list containing only words inside the specified rectangle.
	TextWordList* makeWordListForRect(double xMin, double yMin, double xMax, double yMax);

	// Returns the TextPage object for the last rasterized page, transferring ownership to the caller.
	TextPage* takeText();

	// Turn extra processing for HTML conversion on or off.
	void enableHTMLExtras(bool html) { control.html = html; }

	// Get the counter values.
	int getNumVisibleChars() { return text->nVisibleChars; }

	int getNumInvisibleChars() { return text->nInvisibleChars; }

	int getNumRemovedDupChars() { return text->nRemovedDupChars; }

private:
	void generateBOM();

	TextOutputFunc    outputFunc   = nullptr; // output function
	void*             outputStream = nullptr; // output stream
	bool              needClose    = false;   // need to close the output file? (only if outputStream is a FILE*)
	TextPage*         text         = nullptr; // text for the current page
	TextOutputControl control      = {};      // formatting parameters
	bool              ok           = false;   // set up ok?
};
