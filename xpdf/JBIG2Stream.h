//========================================================================
//
// JBIG2Stream.h
//
// Copyright 2002-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef JBIG2STREAM_H
#define JBIG2STREAM_H

#include <aconf.h>
#include "Object.h"
#include "Stream.h"

class GList;
class JBIG2Segment;
class JBIG2Bitmap;
class JArithmeticDecoder;
class JArithmeticDecoderStats;
class JBIG2HuffmanDecoder;
struct JBIG2HuffmanTable;
class JBIG2MMRDecoder;

//------------------------------------------------------------------------

class JBIG2Stream : public FilterStream
{
public:
	JBIG2Stream(Stream* strA, Object* globalsStreamA);
	virtual ~JBIG2Stream();
	virtual Stream* copy();

	virtual StreamKind getKind() { return strJBIG2; }

	virtual void        reset();
	virtual void        close();
	virtual int         getChar();
	virtual int         lookChar();
	virtual int         getBlock(char* blk, int size);
	virtual std::string getPSFilter(int psLevel, const char* indent, bool okToReadStream);
	virtual bool        isBinary(bool last = true);

	virtual bool hasStrongCompression() { return true; }

private:
	void          decodeImage();
	void          readSegments();
	bool          readSymbolDictSeg(uint32_t segNum, uint32_t length, uint32_t* refSegs, uint32_t nRefSegs);
	void          readTextRegionSeg(uint32_t segNum, bool imm, bool lossless, uint32_t length, uint32_t* refSegs, uint32_t nRefSegs);
	JBIG2Bitmap*  readTextRegion(bool huff, bool refine, int w, int h, uint32_t numInstances, uint32_t logStrips, int numSyms, JBIG2HuffmanTable* symCodeTab, uint32_t symCodeLen, JBIG2Bitmap** syms, uint32_t defPixel, uint32_t combOp, uint32_t transposed, uint32_t refCorner, int sOffset, JBIG2HuffmanTable* huffFSTable, JBIG2HuffmanTable* huffDSTable, JBIG2HuffmanTable* huffDTTable, JBIG2HuffmanTable* huffRDWTable, JBIG2HuffmanTable* huffRDHTable, JBIG2HuffmanTable* huffRDXTable, JBIG2HuffmanTable* huffRDYTable, JBIG2HuffmanTable* huffRSizeTable, uint32_t templ, int* atx, int* aty);
	void          readPatternDictSeg(uint32_t segNum, uint32_t length);
	void          readHalftoneRegionSeg(uint32_t segNum, bool imm, bool lossless, uint32_t length, uint32_t* refSegs, uint32_t nRefSegs);
	void          readGenericRegionSeg(uint32_t segNum, bool imm, bool lossless, uint32_t length);
	void          mmrAddPixels(int a1, int blackPixels, int* codingLine, int* a0i, int w);
	void          mmrAddPixelsNeg(int a1, int blackPixels, int* codingLine, int* a0i, int w);
	JBIG2Bitmap*  readGenericBitmap(bool mmr, int w, int h, int templ, bool tpgdOn, bool useSkip, JBIG2Bitmap* skip, int* atx, int* aty, int mmrDataLength);
	void          readGenericRefinementRegionSeg(uint32_t segNum, bool imm, bool lossless, uint32_t length, uint32_t* refSegs, uint32_t nRefSegs);
	JBIG2Bitmap*  readGenericRefinementRegion(int w, int h, int templ, bool tpgrOn, JBIG2Bitmap* refBitmap, int refDX, int refDY, int* atx, int* aty);
	void          readPageInfoSeg(uint32_t length);
	void          readEndOfStripeSeg(uint32_t length);
	void          readProfilesSeg(uint32_t length);
	void          readCodeTableSeg(uint32_t segNum, uint32_t length);
	void          readExtensionSeg(uint32_t length);
	JBIG2Segment* findSegment(uint32_t segNum);
	void          discardSegment(uint32_t segNum);
	void          resetGenericStats(uint32_t templ, JArithmeticDecoderStats* prevStats);
	void          resetRefinementStats(uint32_t templ, JArithmeticDecoderStats* prevStats);
	void          resetIntStats(int symCodeLen);
	bool          readUByte(uint32_t* x);
	bool          readByte(int* x);
	bool          readUWord(uint32_t* x);
	bool          readULong(uint32_t* x);
	bool          readLong(int* x);

	bool         decoded        = false;   //
	Object       globalsStream  = {};      //
	uint32_t     pageW          = 0;       //
	uint32_t     pageH          = 0;       //
	uint32_t     curPageH       = 0;       //
	uint32_t     pageDefPixel   = 0;       //
	JBIG2Bitmap* pageBitmap     = nullptr; //
	uint32_t     defCombOp      = 0;       //
	GList*       segments       = nullptr; // [JBIG2Segment]
	GList*       globalSegments = nullptr; // [JBIG2Segment]
	Stream*      curStr         = nullptr; //
	uint8_t*     dataPtr        = nullptr; //
	uint8_t*     dataEnd        = nullptr; //
	uint32_t     byteCounter    = 0;       //
	bool         done           = false;   //

	JArithmeticDecoder*      arithDecoder          = nullptr; //
	JArithmeticDecoderStats* genericRegionStats    = nullptr; //
	JArithmeticDecoderStats* refinementRegionStats = nullptr; //
	JArithmeticDecoderStats* iadhStats             = nullptr; //
	JArithmeticDecoderStats* iadwStats             = nullptr; //
	JArithmeticDecoderStats* iaexStats             = nullptr; //
	JArithmeticDecoderStats* iaaiStats             = nullptr; //
	JArithmeticDecoderStats* iadtStats             = nullptr; //
	JArithmeticDecoderStats* iaitStats             = nullptr; //
	JArithmeticDecoderStats* iafsStats             = nullptr; //
	JArithmeticDecoderStats* iadsStats             = nullptr; //
	JArithmeticDecoderStats* iardxStats            = nullptr; //
	JArithmeticDecoderStats* iardyStats            = nullptr; //
	JArithmeticDecoderStats* iardwStats            = nullptr; //
	JArithmeticDecoderStats* iardhStats            = nullptr; //
	JArithmeticDecoderStats* iariStats             = nullptr; //
	JArithmeticDecoderStats* iaidStats             = nullptr; //
	JBIG2HuffmanDecoder*     huffDecoder           = nullptr; //
	JBIG2MMRDecoder*         mmrDecoder            = nullptr; //
};

#endif
