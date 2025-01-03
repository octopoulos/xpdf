//========================================================================
//
// pdftoppm.cc
//
// Copyright 2003 Glyph & Cog, LLC
// updated by octopoulos @ 2024-12-29
//
//========================================================================

// --cfgFileName "c:/web/gori/data/xpdf.cfg" --freetypeYN -- "c:/web/gori/source/pdf/bug.pdf" "c:/web/gori/source/pdf/bug"

#include <aconf.h>
#ifdef _WIN32
#	include <fcntl.h>
#endif
#ifdef DEBUG_FP_LINUX
#	include <fenv.h>
#	include <fpu_control.h>
#endif
#include "Error.h"
#include "GlobalParams.h"
#include "Object.h"
#include "PDFDoc.h"
#include "SplashBitmap.h"
#include "SplashOutputDev.h"

#include <CLI/CLI.hpp>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
	// OPTIONS
	//////////

	CLI_BEGIN("pdftoppm");

	// name, type, default, implicit, needApp, help
	// clang-format off
	CLI_OPTION(antialiasYN  , std::string, "" , "", 0, "enable font anti-aliasing: yes, no");
	CLI_OPTION(cfgFileName  , std::string, "" , "", 0, "configuration file to use in place of .xpdfrc");
	CLI_OPTION(cmyk         , int        , 0  , 1 , 0, "generate a CMYK PAM file");
	CLI_OPTION(firstPage    , int        , 1  , 1 , 0, "first page to convert");
	CLI_OPTION(freetypeYN   , std::string, "" , "", 0, "enable FreeType font rasterizer: yes, no");
	CLI_OPTION(gray         , int        , 0  , 1 , 0, "generate a grayscale PGM file");
	CLI_OPTION(lastPage     , int        , 0  , 0 , 0, "last page to convert");
	CLI_OPTION(mono         , int        , 0  , 1 , 0, "generate a monochrome PBM file");
	CLI_OPTION(ownerPassword, std::string, "" , "", 0, "owner password (for encrypted files)");
	CLI_OPTION(printVersion , int        , 0  , 1 , 0, "print copyright and version info");
	CLI_OPTION(quiet        , int        , 0  , 1 , 0, "don't print any messages or errors");
	CLI_OPTION(resolution   , int        , 150, 0 , 0, "resolution, in DPI (default is 150)");
	CLI_OPTION(rotate       , int        , 0  , 0 , 0, "set page rotation: 0, 90, 180, or 270");
	CLI_OPTION(userPassword , std::string, "" , "", 0, "user password (for encrypted files)");
	CLI_OPTION(vectoraaYN   , std::string, "" , "", 0, "enable vector anti-aliasing: yes, no");
	CLI_OPTION(verbose      , int        , 0  , 1 , 0, "print per-page status information");
	//
	CLI_REQUIRED(fileName, std::string, 1, "<PDF-file>");
	CLI_REQUIRED(ppmRoot , std::string, 1, "<PPM-root>");
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

	int n = 0;
	n += mono ? 1 : 0;
	n += gray ? 1 : 0;
	n += cmyk ? 1 : 0;
	if (n > 1) return 99;

#ifdef DEBUG_FP_LINUX
	// enable exceptions on floating point div-by-zero
	feenableexcept(FE_DIVBYZERO);
	// force 64-bit rounding: this avoids changes in output when minor
	// code changes result in spills of x87 registers; it also avoids
	// differences in output with valgrind's 64-bit floating point
	// emulation (yes, this is a kludge; but it's pretty much
	// unavoidable given the x87 instruction set; see gcc bug 323 for
	// more info)
	fpu_control_t cw;
	_FPU_GETCW(cw);
	cw = (fpu_control_t)((cw & ~_FPU_EXTENDED) | _FPU_DOUBLE);
	_FPU_SETCW(cw);
#endif

	// open PDF file
	auto doc = std::make_unique<PDFDoc>(fileName, ownerPassword, userPassword);
	if (!doc->isOk()) return 1;

	// get page range
	if (firstPage < 1) firstPage = 1;
	if (lastPage < 1 || lastPage > doc->getNumPages())
		lastPage = doc->getNumPages();

	// file name extension
	const char* ext;
	// clang-format off
	     if (mono) ext = "pbm";
	else if (gray) ext = "pgm";
	else if (cmyk) ext = "pam";
	else           ext = "ppm";
	// clang-format on

	// check for stdout; set up to print per-page status info
	const bool toStdout        = (ppmRoot == "-");
	const bool printStatusInfo = !toStdout && globalParams->getPrintStatusInfo();

	// write PPM files
	SplashColor                      paperColor;
	std::shared_ptr<SplashOutputDev> splashOut;
	if (mono)
	{
		paperColor[0] = 0xff;
		splashOut     = std::make_shared<SplashOutputDev>(splashModeMono1, 1, false, paperColor);
	}
	else if (gray)
	{
		paperColor[0] = 0xff;
		splashOut     = std::make_shared<SplashOutputDev>(splashModeMono8, 1, false, paperColor);
#if SPLASH_CMYK
	}
	else if (cmyk)
	{
		paperColor[0] = 0;
		paperColor[1] = 0;
		paperColor[2] = 0;
		paperColor[3] = 0;
		splashOut     = std::make_shared<SplashOutputDev>(splashModeCMYK8, 1, false, paperColor);
#endif // SPLASH_CMYK
	}
	else
	{
		paperColor[0] = 0xff;
		paperColor[1] = 0xff;
		paperColor[2] = 0xff;
		splashOut     = std::make_shared<SplashOutputDev>(splashModeRGB8, 1, false, paperColor);
	}
	splashOut->startDoc(doc->getXRef());
	for (int pg = firstPage; pg <= lastPage; ++pg)
	{
		if (printStatusInfo)
		{
			fflush(stderr);
			printf("[processing page %d]\n", pg);
			fflush(stdout);
		}
		doc->displayPage(splashOut.get(), pg, resolution, resolution, rotate, false, true, false);
		if (toStdout)
		{
#ifdef _WIN32
			_setmode(_fileno(stdout), _O_BINARY);
#endif
			splashOut->getBitmap()->writePNMFile(stdout);
		}
		else
		{
			const auto ppmFile = fmt::format("{}-{:06}.{}", ppmRoot, pg, ext);
			splashOut->getBitmap()->writePNMFile(ppmFile.c_str());
		}
	}

	// check for memory leaks
	Object::memCheck(stderr);
	gMemReport(stderr);
	return 0;
}
