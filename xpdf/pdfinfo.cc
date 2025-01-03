//========================================================================
//
// pdfinfo.cc
//
// Copyright 1998-2013 Glyph & Cog, LLC
// updated by octopoulos @ 2024-12-29
//
//========================================================================

// --cfgFileName "c:/web/gori/data/xpdf.cfg" -- "C:/web/gori/source/pdf/bug.pdf"

#include <aconf.h>
#include "Error.h"
#include "GlobalParams.h"
#include "Page.h"
#include "PDFDoc.h"
#include "TextString.h"
#include "UTF8.h"
#include "Zoox.h"

#include <CLI/CLI.hpp>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void        printInfoString(Object* infoDict, const char* infoKey, ZxDoc* xmp, const char* xmpKey1, const char* xmpKey2, const char* text, bool parseDate, UnicodeMap* uMap);
static void        printCustomInfo(Object* infoDict, UnicodeMap* uMap);
static std::string parseInfoDate(const std::string& s);
static std::string parseXMPDate(const std::string& s);
static void        printBox(const char* text, PDFRectangle* box);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
	// OPTIONS
	//////////

	CLI_BEGIN("pdfinfo");

	// name, type, default, implicit, needApp, help
	// clang-format off
	CLI_OPTION(cfgFileName  , std::string, "", "", 0, "configuration file to use in place of .xpdfrc");
	CLI_OPTION(firstPage    , int        , 1 , 1 , 0, "first page to convert");
	CLI_OPTION(lastPage     , int        , 0 , 0 , 0, "last page to convert");
	CLI_OPTION(ownerPassword, std::string, "", "", 0, "owner password (for encrypted files)");
	CLI_OPTION(printBoxes   , int        , 0 , 1 , 0, "print the page bounding boxes");
	CLI_OPTION(printCustom  , int        , 0 , 1 , 0, "print the custom info dictionary fields");
	CLI_OPTION(printMetadata, int        , 0 , 1 , 0, "print the document metadata (XML)");
	CLI_OPTION(printVersion , int        , 0 , 1 , 0, "print copyright and version info");
	CLI_OPTION(rawDates     , int        , 0 , 1 , 0, "print the undecoded date strings directly from the PDF file");
	CLI_OPTION(textEncName  , std::string, "", "", 0, "output text encoding name");
	CLI_OPTION(userPassword , std::string, "", "", 0, "user password (for encrypted files)");
	//
	CLI_REQUIRED(fileName, std::string, 1, "<PDF-file>");
	// clang-format on

	CLI11_PARSE(cli, argc, argv);
	CLI_NEEDAPP();

	UnicodeMap* uMap;
	Object      info;
	Object*     acroForm;
	char        buf[256];
	std::string metadata;
	ZxDoc*      xmp;
	bool        multiPage;

	// config file
	{
		if (CLI_STRING(cfgFileName) && !std::filesystem::exists(cfgFileName))
			error(errConfig, -1, "Config file '{}' doesn't exist or isn't a file", cfgFileName);

		globalParams = std::make_shared<GlobalParams>(cfgFileName.c_str());
		if (CLI_STRING(textEncName)) globalParams->setTextEncoding(textEncName.c_str());
	}

	// get mapping to output encoding
	if (!(uMap = globalParams->getTextEncoding()))
	{
		error(errConfig, -1, "Couldn't get text encoding");
		return 99;
	}

	// open PDF file
	auto doc = std::make_unique<PDFDoc>(fileName, ownerPassword, userPassword);
	if (!doc->isOk()) return 1;

	// get page range
	if (firstPage < 1) firstPage = 1;
	if (lastPage == 0)
	{
		multiPage = false;
		lastPage  = 1;
	}
	else
	{
		multiPage = true;
	}
	if (lastPage < 1 || lastPage > doc->getNumPages())
		lastPage = doc->getNumPages();

	// print doc info
	doc->getDocInfo(&info);
	if ((metadata = doc->readMetadata()).size())
		xmp = ZxDoc::loadMem(metadata.c_str(), metadata.size());
	else
		xmp = nullptr;
	// clang-format off
	printInfoString(&info, "Title"       , xmp, "dc:title"       , nullptr         , "Title:          ", false    , uMap);
	printInfoString(&info, "Subject"     , xmp, "dc:description" , nullptr         , "Subject:        ", false    , uMap);
	printInfoString(&info, "Keywords"    , xmp, "pdf:Keywords"   , nullptr         , "Keywords:       ", false    , uMap);
	printInfoString(&info, "Author"      , xmp, "dc:creator"     , nullptr         , "Author:         ", false    , uMap);
	printInfoString(&info, "Creator"     , xmp, "xmp:CreatorTool", nullptr         , "Creator:        ", false    , uMap);
	printInfoString(&info, "Producer"    , xmp, "pdf:Producer"   , nullptr         , "Producer:       ", false    , uMap);
	printInfoString(&info, "CreationDate", xmp, "xap:CreateDate" , "xmp:CreateDate", "CreationDate:   ", !rawDates, uMap);
	printInfoString(&info, "ModDate"     , xmp, "xap:ModifyDate" , "xmp:ModifyDate", "ModDate:        ", !rawDates, uMap);
	// clang-format on
	if (printCustom)
		printCustomInfo(&info, uMap);
	info.free();
	if (xmp) delete xmp;

	// print tagging info
	printf("Tagged:         %s\n", doc->getStructTreeRoot()->isDict() ? "yes" : "no");

	// print form info
	if ((acroForm = doc->getCatalog()->getAcroForm())->isDict())
	{
		Object xfa;
		acroForm->dictLookup("XFA", &xfa);
		if (xfa.isStream() || xfa.isArray())
			if (doc->getCatalog()->getNeedsRendering())
				printf("Form:           dynamic XFA\n");
			else
				printf("Form:           static XFA\n");
		else
			printf("Form:           AcroForm\n");
		xfa.free();
	}
	else
	{
		printf("Form:           none\n");
	}

	// print page count
	printf("Pages:          %d\n", doc->getNumPages());

	// print encryption info
	if (doc->isEncrypted())
	{
		CryptAlgorithm encAlgorithm;
		int            encVersion;
		int            keyLength;
		bool           ownerPasswordOk;
		int            permFlags;
		doc->getXRef()->getEncryption(&permFlags, &ownerPasswordOk, &keyLength, &encVersion, &encAlgorithm);
		printf("Encrypted:      %s %d-bit\n", encAlgorithm == cryptRC4 ? "RC4" : "AES", keyLength * 8);
		printf("Permissions:    print:%s copy:%s change:%s addNotes:%s\n", doc->okToPrint(true) ? "yes" : "no", doc->okToCopy(true) ? "yes" : "no", doc->okToChange(true) ? "yes" : "no", doc->okToAddNotes(true) ? "yes" : "no");
	}
	else
	{
		printf("Encrypted:      no\n");
	}

	// print page size
	for (int pg = firstPage; pg <= lastPage; ++pg)
	{
		const double w = doc->getPageCropWidth(pg);
		const double h = doc->getPageCropHeight(pg);
		if (multiPage)
			printf("Page %4d size: %g x %g pts", pg, w, h);
		else
			printf("Page size:      %g x %g pts", w, h);
		if ((fabs(w - 612) < 0.1 && fabs(h - 792) < 0.1) || (fabs(w - 792) < 0.1 && fabs(h - 612) < 0.1))
		{
			printf(" (letter)");
		}
		else
		{
			double hISO = sqrt(sqrt(2.0)) * 7200 / 2.54;
			double wISO = hISO / sqrt(2.0);
			for (int i = 0; i <= 6; ++i)
			{
				if ((fabs(w - wISO) < 1 && fabs(h - hISO) < 1) || (fabs(w - hISO) < 1 && fabs(h - wISO) < 1))
				{
					printf(" (A%d)", i);
					break;
				}
				hISO = wISO;
				wISO /= sqrt(2.0);
			}
		}
		printf(" (rotated %d degrees)", doc->getPageRotate(pg));
		printf("\n");
	}

	// print the boxes
	if (printBoxes)
	{
		if (multiPage)
		{
			for (int pg = firstPage; pg <= lastPage; ++pg)
			{
				Page* page = doc->getCatalog()->getPage(pg);
				snprintf(buf, sizeof(buf), "Page %4d MediaBox: ", pg);
				printBox(buf, page->getMediaBox());
				snprintf(buf, sizeof(buf), "Page %4d CropBox:  ", pg);
				printBox(buf, page->getCropBox());
				snprintf(buf, sizeof(buf), "Page %4d BleedBox: ", pg);
				printBox(buf, page->getBleedBox());
				snprintf(buf, sizeof(buf), "Page %4d TrimBox:  ", pg);
				printBox(buf, page->getTrimBox());
				snprintf(buf, sizeof(buf), "Page %4d ArtBox:   ", pg);
				printBox(buf, page->getArtBox());
			}
		}
		else if (doc->getNumPages() > 0)
		{
			Page* page = doc->getCatalog()->getPage(firstPage);
			printBox("MediaBox:       ", page->getMediaBox());
			printBox("CropBox:        ", page->getCropBox());
			printBox("BleedBox:       ", page->getBleedBox());
			printBox("TrimBox:        ", page->getTrimBox());
			printBox("ArtBox:         ", page->getArtBox());
		}
	}

	// print file size
	FILE* f = openFile(fileName.c_str(), "rb");
	if (f)
	{
		gfseek(f, 0, SEEK_END);
		printf("File size:      %u bytes\n", (uint32_t)gftell(f));
		fclose(f);
	}

	// print linearization info
	printf("Optimized:      %s\n", doc->isLinearized() ? "yes" : "no");

	// print JavaScript info
	printf("JavaScript:     %s\n", doc->usesJavaScript() ? "yes" : "no");

	// print PDF version
	printf("PDF version:    %.1f\n", doc->getPDFVersion());

	// print the metadata
	if (printMetadata && metadata.size())
	{
		fputs("Metadata:\n", stdout);
		fputs(metadata.c_str(), stdout);
		fputc('\n', stdout);
	}

	// clean up
	uMap->decRefCnt();

	// check for memory leaks
	Object::memCheck(stderr);
	gMemReport(stderr);
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void printInfoString(Object* infoDict, const char* infoKey, ZxDoc* xmp, const char* xmpKey1, const char* xmpKey2, const char* text, bool parseDate, UnicodeMap* uMap)
{
	Object      obj;
	TextString* s;
	Unicode*    u;
	char        buf[8];
	std::string value, tmp;
	ZxElement * rdf, *elem, *child;
	ZxNode *    node, *node2;

	//-- check the info dictionary
	if (infoDict->isDict())
	{
		if (infoDict->dictLookup(infoKey, &obj)->isString())
		{
			if (!parseDate || (value = parseInfoDate(obj.getString())).empty())
			{
				s = new TextString(obj.getString());
				u = s->getUnicode();
				value.clear();
				for (int i = 0; i < s->getLength(); ++i)
				{
					const int n = uMap->mapUnicode(u[i], buf, sizeof(buf));
					value.append(buf, n);
				}
				delete s;
			}
		}
		obj.free();
	}

	//-- check the XMP metadata
	if (xmp)
	{
		rdf = xmp->getRoot();
		if (rdf->isElement("x:xmpmeta"))
			rdf = rdf->findFirstChildElement("rdf:RDF");
		if (rdf && rdf->isElement("rdf:RDF"))
		{
			for (node = rdf->getFirstChild(); node; node = node->getNextChild())
			{
				if (node->isElement("rdf:Description"))
				{
					if (!(elem = node->findFirstChildElement(xmpKey1)) && xmpKey2)
						elem = node->findFirstChildElement(xmpKey2);
					if (elem)
					{
						if ((child = elem->findFirstChildElement("rdf:Alt")) || (child = elem->findFirstChildElement("rdf:Seq")))
						{
							if ((node2 = child->findFirstChildElement("rdf:li")))
								node2 = node2->getFirstChild();
						}
						else
						{
							node2 = elem->getFirstChild();
						}
						if (node2 && node2->isCharData())
						{
							if (!parseDate || (value = parseXMPDate(((ZxCharData*)node2)->getData())).empty())
							{
								tmp       = ((ZxCharData*)node2)->getData();
								int     i = 0;
								Unicode uu;
								value.clear();
								while (getUTF8(tmp, &i, &uu))
								{
									const int n = uMap->mapUnicode(uu, buf, sizeof(buf));
									value.append(buf, n);
								}
							}
						}
						break;
					}
				}
			}
		}
	}

	if (value.size())
	{
		fputs(text, stdout);
		fwrite(value.c_str(), 1, value.size(), stdout);
		fputc('\n', stdout);
	}
}

static void printCustomInfo(Object* infoDict, UnicodeMap* uMap)
{
	if (!infoDict->isDict())
		return;
	for (int i = 0; i < infoDict->dictGetLength(); ++i)
	{
		char*  key = infoDict->dictGetKey(i);
		Object obj;
		infoDict->dictGetVal(i, &obj);
		if (obj.isString() && strcmp(key, "Title") && strcmp(key, "Subject") && strcmp(key, "Keywords") && strcmp(key, "Author") && strcmp(key, "Creator") && strcmp(key, "Producer") && strcmp(key, "CreationDate") && strcmp(key, "ModDate"))
		{
			printf("%s: ", key);
			int n = 14 - (int)strlen(key);
			if (n > 0)
				printf("%*s", n, "");
			TextString* s = new TextString(obj.getString());
			Unicode*    u = s->getUnicode();
			char        buf[8];
			for (int j = 0; j < s->getLength(); ++j)
			{
				n = uMap->mapUnicode(u[j], buf, sizeof(buf));
				fwrite(buf, 1, n, stdout);
			}
			delete s;
			printf("\n");
		}
		obj.free();
	}
}

static std::string parseInfoDate(const std::string& s)
{
	int       year, mon, day, hour, min, sec, n;
	struct tm tmStruct;
	char      buf[256];

	const char* p = s.c_str();
	if (p[0] == 'D' && p[1] == ':')
		p += 2;
	if ((n = sscanf(p, "%4d%2d%2d%2d%2d%2d", &year, &mon, &day, &hour, &min, &sec)) < 1)
		return "";
	switch (n)
	{
	case 1: mon = 1;
	case 2: day = 1;
	case 3: hour = 0;
	case 4: min = 0;
	case 5: sec = 0;
	}
	tmStruct.tm_year  = year - 1900;
	tmStruct.tm_mon   = mon - 1;
	tmStruct.tm_mday  = day;
	tmStruct.tm_hour  = hour;
	tmStruct.tm_min   = min;
	tmStruct.tm_sec   = sec;
	tmStruct.tm_wday  = -1;
	tmStruct.tm_yday  = -1;
	tmStruct.tm_isdst = -1;
	// compute the tm_wday and tm_yday fields
	if (!(mktime(&tmStruct) != (time_t)-1 && strftime(buf, sizeof(buf), "%c", &tmStruct)))
		return "";
	return buf;
}

static std::string parseXMPDate(const std::string& s)
{
	int       year, mon, day, hour, min, sec, tz;
	struct tm tmStruct;
	char      buf[256];

	const char* p = s.c_str();
	if (isdigit(p[0]) && isdigit(p[1]) && isdigit(p[2]) && isdigit(p[3]))
	{
		buf[0] = p[0];
		buf[1] = p[1];
		buf[2] = p[2];
		buf[3] = p[3];
		buf[4] = '\0';
		year   = atoi(buf);
		p += 4;
	}
	else
	{
		return "";
	}
	mon = day = 1;
	hour = min = sec = 0;
	tz               = 2000;
	if (p[0] == '-' && isdigit(p[1]) && isdigit(p[2]))
	{
		buf[0] = p[1];
		buf[1] = p[2];
		buf[2] = '\0';
		mon    = atoi(buf);
		p += 3;
		if (p[0] == '-' && isdigit(p[1]) && isdigit(p[2]))
		{
			buf[0] = p[1];
			buf[1] = p[2];
			buf[2] = '\0';
			day    = atoi(buf);
			p += 3;
			if (p[0] == 'T' && isdigit(p[1]) && isdigit(p[2]) && p[3] == ':' && isdigit(p[4]) && isdigit(p[5]))
			{
				buf[0] = p[1];
				buf[1] = p[2];
				buf[2] = '\0';
				hour   = atoi(buf);
				buf[0] = p[4];
				buf[1] = p[5];
				buf[2] = '\0';
				min    = atoi(buf);
				p += 6;
				if (p[0] == ':' && isdigit(p[1]) && isdigit(p[2]))
				{
					buf[0] = p[1];
					buf[1] = p[2];
					buf[2] = '\0';
					sec    = atoi(buf);
					if (p[0] == '.' && isdigit(p[1]))
						p += 2;
				}
				if ((p[0] == '+' || p[0] == '-') && isdigit(p[1]) && isdigit(p[2]) && p[3] == ':' && isdigit(p[4]) && isdigit(p[5]))
				{
					buf[0] = p[1];
					buf[1] = p[2];
					buf[2] = '\0';
					tz     = atoi(buf);
					buf[0] = p[4];
					buf[1] = p[5];
					buf[2] = '\0';
					tz     = tz * 60 + atoi(buf);
					tz     = tz * 60;
					if (p[0] == '-')
						tz = -tz;
				}
			}
		}
	}

	tmStruct.tm_year  = year - 1900;
	tmStruct.tm_mon   = mon - 1;
	tmStruct.tm_mday  = day;
	tmStruct.tm_hour  = hour;
	tmStruct.tm_min   = min;
	tmStruct.tm_sec   = sec;
	tmStruct.tm_wday  = -1;
	tmStruct.tm_yday  = -1;
	tmStruct.tm_isdst = -1;
	// compute the tm_wday and tm_yday fields
	//~ this ignores the timezone
	if (!(mktime(&tmStruct) != (time_t)-1 && strftime(buf, sizeof(buf), "%c", &tmStruct)))
		return "";
	return buf;
}

static void printBox(const char* text, PDFRectangle* box)
{
	printf("%s%8.2f %8.2f %8.2f %8.2f\n", text, box->x1, box->y1, box->x2, box->y2);
}
