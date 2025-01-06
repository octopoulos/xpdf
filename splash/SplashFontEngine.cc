//========================================================================
//
// SplashFontEngine.cc
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include <stdlib.h>
#include <stdio.h>
#ifndef _WIN32
#	include <unistd.h>
#endif
#include "gmem.h"
#include "gmempp.h"
#include "GList.h"
#include "SplashMath.h"
#include "SplashFTFontEngine.h"
#include "SplashFontFile.h"
#include "SplashFontFileID.h"
#include "SplashFont.h"
#include "SplashFontEngine.h"

#ifdef VMS
#	if (__VMS_VER < 70000000)
extern "C" int unlink(char* filename);
#	endif
#endif

//------------------------------------------------------------------------
// SplashFontEngine
//------------------------------------------------------------------------

SplashFontEngine::SplashFontEngine(
#if HAVE_FREETYPE_H
    bool     enableFreeType,
    uint32_t freeTypeFlags,
#endif
    bool aa)
{
	for (int i = 0; i < splashFontCacheSize; ++i)
		fontCache[i] = nullptr;
	badFontFiles = new GList();

#if HAVE_FREETYPE_H
	if (enableFreeType)
		ftEngine = SplashFTFontEngine::init(aa, freeTypeFlags);
	else
		ftEngine = nullptr;
#endif
}

SplashFontEngine::~SplashFontEngine()
{
	for (int i = 0; i < splashFontCacheSize; ++i)
		if (fontCache[i])
			delete fontCache[i];
	deleteGList(badFontFiles, SplashFontFileID);

#if HAVE_FREETYPE_H
	if (ftEngine)
		delete ftEngine;
#endif
}

SplashFontFile* SplashFontEngine::getFontFile(SplashFontFileID* id)
{
	for (int i = 0; i < splashFontCacheSize; ++i)
	{
		if (fontCache[i])
		{
			const auto fontFile = fontCache[i]->getFontFile();
			if (fontFile && fontFile->getID()->matches(id))
				return fontFile;
		}
	}
	return nullptr;
}

bool SplashFontEngine::checkForBadFontFile(SplashFontFileID* id)
{
	for (int i = 0; i < badFontFiles->getLength(); ++i)
		if (((SplashFontFileID*)badFontFiles->get(i))->matches(id))
			return true;
	return false;
}

SplashFontFile* SplashFontEngine::loadType1Font(SplashFontFileID* idA, LOAD_FONT_ARGS_DEFS(), const VEC_STR& enc)
{
	SplashFontFile* fontFile = nullptr;
#if HAVE_FREETYPE_H
	if (!fontFile && ftEngine)
		fontFile = ftEngine->loadType1Font(idA, LOAD_FONT_ARGS_CALLS(), enc);
#endif

#if !LOAD_FONTS_FROM_MEM && !defined(_WIN32) && !defined(__ANDROID__)
	// delete the (temporary) font file --
	// with Unix hard link semantics, this will remove the last link; otherwise it will return an error,
	// leaving the file to be deleted later (if loadXYZFont failed, the file will always be deleted)
	if (deleteFile)
		unlink(fontFile ? fontFile->fileName.c_str() : fileName);
#endif

	if (!fontFile)
		badFontFiles->append(idA);

	return fontFile;
}

SplashFontFile* SplashFontEngine::loadType1CFont(SplashFontFileID* idA, LOAD_FONT_ARGS_DEFS(), int* codeToGID, const VEC_STR& enc)
{
	SplashFontFile* fontFile = nullptr;
	if (!fontFile) gfree(codeToGID);
#if HAVE_FREETYPE_H
	if (!fontFile && ftEngine)
		fontFile = ftEngine->loadType1CFont(idA, LOAD_FONT_ARGS_CALLS(), enc);
#endif

#if !LOAD_FONTS_FROM_MEM && !defined(_WIN32) && !defined(__ANDROID__)
	// delete the (temporary) font file --
	// with Unix hard link semantics, this will remove the last link; otherwise it will return an error,
	// leaving the file to be deleted later (if loadXYZFont failed, the file will always be deleted)
	if (deleteFile)
		unlink(fontFile ? fontFile->fileName.c_str() : fileName);
#endif

	if (!fontFile)
		badFontFiles->append(idA);

	return fontFile;
}

SplashFontFile* SplashFontEngine::loadOpenTypeT1CFont(SplashFontFileID* idA, LOAD_FONT_ARGS_DEFS(), int* codeToGID, const VEC_STR& enc)
{
	SplashFontFile* fontFile = nullptr;
	if (!fontFile) gfree(codeToGID);
#if HAVE_FREETYPE_H
	if (!fontFile && ftEngine)
		fontFile = ftEngine->loadOpenTypeT1CFont(idA, LOAD_FONT_ARGS_CALLS(), enc);
#endif

#if !LOAD_FONTS_FROM_MEM && !defined(_WIN32) && !defined(__ANDROID__)
	// delete the (temporary) font file --
	// with Unix hard link semantics, this will remove the last link; otherwise it will return an error,
	// leaving the file to be deleted later (if loadXYZFont failed, the file will always be deleted)
	if (deleteFile)
		unlink(fontFile ? fontFile->fileName.c_str() : fileName);
#endif

	if (!fontFile)
		badFontFiles->append(idA);

	return fontFile;
}

SplashFontFile* SplashFontEngine::loadCIDFont(SplashFontFileID* idA, LOAD_FONT_ARGS_DEFS(), int* codeToGID, int codeToGIDLen)
{
	SplashFontFile* fontFile = nullptr;
#if HAVE_FREETYPE_H
	if (!fontFile && ftEngine)
		fontFile = ftEngine->loadCIDFont(idA, LOAD_FONT_ARGS_CALLS(), codeToGID, codeToGIDLen);
#endif

	if (!fontFile)
		gfree(codeToGID);

#if !LOAD_FONTS_FROM_MEM && !defined(_WIN32) && !defined(__ANDROID__)
	// delete the (temporary) font file --
	// with Unix hard link semantics, this will remove the last link; otherwise it will return an error,
	// leaving the file to be deleted later (if loadXYZFont failed, the file will always be deleted)
	if (deleteFile)
		unlink(fontFile ? fontFile->fileName.c_str() : fileName);
#endif

	if (!fontFile)
		badFontFiles->append(idA);

	return fontFile;
}

SplashFontFile* SplashFontEngine::loadOpenTypeCFFFont(SplashFontFileID* idA, LOAD_FONT_ARGS_DEFS(), int* codeToGID, int codeToGIDLen)
{
	SplashFontFile* fontFile = nullptr;
#if HAVE_FREETYPE_H
	if (!fontFile && ftEngine)
		fontFile = ftEngine->loadOpenTypeCFFFont(idA, LOAD_FONT_ARGS_CALLS(), codeToGID, codeToGIDLen);
#endif

	if (!fontFile)
		gfree(codeToGID);

#if !LOAD_FONTS_FROM_MEM && !defined(_WIN32) && !defined(__ANDROID__)
	// delete the (temporary) font file --
	// with Unix hard link semantics, this will remove the last link; otherwise it will return an error,
	// leaving the file to be deleted later (if loadXYZFont failed, the file will always be deleted)
	/if (deleteFile)
		unlink(fontFile ? fontFile->fileName.c_str() : fileName);
#endif

	if (!fontFile)
		badFontFiles->append(idA);

	return fontFile;
}

SplashFontFile* SplashFontEngine::loadTrueTypeFont(SplashFontFileID* idA, LOAD_FONT_ARGS_DEFS(), int fontNum, int* codeToGID, int codeToGIDLen, const char* fontName)
{
	SplashFontFile* fontFile = nullptr;
#if HAVE_FREETYPE_H
	if (!fontFile && ftEngine)
		fontFile = ftEngine->loadTrueTypeFont(idA, LOAD_FONT_ARGS_CALLS(), fontNum, codeToGID, codeToGIDLen);
#endif

	if (!fontFile)
		gfree(codeToGID);

#if !LOAD_FONTS_FROM_MEM && !defined(_WIN32) && !defined(__ANDROID__)
	// delete the (temporary) font file --
	// with Unix hard link semantics, this will remove the last link; otherwise it will return an error,
	// leaving the file to be deleted later (if loadXYZFont failed, the file will always be deleted)
	/if (deleteFile)
		unlink(fontFile ? fontFile->fileName.c_str() : fileName);
#endif

	if (!fontFile)
		badFontFiles->append(idA);

	return fontFile;
}

SplashFont* SplashFontEngine::getFont(SplashFontFile* fontFile, SplashCoord* textMat, SplashCoord* ctm)
{
	SplashCoord mat[4];
	SplashFont* font;

	mat[0] = textMat[0] * ctm[0] + textMat[1] * ctm[2];
	mat[1] = -(textMat[0] * ctm[1] + textMat[1] * ctm[3]);
	mat[2] = textMat[2] * ctm[0] + textMat[3] * ctm[2];
	mat[3] = -(textMat[2] * ctm[1] + textMat[3] * ctm[3]);
	if (!splashCheckDet(mat[0], mat[1], mat[2], mat[3], 0.01))
	{
		// avoid a singular (or close-to-singular) matrix
		mat[0] = 0.01;
		mat[1] = 0;
		mat[2] = 0;
		mat[3] = 0.01;
	}

	font = fontCache[0];
	if (font && font->matches(fontFile, mat, textMat))
		return font;
	for (int i = 1; i < splashFontCacheSize; ++i)
	{
		font = fontCache[i];
		if (font && font->matches(fontFile, mat, textMat))
		{
			for (int j = i; j > 0; --j)
				fontCache[j] = fontCache[j - 1];
			fontCache[0] = font;
			return font;
		}
	}
	font = fontFile->makeFont(mat, textMat);
	if (fontCache[splashFontCacheSize - 1])
		delete fontCache[splashFontCacheSize - 1];
	for (int j = splashFontCacheSize - 1; j > 0; --j)
		fontCache[j] = fontCache[j - 1];
	fontCache[0] = font;
	return font;
}
