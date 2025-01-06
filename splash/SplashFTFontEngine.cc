//========================================================================
//
// SplashFTFontEngine.cc
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>

#if HAVE_FREETYPE_H
#	include <stdio.h>
#	ifndef _WIN32
#		include <unistd.h>
#	endif
#	include "gmem.h"
#	include "gmempp.h"
#	include "GString.h"
#	include "gfile.h"
#	include "FoFiTrueType.h"
#	include "FoFiType1C.h"
#	include "SplashFTFontFile.h"
#	include "SplashFTFontEngine.h"
#	include FT_MODULE_H
#	ifdef FT_CFF_DRIVER_H
#		include FT_CFF_DRIVER_H
#	endif

#	ifdef VMS
#		if (__VMS_VER < 70000000)
extern "C" int unlink(char* filename);
#		endif
#	endif

//------------------------------------------------------------------------

static void fileWrite(void* stream, const char* data, size_t len)
{
	fwrite(data, 1, len, (FILE*)stream);
}

#	if LOAD_FONTS_FROM_MEM
static void gstringWrite(void* stream, const char* data, size_t len)
{
	((std::string*)stream)->append(data, len);
}
#	endif

//------------------------------------------------------------------------
// SplashFTFontEngine
//------------------------------------------------------------------------

SplashFTFontEngine::SplashFTFontEngine(bool aaA, uint32_t flagsA, FT_Library libA)
{
	FT_Int major, minor, patch;

	aa    = aaA;
	flags = flagsA;
	lib   = libA;

	// as of FT 2.1.8, CID fonts are indexed by CID instead of GID
	FT_Library_Version(lib, &major, &minor, &patch);
	useCIDs = major > 2 || (major == 2 && (minor > 1 || (minor == 1 && patch > 7)));
}

SplashFTFontEngine* SplashFTFontEngine::init(bool aaA, uint32_t flagsA)
{
	FT_Library libA;
	if (FT_Init_FreeType(&libA)) return nullptr;
	return new SplashFTFontEngine(aaA, flagsA, libA);
}

SplashFTFontEngine::~SplashFTFontEngine()
{
	FT_Done_FreeType(lib);
}

SplashFontFile* SplashFTFontEngine::loadType1Font(SplashFontFileID* idA, LOAD_FONT_ARGS_DEFS(), const VEC_STR& enc)
{
	return SplashFTFontFile::loadType1Font(this, idA, splashFontType1, LOAD_FONT_ARGS_CALLS(), enc);
}

SplashFontFile* SplashFTFontEngine::loadType1CFont(SplashFontFileID* idA, LOAD_FONT_ARGS_DEFS(), const VEC_STR& enc)
{
	return SplashFTFontFile::loadType1Font(this, idA, splashFontType1C, LOAD_FONT_ARGS_CALLS(), enc);
}

SplashFontFile* SplashFTFontEngine::loadOpenTypeT1CFont(SplashFontFileID* idA, LOAD_FONT_ARGS_DEFS(), const VEC_STR& enc)
{
	FoFiTrueType*   ff;
	SplashFontFile* ret;

#	if LOAD_FONTS_FROM_MEM
	if (!(ff = FoFiTrueType::make(fontBuf.c_str(), fontBuf.size(), 0, true)))
	{
#	else
	if (!(ff = FoFiTrueType::load(fileName, 0, true)))
	{
#	endif
		return nullptr;
	}
	if (ff->isHeadlessCFF())
	{
#	if LOAD_FONTS_FROM_MEM
		std::string fontBuf2;
		ff->convertToType1(nullptr, enc, false, &gstringWrite, (void*)&fontBuf2);
		delete ff;
		ret = SplashFTFontFile::loadType1Font(this, idA, splashFontType1, fontBuf2, enc);
#	else
		std::string tmpFileName;
		FILE*       tmpFile;
		if (!openTempFile(&tmpFileName, &tmpFile, "wb", nullptr))
		{
			delete ff;
			return nullptr;
		}
		ff->convertToType1(nullptr, enc, false, &fileWrite, tmpFile);
		delete ff;
		fclose(tmpFile);
		ret = SplashFTFontFile::loadType1Font(this, idA, splashFontType1, tmpFileName.c_str(), true, enc);
		if (ret)
		{
			if (deleteFile)
				unlink(fileName);
		}
		else
		{
			unlink(tmpFileName.c_str());
		}
		delete tmpFileName;
#	endif
	}
	else
	{
		delete ff;
		ret = SplashFTFontFile::loadType1Font(this, idA, splashFontOpenTypeT1C, LOAD_FONT_ARGS_CALLS(), enc);
	}
	return ret;
}

SplashFontFile* SplashFTFontEngine::loadCIDFont(SplashFontFileID* idA, LOAD_FONT_ARGS_DEFS(), int* codeToGID, int codeToGIDLen)
{
	FoFiType1C*     ff;
	int*            cidToGIDMap;
	int             nCIDs;
	SplashFontFile* ret;

	// check for a CFF font
	if (codeToGID)
	{
		cidToGIDMap = nullptr;
		nCIDs       = 0;
	}
	else if (useCIDs)
	{
		cidToGIDMap = nullptr;
		nCIDs       = 0;
#	if LOAD_FONTS_FROM_MEM
	}
	else if ((ff = FoFiType1C::make(fontBuf.c_str(), fontBuf.size())))
	{
#	else
	}
	else if ((ff = FoFiType1C::load(fileName)))
	{
#	endif
		cidToGIDMap = ff->getCIDToGIDMap(&nCIDs);
		delete ff;
	}
	else
	{
		cidToGIDMap = nullptr;
		nCIDs       = 0;
	}
	ret = SplashFTFontFile::loadCIDFont(this, idA, splashFontCID, LOAD_FONT_ARGS_CALLS(), codeToGID ? codeToGID : cidToGIDMap, codeToGID ? codeToGIDLen : nCIDs);
	if (!ret)
		gfree(cidToGIDMap);
	return ret;
}

SplashFontFile* SplashFTFontEngine::loadOpenTypeCFFFont(SplashFontFileID* idA, LOAD_FONT_ARGS_DEFS(), int* codeToGID, int codeToGIDLen)
{
	FoFiTrueType*   ff;
	char*           cffStart;
	int             cffLength;
	int*            cidToGIDMap;
	int             nCIDs;
	SplashFontFile* ret;

#	if LOAD_FONTS_FROM_MEM
	if (!(ff = FoFiTrueType::make(fontBuf.c_str(), fontBuf.size(), 0, true)))
	{
#	else
	if (!(ff = FoFiTrueType::load(fileName, 0, true)))
	{
#	endif
		return nullptr;
	}
	cidToGIDMap = nullptr;
	nCIDs       = 0;
	if (ff->isHeadlessCFF())
	{
		if (!ff->getCFFBlock(&cffStart, &cffLength))
			return nullptr;
#	if LOAD_FONTS_FROM_MEM
		std::string fontBuf2 = std::string(cffStart, cffLength);
		if (!useCIDs)
			cidToGIDMap = ff->getCIDToGIDMap(&nCIDs);
		ret = SplashFTFontFile::loadCIDFont(this, idA, splashFontOpenTypeCFF, fontBuf2, cidToGIDMap, nCIDs);
#	else
		std::string tmpFileName;
		FILE*       tmpFile;
		if (!openTempFile(&tmpFileName, &tmpFile, "wb", nullptr))
		{
			delete ff;
			return nullptr;
		}
		fwrite(cffStart, 1, cffLength, tmpFile);
		fclose(tmpFile);
		if (!useCIDs)
			cidToGIDMap = ff->getCIDToGIDMap(&nCIDs);
		ret = SplashFTFontFile::loadCIDFont(this, idA, splashFontOpenTypeCFF, tmpFileName.c_str(), true, cidToGIDMap, nCIDs);
		if (ret)
		{
			if (deleteFile)
				unlink(fileName);
		}
		else
		{
			unlink(tmpFileName.c_str());
		}
		delete tmpFileName;
#	endif
	}
	else
	{
		if (!codeToGID && !useCIDs && ff->isOpenTypeCFF())
			cidToGIDMap = ff->getCIDToGIDMap(&nCIDs);
		ret = SplashFTFontFile::loadCIDFont(this, idA, splashFontOpenTypeCFF, LOAD_FONT_ARGS_CALLS(), codeToGID ? codeToGID : cidToGIDMap, codeToGID ? codeToGIDLen : nCIDs);
	}
	delete ff;
	if (!ret) gfree(cidToGIDMap);
	return ret;
}

SplashFontFile* SplashFTFontEngine::loadTrueTypeFont(SplashFontFileID* idA, LOAD_FONT_ARGS_DEFS(), int fontNum, int* codeToGID, int codeToGIDLen)
{
	FoFiTrueType*   ff;
	SplashFontFile* ret;

#	if LOAD_FONTS_FROM_MEM
	if (!(ff = FoFiTrueType::make(fontBuf.c_str(), fontBuf.size(), fontNum)))
	{
#	else
	if (!(ff = FoFiTrueType::load(fileName, fontNum)))
	{
#	endif
		return nullptr;
	}
#	if LOAD_FONTS_FROM_MEM
	std::string fontBuf2;
	ff->writeTTF(&gstringWrite, (void*)&fontBuf2);
#	else
	std::string tmpFileName;
	FILE*       tmpFile;
	if (!openTempFile(&tmpFileName, &tmpFile, "wb", nullptr))
	{
		delete ff;
		return nullptr;
	}
	ff->writeTTF(&fileWrite, tmpFile);
	fclose(tmpFile);

	const auto fileName2   = tempFileName.c_str();
	const auto deleteFile2 = true;
#	endif
	delete ff;

	ret = SplashFTFontFile::loadTrueTypeFont(this, idA, splashFontTrueType, LOAD_FONT_ARGS_CALLS(2), 0, codeToGID, codeToGIDLen);
#	if LOAD_FONTS_FROM_MEM
#	else
	if (ret)
	{
		if (deleteFile)
			unlink(fileName);
	}
	else
	{
		unlink(tmpFileName.c_str());
	}
#	endif
	return ret;
}

#endif // HAVE_FREETYPE_H
