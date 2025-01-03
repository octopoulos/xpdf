//========================================================================
//
// pdffonts.cc
//
// Copyright 2001-2007 Glyph & Cog, LLC
// updated by octopoulos @ 2024-12-29
//
//========================================================================

// --cfgFileName "c:/web/gori/data/xpdf.cfg" -- "C:/web/gori/source/pdf/bug.pdf"

#include <aconf.h>
#include "AcroForm.h"
#include "Annot.h"
#include "Error.h"
#include "GfxFont.h"
#include "GlobalParams.h"
#include "PDFDoc.h"

#include <CLI/CLI.hpp>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// NB: this must match the definition of GfxFontType in GfxFont.h.
static const char* fontTypeNames[] = {
	"unknown",
	"Type 1",
	"Type 1C",
	"Type 1C (OT)",
	"Type 3",
	"TrueType",
	"TrueType (OT)",
	"CID Type 0",
	"CID Type 0C",
	"CID Type 0C (OT)",
	"CID TrueType",
	"CID TrueType (OT)"
};

static void scanFonts(Object* obj, PDFDoc* doc, int showFontLoc, int showFontLocPS);
static void scanFonts(Dict* resDict, PDFDoc* doc, int showFontLoc, int showFontLocPS);
static void scanFont(GfxFont* font, PDFDoc* doc, int showFontLoc, int showFontLocPS);
static bool checkObject(Object* in, Object* out, PDFDoc* doc);

static Ref*  fonts;
static int   fontsLen;
static int   fontsSize;
static char* seenObjs;
static int   numObjects;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
	// OPTIONS
	//////////

	CLI_BEGIN("pdffonts");

	// name, type, default, implicit, needApp, help
	// clang-format off
	CLI_OPTION(cfgFileName  , std::string, "", "", 0, "configuration file to use in place of .xpdfrc");
	CLI_OPTION(firstPage    , int        , 1 , 1 , 0, "first page to convert");
	CLI_OPTION(lastPage     , int        , 0 , 0 , 0, "last page to convert");
	CLI_OPTION(ownerPassword, std::string, "", "", 0, "owner password (for encrypted files)");
	CLI_OPTION(printVersion , int        , 0 , 1 , 0, "print copyright and version info");
	CLI_OPTION(showFontLoc  , int        , 0 , 1 , 0, "print extended info on font location");
	CLI_OPTION(showFontLocPS, int        , 0 , 1 , 0, "print extended info on font location for PostScript conversion");
	CLI_OPTION(userPassword , std::string, "", "", 0, "user password (for encrypted files)");
	//
	CLI_REQUIRED(fileName, std::string, 1, "<PDF-file>");
	// clang-format on

	CLI11_PARSE(cli, argc, argv);
	CLI_NEEDAPP();

	Dict*     resDict;
	AcroForm* form;

	// config file
	{
		if (CLI_STRING(cfgFileName) && !std::filesystem::exists(cfgFileName))
			error(errConfig, -1, "Config file '{}' doesn't exist or isn't a file", cfgFileName);

		globalParams = std::make_shared<GlobalParams>(cfgFileName.c_str());
		globalParams->setupBaseFonts(nullptr);
	}

	// open PDF file
	auto doc = std::make_unique<PDFDoc>(fileName, ownerPassword, userPassword);
	if (!doc->isOk()) return 1;

	// get page range
	if (firstPage < 1) firstPage = 1;
	if (lastPage < 1 || lastPage > doc->getNumPages())
		lastPage = doc->getNumPages();

	// scan the fonts
	if (showFontLoc || showFontLocPS)
	{
		printf("name                                           type              emb sub uni prob object ID location\n");
		printf("---------------------------------------------- ----------------- --- --- --- ---- --------- --------\n");
	}
	else
	{
		printf("name                                           type              emb sub uni prob object ID\n");
		printf("---------------------------------------------- ----------------- --- --- --- ---- ---------\n");
	}
	fonts    = nullptr;
	fontsLen = fontsSize = 0;
	numObjects           = doc->getXRef()->getNumObjects();
	seenObjs             = (char*)gmalloc(numObjects);
	memset(seenObjs, 0, numObjects);
	Annots* annots = doc->getAnnots();
	for (int pg = firstPage; pg <= lastPage; ++pg)
	{
		Page* page = doc->getCatalog()->getPage(pg);
		if ((resDict = page->getResourceDict()))
			scanFonts(resDict, doc.get(), showFontLoc, showFontLocPS);
		int nAnnots = annots->getNumAnnots(pg);
		for (int i = 0; i < nAnnots; ++i)
		{
			Object obj1;
			if (annots->getAnnot(pg, i)->getAppearance(&obj1)->isStream())
			{
				Object obj2;
				obj1.streamGetDict()->lookupNF("Resources", &obj2);
				scanFonts(&obj2, doc.get(), showFontLoc, showFontLocPS);
				obj2.free();
			}
			obj1.free();
		}
	}
	if ((form = doc->getCatalog()->getForm()))
	{
		for (int i = 0; i < form->getNumFields(); ++i)
		{
			Object obj1;
			form->getField(i)->getResources(&obj1);
			if (obj1.isArray())
			{
				for (int j = 0; j < obj1.arrayGetLength(); ++j)
				{
					Object obj2;
					obj1.arrayGetNF(j, &obj2);
					scanFonts(&obj2, doc.get(), showFontLoc, showFontLocPS);
					obj2.free();
				}
			}
			else if (obj1.isDict())
			{
				scanFonts(obj1.getDict(), doc.get(), showFontLoc, showFontLocPS);
			}
			obj1.free();
		}
	}

	// clean up
	gfree(fonts);
	gfree(seenObjs);

	// check for memory leaks
	Object::memCheck(stderr);
	gMemReport(stderr);
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void scanFonts(Object* obj, PDFDoc* doc, int showFontLoc, int showFontLocPS)
{
	Object obj2;
	if (checkObject(obj, &obj2, doc) && obj2.isDict())
		scanFonts(obj2.getDict(), std::move(doc), showFontLoc, showFontLocPS);
	obj2.free();
}

static void scanFonts(Dict* resDict, PDFDoc* doc, int showFontLoc, int showFontLocPS)
{
	Object       fontDict1, fontDict2, xObjDict1, xObjDict2, xObj1, xObj2;
	Object       patternDict1, patternDict2, pattern1, pattern2;
	Object       gsDict1, gsDict2, gs1, gs2, smask1, smask2, smaskGroup1, smaskGroup2;
	Object       resObj;
	Ref          r;
	GfxFontDict* gfxFontDict;
	GfxFont*     font;

	// scan the fonts in this resource dictionary
	gfxFontDict = nullptr;
	resDict->lookupNF("Font", &fontDict1);
	if (checkObject(&fontDict1, &fontDict2, doc) && fontDict2.isDict())
	{
		if (fontDict1.isRef())
		{
			r           = fontDict1.getRef();
			gfxFontDict = new GfxFontDict(doc->getXRef(), &r, fontDict2.getDict());
		}
		else
		{
			gfxFontDict = new GfxFontDict(doc->getXRef(), nullptr, fontDict2.getDict());
		}
		if (gfxFontDict)
		{
			for (int i = 0; i < gfxFontDict->getNumFonts(); ++i)
				if ((font = gfxFontDict->getFont(i)))
					scanFont(font, doc, showFontLoc, showFontLocPS);
			delete gfxFontDict;
		}
	}
	fontDict2.free();
	fontDict1.free();

	// recursively scan any resource dictionaries in XObjects in this
	// resource dictionary
	resDict->lookupNF("XObject", &xObjDict1);
	if (checkObject(&xObjDict1, &xObjDict2, doc) && xObjDict2.isDict())
	{
		for (int i = 0; i < xObjDict2.dictGetLength(); ++i)
		{
			xObjDict2.dictGetValNF(i, &xObj1);
			if (checkObject(&xObj1, &xObj2, doc) && xObj2.isStream())
			{
				xObj2.streamGetDict()->lookupNF("Resources", &resObj);
				scanFonts(&resObj, doc, showFontLoc, showFontLocPS);
				resObj.free();
			}
			xObj2.free();
			xObj1.free();
		}
	}
	xObjDict2.free();
	xObjDict1.free();

	// recursively scan any resource dictionaries in Patterns in this
	// resource dictionary
	resDict->lookupNF("Pattern", &patternDict1);
	if (checkObject(&patternDict1, &patternDict2, doc) && patternDict2.isDict())
	{
		for (int i = 0; i < patternDict2.dictGetLength(); ++i)
		{
			patternDict2.dictGetValNF(i, &pattern1);
			if (checkObject(&pattern1, &pattern2, doc) && pattern2.isStream())
			{
				pattern2.streamGetDict()->lookupNF("Resources", &resObj);
				scanFonts(&resObj, doc, showFontLoc, showFontLocPS);
				resObj.free();
			}
			pattern2.free();
			pattern1.free();
		}
	}
	patternDict2.free();
	patternDict1.free();

	// recursively scan any resource dictionaries in ExtGStates in this resource dictionary
	resDict->lookupNF("ExtGState", &gsDict1);
	if (checkObject(&gsDict1, &gsDict2, doc) && gsDict2.isDict())
	{
		for (int i = 0; i < gsDict2.dictGetLength(); ++i)
		{
			gsDict2.dictGetValNF(i, &gs1);
			if (checkObject(&gs1, &gs2, doc) && gs2.isDict())
			{
				gs2.dictLookupNF("SMask", &smask1);
				if (checkObject(&smask1, &smask2, doc) && smask2.isDict())
				{
					smask2.dictLookupNF("G", &smaskGroup1);
					if (checkObject(&smaskGroup1, &smaskGroup2, doc) && smaskGroup2.isStream())
					{
						smaskGroup2.streamGetDict()->lookupNF("Resources", &resObj);
						scanFonts(&resObj, doc, showFontLoc, showFontLocPS);
						resObj.free();
					}
					smaskGroup2.free();
					smaskGroup1.free();
				}
				smask2.free();
				smask1.free();
			}
			gs2.free();
			gs1.free();
		}
	}
	gsDict2.free();
	gsDict1.free();
}

static void scanFont(GfxFont* font, PDFDoc* doc, int showFontLoc, int showFontLocPS)
{
	Ref         fontRef, embRef;
	Object      fontObj, toUnicodeObj;
	bool        emb, subset, hasToUnicode;
	GfxFontLoc* loc;

	fontRef = *font->getID();

	// check for an already-seen font
	for (int i = 0; i < fontsLen; ++i)
		if (fontRef.num == fonts[i].num && fontRef.gen == fonts[i].gen)
			return;

	// font name
	const auto name = font->getName();

	// check for an embedded font
	if (font->getType() == fontType3)
		emb = true;
	else
		emb = font->getEmbeddedFontID(&embRef);

	// look for a ToUnicode map
	hasToUnicode = false;
	if (doc->getXRef()->fetch(fontRef.num, fontRef.gen, &fontObj)->isDict())
	{
		hasToUnicode = fontObj.dictLookup("ToUnicode", &toUnicodeObj)->isStream();
		toUnicodeObj.free();
	}
	fontObj.free();

	// check for a font subset name: capital letters followed by a '+'
	// sign
	subset = false;
	if (name.size())
	{
		int i;
		for (i = 0; i < name.size(); ++i)
			if (name.at(i) < 'A' || name.at(i) > 'Z')
				break;
		subset = i > 0 && i < name.size() && name.at(i) == '+';
	}

	// print the font info
	printf("%-46s %-17s %-3s %-3s %-3s %-4s", name.size() ? name.c_str() : "[none]", fontTypeNames[font->getType()], emb ? "yes" : "no", subset ? "yes" : "no", hasToUnicode ? "yes" : "no", font->problematicForUnicode() ? " X" : "");
	if (fontRef.gen >= 100000)
		printf(" [none]");
	else
		printf(" %6d %2d", fontRef.num, fontRef.gen);
	if (showFontLoc || showFontLocPS)
	{
		if (font->getType() == fontType3)
		{
			printf(" embedded");
		}
		else
		{
			loc = font->locateFont(doc->getXRef(), showFontLocPS);
			if (loc)
			{
				if (loc->locType == gfxFontLocEmbedded)
				{
					printf(" embedded");
				}
				else if (loc->locType == gfxFontLocExternal)
				{
					if (!loc->path.empty())
						printf(" external: %s", loc->path.string().c_str());
					else
						printf(" unavailable");
				}
				else if (loc->locType == gfxFontLocResident)
				{
					if (!loc->path.empty())
						printf(" resident: %s", loc->path.string().c_str());
					else
						printf(" unavailable");
				}
			}
			else
			{
				printf(" unknown");
			}
			delete loc;
		}
	}
	printf("\n");

	// add this font to the list
	if (fontsLen == fontsSize)
	{
		if (fontsSize <= INT_MAX - 32)
		{
			fontsSize += 32;
		}
		else
		{
			// let greallocn throw an exception
			fontsSize = -1;
		}
		fonts = (Ref*)greallocn(fonts, fontsSize, sizeof(Ref));
	}
	fonts[fontsLen++] = *font->getID();
}

static bool checkObject(Object* in, Object* out, PDFDoc* doc)
{
	int objNum;

	if (!in->isRef())
	{
		in->copy(out);
		return true;
	}
	objNum = in->getRefNum();
	if (objNum < 0 || objNum >= numObjects)
	{
		out->initNull();
		return true;
	}
	if (seenObjs[objNum])
	{
		out->initNull();
		return false;
	}
	seenObjs[objNum] = (char)1;
	in->fetch(doc->getXRef(), out);
	return true;
}
