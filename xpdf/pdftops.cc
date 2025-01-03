//========================================================================
//
// pdftops.cc
//
// Copyright 1996-2003 Glyph & Cog, LLC
// updated by octopoulos @ 2024-12-29
//
//========================================================================

// --cfgFileName "c:/web/gori/data/xpdf.cfg" -- "c:/web/gori/source/pdf/bug.pdf"

#include <aconf.h>
#ifdef DEBUG_FP_LINUX
#	include <fenv.h>
#	include <fpu_control.h>
#endif
#include "Error.h"
#include "GlobalParams.h"
#include "Object.h"
#include "Page.h"
#include "PDFDoc.h"
#include "PSOutputDev.h"

#include <CLI/CLI.hpp>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
	// OPTIONS
	//////////

	CLI_BEGIN("pdftops");

	// name, type, default, implicit, needApp, help
	// clang-format off
	CLI_OPTION(cfgFileName  , std::string, "" , "", 0, "configuration file to use in place of .xpdfrc");
	CLI_OPTION(doEPS        , int        , 0  , 1 , 0, "generate Encapsulated PostScript (EPS)");
	CLI_OPTION(doForm       , int        , 0  , 1 , 0, "generate a PostScript form");
	CLI_OPTION(doOPI        , int        , 0  , 1 , 0, "generate OPI comments");
	CLI_OPTION(duplex       , int        , 0  , 1 , 0, "enable duplex printing");
	CLI_OPTION(expand       , int        , 0  , 1 , 0, "expand pages smaller than the paper size");
	CLI_OPTION(firstPage    , int        , 1  , 1 , 0, "first page to convert");
	CLI_OPTION(lastPage     , int        , 0  , 0 , 0, "last page to convert");
	CLI_OPTION(level1       , int        , 0  , 1 , 0, "generate Level 1 PostScript");
	CLI_OPTION(level1Sep    , int        , 0  , 1 , 0, "generate Level 1 separable PostScript");
	CLI_OPTION(level2       , int        , 0  , 1 , 0, "generate Level 2 PostScript");
	CLI_OPTION(level2Gray   , int        , 0  , 1 , 0, "generate Level 2 grayscale PostScript");
	CLI_OPTION(level2Sep    , int        , 0  , 1 , 0, "generate Level 2 separable PostScript");
	CLI_OPTION(level3       , int        , 0  , 1 , 0, "generate Level 3 PostScript");
	CLI_OPTION(level3Gray   , int        , 0  , 1 , 0, "generate Level 3 grayscale PostScript");
	CLI_OPTION(level3Sep    , int        , 0  , 1 , 0, "generate Level 3 separable PostScript");
	CLI_OPTION(noCenter     , int        , 0  , 1 , 0, "don't center pages smaller than the paper size");
	CLI_OPTION(noCrop       , int        , 0  , 1 , 0, "don't crop pages to CropBox");
	CLI_OPTION(noEmbedCIDPS , int        , 0  , 1 , 0, "don't embed CID PostScript fonts");
	CLI_OPTION(noEmbedCIDTT , int        , 0  , 1 , 0, "don't embed CID TrueType fonts");
	CLI_OPTION(noEmbedT1    , int        , 0  , 1 , 0, "don't embed Type 1 fonts");
	CLI_OPTION(noEmbedTT    , int        , 0  , 1 , 0, "don't embed TrueType fonts");
	CLI_OPTION(noShrink     , int        , 0  , 1 , 0, "don't shrink pages larger than the paper size");
	CLI_OPTION(ownerPassword, std::string, "" , "", 0, "owner password (for encrypted files)");
	CLI_OPTION(pageCrop     , int        , 0  , 1 , 0, "treat the CropBox as the page size");
	CLI_OPTION(paperHeight  , int        , 0  , 0 , 0, "paper height, in points");
	CLI_OPTION(paperSize    , std::string, "" , "", 0, "paper size (letter, legal, A4, A3, match)");
	CLI_OPTION(paperWidth   , int        , 0  , 0 , 0, "paper width, in points");
	CLI_OPTION(preload      , int        , 0  , 1 , 0, "preload images and forms");
	CLI_OPTION(printVersion , int        , 0  , 1 , 0, "print copyright and version info");
	CLI_OPTION(quiet        , int        , 0  , 1 , 0, "don't print any messages or errors");
	CLI_OPTION(userPassword , std::string, "" , "", 0, "user password (for encrypted files)");
	CLI_OPTION(userUnit     , int        , 0  , 1 , 0, "honor the UserUnit");
	CLI_OPTION(verbose      , int        , 0  , 1 , 0, "print per-page status information");
	//
	CLI_REQUIRED(fileName  , std::string, 1, "<PDF-file>");
	CLI_REQUIRED(psFileName, std::string, 0, "<PS-file>");
	// clang-format on

	CLI11_PARSE(cli, argc, argv);
	CLI_NEEDAPP();

	// config file
	{
		if (CLI_STRING(cfgFileName) && !std::filesystem::exists(cfgFileName))
			error(errConfig, -1, "Config file '{}' doesn't exist or isn't a file", cfgFileName);

		globalParams = std::make_shared<GlobalParams>(cfgFileName.c_str());
		globalParams->setupBaseFonts(nullptr);
	}

	const int numLevel = level1 + level1Sep + level2 + level2Gray + level2Sep + level3 + level3Gray + level3Sep;
	if (numLevel > 1)
	{
		fprintf(stderr, "Error: use only one of the 'level' options.\n");
		return 99;
	}

	PSLevel level;
	if (level1)
		level = psLevel1;
	else if (level1Sep)
		level = psLevel1Sep;
	else if (level2Gray)
		level = psLevel2Gray;
	else if (level2Sep)
		level = psLevel2Sep;
	else if (level3)
		level = psLevel3;
	else if (level3Gray)
		level = psLevel3Gray;
	else if (level3Sep)
		level = psLevel3Sep;
	else
		level = psLevel2;
	if (doForm && level < psLevel2)
	{
		fprintf(stderr, "Error: forms are only available with Level 2 output.\n");
		return 99;
	}

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

	if (doEPS && doForm)
	{
		fprintf(stderr, "Error: use only one of -eps and -form\n");
		exit(1);
	}
	if (level1)
		level = psLevel1;
	else if (level1Sep)
		level = psLevel1Sep;
	else if (level2Gray)
		level = psLevel2Gray;
	else if (level2Sep)
		level = psLevel2Sep;
	else if (level3)
		level = psLevel3;
	else if (level3Gray)
		level = psLevel3Gray;
	else if (level3Sep)
		level = psLevel3Sep;
	else
		level = psLevel2;
	if (doForm && level < psLevel2)
	{
		fprintf(stderr, "Error: forms are only available with Level 2 output.\n");
		return 99;
	}

	// globals
	{
		if (noCrop) globalParams->setPSCrop(false);
		if (pageCrop) globalParams->setPSUseCropBoxAsPage(true);
		if (expand) globalParams->setPSExpandSmaller(true);
		if (noShrink) globalParams->setPSShrinkLarger(false);
		if (noCenter) globalParams->setPSCenter(false);
		if (duplex) globalParams->setPSDuplex(duplex);
		if (numLevel) globalParams->setPSLevel(level);
		if (noEmbedT1) globalParams->setPSEmbedType1(!noEmbedT1);
		if (noEmbedTT) globalParams->setPSEmbedTrueType(!noEmbedTT);
		if (noEmbedCIDPS) globalParams->setPSEmbedCIDPostScript(!noEmbedCIDPS);
		if (noEmbedCIDTT) globalParams->setPSEmbedCIDTrueType(!noEmbedCIDTT);
		if (preload) globalParams->setPSPreload(preload);
		if (doOPI) globalParams->setPSOPI(doOPI);
		if (verbose) globalParams->setPrintStatusInfo(verbose);
		if (quiet) globalParams->setErrQuiet(quiet);
	}

	if (CLI_STRING(paperSize))
	{
		if (!globalParams->setPSPaperSize(paperSize.c_str()))
		{
			fprintf(stderr, "Invalid paper size\n");
			return 99;
		}
	}
	else
	{
		if (paperWidth) globalParams->setPSPaperWidth(paperWidth);
		if (paperHeight) globalParams->setPSPaperHeight(paperHeight);
	}

	PSOutMode mode = doEPS ? psModeEPS : (doForm ? psModeForm : psModePS);

	// open PDF file
	auto doc = std::make_unique<PDFDoc>(fileName, ownerPassword, userPassword);
	if (!doc->isOk()) return 1;

	// check for print permission
	if (!doc->okToPrint())
	{
		error(errNotAllowed, -1, "Printing this document is not allowed.");
		return 3;
	}

	// construct PostScript file name
	if (psFileName.empty())
	{
		if (const auto pos = fileName.rfind('.'); pos != std::string::npos)
			psFileName = fileName.substr(0, pos) + (doEPS ? ".eps" : ".ps");
	}
	if (psFileName == "-")
		globalParams->setPrintStatusInfo(false);

	// get page range
	if (firstPage < 1) firstPage = 1;
	if (lastPage < 1 || lastPage > doc->getNumPages())
		lastPage = doc->getNumPages();

	// check for multi-page EPS or form
	if ((doEPS || doForm) && firstPage != lastPage)
	{
		error(errCommandLine, -1, "EPS and form files can only contain one page.");
		return 99;
	}

	// write PostScript file
	PSOutputDev psOut(psFileName.c_str(), doc.get(), firstPage, lastPage, mode, 0, 0, 0, 0, false, nullptr, nullptr, userUnit, true);
	if (!psOut.isOk()) return 2;

	doc->displayPages(&psOut, firstPage, lastPage, 72, 72, 0, !globalParams->getPSUseCropBoxAsPage(), globalParams->getPSCrop(), true);
	if (!psOut.checkIO()) return 2;

	// check for memory leaks
	Object::memCheck(stderr);
	gMemReport(stderr);
	return 0;
}
