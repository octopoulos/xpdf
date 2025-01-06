//========================================================================
//
// WebFont.cc
//
// Copyright 2019 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include "gmem.h"
#include "gmempp.h"
#include "FoFiTrueType.h"
#include "FoFiType1C.h"
#include "CharCodeToUnicode.h"
#include "WebFont.h"

WebFont::WebFont(GfxFont* gfxFontA, XRef* xref)
{
	gfxFont = gfxFontA;

	Ref id;
	if (gfxFont->getEmbeddedFontID(&id))
	{
		GfxFontType type = gfxFont->getType();
		if (type == fontTrueType || type == fontTrueTypeOT || type == fontCIDType2 || type == fontCIDType2OT)
		{
			if ((fontBuf = gfxFont->readEmbFontFile(xref, &fontLength)))
				ffTrueType = FoFiTrueType::make(fontBuf, fontLength, 0);
		}
		else if (type == fontType1C || type == fontCIDType0C)
		{
			if ((fontBuf = gfxFont->readEmbFontFile(xref, &fontLength)))
				ffType1C = FoFiType1C::make(fontBuf, fontLength);
		}
		else if (type == fontType1COT || type == fontCIDType0COT)
		{
			if ((fontBuf = gfxFont->readEmbFontFile(xref, &fontLength)))
				isOpenType = true;
		}
	}
}

WebFont::~WebFont()
{
	delete ffTrueType;
	delete ffType1C;
	gfree(fontBuf);
}

bool WebFont::canWriteTTF()
{
	return ffTrueType != nullptr;
}

bool WebFont::canWriteOTF()
{
	return !!ffType1C || isOpenType;
}

static void writeToFile(void* stream, const char* data, size_t len)
{
	fwrite(data, 1, len, (FILE*)stream);
}

static void writeToString(void* stream, const char* data, size_t len)
{
	((std::string*)stream)->append(data, len);
}

bool WebFont::generateTTF(FoFiOutputFunc outFunc, void* stream)
{
	int*     codeToGID;
	uint8_t* cmapTable;
	bool     freeCodeToGID;
	int      nCodes, cmapTableLength;

	if (!ffTrueType)
		return false;
	if (gfxFont->isCIDFont())
	{
		codeToGID = ((GfxCIDFont*)gfxFont)->getCIDToGID();
		nCodes    = ((GfxCIDFont*)gfxFont)->getCIDToGIDLen();
		if (!codeToGID) nCodes = ffTrueType->getNumGlyphs();
		freeCodeToGID = false;
	}
	else
	{
		codeToGID     = ((Gfx8BitFont*)gfxFont)->getCodeToGIDMap(ffTrueType);
		nCodes        = 256;
		freeCodeToGID = true;
	}
	cmapTable = makeUnicodeCmapTable(codeToGID, nCodes, &cmapTableLength);
	if (freeCodeToGID)
		gfree(codeToGID);
	if (!cmapTable)
		return false;
	ffTrueType->writeTTF(outFunc, stream, nullptr, nullptr, cmapTable, cmapTableLength);
	gfree(cmapTable);
	return true;
}

bool WebFont::writeTTF(const char* fontFilePath)
{
	FILE* out = fopen(fontFilePath, "wb");
	if (!out) return false;
	bool ret = generateTTF(writeToFile, out);
	fclose(out);
	return ret;
}

std::string WebFont::getTTFData()
{
	std::string s;
	if (!generateTTF(writeToString, (void*)&s)) return "";
	return s;
}

bool WebFont::generateOTF(FoFiOutputFunc outFunc, void* stream)
{
	int*      codeToGID;
	uint16_t* widths;
	uint8_t*  cmapTable;
	int       nCodes, nWidths, cmapTableLength;

	if (ffType1C)
	{
		if (gfxFont->getType() == fontType1C)
		{
			codeToGID = ((Gfx8BitFont*)gfxFont)->getCodeToGIDMap(ffType1C);
			if (!(cmapTable = makeUnicodeCmapTable(codeToGID, 256, &cmapTableLength)))
			{
				gfree(codeToGID);
				return false;
			}
			widths = makeType1CWidths(codeToGID, 256, &nWidths);
			gfree(codeToGID);
		}
		else
		{ // fontCIDType0C
			codeToGID = ffType1C->getCIDToGIDMap(&nCodes);
			if (!(cmapTable = makeUnicodeCmapTable(codeToGID, nCodes, &cmapTableLength)))
			{
				gfree(codeToGID);
				return false;
			}
			widths = makeCIDType0CWidths(codeToGID, nCodes, &nWidths);
			gfree(codeToGID);
		}
		ffType1C->convertToOpenType(outFunc, stream, nWidths, widths, cmapTable, cmapTableLength);
		gfree(cmapTable);
		gfree(widths);
	}
	else if (isOpenType)
	{
		outFunc(stream, fontBuf, fontLength);
	}
	else
	{
		return false;
	}

	return true;
}

bool WebFont::writeOTF(const char* fontFilePath)
{
	FILE* out = fopen(fontFilePath, "wb");
	if (!out)
		return false;
	bool ret = generateOTF(writeToFile, out);
	fclose(out);
	return ret;
}

std::string WebFont::getOTFData()
{
	std::string s;
	if (!generateOTF(writeToString, (void*)&s))
		return "";
	return s;
}

uint16_t* WebFont::makeType1CWidths(int* codeToGID, int nCodes, int* nWidths)
{
	uint16_t* widths;
	uint16_t  width;
	int       widthsLen, gid;

	widthsLen = ffType1C->getNumGlyphs();
	widths    = (uint16_t*)gmallocn(widthsLen, sizeof(uint16_t));
	for (int i = 0; i < widthsLen; ++i)
		widths[i] = 0;
	for (int i = 0; i < nCodes; ++i)
	{
		gid = codeToGID[i];
		if (gid < 0 || gid >= widthsLen)
			continue;
		width = (uint16_t)(((Gfx8BitFont*)gfxFont)->getWidth((uint8_t)i) * 1000 + 0.5);
		if (width == 0)
			continue;
		widths[gid] = width;
	}
	*nWidths = widthsLen;
	return widths;
}

uint16_t* WebFont::makeCIDType0CWidths(int* codeToGID, int nCodes, int* nWidths)
{
	uint16_t* widths;
	uint16_t  width;
	int       widthsLen, gid;

	widthsLen = ffType1C->getNumGlyphs();
	widths    = (uint16_t*)gmallocn(widthsLen, sizeof(uint16_t));
	for (int i = 0; i < widthsLen; ++i)
		widths[i] = 0;
	for (int i = 0; i < nCodes; ++i)
	{
		gid = codeToGID[i];
		if (gid < 0 || gid >= widthsLen)
			continue;
		width = (uint16_t)(((GfxCIDFont*)gfxFont)->getWidth((CID)i) * 1000 + 0.5);
		if (width == 0)
			continue;
		widths[gid] = width;
	}
	*nWidths = widthsLen;
	return widths;
}

uint8_t* WebFont::makeUnicodeCmapTable(int* codeToGID, int nCodes, int* unicodeCmapLength)
{
	int unicodeToGIDLength, nMappings;
	int nSegs;
	int glyphIdOffset, idRangeOffset;
	int start, end;

	int* unicodeToGID;
	if (!(unicodeToGID = makeUnicodeToGID(codeToGID, nCodes, &unicodeToGIDLength)))
		return nullptr;

	// count the valid code-to-glyph mappings, and the sequences of
	// consecutive valid mappings
	// (note: char code 65535 is used to mark the end of table)
	nMappings = 0;
	nSegs     = 1; // count the last segment, mapping 65535
	for (int c = 0; c < unicodeToGIDLength && c <= 65534; ++c)
	{
		if (unicodeToGID[c])
		{
			++nMappings;
			if (c == 0 || !unicodeToGID[c - 1])
				++nSegs;
		}
	}

	int i             = 1;
	int entrySelector = 0;
	while (2 * i <= nSegs)
	{
		i *= 2;
		++entrySelector;
	}
	const int searchRange = 1 << (entrySelector + 1);
	const int rangeShift  = 2 * nSegs - searchRange;
	const int len         = 28 + nSegs * 8 + nMappings * 2;
	uint8_t*  cmapTable   = (uint8_t*)gmalloc(len);

	// header
	cmapTable[0]  = 0x00; // table version
	cmapTable[1]  = 0x00;
	cmapTable[2]  = 0x00; // number of cmaps
	cmapTable[3]  = 0x01;
	cmapTable[4]  = 0x00; // platform[0]
	cmapTable[5]  = 0x03;
	cmapTable[6]  = 0x00; // encoding[0]
	cmapTable[7]  = 0x01;
	cmapTable[8]  = 0x00; // offset[0]
	cmapTable[9]  = 0x00;
	cmapTable[10] = 0x00;
	cmapTable[11] = 0x0c;

	// table info
	cmapTable[12]                 = 0x00; // cmap format
	cmapTable[13]                 = 0x04;
	cmapTable[14]                 = (uint8_t)((len - 12) >> 8); // cmap length
	cmapTable[15]                 = (uint8_t)(len - 12);
	cmapTable[16]                 = 0x00; // cmap version
	cmapTable[17]                 = 0x00;
	cmapTable[18]                 = (uint8_t)(nSegs >> 7); // segCountX2
	cmapTable[19]                 = (uint8_t)(nSegs << 1);
	cmapTable[20]                 = (uint8_t)(searchRange >> 8); // searchRange
	cmapTable[21]                 = (uint8_t)searchRange;
	cmapTable[22]                 = (uint8_t)(entrySelector >> 8); // entrySelector
	cmapTable[23]                 = (uint8_t)entrySelector;
	cmapTable[24]                 = (uint8_t)(rangeShift >> 8); // rangeShift
	cmapTable[25]                 = (uint8_t)rangeShift;
	cmapTable[26 + nSegs * 2]     = 0; // reservedPad
	cmapTable[26 + nSegs * 2 + 1] = 0;

	i             = 0;
	glyphIdOffset = 28 + nSegs * 8;
	for (int c = 0; c < unicodeToGIDLength && c <= 65534; ++c)
	{
		if (unicodeToGID[c])
		{
			if (c == 0 || !unicodeToGID[c - 1])
			{
				start                                 = c;
				cmapTable[28 + nSegs * 2 + i * 2]     = (uint8_t)(start >> 8);
				cmapTable[28 + nSegs * 2 + i * 2 + 1] = (uint8_t)start;
				cmapTable[28 + nSegs * 4 + i * 2]     = (uint8_t)0; // idDelta
				cmapTable[28 + nSegs * 4 + i * 2 + 1] = (uint8_t)0;
				idRangeOffset                         = glyphIdOffset - (28 + nSegs * 6 + i * 2);
				cmapTable[28 + nSegs * 6 + i * 2]     = (uint8_t)(idRangeOffset >> 8);
				cmapTable[28 + nSegs * 6 + i * 2 + 1] = (uint8_t)idRangeOffset;
			}
			if (c == 65534 || !unicodeToGID[c + 1])
			{
				end                       = c;
				cmapTable[26 + i * 2]     = (uint8_t)(end >> 8);
				cmapTable[26 + i * 2 + 1] = (uint8_t)end;
				++i;
			}
			cmapTable[glyphIdOffset++] = (uint8_t)(unicodeToGID[c] >> 8);
			cmapTable[glyphIdOffset++] = (uint8_t)unicodeToGID[c];
		}
	}

	// last segment maps code 65535 to GID 0
	cmapTable[26 + i * 2]                 = (uint8_t)0xff; // end
	cmapTable[26 + i * 2 + 1]             = (uint8_t)0xff;
	cmapTable[28 + nSegs * 2 + i * 2]     = (uint8_t)0xff; // start
	cmapTable[28 + nSegs * 2 + i * 2 + 1] = (uint8_t)0xff;
	cmapTable[28 + nSegs * 4 + i * 2]     = (uint8_t)0; // idDelta
	cmapTable[28 + nSegs * 4 + i * 2 + 1] = (uint8_t)1;
	cmapTable[28 + nSegs * 6 + i * 2]     = (uint8_t)0; // idRangeOffset
	cmapTable[28 + nSegs * 6 + i * 2 + 1] = (uint8_t)0;

	gfree(unicodeToGID);

	*unicodeCmapLength = len;
	return cmapTable;
}

int* WebFont::makeUnicodeToGID(int* codeToGID, int nCodes, int* unicodeToGIDLength)
{
	CharCodeToUnicode* ctu;
	if (gfxFont->isCIDFont())
	{
		if (!(ctu = ((GfxCIDFont*)gfxFont)->getToUnicode()))
			return nullptr;
	}
	else
	{
		ctu = ((Gfx8BitFont*)gfxFont)->getToUnicode();
	}

	int  len          = 0;
	int  size         = 256;
	int* unicodeToGID = (int*)gmallocn(size, sizeof(int));
	memset(unicodeToGID, 0, size * sizeof(int));
	for (int c = 0; c < nCodes; ++c)
	{
		const int gid = codeToGID ? codeToGID[c] : c;
		if (gid < 0 || gid >= 65536)
			continue;
		Unicode   u[2];
		const int uLen = ctu->mapToUnicode(c, u, 2);
		if (uLen != 1)
			continue;
		if (u[0] >= 65536)
		{ // sanity check
			continue;
		}
		if ((int)u[0] >= size)
		{
			int newSize = 2 * size;
			while ((int)u[0] >= newSize)
				newSize *= 2;
			unicodeToGID = (int*)greallocn(unicodeToGID, newSize, sizeof(int));
			memset(unicodeToGID + size, 0, (newSize - size) * sizeof(int));
			size = newSize;
		}
		unicodeToGID[u[0]] = gid;
		if ((int)u[0] >= len)
			len = u[0] + 1;
	}

	ctu->decRefCnt();

	*unicodeToGIDLength = len;
	return unicodeToGID;
}
