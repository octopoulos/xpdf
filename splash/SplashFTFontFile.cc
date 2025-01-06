//========================================================================
//
// SplashFTFontFile.cc
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>

#if HAVE_FREETYPE_H
#	include "gmem.h"
#	include "gmempp.h"
#	include "GString.h"
#	include "SplashFTFontEngine.h"
#	include "SplashFTFont.h"
#	include "SplashFTFontFile.h"

//------------------------------------------------------------------------
// SplashFTFontFile
//------------------------------------------------------------------------

SplashFontFile* SplashFTFontFile::loadType1Font(SplashFTFontEngine* engineA, SplashFontFileID* idA, SplashFontType fontTypeA, LOAD_FONT_ARGS_DEFS(A), const VEC_STR& encA)
{
	FT_Face     faceA;
	int*        codeToGIDA;
	std::string name;

#	if LOAD_FONTS_FROM_MEM
	if (FT_New_Memory_Face(engineA->lib, (FT_Byte*)fontBufA.c_str(), static_cast<FT_Long>(fontBufA.size()), 0, &faceA))
	{
#	else
	if (FT_New_Face(engineA->lib, fileNameA, 0, &faceA))
	{
#	endif
		return nullptr;
	}
	codeToGIDA = (int*)gmallocn(256, sizeof(int));
	for (int i = 0; i < 256; ++i)
	{
		codeToGIDA[i] = 0;
		if ((name = encA[i]).size())
			codeToGIDA[i] = (int)FT_Get_Name_Index(faceA, name.c_str());
	}

	return new SplashFTFontFile(engineA, idA, fontTypeA, LOAD_FONT_ARGS_CALLS(A), faceA, codeToGIDA, 256);
}

SplashFontFile* SplashFTFontFile::loadCIDFont(SplashFTFontEngine* engineA, SplashFontFileID* idA, SplashFontType fontTypeA, LOAD_FONT_ARGS_DEFS(A), int* codeToGIDA, int codeToGIDLenA)
{
	FT_Face faceA;

#	if LOAD_FONTS_FROM_MEM
	if (FT_New_Memory_Face(engineA->lib, (FT_Byte*)fontBufA.c_str(), static_cast<FT_Long>(fontBufA.size()), 0, &faceA))
	{
#	else
	if (FT_New_Face(engineA->lib, fileNameA, 0, &faceA))
	{
#	endif
		return nullptr;
	}

	return new SplashFTFontFile(engineA, idA, fontTypeA, LOAD_FONT_ARGS_CALLS(A), faceA, codeToGIDA, codeToGIDLenA);
}

SplashFontFile* SplashFTFontFile::loadTrueTypeFont(SplashFTFontEngine* engineA, SplashFontFileID* idA, SplashFontType fontTypeA, LOAD_FONT_ARGS_DEFS(A), int fontNum, int* codeToGIDA, int codeToGIDLenA)
{
	FT_Face faceA;

#	if LOAD_FONTS_FROM_MEM
	if (FT_New_Memory_Face(engineA->lib, (FT_Byte*)fontBufA.c_str(), static_cast<FT_Long>(fontBufA.size()), fontNum, &faceA))
	{
#	else
	if (FT_New_Face(engineA->lib, fileNameA, fontNum, &faceA))
	{
#	endif
		return nullptr;
	}

	return new SplashFTFontFile(engineA, idA, fontTypeA, LOAD_FONT_ARGS_CALLS(A), faceA, codeToGIDA, codeToGIDLenA);
}

SplashFTFontFile::SplashFTFontFile(SplashFTFontEngine* engineA, SplashFontFileID* idA, SplashFontType fontTypeA, LOAD_FONT_ARGS_DEFS(A), FT_Face faceA, int* codeToGIDA, int codeToGIDLenA)
    :
#	if LOAD_FONTS_FROM_MEM
    SplashFontFile(idA, fontTypeA, fontBufA)
#	else
    SplashFontFile(idA, fontTypeA, fileNameA, deleteFileA)
#	endif
{
	engine       = engineA;
	face         = faceA;
	codeToGID    = codeToGIDA;
	codeToGIDLen = codeToGIDLenA;
}

SplashFTFontFile::~SplashFTFontFile()
{
	if (face) FT_Done_Face(face);
	if (codeToGID) gfree(codeToGID);
}

SplashFont* SplashFTFontFile::makeFont(SplashCoord* mat, SplashCoord* textMat)
{
	SplashFont* font = new SplashFTFont(this, mat, textMat);
	font->initCache();
	return font;
}

#endif // HAVE_FREETYPE_H
