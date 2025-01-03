//========================================================================
//
// PDFDoc.cc
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#ifdef _WIN32
#	include <windows.h>
#endif
#include "gmempp.h"
#include "GString.h" // LowerCase, UpperCase
#include "gfile.h"
#include "config.h"
#include "GlobalParams.h"
#include "Page.h"
#include "Catalog.h"
#include "Annot.h"
#include "Stream.h"
#include "XRef.h"
#include "Link.h"
#include "OutputDev.h"
#include "Error.h"
#include "ErrorCodes.h"
#include "Lexer.h"
#include "Parser.h"
#include "SecurityHandler.h"
#include "UTF8.h"
#ifndef DISABLE_OUTLINE
#	include "Outline.h"
#endif
#include "OptionalContent.h"
#include "PDFDoc.h"

//------------------------------------------------------------------------

#define headerSearchSize 1024 // read this many bytes at beginning of
// file to look for '%PDF'

// Avoid sharing files with child processes on Windows, where sharing
// can cause problems.
#ifdef _WIN32
#	define fopenReadMode  "rbN"
#	define wfopenReadMode L"rbN"
#else
#	define fopenReadMode "rb"
#endif

//------------------------------------------------------------------------
// PDFDoc
//------------------------------------------------------------------------

PDFDoc::PDFDoc(const std::string& fileNameA, const std::string& ownerPassword, const std::string& userPassword, PDFCore* coreA)
{
	Object      obj;
	std::string fileName1, fileName2;

	init(coreA);

	fileName = fileNameA;
#ifdef _WIN32
	const int n = TO_INT(fileName.size());
	fileNameU   = (wchar_t*)gmallocn(n + 1, sizeof(wchar_t));
	for (int i = 0; i < n; ++i)
		fileNameU[i] = (wchar_t)(fileName.at(i) & 0xff);
	fileNameU[n] = L'\0';
#endif

	fileName1 = fileName;

	// try to open file
#ifdef VMS
	if (!(file = fopen(fileName1.c_str(), fopenReadMode, "ctx=stm")))
	{
		error(errIO, -1, "Couldn't open file '{}'", fileName1);
		errCode = errOpenFile;
		return;
	}
#else
	if (!(file = fopen(fileName1.c_str(), fopenReadMode)))
	{
		fileName2 = LowerCase(fileName);
		if (!(file = fopen(fileName2.c_str(), fopenReadMode)))
		{
			fileName2 = UpperCase(fileName);
			if (!(file = fopen(fileName2.c_str(), fopenReadMode)))
			{
				error(errIO, -1, "Couldn't open file '{}'", fileName);
				errCode = errOpenFile;
				return;
			}
		}
	}
#endif

	// create stream
	obj.initNull();
	str = new FileStream(file, 0, false, 0, &obj);

	ok = setup(ownerPassword, userPassword);
}

#ifdef _WIN32
PDFDoc::PDFDoc(wchar_t* fileNameA, int fileNameLen, const std::string& ownerPassword, const std::string& userPassword, PDFCore* coreA)
{
	OSVERSIONINFO version;
	Object        obj;

	init(coreA);

	// handle a Windows shortcut
	wchar_t wPath[winMaxLongPath + 1];
	int     n = fileNameLen < winMaxLongPath ? fileNameLen : winMaxLongPath;
	memcpy(wPath, fileNameA, n * sizeof(wchar_t));
	wPath[n] = L'\0';
	readWindowsShortcut(wPath, winMaxLongPath + 1);
	int wPathLen = (int)wcslen(wPath);

	// save both Unicode and 8-bit copies of the file name
	fileName.clear();
	fileNameU = (wchar_t*)gmallocn(wPathLen + 1, sizeof(wchar_t));
	memcpy(fileNameU, wPath, (wPathLen + 1) * sizeof(wchar_t));
	for (int i = 0; i < wPathLen; ++i)
		fileName += (char)fileNameA[i];

	// try to open file
	// NB: _wfopen is only available in NT
	version.dwOSVersionInfoSize = sizeof(version);
	GetVersionEx(&version);
	if (version.dwPlatformId == VER_PLATFORM_WIN32_NT)
		file = _wfopen(fileNameU, wfopenReadMode);
	else
		file = fopen(fileName.c_str(), fopenReadMode);
	if (!file)
	{
		error(errIO, -1, "Couldn't open file '{}'", fileName);
		errCode = errOpenFile;
		return;
	}

	// create stream
	obj.initNull();
	str = new FileStream(file, 0, false, 0, &obj);

	ok = setup(ownerPassword, userPassword);
}
#endif

PDFDoc::PDFDoc(char* fileNameA, const std::string& ownerPassword, const std::string& userPassword, PDFCore* coreA)
{
#ifdef _WIN32
	OSVERSIONINFO version;
#endif
	Object obj;
#ifdef _WIN32
	Unicode u;
#endif

	init(coreA);

	fileName = fileNameA;

#if defined(_WIN32)
	wchar_t wPath[winMaxLongPath + 1];
	int     i = 0;
	int     j = 0;
	while (j < winMaxLongPath && getUTF8(fileName, &i, &u)) wPath[j++] = (wchar_t)u;
	wPath[j] = L'\0';
	readWindowsShortcut(wPath, winMaxLongPath + 1);
	int wPathLen = (int)wcslen(wPath);

	fileNameU = (wchar_t*)gmallocn(wPathLen + 1, sizeof(wchar_t));
	memcpy(fileNameU, wPath, (wPathLen + 1) * sizeof(wchar_t));

	// NB: _wfopen is only available in NT
	version.dwOSVersionInfoSize = sizeof(version);
	GetVersionEx(&version);
	if (version.dwPlatformId == VER_PLATFORM_WIN32_NT)
		file = _wfopen(fileNameU, wfopenReadMode);
	else
		file = fopen(fileName.c_str(), fopenReadMode);
#elif defined(VMS)
	file = fopen(fileName.c_str(), fopenReadMode, "ctx=stm");
#else
	file = fopen(fileName.c_str(), fopenReadMode);
#endif

	if (!file)
	{
		error(errIO, -1, "Couldn't open file '{}'", fileName);
		errCode = errOpenFile;
		return;
	}

	// create stream
	obj.initNull();
	str = new FileStream(file, 0, false, 0, &obj);

	ok = setup(ownerPassword, userPassword);
}

PDFDoc::PDFDoc(BaseStream* strA, const std::string& ownerPassword, const std::string& userPassword, PDFCore* coreA)
{
	init(coreA);

	if (strA->getFileName().size())
	{
		fileName = strA->getFileName();
#ifdef _WIN32
		const int n = TO_INT(fileName.size());
		fileNameU   = (wchar_t*)gmallocn(n + 1, sizeof(wchar_t));
		for (int i = 0; i < n; ++i)
			fileNameU[i] = (wchar_t)(fileName.at(i) & 0xff);
		fileNameU[n] = L'\0';
#endif
	}
	else
	{
		fileName.clear();
#ifdef _WIN32
		fileNameU = nullptr;
#endif
	}
	str = strA;
	ok  = setup(ownerPassword, userPassword);
}

void PDFDoc::init(PDFCore* coreA)
{
	ok      = false;
	errCode = errNone;
	core    = coreA;
	file    = nullptr;
	str     = nullptr;
	xref    = nullptr;
	catalog = nullptr;
	annots  = nullptr;
#ifndef DISABLE_OUTLINE
	outline = nullptr;
#endif
	optContent = nullptr;
}

bool PDFDoc::setup(const std::string& ownerPassword, const std::string& userPassword)
{
	str->reset();

	// check header
	checkHeader();

	// read the xref and catalog
	if (!PDFDoc::setup2(ownerPassword, userPassword, false))
	{
		if (errCode == errDamaged || errCode == errBadCatalog)
		{
			// try repairing the xref table
			error(errSyntaxWarning, -1, "PDF file is damaged - attempting to reconstruct xref table...");
			if (!PDFDoc::setup2(ownerPassword, userPassword, true))
				return false;
		}
		else
		{
			return false;
		}
	}

#ifndef DISABLE_OUTLINE
	// read outline
	outline = new Outline(catalog->getOutline(), xref);
#endif

	// read the optional content info
	optContent = new OptionalContent(this);

	// done
	return true;
}

bool PDFDoc::setup2(const std::string& ownerPassword, const std::string& userPassword, bool repairXRef)
{
	// read xref table
	xref = new XRef(str, repairXRef);
	if (!xref->isOk())
	{
		error(errSyntaxError, -1, "Couldn't read xref table");
		errCode = xref->getErrorCode();
		delete xref;
		xref = nullptr;
		return false;
	}

	// check for encryption
	if (!checkEncryption(ownerPassword, userPassword))
	{
		errCode = errEncrypted;
		delete xref;
		xref = nullptr;
		return false;
	}

	// read catalog
	catalog = new Catalog(this);
	if (!catalog->isOk())
	{
		error(errSyntaxError, -1, "Couldn't read page catalog");
		errCode = errBadCatalog;
		delete catalog;
		catalog = nullptr;
		delete xref;
		xref = nullptr;
		return false;
	}

	// initialize the Annots object
	annots = new Annots(this);

	return true;
}

PDFDoc::~PDFDoc()
{
	if (optContent) delete optContent;
#ifndef DISABLE_OUTLINE
	if (outline) delete outline;
#endif
	if (annots) delete annots;
	if (catalog) delete catalog;
	if (xref) delete xref;
	if (str) delete str;
	if (file) fclose(file);
#ifdef _WIN32
	if (fileNameU) gfree(fileNameU);
#endif
}

// Check for a PDF header on this stream.  Skip past some garbage
// if necessary.
void PDFDoc::checkHeader()
{
	char  hdrBuf[headerSearchSize + 1];
	char* p;
	int   i;

	pdfVersion = 0;
	memset(hdrBuf, 0, headerSearchSize + 1);
	str->getBlock(hdrBuf, headerSearchSize);
	for (i = 0; i < headerSearchSize - 5; ++i)
		if (!strncmp(&hdrBuf[i], "%PDF-", 5))
			break;
	if (i >= headerSearchSize - 5)
	{
		error(errSyntaxWarning, -1, "May not be a PDF file (continuing anyway)");
		return;
	}
	str->moveStart(i);
	if (!(p = strtok(&hdrBuf[i + 5], " \t\n\r")))
	{
		error(errSyntaxWarning, -1, "May not be a PDF file (continuing anyway)");
		return;
	}
	pdfVersion = atof(p);
	if (!(hdrBuf[i + 5] >= '0' && hdrBuf[i + 5] <= '9') || pdfVersion > supportedPDFVersionNum + 0.0001)
		error(errSyntaxWarning, -1, "PDF version {} -- xpdf supports version {} (continuing anyway)", p, supportedPDFVersionStr);
}

bool PDFDoc::checkEncryption(const std::string& ownerPassword, const std::string& userPassword)
{
	Object           encrypt;
	bool             encrypted;
	SecurityHandler* secHdlr;
	bool             ret;

	xref->getTrailerDict()->dictLookup("Encrypt", &encrypt);
	if ((encrypted = encrypt.isDict()))
	{
		if ((secHdlr = SecurityHandler::make(this, &encrypt)))
		{
			if (secHdlr->isUnencrypted())
			{
				// no encryption
				ret = true;
			}
			else if (secHdlr->checkEncryption(ownerPassword, userPassword))
			{
				// authorization succeeded
				xref->setEncryption(secHdlr->getPermissionFlags(), secHdlr->getOwnerPasswordOk(), secHdlr->getFileKey(), secHdlr->getFileKeyLength(), secHdlr->getEncVersion(), secHdlr->getEncAlgorithm());
				ret = true;
			}
			else
			{
				// authorization failed
				ret = false;
			}
			delete secHdlr;
		}
		else
		{
			// couldn't find the matching security handler
			ret = false;
		}
	}
	else
	{
		// document is not encrypted
		ret = true;
	}
	encrypt.free();
	return ret;
}

void PDFDoc::displayPage(OutputDev* out, int page, double hDPI, double vDPI, int rotate, bool useMediaBox, bool crop, bool printing, bool (*abortCheckCbk)(void* data), void* abortCheckCbkData)
{
	if (globalParams->getPrintCommands())
		printf("***** page %d *****\n", page);
	catalog->getPage(page)->display(out, hDPI, vDPI, rotate, useMediaBox, crop, printing, abortCheckCbk, abortCheckCbkData);
}

void PDFDoc::displayPages(OutputDev* out, int firstPage, int lastPage, double hDPI, double vDPI, int rotate, bool useMediaBox, bool crop, bool printing, bool (*abortCheckCbk)(void* data), void* abortCheckCbkData)
{
	for (int page = firstPage; page <= lastPage; ++page)
	{
		if (globalParams->getPrintStatusInfo())
		{
			fflush(stderr);
			printf("[processing page %d]\n", page);
			fflush(stdout);
		}
		displayPage(out, page, hDPI, vDPI, rotate, useMediaBox, crop, printing, abortCheckCbk, abortCheckCbkData);
		catalog->doneWithPage(page);
	}
}

void PDFDoc::displayPageSlice(OutputDev* out, int page, double hDPI, double vDPI, int rotate, bool useMediaBox, bool crop, bool printing, int sliceX, int sliceY, int sliceW, int sliceH, bool (*abortCheckCbk)(void* data), void* abortCheckCbkData)
{
	catalog->getPage(page)->displaySlice(out, hDPI, vDPI, rotate, useMediaBox, crop, sliceX, sliceY, sliceW, sliceH, printing, abortCheckCbk, abortCheckCbkData);
}

Links* PDFDoc::getLinks(int page)
{
	return catalog->getPage(page)->getLinks();
}

void PDFDoc::processLinks(OutputDev* out, int page)
{
	catalog->getPage(page)->processLinks(out);
}

#ifndef DISABLE_OUTLINE
int PDFDoc::getOutlineTargetPage(OutlineItem* outlineItem)
{
	LinkAction*    action;
	LinkActionKind kind;
	LinkDest*      dest;
	std::string    namedDest;
	Ref            pageRef;
	int            pg;

	if (outlineItem->pageNum >= 0)
		return outlineItem->pageNum;
	if (!(action = outlineItem->getAction()))
	{
		outlineItem->pageNum = 0;
		return 0;
	}
	kind = action->getKind();
	if (kind != actionGoTo)
	{
		outlineItem->pageNum = 0;
		return 0;
	}
	if ((dest = ((LinkGoTo*)action)->getDest()))
		dest = dest->copy();
	else if ((namedDest = ((LinkGoTo*)action)->getNamedDest()).size())
		dest = findDest(namedDest);
	pg = 0;
	if (dest)
	{
		if (dest->isPageRef())
		{
			pageRef = dest->getPageRef();
			pg      = findPage(pageRef.num, pageRef.gen);
		}
		else
		{
			pg = dest->getPageNum();
		}
		delete dest;
	}
	outlineItem->pageNum = pg;
	return pg;
}
#endif

bool PDFDoc::isLinearized()
{
	Parser* parser;
	Object  obj1, obj2, obj3, obj4, obj5;

	bool lin = false;
	obj1.initNull();
	parser = new Parser(xref, new Lexer(xref, str->makeSubStream(str->getStart(), false, 0, &obj1)), true);
	parser->getObj(&obj1);
	parser->getObj(&obj2);
	parser->getObj(&obj3);
	parser->getObj(&obj4);
	if (obj1.isInt() && obj2.isInt() && obj3.isCmd("obj") && obj4.isDict())
	{
		obj4.dictLookup("Linearized", &obj5);
		if (obj5.isNum() && obj5.getNum() > 0)
			lin = true;
		obj5.free();
	}
	obj4.free();
	obj3.free();
	obj2.free();
	obj1.free();
	delete parser;
	return lin;
}

bool PDFDoc::saveAs(const std::string& name)
{
	FILE* f;
	char  buf[4096];
	int   n;

	if (!(f = fopen(name.c_str(), "wb")))
	{
		error(errIO, -1, "Couldn't open file '{}'", name);
		return false;
	}
	str->reset();
	while ((n = str->getBlock(buf, sizeof(buf))) > 0)
		fwrite(buf, 1, n, f);
	str->close();
	fclose(f);
	return true;
}

bool PDFDoc::saveEmbeddedFile(int idx, const char* path)
{
	FILE* f;
	bool  ret;

	if (!(f = fopen(path, "wb")))
		return false;
	ret = saveEmbeddedFile2(idx, f);
	fclose(f);
	return ret;
}

bool PDFDoc::saveEmbeddedFileU(int idx, const char* path)
{
	FILE* f;
	if (!(f = openFile(path, "wb"))) return false;
	bool ret = saveEmbeddedFile2(idx, f);
	fclose(f);
	return ret;
}

#ifdef _WIN32
bool PDFDoc::saveEmbeddedFile(int idx, const wchar_t* path, int pathLen)
{
	FILE*         f;
	OSVERSIONINFO version;
	wchar_t       path2w[winMaxLongPath + 1];
	char          path2c[MAX_PATH + 1];
	int           i;
	bool          ret;

	// NB: _wfopen is only available in NT
	version.dwOSVersionInfoSize = sizeof(version);
	GetVersionEx(&version);
	if (version.dwPlatformId == VER_PLATFORM_WIN32_NT)
	{
		for (i = 0; i < pathLen && i < winMaxLongPath; ++i)
			path2w[i] = path[i];
		path2w[i] = 0;

		f = _wfopen(path2w, L"wb");
	}
	else
	{
		for (i = 0; i < pathLen && i < MAX_PATH; ++i)
			path2c[i] = (char)path[i];
		path2c[i] = 0;

		f = fopen(path2c, "wb");
	}
	if (!f) return false;
	ret = saveEmbeddedFile2(idx, f);
	fclose(f);
	return ret;
}
#endif

bool PDFDoc::saveEmbeddedFile2(int idx, FILE* f)
{
	Object strObj;
	char   buf[4096];
	int    n;

	if (!catalog->getEmbeddedFileStreamObj(idx, &strObj)) return false;
	strObj.streamReset();
	while ((n = strObj.streamGetBlock(buf, sizeof(buf))) > 0)
		fwrite(buf, 1, n, f);
	strObj.streamClose();
	strObj.free();
	return true;
}

char* PDFDoc::getEmbeddedFileMem(int idx, int* size)
{
	Object strObj;
	char*  buf;
	int    bufSize, sizeInc, n;

	if (!catalog->getEmbeddedFileStreamObj(idx, &strObj)) return nullptr;
	strObj.streamReset();
	bufSize = 0;
	buf     = nullptr;
	do
	{
		sizeInc = bufSize ? bufSize : 1024;
		if (bufSize > INT_MAX - sizeInc)
		{
			error(errIO, -1, "embedded file is too large");
			*size = 0;
			return nullptr;
		}
		buf = (char*)grealloc(buf, bufSize + sizeInc);
		n   = strObj.streamGetBlock(buf + bufSize, sizeInc);
		bufSize += n;
	}
	while (n == sizeInc);
	strObj.streamClose();
	strObj.free();
	*size = bufSize;
	return buf;
}
