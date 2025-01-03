//========================================================================
//
// pdftohtml.cc
//
// Copyright 2005 Glyph & Cog, LLC
// updated by octopoulos @ 2024-12-29
//
//========================================================================

// --cfgFileName "c:/web/gori/data/xpdf.cfg" -- "C:/web/gori/source/pdf/bug.pdf" "C:/web/gori/source/pdf/html"

#include <aconf.h>
#include "Error.h"
#include "ErrorCodes.h"
#include "GlobalParams.h"
#include "HTMLGen.h"
#include "PDFDoc.h"

#include <CLI/CLI.hpp>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static bool createIndex(std::string_view htmlDir, int firstPage, int lastPage);

static int writeToFile(void* file, const char* data, size_t size)
{
	return (int)fwrite(data, 1, size, (FILE*)file);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
	// OPTIONS
	//////////

	CLI_BEGIN("pdftohtml");

	// name, type, default, implicit, needApp, help
	// clang-format off
	CLI_OPTION(allInvisible , int        , 0  , 1 , 0, "treat all text as invisible");
	CLI_OPTION(cfgFileName  , std::string, "" , "", 0, "configuration file to use in place of .xpdfrc");
	CLI_OPTION(embedBackgnd , int        , 0  , 1 , 0, "embed the background image as base64-encoded data");
	CLI_OPTION(embedFonts   , int        , 0  , 1 , 0, "embed the fonts as base64-encoded data");
	CLI_OPTION(firstPage    , int        , 1  , 1 , 0, "first page to convert");
	CLI_OPTION(formFields   , int        , 0  , 1 , 0, "convert form fields to HTML");
	CLI_OPTION(lastPage     , int        , 0  , 0 , 0, "last page to convert");
	CLI_OPTION(metadata     , int        , 0  , 1 , 0, "include document metadata in the HTML output");
	CLI_OPTION(noFonts      , int        , 0  , 1 , 0, "do not extract embedded fonts");
	CLI_OPTION(overwrite    , int        , 0  , 1 , 0, "overwrite files in an existing output directory");
	CLI_OPTION(ownerPassword, std::string, "" , "", 0, "owner password (for encrypted files)");
	CLI_OPTION(printVersion , int        , 0  , 1 , 0, "print copyright and version info");
	CLI_OPTION(quiet        , int        , 0  , 1 , 0, "don't print any messages or errors");
	CLI_OPTION(resolution   , int        , 150, 0 , 0, "resolution, in DPI (default is 150)");
	CLI_OPTION(skipInvisible, int        , 0  , 1 , 0, "do not draw invisible text");
	CLI_OPTION(tableMode    , int        , 0  , 1 , 0, "use table mode for text extraction");
	CLI_OPTION(userPassword , std::string, "" , "", 0, "user password (for encrypted files)");
	CLI_OPTION(verbose      , int        , 0  , 1 , 0, "print per-page status information");
	CLI_OPTION(vStretch     , double     , 1  , 0 , 0, "vertical stretch factor (1.0 means no stretching)");
	CLI_OPTION(zoom         , double     , 1  , 0 , 0, "initial zoom level (1.0 means 72dpi)");
	//
	CLI_REQUIRED(fileName, std::string, 1, "<PDF-file>");
	CLI_REQUIRED(htmlDir , std::string, 0, "<html-dir>");
	// clang-format on

	CLI11_PARSE(cli, argc, argv);
	CLI_NEEDAPP();

	// config file
	{
		if (CLI_STRING(cfgFileName) && !std::filesystem::exists(cfgFileName))
			error(errConfig, -1, "Config file '{}' doesn't exist or isn't a file", cfgFileName);

		globalParams = std::make_shared<GlobalParams>(cfgFileName.c_str());
		globalParams->setupBaseFonts(nullptr);
		if (verbose) globalParams->setPrintStatusInfo(verbose);
		if (quiet) globalParams->setErrQuiet(quiet);
		globalParams->setupBaseFonts(nullptr);
		globalParams->setTextEncoding("UTF-8");
	}

	FILE *htmlFile, *pngFile;

	// open PDF file
	auto doc = std::make_unique<PDFDoc>(fileName, ownerPassword, userPassword);
	if (!doc->isOk()) return 1;

	// check for copy permission
	if (!doc->okToCopy())
	{
		error(errNotAllowed, -1, "Copying of text from this document is not allowed.");
		return 3;
	}

	// get page range
	if (firstPage < 1) firstPage = 1;
	if (lastPage < 1 || lastPage > doc->getNumPages())
		lastPage = doc->getNumPages();

	// create HTML directory
	std::filesystem::create_directories(htmlDir);
	if (std::filesystem::is_directory(htmlDir))
	{
		if (!overwrite)
		{
			error(errIO, -1, "HTML output directory '{}' already exists (use '-overwrite' to overwrite it)", htmlDir);
			return 2;
		}
	}
	else
	{
		error(errIO, -1, "Couldn't create HTML output directory '{}'", htmlDir);
		return 2;
	}

	// set up the HTMLGen object
	HTMLGen htmlGen(resolution, tableMode);
	if (!htmlGen.isOk()) return 99;
	htmlGen.setZoom(zoom);
	htmlGen.setVStretch(vStretch);
	htmlGen.setDrawInvisibleText(!skipInvisible);
	htmlGen.setAllTextInvisible(allInvisible);
	htmlGen.setEmbedBackgroundImage(embedBackgnd);
	htmlGen.setExtractFontFiles(!noFonts);
	htmlGen.setEmbedFonts(embedFonts);
	htmlGen.setConvertFormFields(formFields);
	htmlGen.setIncludeMetadata(metadata);
	htmlGen.startDoc(doc.get());

	// convert the pages
	for (int pg = firstPage; pg <= lastPage; ++pg)
	{
		if (globalParams->getPrintStatusInfo())
		{
			fflush(stderr);
			printf("[processing page %d]\n", pg);
			fflush(stdout);
		}
		const auto htmlFileName = fmt::format("{}/page{}.html", htmlDir, pg);
		const auto pngFileName  = fmt::format("{}/page{}.png", htmlDir, pg);
		if (!(htmlFile = openFile(htmlFileName.c_str(), "wb")))
		{
			error(errIO, -1, "Couldn't open HTML file '{}'", htmlFileName);
			return 99;
		}
		if (embedBackgnd)
		{
			pngFile = nullptr;
		}
		else if (!(pngFile = openFile(pngFileName.c_str(), "wb")))
		{
			error(errIO, -1, "Couldn't open PNG file '{}'", pngFileName);
			fclose(htmlFile);
			return 99;
		}
		const auto pngURL = fmt::format("page{}.png", pg);
		const int  err    = htmlGen.convertPage(pg, pngURL.c_str(), htmlDir.c_str(), &writeToFile, htmlFile, &writeToFile, pngFile);
		fclose(htmlFile);
		if (!embedBackgnd) fclose(pngFile);
		if (err != errNone)
		{
			error(errIO, -1, "Error converting page {}", pg);
			return 2;
		}
	}

	// create the master index
	if (!createIndex(htmlDir, firstPage, lastPage)) return 2;

	// check for memory leaks
	Object::memCheck(stderr);
	gMemReport(stderr);
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static bool createIndex(std::string_view htmlDir, int firstPage, int lastPage)
{
	const auto htmlFileName = fmt::format("{}/index.html", htmlDir);
	FILE*      html         = openFile(htmlFileName.c_str(), "w");
	if (!html)
	{
		error(errIO, -1, "Couldn't open HTML file '{}'", htmlFileName);
		return false;
	}

	fprintf(html, "<html>\n");
	fprintf(html, "<body>\n");
	for (int pg = firstPage; pg <= lastPage; ++pg)
		fprintf(html, "<a href=\"page%d.html\">page %d</a><br>\n", pg, pg);
	fprintf(html, "</body>\n");
	fprintf(html, "</html>\n");

	fclose(html);
	return true;
}
