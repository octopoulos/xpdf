//========================================================================
//
// pdfimages.cc
//
// Copyright 1998-2003 Glyph & Cog, LLC
// updated by octopoulos @ 2024-12-29
//
//========================================================================

// --cfgFileName "c:/web/gori/data/xpdf.cfg" --dumpJPEG -- "c:/web/gori/source/pdf/bug.pdf" "c:/web/gori/source/pdf/bug"

#include <aconf.h>
#include "Error.h"
#include "GlobalParams.h"
#include "ImageOutputDev.h"
#include "PDFDoc.h"

#include <CLI/CLI.hpp>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
	// OPTIONS
	//////////

	CLI_BEGIN("pdfimages");

	// name, type, default, implicit, needApp, help
	// clang-format off
	CLI_OPTION(cfgFileName  , std::string, "", "", 0, "configuration file to use in place of .xpdfrc");
	CLI_OPTION(dumpJPEG     , int        , 0 , 1 , 0, "write JPEG images as JPEG files");
	CLI_OPTION(dumpRaw      , int        , 0 , 1 , 0, "write raw data in PDF-native formats");
	CLI_OPTION(firstPage    , int        , 1 , 1 , 0, "first page to convert");
	CLI_OPTION(lastPage     , int        , 0 , 0 , 0, "last page to convert");
	CLI_OPTION(list         , int        , 0 , 1 , 0, "write information to stdout for each image");
	CLI_OPTION(ownerPassword, std::string, "", "", 0, "owner password (for encrypted files)");
	CLI_OPTION(printVersion , int        , 0 , 1 , 0, "print copyright and version info");
	CLI_OPTION(quiet        , int        , 0 , 1 , 0, "don't print any messages or errors");
	CLI_OPTION(userPassword , std::string, "", "", 0, "user password (for encrypted files)");
	CLI_OPTION(verbose      , int        , 0 , 1 , 0, "print per-page status information");
	//
	CLI_REQUIRED(fileName, std::string, 1, "<PDF-file>");
	CLI_REQUIRED(imgRoot , std::string, 1, "<image-root>");
	// clang-format on

	CLI11_PARSE(cli, argc, argv);
	CLI_NEEDAPP();

	// config file
	{
		if (CLI_STRING(cfgFileName) && !std::filesystem::exists(cfgFileName))
			error(errConfig, -1, "Config file '{}' doesn't exist or isn't a file", cfgFileName);

		globalParams = std::make_shared<GlobalParams>(cfgFileName.c_str());
		if (verbose) globalParams->setPrintStatusInfo(verbose);
		if (quiet) globalParams->setErrQuiet(quiet);
	}

	// open PDF file
	auto doc = std::make_unique<PDFDoc>(fileName, ownerPassword, userPassword);
	if (!doc->isOk()) return 1;

	// check for copy permission
	if (!doc->okToCopy())
	{
		error(errNotAllowed, -1, "Copying of images from this document is not allowed.");
		return 3;
	}

	// get page range
	if (firstPage < 1) firstPage = 1;
	if (lastPage < 1 || lastPage > doc->getNumPages())
		lastPage = doc->getNumPages();

	// write image files
	{
		ImageOutputDev imgOut(imgRoot, dumpJPEG, dumpRaw, list);
		if (imgOut.isOk())
			doc->displayPages(&imgOut, firstPage, lastPage, 72, 72, 0, false, true, false);
	}

	// check for memory leaks
	Object::memCheck(stderr);
	gMemReport(stderr);
	return 0;
}
