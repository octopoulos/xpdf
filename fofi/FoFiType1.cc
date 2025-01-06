//========================================================================
//
// FoFiType1.cc
//
// Copyright 1999-2003 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include <stdlib.h>
#include <string.h>
#include "gmem.h"
#include "gmempp.h"
#include "FoFiEncodings.h"
#include "FoFiType1.h"

//------------------------------------------------------------------------
// FoFiType1
//------------------------------------------------------------------------

FoFiType1* FoFiType1::make(char* fileA, int lenA)
{
	return new FoFiType1(fileA, lenA, false);
}

FoFiType1* FoFiType1::load(char* fileName)
{
	char* fileA;
	int   lenA;

	if (!(fileA = FoFiBase::readFile(fileName, &lenA))) return nullptr;
	return new FoFiType1(fileA, lenA, true);
}

FoFiType1::FoFiType1(char* fileA, int lenA, bool freeFileDataA)
    : FoFiBase(fileA, lenA, freeFileDataA)
{
	fontMatrix[0] = 0.001;
	fontMatrix[1] = 0;
	fontMatrix[2] = 0;
	fontMatrix[3] = 0.001;
	fontMatrix[4] = 0;
	fontMatrix[5] = 0;
	undoPFB();
}

FoFiType1::~FoFiType1()
{
}

std::string_view FoFiType1::getName()
{
	if (!parsed) parse();
	return name;
}

const VEC_STR& FoFiType1::getEncoding()
{
	if (!parsed) parse();
	return encoding;
}

void FoFiType1::getFontMatrix(double* mat)
{
	if (!parsed) parse();
	for (int i = 0; i < 6; ++i)
		mat[i] = fontMatrix[i];
}

void FoFiType1::writeEncoded(VEC_STR& newEncoding, FoFiOutputFunc outputFunc, void* outputStream)
{
	char *line, *line2, *p;

	// copy everything up to the encoding
	for (line = (char*)file; line && line + 9 <= (char*)file + len && strncmp(line, "/Encoding", 9); line = getNextLine(line))
		;
	if (!line)
	{
		// no encoding - just copy the whole font file
		(*outputFunc)(outputStream, (char*)file, len);
		return;
	}
	(*outputFunc)(outputStream, (char*)file, (int)(line - (char*)file));

	// write the new encoding
	(*outputFunc)(outputStream, "/Encoding 256 array\n", 20);
	(*outputFunc)(outputStream, "0 1 255 {1 index exch /.notdef put} for\n", 40);
	for (int i = 0; i < 256; ++i)
	{
		if (const auto& newEnc = newEncoding[i]; newEnc.size() && newEnc.front() != ':')
		{
			const auto buf = fmt::format("dup {} /{} put\n", i, newEnc);
			(*outputFunc)(outputStream, buf.c_str(), TO_INT(buf.size()));
		}
	}
	(*outputFunc)(outputStream, "readonly def\n", 13);

	// find the end of the encoding data
	//~ this ought to parse PostScript tokens
	if (line + 30 <= (char*)file + len && !strncmp(line, "/Encoding StandardEncoding def", 30))
	{
		line = getNextLine(line);
	}
	else
	{
		// skip "/Encoding" + one whitespace char,
		// then look for 'def' preceded by PostScript whitespace
		p    = line + 10;
		line = nullptr;
		for (; p < (char*)file + len; ++p)
		{
			if ((*p == ' ' || *p == '\t' || *p == '\x0a' || *p == '\x0d' || *p == '\x0c' || *p == '\0') && p + 4 <= (char*)file + len && !strncmp(p + 1, "def", 3))
			{
				line = p + 4;
				break;
			}
		}
	}

	// some fonts have two /Encoding entries in their dictionary, so we
	// check for a second one here
	if (line)
	{
		int i;
		for (line2 = line, i = 0; i < 20 && line2 && line2 + 9 <= (char*)file + len && strncmp(line2, "/Encoding", 9); line2 = getNextLine(line2), ++i)
			;
		if (i < 20 && line2)
		{
			(*outputFunc)(outputStream, line, (int)(line2 - line));
			if (line2 + 30 <= (char*)file + len && !strncmp(line2, "/Encoding StandardEncoding def", 30))
			{
				line = getNextLine(line2);
			}
			else
			{
				// skip "/Encoding",
				// then look for 'def' preceded by PostScript whitespace
				p    = line2 + 9;
				line = nullptr;
				for (; p < (char*)file + len; ++p)
				{
					if ((*p == ' ' || *p == '\t' || *p == '\x0a' || *p == '\x0d' || *p == '\x0c' || *p == '\0') && p + 4 <= (char*)file + len && !strncmp(p + 1, "def", 3))
					{
						line = p + 4;
						break;
					}
				}
			}
		}

		// copy everything after the encoding
		if (line) (*outputFunc)(outputStream, line, (int)(((char*)file + len) - line));
	}
}

char* FoFiType1::getNextLine(char* line)
{
	while (line < (char*)file + len && *line != '\x0a' && *line != '\x0d') ++line;
	if (line < (char*)file + len && *line == '\x0d') ++line;
	if (line < (char*)file + len && *line == '\x0a') ++line;
	if (line >= (char*)file + len) return nullptr;
	return line;
}

void FoFiType1::parse()
{
	char *line, *line1, *p, *p2;
	char  buf[256];
	int   n, code, base, i;

	bool gotMatrix = false;
	for (i = 1, line = (char*)file; i <= 100 && line && (name.empty() || encoding.empty() || !gotMatrix); ++i)
	{
		// get font name
		if (name.empty() && line + 9 <= (char*)file + len && !strncmp(line, "/FontName", 9))
		{
			n = 255;
			if (line + n > (char*)file + len)
				n = (int)(((char*)file + len) - line);
			strncpy(buf, line, n);
			buf[n] = '\0';
			if ((p = strchr(buf + 9, '/')) && (p = strtok(p + 1, " \t\n\r")))
				name = p;
			line = getNextLine(line);

			// get encoding
		}
		else if (encoding.empty() && line + 30 <= (char*)file + len && !strncmp(line, "/Encoding StandardEncoding def", 30))
		{
			encoding = fofiType1StandardEncoding;
		}
		else if (encoding.empty() && line + 19 <= (char*)file + len && !strncmp(line, "/Encoding 256 array", 19))
		{
			encoding.resize(256, "");
			int j;
			for (j = 0, line = getNextLine(line); j < 300 && line && (line1 = getNextLine(line)); ++j, line = line1)
			{
				if ((n = (int)(line1 - line)) > 255) n = 255;
				strncpy(buf, line, n);
				buf[n] = '\0';
				for (p = buf; *p == ' ' || *p == '\t'; ++p)
					;
				const bool endsWithDup   = !strncmp(line - 4, "dup\x0a", 4) || !strncmp(line - 5, "dup\x0d", 4);
				const bool startsWithDup = !strncmp(p, "dup", 3);
				if (endsWithDup || startsWithDup)
				{
					if (startsWithDup) p += 3;
					while (1)
					{
						for (; *p == ' ' || *p == '\t'; ++p)
							;
						code = 0;
						if (*p == '8' && p[1] == '#')
						{
							base = 8;
							p += 2;
						}
						else if (*p >= '0' && *p <= '9')
						{
							base = 10;
						}
						else
						{
							break;
						}
						for (; *p >= '0' && *p < '0' + base; ++p)
							code = code * base + (*p - '0');
						for (; *p == ' ' || *p == '\t'; ++p)
							;
						if (*p != '/') break;
						++p;
						for (p2 = p; *p2 && *p2 != ' ' && *p2 != '\t'; ++p2)
							;
						if (code >= 0 && code < 256)
						{
							const char c = *p2;
							*p2          = '\0';
							encoding[code] = p;
							*p2            = c;
						}
						for (p = p2; *p == ' ' || *p == '\t'; ++p)
							;
						if (strncmp(p, "put", 3)) break;
						for (p += 3; *p == ' ' || *p == '\t'; ++p)
							;
						if (strncmp(p, "dup", 3)) break;
						p += 3;
					}
				}
				else
				{
					if (strtok(buf, " \t") && (p = strtok(nullptr, " \t\n\r")) && !strcmp(p, "def"))
						break;
				}
			}
			//~ check for getinterval/putinterval junk
		}
		else if (!gotMatrix && line + 11 <= (char*)file + len && !strncmp(line, "/FontMatrix", 11))
		{
			n = 255;
			if (line + 11 + n > (char*)file + len)
				n = (int)(((char*)file + len) - (line + 11));
			strncpy(buf, line + 11, n);
			buf[n] = '\0';
			if ((p = strchr(buf, '[')))
			{
				++p;
				if ((p2 = strchr(p, ']')))
				{
					*p2 = '\0';
					for (int j = 0; j < 6; ++j)
						if ((p = strtok(j == 0 ? p : (char*)nullptr, " \t\n\r")))
							fontMatrix[j] = atof(p);
						else
							break;
				}
			}
			gotMatrix = true;
		}
		else
		{
			line = getNextLine(line);
		}
	}

	parsed = true;
}

// Undo the PFB encoding, i.e., remove the PFB headers.
void FoFiType1::undoPFB()
{
	bool ok = true;
	if (getU8(0, &ok) != 0x80 || !ok) return;
	uint8_t* file2 = (uint8_t*)gmalloc(TO_INT(len));
	int      pos1  = 0;
	int      pos2  = 0;
	while (getU8(pos1, &ok) == 0x80 && ok)
	{
		const int type = getU8(pos1 + 1, &ok);
		if (type < 1 || type > 2 || !ok) break;
		const uint32_t segLen = getU32LE(pos1 + 2, &ok);
		pos1 += 6;
		if (!ok || !checkRegion(pos1, segLen)) break;
		memcpy(file2 + pos2, file + pos1, segLen);
		pos1 += segLen;
		pos2 += segLen;
	}
	if (freeFileData) gfree(fileData);
	file         = file2;
	fileData     = file2;
	freeFileData = true;
	len          = pos2;
}
