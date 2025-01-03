//========================================================================
//
// pdfdetach.cc
//
// Copyright 2010 Glyph & Cog, LLC
// updated by octopoulos @ 2024-12-29
//
//========================================================================

// --cfgFileName "c:/web/gori/data/xpdf.cfg" --doList --saveNum 1 -- "c:/web/gori/source/pdf/embedded.pdf"

#include <aconf.h>
#include "Error.h"
#include "GlobalParams.h"
#include "PDFDoc.h"
#include "UnicodeMap.h"
#include "UTF8.h"

#include <CLI/CLI.hpp>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
	// OPTIONS
	//////////

	CLI_BEGIN("pdfdetach");

	// name, type, default, implicit, needApp, help
	// clang-format off
	CLI_OPTION(cfgFileName  , std::string, "" , "", 0, "configuration file to use in place of .xpdfrc");
	CLI_OPTION(doList       , int        , 0  , 1 , 0, "list all embedded files");
	CLI_OPTION(ownerPassword, std::string, "" , "", 0, "owner password (for encrypted files)");
	CLI_OPTION(printVersion , int        , 0  , 1 , 0, "print copyright and version info");
	CLI_OPTION(saveAll      , int        , 0  , 1 , 0, "save all embedded files");
	CLI_OPTION(saveNum      , int        , 0  , 0 , 0, "save the specified embedded files");
	CLI_OPTION(savePath     , std::string, "" , "", 0, "file name for the saved embedded file");
	CLI_OPTION(textEncName  , std::string, "" , "", 0, "output text encoding name");
	CLI_OPTION(userPassword , std::string, "" , "", 0, "user password (for encrypted files)");
	//
	CLI_REQUIRED(fileName, std::string, 1, "<PDF-file>");
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
	}

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

	const int nFiles = doc->getNumEmbeddedFiles();
	char      uBuf[8];

	// list embedded files
	if (doList)
	{
		printf("%d embedded file%s\n", nFiles, (nFiles > 1) ? "s" : "");
		for (int i = 0; i < nFiles; ++i)
		{
			printf("%d: ", i + 1);
			const Unicode* name    = doc->getEmbeddedFileName(i);
			const int      nameLen = doc->getEmbeddedFileNameLength(i);
			for (int j = 0; j < nameLen; ++j)
			{
				const int n = uMap->mapUnicode(name[j], uBuf, sizeof(uBuf));
				fwrite(uBuf, 1, n, stdout);
			}
			fputc('\n', stdout);
		}		
	}

	// save all embedded files
	if (saveAll)
	{
		for (int i = 0; i < nFiles; ++i)
		{
			std::string path;
			if (CLI_STRING(savePath))
			{
				path = savePath;
				path += '/';
			}
			const Unicode* name    = doc->getEmbeddedFileName(i);
			const int      nameLen = doc->getEmbeddedFileNameLength(i);
			for (int j = 0; j < nameLen; ++j)
			{
				const int n = mapUTF8(name[j], uBuf, sizeof(uBuf));
				path.append(uBuf, n);
			}
			if (!doc->saveEmbeddedFileU(i, path.c_str()))
			{
				error(errIO, -1, "Error saving embedded file as '{}'", path);
				return 2;
			}
		}
	}
	// save an embedded file
	else
	{
		if (saveNum < 1 || saveNum > nFiles)
		{
			error(errCommandLine, -1, "Invalid file number");
			return 99;
		}
		std::string path;
		if (CLI_STRING(savePath))
			path = savePath;
		else
		{
			const Unicode* name    = doc->getEmbeddedFileName(saveNum - 1);
			const int      nameLen = doc->getEmbeddedFileNameLength(saveNum - 1);
			for (int j = 0; j < nameLen; ++j)
			{
				const int n = mapUTF8(name[j], uBuf, sizeof(uBuf));
				path.append(uBuf, n);
			}
		}
		if (!doc->saveEmbeddedFileU(saveNum - 1, path.c_str()))
		{
			error(errIO, -1, "Error saving embedded file as '{}'", path);
			return 2;
		}
	}

	// clean up
	uMap->decRefCnt();

	// check for memory leaks
	Object::memCheck(stderr);
	gMemReport(stderr);
	return 0;
}
