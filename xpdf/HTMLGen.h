//========================================================================
//
// HTMLGen.h
//
// Copyright 2010-2021 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "TextOutputDev.h"

class PDFDoc;
class TextOutputDev;
class SplashOutputDev;
class HTMLGenFontDefn;

//------------------------------------------------------------------------

class HTMLGen
{
public:
	HTMLGen(double backgroundResolutionA, bool tableMode);
	~HTMLGen();

	bool isOk() { return ok; }

	double getBackgroundResolution() { return backgroundResolution; }

	void setBackgroundResolution(double backgroundResolutionA)
	{
		backgroundResolution = backgroundResolutionA;
	}

	double getZoom() { return zoom; }

	void setZoom(double zoomA) { zoom = zoomA; }

	void setVStretch(double vStretchA) { vStretch = vStretchA; }

	bool getDrawInvisibleText() { return drawInvisibleText; }

	void setDrawInvisibleText(bool drawInvisibleTextA)
	{
		drawInvisibleText = drawInvisibleTextA;
	}

	bool getAllTextInvisible() { return allTextInvisible; }

	void setAllTextInvisible(bool allTextInvisibleA)
	{
		allTextInvisible = allTextInvisibleA;
	}

	void setExtractFontFiles(bool extractFontFilesA)
	{
		extractFontFiles = extractFontFilesA;
	}

	void setConvertFormFields(bool convertFormFieldsA)
	{
		convertFormFields = convertFormFieldsA;
	}

	void setEmbedBackgroundImage(bool embedBackgroundImageA)
	{
		embedBackgroundImage = embedBackgroundImageA;
	}

	void setEmbedFonts(bool embedFontsA)
	{
		embedFonts = embedFontsA;
	}

	void setIncludeMetadata(bool includeMetadataA)
	{
		includeMetadata = includeMetadataA;
	}

	void startDoc(PDFDoc* docA);
	int  convertPage(int pg, const char* pngURL, const char* htmlDir, int (*writeHTML)(void* stream, const char* data, size_t size), void* htmlStream, int (*writePNG)(void* stream, const char* data, size_t size), void* pngStream);

	// Get the counter values.
	int getNumVisibleChars() { return nVisibleChars; }

	int getNumInvisibleChars() { return nInvisibleChars; }

	int getNumRemovedDupChars() { return nRemovedDupChars; }

private:
	int              findDirSpan(GList* words, int firstWordIdx, int primaryDir, int* spanDir);
	void             appendSpans(GList* words, int firstWordIdx, int lastWordIdx, int primaryDir, int spanDir, double base, bool dropCapLine, std::string& s);
	void             appendUTF8(Unicode u, std::string& s);
	HTMLGenFontDefn* getFontDefn(TextFontInfo* font, const char* htmlDir);
	HTMLGenFontDefn* getFontFile(TextFontInfo* font, const char* htmlDir);
	HTMLGenFontDefn* getSubstituteFont(TextFontInfo* font);
	void             getFontDetails(TextFontInfo* font, const char** family, const char** weight, const char** style, double* scale);
	void             genDocMetadata(int (*writeHTML)(void* stream, const char* data, size_t size), void* htmlStream);
	void             genDocMetadataItem(int (*writeHTML)(void* stream, const char* data, size_t size), void* htmlStream, Dict* infoDict, const char* key);

	double                    backgroundResolution = 0;       //
	double                    zoom                 = 1.0;     //
	double                    vStretch             = 1.0;     //
	bool                      drawInvisibleText    = true;    //
	bool                      allTextInvisible     = false;   //
	bool                      extractFontFiles     = false;   //
	bool                      convertFormFields    = false;   //
	bool                      embedBackgroundImage = false;   //
	bool                      embedFonts           = false;   //
	bool                      includeMetadata      = false;   //
	PDFDoc*                   doc                  = nullptr; //
	TextOutputDev*            textOut              = nullptr; //
	SplashOutputDev*          splashOut            = nullptr; //
	std::vector<TextFontInfo> fonts                = {};      // [TextFontInfo]
	std::vector<double>       fontScales           = {};      //
	GList*                    fontDefns            = nullptr; // [HTMLGenFontDefn]
	int                       nextFontFaceIdx      = 0;       //
	TextFontInfo*             formFieldFont        = nullptr; //
	GList*                    formFieldInfo        = nullptr; // [HTMLGenFormFieldInfo]
	int                       nextFieldID          = 0;       //
	int                       nVisibleChars        = 0;       // number of visible chars on the page
	int                       nInvisibleChars      = 0;       // number of invisible chars on the page
	int                       nRemovedDupChars     = 0;       // number of duplicate chars removed
	bool                      ok                   = true;    //
};
