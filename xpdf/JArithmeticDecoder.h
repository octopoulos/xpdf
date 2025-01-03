//========================================================================
//
// JArithmeticDecoder.h
//
// Arithmetic decoder used by the JBIG2 and JPEG2000 decoders.
//
// Copyright 2002-2004 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>

class Stream;

//------------------------------------------------------------------------
// JArithmeticDecoderStats
//------------------------------------------------------------------------

class JArithmeticDecoderStats
{
public:
	JArithmeticDecoderStats(int contextSizeA);
	~JArithmeticDecoderStats();
	JArithmeticDecoderStats* copy();
	void                     reset();

	int getContextSize() { return contextSize; }

	void copyFrom(JArithmeticDecoderStats* stats);
	void setEntry(uint32_t cx, int i, int mps);

private:
	uint8_t* cxTab;       // cxTab[cx] = (i[cx] << 1) + mps[cx]
	int      contextSize; //

	friend class JArithmeticDecoder;
};

//------------------------------------------------------------------------
// JArithmeticDecoder
//------------------------------------------------------------------------

class JArithmeticDecoder
{
public:
	JArithmeticDecoder();
	~JArithmeticDecoder();

	void setStream(Stream* strA)
	{
		str         = strA;
		dataLen     = 0;
		limitStream = false;
	}

	void setStream(Stream* strA, int dataLenA)
	{
		str         = strA;
		dataLen     = dataLenA;
		limitStream = true;
	}

	// Start decoding on a new stream.  This fills the byte buffers and
	// runs INITDEC.
	void start();

	// Restart decoding on an interrupted stream.  This refills the
	// buffers if needed, but does not run INITDEC.  (This is used in
	// JPEG 2000 streams when codeblock data is split across multiple
	// packets/layers.)
	void restart(int dataLenA);

	// Read any leftover data in the stream.
	void cleanup();

	// Decode one bit.
	int decodeBit(uint32_t context, JArithmeticDecoderStats* stats);

	// Decode eight bits.
	int decodeByte(uint32_t context, JArithmeticDecoderStats* stats);

	// Returns false for OOB, otherwise sets *<x> and returns true.
	bool decodeInt(int* x, JArithmeticDecoderStats* stats);

	uint32_t decodeIAID(uint32_t codeLen, JArithmeticDecoderStats* stats);

	void resetByteCounter() { nBytesRead = 0; }

	uint32_t getByteCounter() { return nBytesRead; }

private:
	uint32_t readByte();
	int      decodeIntBit(JArithmeticDecoderStats* stats);
	void     byteIn();

	static uint32_t qeTab[47];     //
	static int      nmpsTab[47];   //
	static int      nlpsTab[47];   //
	static int      switchTab[47]; //
	uint32_t        buf0;          //
	uint32_t        buf1;          //
	uint32_t        c;             //
	uint32_t        a;             //
	int             ct;            //
	uint32_t        prev;          // for the integer decoder
	Stream*         str;           //
	uint32_t        nBytesRead;    //
	int             dataLen;       //
	bool            limitStream;   //
	int             readBuf;       //
};
