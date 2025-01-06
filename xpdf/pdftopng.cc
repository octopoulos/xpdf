//========================================================================
//
// pdftopng.cc
//
// Copyright 2009 Glyph & Cog, LLC
// updated by octopoulos @ 2024-12-29
//
//========================================================================

// --cfgFileName "c:/web/gori/data/xpdf.cfg" --gray --useStb -- "c:/web/gori/source/pdf/bug.pdf" "c:/web/gori/source/pdf/bug"

#include <aconf.h>
#ifdef _WIN32
#	include <fcntl.h>
#endif
#include "Error.h"
#include "GlobalParams.h"
#include "Object.h"
#include "PDFDoc.h"
#include "SplashBitmap.h"
#include "SplashOutputDev.h"

#include <CLI/CLI.hpp>
#include <png.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void SavePng(const std::string& filename, SplashBitmap* bitmap, int bitDepth, int channels);
static void setupPNG(png_structp* png, png_infop* pngInfo, FILE* f, int bitDepth, int colorType, double res, SplashBitmap* bitmap);
static void writePNGData(png_structp png, SplashBitmap* bitmap, int pngAlpha);
static void finishPNG(png_structp* png, png_infop* pngInfo);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
	// OPTIONS
	//////////

	CLI_BEGIN("pdftopng");

	// name, type, default, implicit, needApp, help
	// clang-format off
	CLI_OPTION(antialiasYN  , std::string, "" , "", 0, "enable font anti-aliasing: yes, no");
	CLI_OPTION(cfgFileName  , std::string, "" , "", 0, "configuration file to use in place of .xpdfrc");
	CLI_OPTION(firstPage    , int        , 1  , 1 , 0, "first page to convert");
	CLI_OPTION(freetypeYN   , std::string, "" , "", 0, "enable FreeType font rasterizer: yes, no");
	CLI_OPTION(gray         , int        , 0  , 1 , 0, "generate a grayscale PNG file");
	CLI_OPTION(lastPage     , int        , 0  , 0 , 0, "last page to convert");
	CLI_OPTION(mono         , int        , 0  , 1 , 0, "generate a monochrome PNG file");
	CLI_OPTION(ownerPassword, std::string, "" , "", 0, "owner password (for encrypted files)");
	CLI_OPTION(pngAlpha     , int        , 0  , 1 , 0, "include an alpha channel in the PNG file");
	CLI_OPTION(printVersion , int        , 0  , 1 , 0, "print copyright and version info");
	CLI_OPTION(quiet        , int        , 0  , 1 , 0, "don't print any messages or errors");
	CLI_OPTION(resolution   , int        , 150, 0 , 0, "resolution, in DPI (default is 150)");
	CLI_OPTION(rotate       , int        , 0  , 0 , 0, "set page rotation: 0, 90, 180, or 270");
	CLI_OPTION(useStb       , int        , 0  , 1 , 0, "use STB library");
	CLI_OPTION(userPassword , std::string, "" , "", 0, "user password (for encrypted files)");
	CLI_OPTION(vectoraaYN   , std::string, "" , "", 0, "enable vector anti-aliasing: yes, no");
	CLI_OPTION(verbose      , int        , 0  , 1 , 0, "print per-page status information");
	//
	CLI_REQUIRED(fileName, std::string, 1, "<PDF-file>");
	CLI_REQUIRED(pngRoot , std::string, 1, "<PNG-root>");
	// clang-format on

	CLI11_PARSE(cli, argc, argv);
	CLI_NEEDAPP();

	// config file
	{
		if (CLI_STRING(cfgFileName) && !std::filesystem::exists(cfgFileName))
			error(errConfig, -1, "Config file '{}' doesn't exist or isn't a file", cfgFileName);

		globalParams = std::make_shared<GlobalParams>(cfgFileName.c_str());
		globalParams->setupBaseFonts(nullptr);
		if (CLI_STRING(freetypeYN))
		{
			if (!globalParams->setEnableFreeType(freetypeYN.c_str()))
				fprintf(stderr, "Bad '-freetype' value on command line\n");
		}
		if (CLI_STRING(antialiasYN))
		{
			if (!globalParams->setAntialias(antialiasYN.c_str()))
				fprintf(stderr, "Bad '-aa' value on command line\n");
		}
		if (CLI_STRING(vectoraaYN))
		{
			if (!globalParams->setVectorAntialias(vectoraaYN.c_str()))
				fprintf(stderr, "Bad '-aaVector' value on command line\n");
		}
		if (verbose) globalParams->setPrintStatusInfo(verbose);
		if (quiet) globalParams->setErrQuiet(quiet);
	}

	if (gray && mono)
	{
		fprintf(stderr, "Specify one of -mono or -gray\n");
		return 99;
	}
	if (mono && pngAlpha)
	{
		fprintf(stderr, "The -alpha flag cannot be used with -mono\n");
		return 99;
	}

	png_structp png;
	png_infop   pngInfo;
	FILE*       f;

	// open PDF file
	auto doc = std::make_unique<PDFDoc>(fileName, ownerPassword, userPassword);
	if (!doc->isOk()) return 1;

	// get page range
	if (firstPage < 1) firstPage = 1;
	if (lastPage < 1 || lastPage > doc->getNumPages())
		lastPage = doc->getNumPages();

	// check for stdout; set up to print per-page status info
	const bool toStdout        = (pngRoot == "-");
	const bool printStatusInfo = !toStdout && globalParams->getPrintStatusInfo();

	// write PNG files
	SplashColor      paperColor;
	SplashOutputDev* splashOut;
	if (mono)
	{
		paperColor[0] = 0xff;
		splashOut     = new SplashOutputDev(splashModeMono1, 1, false, paperColor);
	}
	else if (gray)
	{
		paperColor[0] = 0xff;
		splashOut     = new SplashOutputDev(splashModeMono8, 1, false, paperColor);
	}
	else
	{
		paperColor[0] = 0xff;
		paperColor[1] = 0xff;
		paperColor[2] = 0xff;
		splashOut     = new SplashOutputDev(splashModeRGB8, 1, false, paperColor);
	}
	if (pngAlpha) splashOut->setNoComposite(true);
	splashOut->startDoc(doc->getXRef());
	for (int pg = firstPage; pg <= lastPage; ++pg)
	{
		if (printStatusInfo)
		{
			fflush(stderr);
			printf("[processing page %d]\n", pg);
			fflush(stdout);
		}

		const auto pngFile = fmt::format("{}-{:06}.png", pngRoot, pg);

		doc->displayPage(splashOut, pg, resolution, resolution, rotate, false, true, false);
		if (mono)
		{
			if (toStdout)
			{
				f = stdout;
#ifdef _WIN32
				_setmode(_fileno(f), _O_BINARY);
#endif
			}
			else if (useStb)
				SavePng(pngFile, splashOut->getBitmap(), 8, pngAlpha ? 4 : 3);
			else
			{
				if (!(f = openFile(pngFile.c_str(), "wb"))) exit(2);
				setupPNG(&png, &pngInfo, f, 1, PNG_COLOR_TYPE_GRAY, resolution, splashOut->getBitmap());
				writePNGData(png, splashOut->getBitmap(), pngAlpha);
				finishPNG(&png, &pngInfo);
				fclose(f);
			}
		}
		else if (gray)
		{
			if (toStdout)
			{
				f = stdout;
#ifdef _WIN32
				_setmode(_fileno(f), _O_BINARY);
#endif
			}
			else if (useStb)
				SavePng(pngFile, splashOut->getBitmap(), 8, pngAlpha ? 4 : 3);
			else
			{
				if (!(f = openFile(pngFile.c_str(), "wb"))) exit(2);
				setupPNG(&png, &pngInfo, f, 8, pngAlpha ? PNG_COLOR_TYPE_GRAY_ALPHA : PNG_COLOR_TYPE_GRAY, resolution, splashOut->getBitmap());
				writePNGData(png, splashOut->getBitmap(), pngAlpha);
				finishPNG(&png, &pngInfo);
				fclose(f);
			}
		}
		else
		{
			// RGB
			if (toStdout)
			{
				f = stdout;
#ifdef _WIN32
				_setmode(_fileno(f), _O_BINARY);
#endif
			}
			else if (useStb)
				SavePng(pngFile, splashOut->getBitmap(), 8, pngAlpha ? 4 : 3);
			else
			{
				if (!(f = openFile(pngFile.c_str(), "wb"))) exit(2);
				setupPNG(&png, &pngInfo, f, 8, pngAlpha ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB, resolution, splashOut->getBitmap());
				writePNGData(png, splashOut->getBitmap(), pngAlpha);
				finishPNG(&png, &pngInfo);
				fclose(f);
			}
		}
	}
	delete splashOut;

	// check for memory leaks
	Object::memCheck(stderr);
	gMemReport(stderr);
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void SavePng(const std::string& filename, SplashBitmap* bitmap, int bitDepth, int channels)
{
	const int width      = bitmap->getWidth();
	const int height     = bitmap->getHeight();
	uint8_t*  image_data = new uint8_t[width * height * channels];
	uint8_t*  p          = bitmap->getDataPtr();

	// Fill the image data with some pattern (e.g., a gradient)
	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			const int index       = (y * width + x) * channels;
			image_data[index + 0] = *p++;
			image_data[index + 1] = *p++;
			image_data[index + 2] = *p++;
			// image_data[index + 3] = *p++;
		}
	}

	stbi_write_png(filename.c_str(), width, height, channels, image_data, width * channels);

	// Clean up
	delete[] image_data;
}

static void setupPNG(png_structp* png, png_infop* pngInfo, FILE* f, int bitDepth, int colorType, double res, SplashBitmap* bitmap)
{
	if (!(*png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr))) exit(2);
	if (!(*pngInfo = png_create_info_struct(*png))) exit(2);
	if (setjmp(png_jmpbuf(*png))) exit(2);

	printf("f=%p", f);

	png_init_io(*png, f);
	png_set_IHDR(*png, *pngInfo, bitmap->getWidth(), bitmap->getHeight(), bitDepth, colorType, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	if (colorType == PNG_COLOR_TYPE_GRAY_ALPHA || colorType == PNG_COLOR_TYPE_RGB_ALPHA)
	{
		png_color_16 background;
		background.index = 0;
		background.red   = 0xff;
		background.green = 0xff;
		background.blue  = 0xff;
		background.gray  = 0xff;
		png_set_bKGD(*png, *pngInfo, &background);
	}

	const int pixelsPerMeter = (int)(res * (1000 / 25.4) + 0.5);
	png_set_pHYs(*png, *pngInfo, pixelsPerMeter, pixelsPerMeter, PNG_RESOLUTION_METER);
	png_write_info(*png, *pngInfo);
}

static void writePNGData(png_structp png, SplashBitmap* bitmap, int pngAlpha)
{
	if (setjmp(png_jmpbuf(png))) exit(2);
	uint8_t* p = bitmap->getDataPtr();
	if (pngAlpha)
	{
		uint8_t* alpha = bitmap->getAlphaPtr();
		if (bitmap->getMode() == splashModeMono8)
		{
			uint8_t* rowBuf = (uint8_t*)gmallocn(bitmap->getWidth(), 2);
			for (int y = 0; y < bitmap->getHeight(); ++y)
			{
				uint8_t* rowBufPtr = rowBuf;
				for (int x = 0; x < bitmap->getWidth(); ++x)
				{
					*rowBufPtr++ = *p++;
					*rowBufPtr++ = *alpha++;
				}
				png_write_row(png, (png_bytep)rowBuf);
			}
			gfree(rowBuf);
		}
		else
		{
			// splashModeRGB8
			uint8_t* rowBuf = (uint8_t*)gmallocn(bitmap->getWidth(), 4);
			for (int y = 0; y < bitmap->getHeight(); ++y)
			{
				uint8_t* rowBufPtr = rowBuf;
				for (int x = 0; x < bitmap->getWidth(); ++x)
				{
					*rowBufPtr++ = *p++;
					*rowBufPtr++ = *p++;
					*rowBufPtr++ = *p++;
					*rowBufPtr++ = *alpha++;
				}
				png_write_row(png, (png_bytep)rowBuf);
			}
			gfree(rowBuf);
		}
	}
	else
	{
		for (int y = 0; y < bitmap->getHeight(); ++y)
		{
			png_write_row(png, (png_bytep)p);
			p += bitmap->getRowSize();
		}
	}
}

static void finishPNG(png_structp* png, png_infop* pngInfo)
{
	if (setjmp(png_jmpbuf(*png))) exit(2);
	png_write_end(*png, *pngInfo);
	png_destroy_write_struct(png, pngInfo);
}
