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
	uint32_t rl; //
	uint32_t ol; //
	uint32_t ra; //
	uint32_t oa; //
	uint32_t rb; //
	uint32_t ob; //
	uint32_t il; //
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
	uint32_t  nEntries = 0;       // number of entries in the palette
	uint32_t  nComps   = 0;       // number of components in each entry
	uint32_t* bpc      = nullptr; // bits per component, for each component
	int*      c        = nullptr; // color data: c[i*nComps+j] = entry i, component j
};

//------------------------------------------------------------------------

struct JPXCompMap
{
	uint32_t  nChannels = 0;       // number of channels
	uint32_t* comp      = nullptr; // codestream components mapped to each channel
	uint32_t* type      = nullptr; // 0 for direct use, 1 for palette mapping
	uint32_t* pComp     = nullptr; // palette components to use
};

//------------------------------------------------------------------------

struct JPXChannelDefn
{
	uint32_t  nChannels = 0;       // number of channels
	uint32_t* idx       = nullptr; // channel indexes
	uint32_t* type      = nullptr; // channel types
	uint32_t* assoc     = nullptr; // channel associations
};

//------------------------------------------------------------------------

struct JPXTagTreeNode
{
	bool     finished = false; // true if this node is finished
	uint32_t val      = 0;     // current value
};

//------------------------------------------------------------------------

struct JPXCodeBlock
{
	//----- size
	uint32_t x0 = 0; //
	uint32_t y0 = 0; //
	uint32_t x1 = 0; //
	uint32_t y1 = 0; // bounds

	//----- persistent state
	bool     seen     = false; // true if this code-block has already been seen
	uint32_t lBlock   = 0;     // base number of bits used for pkt data length
	uint32_t nextPass = 0;     // next coding pass

	//---- info from first packet
	uint32_t nZeroBitPlanes = 0; // number of zero bit planes

	//----- info for the current packet
	uint32_t  included      = 0;       // code-block inclusion in this packet: 0=not included, 1=included
	uint32_t  nCodingPasses = 0;       // number of coding passes in this pkt
	uint32_t* dataLen       = nullptr; // data lengths (one per codeword segment)
	uint32_t  dataLenSize   = 0;       // size of the dataLen array

	//----- coefficient data
	int*                     coeffs       = nullptr; //
	char*                    touched      = nullptr; // coefficient 'touched' flags
	uint16_t                 len          = 0;       // coefficient length
	JArithmeticDecoder*      arithDecoder = nullptr; // arithmetic decoder
	JArithmeticDecoderStats* stats        = nullptr; // arithmetic decoder stats
};

//------------------------------------------------------------------------

struct JPXSubband
{
	//----- computed
	uint32_t nXCBs = 0; //
	uint32_t nYCBs = 0; // number of code-blocks in the x and y directions

	//----- tag trees
	uint32_t        maxTTLevel   = 0;       // max tag tree level
	JPXTagTreeNode* inclusion    = nullptr; // inclusion tag tree for each subband
	JPXTagTreeNode* zeroBitPlane = nullptr; // zero-bit plane tag tree for each subband

	//----- children
	JPXCodeBlock* cbs = nullptr; // the code-blocks (len = nXCBs * nYCBs)
};

//------------------------------------------------------------------------

struct JPXPrecinct
{
	//----- children
	JPXSubband* subbands = nullptr; // the subbands
};

//------------------------------------------------------------------------

struct JPXResLevel
{
	//----- from the COD and COC segments (main and tile)
	uint32_t precinctWidth  = 0; // log2(precinct width)
	uint32_t precinctHeight = 0; // log2(precinct height)
	uint32_t nPrecincts     = 0;

	//----- computed
	uint32_t x0         = 0;     //
	uint32_t y0         = 0;     //
	uint32_t x1         = 0;     //
	uint32_t y1         = 0;     // bounds of this tile-comp at this res level
	uint32_t bx0[3]     = {};    //
	uint32_t by0[3]     = {};    // subband bounds
	uint32_t bx1[3]     = {};    //
	uint32_t by1[3]     = {};    //
	uint32_t codeBlockW = 0;     // log2(code-block width)
	uint32_t codeBlockH = 0;     // log2(code-block height)
	uint32_t cbW        = 0;     // code-block width
	uint32_t cbH        = 0;     // code-block height
	bool     empty      = false; // true if all subbands and precincts are zero width or height

	//---- children
	JPXPrecinct* precincts = nullptr; // the precincts
};

//------------------------------------------------------------------------

struct JPXTileComp
{
	//----- from the SIZ segment
	bool     sgned = false; // 1 for signed, 0 for unsigned
	uint32_t prec  = 0;     // precision, in bits
	uint32_t hSep  = 0;     // horizontal separation of samples
	uint32_t vSep  = 0;     // vertical separation of samples

	//----- from the COD and COC segments (main and tile)
	uint32_t style          = 0; // coding style parameter (Scod / Scoc)
	uint32_t nDecompLevels  = 0; // number of decomposition levels
	uint32_t codeBlockW     = 0; // log2(code-block width)
	uint32_t codeBlockH     = 0; // log2(code-block height)
	uint32_t codeBlockStyle = 0; // code-block style
	uint32_t transform      = 0; // wavelet transformation

	//----- from the QCD and QCC segments (main and tile)
	uint32_t  quantStyle  = 0;       // quantization style
	uint32_t* quantSteps  = nullptr; // quantization step size for each subband
	uint32_t  nQuantSteps = 0;       // number of entries in quantSteps

	//----- computed
	uint32_t x0  = 0; //
	uint32_t y0  = 0; //
	uint32_t x1  = 0; //
	uint32_t y1  = 0; // bounds of the tile-comp, in ref coords
	uint32_t x0r = 0; //
	uint32_t y0r = 0; // x0 >> reduction, y0 >> reduction
	uint32_t w   = 0; //
	uint32_t h   = 0; // data size = {x1 - x0, y1 - y0} >> reduction

	//----- image data
	int* data = nullptr; // the decoded image data
	int* buf  = nullptr; // intermediate buffer for the inverse transform

	//----- children
	JPXResLevel* resLevels = nullptr; // the resolution levels (len = nDecompLevels + 1)
};

//------------------------------------------------------------------------

struct JPXTile
{
	bool init = false;

	//----- from the COD segments (main and tile)
	uint32_t progOrder = 0; // progression order
	uint32_t nLayers   = 0; // number of layers
	uint32_t multiComp = 0; // multiple component transformation

	//----- computed
	uint32_t x0               = 0; //
	uint32_t y0               = 0; //
	uint32_t x1               = 0; //
	uint32_t y1               = 0; // bounds of the tile, in ref coords
	uint32_t maxNDecompLevels = 0; // max number of decomposition levels used in any component in this tile
	uint32_t maxNPrecincts    = 0; // max number of precints in any component/res level in this tile

	//----- progression order loop counters
	uint32_t comp     = 0;     // component
	uint32_t res      = 0;     // resolution level
	uint32_t precinct = 0;     // precinct
	uint32_t layer    = 0;     // layer
	bool     done     = false; // set when this tile is done

	//----- tile part info
	uint32_t nextTilePart = 0; // next expected tile-part

	//----- children
	JPXTileComp* tileComps = nullptr; // the tile-components (len = JPXImage.nComps)
};

//------------------------------------------------------------------------

struct JPXImage
{
	//----- from the SIZ segment
	uint32_t xSize       = 0; //
	uint32_t ySize       = 0; // size of reference grid
	uint32_t xOffset     = 0; //
	uint32_t yOffset     = 0; // image offset
	uint32_t xTileSize   = 0; //
	uint32_t yTileSize   = 0; // size of tiles
	uint32_t xTileOffset = 0; // offset of first tile
	uint32_t yTileOffset = 0; //
	uint32_t xSizeR      = 0; //
	uint32_t ySizeR      = 0; // size of reference grid >> reduction
	uint32_t xOffsetR    = 0;
	uint32_t yOffsetR    = 0; // image offset >> reduction
	uint32_t nComps      = 0; // number of components

	//----- computed
	uint32_t nXTiles = 0; // number of tiles in x direction
	uint32_t nYTiles = 0; // number of tiles in y direction

	//----- children
	JPXTile* tiles = nullptr; // the tiles (len = nXTiles * nYTiles)
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

	BufStream*     bufStr          = nullptr; // buffered stream (for lookahead)
	bool           decoded         = false;   // set when the image has been decoded
	uint32_t       nComps          = 0;       // number of components
	uint32_t*      bpc             = nullptr; // bits per component, for each component
	uint32_t       width           = 0;       //
	uint32_t       height          = 0;       // image size
	int            reduction       = 0;       // log2(reduction in resolution)
	bool           haveImgHdr      = false;   // set if a JP2/JPX image header has been found
	JPXColorSpec   cs              = {};      // color specification
	bool           haveCS          = false;   // set if a color spec has been found
	JPXPalette     palette         = {};      // the palette
	bool           havePalette     = false;   // set if a palette has been found
	JPXCompMap     compMap         = {};      // the component mapping
	bool           haveCompMap     = false;   // set if a component mapping has been found
	JPXChannelDefn channelDefn     = {};      // channel definition
	bool           haveChannelDefn = false;   // set if a channel defn has been found
	JPXImage       img             = {};      // JPEG2000 decoder data
	uint32_t       bitBuf          = 0;       // buffer for bit reads
	int            bitBufLen       = 0;       // number of bits in bitBuf
	bool           bitBufSkip      = false;   // true if next bit should be skipped (for bit stuffing)
	uint32_t       byteCount       = 0;       // number of available bytes left
	uint32_t       curX            = 0;       //
	uint32_t       curY            = 0;       //
	uint32_t       curComp         = 0;       // current position for lookChar/getChar
	uint32_t       readBuf         = 0;       // read buffer
	uint32_t       readBufLen      = 0;       // number of valid bits in readBuf
};
