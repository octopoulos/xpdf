//========================================================================
//
// PDFDoc.h
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include <stdio.h>
#include "XRef.h"
#include "Catalog.h"
#include "Page.h"

class BaseStream;
class OutputDev;
class Annots;
class Links;
class LinkAction;
class LinkDest;
class Outline;
class OutlineItem;
class OptionalContent;
class PDFCore;

//------------------------------------------------------------------------
// PDFDoc
//------------------------------------------------------------------------

class PDFDoc
{
public:
	PDFDoc(const std::string& fileNameA, const std::string& ownerPassword = "", const std::string& userPassword = "", PDFCore* coreA = nullptr);

#ifdef _WIN32
	PDFDoc(wchar_t* fileNameA, int fileNameLen, const std::string& ownerPassword = "", const std::string& userPassword = "", PDFCore* coreA = nullptr);
#endif

	// This version takes a UTF-8 file name (which is only relevant on
	// Windows).
	PDFDoc(char* fileNameA, const std::string& ownerPassword = "", const std::string& userPassword = "", PDFCore* coreA = nullptr);

	PDFDoc(BaseStream* strA, const std::string& ownerPassword = "", const std::string& userPassword = "", PDFCore* coreA = nullptr);

	~PDFDoc();

	// Was PDF document successfully opened?
	bool isOk() { return ok; }

	// Get the error code (if isOk() returns false).
	int getErrorCode() { return errCode; }

	// Get file name.
	std::string getFileName() { return fileName; }
#ifdef _WIN32
	wchar_t* getFileNameU() { return fileNameU; }
#endif

	// Get the xref table.
	XRef* getXRef() { return xref; }

	// Get catalog.
	Catalog* getCatalog() { return catalog; }

	// Get annotations.
	Annots* getAnnots() { return annots; }

	// Get base stream.
	BaseStream* getBaseStream() { return str; }

	// Get page parameters.
	double getPageMediaWidth(int page)
	{
		return catalog->getPage(page)->getMediaWidth();
	}

	double getPageMediaHeight(int page)
	{
		return catalog->getPage(page)->getMediaHeight();
	}

	double getPageCropWidth(int page)
	{
		return catalog->getPage(page)->getCropWidth();
	}

	double getPageCropHeight(int page)
	{
		return catalog->getPage(page)->getCropHeight();
	}

	int getPageRotate(int page)
	{
		return catalog->getPage(page)->getRotate();
	}

	// Get number of pages.
	int getNumPages() { return catalog->getNumPages(); }

	// Return the contents of the metadata stream, or nullptr if there is
	// no metadata.
	std::string readMetadata() { return catalog->readMetadata(); }

	// Return the structure tree root object.
	Object* getStructTreeRoot() { return catalog->getStructTreeRoot(); }

	// Display a page.
	void displayPage(OutputDev* out, int page, double hDPI, double vDPI, int rotate, bool useMediaBox, bool crop, bool printing, bool (*abortCheckCbk)(void* data) = nullptr, void* abortCheckCbkData = nullptr);

	// Display a range of pages.
	void displayPages(OutputDev* out, int firstPage, int lastPage, double hDPI, double vDPI, int rotate, bool useMediaBox, bool crop, bool printing, bool (*abortCheckCbk)(void* data) = nullptr, void* abortCheckCbkData = nullptr);

	// Display part of a page.
	void displayPageSlice(OutputDev* out, int page, double hDPI, double vDPI, int rotate, bool useMediaBox, bool crop, bool printing, int sliceX, int sliceY, int sliceW, int sliceH, bool (*abortCheckCbk)(void* data) = nullptr, void* abortCheckCbkData = nullptr);

	// Find a page, given its object ID.  Returns page number, or 0 if
	// not found.
	int findPage(int num, int gen) { return catalog->findPage(num, gen); }

	// Returns the links for the current page, transferring ownership to
	// the caller.
	Links* getLinks(int page);

	// Find a named destination.  Returns the link destination, or
	// nullptr if <name> is not a destination.
	LinkDest* findDest(const std::string& name)
	{
		return catalog->findDest(name);
	}

	// Process the links for a page.
	void processLinks(OutputDev* out, int page);

#ifndef DISABLE_OUTLINE
	// Return the outline object.
	Outline* getOutline() { return outline; }

	// Return the target page number for an outline item.  Returns 0 if
	// the item doesn't target a page in this PDF file.
	int getOutlineTargetPage(OutlineItem* outlineItem);
#endif

	// Return the OptionalContent object.
	OptionalContent* getOptionalContent() { return optContent; }

	// Is the file encrypted?
	bool isEncrypted() { return xref->isEncrypted(); }

	// Check various permissions.
	bool okToPrint(bool ignoreOwnerPW = false)
	{
		return xref->okToPrint(ignoreOwnerPW);
	}

	bool okToChange(bool ignoreOwnerPW = false)
	{
		return xref->okToChange(ignoreOwnerPW);
	}

	bool okToCopy(bool ignoreOwnerPW = false)
	{
		return xref->okToCopy(ignoreOwnerPW);
	}

	bool okToAddNotes(bool ignoreOwnerPW = false)
	{
		return xref->okToAddNotes(ignoreOwnerPW);
	}

	// Is the PDF file damaged?  This checks to see if the xref table
	// was constructed by the repair code.
	bool isDamaged() { return xref->isRepaired(); }

	// Is this document linearized?
	bool isLinearized();

	// Return the document's Info dictionary (if any).
	Object* getDocInfo(Object* obj) { return xref->getDocInfo(obj); }

	Object* getDocInfoNF(Object* obj) { return xref->getDocInfoNF(obj); }

	// Return the PDF version specified by the file.
	double getPDFVersion() { return pdfVersion; }

	// Save this file with another name.
	bool saveAs(const std::string& name);

	// Return a pointer to the PDFCore object.
	PDFCore* getCore() { return core; }

	// Get the list of embedded files.
	int getNumEmbeddedFiles() { return catalog->getNumEmbeddedFiles(); }

	Unicode* getEmbeddedFileName(int idx)
	{
		return catalog->getEmbeddedFileName(idx);
	}

	int getEmbeddedFileNameLength(int idx)
	{
		return catalog->getEmbeddedFileNameLength(idx);
	}

	bool saveEmbeddedFile(int idx, const char* path);
	bool saveEmbeddedFileU(int idx, const char* path);
#ifdef _WIN32
	bool saveEmbeddedFile(int idx, const wchar_t* path, int pathLen);
#endif
	char* getEmbeddedFileMem(int idx, int* size);

	// Return true if the document uses JavaScript.
	bool usesJavaScript() { return catalog->usesJavaScript(); }

private:
	void init(PDFCore* coreA);
	bool setup(const std::string& ownerPassword, const std::string& userPassword);
	bool setup2(const std::string& ownerPassword, const std::string& userPassword, bool repairXRef);
	void checkHeader();
	bool checkEncryption(const std::string& ownerPassword, const std::string& userPassword);
	bool saveEmbeddedFile2(int idx, FILE* f);

	std::string fileName; //
#ifdef _WIN32
	wchar_t* fileNameU; //
#endif
	FILE*            file;       //
	BaseStream*      str;        //
	PDFCore*         core;       //
	double           pdfVersion; //
	XRef*            xref;       //
	Catalog*         catalog;    //
	Annots*          annots;     //
	Outline*         outline;    //
	OptionalContent* optContent; //

	bool ok;      //
	int  errCode; //
};
