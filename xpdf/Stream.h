//========================================================================
//
// Stream.h
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include <stdio.h>
#if HAVE_JPEGLIB
#	include <jpeglib.h>
#	include <setjmp.h>
#endif
#include "gfile.h"
#include "Object.h"

class BaseStream;
class SharedFile;

//------------------------------------------------------------------------

enum StreamKind
{
	strFile,
	strASCIIHex,
	strASCII85,
	strLZW,
	strRunLength,
	strCCITTFax,
	strDCT,
	strFlate,
	strJBIG2,
	strJPX,
	strWeird // internal-use stream types
};

enum StreamColorSpaceMode
{
	streamCSNone,
	streamCSDeviceGray,
	streamCSDeviceRGB,
	streamCSDeviceCMYK
};

//------------------------------------------------------------------------

// This is in Stream.h instead of Decrypt.h to avoid really annoying
// include file dependency loops.
enum CryptAlgorithm
{
	cryptRC4,
	cryptAES,
	cryptAES256
};

//------------------------------------------------------------------------
// Stream (base class)
//------------------------------------------------------------------------

class Stream
{
public:
	// Constructor.
	Stream();

	// Destructor.
	virtual ~Stream();

	virtual Stream* copy() = 0;

	// Get kind of stream.
	virtual StreamKind getKind() = 0;

	virtual bool isEmbedStream() { return false; }

	// Disable checking for 'decompression bombs', i.e., cases where the
	// encryption ratio looks suspiciously high.  This should be called
	// for things like images which (a) can have very high compression
	// ratios in certain cases, and (b) have fixed data sizes controlled
	// by the reader.
	virtual void disableDecompressionBombChecking() {}

	// Reset stream to beginning.
	virtual void reset() = 0;

	// Close down the stream.
	virtual void close();

	// Get next char from stream.
	virtual int getChar() = 0;

	// Peek at next char in stream.
	virtual int lookChar() = 0;

	// Get next char from stream without using the predictor.
	// This is only used by StreamPredictor.
	virtual int getRawChar();

	// Get exactly <size> bytes from stream.  Returns the number of
	// bytes read -- the returned count will be less than <size> at EOF.
	virtual int getBlock(char* blk, int size);

	// Get next line from stream.
	virtual char* getLine(char* buf, int size);

	// Discard the next <n> bytes from stream.  Returns the number of
	// bytes discarded, which will be less than <n> only if EOF is
	// reached.
	virtual uint32_t discardChars(uint32_t n);

	// Get current position in file.
	virtual int64_t getPos() = 0;

	// Go to a position in the stream.  If <dir> is negative, the
	// position is from the end of the file; otherwise the position is
	// from the start of the file.
	virtual void setPos(int64_t pos, int dir = 0) = 0;

	// Get PostScript command for the filter(s).
	virtual std::string getPSFilter(int psLevel, const char* indent, bool okToReadStream);

	// Does this stream type potentially contain non-printable chars?
	virtual bool isBinary(bool last = true) = 0;

	// Does this stream include a "strong" compression filter (anything
	// other than RLE)?
	virtual bool hasStrongCompression() { return false; }

	// Get the BaseStream of this stream.
	virtual BaseStream* getBaseStream() = 0;

	// Get the stream after the last decoder (this may be a BaseStream
	// or a DecryptStream).
	virtual Stream* getUndecodedStream() = 0;

	// Get the dictionary associated with this stream.
	virtual Dict* getDict() = 0;

	// Is this an encoding filter?
	virtual bool isEncoder() { return false; }

	// Get image parameters which are defined by the stream contents.
	virtual void getImageParams(int* bitsPerComponent, StreamColorSpaceMode* csMode) {}

	// Return the next stream in the "stack".
	virtual Stream* getNextStream() { return nullptr; }

	// Add filters to this stream according to the parameters in <dict>.
	// Returns the new stream.
	Stream* addFilters(Object* dict, int recursion = 0);

private:
	Stream* makeFilter(char* name, Stream* str, Object* params, int recursion);
};

//------------------------------------------------------------------------
// BaseStream
//
// This is the base class for all streams that read directly from a file.
//------------------------------------------------------------------------

class BaseStream : public Stream
{
public:
	BaseStream(Object* dictA);
	virtual ~BaseStream();
	virtual Stream* makeSubStream(int64_t start, bool limited, int64_t length, Object* dict) = 0;
	virtual void    setPos(int64_t pos, int dir = 0)                                         = 0;

	virtual bool isBinary(bool last = true) { return last; }

	virtual BaseStream* getBaseStream() { return this; }

	virtual Stream* getUndecodedStream() { return this; }

	virtual Dict* getDict() { return dict.getDict(); }

	virtual std::string getFileName() { return nullptr; }

	// Get/set position of first byte of stream within the file.
	virtual int64_t getStart()           = 0;
	virtual void    moveStart(int delta) = 0;

protected:
	Object dict = {}; //
};

//------------------------------------------------------------------------
// FilterStream
//
// This is the base class for all streams that filter another stream.
//------------------------------------------------------------------------

class FilterStream : public Stream
{
public:
	FilterStream(Stream* strA);
	virtual ~FilterStream();

	virtual void disableDecompressionBombChecking()
	{
		str->disableDecompressionBombChecking();
	}

	virtual void close();

	virtual int64_t getPos() { return str->getPos(); }

	virtual void setPos(int64_t pos, int dir = 0);

	virtual BaseStream* getBaseStream() { return str->getBaseStream(); }

	virtual Stream* getUndecodedStream() { return str->getUndecodedStream(); }

	virtual Dict* getDict() { return str->getDict(); }

	virtual Stream* getNextStream() { return str; }

protected:
	Stream* str = nullptr; //
};

//------------------------------------------------------------------------
// ImageStream
//------------------------------------------------------------------------

class ImageStream
{
public:
	// Create an image stream object for an image with the specified
	// parameters.  Note that these are the actual image parameters,
	// which may be different from the predictor parameters.
	ImageStream(Stream* strA, int widthA, int nCompsA, int nBitsA);

	~ImageStream();

	// Reset the stream.
	void reset();

	// Close down the stream.
	void close();

	// Gets the next pixel from the stream.  <pix> should be able to hold
	// at least nComps elements.  Returns false at end of file.
	bool getPixel(uint8_t* pix);

	// Returns a pointer to the next line of pixels.  Returns nullptr at
	// end of file.
	uint8_t* getLine();

	// Skip an entire line from the image.
	void skipLine();

private:
	Stream*  str           = nullptr; // base stream
	int      width         = 0;       // pixels per line
	int      nComps        = 0;       // components per pixel
	int      nBits         = 0;       // bits per component
	int      nVals         = 0;       // components per line
	int      inputLineSize = 0;       // input line buffer size
	char*    inputLine     = nullptr; // input line buffer
	uint8_t* imgLine       = nullptr; // line buffer
	int      imgIdx        = 0;       // current index in imgLine
};

//------------------------------------------------------------------------
// StreamPredictor
//------------------------------------------------------------------------

class StreamPredictor
{
public:
	// Create a predictor object.  Note that the parameters are for the
	// predictor, and may not match the actual image parameters.
	StreamPredictor(Stream* strA, int predictorA, int widthA, int nCompsA, int nBitsA);

	~StreamPredictor();

	bool isOk() { return ok; }

	void reset();

	int lookChar();
	int getChar();
	int getBlock(char* blk, int size);

	int getPredictor() { return predictor; }

	int getWidth() { return width; }

	int getNComps() { return nComps; }

	int getNBits() { return nBits; }

private:
	bool getNextLine();

	Stream*  str       = nullptr; // base stream
	int      predictor = 0;       // predictor
	int      width     = 0;       // pixels per line
	int      nComps    = 0;       // components per pixel
	int      nBits     = 0;       // bits per component
	int      nVals     = 0;       // components per line
	int      pixBytes  = 0;       // bytes per pixel
	int      rowBytes  = 0;       // bytes per line
	uint8_t* predLine  = nullptr; // line buffer
	int      predIdx   = 0;       // current index in predLine
	bool     ok        = false;   //
};

//------------------------------------------------------------------------
// FileStream
//------------------------------------------------------------------------

#define fileStreamBufSize 256

class FileStream : public BaseStream
{
public:
	FileStream(FILE* fA, int64_t startA, bool limitedA, int64_t lengthA, Object* dictA);
	virtual ~FileStream();
	virtual Stream* copy();
	virtual Stream* makeSubStream(int64_t startA, bool limitedA, int64_t lengthA, Object* dictA);

	virtual StreamKind getKind() { return strFile; }

	virtual void reset();

	virtual int getChar()
	{
		return (bufPtr >= bufEnd && !fillBuf()) ? EOF : (*bufPtr++ & 0xff);
	}

	virtual int lookChar()
	{
		return (bufPtr >= bufEnd && !fillBuf()) ? EOF : (*bufPtr & 0xff);
	}

	virtual int getBlock(char* blk, int size);

	virtual int64_t getPos() { return bufPos + (int)(bufPtr - buf); }

	virtual void setPos(int64_t pos, int dir = 0);

	virtual int64_t getStart() { return start; }

	virtual void moveStart(int delta);

private:
	FileStream(SharedFile* fA, int64_t startA, bool limitedA, int64_t lengthA, Object* dictA);
	bool fillBuf();

	SharedFile* f                      = nullptr; //
	int64_t     start                  = 0;       //
	bool        limited                = false;   //
	int64_t     length                 = 0;       //
	char        buf[fileStreamBufSize] = {};      //
	char*       bufPtr                 = nullptr; //
	char*       bufEnd                 = nullptr; //
	int64_t     bufPos                 = 0;       //
};

//------------------------------------------------------------------------
// MemStream
//------------------------------------------------------------------------

class MemStream : public BaseStream
{
public:
	MemStream(char* bufA, int64_t startA, int64_t lengthA, Object* dictA);
	virtual ~MemStream();
	virtual Stream* copy();
	virtual Stream* makeSubStream(int64_t start, bool limited, int64_t lengthA, Object* dictA);

	virtual StreamKind getKind() { return strWeird; }

	virtual void reset();
	virtual void close();

	virtual int getChar()
	{
		return (bufPtr < bufEnd) ? (*bufPtr++ & 0xff) : EOF;
	}

	virtual int lookChar()
	{
		return (bufPtr < bufEnd) ? (*bufPtr & 0xff) : EOF;
	}

	virtual int getBlock(char* blk, int size);

	virtual int64_t getPos() { return (int64_t)(bufPtr - buf); }

	virtual void setPos(int64_t pos, int dir = 0);

	virtual int64_t getStart() { return start; }

	virtual void moveStart(int delta);

private:
	char*   buf      = nullptr; //
	int64_t start    = 0;       //
	int64_t length   = 0;       //
	char*   bufEnd   = nullptr; //
	char*   bufPtr   = nullptr; //
	bool    needFree = false;   //
};

//------------------------------------------------------------------------
// EmbedStream
//
// This is a special stream type used for embedded streams (inline
// images).  It reads directly from the base stream -- after the
// EmbedStream is deleted, reads from the base stream will proceed where
// the BaseStream left off.  Note that this is very different behavior
// that creating a new FileStream (using makeSubStream).
//------------------------------------------------------------------------

class EmbedStream : public BaseStream
{
public:
	EmbedStream(Stream* strA, Object* dictA, bool limitedA, int64_t lengthA);
	virtual ~EmbedStream();
	virtual Stream* copy();
	virtual Stream* makeSubStream(int64_t start, bool limitedA, int64_t lengthA, Object* dictA);

	virtual StreamKind getKind() { return str->getKind(); }

	virtual bool isEmbedStream() { return true; }

	virtual void reset() {}

	virtual int getChar();
	virtual int lookChar();
	virtual int getBlock(char* blk, int size);

	virtual int64_t getPos() { return str->getPos(); }

	virtual void    setPos(int64_t pos, int dir = 0);
	virtual int64_t getStart();
	virtual void    moveStart(int delta);

private:
	Stream* str     = nullptr; //
	bool    limited = false;   //
	int64_t length  = 0;       //
};

//------------------------------------------------------------------------
// ASCIIHexStream
//------------------------------------------------------------------------

class ASCIIHexStream : public FilterStream
{
public:
	ASCIIHexStream(Stream* strA);
	virtual ~ASCIIHexStream();
	virtual Stream* copy();

	virtual StreamKind getKind() { return strASCIIHex; }

	virtual void reset();

	virtual int getChar()
	{
		int c = lookChar();
		buf   = EOF;
		return c;
	}

	virtual int         lookChar();
	virtual std::string getPSFilter(int psLevel, const char* indent, bool okToReadStream);
	virtual bool        isBinary(bool last = true);

private:
	int  buf = 0;     //
	bool eof = false; //
};

//------------------------------------------------------------------------
// ASCII85Stream
//------------------------------------------------------------------------

class ASCII85Stream : public FilterStream
{
public:
	ASCII85Stream(Stream* strA);
	virtual ~ASCII85Stream();
	virtual Stream* copy();

	virtual StreamKind getKind() { return strASCII85; }

	virtual void reset();

	virtual int getChar()
	{
		int ch = lookChar();
		++index;
		return ch;
	}

	virtual int         lookChar();
	virtual std::string getPSFilter(int psLevel, const char* indent, bool okToReadStream);
	virtual bool        isBinary(bool last = true);

private:
	int  c[5]  = {};    //
	int  b[4]  = {};    //
	int  index = 0;     //
	int  n     = 0;     //
	bool eof   = false; //
};

//------------------------------------------------------------------------
// LZWStream
//------------------------------------------------------------------------

class LZWStream : public FilterStream
{
public:
	LZWStream(Stream* strA, int predictor, int columns, int colors, int bits, int earlyA);
	virtual ~LZWStream();
	virtual Stream* copy();

	virtual StreamKind getKind() { return strLZW; }

	virtual void        disableDecompressionBombChecking();
	virtual void        reset();
	virtual int         getChar();
	virtual int         lookChar();
	virtual int         getRawChar();
	virtual int         getBlock(char* blk, int size);
	virtual std::string getPSFilter(int psLevel, const char* indent, bool okToReadStream);
	virtual bool        isBinary(bool last = true);

	virtual bool hasStrongCompression() { return true; }

private:
	StreamPredictor* pred      = nullptr; // predictor
	int              early     = 0;       // early parameter
	bool             eof       = false;   // true if at eof
	int              inputBuf  = 0;       // input buffer
	int              inputBits = 0;       // number of bits in input buffer

	// decoding table
	struct
	{
		int     length; //
		int     head;   //
		uint8_t tail;   //
	} table[4097];

	int      nextCode                   = 0;     // next code to be used
	int      nextBits                   = 0;     // number of bits in next code word
	int      prevCode                   = 0;     // previous code used in stream
	int      newChar                    = 0;     // next char to be added to table
	uint8_t  seqBuf[4097]               = {};    // buffer for current sequence
	int      seqLength                  = 0;     // length of current sequence
	int      seqIndex                   = 0;     // index into current sequence
	bool     first                      = false; // first code after a table clear
	bool     checkForDecompressionBombs = false; //
	uint64_t totalIn                    = 0;     // total number of encoded bytes read so far
	uint64_t totalOut                   = 0;     // total number of bytes decoded so far

	bool processNextCode();
	void clearTable();
	int  getCode();
};

//------------------------------------------------------------------------
// RunLengthStream
//------------------------------------------------------------------------

class RunLengthStream : public FilterStream
{
public:
	RunLengthStream(Stream* strA);
	virtual ~RunLengthStream();
	virtual Stream* copy();

	virtual StreamKind getKind() { return strRunLength; }

	virtual void reset();

	virtual int getChar()
	{
		return (bufPtr >= bufEnd && !fillBuf()) ? EOF : (*bufPtr++ & 0xff);
	}

	virtual int lookChar()
	{
		return (bufPtr >= bufEnd && !fillBuf()) ? EOF : (*bufPtr & 0xff);
	}

	virtual int         getBlock(char* blk, int size);
	virtual std::string getPSFilter(int psLevel, const char* indent, bool okToReadStream);
	virtual bool        isBinary(bool last = true);

private:
	char  buf[128] = {};      // buffer
	char* bufPtr   = nullptr; // next char to read
	char* bufEnd   = nullptr; // end of buffer
	bool  eof      = false;   //

	bool fillBuf();
};

//------------------------------------------------------------------------
// CCITTFaxStream
//------------------------------------------------------------------------

struct CCITTCodeTable;

class CCITTFaxStream : public FilterStream
{
public:
	CCITTFaxStream(Stream* strA, int encodingA, bool endOfLineA, bool byteAlignA, int columnsA, int rowsA, bool endOfBlockA, bool blackA);
	virtual ~CCITTFaxStream();
	virtual Stream* copy();

	virtual StreamKind getKind() { return strCCITTFax; }

	virtual void        reset();
	virtual int         getChar();
	virtual int         lookChar();
	virtual int         getBlock(char* blk, int size);
	virtual std::string getPSFilter(int psLevel, const char* indent, bool okToReadStream);
	virtual bool        isBinary(bool last = true);

	virtual bool hasStrongCompression() { return true; }

private:
	int      encoding   = 0;       // 'K' parameter
	bool     endOfLine  = false;   // 'EndOfLine' parameter
	bool     byteAlign  = false;   // 'EncodedByteAlign' parameter
	int      columns    = 0;       // 'Columns' parameter
	int      rows       = 0;       // 'Rows' parameter
	bool     endOfBlock = false;   // 'EndOfBlock' parameter
	bool     black      = false;   // 'BlackIs1' parameter
	int      blackXOR   = 0;       //
	bool     eof        = false;   // true if at eof
	bool     nextLine2D = false;   // true if next line uses 2D encoding
	int      row        = 0;       // current row
	uint32_t inputBuf   = 0;       // input buffer
	int      inputBits  = 0;       // number of bits in input buffer
	int*     codingLine = nullptr; // coding line changing elements
	int*     refLine    = nullptr; // reference line changing elements
	int      nextCol    = 0;       // next column to read
	int      a0i        = 0;       // index into codingLine
	bool     err        = false;   // error on current line
	int      nErrors    = 0;       // number of errors so far in this stream

	void  addPixels(int a1, int blackPixels);
	void  addPixelsNeg(int a1, int blackPixels);
	bool  readRow();
	short getTwoDimCode();
	short getWhiteCode();
	short getBlackCode();
	short lookBits(int n);

	void eatBits(int n)
	{
		if ((inputBits -= n) < 0) inputBits = 0;
	}
};

//------------------------------------------------------------------------
// DCTStream
//------------------------------------------------------------------------

#if HAVE_JPEGLIB

class DCTStream;

#	define dctStreamBufSize 4096

struct DCTSourceMgr
{
	jpeg_source_mgr src                   = {};      //
	DCTStream*      str                   = nullptr; //
	char            buf[dctStreamBufSize] = {};      //
};

struct DCTErrorMgr
{
	struct jpeg_error_mgr err       = {}; //
	jmp_buf               setjmpBuf = {}; //
};

#else // HAVE_JPEGLIB

// DCT component info
struct DCTCompInfo
{
	int id         = 0; // component ID
	int hSample    = 0; //
	int vSample    = 0; // horiz/vert sampling resolutions
	int quantTable = 0; // quantization table number
	int prevDC     = 0; // DC coefficient accumulator
};

struct DCTScanInfo
{
	bool comp[4]        = {}; // comp[i] is set if component i is included in this scan
	int  numComps       = 0;  // number of components in the scan
	int  dcHuffTable[4] = {}; // DC Huffman table numbers
	int  acHuffTable[4] = {}; // AC Huffman table numbers
	int  firstCoeff     = 0;  //
	int  lastCoeff      = 0;  // first and last DCT coefficient
	int  ah             = 0;  //
	int  al             = 0;  // successive approximation parameters
};

// DCT Huffman decoding table
struct DCTHuffTable
{
	uint8_t  firstSym[17]  = {}; // first symbol for this bit length
	uint16_t firstCode[17] = {}; // first code for this bit length
	uint16_t numCodes[17]  = {}; // number of codes of this bit length
	uint8_t  sym[256]      = {}; // symbols
};

#endif // HAVE_JPEGLIB

class DCTStream : public FilterStream
{
public:
	DCTStream(Stream* strA, int colorXformA);
	virtual ~DCTStream();
	virtual Stream* copy();

	virtual StreamKind getKind() { return strDCT; }

	virtual void        reset();
	virtual void        close();
	virtual int         getChar();
	virtual int         lookChar();
	virtual int         getBlock(char* blk, int size);
	virtual std::string getPSFilter(int psLevel, const char* indent, bool okToReadStream);
	virtual bool        isBinary(bool last = true);

	virtual bool hasStrongCompression() { return true; }

	Stream* getRawStream() { return str; }

private:
	bool checkSequentialInterleaved();

#if HAVE_JPEGLIB

	int                    colorXform     = -1;      // color transform: -1 = unspecified, 0 = none, 1 = YUV/YUVK -> RGB/CMYK
	jpeg_decompress_struct decomp         = {};      //
	DCTErrorMgr            errorMgr       = {};      //
	DCTSourceMgr           sourceMgr      = {};      //
	bool                   error          = false;   //
	char*                  lineBuf        = nullptr; //
	int                    lineBufHeight  = 0;       //
	char*                  lineBufRows[4] = {};      //
	char*                  bufPtr         = nullptr; //
	char*                  bufEnd         = nullptr; //
	bool                   inlineImage    = false;   //

	bool           fillBuf();
	static void    errorExit(j_common_ptr d);
	static void    errorMessage(j_common_ptr d);
	static void    initSourceCbk(j_decompress_ptr d);
	static boolean fillInputBufferCbk(j_decompress_ptr d);
	static void    skipInputDataCbk(j_decompress_ptr d, long numBytes);
	static void    termSourceCbk(j_decompress_ptr d);

#else // HAVE_JPEGLIB

	bool         prepared           = false;   // set after prepare() is called
	bool         progressive        = false;   // set if in progressive mode
	bool         interleaved        = false;   // set if in interleaved mode
	int          width              = 0;       //
	int          height             = 0;       // // image size
	int          mcuWidth           = 0;       //
	int          mcuHeight          = 0;       // size of min coding unit, in data units
	int          bufWidth           = 0;       //
	int          bufHeight          = 0;       // frameBuf size
	DCTCompInfo  compInfo[4]        = {};      // info for each component
	DCTScanInfo  scanInfo           = {};      // info for the current scan
	int          numComps           = 0;       // number of components in image
	int          colorXform         = 0;       // color transform: -1 = unspecified, 0 = none, 1 = YUV/YUVK -> RGB/CMYK
	bool         gotJFIFMarker      = false;   // set if APP0 JFIF marker was present
	bool         gotAdobeMarker     = false;   // set if APP14 Adobe marker was present
	int          restartInterval    = 0;       // restart interval, in MCUs
	uint16_t     quantTables[4][64] = {};      // quantization tables
	int          numQuantTables     = 0;       // number of quantization tables
	DCTHuffTable dcHuffTables[4]    = {};      // DC Huffman tables
	DCTHuffTable acHuffTables[4]    = {};      // AC Huffman tables
	int          numDCHuffTables    = 0;       // number of DC Huffman tables
	int          numACHuffTables    = 0;       // number of AC Huffman tables
	uint8_t*     rowBuf             = nullptr; //
	uint8_t*     rowBufPtr          = nullptr; // current position within rowBuf
	uint8_t*     rowBufEnd          = nullptr; // end of valid data in rowBuf
	int*         frameBuf[4]        = {};      // buffer for frame (progressive mode)
	int          comp               = 0;       //
	int          x                  = 0;       //
	int          y                  = 0;       // current position within image/MCU
	int          restartCtr         = 0;       // MCUs left until restart
	int          restartMarker      = 0;       // next restart marker
	int          eobRun             = 0;       // number of EOBs left in the current run
	int          inputBuf           = 0;       // input buffer for variable length codes
	int          inputBits          = 0;       // number of valid bits in input buffer

	void prepare();
	void restart();
	bool readMCURow();
	void readScan();
	bool readDataUnit(DCTHuffTable* dcHuffTable, DCTHuffTable* acHuffTable, int* prevDC, int data[64]);
	bool readProgressiveDataUnit(DCTHuffTable* dcHuffTable, DCTHuffTable* acHuffTable, int* prevDC, int data[64]);
	void decodeImage();
	void transformDataUnit(uint16_t* quantTable, int dataIn[64], uint8_t dataOut[64]);
	int  readHuffSym(DCTHuffTable* table);
	int  readAmp(int size);
	int  readBit();
	bool readHeader(bool frame);
	bool readBaselineSOF();
	bool readProgressiveSOF();
	bool readScanInfo();
	bool readQuantTables();
	bool readHuffmanTables();
	bool readRestartInterval();
	bool readJFIFMarker();
	bool readAdobeMarker();
	bool readTrailer();
	int  readMarker();
	int  read16();

#endif // HAVE_JPEGLIB
};

//------------------------------------------------------------------------
// FlateStream
//------------------------------------------------------------------------

#define flateWindow          32768 // buffer size
#define flateMask            (flateWindow - 1)
#define flateMaxHuffman      15  // max Huffman code length
#define flateMaxCodeLenCodes 19  // max # code length codes
#define flateMaxLitCodes     288 // max # literal codes
#define flateMaxDistCodes    30  // max # distance codes

// Huffman code table entry
struct FlateCode
{
	uint16_t len = 0; // code length, in bits
	uint16_t val = 0; // value represented by this code
};

struct FlateHuffmanTab
{
	FlateCode* codes  = nullptr; //
	int        maxLen = 0;       //
};

// Decoding info for length and distance code words
struct FlateDecode
{
	int bits  = 0; // # extra bits
	int first = 0; // first length/distance
};

class FlateStream : public FilterStream
{
public:
	FlateStream(Stream* strA, int predictor, int columns, int colors, int bits);
	virtual ~FlateStream();
	virtual Stream* copy();

	virtual StreamKind getKind() { return strFlate; }

	virtual void        disableDecompressionBombChecking();
	virtual void        reset();
	virtual int         getChar();
	virtual int         lookChar();
	virtual int         getRawChar();
	virtual int         getBlock(char* blk, int size);
	virtual std::string getPSFilter(int psLevel, const char* indent, bool okToReadStream);
	virtual bool        isBinary(bool last = true);

	virtual bool hasStrongCompression() { return true; }

private:
	StreamPredictor* pred                                              = nullptr; // predictor
	uint8_t          buf[flateWindow]                                  = {};      // output data buffer
	int              index                                             = 0;       // current index into output buffer
	int              remain                                            = 0;       // number valid bytes in output buffer
	int              codeBuf                                           = 0;       // input buffer
	int              codeSize                                          = 0;       // number of bits in input buffer
	int              codeLengths[flateMaxLitCodes + flateMaxDistCodes] = {};      // literal and distance code lengths
	FlateHuffmanTab  litCodeTab                                        = {};      // literal code table
	FlateHuffmanTab  distCodeTab                                       = {};      // distance code table
	bool             compressedBlock                                   = false;   // set if reading a compressed block
	int              blockLen                                          = 0;       // remaining length of uncompressed block
	bool             endOfBlock                                        = false;   // set when end of block is reached
	bool             eof                                               = false;   // set when end of stream is reached
	bool             checkForDecompressionBombs                        = false;   //
	uint64_t         totalIn                                           = 0;       // total number of encoded bytes read so far
	uint64_t         totalOut                                          = 0;       // total number of bytes decoded so far

	static int             codeLenCodeMap[flateMaxCodeLenCodes]; // code length code reordering
	static FlateDecode     lengthDecode[flateMaxLitCodes - 257]; // length decoding info
	static FlateDecode     distDecode[flateMaxDistCodes];        // distance decoding info
	static FlateHuffmanTab fixedLitCodeTab;                      // fixed literal code table
	static FlateHuffmanTab fixedDistCodeTab;                     // fixed distance code table

	void readSome();
	bool startBlock();
	void loadFixedCodes();
	bool readDynamicCodes();
	void compHuffmanCodes(int* lengths, int n, FlateHuffmanTab* tab);
	int  getHuffmanCodeWord(FlateHuffmanTab* tab);
	int  getCodeWord(int bits);
};

//------------------------------------------------------------------------
// EOFStream
//------------------------------------------------------------------------

class EOFStream : public FilterStream
{
public:
	EOFStream(Stream* strA);
	virtual ~EOFStream();
	virtual Stream* copy();

	virtual StreamKind getKind() { return strWeird; }

	virtual void reset() {}

	virtual int getChar() { return EOF; }

	virtual int lookChar() { return EOF; }

	virtual int getBlock(char* blk, int size) { return 0; }

	virtual std::string getPSFilter(int psLevel, const char* indent, bool okToReadStream) { return ""; }

	virtual bool isBinary(bool last = true) { return false; }
};

//------------------------------------------------------------------------
// BufStream
//------------------------------------------------------------------------

class BufStream : public FilterStream
{
public:
	BufStream(Stream* strA, int bufSizeA);
	virtual ~BufStream();
	virtual Stream* copy();

	virtual StreamKind getKind() { return strWeird; }

	virtual void reset();
	virtual int  getChar();
	virtual int  lookChar();

	virtual std::string getPSFilter(int psLevel, const char* indent, bool okToReadStream) { return ""; }

	virtual bool isBinary(bool last = true);

	int lookChar(int idx);

private:
	int* buf     = nullptr; //
	int  bufSize = 0;       //
};

//------------------------------------------------------------------------
// FixedLengthEncoder
//------------------------------------------------------------------------

class FixedLengthEncoder : public FilterStream
{
public:
	FixedLengthEncoder(Stream* strA, int lengthA);
	~FixedLengthEncoder();
	virtual Stream* copy();

	virtual StreamKind getKind() { return strWeird; }

	virtual void reset();
	virtual int  getChar();
	virtual int  lookChar();

	virtual std::string getPSFilter(int psLevel, const char* indent, bool okToReadStream) { return ""; }

	virtual bool isBinary(bool last = true);

	virtual bool isEncoder() { return true; }

private:
	int length = 0; //
	int count  = 0; //
};

//------------------------------------------------------------------------
// ASCIIHexEncoder
//------------------------------------------------------------------------

class ASCIIHexEncoder : public FilterStream
{
public:
	ASCIIHexEncoder(Stream* strA);
	virtual ~ASCIIHexEncoder();
	virtual Stream* copy();

	virtual StreamKind getKind() { return strWeird; }

	virtual void reset();

	virtual int getChar()
	{
		return (bufPtr >= bufEnd && !fillBuf()) ? EOF : (*bufPtr++ & 0xff);
	}

	virtual int lookChar()
	{
		return (bufPtr >= bufEnd && !fillBuf()) ? EOF : (*bufPtr & 0xff);
	}

	virtual std::string getPSFilter(int psLevel, const char* indent, bool okToReadStream) { return ""; }

	virtual bool isBinary(bool last = true) { return false; }

	virtual bool isEncoder() { return true; }

private:
	char  buf[4]  = {};      //
	char* bufPtr  = nullptr; //
	char* bufEnd  = nullptr; //
	int   lineLen = 0;       //
	bool  eof     = false;   //

	bool fillBuf();
};

//------------------------------------------------------------------------
// ASCII85Encoder
//------------------------------------------------------------------------

class ASCII85Encoder : public FilterStream
{
public:
	ASCII85Encoder(Stream* strA);
	virtual ~ASCII85Encoder();
	virtual Stream* copy();

	virtual StreamKind getKind() { return strWeird; }

	virtual void reset();

	virtual int getChar()
	{
		return (bufPtr >= bufEnd && !fillBuf()) ? EOF : (*bufPtr++ & 0xff);
	}

	virtual int lookChar()
	{
		return (bufPtr >= bufEnd && !fillBuf()) ? EOF : (*bufPtr & 0xff);
	}

	virtual std::string getPSFilter(int psLevel, const char* indent, bool okToReadStream) { return ""; }

	virtual bool isBinary(bool last = true) { return false; }

	virtual bool isEncoder() { return true; }

private:
	char  buf[8]  = {};      //
	char* bufPtr  = nullptr; //
	char* bufEnd  = nullptr; //
	int   lineLen = 0;       //
	bool  eof     = false;   //

	bool fillBuf();
};

//------------------------------------------------------------------------
// RunLengthEncoder
//------------------------------------------------------------------------

class RunLengthEncoder : public FilterStream
{
public:
	RunLengthEncoder(Stream* strA);
	virtual ~RunLengthEncoder();
	virtual Stream* copy();

	virtual StreamKind getKind() { return strWeird; }

	virtual void reset();

	virtual int getChar()
	{
		return (bufPtr >= bufEnd && !fillBuf()) ? EOF : (*bufPtr++ & 0xff);
	}

	virtual int lookChar()
	{
		return (bufPtr >= bufEnd && !fillBuf()) ? EOF : (*bufPtr & 0xff);
	}

	virtual std::string getPSFilter(int psLevel, const char* indent, bool okToReadStream) { return ""; }

	virtual bool isBinary(bool last = true) { return true; }

	virtual bool isEncoder() { return true; }

private:
	char  buf[131] = {};      //
	char* bufPtr   = nullptr; //
	char* bufEnd   = nullptr; //
	char* nextEnd  = nullptr; //
	bool  eof      = false;   //

	bool fillBuf();
};

//------------------------------------------------------------------------
// LZWEncoder
//------------------------------------------------------------------------

struct LZWEncoderNode
{
	int             byte     = 0;       //
	LZWEncoderNode* next     = nullptr; // next sibling
	LZWEncoderNode* children = nullptr; // first child
};

class LZWEncoder : public FilterStream
{
public:
	LZWEncoder(Stream* strA);
	virtual ~LZWEncoder();
	virtual Stream* copy();

	virtual StreamKind getKind() { return strWeird; }

	virtual void reset();
	virtual int  getChar();
	virtual int  lookChar();

	virtual std::string getPSFilter(int psLevel, const char* indent, bool okToReadStream) { return ""; }

	virtual bool isBinary(bool last = true) { return true; }

	virtual bool isEncoder() { return true; }

private:
	LZWEncoderNode table[4096] = {};    //
	int            nextSeq     = 0;     //
	int            codeLen     = 0;     //
	uint8_t        inBuf[8192] = {};    //
	int            inBufStart  = 0;     //
	int            inBufLen    = 0;     //
	int            outBuf      = 0;     //
	int            outBufLen   = 0;     //
	bool           needEOD     = false; //

	void fillBuf();
};
