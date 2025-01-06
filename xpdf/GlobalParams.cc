//========================================================================
//
// GlobalParams.cc
//
// Copyright 2001-2003 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#ifdef _WIN32
#	include <shlobj.h>
#endif
#if HAVE_PAPER_H
#	include <paper.h>
#endif
#if HAVE_FONTCONFIG
#	include <fontconfig/fontconfig.h>
#endif
#include "gmem.h"
#include "gmempp.h"
#include "GList.h"
#include "gfile.h"
#include "FoFiIdentifier.h"
#include "Error.h"
#include "CharCodeToUnicode.h"
#include "UnicodeRemapping.h"
#include "UnicodeMap.h"
#include "CMap.h"
#include "BuiltinFontTables.h"
#include "FontEncodingTables.h"
#include "GlobalParams.h"

#ifdef _WIN32
#	ifndef __GNUC__
#		define strcasecmp  stricmp
#		define strncasecmp strnicmp
#	endif
#endif

#if MULTITHREADED
#	define lockGlobalParams      gLockMutex(&mutex)
#	define lockUnicodeMapCache   gLockMutex(&unicodeMapCacheMutex)
#	define lockCMapCache         gLockMutex(&cMapCacheMutex)
#	define unlockGlobalParams    gUnlockMutex(&mutex)
#	define unlockUnicodeMapCache gUnlockMutex(&unicodeMapCacheMutex)
#	define unlockCMapCache       gUnlockMutex(&cMapCacheMutex)
#else
#	define lockGlobalParams
#	define lockUnicodeMapCache
#	define lockCMapCache
#	define unlockGlobalParams
#	define unlockUnicodeMapCache
#	define unlockCMapCache
#endif

#include "NameToUnicodeTable.h"
#include "UnicodeMapTables.h"
#include "UTF8.h"

//------------------------------------------------------------------------

#define cidToUnicodeCacheSize     4
#define unicodeToUnicodeCacheSize 4

//------------------------------------------------------------------------

struct DisplayFontTab
{
	std::string name          = ""; //
	std::string t1FileName    = ""; //
	std::string ttFileName    = ""; //
	std::string macFileName   = ""; // may be .dfont, .ttf, or .ttc
	std::string macFontName   = ""; // font name inside .dfont or .ttc
	std::string obliqueFont   = ""; // name of font to oblique
	double      obliqueFactor = 0;  // oblique sheer factor
};

// clang-format off
std::vector<DisplayFontTab> displayFontTabs = {
	{ "Courier"              , "n022003l.pfb", "cour.ttf"   , "Courier"     , "Courier"               , ""              , 0        },
	{ "Courier-Bold"         , "n022004l.pfb", "courbd.ttf" , "Courier"     , "Courier Bold"          , ""              , 0        },
	{ "Courier-BoldOblique"  , "n022024l.pfb", "courbi.ttf" , "Courier"     , "Courier Bold Oblique"  , "Courier-Bold"  , 0.212557 },
	{ "Courier-Oblique"      , "n022023l.pfb", "couri.ttf"  , "Courier"     , "Courier Oblique"       , "Courier"       , 0.212557 },
	{ "Helvetica"            , "n019003l.pfb", "arial.ttf"  , "Helvetica"   , "Helvetica"             , ""              , 0        },
	{ "Helvetica-Bold"       , "n019004l.pfb", "arialbd.ttf", "Helvetica"   , "Helvetica Bold"        , ""              , 0        },
	{ "Helvetica-BoldOblique", "n019024l.pfb", "arialbi.ttf", "Helvetica"   , "Helvetica Bold Oblique", "Helvetica-Bold", 0.212557 },
	{ "Helvetica-Oblique"    , "n019023l.pfb", "ariali.ttf" , "Helvetica"   , "Helvetica Oblique"     , "Helvetica"     , 0.212557 },
	{ "Symbol"               , "s050000l.pfb", ""           , "Symbol"      , "Symbol"                , ""              , 0        },
	{ "Times-Bold"           , "n021004l.pfb", "timesbd.ttf", "Times"       , "Times Bold"            , ""              , 0        },
	{ "Times-BoldItalic"     , "n021024l.pfb", "timesbi.ttf", "Times"       , "Times Bold Italic"     , ""              , 0        },
	{ "Times-Italic"         , "n021023l.pfb", "timesi.ttf" , "Times"       , "Times Italic"          , ""              , 0        },
	{ "Times-Roman"          , "n021003l.pfb", "times.ttf"  , "Times"       , "Times Roman"           , ""              , 0        },
	{ "ZapfDingbats"         , "d050000l.pfb", ""           , "ZapfDingbats", "Zapf Dingbats"         , ""              , 0        },
};
// clang-format on

static const char* displayFontDirs[] = {
#ifdef BASE14_FONT_DIR
	BASE14_FONT_DIR,
#endif
#ifdef _WIN32
	"c:/windows/fonts",
	"c:/winnt/fonts",
#else // _WIN32
	"/usr/share/ghostscript/fonts",
	"/usr/local/share/ghostscript/fonts",
	"/usr/share/fonts/default/Type1",
	"/usr/share/fonts/default/ghostscript",
	"/usr/share/fonts/type1/gsfonts",
#	if defined(__sun) && defined(__SVR4)
	"/usr/sfw/share/ghostscript/fonts",
#	endif
#endif // _WIN32
	nullptr
};

#ifdef __APPLE__
static const char* macSystemFontPath = "/System/Library/Fonts";
#endif

//------------------------------------------------------------------------

std::shared_ptr<GlobalParams> globalParams;

std::string GlobalParams::defaultTextEncoding = "Latin1";

//------------------------------------------------------------------------
// PSFontParam16
//------------------------------------------------------------------------

PSFontParam16::PSFontParam16(const std::string& nameA, int wModeA, const std::string& psFontNameA, const std::string& encodingA)
{
	name       = nameA;
	wMode      = wModeA;
	psFontName = psFontNameA;
	encoding   = encodingA;
}

PSFontParam16::~PSFontParam16()
{
}

//------------------------------------------------------------------------
// SysFontInfo
//------------------------------------------------------------------------

class SysFontInfo
{
public:
	std::string name;
	std::string path;
	SysFontType type;
	int         fontNum; // for TrueType collections

	SysFontInfo(const std::string& nameA, const std::string& pathA, SysFontType typeA, int fontNumA);
	~SysFontInfo();
	std::string mungeName1(const std::string& in);
	std::string mungeName2(const std::string& in);
	void        mungeName3(std::string& nameA, bool* bold, bool* italic);
	int         match(const std::string& nameA);
};

SysFontInfo::SysFontInfo(const std::string& nameA, const std::string& pathA, SysFontType typeA, int fontNumA)
{
	name    = nameA;
	path    = pathA;
	type    = typeA;
	fontNum = fontNumA;
}

SysFontInfo::~SysFontInfo()
{
}

// Remove space/comma/dash/underscore chars.
// Uppercase the name.
std::string SysFontInfo::mungeName1(const std::string& in)
{
	std::string out;
	for (char* p = (char*)in.c_str(); *p; ++p)
	{
		if (*p == ' ' || *p == ',' || *p == '-' || *p == '_')
		{
			// skip
		}
		else if (*p >= 'a' && *p <= 'z')
		{
			out += (char)(*p & 0xdf);
		}
		else
		{
			out += *p;
		}
	}
	return out;
}

// Remove trailing encoding tags from the name.
// Split the name into tokens at space/comma/dash/underscore.
// Remove trailing "MT" or "BT" from tokens.
// Remove trailing "PS" and "WGL4" from tokens.
// Uppercase each token.
// Concatenate tokens (dropping the space/comma/dash chars).
std::string SysFontInfo::mungeName2(const std::string& in)
{
	std::string out;
	char*       p0 = (char*)in.c_str();
	while (*p0)
	{
		if (!strcmp(p0, "Identity-H") || !strcmp(p0, "Identity-V") || !strcmp(p0, "GB2312") || !strcmp(p0, "UniGB-UCS2-H") || !strcmp(p0, "UniGB-UCS2-V"))
			break;
		char* p1;
		for (p1 = p0 + 1; *p1 && *p1 != ' ' && *p1 != ',' && *p1 != '-' && *p1 != '_'; ++p1)
			;
		char* p2 = p1;
		if (p2 - p0 >= 2 && (p2[-2] == 'B' || p2[-2] == 'M') && p2[-1] == 'T')
			p2 -= 2;
		if (p2 - p0 >= 2 && p2[-2] == 'P' && p2[-1] == 'S')
			p2 -= 2;
		if (p2 - p0 >= 4 && p2[-4] == 'W' && p2[-3] == 'G' && p2[-2] == 'L' && p2[-1] == '4')
			p2 -= 4;
		for (; p0 < p2; ++p0)
			if (*p0 >= 'a' && *p0 <= 'z')
				out += (char)(*p0 & 0xdf);
			else
				out += *p0;
		for (p0 = p1; *p0 == ' ' || *p0 == ',' || *p0 == '-' || *p0 == '_'; ++p0)
			;
	}
	return out;
}

// Remove trailing bold/italic/regular/roman tags from the name.
// (Note: the names have already been uppercased by mungeName1/2.)
void SysFontInfo::mungeName3(std::string& nameA, bool* bold, bool* italic)
{
	*bold   = false;
	*italic = false;
	int n   = TO_INT(nameA.size());
	while (1)
	{
		if (n >= 4 && !strcmp(nameA.c_str() + n - 4, "BOLD"))
		{
			nameA.erase(n - 4, 4);
			n -= 4;
			*bold = true;
		}
		else if (n >= 6 && !strcmp(nameA.c_str() + n - 6, "ITALIC"))
		{
			nameA.erase(n - 6, 6);
			n -= 6;
			*italic = true;
		}
		else if (n >= 7 && !strcmp(nameA.c_str() + n - 7, "REGULAR"))
		{
			nameA.erase(n - 7, 7);
			n -= 7;
		}
		else if (n >= 5 && !strcmp(nameA.c_str() + n - 5, "ROMAN"))
		{
			nameA.erase(n - 5, 5);
			n -= 5;
		}
		else
		{
			break;
		}
	}
}

// Returns a score indicating how well this font matches [nameA].  A
// higher score is better.  Zero indicates a non-match.
int SysFontInfo::match(const std::string& nameA)
{
	// fast fail: check if the first two letters match
	if (strncasecmp(name.c_str(), nameA.c_str(), 2))
		return 0;

	std::string pdfName1 = mungeName1(nameA);
	std::string sysName1 = mungeName1(name);
	if (pdfName1 == sysName1)
		return 8;

	std::string pdfName2 = mungeName2(nameA);
	std::string sysName2 = mungeName2(name);
	if (pdfName2 == sysName2)
		return 7;

	bool pdfBold1, pdfItalic1, sysBold1, sysItalic1;
	mungeName3(pdfName1, &pdfBold1, &pdfItalic1);
	mungeName3(sysName1, &sysBold1, &sysItalic1);
	int eq1 = (pdfName1 == sysName1);

	bool pdfBold2, pdfItalic2, sysBold2, sysItalic2;
	mungeName3(pdfName2, &pdfBold2, &pdfItalic2);
	mungeName3(sysName2, &sysBold2, &sysItalic2);
	int eq2 = (pdfName2 == sysName2);

	if (eq1 && pdfBold1 == sysBold1 && pdfItalic1 == sysItalic1) return 6;
	if (eq2 && pdfBold2 == sysBold2 && pdfItalic2 == sysItalic2) return 5;
	if (eq1 && pdfItalic1 == sysItalic1) return 4;
	if (eq2 && pdfItalic2 == sysItalic2) return 3;
	if (eq1) return 2;
	if (eq2) return 1;

	return 0;
}

//------------------------------------------------------------------------
// SysFontList
//------------------------------------------------------------------------

class SysFontList
{
public:
	SysFontList();
	~SysFontList();
	SysFontInfo* find(const std::string& name);

#ifdef _WIN32
	void scanWindowsFonts(char* winFontDir);
#endif

#if HAVE_FONTCONFIG
	void scanFontconfigFonts();
#endif

private:
#ifdef _WIN32
	SysFontInfo* makeWindowsFont(const std::string& name, int fontNum, const char* path);
#endif

	GList* fonts; // [SysFontInfo]
};

SysFontList::SysFontList()
{
	fonts = new GList();
}

SysFontList::~SysFontList()
{
	deleteGList(fonts, SysFontInfo);
}

SysFontInfo* SysFontList::find(const std::string& name)
{
	SysFontInfo* match = nullptr;
	int          score = 0;
	for (int i = 0; i < fonts->getLength(); ++i)
	{
		SysFontInfo* fi = (SysFontInfo*)fonts->get(i);
		int          s  = fi->match(name);
		if (s > score)
		{
			match = fi;
			score = s;
		}
	}
	return match;
}

#ifdef _WIN32
void SysFontList::scanWindowsFonts(char* winFontDir)
{
	OSVERSIONINFO version;
	const char*   path;
	DWORD         idx, valNameLen, dataLen, type;
	HKEY          regKey;
	char          valName[1024], data[1024];
	int           n, fontNum;
	char *        p0, *p1;

	version.dwOSVersionInfoSize = sizeof(version);
	GetVersionEx(&version);
	if (version.dwPlatformId == VER_PLATFORM_WIN32_NT)
		path = "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts\\";
	else
		path = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Fonts\\";
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0, KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS, &regKey) == ERROR_SUCCESS)
	{
		idx = 0;
		while (1)
		{
			valNameLen = sizeof(valName) - 1;
			dataLen    = sizeof(data) - 1;
			if (RegEnumValueA(regKey, idx, valName, &valNameLen, nullptr, &type, (LPBYTE)data, &dataLen) != ERROR_SUCCESS)
				break;
			if (type == REG_SZ && valNameLen > 0 && valNameLen < sizeof(valName) && dataLen > 0 && dataLen < sizeof(data))
			{
				valName[valNameLen] = '\0';
				data[dataLen]       = '\0';
				n                   = (int)strlen(data);
				if (!strcasecmp(data + n - 4, ".ttf") || !strcasecmp(data + n - 4, ".ttc") || !strcasecmp(data + n - 4, ".otf"))
				{
					std::string fontPath;
					if (!(dataLen >= 3 && data[1] == ':' && data[2] == '\\'))
					{
						fontPath.insert(0, "\\");
						fontPath.insert(0, winFontDir);
					}
					p0      = valName;
					fontNum = 0;
					while (*p0)
					{
						p1 = strstr(p0, " & ");
						if (p1)
						{
							*p1 = '\0';
							p1  = p1 + 3;
						}
						else
						{
							p1 = p0 + strlen(p0);
						}
						fonts->append(makeWindowsFont(p0, fontNum, fontPath.c_str()));
						p0 = p1;
						++fontNum;
					}
				}
			}
			++idx;
		}
		RegCloseKey(regKey);
	}
}

SysFontInfo* SysFontList::makeWindowsFont(const std::string& name, int fontNum, const char* path)
{
	int n = TO_INT(name.size());

	// remove trailing ' (TrueType)' or ' (OpenType)'
	std::string clean = name;
	if (clean.ends_with(" (TrueType") || clean.ends_with(" (OpenType)"))
		clean.erase(n - 11);

	SysFontType type;
	if (!strcasecmp(path + strlen(path) - 4, ".ttc"))
		type = sysFontTTC;
	else if (!strcasecmp(path + strlen(path) - 4, ".otf"))
		type = sysFontOTF;
	else
		type = sysFontTTF;

	return new SysFontInfo(clean, path, type, fontNum);
}
#endif // _WIN32

#if HAVE_FONTCONFIG
void SysFontList::scanFontconfigFonts()
{
	FcConfig* cfg;
	if (!(cfg = FcInitLoadConfigAndFonts()))
		return;

	FcPattern*   pattern = FcPatternBuild(nullptr, FC_OUTLINE, FcTypeBool, FcTrue, FC_SCALABLE, FcTypeBool, FcTrue, nullptr);
	FcObjectSet* objSet  = FcObjectSetBuild(FC_FULLNAME, FC_FILE, FC_INDEX, nullptr);
	FcFontSet*   fontSet = FcFontList(cfg, pattern, objSet);
	FcPatternDestroy(pattern);
	FcObjectSetDestroy(objSet);

	if (fontSet)
	{
		for (int i = 0; i < fontSet->nfont; ++i)
		{
			//--- font file, font type
			char* file;
			if (FcPatternGetString(fontSet->fonts[i], FC_FILE, 0, (FcChar8**)&file) != FcResultMatch)
				continue;
			const int n = (int)strlen(file);
			SysFontType  type;
			if (n > 4 && !strcasecmp(file + n - 4, ".pfa"))
				type = sysFontPFA;
			else if (n > 4 && !strcasecmp(file + n - 4, ".pfb"))
				type = sysFontPFB;
			else if (n > 4 && !strcasecmp(file + n - 4, ".ttf"))
				type = sysFontTTF;
			else if (n > 4 && !strcasecmp(file + n - 4, ".otf"))
				type = sysFontOTF;
			else
				continue;

			//--- font number
			int fontNum;
			if (FcPatternGetInteger(fontSet->fonts[i], FC_INDEX, 0, &fontNum) != FcResultMatch)
				fontNum = 0;

			//--- font name
			char *name;
			if (FcPatternGetString(fontSet->fonts[i], FC_FULLNAME, 0, (FcChar8**)&name) != FcResultMatch)
				continue;

			fonts->append(new SysFontInfo(std::string(name), new GString(file), type, fontNum));
		}

		FcFontSetDestroy(fontSet);
	}

	FcConfigDestroy(cfg);
}
#endif // HAVE_FONTCONFIG

//------------------------------------------------------------------------
// KeyBinding
//------------------------------------------------------------------------

KeyBinding::KeyBinding(int codeA, int modsA, int contextA, const char* cmd0)
{
	code    = codeA;
	mods    = modsA;
	context = contextA;
	cmds.clear();
	cmds.push_back(cmd0);
}

KeyBinding::KeyBinding(int codeA, int modsA, int contextA, const char* cmd0, const char* cmd1)
{
	code    = codeA;
	mods    = modsA;
	context = contextA;
	cmds.clear();
	cmds.push_back(cmd0);
	cmds.push_back(cmd1);
}

KeyBinding::KeyBinding(int codeA, int modsA, int contextA, const VEC_STR& cmdsA)
{
	code    = codeA;
	mods    = modsA;
	context = contextA;
	cmds    = cmdsA;
}

KeyBinding::~KeyBinding()
{
}

//------------------------------------------------------------------------
// PopupMenuCmd
//------------------------------------------------------------------------

PopupMenuCmd::PopupMenuCmd(const std::string& labelA, const VEC_STR& cmdsA)
{
	label = labelA;
	cmds  = cmdsA;
}

PopupMenuCmd::~PopupMenuCmd()
{
}

//------------------------------------------------------------------------
// parsing
//------------------------------------------------------------------------

GlobalParams::GlobalParams(const char* cfgFileName)
{
#if MULTITHREADED
	gInitMutex(&mutex);
	gInitMutex(&unicodeMapCacheMutex);
	gInitMutex(&cMapCacheMutex);
#endif

#ifdef _WIN32
	tlsWin32ErrorInfo = TlsAlloc();
#endif

	// scan the encoding in reverse because we want the lowest-numbered index for each char name ('space' is encoded twice)
	for (int i = 255; i >= 0; --i)
	{
		if (const auto& text = macRomanEncoding[i]; text.size() && text.front() != ':')
			macRomanReverseMap.emplace(text, i);
	}

#ifdef _WIN32
	// baseDir will be set by a call to setBaseDir
	baseDir.clear();
#else
	baseDir = appendToPath(getHomeDir(), ".xpdf");
#endif
	setDataDirVar();
	unicodeRemapping = new UnicodeRemapping();
	sysFonts         = new SysFontList();
#if HAVE_PAPER_H
	const char*         paperName;
	const struct paper* paperType;
	paperinit();
	if ((paperName = systempapername()))
	{
		paperType     = paperinfo(paperName);
		psPaperWidth  = (int)paperpswidth(paperType);
		psPaperHeight = (int)paperpsheight(paperType);
	}
	else
	{
		error(errConfig, -1, "No paper information available - using defaults");
	}
	paperdone();
#endif
	psImageableLLX = psImageableLLY = 0;
	psImageableURX.store(psPaperWidth.load());
	psImageableURY.store(psPaperHeight.load());
#if defined(_WIN32)
	textEOL = eolDOS;
#else
	textEOL = eolUnix;
#endif
	zoomValues.push_back("25");
	zoomValues.push_back("50");
	zoomValues.push_back("75");
	zoomValues.push_back("100");
	zoomValues.push_back("110");
	zoomValues.push_back("125");
	zoomValues.push_back("150");
	zoomValues.push_back("175");
	zoomValues.push_back("200");
	zoomValues.push_back("300");
	zoomValues.push_back("400");
	zoomValues.push_back("600");
	zoomValues.push_back("800");
	createDefaultKeyBindings();
	initStateFilePaths();

	cidToUnicodeCache     = new CharCodeToUnicodeCache(cidToUnicodeCacheSize);
	unicodeToUnicodeCache = new CharCodeToUnicodeCache(unicodeToUnicodeCacheSize);
	unicodeMapCache       = new UnicodeMapCache();
	cMapCache             = new CMapCache();

	// set up the initial nameToUnicode table
	for (const auto& item : nameToUnicodeTab)
		nameToUnicode.emplace(item.name, item.value);

	// set up the residentUnicodeMaps table
	// clang-format off
	residentUnicodeMaps.emplace("Latin1"      , UnicodeMap("Latin1"      , false, latin1UnicodeMapRanges, latin1UnicodeMapLen));
	residentUnicodeMaps.emplace("ASCII7"      , UnicodeMap("ASCII7"      , false, ascii7UnicodeMapRanges, ascii7UnicodeMapLen));
	residentUnicodeMaps.emplace("Symbol"      , UnicodeMap("Symbol"      , false, symbolUnicodeMapRanges, symbolUnicodeMapLen));
	residentUnicodeMaps.emplace("ZapfDingbats", UnicodeMap("ZapfDingbats", false, zapfDingbatsUnicodeMapRanges, zapfDingbatsUnicodeMapLen));
	residentUnicodeMaps.emplace("UTF-8"       , UnicodeMap("UTF-8"       , true, &mapUTF8));
	residentUnicodeMaps.emplace("UCS-2"       , UnicodeMap("UCS-2"       , true, &mapUCS2));
	// clang-format on

	// look for a user config file, then a system-wide config file
	FILE*       f = nullptr;
	std::string fileName;
	if (cfgFileName && cfgFileName[0])
	{
		fileName = cfgFileName;
		if (!(f = fopen(fileName.c_str(), "r")))
		{
		}
	}
	if (!f)
	{
		fileName = appendToPath(getHomeDir(), xpdfUserConfigFile);
		if (!(f = fopen(fileName.c_str(), "r")))
		{
		}
	}
	if (!f)
	{
#ifdef _WIN32
		char      buf[512];
		const int i = GetModuleFileNameA(nullptr, buf, sizeof(buf));
		if (i <= 0 || i >= sizeof(buf))
		{
			// error or path too long for buffer - just use the current dir
			buf[0] = '\0';
		}
		fileName = grabPath(buf);
		appendToPath(fileName, xpdfSysConfigFile);
#else
		fileName = xpdfSysConfigFile;
#endif
		if (!(f = fopen(fileName.c_str(), "r")))
		{
		}
	}
	if (f)
	{
		parseFile(fileName, f);
		fclose(f);
	}
}

void GlobalParams::setDataDirVar()
{
	std::string dir;
#if defined(XPDFRC_DATADIR)
	dir = XPDFRC_DATADIR;
#else
	//~ may be useful to allow the options of using the install dir
	//~   and/or the user's home dir (?)
	dir = "./data";
#endif
	configFileVars.emplace("DATADIR", dir);
}

void GlobalParams::createDefaultKeyBindings()
{
	//----- mouse buttons
	keyBindings.emplace_back(xpdfKeyCodeMousePress1, xpdfKeyModNone, xpdfKeyContextAny, "startSelection");
	keyBindings.emplace_back(xpdfKeyCodeMousePress1, xpdfKeyModShift, xpdfKeyContextAny, "startExtendedSelection");
	keyBindings.emplace_back(xpdfKeyCodeMouseRelease1, xpdfKeyModNone, xpdfKeyContextAny, "endSelection");
	keyBindings.emplace_back(xpdfKeyCodeMouseRelease1, xpdfKeyModShift, xpdfKeyContextAny, "endSelection");
	keyBindings.emplace_back(xpdfKeyCodeMouseDoubleClick1, xpdfKeyModNone, xpdfKeyContextAny, "selectWord");
	keyBindings.emplace_back(xpdfKeyCodeMouseTripleClick1, xpdfKeyModNone, xpdfKeyContextAny, "selectLine");
	keyBindings.emplace_back(xpdfKeyCodeMouseClick1, xpdfKeyModNone, xpdfKeyContextAny, "followLinkNoSel");
	keyBindings.emplace_back(xpdfKeyCodeMouseClick2, xpdfKeyModNone, xpdfKeyContextOverLink, "followLinkInNewTab");
	keyBindings.emplace_back(xpdfKeyCodeMousePress2, xpdfKeyModNone, xpdfKeyContextAny, "startPan");
	keyBindings.emplace_back(xpdfKeyCodeMouseRelease2, xpdfKeyModNone, xpdfKeyContextAny, "endPan");
	keyBindings.emplace_back(xpdfKeyCodeMousePress3, xpdfKeyModNone, xpdfKeyContextAny, "postPopupMenu");
	keyBindings.emplace_back(xpdfKeyCodeMousePress4, xpdfKeyModNone, xpdfKeyContextAny, "scrollUpPrevPage(16)");
	keyBindings.emplace_back(xpdfKeyCodeMousePress5, xpdfKeyModNone, xpdfKeyContextAny, "scrollDownNextPage(16)");
	keyBindings.emplace_back(xpdfKeyCodeMousePress6, xpdfKeyModNone, xpdfKeyContextAny, "scrollLeft(16)");
	keyBindings.emplace_back(xpdfKeyCodeMousePress7, xpdfKeyModNone, xpdfKeyContextAny, "scrollRight(16)");
	keyBindings.emplace_back(xpdfKeyCodeMousePress4, xpdfKeyModCtrl, xpdfKeyContextAny, "zoomIn");
	keyBindings.emplace_back(xpdfKeyCodeMousePress5, xpdfKeyModCtrl, xpdfKeyContextAny, "zoomOut");

	//----- control keys
	keyBindings.emplace_back('o', xpdfKeyModCtrl, xpdfKeyContextAny, "open");
	keyBindings.emplace_back('r', xpdfKeyModCtrl, xpdfKeyContextAny, "reload");
	keyBindings.emplace_back('f', xpdfKeyModCtrl, xpdfKeyContextAny, "find");
	keyBindings.emplace_back('g', xpdfKeyModCtrl, xpdfKeyContextAny, "findNext");
	keyBindings.emplace_back('c', xpdfKeyModCtrl, xpdfKeyContextAny, "copy");
	keyBindings.emplace_back('p', xpdfKeyModCtrl, xpdfKeyContextAny, "print");
	keyBindings.emplace_back('0', xpdfKeyModCtrl, xpdfKeyContextAny, "zoomPercent(125)");
	keyBindings.emplace_back('+', xpdfKeyModCtrl, xpdfKeyContextAny, "zoomIn");
	keyBindings.emplace_back('=', xpdfKeyModCtrl, xpdfKeyContextAny, "zoomIn");
	keyBindings.emplace_back('-', xpdfKeyModCtrl, xpdfKeyContextAny, "zoomOut");
	keyBindings.emplace_back('s', xpdfKeyModCtrl, xpdfKeyContextAny, "saveAs");
	keyBindings.emplace_back('t', xpdfKeyModCtrl, xpdfKeyContextAny, "newTab");
	keyBindings.emplace_back('n', xpdfKeyModCtrl, xpdfKeyContextAny, "newWindow");
	keyBindings.emplace_back('w', xpdfKeyModCtrl, xpdfKeyContextAny, "closeTabOrQuit");
	keyBindings.emplace_back('l', xpdfKeyModCtrl, xpdfKeyContextAny, "toggleFullScreenMode");
	keyBindings.emplace_back('q', xpdfKeyModCtrl, xpdfKeyContextAny, "quit");
	keyBindings.emplace_back(xpdfKeyCodeTab, xpdfKeyModCtrl, xpdfKeyContextAny, "nextTab");
	keyBindings.emplace_back(xpdfKeyCodeTab, xpdfKeyModShift | xpdfKeyModCtrl, xpdfKeyContextAny, "prevTab");
	keyBindings.emplace_back('?', xpdfKeyModCtrl, xpdfKeyContextAny, "help");

	//----- alt keys
	keyBindings.emplace_back(xpdfKeyCodeLeft, xpdfKeyModAlt, xpdfKeyContextAny, "goBackward");
	keyBindings.emplace_back(xpdfKeyCodeRight, xpdfKeyModAlt, xpdfKeyContextAny, "goForward");

	//----- home/end keys
	keyBindings.emplace_back(xpdfKeyCodeHome, xpdfKeyModCtrl, xpdfKeyContextAny, "gotoPage(1)");
	keyBindings.emplace_back(xpdfKeyCodeHome, xpdfKeyModNone, xpdfKeyContextAny, "scrollToTopLeft");
	keyBindings.emplace_back(xpdfKeyCodeEnd, xpdfKeyModCtrl, xpdfKeyContextAny, "gotoLastPage");
	keyBindings.emplace_back(xpdfKeyCodeEnd, xpdfKeyModNone, xpdfKeyContextAny, "scrollToBottomRight");

	//----- pgup/pgdn keys
	keyBindings.emplace_back(xpdfKeyCodePgUp, xpdfKeyModNone, xpdfKeyContextAny, "pageUp");
	keyBindings.emplace_back(xpdfKeyCodePgDn, xpdfKeyModNone, xpdfKeyContextAny, "pageDown");
	keyBindings.emplace_back(xpdfKeyCodePgUp, xpdfKeyModCtrl, xpdfKeyContextAny, "prevPage");
	keyBindings.emplace_back(xpdfKeyCodePgDn, xpdfKeyModCtrl, xpdfKeyContextAny, "nextPage");
	keyBindings.emplace_back(xpdfKeyCodePgUp, xpdfKeyModCtrl, xpdfKeyContextScrLockOn, "prevPageNoScroll");
	keyBindings.emplace_back(xpdfKeyCodePgDn, xpdfKeyModCtrl, xpdfKeyContextScrLockOn, "nextPageNoScroll");

	//----- esc key
	keyBindings.emplace_back(xpdfKeyCodeEsc, xpdfKeyModNone, xpdfKeyContextFullScreen, "windowMode");

	//----- arrow keys
	keyBindings.emplace_back(xpdfKeyCodeLeft, xpdfKeyModNone, xpdfKeyContextAny, "scrollLeft(16)");
	keyBindings.emplace_back(xpdfKeyCodeRight, xpdfKeyModNone, xpdfKeyContextAny, "scrollRight(16)");
	keyBindings.emplace_back(xpdfKeyCodeUp, xpdfKeyModNone, xpdfKeyContextAny, "scrollUp(16)");
	keyBindings.emplace_back(xpdfKeyCodeDown, xpdfKeyModNone, xpdfKeyContextAny, "scrollDown(16)");
	keyBindings.emplace_back(xpdfKeyCodeUp, xpdfKeyModCtrl, xpdfKeyContextAny, "prevPage");
	keyBindings.emplace_back(xpdfKeyCodeDown, xpdfKeyModCtrl, xpdfKeyContextAny, "nextPage");
	keyBindings.emplace_back(xpdfKeyCodeUp, xpdfKeyModCtrl, xpdfKeyContextScrLockOn, "prevPageNoScroll");
	keyBindings.emplace_back(xpdfKeyCodeDown, xpdfKeyModCtrl, xpdfKeyContextScrLockOn, "nextPageNoScroll");

	//----- letter keys
	keyBindings.emplace_back(' ', xpdfKeyModNone, xpdfKeyContextAny, "pageDown");
	keyBindings.emplace_back('g', xpdfKeyModNone, xpdfKeyContextAny, "focusToPageNum");
	keyBindings.emplace_back('z', xpdfKeyModNone, xpdfKeyContextAny, "zoomFitPage");
	keyBindings.emplace_back('w', xpdfKeyModNone, xpdfKeyContextAny, "zoomFitWidth");
}

void GlobalParams::initStateFilePaths()
{
#ifdef _WIN32
	char path[MAX_PATH];
	if (SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path) != S_OK)
		return;
	const auto dir = appendToPath(path, "xpdf");
	CreateDirectoryA(dir.c_str(), nullptr);
	pagesFile    = appendToPath(dir, "xpdf.pages");
	tabStateFile = appendToPath(dir, "xpdf.tab-state");
	sessionFile  = appendToPath(dir, "xpdf.session");
#else
	pagesFile    = appendToPath(getHomeDir(), ".xpdf.pages");
	tabStateFile = appendToPath(getHomeDir(), ".xpdf.tab-state");
	sessionFile  = appendToPath(getHomeDir(), ".xpdf.session");
#endif
}

void GlobalParams::parseFile(const std::string& fileName, FILE* f)
{
	int  line;
	char buf[512];

	line = 1;
	while (getLine(buf, sizeof(buf) - 1, f))
	{
		parseLine(buf, fileName, line);
		++line;
	}
}

void GlobalParams::parseLine(char* buf, const std::string& fileName, int line)
{
	// break the line into tokens
	const auto tokens = parseLineTokens(buf, fileName, line);

	// parse the line
	if (tokens.size() > 0 && (tokens.at(0)).at(0) != '#')
	{
		const auto cmd = tokens.at(0);
		if (cmd == "include")
		{
			if (tokens.size() == 2)
			{
				const auto incFile = tokens.at(1);
				FILE*      f2;
				if ((f2 = openFile(incFile.c_str(), "r")))
				{
					parseFile(incFile, f2);
					fclose(f2);
				}
				else
				{
					error(errConfig, -1, "Couldn't find included config file: '{}' ({}:{})", incFile, fileName, line);
				}
			}
			else
			{
				error(errConfig, -1, "Bad 'include' config file command ({}:{})", fileName, line);
			}
		}
		else if (cmd == "nameToUnicode")
		{
			parseNameToUnicode(tokens, fileName, line);
		}
		else if (cmd == "cidToUnicode")
		{
			parseCIDToUnicode(tokens, fileName, line);
		}
		else if (cmd == "unicodeToUnicode")
		{
			parseUnicodeToUnicode(tokens, fileName, line);
		}
		else if (cmd == "unicodeMap")
		{
			parseUnicodeMap(tokens, fileName, line);
		}
		else if (cmd == "cMapDir")
		{
			parseCMapDir(tokens, fileName, line);
		}
		else if (cmd == "toUnicodeDir")
		{
			parseToUnicodeDir(tokens, fileName, line);
		}
		else if (cmd == "unicodeRemapping")
		{
			parseUnicodeRemapping(tokens, fileName, line);
		}
		else if (cmd == "fontFile")
		{
			parseFontFile(tokens, fileName, line);
		}
		else if (cmd == "fontDir")
		{
			parseFontDir(tokens, fileName, line);
		}
		else if (cmd == "fontFileCC")
		{
			parseFontFileCC(tokens, fileName, line);
		}
		else if (cmd == "psPaperSize")
		{
			parsePSPaperSize(tokens, fileName, line);
		}
		else if (cmd == "psImageableArea")
		{
			parsePSImageableArea(tokens, fileName, line);
		}
		else if (cmd == "psCrop")
		{
			parseYesNo("psCrop", &psCrop, tokens, fileName, line);
		}
		else if (cmd == "psUseCropBoxAsPage")
		{
			parseYesNo("psUseCropBoxAsPage", &psUseCropBoxAsPage, tokens, fileName, line);
		}
		else if (cmd == "psExpandSmaller")
		{
			parseYesNo("psExpandSmaller", &psExpandSmaller, tokens, fileName, line);
		}
		else if (cmd == "psShrinkLarger")
		{
			parseYesNo("psShrinkLarger", &psShrinkLarger, tokens, fileName, line);
		}
		else if (cmd == "psCenter")
		{
			parseYesNo("psCenter", &psCenter, tokens, fileName, line);
		}
		else if (cmd == "psDuplex")
		{
			parseYesNo("psDuplex", &psDuplex, tokens, fileName, line);
		}
		else if (cmd == "psLevel")
		{
			parsePSLevel(tokens, fileName, line);
		}
		else if (cmd == "psResidentFont")
		{
			parsePSResidentFont(tokens, fileName, line);
		}
		else if (cmd == "psResidentFont16")
		{
			parsePSResidentFont16(tokens, fileName, line);
		}
		else if (cmd == "psResidentFontCC")
		{
			parsePSResidentFontCC(tokens, fileName, line);
		}
		else if (cmd == "psEmbedType1Fonts")
		{
			parseYesNo("psEmbedType1", &psEmbedType1, tokens, fileName, line);
		}
		else if (cmd == "psEmbedTrueTypeFonts")
		{
			parseYesNo("psEmbedTrueType", &psEmbedTrueType, tokens, fileName, line);
		}
		else if (cmd == "psEmbedCIDPostScriptFonts")
		{
			parseYesNo("psEmbedCIDPostScript", &psEmbedCIDPostScript, tokens, fileName, line);
		}
		else if (cmd == "psEmbedCIDTrueTypeFonts")
		{
			parseYesNo("psEmbedCIDTrueType", &psEmbedCIDTrueType, tokens, fileName, line);
		}
		else if (cmd == "psFontPassthrough")
		{
			parseYesNo("psFontPassthrough", &psFontPassthrough, tokens, fileName, line);
		}
		else if (cmd == "psPreload")
		{
			parseYesNo("psPreload", &psPreload, tokens, fileName, line);
		}
		else if (cmd == "psOPI")
		{
			parseYesNo("psOPI", &psOPI, tokens, fileName, line);
		}
		else if (cmd == "psASCIIHex")
		{
			parseYesNo("psASCIIHex", &psASCIIHex, tokens, fileName, line);
		}
		else if (cmd == "psLZW")
		{
			parseYesNo("psLZW", &psLZW, tokens, fileName, line);
		}
		else if (cmd == "psUncompressPreloadedImages")
		{
			parseYesNo("psUncompressPreloadedImages", &psUncompressPreloadedImages, tokens, fileName, line);
		}
		else if (cmd == "psMinLineWidth")
		{
			parseFloat("psMinLineWidth", &psMinLineWidth, tokens, fileName, line);
		}
		else if (cmd == "psRasterResolution")
		{
			parseFloat("psRasterResolution", &psRasterResolution, tokens, fileName, line);
		}
		else if (cmd == "psRasterMono")
		{
			parseYesNo("psRasterMono", &psRasterMono, tokens, fileName, line);
		}
		else if (cmd == "psRasterSliceSize")
		{
			parseInteger("psRasterSliceSize", &psRasterSliceSize, tokens, fileName, line);
		}
		else if (cmd == "psAlwaysRasterize")
		{
			parseYesNo("psAlwaysRasterize", &psAlwaysRasterize, tokens, fileName, line);
		}
		else if (cmd == "psNeverRasterize")
		{
			parseYesNo("psNeverRasterize", &psNeverRasterize, tokens, fileName, line);
		}
		else if (cmd == "textEncoding")
		{
			parseString("textEncoding", &textEncoding, tokens, fileName, line);
		}
		else if (cmd == "textEOL")
		{
			parseTextEOL(tokens, fileName, line);
		}
		else if (cmd == "textPageBreaks")
		{
			parseYesNo("textPageBreaks", &textPageBreaks, tokens, fileName, line);
		}
		else if (cmd == "textKeepTinyChars")
		{
			parseYesNo("textKeepTinyChars", &textKeepTinyChars, tokens, fileName, line);
		}
		else if (cmd == "initialZoom")
		{
			parseString("initialZoom", &initialZoom, tokens, fileName, line);
		}
		else if (cmd == "defaultFitZoom")
		{
			parseInteger("defaultFitZoom", &defaultFitZoom, tokens, fileName, line);
		}
		else if (cmd == "zoomScaleFactor")
		{
			parseZoomScaleFactor(tokens, fileName, line);
		}
		else if (cmd == "zoomValues")
		{
			parseZoomValues(tokens, fileName, line);
		}
		else if (cmd == "initialDisplayMode")
		{
			parseString("initialDisplayMode", &initialDisplayMode, tokens, fileName, line);
		}
		else if (cmd == "initialToolbarState")
		{
			parseYesNo("initialToolbarState", &initialToolbarState, tokens, fileName, line);
		}
		else if (cmd == "initialSidebarState")
		{
			parseYesNo("initialSidebarState", &initialSidebarState, tokens, fileName, line);
		}
		else if (cmd == "initialSidebarWidth")
		{
			parseInteger("initialSidebarWidth", &initialSidebarWidth, tokens, fileName, line);
		}
		else if (cmd == "initialSelectMode")
		{
			parseString("initialSelectMode", &initialSelectMode, tokens, fileName, line);
		}
		else if (cmd == "maxTileWidth")
		{
			parseInteger("maxTileWidth", &maxTileWidth, tokens, fileName, line);
		}
		else if (cmd == "maxTileHeight")
		{
			parseInteger("maxTileHeight", &maxTileHeight, tokens, fileName, line);
		}
		else if (cmd == "tileCacheSize")
		{
			parseInteger("tileCacheSize", &tileCacheSize, tokens, fileName, line);
		}
		else if (cmd == "workerThreads")
		{
			parseInteger("workerThreads", &workerThreads, tokens, fileName, line);
		}
		else if (cmd == "enableFreeType")
		{
			parseYesNo("enableFreeType", &enableFreeType, tokens, fileName, line);
		}
		else if (cmd == "disableFreeTypeHinting")
		{
			parseYesNo("disableFreeTypeHinting", &disableFreeTypeHinting, tokens, fileName, line);
		}
		else if (cmd == "antialias")
		{
			parseYesNo("antialias", &antialias, tokens, fileName, line);
		}
		else if (cmd == "vectorAntialias")
		{
			parseYesNo("vectorAntialias", &vectorAntialias, tokens, fileName, line);
		}
		else if (cmd == "imageMaskAntialias")
		{
			parseYesNo("imageMaskAntialias", &imageMaskAntialias, tokens, fileName, line);
		}
		else if (cmd == "antialiasPrinting")
		{
			parseYesNo("antialiasPrinting", &antialiasPrinting, tokens, fileName, line);
		}
		else if (cmd == "strokeAdjust")
		{
			parseStrokeAdjust(tokens, fileName, line);
		}
		else if (cmd == "screenType")
		{
			parseScreenType(tokens, fileName, line);
		}
		else if (cmd == "screenSize")
		{
			parseInteger("screenSize", &screenSize, tokens, fileName, line);
		}
		else if (cmd == "screenDotRadius")
		{
			parseInteger("screenDotRadius", &screenDotRadius, tokens, fileName, line);
		}
		else if (cmd == "screenGamma")
		{
			parseFloat("screenGamma", &screenGamma, tokens, fileName, line);
		}
		else if (cmd == "screenBlackThreshold")
		{
			parseFloat("screenBlackThreshold", &screenBlackThreshold, tokens, fileName, line);
		}
		else if (cmd == "screenWhiteThreshold")
		{
			parseFloat("screenWhiteThreshold", &screenWhiteThreshold, tokens, fileName, line);
		}
		else if (cmd == "minLineWidth")
		{
			parseFloat("minLineWidth", &minLineWidth, tokens, fileName, line);
		}
		else if (cmd == "enablePathSimplification")
		{
			parseYesNo("enablePathSimplification", &enablePathSimplification, tokens, fileName, line);
		}
		else if (cmd == "drawAnnotations")
		{
			parseYesNo("drawAnnotations", &drawAnnotations, tokens, fileName, line);
		}
		else if (cmd == "drawFormFields")
		{
			parseYesNo("drawFormFields", &drawFormFields, tokens, fileName, line);
		}
		else if (cmd == "enableXFA")
		{
			parseYesNo("enableXFA", &enableXFA, tokens, fileName, line);
		}
		else if (cmd == "overprintPreview")
		{
			parseYesNo("overprintPreview", &overprintPreview, tokens, fileName, line);
		}
		else if (cmd == "paperColor")
		{
			parseString("paperColor", &paperColor, tokens, fileName, line);
		}
		else if (cmd == "matteColor")
		{
			parseString("matteColor", &matteColor, tokens, fileName, line);
		}
		else if (cmd == "fullScreenMatteColor")
		{
			parseString("fullScreenMatteColor", &fullScreenMatteColor, tokens, fileName, line);
		}
		else if (cmd == "selectionColor")
		{
			parseString("selectionColor", &selectionColor, tokens, fileName, line);
		}
		else if (cmd == "reverseVideoInvertImages")
		{
			parseYesNo("reverseVideoInvertImages", &reverseVideoInvertImages, tokens, fileName, line);
		}
		else if (cmd == "allowLinksToChangeZoom")
		{
			parseYesNo("allowLinksToChangeZoom", &allowLinksToChangeZoom, tokens, fileName, line);
		}
		else if (cmd == "launchCommand")
		{
			parseString("launchCommand", &launchCommand, tokens, fileName, line);
		}
		else if (cmd == "movieCommand")
		{
			parseString("movieCommand", &movieCommand, tokens, fileName, line);
		}
		else if (cmd == "defaultPrinter")
		{
			parseString("defaultPrinter", &defaultPrinter, tokens, fileName, line);
		}
		else if (cmd == "mapNumericCharNames")
		{
			parseYesNo("mapNumericCharNames", &mapNumericCharNames, tokens, fileName, line);
		}
		else if (cmd == "mapUnknownCharNames")
		{
			parseYesNo("mapUnknownCharNames", &mapUnknownCharNames, tokens, fileName, line);
		}
		else if (cmd == "mapExtTrueTypeFontsViaUnicode")
		{
			parseYesNo("mapExtTrueTypeFontsViaUnicode", &mapExtTrueTypeFontsViaUnicode, tokens, fileName, line);
		}
		else if (cmd == "useTrueTypeUnicodeMapping")
		{
			parseYesNo("useTrueTypeUnicodeMapping", &useTrueTypeUnicodeMapping, tokens, fileName, line);
		}
		else if (cmd == "ignoreWrongSizeToUnicode")
		{
			parseYesNo("ignoreWrongSizeToUnicode", &ignoreWrongSizeToUnicode, tokens, fileName, line);
		}
		else if (cmd == "dropFont")
		{
			parseDropFont(tokens, fileName, line);
		}
		else if (cmd == "separateRotatedText")
		{
			parseYesNo("separateRotatedText", &separateRotatedText, tokens, fileName, line);
		}
		else if (cmd == "bind")
		{
			parseBind(tokens, fileName, line);
		}
		else if (cmd == "unbind")
		{
			parseUnbind(tokens, fileName, line);
		}
		else if (cmd == "popupMenuCmd")
		{
			parsePopupMenuCmd(tokens, fileName, line);
		}
		else if (cmd == "tabStateFile")
		{
			parseString("tabStateFile", &tabStateFile, tokens, fileName, line);
		}
		else if (cmd == "sessionFile")
		{
			parseString("sessionFile", &sessionFile, tokens, fileName, line);
		}
		else if (cmd == "saveSessionOnQuit")
		{
			parseYesNo("saveSessionOnQuit", &saveSessionOnQuit, tokens, fileName, line);
		}
		else if (cmd == "savePageNumbers")
		{
			parseYesNo("savePageNumbers", &savePageNumbers, tokens, fileName, line);
		}
		else if (cmd == "printCommands")
		{
			parseYesNo("printCommands", &printCommands, tokens, fileName, line);
		}
		else if (cmd == "printStatusInfo")
		{
			parseYesNo("printStatusInfo", &printStatusInfo, tokens, fileName, line);
		}
		else if (cmd == "errQuiet")
		{
			parseYesNo("errQuiet", &errQuiet, tokens, fileName, line);
		}
		else if (cmd == "debugLogFile")
		{
			parseString("debugLogFile", &debugLogFile, tokens, fileName, line);
		}
		else
		{
			error(errConfig, -1, "Unknown config file command '{}' ({}:{})", cmd, fileName, line);
			if (cmd == "displayFontX" || cmd == "displayNamedCIDFontX" || cmd == "displayCIDFontX")
				error(errConfig, -1, "Xpdf no longer supports X fonts");
			else if (cmd == "enableT1lib")
				error(errConfig, -1, "Xpdf no longer uses t1lib");
			else if (cmd == "t1libControl" || cmd == "freetypeControl")
				error(errConfig, -1, "The t1libControl and freetypeControl options have been replaced by the enableT1lib, enableFreeType, and antialias options");
			else if (cmd == "fontpath" || cmd == "fontmap")
				error(errConfig, -1, "The config file format has changed since Xpdf 0.9x");
		}
	}
}

// Split a line into a sequence of tokens.  Tokens are separated by whitespace.  Each token is one of:
//   - unquoted string, which can contain any char other than whitespace, and which cannot start with a single quote, double quote, or at-double-quote (xxxx)
//   - single-quoted string, which can contain any char other than the single quote ('xxxx')
//   - double-quoted string, which can contain any char other than the double quote ("xxxx")
//   - at-double-quoted string, which can contain variables and escape chars (@"xxxx")
//     - variables look like ${name}
//     - special chars (${}") can be escaped with %, e.g.,
//       @"foo%"bar", @"foo%$bar", @"foo%%bar"
VEC_STR GlobalParams::parseLineTokens(char* buf, const std::string& fileName, int line)
{
	VEC_STR tokens;
	char*   p1 = buf;
	while (*p1)
	{
		for (; *p1 && isspace(*p1); ++p1)
			;
		if (!*p1)
			break;
		if (*p1 == '"' || *p1 == '\'')
		{
			char* p2;
			for (p2 = p1 + 1; *p2 && *p2 != *p1; ++p2)
				;
			++p1;
			tokens.push_back(std::string(p1, (int)(p2 - p1)));
			p1 = *p2 ? p2 + 1 : p2;
		}
		else if (*p1 == '@' && p1[1] == '"')
		{
			std::string token;
			char*       p2 = p1 + 2;
			while (*p2 && *p2 != '"')
			{
				if (*p2 == '%' && p2[1])
				{
					token += p2[1];
					p2 += 2;
				}
				else if (*p2 == '$' && p2[1] == '{')
				{
					p2 += 2;
					char* p3;
					for (p3 = p2; *p3 && *p3 != '}'; ++p3)
						;
					const auto varName = std::string(p2, (int)(p3 - p2));
					if (const auto& it = configFileVars.find(varName); it != configFileVars.end())
						token += it->second;
					else
						error(errConfig, -1, "Unknown config file variable '%t'", varName);
					p2 = *p3 ? p3 + 1 : p3;
				}
				else
				{
					token += *p2;
					++p2;
				}
			}
			tokens.push_back(token);
			p1 = *p2 ? p2 + 1 : p2;
		}
		else
		{
			char* p2;
			for (p2 = p1 + 1; *p2 && !isspace(*p2); ++p2)
				;
			tokens.push_back(std::string(p1, (int)(p2 - p1)));
			p1 = p2;
		}
	}
	return tokens;
}

void GlobalParams::parseNameToUnicode(const VEC_STR& tokens, const std::string& fileName, int line)
{
	char *  tok1, *tok2;
	FILE*   f;
	char    buf[256];
	int     line2;
	Unicode u;

	if (tokens.size() != 2)
	{
		error(errConfig, -1, "Bad 'nameToUnicode' config file command ({}:{})", fileName, line);
		return;
	}
	const auto name = tokens.at(1);
	if (!(f = openFile(name.c_str(), "r")))
	{
		error(errConfig, -1, "Couldn't open 'nameToUnicode' file '{}'", name);
		return;
	}
	line2 = 1;
	while (getLine(buf, sizeof(buf), f))
	{
		tok1 = strtok(buf, " \t\r\n");
		tok2 = strtok(nullptr, " \t\r\n");
		if (tok1 && tok2)
		{
			sscanf(tok1, "%x", &u);
			nameToUnicode.emplace(std::string { tok2 }, u);
		}
		else
		{
			error(errConfig, -1, "Bad line in 'nameToUnicode' file ({}:{})", name, line2);
		}
		++line2;
	}
	fclose(f);
}

void GlobalParams::parseCIDToUnicode(const VEC_STR& tokens, const std::string& fileName, int line)
{
	if (tokens.size() != 3)
	{
		error(errConfig, -1, "Bad 'cidToUnicode' config file command ({}:{})", fileName, line);
		return;
	}
	const auto collection     = tokens.at(1);
	const auto name           = tokens.at(2);
	cidToUnicodes[collection] = name;
}

void GlobalParams::parseUnicodeToUnicode(const VEC_STR& tokens, const std::string& fileName, int line)
{
	if (tokens.size() != 3)
	{
		error(errConfig, -1, "Bad 'unicodeToUnicode' config file command ({}:{})", fileName, line);
		return;
	}
	const auto font = tokens.at(1);
	const auto file = tokens.at(2);
	unicodeToUnicodes.emplace(font, file);
}

void GlobalParams::parseUnicodeMap(const VEC_STR& tokens, const std::string& fileName, int line)
{
	if (tokens.size() != 3)
	{
		error(errConfig, -1, "Bad 'unicodeMap' config file command ({}:{})", fileName, line);
		return;
	}
	const auto encodingName = tokens.at(1);
	const auto name         = tokens.at(2);
	unicodeMaps.emplace(encodingName, name);
}

void GlobalParams::parseCMapDir(const VEC_STR& tokens, const std::string& fileName, int line)
{
	if (tokens.size() != 3)
	{
		error(errConfig, -1, "Bad 'cMapDir' config file command ({}:{})", fileName, line);
		return;
	}
	const auto collection = tokens.at(1);
	const auto dir        = tokens.at(2);
	if (!cMapDirs.contains(collection))
		cMapDirs.emplace(collection, VEC_STR {});
	if (auto it = cMapDirs.find(collection); it != cMapDirs.end())
		it->second.push_back(dir);
}

void GlobalParams::parseToUnicodeDir(const VEC_STR& tokens, const std::string& fileName, int line)
{
	if (tokens.size() != 2)
	{
		error(errConfig, -1, "Bad 'toUnicodeDir' config file command ({}:{})", fileName, line);
		return;
	}
	toUnicodeDirs.push_back(tokens.at(1));
}

void GlobalParams::parseUnicodeRemapping(const VEC_STR& tokens, const std::string& fileName, int line)
{
	if (tokens.size() != 2)
	{
		error(errConfig, -1, "Bad 'unicodeRemapping' config file command ({}:{})", fileName, line);
		return;
	}
	unicodeRemapping->parseFile(tokens.at(1));
}

void GlobalParams::parseFontFile(const VEC_STR& tokens, const std::string& fileName, int line)
{
	if (tokens.size() != 3)
	{
		error(errConfig, -1, "Bad 'fontFile' config file command ({}:{})", fileName, line);
		return;
	}
	fprintf(stderr, "parseFontFile: [%s, %s]\n", tokens.at(1).c_str(), tokens.at(2).c_str());
	fontFiles.emplace(tokens.at(1), tokens.at(2));
}

void GlobalParams::parseFontDir(const VEC_STR& tokens, const std::string& fileName, int line)
{
	if (tokens.size() != 2)
	{
		error(errConfig, -1, "Bad 'fontDir' config file command ({}:{})", fileName, line);
		return;
	}
	fontDirs.push_back(tokens.at(1));
}

void GlobalParams::parseFontFileCC(const VEC_STR& tokens, const std::string& fileName, int line)
{
	if (tokens.size() != 3)
	{
		error(errConfig, -1, "Bad 'fontFileCC' config file command ({}:{})", fileName, line);
		return;
	}
	ccFontFiles.emplace(tokens.at(1), tokens.at(2));
}

void GlobalParams::parsePSPaperSize(const VEC_STR& tokens, const std::string& fileName, int line)
{
	if (tokens.size() == 2)
	{
		const auto& tok = tokens.at(1);
		if (!setPSPaperSize(tok.c_str()))
			error(errConfig, -1, "Bad 'psPaperSize' config file command ({}:{})", fileName, line);
	}
	else if (tokens.size() == 3)
	{
		const auto& tok  = tokens.at(1);
		psPaperWidth     = atoi(tok.c_str());
		const auto& tok2 = tokens.at(2);
		psPaperHeight    = atoi(tok2.c_str());
		psImageableLLX   = 0;
		psImageableLLY   = 0;
		psImageableURX.store(psPaperWidth.load());
		psImageableURY.store(psPaperHeight.load());
	}
	else
	{
		error(errConfig, -1, "Bad 'psPaperSize' config file command ({}:{})", fileName, line);
	}
}

void GlobalParams::parsePSImageableArea(const VEC_STR& tokens, const std::string& fileName, int line)
{
	if (tokens.size() != 5)
	{
		error(errConfig, -1, "Bad 'psImageableArea' config file command ({}:{})", fileName, line);
		return;
	}
	psImageableLLX = atoi(tokens.at(1).c_str());
	psImageableLLY = atoi(tokens.at(2).c_str());
	psImageableURX = atoi(tokens.at(3).c_str());
	psImageableURY = atoi(tokens.at(4).c_str());
}

void GlobalParams::parsePSLevel(const VEC_STR& tokens, const std::string& fileName, int line)
{
	if (tokens.size() != 2)
	{
		error(errConfig, -1, "Bad 'psLevel' config file command ({}:{})", fileName, line);
		return;
	}
	const auto& tok = tokens.at(1);
	if (tok == "level1")
		psLevel = psLevel1;
	else if (tok == "level1sep")
		psLevel = psLevel1Sep;
	else if (tok == "level2")
		psLevel = psLevel2;
	else if (tok == "level2gray")
		psLevel = psLevel2Gray;
	else if (tok == "level2sep")
		psLevel = psLevel2Sep;
	else if (tok == "level3")
		psLevel = psLevel3;
	else if (tok == "level3gray")
		psLevel = psLevel3Gray;
	else if (tok == "level3Sep")
		psLevel = psLevel3Sep;
	else
		error(errConfig, -1, "Bad 'psLevel' config file command ({}:{})", fileName, line);
}

void GlobalParams::parsePSResidentFont(const VEC_STR& tokens, const std::string& fileName, int line)
{
	if (tokens.size() != 3)
	{
		error(errConfig, -1, "Bad 'psResidentFont' config file command ({}:{})", fileName, line);
		return;
	}
	psResidentFonts.emplace(tokens.at(1), tokens.at(2));
}

void GlobalParams::parsePSResidentFont16(const VEC_STR& tokens, const std::string& fileName, int line)
{
	if (tokens.size() != 5)
	{
		error(errConfig, -1, "Bad 'psResidentFont16' config file command ({}:{})", fileName, line);
		return;
	}
	const auto& tok = tokens.at(2);
	int         wMode;
	if (tok == "H")
	{
		wMode = 0;
	}
	else if (tok == "V")
	{
		wMode = 1;
	}
	else
	{
		error(errConfig, -1, "Bad wMode in psResidentFont16 config file command ({}:{})", fileName, line);
		return;
	}
	psResidentFonts16.emplace_back(tokens.at(1), wMode, tokens.at(3), tokens.at(4));
}

void GlobalParams::parsePSResidentFontCC(const VEC_STR& tokens, const std::string& fileName, int line)
{
	if (tokens.size() != 5)
	{
		error(errConfig, -1, "Bad 'psResidentFontCC' config file command ({}:{})", fileName, line);
		return;
	}
	const auto& tok = tokens.at(2);
	int         wMode;
	if (tok == "H")
	{
		wMode = 0;
	}
	else if (tok == "V")
	{
		wMode = 1;
	}
	else
	{
		error(errConfig, -1, "Bad wMode in psResidentFontCC config file command ({}:{})", fileName, line);
		return;
	}
	psResidentFontsCC.emplace_back(tokens.at(1), wMode, tokens.at(3), tokens.at(4));
}

void GlobalParams::parseTextEOL(const VEC_STR& tokens, const std::string& fileName, int line)
{
	if (tokens.size() != 2)
	{
		error(errConfig, -1, "Bad 'textEOL' config file command ({}:{})", fileName, line);
		return;
	}
	const auto& tok = tokens.at(1);
	if (tok == "unix")
		textEOL = eolUnix;
	else if (tok == "dos")
		textEOL = eolDOS;
	else if (tok == "mac")
		textEOL = eolMac;
	else
		error(errConfig, -1, "Bad 'textEOL' config file command ({}:{})", fileName, line);
}

void GlobalParams::parseStrokeAdjust(const VEC_STR& tokens, const std::string& fileName, int line)
{
	if (tokens.size() != 2)
	{
		error(errConfig, -1, "Bad 'strokeAdjust' config file command ({}:{})", fileName, line);
		return;
	}
	const auto& tok = tokens.at(1);
	if (tok == "no")
		strokeAdjust = strokeAdjustOff;
	else if (tok == "yes")
		strokeAdjust = strokeAdjustNormal;
	else if (tok == "cad")
		strokeAdjust = strokeAdjustCAD;
	else
		error(errConfig, -1, "Bad 'strokeAdjust' config file command ({}:{})", fileName, line);
}

void GlobalParams::parseScreenType(const VEC_STR& tokens, const std::string& fileName, int line)
{
	if (tokens.size() != 2)
	{
		error(errConfig, -1, "Bad 'screenType' config file command ({}:{})", fileName, line);
		return;
	}
	const auto& tok = tokens.at(1);
	if (tok == "dispersed")
		screenType = screenDispersed;
	else if (tok == "clustered")
		screenType = screenClustered;
	else if (tok == "stochasticClustered")
		screenType = screenStochasticClustered;
	else
		error(errConfig, -1, "Bad 'screenType' config file command ({}:{})", fileName, line);
}

void GlobalParams::parseDropFont(const VEC_STR& tokens, const std::string& fileName, int line)
{
	if (tokens.size() != 2)
	{
		error(errConfig, -1, "Bad 'dropFont' config file command ({}:{})", fileName, line);
		return;
	}
	droppedFonts.emplace(tokens.at(1), 1);
}

void GlobalParams::parseBind(const VEC_STR& tokens, const std::string& fileName, int line)
{
	int code, mods, context;

	if (tokens.size() < 4)
	{
		error(errConfig, -1, "Bad 'bind' config file command ({}:{})", fileName, line);
		return;
	}
	if (!parseKey(tokens.at(1), tokens.at(2), &code, &mods, &context, "bind", tokens, fileName, line))
		return;

	for (auto it = keyBindings.begin(); it != keyBindings.end();)
	{
		const auto& binding = *it;
		if (binding.code == code && binding.mods == mods && binding.context == context)
		{
			keyBindings.erase(it);
			break;
		}
	}

	VEC_STR cmds;
	for (int i = 3; i < tokens.size(); ++i)
		cmds.push_back(tokens.at(i));
	keyBindings.emplace_back(code, mods, context, cmds);
}

void GlobalParams::parseUnbind(const VEC_STR& tokens, const std::string& fileName, int line)
{
	if (tokens.size() != 3)
	{
		error(errConfig, -1, "Bad 'unbind' config file command ({}:{})", fileName, line);
		return;
	}
	int code, mods, context;
	if (!parseKey(tokens.at(1), tokens.at(2), &code, &mods, &context, "unbind", tokens, fileName, line))
		return;

	for (auto it = keyBindings.begin(); it != keyBindings.end();)
	{
		const auto& binding = *it;
		if (binding.code == code && binding.mods == mods && binding.context == context)
		{
			keyBindings.erase(it);
			break;
		}
	}
}

bool GlobalParams::parseKey(const std::string& modKeyStr, const std::string& contextStr, int* code, int* mods, int* context, const char* cmdName, const VEC_STR& tokens, const std::string& fileName, int line)
{
	int btn;

	*mods          = xpdfKeyModNone;
	const char* p0 = modKeyStr.c_str();
	while (1)
	{
		if (!strncmp(p0, "shift-", 6))
		{
			*mods |= xpdfKeyModShift;
			p0 += 6;
		}
		else if (!strncmp(p0, "ctrl-", 5))
		{
			*mods |= xpdfKeyModCtrl;
			p0 += 5;
		}
		else if (!strncmp(p0, "alt-", 4))
		{
			*mods |= xpdfKeyModAlt;
			p0 += 4;
		}
		else
		{
			break;
		}
	}

	if (!strcmp(p0, "space"))
	{
		*code = ' ';
	}
	else if (!strcmp(p0, "tab"))
	{
		*code = xpdfKeyCodeTab;
	}
	else if (!strcmp(p0, "return"))
	{
		*code = xpdfKeyCodeReturn;
	}
	else if (!strcmp(p0, "enter"))
	{
		*code = xpdfKeyCodeEnter;
	}
	else if (!strcmp(p0, "backspace"))
	{
		*code = xpdfKeyCodeBackspace;
	}
	else if (!strcmp(p0, "esc"))
	{
		*code = xpdfKeyCodeEsc;
	}
	else if (!strcmp(p0, "insert"))
	{
		*code = xpdfKeyCodeInsert;
	}
	else if (!strcmp(p0, "delete"))
	{
		*code = xpdfKeyCodeDelete;
	}
	else if (!strcmp(p0, "home"))
	{
		*code = xpdfKeyCodeHome;
	}
	else if (!strcmp(p0, "end"))
	{
		*code = xpdfKeyCodeEnd;
	}
	else if (!strcmp(p0, "pgup"))
	{
		*code = xpdfKeyCodePgUp;
	}
	else if (!strcmp(p0, "pgdn"))
	{
		*code = xpdfKeyCodePgDn;
	}
	else if (!strcmp(p0, "left"))
	{
		*code = xpdfKeyCodeLeft;
	}
	else if (!strcmp(p0, "right"))
	{
		*code = xpdfKeyCodeRight;
	}
	else if (!strcmp(p0, "up"))
	{
		*code = xpdfKeyCodeUp;
	}
	else if (!strcmp(p0, "down"))
	{
		*code = xpdfKeyCodeDown;
	}
	else if (p0[0] == 'f' && p0[1] >= '1' && p0[1] <= '9' && !p0[2])
	{
		*code = xpdfKeyCodeF1 + (p0[1] - '1');
	}
	else if (p0[0] == 'f' && ((p0[1] >= '1' && p0[1] <= '2' && p0[2] >= '0' && p0[2] <= '9') || (p0[1] == '3' && p0[2] >= '0' && p0[2] <= '5')) && !p0[3])
	{
		*code = xpdfKeyCodeF1 + 10 * (p0[1] - '0') + (p0[2] - '0') - 1;
	}
	else if (!strncmp(p0, "mousePress", 10) && p0[10] >= '0' && p0[10] <= '9' && (!p0[11] || (p0[11] >= '0' && p0[11] <= '9' && !p0[12])) && (btn = atoi(p0 + 10)) >= 1 && btn <= 32)
	{
		*code = xpdfKeyCodeMousePress1 + btn - 1;
	}
	else if (!strncmp(p0, "mouseRelease", 12) && p0[12] >= '0' && p0[12] <= '9' && (!p0[13] || (p0[13] >= '0' && p0[13] <= '9' && !p0[14])) && (btn = atoi(p0 + 12)) >= 1 && btn <= 32)
	{
		*code = xpdfKeyCodeMouseRelease1 + btn - 1;
	}
	else if (!strncmp(p0, "mouseClick", 10) && p0[10] >= '0' && p0[10] <= '9' && (!p0[11] || (p0[11] >= '0' && p0[11] <= '9' && !p0[12])) && (btn = atoi(p0 + 10)) >= 1 && btn <= 32)
	{
		*code = xpdfKeyCodeMouseClick1 + btn - 1;
	}
	else if (!strncmp(p0, "mouseDoubleClick", 16) && p0[16] >= '0' && p0[16] <= '9' && (!p0[17] || (p0[17] >= '0' && p0[17] <= '9' && !p0[18])) && (btn = atoi(p0 + 16)) >= 1 && btn <= 32)
	{
		*code = xpdfKeyCodeMouseDoubleClick1 + btn - 1;
	}
	else if (!strncmp(p0, "mouseTripleClick", 16) && p0[16] >= '0' && p0[16] <= '9' && (!p0[17] || (p0[17] >= '0' && p0[17] <= '9' && !p0[18])) && (btn = atoi(p0 + 16)) >= 1 && btn <= 32)
	{
		*code = xpdfKeyCodeMouseTripleClick1 + btn - 1;
	}
	else if (*p0 >= 0x20 && *p0 <= 0x7e && !p0[1])
	{
		*code = (int)*p0;
	}
	else
	{
		error(errConfig, -1, "Bad key/modifier in '{}' config file command ({}:{})", cmdName, fileName, line);
		return false;
	}

	p0 = contextStr.c_str();
	if (!strcmp(p0, "any"))
	{
		*context = xpdfKeyContextAny;
	}
	else
	{
		*context = xpdfKeyContextAny;
		while (1)
		{
			if (!strncmp(p0, "fullScreen", 10))
			{
				*context |= xpdfKeyContextFullScreen;
				p0 += 10;
			}
			else if (!strncmp(p0, "window", 6))
			{
				*context |= xpdfKeyContextWindow;
				p0 += 6;
			}
			else if (!strncmp(p0, "continuous", 10))
			{
				*context |= xpdfKeyContextContinuous;
				p0 += 10;
			}
			else if (!strncmp(p0, "singlePage", 10))
			{
				*context |= xpdfKeyContextSinglePage;
				p0 += 10;
			}
			else if (!strncmp(p0, "overLink", 8))
			{
				*context |= xpdfKeyContextOverLink;
				p0 += 8;
			}
			else if (!strncmp(p0, "offLink", 7))
			{
				*context |= xpdfKeyContextOffLink;
				p0 += 7;
			}
			else if (!strncmp(p0, "outline", 7))
			{
				*context |= xpdfKeyContextOutline;
				p0 += 7;
			}
			else if (!strncmp(p0, "mainWin", 7))
			{
				*context |= xpdfKeyContextMainWin;
				p0 += 7;
			}
			else if (!strncmp(p0, "scrLockOn", 9))
			{
				*context |= xpdfKeyContextScrLockOn;
				p0 += 9;
			}
			else if (!strncmp(p0, "scrLockOff", 10))
			{
				*context |= xpdfKeyContextScrLockOff;
				p0 += 10;
			}
			else
			{
				error(errConfig, -1, "Bad context in '{}' config file command ({}:{})", cmdName, fileName, line);
				return false;
			}
			if (!*p0)
				break;
			if (*p0 != ',')
			{
				error(errConfig, -1, "Bad context in '{}' config file command ({}:{})", cmdName, fileName, line);
				return false;
			}
			++p0;
		}
	}

	return true;
}

void GlobalParams::parsePopupMenuCmd(const VEC_STR& tokens, const std::string& fileName, int line)
{
	if (tokens.size() < 3)
	{
		error(errConfig, -1, "Bad 'popupMenuCmd' config file command ({}:{})", fileName, line);
		return;
	}
	VEC_STR cmds;
	for (int i = 2; i < tokens.size(); ++i)
		cmds.push_back(tokens.at(i));
	popupMenuCmds.emplace_back(tokens.at(1), cmds);
}

void GlobalParams::parseZoomScaleFactor(const VEC_STR& tokens, const std::string& fileName, int line)
{
	if (tokens.size() != 2)
	{
		error(errConfig, -1, "Bad 'zoomScaleFactor' config file command ({}:{})", fileName, line);
		return;
	}
	const auto& tok = tokens.at(1);
	if (tok.size() == 0)
	{
		error(errConfig, -1, "Bad 'zoomScaleFactor' config file command ({}:{})", fileName, line);
		return;
	}
	if (tok == "actual")
	{
		zoomScaleFactor = -1;
	}
	else
	{
		for (int i = 0; i < tok.size(); ++i)
		{
			if (!((tok.at(i) >= '0' && tok.at(i) <= '9') || tok.at(i) == '.'))
			{
				error(errConfig, -1, "Bad 'zoomScaleFactor' config file command ({}:{})", fileName, line);
				return;
			}
		}
		zoomScaleFactor = atof(tok.c_str());
	}
}

void GlobalParams::parseZoomValues(const VEC_STR& tokens, const std::string& fileName, int line)
{
	if (tokens.size() < 2)
	{
		error(errConfig, -1, "Bad 'zoomValues' config file command ({}:{})", fileName, line);
		return;
	}
	for (int i = 1; i < tokens.size(); ++i)
	{
		const auto& tok = tokens.at(i);
		for (int j = 0; j < tok.size(); ++j)
		{
			if (tok.at(j) < '0' || tok.at(j) > '9')
			{
				error(errConfig, -1, "Bad 'zoomValues' config file command ({}:{})", fileName, line);
				return;
			}
		}
	}

	zoomValues.clear();
	for (int i = 1; i < tokens.size(); ++i)
		zoomValues.push_back(tokens.at(i));
}

void GlobalParams::parseYesNo(const char* cmdName, std::atomic<bool>* flag, const VEC_STR& tokens, const std::string& fileName, int line)
{
	if (tokens.size() != 2)
	{
		error(errConfig, -1, "Bad '{}' config file command ({}:{})", cmdName, fileName, line);
		return;
	}
	const auto& tok = tokens.at(1);
	if (!parseYesNo2(tok.c_str(), flag))
		error(errConfig, -1, "Bad '{}' config file command ({}:{})", cmdName, fileName, line);
}

bool GlobalParams::parseYesNo2(const char* token, std::atomic<bool>* flag)
{
	if (!strcmp(token, "yes"))
		*flag = true;
	else if (!strcmp(token, "no"))
		*flag = false;
	else
		return false;
	return true;
}

void GlobalParams::parseString(const char* cmdName, std::string* s, const VEC_STR& tokens, const std::string& fileName, int line)
{
	if (tokens.size() != 2)
	{
		error(errConfig, -1, "Bad '{}' config file command ({}:{})", cmdName, fileName, line);
		return;
	}
	*s = tokens.at(1);
}

void GlobalParams::parseInteger(const char* cmdName, std::atomic<int>* val, const VEC_STR& tokens, const std::string& fileName, int line)
{
	if (tokens.size() != 2)
	{
		error(errConfig, -1, "Bad '{}' config file command ({}:{})", cmdName, fileName, line);
		return;
	}
	const auto& tok = tokens.at(1);
	if (tok.size() == 0)
	{
		error(errConfig, -1, "Bad '{}' config file command ({}:{})", cmdName, fileName, line);
		return;
	}
	int i;
	if (tok.at(0) == '-')
		i = 1;
	else
		i = 0;
	for (; i < tok.size(); ++i)
	{
		if (tok.at(i) < '0' || tok.at(i) > '9')
		{
			error(errConfig, -1, "Bad '{}' config file command ({}:{})", cmdName, fileName, line);
			return;
		}
	}
	val->store(atoi(tok.c_str()));
}

void GlobalParams::parseFloat(const char* cmdName, std::atomic<double>* val, const VEC_STR& tokens, const std::string& fileName, int line)
{
	if (tokens.size() != 2)
	{
		error(errConfig, -1, "Bad '{}' config file command ({}:{})", cmdName, fileName, line);
		return;
	}
	const auto& tok = tokens.at(1);
	if (tok.size() == 0)
	{
		error(errConfig, -1, "Bad '{}' config file command ({}:{})", cmdName, fileName, line);
		return;
	}
	int i;
	if (tok.at(0) == '-')
		i = 1;
	else
		i = 0;
	for (; i < tok.size(); ++i)
	{
		if (!((tok.at(i) >= '0' && tok.at(i) <= '9') || tok.at(i) == '.'))
		{
			error(errConfig, -1, "Bad '{}' config file command ({}:{})", cmdName, fileName, line);
			return;
		}
	}
	val->store(atof(tok.c_str()));
}

GlobalParams::~GlobalParams()
{
	delete unicodeRemapping;
	delete sysFonts;
	delete cidToUnicodeCache;
	delete unicodeToUnicodeCache;
	delete unicodeMapCache;
	delete cMapCache;

#if MULTITHREADED
	gDestroyMutex(&mutex);
	gDestroyMutex(&unicodeMapCacheMutex);
	gDestroyMutex(&cMapCacheMutex);
#endif
}

//------------------------------------------------------------------------

void GlobalParams::setBaseDir(const char* dir)
{
	baseDir = dir;
}

#ifdef _WIN32
static void getWinFontDir(char* winFontDir)
{
	HMODULE shell32Lib;
	BOOL(__stdcall * SHGetSpecialFolderPathFunc)
	(HWND hwndOwner, LPSTR lpszPath, int nFolder, BOOL fCreate);
	char* p;
	int   i;

	// SHGetSpecialFolderPath isn't available in older versions of
	// shell32.dll (Win95 and WinNT4), so do a dynamic load
	winFontDir[0] = '\0';
	if ((shell32Lib = LoadLibraryA("shell32.dll")))
	{
		if ((SHGetSpecialFolderPathFunc = (BOOL(__stdcall*)(HWND hwndOwner, LPSTR lpszPath, int nFolder, BOOL fCreate))GetProcAddress(shell32Lib, "SHGetSpecialFolderPathA")))
		{
			if (!(*SHGetSpecialFolderPathFunc)(nullptr, winFontDir, CSIDL_FONTS, FALSE))
				winFontDir[0] = '\0';
			// kludge: Terminal Server changes CSIDL_FONTS to something like
			// "C:\Users\whatever\Windows\Fonts", which doesn't actually
			// contain any fonts -- kill that, so we hit the fallback code
			// below.
			for (p = winFontDir; *p; ++p)
			{
				if (!strncasecmp(p, "\\Users\\", 7))
				{
					winFontDir[0] = '\0';
					break;
				}
			}
		}
		FreeLibrary(shell32Lib);
	}
	// if something went wrong, or we're on a Terminal Server, try using
	// %SYSTEMROOT%\Fonts
	if (!winFontDir[0])
	{
		GetSystemWindowsDirectoryA(winFontDir, MAX_PATH - 6);
		winFontDir[MAX_PATH - 7] = '\0';
		i                        = (int)strlen(winFontDir);
		if (winFontDir[i - 1] != '\\')
			winFontDir[i++] = '\\';
		strcpy(winFontDir + i, "Fonts");
	}
}
#endif

#ifdef __APPLE__
// Apple dfonts and ttc fonts seem to randomly and interchangeably use
// space and hyphen, and sometimes drop the separator entirely.
static bool macFontNameMatches(const std::string& name1, const char* name2)
{
	const char *p1, *p2;
	char        c1, c2;

	p1 = name1.c_str();
	p2 = name2;
	while (*p1 && *p2)
	{
		c1 = *p1;
		c2 = *p2;
		if (c2 == ' ')
		{
			// * space or hyphen matches a space in the pattern
			// * separators can also be dropped, in which case we move to
			//   the next char in the pattern
			if (c1 == ' ' || c1 == '-')
				++p1;
		}
		else
		{
			if (c1 != c2)
				return false;
			++p1;
		}
		++p2;
	}
	if (*p1 || *p2)
		return false;
	return true;
}
#endif

void GlobalParams::setupBaseFonts(const char* dir)
{
#ifdef _WIN32
	char winFontDir[MAX_PATH];
#endif
#ifdef __APPLE__
	static const char* macFontExts[3] = { "dfont", "ttc", "ttf" };
#endif
	FILE* f;

#ifdef _WIN32
	getWinFontDir(winFontDir);
#endif
#ifdef __APPLE__
	GList* dfontFontNames = nullptr;
#endif
	for (const auto& tab : displayFontTabs)
	{
		if (fontFiles.contains(tab.name)) continue;
		std::string fontName = tab.name;
		std::string fileName;
		int         fontNum = 0;
		if (dir)
		{
			fileName = appendToPath(dir, tab.t1FileName.c_str());
			if ((f = fopen(fileName.c_str(), "rb")))
				fclose(f);
			else
				fileName.clear();
		}
#ifdef _WIN32
		if (fileName.empty() && winFontDir[0] && tab.ttFileName.size())
		{
			fileName = appendToPath(winFontDir, tab.ttFileName.c_str());
			if ((f = fopen(fileName.c_str(), "rb")))
				fclose(f);
			else
				fileName.clear();
		}
#endif
#ifdef __APPLE__
		// Check for Mac OS X system fonts.
		const char* s = tab.macFileName.c_str();
		if (dfontFontNames && i > 0 && (!s || strcmp(s, displayFontTab[i - 1].macFileName)))
		{
			deleteGList(dfontFontNames, GString);
			dfontFontNames = nullptr;
		}
		if (fileName.empty() && s)
		{
			for (int j = 0; j < 3; ++j)
			{
				fileName = fmt::format("{}/{}.{}", macSystemFontPath, s, macFontExts[j]);
				if (!(f = fopen(fileName.c_str(), "rb")))
				{
					fileName.clear();
				}
				else
				{
					fclose(f);
					bool found = false;
					// for .dfont or .ttc, we need to scan the font list
					if (j < 2)
					{
						if (!dfontFontNames)
							dfontFontNames = FoFiIdentifier::getFontList(fileName.c_str());
						if (dfontFontNames)
						{
							for (int k = 0; k < dfontFontNames.size(); ++k)
							{
								if (macFontNameMatches((GString*)dfontFontNames->get(k), tab.macFontName))
								{
									fontNum = k;
									found   = true;
									break;
								}
							}
						}
						// for .ttf, we just use the font
					}
					else
					{
						found = true;
					}
					if (!found) fileName.clear();
					break;
				}
			}
		}
#endif // __APPLE__
       // On Linux, this checks the "standard" ghostscript font directories.
	   // On Windows, it checks the "standard" system font directories (because SHGetSpecialFolderPath(CSIDL_FONTS) doesn't work on Win 2k Server or Win2003 Server, or with older versions of shell32.dll).
#ifdef _WIN32
		const char* s = tab.ttFileName.c_str();
#else
		const char* s = tab.t1FileName.c_str();
#endif
		if (fileName.empty() && s)
		{
			for (int j = 0; fileName.empty() && displayFontDirs[j]; ++j)
			{
				fileName = appendToPath(displayFontDirs[j], s);
				if ((f = fopen(fileName.c_str(), "rb")))
					fclose(f);
				else
					fileName.clear();
			}
		}
		if (fileName.empty())
		{
			fontName.clear();
			continue;
		}
		base14SysFonts.emplace(fontName, Base14FontInfo(fileName, fontNum, 0));
	}
#ifdef __APPLE__
	if (dfontFontNames)
		deleteGList(dfontFontNames, GString);
#endif
	for (const auto& tab : displayFontTabs)
	{
		if (!base14SysFonts.contains(tab.name) && !fontFiles.contains(tab.name))
		{
			bool found = false;
			if (tab.obliqueFont.size())
			{
				if (const auto& it = base14SysFonts.find(tab.obliqueFont); it != base14SysFonts.end())
				{
					const auto& base14 = it->second;
					base14SysFonts.emplace(tab.name, Base14FontInfo(base14.fileName, base14.fontNum, tab.obliqueFactor));
					found = true;
				}
			}
			if (!found) error(errConfig, -1, "No display font for '{}'", tab.name);
		}
	}
#ifdef _WIN32
	if (winFontDir[0])
		sysFonts->scanWindowsFonts(winFontDir);
#endif
#if HAVE_FONTCONFIG
	sysFonts->scanFontconfigFonts();
#endif
}

//------------------------------------------------------------------------
// accessors
//------------------------------------------------------------------------

CharCode GlobalParams::getMacRomanCharCode(const std::string& charName)
{
	// no need to lock - macRomanReverseMap is constant
	return FindDefault(macRomanReverseMap, charName, 0);
}

std::string GlobalParams::getBaseDir()
{
	return baseDir;
}

Unicode GlobalParams::mapNameToUnicode(const std::string& charName)
{
	// no need to lock - nameToUnicode is constant
	return FindDefault(nameToUnicode, charName, 0);
}

UnicodeMap* GlobalParams::getResidentUnicodeMap(const std::string& encodingName)
{
	std::lock_guard<std::mutex> lock(mtx);
	if (const auto& it = residentUnicodeMaps.find(encodingName); it != residentUnicodeMaps.end())
	{
		auto map = &it->second;
		map->incRefCnt();
		return map;
	}
	return nullptr;
}

FILE* GlobalParams::getUnicodeMapFile(const std::string& encodingName)
{
	std::lock_guard<std::mutex> lock(mtx);

	if (const auto fileName = FindDefault(unicodeMaps, encodingName, ""); fileName.size())
		return openFile(fileName.c_str(), "r");
	else
		return nullptr;
}

FILE* GlobalParams::findCMapFile(const std::string& collection, const std::string& cMapName)
{
	std::lock_guard<std::mutex> lock(mtx);

	if (auto it = cMapDirs.find(collection); it != cMapDirs.end())
	{
		auto& list = it->second;
		for (const auto& dir : list)
		{
			const auto fileName = appendToPath(dir, cMapName.c_str());
			if (FILE* f = openFile(fileName.c_str(), "r")) return f;
		}
	}
	return nullptr;
}

FILE* GlobalParams::findToUnicodeFile(const std::string& name)
{
	std::lock_guard<std::mutex> lock(mtx);

	for (const auto& dir : toUnicodeDirs)
	{
		const auto fileName = appendToPath(dir, name.c_str());
		if (FILE* f = openFile(fileName.c_str(), "r")) return f;
	}
	return nullptr;
}

UnicodeRemapping* GlobalParams::getUnicodeRemapping()
{
	std::lock_guard<std::mutex> lock(mtx);
	return unicodeRemapping;
}

std::filesystem::path GlobalParams::findFontFile(const std::string& fontName)
{
	std::lock_guard<std::mutex> lock(mtx);

	const VEC_STR& exts = { ".pfa", ".pfb", ".ttf", ".ttc", ".otf" };

	if (const auto path = FindDefault(fontFiles, fontName, ""); path.size())
		return path;

	for (const auto& fontDir : fontDirs)
	{
		for (const auto& ext : exts)
		{
			const auto path = std::filesystem::path(fontDir) / (fontName + ext);
			if (std::filesystem::exists(path)) return path;
		}
	}

	return {};
}

std::filesystem::path GlobalParams::findBase14FontFile(const std::string& fontName, int* fontNum, double* oblique)
{
	std::lock_guard<std::mutex> lock(mtx);

	std::string path;
	if (const auto& it = base14SysFonts.find(fontName); it != base14SysFonts.end())
	{
		const auto& fi = it->second;
		path           = fi.fileName;
		*fontNum       = fi.fontNum;
		*oblique       = fi.oblique;
		return path;
	}
	*fontNum = 0;
	*oblique = 0;
	return findFontFile(fontName);
}

std::filesystem::path GlobalParams::findSystemFontFile(const std::string& fontName, SysFontType* type, int* fontNum)
{
	std::lock_guard<std::mutex> lock(mtx);

	std::string path;
	if (const auto fi = sysFonts->find(fontName))
	{
		path     = fi->path;
		*type    = fi->type;
		*fontNum = fi->fontNum;
	}
	return path;
}

std::string GlobalParams::findCCFontFile(const std::string& collection)
{
	std::lock_guard<std::mutex> lock(mtx);
	return FindDefault(ccFontFiles, collection, "");
}

int GlobalParams::getPSPaperWidth()
{
	return psPaperWidth;
}

int GlobalParams::getPSPaperHeight()
{
	return psPaperHeight;
}

void GlobalParams::getPSImageableArea(int* llx, int* lly, int* urx, int* ury)
{
	std::lock_guard<std::mutex> lock(mtx);
	*llx = psImageableLLX;
	*lly = psImageableLLY;
	*urx = psImageableURX;
	*ury = psImageableURY;
}

bool GlobalParams::getPSCrop()
{
	return psCrop;
}

bool GlobalParams::getPSUseCropBoxAsPage()
{
	return psUseCropBoxAsPage;
}

bool GlobalParams::getPSExpandSmaller()
{
	return psExpandSmaller;
}

bool GlobalParams::getPSShrinkLarger()
{
	return psShrinkLarger;
}

bool GlobalParams::getPSCenter()
{
	return psCenter;
}

bool GlobalParams::getPSDuplex()
{
	return psDuplex;
}

PSLevel GlobalParams::getPSLevel()
{
	std::lock_guard<std::mutex> lock(mtx);
	return psLevel;
}

std::string GlobalParams::getPSResidentFont(const std::string& fontName)
{
	std::lock_guard<std::mutex> lock(mtx);
	return FindDefault(psResidentFonts, fontName, "");
}

VEC_STR GlobalParams::getPSResidentFonts()
{
	std::lock_guard<std::mutex> lock(mtx);

	VEC_STR names;
	for (const auto& [pdfName, psName] : psResidentFonts)
		names.push_back(psName);

	return names;
}

PSFontParam16* GlobalParams::getPSResidentFont16(const std::string& fontName, int wMode)
{
	std::lock_guard<std::mutex> lock(mtx);

	for (auto& font : psResidentFonts16)
		if (font.name == fontName && font.wMode == wMode) return &font;
	return nullptr;
}

PSFontParam16* GlobalParams::getPSResidentFontCC(const std::string& collection, int wMode)
{
	std::lock_guard<std::mutex> lock(mtx);

	for (auto& font : psResidentFontsCC)
		if (font.name == collection && font.wMode == wMode) return &font;
	return nullptr;
}

bool GlobalParams::getPSEmbedType1()
{
	return psEmbedType1;
}

bool GlobalParams::getPSEmbedTrueType()
{
	return psEmbedTrueType;
}

bool GlobalParams::getPSEmbedCIDPostScript()
{
	return psEmbedCIDPostScript;
}

bool GlobalParams::getPSEmbedCIDTrueType()
{
	return psEmbedCIDTrueType;
}

bool GlobalParams::getPSFontPassthrough()
{
	return psFontPassthrough;
}

bool GlobalParams::getPSPreload()
{
	return psPreload;
}

bool GlobalParams::getPSOPI()
{
	return psOPI;
}

bool GlobalParams::getPSASCIIHex()
{
	return psASCIIHex;
}

bool GlobalParams::getPSLZW()
{
	return psLZW;
}

bool GlobalParams::getPSUncompressPreloadedImages()
{
	return psUncompressPreloadedImages;
}

double GlobalParams::getPSMinLineWidth()
{
	return psMinLineWidth;
}

double GlobalParams::getPSRasterResolution()
{
	return psRasterResolution;
}

bool GlobalParams::getPSRasterMono()
{
	return psRasterMono;
}

int GlobalParams::getPSRasterSliceSize()
{
	return psRasterSliceSize;
}

bool GlobalParams::getPSAlwaysRasterize()
{
	return psAlwaysRasterize;
}

bool GlobalParams::getPSNeverRasterize()
{
	return psNeverRasterize;
}

std::string GlobalParams::getTextEncodingName()
{
	std::lock_guard<std::mutex> lock(mtx);
	return textEncoding;
}

VEC_STR GlobalParams::getAvailableTextEncodings()
{
	std::lock_guard<std::mutex> lock(mtx);

	VEC_STR list;
	for (const auto& [key, value] : residentUnicodeMaps)
		list.push_back(key);

	for (const auto& [key, value] : unicodeMaps)
		list.push_back(key);
	return list;
}

EndOfLineKind GlobalParams::getTextEOL()
{
	std::lock_guard<std::mutex> lock(mtx);
	return textEOL;
}

bool GlobalParams::getTextPageBreaks()
{
	return textPageBreaks;
}

bool GlobalParams::getTextKeepTinyChars()
{
	return textKeepTinyChars;
}

std::string GlobalParams::getInitialZoom()
{
	std::lock_guard<std::mutex> lock(mtx);
	return initialZoom;
}

int GlobalParams::getDefaultFitZoom()
{
	return defaultFitZoom;
}

double GlobalParams::getZoomScaleFactor()
{
	return zoomScaleFactor;
}

VEC_STR GlobalParams::getZoomValues()
{
	std::lock_guard<std::mutex> lock(mtx);
	return zoomValues;
}

std::string GlobalParams::getInitialDisplayMode()
{
	std::lock_guard<std::mutex> lock(mtx);
	return initialDisplayMode;
}

bool GlobalParams::getInitialToolbarState()
{
	return initialToolbarState;
}

bool GlobalParams::getInitialSidebarState()
{
	return initialSidebarState;
}

int GlobalParams::getInitialSidebarWidth()
{
	return initialSidebarWidth;
}

std::string GlobalParams::getInitialSelectMode()
{
	std::lock_guard<std::mutex> lock(mtx);
	return initialSelectMode;
}

int GlobalParams::getMaxTileWidth()
{
	return maxTileWidth;
}

int GlobalParams::getMaxTileHeight()
{
	return maxTileHeight;
}

int GlobalParams::getTileCacheSize()
{
	return tileCacheSize;
}

int GlobalParams::getWorkerThreads()
{
	return workerThreads;
}

bool GlobalParams::getEnableFreeType()
{
	return enableFreeType;
}

bool GlobalParams::getDisableFreeTypeHinting()
{
	return disableFreeTypeHinting;
}

bool GlobalParams::getAntialias()
{
	return antialias;
}

bool GlobalParams::getVectorAntialias()
{
	return vectorAntialias;
}

bool GlobalParams::getImageMaskAntialias()
{
	return imageMaskAntialias;
}

bool GlobalParams::getAntialiasPrinting()
{
	return antialiasPrinting;
}

StrokeAdjustMode GlobalParams::getStrokeAdjust()
{
	std::lock_guard<std::mutex> lock(mtx);
	return strokeAdjust;
}

ScreenType GlobalParams::getScreenType()
{
	std::lock_guard<std::mutex> lock(mtx);
	return screenType;
}

int GlobalParams::getScreenSize()
{
	return screenSize;
}

int GlobalParams::getScreenDotRadius()
{
	return screenDotRadius;
}

double GlobalParams::getScreenGamma()
{
	return screenGamma;
}

double GlobalParams::getScreenBlackThreshold()
{
	return screenBlackThreshold;
}

double GlobalParams::getScreenWhiteThreshold()
{
	return screenWhiteThreshold;
}

double GlobalParams::getMinLineWidth()
{
	return minLineWidth;
}

bool GlobalParams::getEnablePathSimplification()
{
	return enablePathSimplification;
}

bool GlobalParams::getDrawAnnotations()
{
	return drawAnnotations;
}

bool GlobalParams::getDrawFormFields()
{
	return drawFormFields;
}

bool GlobalParams::getEnableXFA()
{
	return enableXFA;
}

std::string GlobalParams::getPaperColor()
{
	std::lock_guard<std::mutex> lock(mtx);
	return paperColor;
}

std::string GlobalParams::getMatteColor()
{
	std::lock_guard<std::mutex> lock(mtx);
	return matteColor;
}

std::string GlobalParams::getFullScreenMatteColor()
{
	std::lock_guard<std::mutex> lock(mtx);
	return fullScreenMatteColor;
}

std::string GlobalParams::getSelectionColor()
{
	std::lock_guard<std::mutex> lock(mtx);
	return selectionColor;
}

bool GlobalParams::getReverseVideoInvertImages()
{
	return reverseVideoInvertImages;
}

bool GlobalParams::getAllowLinksToChangeZoom()
{
	return allowLinksToChangeZoom;
}

std::string GlobalParams::getDefaultPrinter()
{
	std::lock_guard<std::mutex> lock(mtx);
	return defaultPrinter;
}

bool GlobalParams::getMapNumericCharNames()
{
	return mapNumericCharNames;
}

bool GlobalParams::getMapUnknownCharNames()
{
	return mapUnknownCharNames;
}

bool GlobalParams::getMapExtTrueTypeFontsViaUnicode()
{
	return mapExtTrueTypeFontsViaUnicode;
}

bool GlobalParams::getUseTrueTypeUnicodeMapping()
{
	return useTrueTypeUnicodeMapping;
}

bool GlobalParams::getIgnoreWrongSizeToUnicode()
{
	return ignoreWrongSizeToUnicode;
}

bool GlobalParams::isDroppedFont(const std::string& fontName)
{
	std::lock_guard<std::mutex> lock(mtx);
	return droppedFonts.contains(fontName);
}

bool GlobalParams::getSeparateRotatedText()
{
	return separateRotatedText;
}

VEC_STR GlobalParams::getKeyBinding(int code, int mods, int context)
{
	std::lock_guard<std::mutex> lock(mtx);

	// for ASCII chars, ignore the shift modifier
	const int modMask = (code >= 0x21 && code <= 0xff) ? ~xpdfKeyModShift : ~0;

	for (const auto& binding : keyBindings)
		if (binding.code == code && (binding.mods & modMask) == (mods & modMask) && (~binding.context | context) == ~0)
			return binding.cmds;

	return {};
}

std::vector<KeyBinding> GlobalParams::getAllKeyBindings()
{
	std::lock_guard<std::mutex> lock(mtx);
	return keyBindings;
}

int GlobalParams::getNumPopupMenuCmds()
{
	std::lock_guard<std::mutex> lock(mtx);
	return TO_INT(popupMenuCmds.size());
}

PopupMenuCmd* GlobalParams::getPopupMenuCmd(int idx)
{
	std::lock_guard<std::mutex> lock(mtx);
	return (idx < popupMenuCmds.size()) ? &popupMenuCmds[idx] : nullptr;
}

std::string GlobalParams::getPagesFile()
{
	std::lock_guard<std::mutex> lock(mtx);
	return pagesFile;
}

std::string GlobalParams::getTabStateFile()
{
	std::lock_guard<std::mutex> lock(mtx);
	return tabStateFile;
}

std::string GlobalParams::getSessionFile()
{
	std::lock_guard<std::mutex> lock(mtx);
	return sessionFile;
}

bool GlobalParams::getSavePageNumbers()
{
	return savePageNumbers;
}

bool GlobalParams::getSaveSessionOnQuit()
{
	return saveSessionOnQuit;
}

bool GlobalParams::getPrintCommands()
{
	return printCommands;
}

bool GlobalParams::getPrintStatusInfo()
{
	return printStatusInfo;
}

bool GlobalParams::getErrQuiet()
{
	// no locking -- this function may get called from inside a locked section
	return errQuiet;
}

std::string GlobalParams::getDebugLogFile()
{
	std::lock_guard<std::mutex> lock(mtx);
	return debugLogFile;
}

void GlobalParams::debugLogPrintf(const char* fmt, ...)
{
	FILE*     f;
	time_t    t;
	struct tm tm;
	va_list   args;

	const std::string path = getDebugLogFile();
	if (path.empty()) return;
	bool needClose = false;
	if (path == "-")
	{
		f = stdout;
	}
	else if (path == "+")
	{
		f = stderr;
	}
	else
	{
		f         = fopen(path.c_str(), "a");
		needClose = true;
	}
	if (!f)
		return;
	t = time(nullptr);
#ifdef _WIN32
	localtime_s(&tm, &t);
#else
	localtime_r(&t, &tm);
#endif
	fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d] ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	va_start(args, fmt);
	vfprintf(f, fmt, args);
	va_end(args);
	fflush(f);
	if (needClose) fclose(f);
}

CharCodeToUnicode* GlobalParams::getCIDToUnicode(const std::string& collection)
{
	std::lock_guard<std::mutex> lock(mtx);

	if (auto ctu = cidToUnicodeCache->getCharCodeToUnicode(collection))
		return ctu;

	if (const auto& it = cidToUnicodes.find(collection); it != cidToUnicodes.end())
	{
		if (auto ctu = CharCodeToUnicode::parseCIDToUnicode(it->second, collection))
		{
			cidToUnicodeCache->add(ctu);
			return ctu;
		}
	}

	return nullptr;
}

CharCodeToUnicode* GlobalParams::getUnicodeToUnicode(const std::string& fontName)
{
	std::lock_guard<std::mutex> lock(mtx);

	for (const auto& [fontPattern, fileName] : unicodeToUnicodes)
	{
		if (fontName.find(fontPattern) != std::string::npos)
		{
			auto ctu = unicodeToUnicodeCache->getCharCodeToUnicode(fileName);
			if (!ctu)
			{
				if (ctu = CharCodeToUnicode::parseUnicodeToUnicode(fileName))
					unicodeToUnicodeCache->add(ctu);
			}
			if (ctu) return ctu;
		}
	}

	return nullptr;
}

UnicodeMap* GlobalParams::getUnicodeMap(const std::string& encodingName)
{
	return getUnicodeMap2(encodingName);
}

UnicodeMap* GlobalParams::getUnicodeMap2(const std::string& encodingName)
{
	UnicodeMap* map;

	if (!(map = getResidentUnicodeMap(encodingName)))
	{
		lockUnicodeMapCache;
		map = unicodeMapCache->getUnicodeMap(encodingName);
		unlockUnicodeMapCache;
	}
	return map;
}

CMap* GlobalParams::getCMap(const std::string& collection, const std::string& cMapName)
{
	CMap* cMap;

	lockCMapCache;
	cMap = cMapCache->getCMap(collection, cMapName);
	unlockCMapCache;
	return cMap;
}

UnicodeMap* GlobalParams::getTextEncoding()
{
	return getUnicodeMap2(textEncoding);
}

//------------------------------------------------------------------------
// functions to set parameters
//------------------------------------------------------------------------

void GlobalParams::addUnicodeRemapping(Unicode in, Unicode* out, int len)
{
	unicodeRemapping->addRemapping(in, out, len);
}

void GlobalParams::addFontFile(const std::string& fontName, const std::string& path)
{
	std::lock_guard<std::mutex> lock(mtx);
	fontFiles.emplace(fontName, path);
}

bool GlobalParams::setPSPaperSize(std::string_view size)
{
	std::lock_guard<std::mutex> lock(mtx);
	if (size == "match")
	{
		psPaperWidth = psPaperHeight = -1;
	}
	else if (size == "letter")
	{
		psPaperWidth  = 612;
		psPaperHeight = 792;
	}
	else if (size == "legal")
	{
		psPaperWidth  = 612;
		psPaperHeight = 1008;
	}
	else if (size == "A4")
	{
		psPaperWidth  = 595;
		psPaperHeight = 842;
	}
	else if (size == "A3")
	{
		psPaperWidth  = 842;
		psPaperHeight = 1190;
	}
	else
	{
		return false;
	}
	psImageableLLX = 0;
	psImageableLLY = 0;
	psImageableURX.store(psPaperWidth.load());
	psImageableURY.store(psPaperHeight.load());
	return true;
}

void GlobalParams::setPSPaperWidth(int width)
{
	std::lock_guard<std::mutex> lock(mtx);
	psPaperWidth   = width;
	psImageableLLX = 0;
	psImageableURX = width;
}

void GlobalParams::setPSPaperHeight(int height)
{
	std::lock_guard<std::mutex> lock(mtx);
	psPaperHeight  = height;
	psImageableLLY = 0;
	psImageableURY = height;
}

void GlobalParams::setPSImageableArea(int llx, int lly, int urx, int ury)
{
	std::lock_guard<std::mutex> lock(mtx);
	psImageableLLX = llx;
	psImageableLLY = lly;
	psImageableURX = urx;
	psImageableURY = ury;
}

void GlobalParams::setPSCrop(bool crop)
{
	psCrop = crop;
}

void GlobalParams::setPSUseCropBoxAsPage(bool crop)
{
	psUseCropBoxAsPage = crop;
}

void GlobalParams::setPSExpandSmaller(bool expand)
{
	psExpandSmaller = expand;
}

void GlobalParams::setPSShrinkLarger(bool shrink)
{
	psShrinkLarger = shrink;
}

void GlobalParams::setPSCenter(bool center)
{
	psCenter = center;
}

void GlobalParams::setPSDuplex(bool duplex)
{
	psDuplex = duplex;
}

void GlobalParams::setPSLevel(PSLevel level)
{
	std::lock_guard<std::mutex> lock(mtx);
	psLevel = level;
}

void GlobalParams::setPSEmbedType1(bool embed)
{
	psEmbedType1 = embed;
}

void GlobalParams::setPSEmbedTrueType(bool embed)
{
	psEmbedTrueType = embed;
}

void GlobalParams::setPSEmbedCIDPostScript(bool embed)
{
	psEmbedCIDPostScript = embed;
}

void GlobalParams::setPSEmbedCIDTrueType(bool embed)
{
	psEmbedCIDTrueType = embed;
}

void GlobalParams::setPSFontPassthrough(bool passthrough)
{
	psFontPassthrough = passthrough;
}

void GlobalParams::setPSPreload(bool preload)
{
	psPreload = preload;
}

void GlobalParams::setPSOPI(bool opi)
{
	psOPI = opi;
}

void GlobalParams::setPSASCIIHex(bool hex)
{
	psASCIIHex = hex;
}

void GlobalParams::setTextEncoding(std::string_view encodingName)
{
	std::lock_guard<std::mutex> lock(mtx);
	textEncoding = encodingName;
}

bool GlobalParams::setTextEOL(std::string_view s)
{
	std::lock_guard<std::mutex> lock(mtx);
	if (s == "unix")
		textEOL = eolUnix;
	else if (s == "dos")
		textEOL = eolDOS;
	else if (s == "mac")
		textEOL = eolMac;
	else
		return false;
	return true;
}

void GlobalParams::setTextPageBreaks(bool pageBreaks)
{
	textPageBreaks = pageBreaks;
}

void GlobalParams::setTextKeepTinyChars(bool keep)
{
	textKeepTinyChars = keep;
}

void GlobalParams::setInitialZoom(std::string_view s)
{
	std::lock_guard<std::mutex> lock(mtx);
	initialZoom = s;
}

void GlobalParams::setDefaultFitZoom(int z)
{
	defaultFitZoom = z;
}

bool GlobalParams::setEnableFreeType(const char* s)
{
	std::lock_guard<std::mutex> lock(mtx);
	return parseYesNo2(s, &enableFreeType);
}

bool GlobalParams::setAntialias(const char* s)
{
	std::lock_guard<std::mutex> lock(mtx);
	return parseYesNo2(s, &antialias);
}

bool GlobalParams::setVectorAntialias(const char* s)
{
	std::lock_guard<std::mutex> lock(mtx);
	return parseYesNo2(s, &vectorAntialias);
}

void GlobalParams::setScreenType(ScreenType t)
{
	std::lock_guard<std::mutex> lock(mtx);
	screenType = t;
}

void GlobalParams::setScreenSize(int size)
{
	screenSize = size;
}

void GlobalParams::setScreenDotRadius(int r)
{
	screenDotRadius = r;
}

void GlobalParams::setScreenGamma(double gamma)
{
	screenGamma = gamma;
}

void GlobalParams::setScreenBlackThreshold(double thresh)
{
	screenBlackThreshold = thresh;
}

void GlobalParams::setScreenWhiteThreshold(double thresh)
{
	screenWhiteThreshold = thresh;
}

void GlobalParams::setDrawFormFields(bool draw)
{
	drawFormFields = draw;
}

void GlobalParams::setOverprintPreview(bool preview)
{
	overprintPreview = preview;
}

void GlobalParams::setMapNumericCharNames(bool map)
{
	mapNumericCharNames = map;
}

void GlobalParams::setMapUnknownCharNames(bool map)
{
	mapUnknownCharNames = map;
}

void GlobalParams::setMapExtTrueTypeFontsViaUnicode(bool map)
{
	mapExtTrueTypeFontsViaUnicode = map;
}

void GlobalParams::setTabStateFile(std::string_view tabStateFileA)
{
	std::lock_guard<std::mutex> lock(mtx);
	tabStateFile = tabStateFileA;
}

void GlobalParams::setSessionFile(std::string_view sessionFileA)
{
	std::lock_guard<std::mutex> lock(mtx);
	sessionFile = sessionFileA;
}

void GlobalParams::setPrintCommands(bool printCommandsA)
{
	printCommands = printCommandsA;
}

void GlobalParams::setPrintStatusInfo(bool printStatusInfoA)
{
	printStatusInfo = printStatusInfoA;
}

void GlobalParams::setErrQuiet(bool errQuietA)
{
	errQuiet = errQuietA;
}

#ifdef _WIN32
void GlobalParams::setWin32ErrorInfo(const char* func, DWORD code)
{
	if (tlsWin32ErrorInfo == TLS_OUT_OF_INDEXES) return;
	auto* errorInfo = (XpdfWin32ErrorInfo*)TlsGetValue(tlsWin32ErrorInfo);
	if (!errorInfo)
	{
		errorInfo = new XpdfWin32ErrorInfo();
		TlsSetValue(tlsWin32ErrorInfo, errorInfo);
	}
	errorInfo->func = func;
	errorInfo->code = code;
}

XpdfWin32ErrorInfo* GlobalParams::getWin32ErrorInfo()
{
	if (tlsWin32ErrorInfo == TLS_OUT_OF_INDEXES) return nullptr;
	auto* errorInfo = (XpdfWin32ErrorInfo*)TlsGetValue(tlsWin32ErrorInfo);
	if (!errorInfo)
	{
		errorInfo = new XpdfWin32ErrorInfo();
		TlsSetValue(tlsWin32ErrorInfo, errorInfo);
		errorInfo->func = nullptr;
		errorInfo->code = 0;
	}
	return errorInfo;
}
#endif
