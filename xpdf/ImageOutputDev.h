//========================================================================
//
// ImageOutputDev.h
//
// Copyright 1998-2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include <stdio.h>
#include "OutputDev.h"

class GfxImageColorMap;
class GfxState;
class Object;

//------------------------------------------------------------------------
// ImageOutputDev
//------------------------------------------------------------------------

class ImageOutputDev : public OutputDev
{
public:
	// Create an OutputDev which will write images to files named <fileRoot>-NNN.<type>.
	// Normally, all images are written as PBM (.pbm) or PPM (.ppm) files.
	// If <dumpJPEG> is set, JPEG images are written as JPEG (.jpg) files.
	// If <dumpRaw> is set, all images are written in PDF-native formats.
	// If <list> is set, a one-line summary will be written to stdout for each image.
	ImageOutputDev(std::string_view fileRootA, bool dumpJPEGA, bool dumpRawA, bool listA);

	// Destructor.
	virtual ~ImageOutputDev();

	// Check if file was successfully created.
	virtual bool isOk() { return ok; }

	// Does this device use tilingPatternFill()?
	// If this returns false, tiling pattern fills will be reduced to a series of other drawing operations.
	virtual bool useTilingPatternFill() { return true; }

	// Does this device use beginType3Char/endType3Char?
	// Otherwise, text in Type 3 fonts will be drawn with drawChar/drawString.
	virtual bool interpretType3Chars() { return false; }

	// Does this device need non-text content?
	virtual bool needNonText() { return true; }

	//---- get info about output device

	// Does this device use upside-down coordinates?
	// (Upside-down means (0,0) is the top left corner of the page.)
	virtual bool upsideDown() { return true; }

	// Does this device use drawChar() or drawString()?
	virtual bool useDrawChar() { return false; }

	//----- initialization and control
	virtual void startPage(int pageNum, GfxState* state);

	//----- path painting
	virtual void tilingPatternFill(GfxState* state, Gfx* gfx, Object* strRef, int paintType, int tilingType, Dict* resDict, double* mat, double* bbox, int x0, int y0, int x1, int y1, double xStep, double yStep);

	//----- image drawing
	virtual void drawImageMask(GfxState* state, Object* ref, Stream* str, int width, int height, bool invert, bool inlineImg, bool interpolate);
	virtual void drawImage(GfxState* state, Object* ref, Stream* str, int width, int height, GfxImageColorMap* colorMap, int* maskColors, bool inlineImg, bool interpolate);
	virtual void drawMaskedImage(GfxState* state, Object* ref, Stream* str, int width, int height, GfxImageColorMap* colorMap, Object* maskRef, Stream* maskStr, int maskWidth, int maskHeight, bool maskInvert, bool interpolate);
	virtual void drawSoftMaskedImage(GfxState* state, Object* ref, Stream* str, int width, int height, GfxImageColorMap* colorMap, Object* maskRef, Stream* maskStr, int maskWidth, int maskHeight, GfxImageColorMap* maskColorMap, double* matte, bool interpolate);

private:
	Stream*     getRawStream(Stream* str);
	const char* getRawFileExtension(Stream* str);
	void        writeImageInfo(const std::string& fileName, int width, int height, GfxState* state, GfxImageColorMap* colorMap);

	std::string fileRoot   = "";    // root of output file names
	bool        dumpJPEG   = false; // set to dump native JPEG files
	bool        dumpRaw    = false; // set to dump raw PDF-native image files
	bool        list       = false; // set to write image info to stdout
	int         imgNum     = 0;     // current image number
	int         curPageNum = 0;     // current page number
	bool        ok         = false; // set up ok?
};
