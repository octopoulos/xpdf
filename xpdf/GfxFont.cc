//========================================================================
//
// GfxFont.cc
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>
#include "gmem.h"
#include "gmempp.h"
#include "GList.h"
#include "Error.h"
#include "Object.h"
#include "Dict.h"
#include "GlobalParams.h"
#include "CMap.h"
#include "CharCodeToUnicode.h"
#include "FontEncodingTables.h"
#include "BuiltinFontTables.h"
#include "FoFiIdentifier.h"
#include "FoFiType1.h"
#include "FoFiType1C.h"
#include "FoFiTrueType.h"
#include "GfxFont.h"
#include "GString.h" // LowerCase

//------------------------------------------------------------------------

struct Base14FontMapEntry
{
	std::string altName;
	std::string base14Name;
};

// clang-format off
static Base14FontMapEntry base14FontMap[] = {
	{ "Arial"                       , "Helvetica"             },
	{ "Arial,Bold"                  , "Helvetica-Bold"        },
	{ "Arial,BoldItalic"            , "Helvetica-BoldOblique" },
	{ "Arial,Italic"                , "Helvetica-Oblique"     },
	{ "Arial-Bold"                  , "Helvetica-Bold"        },
	{ "Arial-BoldItalic"            , "Helvetica-BoldOblique" },
	{ "Arial-BoldItalicMT"          , "Helvetica-BoldOblique" },
	{ "Arial-BoldMT"                , "Helvetica-Bold"        },
	{ "Arial-Italic"                , "Helvetica-Oblique"     },
	{ "Arial-ItalicMT"              , "Helvetica-Oblique"     },
	{ "ArialMT"                     , "Helvetica"             },
	{ "Courier"                     , "Courier"               },
	{ "Courier,Bold"                , "Courier-Bold"          },
	{ "Courier,BoldItalic"          , "Courier-BoldOblique"   },
	{ "Courier,Italic"              , "Courier-Oblique"       },
	{ "Courier-Bold"                , "Courier-Bold"          },
	{ "Courier-BoldOblique"         , "Courier-BoldOblique"   },
	{ "Courier-Oblique"             , "Courier-Oblique"       },
	{ "CourierNew"                  , "Courier"               },
	{ "CourierNew,Bold"             , "Courier-Bold"          },
	{ "CourierNew,BoldItalic"       , "Courier-BoldOblique"   },
	{ "CourierNew,Italic"           , "Courier-Oblique"       },
	{ "CourierNew-Bold"             , "Courier-Bold"          },
	{ "CourierNew-BoldItalic"       , "Courier-BoldOblique"   },
	{ "CourierNew-Italic"           , "Courier-Oblique"       },
	{ "CourierNewPS-BoldItalicMT"   , "Courier-BoldOblique"   },
	{ "CourierNewPS-BoldMT"         , "Courier-Bold"          },
	{ "CourierNewPS-ItalicMT"       , "Courier-Oblique"       },
	{ "CourierNewPSMT"              , "Courier"               },
	{ "Helvetica"                   , "Helvetica"             },
	{ "Helvetica,Bold"              , "Helvetica-Bold"        },
	{ "Helvetica,BoldItalic"        , "Helvetica-BoldOblique" },
	{ "Helvetica,Italic"            , "Helvetica-Oblique"     },
	{ "Helvetica-Bold"              , "Helvetica-Bold"        },
	{ "Helvetica-BoldItalic"        , "Helvetica-BoldOblique" },
	{ "Helvetica-BoldOblique"       , "Helvetica-BoldOblique" },
	{ "Helvetica-Italic"            , "Helvetica-Oblique"     },
	{ "Helvetica-Oblique"           , "Helvetica-Oblique"     },
	{ "Symbol"                      , "Symbol"                },
	{ "Symbol,Bold"                 , "Symbol"                },
	{ "Symbol,BoldItalic"           , "Symbol"                },
	{ "Symbol,Italic"               , "Symbol"                },
	{ "Times-Bold"                  , "Times-Bold"            },
	{ "Times-BoldItalic"            , "Times-BoldItalic"      },
	{ "Times-Italic"                , "Times-Italic"          },
	{ "Times-Roman"                 , "Times-Roman"           },
	{ "TimesNewRoman"               , "Times-Roman"           },
	{ "TimesNewRoman,Bold"          , "Times-Bold"            },
	{ "TimesNewRoman,BoldItalic"    , "Times-BoldItalic"      },
	{ "TimesNewRoman,Italic"        , "Times-Italic"          },
	{ "TimesNewRoman-Bold"          , "Times-Bold"            },
	{ "TimesNewRoman-BoldItalic"    , "Times-BoldItalic"      },
	{ "TimesNewRoman-Italic"        , "Times-Italic"          },
	{ "TimesNewRomanPS"             , "Times-Roman"           },
	{ "TimesNewRomanPS-Bold"        , "Times-Bold"            },
	{ "TimesNewRomanPS-BoldItalic"  , "Times-BoldItalic"      },
	{ "TimesNewRomanPS-BoldItalicMT", "Times-BoldItalic"      },
	{ "TimesNewRomanPS-BoldMT"      , "Times-Bold"            },
	{ "TimesNewRomanPS-Italic"      , "Times-Italic"          },
	{ "TimesNewRomanPS-ItalicMT"    , "Times-Italic"          },
	{ "TimesNewRomanPSMT"           , "Times-Roman"           },
	{ "TimesNewRomanPSMT,Bold"      , "Times-Bold"            },
	{ "TimesNewRomanPSMT,BoldItalic", "Times-BoldItalic"      },
	{ "TimesNewRomanPSMT,Italic"    , "Times-Italic"          },
	{ "ZapfDingbats"                , "ZapfDingbats"          },
};
// clang-format on

//------------------------------------------------------------------------

// index: {fixed:0, sans-serif:4, serif:8} + bold*2 + italic
// NB: must be in same order as psSubstFonts in PSOutputDev.cc
static const VEC_STR base14SubstFonts = {
	"Courier",
	"Courier-Oblique",
	"Courier-Bold",
	"Courier-BoldOblique",
	"Helvetica",
	"Helvetica-Oblique",
	"Helvetica-Bold",
	"Helvetica-BoldOblique",
	"Times-Roman",
	"Times-Italic",
	"Times-Bold",
	"Times-BoldItalic",
	// the last two are never used for substitution
	"Symbol",
	"ZapfDingbats"
};

//------------------------------------------------------------------------

static int readFromStream(void* data)
{
	return ((Stream*)data)->getChar();
}

//------------------------------------------------------------------------
// GfxFontLoc
//------------------------------------------------------------------------

GfxFontLoc::GfxFontLoc()
{
}

GfxFontLoc::~GfxFontLoc()
{
}

//------------------------------------------------------------------------
// GfxFont
//------------------------------------------------------------------------

GfxFont* GfxFont::makeFont(XRef* xref, std::string_view tagA, Ref idA, Dict* fontDict)
{
	// get base font name
	std::string nameA;
	Object      obj1;
	fontDict->lookup("BaseFont", &obj1);
	if (obj1.isName())
		nameA = obj1.getName();
	else if (obj1.isString())
		nameA = obj1.getString();
	obj1.free();

	// get embedded font ID and font type
	Ref         embFontIDA;
	GfxFontType typeA = getFontType(xref, fontDict, &embFontIDA);

	// create the font object
	GfxFont* font = nullptr;
	if (typeA < fontCIDType0)
		font = new Gfx8BitFont(xref, tagA, idA, nameA, typeA, embFontIDA, fontDict);
	else
		font = new GfxCIDFont(xref, tagA, idA, nameA, typeA, embFontIDA, fontDict);

	return font;
}

GfxFont* GfxFont::makeDefaultFont(XRef* xref)
{
	Object type, subtype, baseFont;
	type.initName("Font");
	subtype.initName("Type1");
	baseFont.initName("Helvetica");

	Object fontDict;
	fontDict.initDict(xref);
	fontDict.dictAdd(copyString("Type"), &type);
	fontDict.dictAdd(copyString("Subtype"), &subtype);
	fontDict.dictAdd(copyString("BaseFont"), &baseFont);

	Ref ref = {
		.num = GfxFontDict::hashFontObject(&fontDict),
		.gen = 100000,
	};
	GfxFont* font = makeFont(xref, "undef", ref, fontDict.getDict());
	fontDict.free();

	return font;
}

GfxFont::GfxFont(std::string_view tagA, Ref idA, std::string_view nameA, GfxFontType typeA, Ref embFontIDA)
{
	tag       = tagA;
	id        = idA;
	name      = nameA;
	type      = typeA;
	embFontID = embFontIDA;
}

GfxFont::~GfxFont()
{
}

// This function extracts three pieces of information:
// 1. the "expected" font type, i.e., the font type implied by Font.Subtype, DescendantFont.Subtype, and FontDescriptor.FontFile3.Subtype
// 2. the embedded font object ID
// 3. the actual font type - determined by examining the embedded font if there is one, otherwise equal to the expected font type
// If the expected and actual font types don't match, a warning message is printed.
// The expected font type is not used for anything else.
GfxFontType GfxFont::getFontType(XRef* xref, Dict* fontDict, Ref* embID)
{
	embID->num = -1;
	embID->gen = -1;

	int    err     = 0;
	bool   isType0 = false;
	Object subtype;

	fontDict->lookup("Subtype", &subtype);
	GfxFontType expectedType = fontUnknownType;
	if (subtype.isName("Type1") || subtype.isName("MMType1"))
		expectedType = fontType1;
	else if (subtype.isName("Type1C"))
		expectedType = fontType1C;
	else if (subtype.isName("Type3"))
		expectedType = fontType3;
	else if (subtype.isName("TrueType"))
		expectedType = fontTrueType;
	else if (subtype.isName("Type0"))
		isType0 = true;
	else
		error(errSyntaxWarning, -1, "Unknown font type: '{}'", subtype.isName() ? subtype.getName() : "???");
	subtype.free();

	Dict*  fontDict2 = fontDict;
	Object obj1;
	Object obj2;
	if (fontDict->lookup("DescendantFonts", &obj1)->isArray())
	{
		if (obj1.arrayGetLength() == 0)
		{
			error(errSyntaxWarning, -1, "Empty DescendantFonts array in font");
			obj2.initNull();
		}
		else if (obj1.arrayGet(0, &obj2)->isDict())
		{
			if (!isType0)
				error(errSyntaxWarning, -1, "Non-CID font with DescendantFonts array");
			fontDict2 = obj2.getDict();
			fontDict2->lookup("Subtype", &subtype);
			if (subtype.isName("CIDFontType0"))
			{
				if (isType0)
					expectedType = fontCIDType0;
			}
			else if (subtype.isName("CIDFontType2"))
			{
				if (isType0)
					expectedType = fontCIDType2;
			}
			subtype.free();
		}
	}
	else
	{
		obj2.initNull();
	}

	// NB: the PDF spec doesn't say anything about precedence, but Adobe uses FontFile3 over FontFile2 if both are present.
	Object fontDesc;
	Object obj3;
	Object obj4;
	if (fontDict2->lookup("FontDescriptor", &fontDesc)->isDict())
	{
		if (fontDesc.dictLookupNF("FontFile3", &obj3)->isRef())
		{
			*embID = obj3.getRef();
			if (obj3.fetch(xref, &obj4)->isStream())
			{
				obj4.streamGetDict()->lookup("Subtype", &subtype);
				if (subtype.isName("Type1"))
				{
					if (expectedType != fontType1)
					{
						err |= 1;
						expectedType = isType0 ? fontCIDType0 : fontType1;
					}
				}
				else if (subtype.isName("Type1C"))
				{
					if (expectedType == fontType1)
					{
						expectedType = fontType1C;
					}
					else if (expectedType != fontType1C)
					{
						err |= 2;
						expectedType = isType0 ? fontCIDType0C : fontType1C;
					}
				}
				else if (subtype.isName("TrueType"))
				{
					if (expectedType != fontTrueType)
					{
						err |= 4;
						expectedType = isType0 ? fontCIDType2 : fontTrueType;
					}
				}
				else if (subtype.isName("CIDFontType0C"))
				{
					if (expectedType == fontCIDType0)
					{
						expectedType = fontCIDType0C;
					}
					else
					{
						err |= 8;
						expectedType = isType0 ? fontCIDType0C : fontType1C;
					}
				}
				else if (subtype.isName("OpenType"))
				{
					if (expectedType == fontTrueType)
						expectedType = fontTrueTypeOT;
					else if (expectedType == fontType1)
						expectedType = fontType1COT;
					else if (expectedType == fontCIDType0)
						expectedType = fontCIDType0COT;
					else if (expectedType == fontCIDType2)
						expectedType = fontCIDType2OT;
					else
						err |= 16;
				}
				else
				{
					error(errSyntaxError, -1, "Unknown font type '{}'", subtype.isName() ? subtype.getName() : "???");
				}
				subtype.free();
			}
			obj4.free();
		}
		obj3.free();
		if (embID->num == -1)
		{
			if (fontDesc.dictLookupNF("FontFile2", &obj3)->isRef())
			{
				*embID = obj3.getRef();
				if (isType0)
					expectedType = fontCIDType2;
				else if (expectedType != fontTrueType)
					err |= 32;
			}
			obj3.free();
		}
		if (embID->num == -1)
		{
			if (fontDesc.dictLookupNF("FontFile", &obj3)->isRef())
			{
				*embID = obj3.getRef();
				if (expectedType != fontType1)
					err |= 64;
			}
			obj3.free();
		}
	}
	fontDesc.free();

	GfxFontType t = fontUnknownType;
	if (embID->num >= 0)
	{
		obj3.initRef(embID->num, embID->gen);
		obj3.fetch(xref, &obj4);
		if (obj4.isStream())
		{
			obj4.streamReset();
			FoFiIdentifierType fft = FoFiIdentifier::identifyStream(&readFromStream, obj4.getStream());
			obj4.streamClose();
			switch (fft)
			{
			case fofiIdType1PFA:
			case fofiIdType1PFB:
				t = fontType1;
				break;
			case fofiIdCFF8Bit:
				t = isType0 ? fontCIDType0C : fontType1C;
				break;
			case fofiIdCFFCID:
				t = fontCIDType0C;
				break;
			case fofiIdTrueType:
			case fofiIdTrueTypeCollection:
				t = isType0 ? fontCIDType2 : fontTrueType;
				break;
			case fofiIdOpenTypeCFF8Bit:
				t = isType0 ? fontCIDType0COT : fontType1COT;
				break;
			case fofiIdOpenTypeCFFCID:
				t = fontCIDType0COT;
				break;
			default:
				error(errSyntaxError, -1, "Embedded font file may be invalid");
				break;
			}
		}
		obj4.free();
		obj3.free();
	}

	if (t == fontUnknownType) t = expectedType;
	if (t != expectedType) err |= 128;

	if (err) error(errSyntaxWarning, -1, "Mismatch between font type and embedded font file: err={}", err);

	obj2.free();
	obj1.free();
	return t;
}

void GfxFont::readFontDescriptor(XRef* xref, Dict* fontDict)
{
	Object obj1, obj2, obj3, obj4;
	double t, t2;

	// assume Times-Roman by default (for substitution purposes)
	flags = fontSerif;

	if (fontDict->lookup("FontDescriptor", &obj1)->isDict())
	{
		// get flags
		if (obj1.dictLookup("Flags", &obj2)->isInt())
			flags = obj2.getInt();
		obj2.free();

		// get name
		obj1.dictLookup("FontName", &obj2);
		if (obj2.isName()) ASSIGN_CSTRING(embFontName, obj2.getName());
		obj2.free();

		// look for MissingWidth
		obj1.dictLookup("MissingWidth", &obj2);
		if (obj2.isNum())
			missingWidth = obj2.getNum();
		obj2.free();

		// get Ascent
		// (CapHeight is a little more reliable - so use it if present)
		obj1.dictLookup("Ascent", &obj2);
		obj1.dictLookup("CapHeight", &obj3);
		if (obj2.isNum() || obj3.isNum())
		{
			if (obj2.isNum())
			{
				t = 0.001 * obj2.getNum();
				// some broken font descriptors specify a negative ascent
				if (t < 0)
					t = -t;
			}
			else
			{
				t = 0;
			}
			if (obj3.isNum())
			{
				t2 = 0.001 * obj3.getNum();
				// some broken font descriptors specify a negative ascent
				if (t2 < 0)
					t2 = -t2;
			}
			else
			{
				t2 = 0;
			}
			if (t != 0 && t < 1.9)
				declaredAscent = t;
			// if both Ascent and CapHeight are set, use the smaller one
			// (because the most common problem is that Ascent is too large)
			if (t2 != 0 && (t == 0 || t2 < t))
				t = t2;
			// some broken font descriptors set ascent and descent to 0;
			// others set it to ridiculous values (e.g., 32768)
			if (t != 0 && t < 1.9)
				ascent = t;
		}
		obj2.free();
		obj3.free();

		// get Descent
		obj1.dictLookup("Descent", &obj2);
		if (obj2.isNum())
		{
			t = 0.001 * obj2.getNum();
			// some broken font descriptors specify a positive descent
			if (t > 0)
				t = -t;
			// some broken font descriptors set ascent and descent to 0
			if (t != 0 && t > -1.9)
				descent = t;
		}
		obj2.free();

		// font FontBBox
		if (obj1.dictLookup("FontBBox", &obj2)->isArray())
		{
			for (int i = 0; i < 4 && i < obj2.arrayGetLength(); ++i)
			{
				if (obj2.arrayGet(i, &obj3)->isNum())
					fontBBox[i] = 0.001 * obj3.getNum();
				obj3.free();
			}
		}
		obj2.free();
	}
	obj1.free();

	// scan font name for bold/italic tags and update the flags
	if (int i = TO_INT(name.size()))
	{
		if (i > 2 && !strncmp(name.c_str() + i - 2, "MT", 2))
			i -= 2;
		if (i > 6 && !strncmp(name.c_str() + i - 6, "Italic", 6))
		{
			flags |= fontItalic;
			i -= 6;
		}
		else if (i > 2 && !strncmp(name.c_str() + i - 2, "It", 2))
		{
			flags |= fontItalic;
			i -= 2;
		}
		else if (i > 7 && !strncmp(name.c_str() + i - 7, "Oblique", 7))
		{
			flags |= fontItalic;
			i -= 7;
		}
		char c = name.at(i - 1);
		if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')))
			--i;
		if (i > 4 && !strncmp(name.c_str() + i - 4, "Bold", 4))
			flags |= fontBold;
	}
}

CharCodeToUnicode* GfxFont::readToUnicodeCMap(Dict* fontDict, int nBits, CharCodeToUnicode* ctu)
{
	Object obj1;
	char   buf2[4096];

	if (!fontDict->lookup("ToUnicode", &obj1)->isStream())
	{
		obj1.free();
		return nullptr;
	}
	std::string buf;
	obj1.streamReset();
	int    n;
	while ((n = obj1.streamGetBlock(buf2, sizeof(buf2))) > 0)
		buf.append(buf2, n);
	obj1.streamClose();
	obj1.free();
	if (ctu)
		ctu->mergeCMap(buf, nBits);
	else
		ctu = CharCodeToUnicode::parseCMap(buf, nBits);
	hasToUnicode = true;
	return ctu;
}

GfxFontLoc* GfxFont::locateFont(XRef* xref, bool ps)
{
	GfxFontLoc*        fontLoc;
	SysFontType        sysFontType;
	FoFiIdentifierType fft;
	PSFontParam16*     psFont16;
	Object             refObj, embFontObj;
	int                substIdx, fontNum;
	double             oblique;
	bool               embed;

	if (type == fontType3) return nullptr;

	//----- embedded font
	if (embFontID.num >= 0)
	{
		embed = true;
		refObj.initRef(embFontID.num, embFontID.gen);
		refObj.fetch(xref, &embFontObj);
		if (!embFontObj.isStream())
		{
			error(errSyntaxError, -1, "Embedded font object is wrong type");
			embed = false;
		}
		embFontObj.free();
		refObj.free();
		if (embed)
		{
			if (ps)
			{
				switch (type)
				{
				case fontType1:
				case fontType1C:
				case fontType1COT: embed = globalParams->getPSEmbedType1(); break;
				case fontTrueType:
				case fontTrueTypeOT: embed = globalParams->getPSEmbedTrueType(); break;
				case fontCIDType0C:
				case fontCIDType0COT: embed = globalParams->getPSEmbedCIDPostScript(); break;
				case fontCIDType2:
				case fontCIDType2OT: embed = globalParams->getPSEmbedCIDTrueType(); break;
				default: break;
				}
			}
			if (embed)
			{
				fontLoc            = new GfxFontLoc();
				fontLoc->locType   = gfxFontLocEmbedded;
				fontLoc->fontType  = type;
				fontLoc->embFontID = embFontID;
				return fontLoc;
			}
		}
	}

	//----- PS passthrough
	if (ps && name.size() && !isCIDFont() && globalParams->getPSFontPassthrough())
	{
		fontLoc           = new GfxFontLoc();
		fontLoc->locType  = gfxFontLocResident;
		fontLoc->fontType = fontType1;
		fontLoc->path     = name;
		return fontLoc;
	}

	//----- external font file (fontFile, fontDir)
	if (name.size())
	{
		if (const auto path = globalParams->findFontFile(name); !path.empty())
		{
			if ((fontLoc = getExternalFont(path, 0, 0, isCIDFont()))) return fontLoc;
		}
	}

	//----- PS resident Base-14 font
	if (ps && !isCIDFont() && ((Gfx8BitFont*)this)->base14)
	{
		fontLoc           = new GfxFontLoc();
		fontLoc->locType  = gfxFontLocResident;
		fontLoc->fontType = fontType1;
		fontLoc->path     = ((Gfx8BitFont*)this)->base14->base14Name;
		return fontLoc;
	}

	//----- external font file for Base-14 font
	std::filesystem::path path;
	if (!ps && !isCIDFont() && ((Gfx8BitFont*)this)->base14)
	{
		const auto base14Name = ((Gfx8BitFont*)this)->base14->base14Name;
		path                  = globalParams->findBase14FontFile(base14Name, &fontNum, &oblique);
		if (!path.empty() && (fontLoc = getExternalFont(path, fontNum, oblique, false)))
			return fontLoc;
	}

	//----- system font
	if (name.size() && !(path = globalParams->findSystemFontFile(name, &sysFontType, &fontNum)).empty())
	{
		fontLoc          = new GfxFontLoc();
		fontLoc->locType = gfxFontLocExternal;
		fontLoc->path    = path;
		fontLoc->fontNum = fontNum;
		if (isCIDFont())
		{
			if (sysFontType == sysFontTTF || sysFontType == sysFontTTC)
			{
				fontLoc->fontType = fontCIDType2;
				return fontLoc;
			}
			else if (sysFontType == sysFontOTF)
			{
				fft = FoFiIdentifier::identifyFile(fontLoc->path.string().c_str());
				if (fft == fofiIdOpenTypeCFFCID)
				{
					fontLoc->fontType = fontCIDType0COT;
					return fontLoc;
				}
				else if (fft == fofiIdTrueType)
				{
					fontLoc->fontType = fontCIDType2;
					return fontLoc;
				}
			}
		}
		else if (sysFontType == sysFontTTF || sysFontType == sysFontTTC)
		{
			fontLoc->fontType = fontTrueType;
			return fontLoc;
		}
		else if (sysFontType == sysFontPFA || sysFontType == sysFontPFB)
		{
			fontLoc->fontType = fontType1;
			return fontLoc;
		}
		else if (sysFontType == sysFontOTF)
		{
			fft = FoFiIdentifier::identifyFile(fontLoc->path.string().c_str());
			if (fft == fofiIdOpenTypeCFF8Bit)
			{
				fontLoc->fontType = fontType1COT;
				return fontLoc;
			}
			else if (fft == fofiIdTrueType)
			{
				fontLoc->fontType = fontTrueTypeOT;
				return fontLoc;
			}
		}
		delete fontLoc;
	}

	if (!isCIDFont())
	{
		//----- 8-bit PS resident font
		if (ps)
		{
			if (name.size() && !(path = globalParams->getPSResidentFont(name)).empty())
			{
				fontLoc           = new GfxFontLoc();
				fontLoc->locType  = gfxFontLocResident;
				fontLoc->fontType = fontType1;
				fontLoc->path     = path;
				return fontLoc;
			}
		}

		//----- 8-bit font substitution
		if (flags & fontFixedWidth)
			substIdx = 0;
		else if (flags & fontSerif)
			substIdx = 8;
		else
			substIdx = 4;
		if (isBold()) substIdx += 2;
		if (isItalic()) substIdx += 1;

		std::string substName = base14SubstFonts[substIdx];
		if (ps)
		{
			error(errSyntaxWarning, -1, "Substituting font '{}' for '{}'", base14SubstFonts[substIdx], name);
			fontLoc           = new GfxFontLoc();
			fontLoc->locType  = gfxFontLocResident;
			fontLoc->fontType = fontType1;
			fontLoc->path     = substName;
			fontLoc->substIdx = substIdx;
			return fontLoc;
		}
		else if (path = globalParams->findBase14FontFile(substName, &fontNum, &oblique); !path.empty())
		{
			if ((fontLoc = getExternalFont(path, fontNum, oblique, false)))
			{
				error(errSyntaxWarning, -1, "Substituting font '{}' for '{}'", base14SubstFonts[substIdx], name);
				fontLoc->substIdx = substIdx;
				return fontLoc;
			}
		}

		// failed to find a substitute font
		return nullptr;
	}

	//----- 16-bit PS resident font
	if (ps && name.size() && ((psFont16 = globalParams->getPSResidentFont16(name, ((GfxCIDFont*)this)->getWMode()))))
	{
		fontLoc           = new GfxFontLoc();
		fontLoc->locType  = gfxFontLocResident;
		fontLoc->fontType = fontCIDType0; // this is not used
		fontLoc->path     = psFont16->psFontName;
		fontLoc->encoding = psFont16->encoding;
		fontLoc->wMode    = psFont16->wMode;
		return fontLoc;
	}
	if (ps && ((psFont16 = globalParams->getPSResidentFontCC(((GfxCIDFont*)this)->getCollection(), ((GfxCIDFont*)this)->getWMode()))))
	{
		error(errSyntaxWarning, -1, "Substituting font '{}' for '{}'", psFont16->psFontName, name);
		fontLoc           = new GfxFontLoc();
		fontLoc->locType  = gfxFontLocResident;
		fontLoc->fontType = fontCIDType0; // this is not used
		fontLoc->path     = psFont16->psFontName;
		fontLoc->encoding = psFont16->encoding;
		fontLoc->wMode    = psFont16->wMode;
		return fontLoc;
	}

	//----- CID font substitution
	if (path = globalParams->findCCFontFile(((GfxCIDFont*)this)->getCollection()); !path.empty())
	{
		if ((fontLoc = getExternalFont(path, 0, 0, true)))
		{
			error(errSyntaxWarning, -1, "Substituting font '{}' for '{}'", fontLoc->path.string(), name);
			return fontLoc;
		}
	}

	// failed to find a substitute font
	return nullptr;
}

GfxFontLoc* GfxFont::locateBase14Font(const std::string& base14Name)
{
	int    fontNum;
	double oblique;

	const auto path = globalParams->findBase14FontFile(base14Name, &fontNum, &oblique);
	if (path.empty()) return nullptr;
	return getExternalFont(path, fontNum, oblique, false);
}

GfxFontLoc* GfxFont::getExternalFont(const std::filesystem::path& path, int fontNum, double oblique, bool cid)
{
	GfxFontType fontType;
	GfxFontLoc* fontLoc;

	FoFiIdentifierType fft = FoFiIdentifier::identifyFile(path.string().c_str());
	switch (fft)
	{
	case fofiIdType1PFA:
	case fofiIdType1PFB:
		fontType = fontType1;
		break;
	case fofiIdCFF8Bit:
		fontType = fontType1C;
		break;
	case fofiIdCFFCID:
		fontType = fontCIDType0C;
		break;
	case fofiIdTrueType:
	case fofiIdTrueTypeCollection:
		fontType = cid ? fontCIDType2 : fontTrueType;
		break;
	case fofiIdOpenTypeCFF8Bit:
		fontType = fontType1COT;
		break;
	case fofiIdOpenTypeCFFCID:
		fontType = fontCIDType0COT;
		break;
	case fofiIdDfont:
		fontType = cid ? fontCIDType2 : fontTrueType;
		break;
	case fofiIdUnknown:
	case fofiIdError:
	default:
		fontType = fontUnknownType;
		break;
	}
	if (fontType == fontUnknownType || (cid ? (fontType < fontCIDType0) : (fontType >= fontCIDType0)))
		return nullptr;
	fontLoc           = new GfxFontLoc();
	fontLoc->locType  = gfxFontLocExternal;
	fontLoc->fontType = fontType;
	fontLoc->path     = path;
	fontLoc->fontNum  = fontNum;
	fontLoc->oblique  = oblique;
	return fontLoc;
}

char* GfxFont::readEmbFontFile(XRef* xref, int* len)
{
	Object  obj1, obj2;
	Stream* str;
	int     n;

	obj1.initRef(embFontID.num, embFontID.gen);
	obj1.fetch(xref, &obj2);
	if (!obj2.isStream())
	{
		error(errSyntaxError, -1, "Embedded font file is not a stream");
		obj2.free();
		obj1.free();
		embFontID.num = -1;
		return nullptr;
	}
	str = obj2.getStream();

	int   size = 4096;
	char* buf  = (char*)gmalloc(size);
	*len       = 0;
	str->reset();
	do
	{
		if (*len > size - 4096)
		{
			if (size > INT_MAX / 2)
			{
				error(errSyntaxError, -1, "Embedded font file is too large");
				break;
			}
			size *= 2;
			buf = (char*)grealloc(buf, size);
		}
		n = str->getBlock(buf + *len, 4096);
		*len += n;
	}
	while (n == 4096);
	str->close();

	obj2.free();
	obj1.free();

	return buf;
}

//------------------------------------------------------------------------
// Gfx8BitFont
//------------------------------------------------------------------------

Gfx8BitFont::Gfx8BitFont(XRef* xref, std::string_view tagA, Ref idA, std::string_view nameA, GfxFontType typeA, Ref embFontIDA, Dict* fontDict)
    : GfxFont(tagA, idA, nameA, typeA, embFontIDA)
{
	BuiltinFont*       builtinFont;
	int                len;
	FoFiType1*         ffT1;
	FoFiType1C*        ffT1C;
	int                code, code2;
	Unicode            toUnicode[256];
	CharCodeToUnicode *utu, *ctu2;
	Unicode            uBuf[8];
	double             mul;
	int                firstChar, lastChar;
	uint16_t           w;
	Object             obj1, obj2, obj3;
	int                n, i, a, b, m;

	enc.resize(256, "");
	ctu = nullptr;

	// do font name substitution for various aliases of the Base 14 font names
	base14 = nullptr;
	std::string name2;

	if (name.size())
	{
		name2 = name;
		i     = 0;
		while (i < name2.size())
			if (name2.at(i) == ' ')
				name2.erase(i, 1);
			else
				++i;
		a = 0;
		b = sizeof(base14FontMap) / sizeof(Base14FontMapEntry);
		// invariant: base14FontMap[a].altName <= name2 < base14FontMap[b].altName
		while (b - a > 1)
		{
			m = (a + b) / 2;
			if (name2 >= base14FontMap[m].altName)
				a = m;
			else
				b = m;
		}
		if (name2 == base14FontMap[a].altName)
			base14 = &base14FontMap[a];
	}

	// is it a built-in font?
	builtinFont = nullptr;
	if (base14)
	{
		for (i = 0; i < nBuiltinFonts; ++i)
		{
			if (base14->base14Name == builtinFonts[i].name)
			{
				builtinFont = &builtinFonts[i];
				break;
			}
		}
	}

	// default ascent/descent values
	if (builtinFont)
	{
		missingWidth   = builtinFont->missingWidth;
		ascent         = 0.001 * builtinFont->ascent;
		descent        = 0.001 * builtinFont->descent;
		declaredAscent = ascent;
		fontBBox[0]    = 0.001 * builtinFont->bbox[0];
		fontBBox[1]    = 0.001 * builtinFont->bbox[1];
		fontBBox[2]    = 0.001 * builtinFont->bbox[2];
		fontBBox[3]    = 0.001 * builtinFont->bbox[3];
	}
	else
	{
		missingWidth   = 0;
		ascent         = 0.75;
		descent        = -0.25;
		declaredAscent = ascent;
		fontBBox[0] = fontBBox[1] = fontBBox[2] = fontBBox[3] = 0;
	}

	// get info from font descriptor
	readFontDescriptor(xref, fontDict);

	// for Base-14 fonts (even if embedded), don't trust the ascent/descent/bbox values from the font descriptor
	if (builtinFont)
	{
		ascent         = 0.001 * builtinFont->ascent;
		descent        = 0.001 * builtinFont->descent;
		declaredAscent = ascent;
		fontBBox[0]    = 0.001 * builtinFont->bbox[0];
		fontBBox[1]    = 0.001 * builtinFont->bbox[1];
		fontBBox[2]    = 0.001 * builtinFont->bbox[2];
		fontBBox[3]    = 0.001 * builtinFont->bbox[3];
	}

	// get font matrix
	fontMat[0] = fontMat[3] = 1;
	fontMat[1] = fontMat[2] = fontMat[4] = fontMat[5] = 0;
	if (fontDict->lookup("FontMatrix", &obj1)->isArray())
	{
		for (i = 0; i < 6 && i < obj1.arrayGetLength(); ++i)
		{
			if (obj1.arrayGet(i, &obj2)->isNum())
				fontMat[i] = obj2.getNum();
			obj2.free();
		}
	}
	obj1.free();

	// get Type 3 bounding box, font definition, and resources
	if (type == fontType3)
	{
		if (fontDict->lookup("FontBBox", &obj1)->isArray())
		{
			for (i = 0; i < 4 && i < obj1.arrayGetLength(); ++i)
			{
				if (obj1.arrayGet(i, &obj2)->isNum())
					fontBBox[i] = obj2.getNum();
				obj2.free();
			}
		}
		obj1.free();
		if (!fontDict->lookup("CharProcs", &charProcs)->isDict())
		{
			error(errSyntaxError, -1, "Missing or invalid CharProcs dictionary in Type 3 font");
			charProcs.free();
		}
		if (!fontDict->lookup("Resources", &resources)->isDict())
			resources.free();
	}

	//----- build the font encoding -----

	// Encodings start with a base encoding, which can come from (in order of priority):
	//   1. FontDict.Encoding or FontDict.Encoding.BaseEncoding
	//        - MacRoman / MacExpert / WinAnsi / Standard
	//   2. embedded or external font file
	//   3. default:
	//        - builtin --> builtin encoding
	//        - TrueType --> WinAnsiEncoding
	//        - others --> StandardEncoding
	// and then add a list of differences (if any) from FontDict.Encoding.Differences.

	// check FontDict for base encoding
	const VEC_STR* baseEnc = nullptr;

	hasEncoding         = false;
	usesMacRomanEnc     = false;
	baseEncFromFontFile = false;
	fontDict->lookup("Encoding", &obj1);
	if (obj1.isDict())
	{
		obj1.dictLookup("BaseEncoding", &obj2);
		if (obj2.isName("MacRomanEncoding"))
		{
			hasEncoding     = true;
			usesMacRomanEnc = true;
			baseEnc         = &macRomanEncoding;
		}
		else if (obj2.isName("MacExpertEncoding"))
		{
			hasEncoding = true;
			baseEnc     = &macExpertEncoding;
		}
		else if (obj2.isName("WinAnsiEncoding"))
		{
			hasEncoding = true;
			baseEnc     = &winAnsiEncoding;
		}
		obj2.free();
	}
	else if (obj1.isName("MacRomanEncoding"))
	{
		hasEncoding     = true;
		usesMacRomanEnc = true;
		baseEnc         = &macRomanEncoding;
	}
	else if (obj1.isName("MacExpertEncoding"))
	{
		hasEncoding = true;
		baseEnc     = &macExpertEncoding;
	}
	else if (obj1.isName("WinAnsiEncoding"))
	{
		hasEncoding = true;
		baseEnc     = &winAnsiEncoding;
	}

	// check embedded font file for base encoding
	// (only for Type 1 fonts - trying to get an encoding out of a TrueType font is a losing proposition)
	ffT1  = nullptr;
	ffT1C = nullptr;
	if (type == fontType1 && embFontID.num >= 0)
	{
		if (char* buf = readEmbFontFile(xref, &len))
		{
			if ((ffT1 = FoFiType1::make(buf, len)))
			{
				embFontName = ffT1->getName();
				if (!baseEnc)
				{
					baseEnc             = &ffT1->getEncoding();
					baseEncFromFontFile = true;
				}
			}
			gfree(buf);
		}
	}
	else if (type == fontType1C && embFontID.num >= 0)
	{
		if (char* buf = readEmbFontFile(xref, &len))
		{
			if ((ffT1C = FoFiType1C::make(buf, len)))
			{
				embFontName = ffT1C->getName();
				if (!baseEnc)
				{
					baseEnc             = &ffT1C->getEncoding();
					baseEncFromFontFile = true;
				}
			}
			gfree(buf);
		}
	}

	// get default base encoding
	if (!baseEnc)
	{
		if (builtinFont && embFontID.num < 0)
		{
			baseEnc     = &builtinFont->defaultBaseEnc;
			hasEncoding = true;
		}
		else if (type == fontTrueType)
		{
			baseEnc = &winAnsiEncoding;
		}
		else
		{
			baseEnc = &standardEncoding;
		}
	}

	// copy the base encoding
	if (baseEnc) enc = *baseEnc;

	// some Type 1C font files have empty encodings, which can break the T1C->T1 conversion
	// (since the 'seac' operator depends on having the accents in the encoding),
	// so we fill in any gaps from StandardEncoding
	if (type == fontType1C && embFontID.num >= 0 && baseEncFromFontFile)
	{
		for (int i = 0; i < 256; ++i)
		{
			if (enc[i].empty() && standardEncoding[i].size())
			{
				enc[i] = standardEncoding[i];
			}
		}
	}

	// merge differences into encoding
	if (obj1.isDict())
	{
		obj1.dictLookup("Differences", &obj2);
		if (obj2.isArray())
		{
			hasEncoding = true;
			code        = 0;
			for (int i = 0; i < obj2.arrayGetLength(); ++i)
			{
				obj2.arrayGet(i, &obj3);
				if (obj3.isInt())
				{
					code = obj3.getInt();
				}
				else if (obj3.isName())
				{
					if (code >= 0 && code < 256)
					{
						enc[code] = copyString(obj3.getName());
					}
					++code;
				}
				else
				{
					error(errSyntaxError, -1, "Wrong type in font encoding resource differences ({})", obj3.getTypeName());
				}
				obj3.free();
			}
		}
		obj2.free();
	}
	obj1.free();
	if (ffT1) delete ffT1;
	if (ffT1C) delete ffT1C;

	//----- build the mapping to Unicode -----

	// pass 1: use the name-to-Unicode mapping table
	std::string charName;
	bool        missing = false;
	bool        hex     = false;
	for (code = 0; code < 256; ++code)
	{
		if ((charName = enc[code]).size())
		{
			if (!(toUnicode[code] = globalParams->mapNameToUnicode(charName)) && charName != ".notdef")
			{
				// if it wasn't in the name-to-Unicode table, check for a name that looks like 'Axx' or 'xx', where 'A' is any letter and 'xx' is two hex digits
				if ((charName.size() == 3 && isalpha(charName[0] & 0xff) && isxdigit(charName[1] & 0xff) && isxdigit(charName[2] & 0xff) && ((charName[1] >= 'a' && charName[1] <= 'f') || (charName[1] >= 'A' && charName[1] <= 'F') || (charName[2] >= 'a' && charName[2] <= 'f') || (charName[2] >= 'A' && charName[2] <= 'F'))) || (charName.size() == 2 && isxdigit(charName[0] & 0xff) && isxdigit(charName[1] & 0xff) && ((charName[0] >= 'a' && charName[0] <= 'f') || (charName[0] >= 'A' && charName[0] <= 'F') || (charName[1] >= 'a' && charName[1] <= 'f') || (charName[1] >= 'A' && charName[1] <= 'F'))))
					hex = true;
				missing = true;
			}
		}
		else
		{
			toUnicode[code] = 0;
		}
	}

	// pass 2: try to fill in the missing chars, looking for names of any of the following forms:
	// - 'xx'
	// - 'Axx'
	// - 'nn'
	// - 'Ann'
	// - 'ABnn'
	// - 'unixxxx' (possibly followed by garbage - some Arabic files use 'uni0628.medi', etc.)
	// where 'A' and 'B' are any letters, 'xx' is two hex digits, 'xxxx' is four hex digits, and 'nn' is 2-4 decimal digits
	usedNumericHeuristic = false;
	if (missing && globalParams->getMapNumericCharNames())
	{
		for (code = 0; code < 256; ++code)
		{
			if ((charName = enc[code]).size() && !toUnicode[code] && charName != ".notdef")
			{
				const int n = TO_INT(charName.size());
				code2       = -1;
				if (hex && n == 3 && isalpha(charName[0] & 0xff) && isxdigit(charName[1] & 0xff) && isxdigit(charName[2] & 0xff))
					sscanf(charName.c_str() + 1, "%x", &code2);
				else if (hex && n == 2 && isxdigit(charName[0] & 0xff) && isxdigit(charName[1] & 0xff))
					sscanf(charName.c_str(), "%x", &code2);
				else if (!hex && n >= 2 && n <= 4 && isdigit(charName[0] & 0xff) && isdigit(charName[1] & 0xff))
					code2 = atoi(charName.c_str());
				else if (n >= 3 && n <= 5 && isdigit(charName[1] & 0xff) && isdigit(charName[2] & 0xff))
					code2 = atoi(charName.c_str() + 1);
				else if (n >= 4 && n <= 6 && isdigit(charName[2] & 0xff) && isdigit(charName[3] & 0xff))
					code2 = atoi(charName.c_str() + 2);
				else if (n >= 7 && charName[0] == 'u' && charName[1] == 'n' && charName[2] == 'i' && isxdigit(charName[3] & 0xff) && isxdigit(charName[4] & 0xff) && isxdigit(charName[5] & 0xff) && isxdigit(charName[6] & 0xff))
					sscanf(charName.c_str() + 3, "%x", &code2);
				if (code2 >= 0 && code2 <= 0xffff)
				{
					toUnicode[code]      = (Unicode)code2;
					usedNumericHeuristic = true;
				}
			}
		}

		// if the 'mapUnknownCharNames' flag is set, do a simple pass-through mapping for unknown character names
	}
	else if (missing && globalParams->getMapUnknownCharNames())
	{
		for (code = 0; code < 256; ++code)
			if (!toUnicode[code])
				toUnicode[code] = code;
	}

	// construct the char code -> Unicode mapping object
	ctu = CharCodeToUnicode::make8BitToUnicode(toUnicode);

	// merge in a ToUnicode CMap, if there is one
	// -- this overwrites existing entries in ctu, i.e., the ToUnicode CMap takes precedence,
	// but the other encoding info is allowed to fill in any holes
	readToUnicodeCMap(fontDict, 8, ctu);

	// look for a Unicode-to-Unicode mapping
	if (name.size() && (utu = globalParams->getUnicodeToUnicode(name)))
	{
		for (i = 0; i < 256; ++i)
			toUnicode[i] = 0;
		ctu2 = CharCodeToUnicode::make8BitToUnicode(toUnicode);
		for (i = 0; i < 256; ++i)
		{
			n = ctu->mapToUnicode((CharCode)i, uBuf, 8);
			if (n >= 1)
			{
				n = utu->mapToUnicode((CharCode)uBuf[0], uBuf, 8);
				if (n >= 1)
					ctu2->setMapping((CharCode)i, uBuf, n);
			}
		}
		utu->decRefCnt();
		delete ctu;
		ctu = ctu2;
	}

	//----- get the character widths -----

	// initialize all widths
	for (code = 0; code < 256; ++code)
		widths[code] = missingWidth * 0.001;

	// use widths from font dict, if present
	fontDict->lookup("FirstChar", &obj1);
	firstChar = obj1.isInt() ? obj1.getInt() : 0;
	obj1.free();
	if (firstChar < 0 || firstChar > 255)
		firstChar = 0;
	fontDict->lookup("LastChar", &obj1);
	lastChar = obj1.isInt() ? obj1.getInt() : 255;
	obj1.free();
	if (lastChar < 0 || lastChar > 255)
		lastChar = 255;
	mul = (type == fontType3) ? fontMat[0] : 0.001;
	fontDict->lookup("Widths", &obj1);
	if (obj1.isArray())
	{
		flags |= fontFixedWidth;
		if (obj1.arrayGetLength() < lastChar - firstChar + 1)
			lastChar = firstChar + obj1.arrayGetLength() - 1;
		for (code = firstChar; code <= lastChar; ++code)
		{
			obj1.arrayGet(code - firstChar, &obj2);
			if (obj2.isNum())
			{
				widths[code] = obj2.getNum() * mul;
				if (fabs(widths[code] - widths[firstChar]) > 0.00001)
					flags &= ~fontFixedWidth;
			}
			obj2.free();
		}

		// use widths from built-in font
	}
	else if (builtinFont)
	{
		// this is a kludge for broken PDF files that encode char 32 as .notdef
		if (const int ww = FindDefault(builtinFont->widths, "space", 0))
			widths[32] = 0.001 * ww;
		for (code = 0; code < 256; ++code)
			if (enc[code].size() && (w = FindDefault(builtinFont->widths, enc[code], 0)))
				widths[code] = 0.001 * w;

		// couldn't find widths -- use defaults
	}
	else
	{
		// this is technically an error
		// -- the Widths entry is required for all but the Base-14 fonts
		// -- but certain PDF generators apparently don't include widths for Arial and TimesNewRoman
		if (isFixedWidth())
			i = 0;
		else if (isSerif())
			i = 8;
		else
			i = 4;
		if (isBold())
			i += 2;
		if (isItalic())
			i += 1;
		builtinFont = builtinFontSubst[i];
		// this is a kludge for broken PDF files that encode char 32 as .notdef
		if (const int ww = FindDefault(builtinFont->widths, "space", 0))
			widths[32] = 0.001 * ww;
		for (code = 0; code < 256; ++code)
			if (enc[code].size() && (w = FindDefault(builtinFont->widths, enc[code], 0)))
				widths[code] = 0.001 * w;
	}
	obj1.free();

	ok = true;
}

Gfx8BitFont::~Gfx8BitFont()
{
	ctu->decRefCnt();
	if (charProcs.isDict()) charProcs.free();
	if (resources.isDict()) resources.free();
}

int Gfx8BitFont::getNextChar(const char* s, int len, CharCode* code, Unicode* u, int uSize, int* uLen, double* dx, double* dy, double* ox, double* oy)
{
	CharCode c;
	*code = c = (CharCode)(*s & 0xff);
	*uLen     = ctu->mapToUnicode(c, u, uSize);
	*dx       = widths[c];
	*dy = *ox = *oy = 0;
	return 1;
}

CharCodeToUnicode* Gfx8BitFont::getToUnicode()
{
	ctu->incRefCnt();
	return ctu;
}

int* Gfx8BitFont::getCodeToGIDMap(FoFiTrueType* ff)
{
	int     cmapPlatform, cmapEncoding;
	int     unicodeCmap, macRomanCmap, macUnicodeCmap, msSymbolCmap, cmap;
	bool    nonsymbolic, useMacRoman, useUnicode;
	Unicode u;
	int     code, n;

	int* map = (int*)gmallocn(256, sizeof(int));
	for (int i = 0; i < 256; ++i)
		map[i] = 0;

	// This is based on the cmap/encoding selection algorithm in the PDF 2.0 spec,
	// but with some differences to match up with Adobe's behavior.
	unicodeCmap = macRomanCmap = macUnicodeCmap = msSymbolCmap = -1;
	for (int i = 0; i < ff->getNumCmaps(); ++i)
	{
		cmapPlatform = ff->getCmapPlatform(i);
		cmapEncoding = ff->getCmapEncoding(i);
		if (cmapPlatform == 3 && cmapEncoding == 1)
			unicodeCmap = i;
		else if (cmapPlatform == 1 && cmapEncoding == 0)
			macRomanCmap = i;
		else if (cmapPlatform == 0 && cmapEncoding <= 4)
			macUnicodeCmap = i;
		else if (cmapPlatform == 3 && cmapEncoding == 0)
			msSymbolCmap = i;
	}
	useMacRoman = false;
	useUnicode  = false;
	nonsymbolic = !(flags & fontSymbolic);
	if (embFontID.num < 0 && hasEncoding && unicodeCmap >= 0)
	{
		cmap       = unicodeCmap;
		useUnicode = true;
	}
	else if (!nonsymbolic && msSymbolCmap >= 0)
	{
		cmap = msSymbolCmap;
	}
	else if (unicodeCmap >= 0)
	{
		cmap       = unicodeCmap;
		useUnicode = nonsymbolic;
	}
	else if (usesMacRomanEnc && macRomanCmap >= 0)
	{
		// MacRoman cmap has higher precedence than Mac Unicode only if the font uses the MacRoman encoding
		cmap        = macRomanCmap;
		useMacRoman = true;
	}
	else if (macUnicodeCmap >= 0)
	{
		cmap       = macUnicodeCmap;
		useUnicode = nonsymbolic;
	}
	else if (macRomanCmap >= 0)
	{
		cmap        = macRomanCmap;
		useMacRoman = nonsymbolic;
	}
	else
	{
		cmap = 0;
	}

	// reverse map the char names through MacRomanEncoding, then map the char codes through the cmap;
	// fall back on Unicode if that doesn't work
	std::string charName;
	if (useMacRoman)
	{
		for (int i = 0; i < 256; ++i)
		{
			if ((charName = enc[i]).size())
			{
				if ((code = globalParams->getMacRomanCharCode(charName)))
					map[i] = ff->mapCodeToGID(cmap, code);
				else if (unicodeCmap >= 0 && (u = globalParams->mapNameToUnicode(charName)))
					map[i] = ff->mapCodeToGID(unicodeCmap, u);
			}
			else if (unicodeCmap >= 0 && (n = ctu->mapToUnicode((CharCode)i, &u, 1)))
			{
				map[i] = ff->mapCodeToGID(cmap, u);
			}
			else
			{
				map[i] = -1;
			}
		}

		// map Unicode through the cmap
	}
	else if (useUnicode)
	{
		for (int i = 0; i < 256; ++i)
			if (((charName = enc[i]).size() && (u = globalParams->mapNameToUnicode(charName))) || (n = ctu->mapToUnicode((CharCode)i, &u, 1)))
				map[i] = ff->mapCodeToGID(cmap, u);
			else
				map[i] = -1;

		// map the char codes through the cmap, possibly with an offset of 0xf000
	}
	else
	{
		for (int i = 0; i < 256; ++i)
			if (!(map[i] = ff->mapCodeToGID(cmap, i)))
				map[i] = ff->mapCodeToGID(cmap, 0xf000 + i);
	}

	// try the TrueType 'post' table to handle any unmapped characters
	for (int i = 0; i < 256; ++i)
		if (map[i] <= 0 && (charName = enc[i]).size())
			map[i] = ff->mapNameToGID(charName);

	return map;
}

int* Gfx8BitFont::getCodeToGIDMap(FoFiType1C* ff)
{
	int* map = (int*)gmallocn(256, sizeof(int));
	for (int i = 0; i < 256; ++i)
		map[i] = 0;

	UMAP_STR_INT nameToGID = ff->getNameToGIDMap();
	for (int i = 0; i < 256; ++i)
	{
		if (enc[i].empty() || enc[i].front() == ':') continue;
		const int gid = FindDefault(nameToGID, enc[i], -1);
		if (gid < 0 || gid >= 65536) continue;
		map[i] = gid;
	}

	return map;
}

Dict* Gfx8BitFont::getCharProcs()
{
	return charProcs.isDict() ? charProcs.getDict() : (Dict*)nullptr;
}

Object* Gfx8BitFont::getCharProc(int code, Object* proc)
{
	if (enc[code].size() && charProcs.isDict())
		charProcs.dictLookup(enc[code].c_str(), proc);
	else
		proc->initNull();
	return proc;
}

Object* Gfx8BitFont::getCharProcNF(int code, Object* proc)
{
	if (enc[code].size() && charProcs.isDict())
		charProcs.dictLookupNF(enc[code].c_str(), proc);
	else
		proc->initNull();
	return proc;
}

Dict* Gfx8BitFont::getResources()
{
	return resources.isDict() ? resources.getDict() : (Dict*)nullptr;
}

bool Gfx8BitFont::problematicForUnicode()
{
	// potential inputs:
	// - font is embedded (GfxFont.embFontID.num >= 0)
	// - font name (GfxFont.name)
	// - font type (GfxFont.type)
	// - Base-14 font (Gfx8BitFont.base14 != nullptr)
	// - symbolic (GfxFont.flags & fontSymbolic)
	// - has Encoding array (Gfx8BitFont.hasEncoding)
	// - extracted base encoding from embedded font file (Gfx8BitFont.baseEncFromFontFile)
	// - has a ToUnicode map (GfxFont.hasToUnicode)
	// - used the numeric glyph name heuristic (Gfx8BitFont.usedNumericHeuristic)

	if (name.size())
	{
		const auto nameLC   = LowerCase(name);
		const bool symbolic = strstr(nameLC.c_str(), "dingbat") || strstr(nameLC.c_str(), "wingding") || strstr(nameLC.c_str(), "commpi");
		if (symbolic) return false;
	}

	if (embFontID.num >= 0)
	{
		switch (type)
		{
		case fontType1:
		case fontType1C:
		case fontType1COT:
			return !hasToUnicode && (!hasEncoding || usedNumericHeuristic);

		case fontType3:
			return !hasToUnicode && !hasEncoding;

		case fontTrueType:
		case fontTrueTypeOT:
			return !hasToUnicode && !hasEncoding;

		default:
			return !hasToUnicode;
		}
	}
	else
	{
		// NB: type will be fontTypeUnknown if the PDF specifies an invalid font type
		// -- which is ok, if we have a ToUnicode map or an encoding
		return !hasToUnicode && !hasEncoding;
	}
}

//------------------------------------------------------------------------
// GfxCIDFont
//------------------------------------------------------------------------

GfxCIDFont::GfxCIDFont(XRef* xref, std::string_view tagA, Ref idA, std::string_view nameA, GfxFontType typeA, Ref embFontIDA, Dict* fontDict)
    : GfxFont(tagA, idA, nameA, typeA, embFontIDA)
{
	Dict*              desFontDict;
	Object             desFontDictObj;
	Object             obj1, obj2, obj3, obj4, obj5, obj6;
	CharCodeToUnicode* utu;
	CharCode           c;
	Unicode            uBuf[8];
	int                c1, c2;
	int                excepsSize, i, j, k, n;

	missingWidth     = 0;
	ascent           = 0.95;
	descent          = -0.35;
	declaredAscent   = ascent;
	fontBBox[0]      = 0;
	fontBBox[1]      = 0;
	fontBBox[2]      = 0;
	fontBBox[3]      = 0;
	widths.defWidth  = 1.0;
	widths.defHeight = -1.0;
	widths.defVY     = 0.880;
	widths.exceps    = nullptr;
	widths.nExceps   = 0;
	widths.excepsV   = nullptr;
	widths.nExcepsV  = 0;

	// get the descendant font
	if (!fontDict->lookup("DescendantFonts", &obj1)->isArray() || obj1.arrayGetLength() == 0)
	{
		error(errSyntaxError, -1, "Missing or empty DescendantFonts entry in Type 0 font");
		obj1.free();
		goto err1;
	}
	if (!obj1.arrayGet(0, &desFontDictObj)->isDict())
	{
		error(errSyntaxError, -1, "Bad descendant font in Type 0 font");
		goto err2;
	}
	obj1.free();
	desFontDict = desFontDictObj.getDict();

	// get info from font descriptor
	readFontDescriptor(xref, desFontDict);

	//----- encoding info -----

	// char collection
	if (!desFontDict->lookup("CIDSystemInfo", &obj1)->isDict())
	{
		error(errSyntaxError, -1, "Missing CIDSystemInfo dictionary in Type 0 descendant font");
		goto err2;
	}
	obj1.dictLookup("Registry", &obj2);
	obj1.dictLookup("Ordering", &obj3);
	if (!obj2.isString() || !obj3.isString())
	{
		error(errSyntaxError, -1, "Invalid CIDSystemInfo dictionary in Type 0 descendant font");
		goto err3;
	}
	collection = fmt::format("{}-{}", obj2.getString(), obj3.getString());
	obj3.free();
	obj2.free();
	obj1.free();

	// encoding (i.e., CMap)
	if (fontDict->lookup("Encoding", &obj1)->isNull())
	{
		error(errSyntaxError, -1, "Missing Encoding entry in Type 0 font");
		goto err2;
	}
	if (!(cMap = CMap::parse(nullptr, collection, &obj1)))
		goto err2;

	// check for fonts that use the Identity-H encoding (cmap), and the Adobe-Identity character collection
	identityEnc = obj1.isName("Identity-H") && (collection == "Adobe-Identity");

	obj1.free();

	// CIDToGIDMap
	// (the PDF 1.7 spec only allows these for TrueType fonts, but Acrobat apparently also allows them for OpenType CFF fonts
	// -- and the PDF 2.0 spec has removed the prohibition)
	hasIdentityCIDToGID = false;
	desFontDict->lookup("CIDToGIDMap", &obj1);
	if (obj1.isStream())
	{
		cidToGIDLen = 0;
		i           = 64;
		cidToGID    = (int*)gmallocn(i, sizeof(int));
		obj1.streamReset();
		while ((c1 = obj1.streamGetChar()) != EOF && (c2 = obj1.streamGetChar()) != EOF)
		{
			if (cidToGIDLen == i)
			{
				i *= 2;
				cidToGID = (int*)greallocn(cidToGID, i, sizeof(int));
			}
			cidToGID[cidToGIDLen++] = (c1 << 8) + c2;
		}
		obj1.streamClose();
		identityEnc = false;
	}
	else if (obj1.isName("Identity"))
	{
		hasIdentityCIDToGID = true;
	}
	else if (!obj1.isNull())
	{
		error(errSyntaxError, -1, "Invalid CIDToGIDMap entry in CID font");
	}
	obj1.free();

	// look for a ToUnicode CMap
	hasKnownCollection = false;
	if (globalParams->getUseTrueTypeUnicodeMapping())
		readTrueTypeUnicodeMapping(xref);
	if (!ctu)
		ctu = readToUnicodeCMap(fontDict, 16, nullptr);
	if (!ctu)
	{
		ctuUsesCharCode = false;

		// use an identity mapping for the "Adobe-Identity" and "Adobe-UCS" collections
		if (collection == "Adobe-Identity" || collection == "Adobe-UCS")
		{
			ctu = CharCodeToUnicode::makeIdentityMapping();

			// look for a user-supplied .cidToUnicode file
		}
		else if ((ctu = globalParams->getCIDToUnicode(collection)))
		{
			hasKnownCollection = true;
		}
		else
		{
			error(errSyntaxError, -1, "Unknown character collection '{}'", collection);

			// fall back to an identity mapping
			ctu = CharCodeToUnicode::makeIdentityMapping();
		}
	}

	// look for a Unicode-to-Unicode mapping
	if (name.size() && (utu = globalParams->getUnicodeToUnicode(name)))
	{
		if (ctu)
		{
			if (ctu->isIdentity())
			{
				ctu->decRefCnt();
				ctu = utu;
			}
			else
			{
				for (c = 0; c < ctu->getLength(); ++c)
				{
					n = ctu->mapToUnicode(c, uBuf, 8);
					if (n >= 1)
					{
						n = utu->mapToUnicode((CharCode)uBuf[0], uBuf, 8);
						if (n >= 1) ctu->setMapping(c, uBuf, n);
					}
				}
				utu->decRefCnt();
			}
		}
		else
		{
			ctu = utu;
		}
	}

	//----- character metrics -----

	// default char width
	if (desFontDict->lookup("DW", &obj1)->isNum())
		widths.defWidth = obj1.getNum() * 0.001;
	obj1.free();

	// char width exceptions
	if (desFontDict->lookup("W", &obj1)->isArray())
	{
		excepsSize = 0;
		i          = 0;
		while (i + 1 < obj1.arrayGetLength())
		{
			obj1.arrayGet(i, &obj2);
			obj1.arrayGet(i + 1, &obj3);
			if (obj2.isInt() && obj3.isInt() && i + 2 < obj1.arrayGetLength())
			{
				if (obj1.arrayGet(i + 2, &obj4)->isNum())
				{
					if (widths.nExceps == excepsSize)
					{
						excepsSize += 16;
						widths.exceps = (GfxFontCIDWidthExcep*)greallocn(widths.exceps, excepsSize, sizeof(GfxFontCIDWidthExcep));
					}
					widths.exceps[widths.nExceps].first = obj2.getInt();
					widths.exceps[widths.nExceps].last  = obj3.getInt();
					widths.exceps[widths.nExceps].width = obj4.getNum() * 0.001;
					++widths.nExceps;
				}
				else
				{
					error(errSyntaxError, -1, "Bad widths array in Type 0 font");
				}
				obj4.free();
				i += 3;
			}
			else if (obj2.isInt() && obj3.isArray())
			{
				if (widths.nExceps + obj3.arrayGetLength() > excepsSize)
				{
					excepsSize    = (widths.nExceps + obj3.arrayGetLength() + 15) & ~15;
					widths.exceps = (GfxFontCIDWidthExcep*)greallocn(widths.exceps, excepsSize, sizeof(GfxFontCIDWidthExcep));
				}
				j = obj2.getInt();
				for (k = 0; k < obj3.arrayGetLength(); ++k)
				{
					if (obj3.arrayGet(k, &obj4)->isNum())
					{
						widths.exceps[widths.nExceps].first = j;
						widths.exceps[widths.nExceps].last  = j;
						widths.exceps[widths.nExceps].width = obj4.getNum() * 0.001;
						++j;
						++widths.nExceps;
					}
					else
					{
						error(errSyntaxError, -1, "Bad widths array in Type 0 font");
					}
					obj4.free();
				}
				i += 2;
			}
			else
			{
				error(errSyntaxError, -1, "Bad widths array in Type 0 font");
				++i;
			}
			obj3.free();
			obj2.free();
		}
	}
	obj1.free();

	// default metrics for vertical font
	if (desFontDict->lookup("DW2", &obj1)->isArray() && obj1.arrayGetLength() == 2)
	{
		if (obj1.arrayGet(0, &obj2)->isNum())
			widths.defVY = obj2.getNum() * 0.001;
		obj2.free();
		if (obj1.arrayGet(1, &obj2)->isNum())
			widths.defHeight = obj2.getNum() * 0.001;
		obj2.free();
	}
	obj1.free();

	// char metric exceptions for vertical font
	if (desFontDict->lookup("W2", &obj1)->isArray())
	{
		excepsSize = 0;
		i          = 0;
		while (i + 1 < obj1.arrayGetLength())
		{
			obj1.arrayGet(i, &obj2);
			obj1.arrayGet(i + 1, &obj3);
			if (obj2.isInt() && obj3.isInt() && i + 4 < obj1.arrayGetLength())
			{
				if (obj1.arrayGet(i + 2, &obj4)->isNum() && obj1.arrayGet(i + 3, &obj5)->isNum() && obj1.arrayGet(i + 4, &obj6)->isNum())
				{
					if (widths.nExcepsV == excepsSize)
					{
						excepsSize += 16;
						widths.excepsV = (GfxFontCIDWidthExcepV*)greallocn(widths.excepsV, excepsSize, sizeof(GfxFontCIDWidthExcepV));
					}
					widths.excepsV[widths.nExcepsV].first  = obj2.getInt();
					widths.excepsV[widths.nExcepsV].last   = obj3.getInt();
					widths.excepsV[widths.nExcepsV].height = obj4.getNum() * 0.001;
					widths.excepsV[widths.nExcepsV].vx     = obj5.getNum() * 0.001;
					widths.excepsV[widths.nExcepsV].vy     = obj6.getNum() * 0.001;
					++widths.nExcepsV;
				}
				else
				{
					error(errSyntaxError, -1, "Bad widths (W2) array in Type 0 font");
				}
				obj6.free();
				obj5.free();
				obj4.free();
				i += 5;
			}
			else if (obj2.isInt() && obj3.isArray())
			{
				if (widths.nExcepsV + obj3.arrayGetLength() / 3 > excepsSize)
				{
					excepsSize     = (widths.nExcepsV + obj3.arrayGetLength() / 3 + 15) & ~15;
					widths.excepsV = (GfxFontCIDWidthExcepV*)greallocn(widths.excepsV, excepsSize, sizeof(GfxFontCIDWidthExcepV));
				}
				j = obj2.getInt();
				for (k = 0; k + 2 < obj3.arrayGetLength(); k += 3)
				{
					if (obj3.arrayGet(k, &obj4)->isNum() && obj3.arrayGet(k + 1, &obj5)->isNum() && obj3.arrayGet(k + 2, &obj6)->isNum())
					{
						widths.excepsV[widths.nExcepsV].first  = j;
						widths.excepsV[widths.nExcepsV].last   = j;
						widths.excepsV[widths.nExcepsV].height = obj4.getNum() * 0.001;
						widths.excepsV[widths.nExcepsV].vx     = obj5.getNum() * 0.001;
						widths.excepsV[widths.nExcepsV].vy     = obj6.getNum() * 0.001;
						++j;
						++widths.nExcepsV;
					}
					else
					{
						error(errSyntaxError, -1, "Bad widths (W2) array in Type 0 font");
					}
					obj6.free();
					obj5.free();
					obj4.free();
				}
				i += 2;
			}
			else
			{
				error(errSyntaxError, -1, "Bad widths (W2) array in Type 0 font");
				++i;
			}
			obj3.free();
			obj2.free();
		}
	}
	obj1.free();

	desFontDictObj.free();
	ok = true;
	return;

err3:
	obj3.free();
	obj2.free();
err2:
	obj1.free();
	desFontDictObj.free();
err1:
	error(errSyntaxError, -1, "Failed to parse font object for '{}'", name);
}

GfxCIDFont::~GfxCIDFont()
{
	if (cMap) cMap->decRefCnt();
	if (ctu) ctu->decRefCnt();
	gfree(widths.exceps);
	gfree(widths.excepsV);
	if (cidToGID) gfree(cidToGID);
}

// Construct a code-to-Unicode mapping, based on the TrueType Unicode cmap (if present).
// Constructs ctu if succesful; leaves ctu = null otherwise.
// Always leaves ctu = null for non-TrueType fonts.
void GfxCIDFont::readTrueTypeUnicodeMapping(XRef* xref)
{
	char*         buf;
	FoFiTrueType* ff;
	Unicode *     gidToUnicode, *codeToUnicode;
	Unicode       u;
	int           bufLen, cmapPlatform, cmapEncoding, unicodeCmap;
	int           nGlyphs, nMappings, gid, i;

	// must be an embedded TrueType font, with an unknown char collection
	if ((type != fontCIDType2 && type == fontCIDType2OT) || embFontID.num < 0 || hasKnownCollection)
		goto err0;

	// read the embedded font and construct a FoFiTrueType
	if (!(buf = readEmbFontFile(xref, &bufLen)))
		goto err0;
	if (!(ff = FoFiTrueType::make(buf, bufLen, 0)))
		goto err1;

	// find the TrueType Unicode cmap
	unicodeCmap = -1;
	for (i = 0; i < ff->getNumCmaps(); ++i)
	{
		cmapPlatform = ff->getCmapPlatform(i);
		cmapEncoding = ff->getCmapEncoding(i);
		if ((cmapPlatform == 3 && cmapEncoding == 1) || (cmapPlatform == 0 && cmapEncoding <= 4))
		{
			unicodeCmap = i;
			break;
		}
	}
	if (unicodeCmap < 0)
		goto err2;

	// construct reverse GID-to-Unicode map
	nGlyphs      = ff->getNumGlyphs();
	gidToUnicode = (Unicode*)gmallocn(nGlyphs, sizeof(Unicode));
	memset(gidToUnicode, 0, nGlyphs * sizeof(Unicode));
	nMappings = 0;
	for (u = 1; u <= 0xffff; ++u)
	{
		gid = ff->mapCodeToGID(unicodeCmap, (int)u);
		if (gid > 0 && gid < nGlyphs)
		{
			gidToUnicode[gid] = u;
			++nMappings;
		}
	}
	// bail out if the Unicode cmap was completely empty
	if (nMappings == 0)
		goto err3;

	// construct code-to-Unicode map
	codeToUnicode = (Unicode*)gmallocn(65536, sizeof(Unicode));
	memset(codeToUnicode, 0, 65536 * sizeof(Unicode));
	for (i = 0; i <= 0xffff; ++i)
	{
		// we've already checked for an identity encoding, so CID = i
		if (cidToGID && i < cidToGIDLen)
			gid = cidToGID[i];
		else
			gid = i;
		if (gid < nGlyphs && gidToUnicode[gid] > 0)
			codeToUnicode[i] = gidToUnicode[gid];
	}
	ctu = CharCodeToUnicode::make16BitToUnicode(codeToUnicode);

	gfree(codeToUnicode);
err3:
	gfree(gidToUnicode);
err2:
	delete ff;
err1:
	gfree(buf);
err0:
	return;
}

int GfxCIDFont::getNextChar(const char* s, int len, CharCode* code, Unicode* u, int uSize, int* uLen, double* dx, double* dy, double* ox, double* oy)
{
	CID      cid;
	CharCode c;
	int      n;

	if (!cMap)
	{
		*code = 0;
		*uLen = 0;
		*dx = *dy = 0;
		return 1;
	}

	*code = (CharCode)(cid = cMap->getCID(s, len, &c, &n));
	if (ctu)
		*uLen = ctu->mapToUnicode(ctuUsesCharCode ? c : cid, u, uSize);
	else
		*uLen = 0;
	if (!*uLen && uSize >= 1 && globalParams->getMapUnknownCharNames())
	{
		u[0]  = *code;
		*uLen = 1;
	}

	// horizontal
	if (cMap->getWMode() == 0)
	{
		getHorizontalMetrics(cid, dx);
		*dy = *ox = *oy = 0;

		// vertical
	}
	else
	{
		getVerticalMetrics(cid, dy, ox, oy);
		*dx = 0;
	}

	return n;
}

// NB: Section 9.7.4.3 in the PDF 2.0 spec says that, in the case of duplicate entries in the metrics, the first entry should be used.
// This means we need to leave the metrics in the original order and perform a linear search.
// (Or use a more complex data structure.)
void GfxCIDFont::getHorizontalMetrics(CID cid, double* w)
{
	int i;
	for (i = 0; i < widths.nExceps; ++i)
	{
		if (widths.exceps[i].first <= cid && cid <= widths.exceps[i].last)
		{
			*w = widths.exceps[i].width;
			return;
		}
	}
	*w = widths.defWidth;
}

// NB: Section 9.7.4.3 in the PDF 2.0 spec says that, in the case of duplicate entries in the metrics, the first entry should be used.
// This means we need to leave the metrics in the original order and perform a linear search.
// (Or use a more complex data structure.)
void GfxCIDFont::getVerticalMetrics(CID cid, double* h, double* vx, double* vy)
{
	int i;
	for (i = 0; i < widths.nExcepsV; ++i)
	{
		if (widths.excepsV[i].first <= cid && cid <= widths.excepsV[i].last)
		{
			*h  = widths.excepsV[i].height;
			*vx = widths.excepsV[i].vx;
			*vy = widths.excepsV[i].vy;
			return;
		}
	}
	*h = widths.defHeight;
	getHorizontalMetrics(cid, vx);
	*vx /= 2;
	*vy = widths.defVY;
}

int GfxCIDFont::getWMode()
{
	return cMap ? cMap->getWMode() : 0;
}

CharCodeToUnicode* GfxCIDFont::getToUnicode()
{
	if (ctu)
		ctu->incRefCnt();
	return ctu;
}

std::string GfxCIDFont::getCollection()
{
	return cMap ? cMap->getCollection() : "";
}

double GfxCIDFont::getWidth(CID cid)
{
	double w;

	getHorizontalMetrics(cid, &w);
	return w;
}

bool GfxCIDFont::problematicForUnicode()
{
	// potential inputs:
	// - font is embedded (GfxFont.embFontID.num >= 0)
	// - font name (GfxFont.name)
	// - font type (GfxFont.type)
	// - symbolic (GfxFont.flags & fontSymbolic)
	// - has a ToUnicode map (GfxFont.hasToUnicode)
	// - collection is Adobe-Identity or Adobe-UCS (GfxCIDFont.collection - compare string)
	// - collection is known AdobeCJK (GfxCIDFont.hasKnownCollection)
	// - has non-Identity CIDToGIDMap (GfxCIDFont.cidToGID != nullptr)
	// - has Identity CIDToGIDMap (GfxCIDFont.hasIdentityCIDToGID)
	if (name.size())
	{
		const auto nameLC   = LowerCase(name);
		const bool symbolic = strstr(nameLC.c_str(), "dingbat") || strstr(nameLC.c_str(), "wingding") || strstr(nameLC.c_str(), "commpi");
		if (symbolic) return false;
	}

	if (embFontID.num >= 0)
	{
		switch (type)
		{
		case fontCIDType0:
		case fontCIDType0C:
		case fontCIDType0COT:
			return !hasToUnicode && !hasKnownCollection;

		case fontCIDType2:
		case fontCIDType2OT:
			return !hasToUnicode && !hasKnownCollection;

		default:
			return !hasToUnicode;
		}
	}
	else
	{
		return !hasToUnicode;
	}
}

//------------------------------------------------------------------------
// GfxFontDict
//------------------------------------------------------------------------

GfxFontDictEntry::GfxFontDictEntry(Ref refA, Object* fontObjA)
{
	ref = refA;
	fontObjA->copy(&fontObj);
}

GfxFontDictEntry::~GfxFontDictEntry()
{
	// NB: If the font has been loaded, font is non-nullptr and is owned by GfxFontDict.uniqueFonts.
	if (!loaded) fontObj.free();
}

void GfxFontDictEntry::load(GfxFont* fontA)
{
	loaded = true;
	font   = fontA;
	fontObj.free();
}

GfxFontDict::GfxFontDict(XRef* xrefA, Ref* fontDictRef, Dict* fontDict)
{
	xref        = xrefA;
	uniqueFonts = new GList();

	for (int i = 0; i < fontDict->getLength(); ++i)
	{
		char*  tag = fontDict->getKey(i);
		Object fontObj;
		fontDict->getValNF(i, &fontObj);
		Ref r;
		if (fontObj.isRef())
		{
			r = fontObj.getRef();
		}
		else if (fontDictRef)
		{
			// legal generation numbers are five digits, so we use a 6-digit number here
			r.gen = 100000 + fontDictRef->num;
			r.num = i;
		}
		else
		{
			// no indirect reference for this font, or for the containing font dict, so hash the font and use that
			r.gen = 100000;
			r.num = hashFontObject(&fontObj);
		}
		fonts.emplace(tag, GfxFontDictEntry(r, &fontObj));
		fontObj.free();
	}
}

GfxFontDict::~GfxFontDict()
{
	deleteGList(uniqueFonts, GfxFont);
}

GfxFont* GfxFontDict::lookup(const char* tag)
{
	if (const auto& it = fonts.find(tag); it != fonts.end())
	{
		auto& entry = it->second;
		load(tag, &entry);
		return entry.font;
	}
	return nullptr;
}

GfxFont* GfxFontDict::lookupByRef(Ref ref)
{
	for (auto& [tag, entry] : fonts)
	{
		if (entry.ref.num == ref.num && entry.ref.gen == ref.gen)
		{
			load(tag.c_str(), &entry);
			return entry.font;
		}
	}
	return nullptr;
}

int GfxFontDict::getNumFonts()
{
	loadAll();
	return uniqueFonts->getLength();
}

GfxFont* GfxFontDict::getFont(int i)
{
	return (GfxFont*)uniqueFonts->get(i);
}

void GfxFontDict::loadAll()
{
	for (auto& [tag, entry] : fonts)
		load(tag.c_str(), &entry);
}

void GfxFontDict::load(const char* tag, GfxFontDictEntry* entry)
{
	if (entry->loaded) return;

	// check for a duplicate that has already been loaded (don't do this for "synthetic" refs)
	if (entry->fontObj.isRef())
	{
		for (int i = 0; i < uniqueFonts->getLength(); ++i)
		{
			GfxFont* font = (GfxFont*)uniqueFonts->get(i);
			if (font->getID()->num == entry->ref.num && font->getID()->gen == entry->ref.gen)
			{
				entry->load(font);
				return;
			}
		}
	}

	GfxFont* font = nullptr;
	Object   obj;
	entry->fontObj.fetch(xref, &obj);
	if (obj.isDict())
	{
		font = GfxFont::makeFont(xref, tag, entry->ref, obj.getDict());
		if (font->isOk())
		{
			uniqueFonts->append(font);
		}
		else
		{
			delete font;
			font = nullptr;
		}
	}
	else
	{
		error(errSyntaxError, -1, "font resource is not a dictionary");
	}
	obj.free();
	entry->load(font);
}

// FNV-1a hash
class FNVHash
{
public:
	FNVHash()
	{
		h = 2166136261U;
	}

	void hash(char c)
	{
		h ^= c & 0xff;
		h *= 16777619;
	}

	void hash(const char* p, int n)
	{
		int i;
		for (i = 0; i < n; ++i)
			hash(p[i]);
	}

	int get31()
	{
		return (h ^ (h >> 31)) & 0x7fffffff;
	}

private:
	uint32_t h;
};

int GfxFontDict::hashFontObject(Object* obj)
{
	FNVHash h;

	hashFontObject1(obj, &h);
	return h.get31();
}

void GfxFontDict::hashFontObject1(Object* obj, FNVHash* h)
{
	Object      obj2;
	std::string s;
	char*       p;
	double      r;
	int         n, i;

	switch (obj->getType())
	{
	case objBool:
		h->hash('b');
		h->hash(obj->getBool() ? 1 : 0);
		break;
	case objInt:
		h->hash('i');
		n = obj->getInt();
		h->hash((char*)&n, sizeof(int));
		break;
	case objReal:
		h->hash('r');
		r = obj->getReal();
		h->hash((char*)&r, sizeof(double));
		break;
	case objString:
		h->hash('s');
		s = obj->getString();
		h->hash(s.c_str(), TO_INT(s.size()));
		break;
	case objName:
		h->hash('n');
		p = obj->getName();
		h->hash(p, (int)strlen(p));
		break;
	case objNull:
		h->hash('z');
		break;
	case objArray:
		h->hash('a');
		n = obj->arrayGetLength();
		h->hash((char*)&n, sizeof(int));
		for (i = 0; i < n; ++i)
		{
			obj->arrayGetNF(i, &obj2);
			hashFontObject1(&obj2, h);
			obj2.free();
		}
		break;
	case objDict:
		h->hash('d');
		n = obj->dictGetLength();
		h->hash((char*)&n, sizeof(int));
		for (i = 0; i < n; ++i)
		{
			p = obj->dictGetKey(i);
			h->hash(p, (int)strlen(p));
			obj->dictGetValNF(i, &obj2);
			hashFontObject1(&obj2, h);
			obj2.free();
		}
		break;
	case objStream:
		// this should never happen - streams must be indirect refs
		break;
	case objRef:
		h->hash('f');
		n = obj->getRefNum();
		h->hash((char*)&n, sizeof(int));
		n = obj->getRefGen();
		h->hash((char*)&n, sizeof(int));
		break;
	default:
		h->hash('u');
		break;
	}
}
