//========================================================================
//
// JPXStream.h
//
// Copyright 2002-2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "Object.h"
#include "Stream.h"

class JArithmeticDecoder;
class JArithmeticDecoderStats;

//------------------------------------------------------------------------

enum JPXColorSpaceType
{
	jpxCSBiLevel   = 0,
	jpxCSYCbCr1    = 1,
	jpxCSYCbCr2    = 3,
	jpxCSYCBCr3    = 4,
	jpxCSPhotoYCC  = 9,
	jpxCSCMY       = 11,
	jpxCSCMYK      = 12,
	jpxCSYCCK      = 13,
	jpxCSCIELab    = 14,
	jpxCSsRGB      = 16,
	jpxCSGrayscale = 17,
	jpxCSBiLevel2  = 18,
	jpxCSCIEJab    = 19,
	jpxCSCISesRGB  = 20,
	jpxCSROMMRGB   = 21,
	jpxCSsRGBYCbCr = 22,
	jpxCSYPbPr1125 = 23,
	jpxCSYPbPr1250 = 24
};

struct JPXColorSpecCIELab
{
	uint32_t rl, ol, ra, oa, rb, ob, il;
};

struct JPXColorSpecEnumerated
{
	JPXColorSpaceType type; // color space type

	union
	{
		JPXColorSpecCIELab cieLab;
	};
};

struct JPXColorSpec
{
	uint32_t meth; // method
	int      prec; // precedence

	union
	{
		JPXColorSpecEnumerated enumerated;
	};
};

//------------------------------------------------------------------------

struct JPXPalette
{
	uint32_t  nEntries; // number of entries in the palette
	uint32_t  nComps;   // number of components in each entry
	uint32_t* bpc;      // bits per component, for each component
	int*      c;        // color data: c[i*nComps+j] = entry i, component j
};

//------------------------------------------------------------------------

struct JPXCompMap
{
	uint32_t  nChannels; // number of channels
	uint32_t* comp;      // codestream components mapped to each channel
	uint32_t* type;      // 0 for direct use, 1 for palette mapping
	uint32_t* pComp;     // palette components to use
};

//------------------------------------------------------------------------

struct JPXChannelDefn
{
	uint32_t  nChannels; // number of channels
	uint32_t* idx;       // channel indexes
	uint32_t* type;      // channel types
	uint32_t* assoc;     // channel associations
};

//------------------------------------------------------------------------

struct JPXTagTreeNode
{
	bool     finished; // true if this node is finished
	uint32_t val;      // current value
};

//------------------------------------------------------------------------

struct JPXCodeBlock
{
	//----- size
	uint32_t x0, y0, x1, y1; // bounds

	//----- persistent state
	bool     seen;     // true if this code-block has already been seen
	uint32_t lBlock;   // base number of bits used for pkt data length
	uint32_t nextPass; // next coding pass

	//---- info from first packet
	uint32_t nZeroBitPlanes; // number of zero bit planes

	//----- info for the current packet
	uint32_t  included;      // code-block inclusion in this packet: 0=not included, 1=included
	uint32_t  nCodingPasses; // number of coding passes in this pkt
	uint32_t* dataLen;       // data lengths (one per codeword segment)
	uint32_t  dataLenSize;   // size of the dataLen array

	//----- coefficient data
	int*                     coeffs;       //
	char*                    touched;      // coefficient 'touched' flags
	uint16_t                 len;          // coefficient length
	JArithmeticDecoder*      arithDecoder; // arithmetic decoder
	JArithmeticDecoderStats* stats;        // arithmetic decoder stats
};

//------------------------------------------------------------------------

struct JPXSubband
{
	//----- computed
	uint32_t nXCBs; //
	uint32_t nYCBs; // number of code-blocks in the x and y directions

	//----- tag trees
	uint32_t        maxTTLevel;   // max tag tree level
	JPXTagTreeNode* inclusion;    // inclusion tag tree for each subband
	JPXTagTreeNode* zeroBitPlane; // zero-bit plane tag tree for each subband

	//----- children
	JPXCodeBlock* cbs; // the code-blocks (len = nXCBs * nYCBs)
};

//------------------------------------------------------------------------

struct JPXPrecinct
{
	//----- children
	JPXSubband* subbands; // the subbands
};

//------------------------------------------------------------------------

struct JPXResLevel
{
	//----- from the COD and COC segments (main and tile)
	uint32_t precinctWidth;  // log2(precinct width)
	uint32_t precinctHeight; // log2(precinct height)
	uint32_t nPrecincts;

	//----- computed
	uint32_t x0;         //
	uint32_t y0;         //
	uint32_t x1;         //
	uint32_t y1;         // bounds of this tile-comp at this res level
	uint32_t bx0[3];     //
	uint32_t by0[3];     // subband bounds
	uint32_t bx1[3];     //
	uint32_t by1[3];     //
	uint32_t codeBlockW; // log2(code-block width)
	uint32_t codeBlockH; // log2(code-block height)
	uint32_t cbW;        // code-block width
	uint32_t cbH;        // code-block height
	bool     empty;      // true if all subbands and precincts are zero width or height

	//---- children
	JPXPrecinct* precincts; // the precincts
};

//------------------------------------------------------------------------

struct JPXTileComp
{
	//----- from the SIZ segment
	bool     sgned; // 1 for signed, 0 for unsigned
	uint32_t prec;  // precision, in bits
	uint32_t hSep;  // horizontal separation of samples
	uint32_t vSep;  // vertical separation of samples

	//----- from the COD and COC segments (main and tile)
	uint32_t style;          // coding style parameter (Scod / Scoc)
	uint32_t nDecompLevels;  // number of decomposition levels
	uint32_t codeBlockW;     // log2(code-block width)
	uint32_t codeBlockH;     // log2(code-block height)
	uint32_t codeBlockStyle; // code-block style
	uint32_t transform;      // wavelet transformation

	//----- from the QCD and QCC segments (main and tile)
	uint32_t  quantStyle;  // quantization style
	uint32_t* quantSteps;  // quantization step size for each subband
	uint32_t  nQuantSteps; // number of entries in quantSteps

	//----- computed
	uint32_t x0, y0, x1, y1; // bounds of the tile-comp, in ref coords
	uint32_t x0r, y0r;       // x0 >> reduction, y0 >> reduction
	uint32_t w, h;           // data size = {x1 - x0, y1 - y0} >> reduction

	//----- image data
	int* data; // the decoded image data
	int* buf;  // intermediate buffer for the inverse transform

	//----- children
	JPXResLevel* resLevels; // the resolution levels (len = nDecompLevels + 1)
};

//------------------------------------------------------------------------

struct JPXTile
{
	bool init;

	//----- from the COD segments (main and tile)
	uint32_t progOrder; // progression order
	uint32_t nLayers;   // number of layers
	uint32_t multiComp; // multiple component transformation

	//----- computed
	uint32_t x0, y0, x1, y1;   // bounds of the tile, in ref coords
	uint32_t maxNDecompLevels; // max number of decomposition levels used in any component in this tile
	uint32_t maxNPrecincts;    // max number of precints in any component/res level in this tile

	//----- progression order loop counters
	uint32_t comp;     // component
	uint32_t res;      // resolution level
	uint32_t precinct; // precinct
	uint32_t layer;    // layer
	bool     done;     // set when this tile is done

	//----- tile part info
	uint32_t nextTilePart; // next expected tile-part

	//----- children
	JPXTileComp* tileComps; // the tile-components (len = JPXImage.nComps)
};

//------------------------------------------------------------------------

struct JPXImage
{
	//----- from the SIZ segment
	uint32_t xSize;              //
	uint32_t ySize;              // size of reference grid
	uint32_t xOffset;            //
	uint32_t yOffset;            // image offset
	uint32_t xTileSize;          //
	uint32_t yTileSize;          // size of tiles
	uint32_t xTileOffset;        // offset of first tile
	uint32_t yTileOffset;        //
	uint32_t xSizeR, ySizeR;     // size of reference grid >> reduction
	uint32_t xOffsetR, yOffsetR; // image offset >> reduction
	uint32_t nComps;             // number of components

	//----- computed
	uint32_t nXTiles; // number of tiles in x direction
	uint32_t nYTiles; // number of tiles in y direction

	//----- children
	JPXTile* tiles; // the tiles (len = nXTiles * nYTiles)
};

//------------------------------------------------------------------------

enum JPXDecodeResult
{
	jpxDecodeOk,
	jpxDecodeNonFatalError,
	jpxDecodeFatalError
};

//------------------------------------------------------------------------

class JPXStream : public FilterStream
{
public:
	JPXStream(Stream* strA);
	virtual ~JPXStream();
	virtual Stream* copy();

	virtual StreamKind getKind() { return strJPX; }

	virtual void        reset();
	virtual void        close();
	virtual int         getChar();
	virtual int         lookChar();
	virtual std::string getPSFilter(int psLevel, const char* indent, bool okToReadStream);
	virtual bool        isBinary(bool last = true);

	virtual bool hasStrongCompression() { return true; }

	virtual void getImageParams(int* bitsPerComponent, StreamColorSpaceMode* csMode);

	void reduceResolution(int reductionA) { reduction = reductionA; }

private:
	void            decodeImage();
	void            fillReadBuf();
	void            getImageParams2(int* bitsPerComponent, StreamColorSpaceMode* csMode);
	JPXDecodeResult readBoxes();
	bool            readColorSpecBox(uint32_t dataLen);
	JPXDecodeResult readCodestream(uint32_t len);
	bool            readTilePart();
	bool            readTilePartData(uint32_t tileIdx, uint32_t tilePartLen, bool tilePartToEOC);
	bool            readCodeBlockData(JPXTileComp* tileComp, JPXResLevel* resLevel, JPXPrecinct* precinct, JPXSubband* subband, uint32_t res, uint32_t sb, JPXCodeBlock* cb);
	void            inverseTransform(JPXTileComp* tileComp);
	void            inverseTransformLevel(JPXTileComp* tileComp, uint32_t r, JPXResLevel* resLevel);
	void            inverseTransform1D(JPXTileComp* tileComp, int* data, uint32_t offset, uint32_t n);
	bool            inverseMultiCompAndDC(JPXTile* tile);
	bool            readBoxHdr(uint32_t* boxType, uint32_t* boxLen, uint32_t* dataLen);
	int             readMarkerHdr(int* segType, uint32_t* segLen);
	bool            readUByte(uint32_t* x);
	bool            readByte(int* x);
	bool            readUWord(uint32_t* x);
	bool            readULong(uint32_t* x);
	bool            readNBytes(int nBytes, bool signd, int* x);
	void            startBitBuf(uint32_t byteCountA);
	bool            readBits(int nBits, uint32_t* x);
	void            skipSOP();
	void            skipEPH();
	uint32_t        finishBitBuf();

	BufStream*     bufStr;          // buffered stream (for lookahead)
	bool           decoded;         // set when the image has been decoded
	uint32_t       nComps;          // number of components
	uint32_t*      bpc;             // bits per component, for each component
	uint32_t       width;           //
	uint32_t       height;          // image size
	int            reduction;       // log2(reduction in resolution)
	bool           haveImgHdr;      // set if a JP2/JPX image header has been found
	JPXColorSpec   cs;              // color specification
	bool           haveCS;          // set if a color spec has been found
	JPXPalette     palette;         // the palette
	bool           havePalette;     // set if a palette has been found
	JPXCompMap     compMap;         // the component mapping
	bool           haveCompMap;     // set if a component mapping has been found
	JPXChannelDefn channelDefn;     // channel definition
	bool           haveChannelDefn; // set if a channel defn has been found
	JPXImage       img;             // JPEG2000 decoder data
	uint32_t       bitBuf;          // buffer for bit reads
	int            bitBufLen;       // number of bits in bitBuf
	bool           bitBufSkip;      // true if next bit should be skipped (for bit stuffing)
	uint32_t       byteCount;       // number of available bytes left
	uint32_t       curX;            //
	uint32_t       curY;            //
	uint32_t       curComp;         // current position for lookChar/getChar
	uint32_t       readBuf;         // read buffer
	uint32_t       readBufLen;      // number of valid bits in readBuf
};
