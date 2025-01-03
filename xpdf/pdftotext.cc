//========================================================================
//
// pdftotext.cc
//
// Copyright 1997-2013 Glyph & Cog, LLC
// updated by octopoulos @ 2024-12-29
//
//========================================================================

// --cfgFileName "c:/web/gori/data/xpdf.cfg" --tableLayout -- "c:/web/gori/source/pdf/bug.pdf" "c:/web/gori/source/pdf/bug-table.txt"

#include <aconf.h>
#ifdef DEBUG_FP_LINUX
#	include <fenv.h>
#	include <fpu_control.h>
#endif
#include "Error.h"
#include "GlobalParams.h"
#include "Page.h"
#include "PDFDoc.h"
#include "TextOutputDev.h"

#include <CLI/CLI.hpp>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
	// OPTIONS
	//////////

	CLI_BEGIN("pdftotext");

	// name, type, default, implicit, needApp, help
	// clang-format off
	CLI_OPTION(cfgFileName  , std::string, "" , "", 0, "configuration file to use in place of .xpdfrc");
	CLI_OPTION(clipText     , int        , 0  , 1 , 0, "separate clipped text");
	CLI_OPTION(discardDiag  , int        , 0  , 1 , 0, "discard diagonal text");
	CLI_OPTION(firstPage    , int        , 1  , 1 , 0, "first page to convert");
	CLI_OPTION(fixedPitch   , double     , 0  , 0 , 0, "assume fixed-pitch (or tabular) text");
	CLI_OPTION(insertBOM    , int        , 0  , 1 , 0, "insert a Unicode BOM at the start of the text file");
	CLI_OPTION(lastPage     , int        , 0  , 0 , 0, "last page to convert");
	CLI_OPTION(linePrinter  , int        , 0  , 1 , 0, "use strict fixed-pitch/height layout");
	CLI_OPTION(lineSpacing  , double     , 0  , 0 , 0, "fixed line spacing for LinePrinter mode");
	CLI_OPTION(listEncodings, int        , 0  , 1 , 0, "list all available output text encodings");
	CLI_OPTION(marginBottom , double     , 0  , 0 , 0, "bottom page margin");
	CLI_OPTION(marginLeft   , double     , 0  , 0 , 0, "left page margin");
	CLI_OPTION(marginRight  , double     , 0  , 0 , 0, "right page margin");
	CLI_OPTION(marginTop    , double     , 0  , 0 , 0, "top page margin");
	CLI_OPTION(noPageBreaks , int        , 0  , 1 , 0, "don't insert a page break at the end of each page");
	CLI_OPTION(ownerPassword, std::string, "" , "", 0, "owner password (for encrypted files)");
	CLI_OPTION(physLayout   , int        , 0  , 1 , 0, "maintain original physical layout");
	CLI_OPTION(printVersion , int        , 0  , 1 , 0, "print copyright and version info");
	CLI_OPTION(quiet        , int        , 0  , 1 , 0, "don't print any messages or errors");
	CLI_OPTION(rawOrder     , int        , 0  , 1 , 0, "keep strings in content stream order");
	CLI_OPTION(simpleLayout , int        , 0  , 1 , 0, "simple one-column page layout");
	CLI_OPTION(simple2Layout, int        , 0  , 1 , 0, "simple one-column page layout, version 2");
	CLI_OPTION(tableLayout  , int        , 0  , 1 , 0, "similar to --physLayout, but optimized for tables");
	CLI_OPTION(textEncName  , std::string, "" , "", 0, "output text encoding name");
	CLI_OPTION(textEOL      , std::string, "" , "", 0, "output end-of-line convention (unix, dos, or mac)");
	CLI_OPTION(userPassword , std::string, "" , "", 0, "user password (for encrypted files)");
	CLI_OPTION(verbose      , int        , 0  , 1 , 0, "print per-page status information");
	//
	CLI_REQUIRED(fileName    , std::string, 1, "<PDF-file>");
	CLI_REQUIRED(textFileName, std::string, 0, "<text-file>");
	// clang-format on

	CLI11_PARSE(cli, argc, argv);
	CLI_NEEDAPP();

	// config file
	{
		if (CLI_STRING(cfgFileName) && !std::filesystem::exists(cfgFileName))
			error(errConfig, -1, "Config file '{}' doesn't exist or isn't a file", cfgFileName);

		globalParams = std::make_shared<GlobalParams>(cfgFileName.c_str());
		globalParams->setupBaseFonts(nullptr);
		if (CLI_STRING(textEncName)) globalParams->setTextEncoding(textEncName.c_str());
		if (CLI_STRING(textEOL))
		{
			if (!globalParams->setTextEOL(textEOL.c_str()))
				fprintf(stderr, "Bad '-eol' value on command line\n");
		}
		if (noPageBreaks) globalParams->setTextPageBreaks(false);
		if (verbose) globalParams->setPrintStatusInfo(verbose);
		if (quiet) globalParams->setErrQuiet(quiet);
	}

	if (listEncodings)
	{
		// list available encodings
		const auto encs = globalParams->getAvailableTextEncodings();
		for (int i = 0; i < encs.size(); ++i)
			printf("%s\n", encs.at(i).c_str());
		return 99;
	}

#ifdef DEBUG_FP_LINUX
	// enable exceptions on floating point div-by-zero
	feenableexcept(FE_DIVBYZERO);
	// force 64-bit rounding: this avoids changes in output when minor code changes result in spills of x87 registers;
	// it also avoids differences in output with valgrind's 64-bit floating point emulation
	// (yes, this is a kludge; but it's pretty much unavoidable given the x87 instruction set; see gcc bug 323 for more info)
	fpu_control_t cw;
	_FPU_GETCW(cw);
	cw = (fpu_control_t)((cw & ~_FPU_EXTENDED) | _FPU_DOUBLE);
	_FPU_SETCW(cw);
#endif

	// get mapping to output encoding
	UnicodeMap* uMap;
	if (!(uMap = globalParams->getTextEncoding()))
	{
		error(errConfig, -1, "Couldn't get text encoding");
		return 99;
	}

	// open PDF file
	auto doc = std::make_unique<PDFDoc>(fileName, ownerPassword, userPassword);
	if (!doc->isOk()) return 1;

	// check for copy permission
	if (!doc->okToCopy())
	{
		error(errNotAllowed, -1, "Copying of text from this document is not allowed.");
		return 3;
	}

	// construct text file name
	if (textFileName.empty())
	{
		if (const auto pos = fileName.rfind('.'); pos != std::string::npos)
			textFileName = fileName.substr(0, pos) + ".txt";
	}
	if (textFileName == "-")
		globalParams->setPrintStatusInfo(false);

	// get page range
	if (firstPage < 1) firstPage = 1;
	if (lastPage < 1 || lastPage > doc->getNumPages())
		lastPage = doc->getNumPages();

	// write text file
	TextOutputControl textOutControl;
	if (tableLayout)
	{
		textOutControl.mode       = textOutTableLayout;
		textOutControl.fixedPitch = fixedPitch;
	}
	else if (physLayout)
	{
		textOutControl.mode       = textOutPhysLayout;
		textOutControl.fixedPitch = fixedPitch;
	}
	else if (simpleLayout)
		textOutControl.mode = textOutSimpleLayout;
	else if (simple2Layout)
		textOutControl.mode = textOutSimple2Layout;
	else if (linePrinter)
	{
		textOutControl.mode             = textOutLinePrinter;
		textOutControl.fixedPitch       = fixedPitch;
		textOutControl.fixedLineSpacing = lineSpacing;
	}
	else if (rawOrder)
		textOutControl.mode = textOutRawOrder;
	else
		textOutControl.mode = textOutReadingOrder;

	textOutControl.clipText            = clipText;
	textOutControl.discardDiagonalText = discardDiag;
	textOutControl.insertBOM           = insertBOM;
	textOutControl.marginLeft          = marginLeft;
	textOutControl.marginRight         = marginRight;
	textOutControl.marginTop           = marginTop;
	textOutControl.marginBottom        = marginBottom;

	TextOutputDev textOut(textFileName.c_str(), &textOutControl, false, true);
	if (textOut.isOk())
		doc->displayPages(&textOut, firstPage, lastPage, 72, 72, 0, false, true, false);
	else return 2;

	// clean up
	uMap->decRefCnt();

	// check for memory leaks
	Object::memCheck(stderr);
	gMemReport(stderr);
	return 0;
}
