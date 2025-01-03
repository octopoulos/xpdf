//========================================================================
//
// ImageOutputDev.cc
//
// Copyright 1998-2003 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>
#include <math.h>
#include "gmem.h"
#include "gmempp.h"
#include "config.h"
#include "Error.h"
#include "GfxState.h"
#include "Gfx.h"
#include "Object.h"
#include "Stream.h"
#include "ImageOutputDev.h"

ImageOutputDev::ImageOutputDev(std::string_view fileRootA, bool dumpJPEGA, bool dumpRawA, bool listA)
{
	fileRoot = fileRootA;
	dumpJPEG = dumpJPEGA;
	dumpRaw  = dumpRawA;
	list     = listA;
	ok       = true;
}

ImageOutputDev::~ImageOutputDev()
{
}

void ImageOutputDev::startPage(int pageNum, GfxState* state)
{
	curPageNum = pageNum;
}

void ImageOutputDev::tilingPatternFill(GfxState* state, Gfx* gfx, Object* strRef, int paintType, int tilingType, Dict* resDict, double* mat, double* bbox, int x0, int y0, int x1, int y1, double xStep, double yStep)
{
	// draw the tile once, just in case it contains image(s)
	gfx->drawForm(strRef, resDict, mat, bbox);
}

void ImageOutputDev::drawImageMask(GfxState* state, Object* ref, Stream* str, int width, int height, bool invert, bool inlineImg, bool interpolate)
{
	std::string fileName;
	FILE*       f;
	char        buf[4096];

	// dump raw file
	if (dumpRaw && !inlineImg)
	{
		// open the image file
		fileName = fmt::format("{}-{:04}.{}", fileRoot, imgNum, getRawFileExtension(str));
		++imgNum;
		if (!(f = openFile(fileName.c_str(), "wb")))
		{
			error(errIO, -1, "Couldn't open image file '{}'", fileName);
			return;
		}

		// initialize stream
		str = getRawStream(str);
		str->reset();

		// copy the stream
		int n;
		while ((n = str->getBlock(buf, sizeof(buf))) > 0)
			fwrite(buf, 1, n, f);

		str->close();
		fclose(f);

		// dump JPEG file
	}
	else if (dumpJPEG && str->getKind() == strDCT && !inlineImg)
	{
		// open the image file
		fileName = fmt::format("{}-{:04}.jpg", fileRoot, imgNum);
		++imgNum;
		if (!(f = openFile(fileName.c_str(), "wb")))
		{
			error(errIO, -1, "Couldn't open image file '{}'", fileName);
			return;
		}

		// initialize stream
		str = ((DCTStream*)str)->getRawStream();
		str->reset();

		// copy the stream
		int n;
		while ((n = str->getBlock(buf, sizeof(buf))) > 0)
			fwrite(buf, 1, n, f);

		str->close();
		fclose(f);

		// dump PBM file
	}
	else
	{
		// open the image file and write the PBM header
		fileName = fmt::format("{}-{:04}.pbm", fileRoot, imgNum);
		++imgNum;
		if (!(f = openFile(fileName.c_str(), "wb")))
		{
			error(errIO, -1, "Couldn't open image file '{}'", fileName);
			return;
		}
		fprintf(f, "P4\n");
		fprintf(f, "%d %d\n", width, height);

		// initialize stream
		str->reset();

		// copy the stream
		int size = height * ((width + 7) / 8);
		while (size > 0)
		{
			const int i = size < (int)sizeof(buf) ? size : (int)sizeof(buf);
			const int n = str->getBlock(buf, i);
			fwrite(buf, 1, n, f);
			if (n < i) break;
			size -= n;
		}

		str->close();
		fclose(f);
	}

	if (list) writeImageInfo(fileName, width, height, state, nullptr);
}

void ImageOutputDev::drawImage(GfxState* state, Object* ref, Stream* str, int width, int height, GfxImageColorMap* colorMap, int* maskColors, bool inlineImg, bool interpolate)
{
	FILE*        f;
	ImageStream* imgStr;
	uint8_t*     p;
	GfxRGB       rgb;
	GfxGray      gray;
	char         buf[4096];
	int          size, n, i, j;

	std::string fileName;

	GfxColorSpaceMode csMode = colorMap->getColorSpace()->getMode();
	if (csMode == csIndexed)
		csMode = ((GfxIndexedColorSpace*)colorMap->getColorSpace())->getBase()->getMode();

	// dump raw file
	if (dumpRaw && !inlineImg)
	{
		// open the image file
		fileName = fmt::format("{}-{:04}.{}", fileRoot, imgNum, getRawFileExtension(str));
		++imgNum;
		if (!(f = openFile(fileName.c_str(), "wb")))
		{
			error(errIO, -1, "Couldn't open image file '{}'", fileName);
			return;
		}

		// initialize stream
		str = getRawStream(str);
		str->reset();

		// copy the stream
		while ((n = str->getBlock(buf, sizeof(buf))) > 0)
			fwrite(buf, 1, n, f);

		str->close();
		fclose(f);

		// dump JPEG file
	}
	else if (dumpJPEG && str->getKind() == strDCT && (colorMap->getNumPixelComps() == 1 || colorMap->getNumPixelComps() == 3) && !inlineImg)
	{
		// open the image file
		fileName = fmt::format("{}-{:04}.jpg", fileRoot, imgNum);
		++imgNum;
		if (!(f = openFile(fileName.c_str(), "wb")))
		{
			error(errIO, -1, "Couldn't open image file '{}'", fileName);
			return;
		}

		// initialize stream
		str = ((DCTStream*)str)->getRawStream();
		str->reset();

		// copy the stream
		while ((n = str->getBlock(buf, sizeof(buf))) > 0)
			fwrite(buf, 1, n, f);

		str->close();
		fclose(f);

		// dump PBM file
	}
	else if (colorMap->getNumPixelComps() == 1 && colorMap->getBits() == 1)
	{
		// open the image file and write the PBM header
		fileName = fmt::format("{}-{:04}.pbm", fileRoot, imgNum);
		++imgNum;
		if (!(f = openFile(fileName.c_str(), "wb")))
		{
			error(errIO, -1, "Couldn't open image file '{}'", fileName);
			return;
		}
		fprintf(f, "P4\n");
		fprintf(f, "%d %d\n", width, height);

		// initialize stream
		str->reset();

		// copy the stream
		size = height * ((width + 7) / 8);
		while (size > 0)
		{
			i = size < (int)sizeof(buf) ? size : (int)sizeof(buf);
			n = str->getBlock(buf, i);
			for (j = 0; j < n; ++j)
				buf[j] = (char)(buf[j] ^ 0xff);
			fwrite(buf, 1, n, f);
			if (n < i)
				break;
			size -= n;
		}

		str->close();
		fclose(f);

		// dump PGM file
	}
	else if (colorMap->getNumPixelComps() == 1 && (csMode == csDeviceGray || csMode == csCalGray))
	{
		// open the image file and write the PGM header
		fileName = fmt::format("{}-{:04}.pgm", fileRoot, imgNum);
		++imgNum;
		if (!(f = openFile(fileName.c_str(), "wb")))
		{
			error(errIO, -1, "Couldn't open image file '{}'", fileName);
			return;
		}
		fprintf(f, "P5\n");
		fprintf(f, "%d %d\n", width, height);
		fprintf(f, "255\n");

		// initialize stream
		imgStr = new ImageStream(str, width, colorMap->getNumPixelComps(), colorMap->getBits());
		imgStr->reset();

		// for each line...
		for (int y = 0; y < height; ++y)
		{
			// write the line
			if ((p = imgStr->getLine()))
			{
				for (int x = 0; x < width; ++x)
				{
					colorMap->getGray(p, &gray, state->getRenderingIntent());
					fputc(colToByte(gray), f);
					++p;
				}
			}
			else
			{
				for (int x = 0; x < width; ++x)
					fputc(0, f);
			}
		}

		imgStr->close();
		delete imgStr;

		fclose(f);

		// dump PPM file
	}
	else
	{
		// open the image file and write the PPM header
		fileName = fmt::format("{}-{:04}.ppm", fileRoot, imgNum);
		++imgNum;
		if (!(f = openFile(fileName.c_str(), "wb")))
		{
			error(errIO, -1, "Couldn't open image file '{}'", fileName);
			return;
		}
		fprintf(f, "P6\n");
		fprintf(f, "%d %d\n", width, height);
		fprintf(f, "255\n");

		// initialize stream
		imgStr = new ImageStream(str, width, colorMap->getNumPixelComps(), colorMap->getBits());
		imgStr->reset();

		// for each line...
		for (int y = 0; y < height; ++y)
		{
			// write the line
			if ((p = imgStr->getLine()))
			{
				for (int x = 0; x < width; ++x)
				{
					colorMap->getRGB(p, &rgb, state->getRenderingIntent());
					fputc(colToByte(rgb.r), f);
					fputc(colToByte(rgb.g), f);
					fputc(colToByte(rgb.b), f);
					p += colorMap->getNumPixelComps();
				}
			}
			else
			{
				for (int x = 0; x < width; ++x)
				{
					fputc(0, f);
					fputc(0, f);
					fputc(0, f);
				}
			}
		}

		imgStr->close();
		delete imgStr;

		fclose(f);
	}

	if (list)
		writeImageInfo(fileName, width, height, state, colorMap);
}

void ImageOutputDev::drawMaskedImage(GfxState* state, Object* ref, Stream* str, int width, int height, GfxImageColorMap* colorMap, Object* maskRef, Stream* maskStr, int maskWidth, int maskHeight, bool maskInvert, bool interpolate)
{
	drawImage(state, ref, str, width, height, colorMap, nullptr, false, interpolate);
	drawImageMask(state, maskRef, maskStr, maskWidth, maskHeight, maskInvert, false, interpolate);
}

void ImageOutputDev::drawSoftMaskedImage(GfxState* state, Object* ref, Stream* str, int width, int height, GfxImageColorMap* colorMap, Object* maskRef, Stream* maskStr, int maskWidth, int maskHeight, GfxImageColorMap* maskColorMap, double* matte, bool interpolate)
{
	drawImage(state, ref, str, width, height, colorMap, nullptr, false, interpolate);
	drawImage(state, maskRef, maskStr, maskWidth, maskHeight, maskColorMap, nullptr, false, interpolate);
}

Stream* ImageOutputDev::getRawStream(Stream* str)
{
	switch (str->getKind())
	{
	case strLZW:
	case strRunLength:
	case strCCITTFax:
	case strDCT:
	case strFlate:
	case strJBIG2:
	case strJPX:
		return ((FilterStream*)str)->getNextStream();
	default:
		return str;
	}
}

const char* ImageOutputDev::getRawFileExtension(Stream* str)
{
	switch (str->getKind())
	{
	case strLZW: return "lzw";
	case strRunLength: return "rle";
	case strCCITTFax: return "fax";
	case strDCT: return "jpg";
	case strFlate: return "flate";
	case strJBIG2: return "jb2";
	case strJPX: return "jpx";
	default: return "unknown";
	}
}

void ImageOutputDev::writeImageInfo(const std::string& fileName, int width, int height, GfxState* state, GfxImageColorMap* colorMap)
{
	const char* mode;
	double      hdpi, vdpi, x0, y0, x1, y1;
	int         bpc;

	// this works for 0/90/180/270-degree rotations, along with
	// horizontal/vertical flips
	state->transformDelta(1, 0, &x0, &y0);
	state->transformDelta(0, 1, &x1, &y1);
	x0 = fabs(x0);
	y0 = fabs(y0);
	x1 = fabs(x1);
	y1 = fabs(y1);
	if (x0 > y0)
	{
		hdpi = (72 * width) / x0;
		vdpi = (72 * height) / y1;
	}
	else
	{
		hdpi = (72 * height) / x1;
		vdpi = (72 * width) / y0;
	}

	if (colorMap)
	{
		mode = GfxColorSpace::getColorSpaceModeName(
		    colorMap->getColorSpace()->getMode());
		bpc = colorMap->getBits();
	}
	else
	{
		mode = nullptr;
		bpc  = 1;
	}

	printf("%s: page=%d width=%d height=%d hdpi=%.2f vdpi=%.2f %s%s bpc=%d\n", fileName.c_str(), curPageNum, width, height, hdpi, vdpi, mode ? "colorspace=" : "mask", mode ? mode : "", bpc);
}
