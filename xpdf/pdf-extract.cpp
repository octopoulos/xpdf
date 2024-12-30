// pdf-extract.cpp
// @author octopoulo <polluxyz@gmail.com>
// @version 2024-08-09

#define NOMINMAX

#include <iostream>
#include <filesystem>
#include <fstream>

#include "GlobalParams.h"
#include "Object.h"
#include "PDFDoc.h"
#include "SplashBitmap.h"
#include "Splash.h"
#include "SplashOutputDev.h"
#include "TextOutputDev.h"
#include "UnicodeMap.h"

#include "pdf-extract.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

PdfExtract::PdfExtract(const std::filesystem::path& configFile, int quiet)
{
	for (int i = 1; i < 200000000; i *= 2)
	{
		auto buf = GString::format("%!PS-TrueTypeFont-{0:2g} {1:d}\n", (double)i / 65536.0, i);
		fprintf(stderr, "buf=%s\n", buf->getCString());
		delete buf;
	}

	if (!globalParams)
	{
		globalParams = new GlobalParams(configFile.string().c_str());
		globalParams->setupBaseFonts(nullptr);
		globalParams->setErrQuiet(quiet);
	}
}

PdfExtract::~PdfExtract()
{
	ClosePdf();
}

void PdfExtract::ClosePdf()
{
	if (pdfDoc)
	{
		delete (PDFDoc*)pdfDoc;
		pdfDoc = nullptr;
	}
}

void PdfExtract::Destroy()
{
	if (globalParams)
	{
		delete globalParams;
		globalParams = nullptr;
	}
}

/**
 * Extract rendered bitmap of a page
 */
PdfBitmap PdfExtract::ExtractBitmap(int page, double dpi, int rotate, bool alpha)
{
	SplashColor     paperColor = { 0xff, 0xff, 0xff };
	SplashOutputDev splashOut(splashModeRGB8, 1, gFalse, paperColor);

	if (alpha) splashOut.setNoComposite(1);

	const auto doc     = (PDFDoc*)pdfDoc;
	const int  numPage = doc->getNumPages();
	splashOut.startDoc(doc->getXRef());

	if (page < 1) page = 1;
	if (page > numPage) return {};

	doc->displayPage(&splashOut, page, dpi, dpi, rotate, gFalse, gTrue, gFalse);

	auto bitmap = splashOut.getBitmap();
	return {
		.alpha    = bitmap->takeAlpha(),
		.data     = bitmap->takeData(),
		.height   = bitmap->getHeight(),
		.mode     = bitmap->getMode(),
		.rowSize  = bitmap->getRowSize(),
		.rowSizeA = bitmap->getAlphaRowSize(),
		.width    = bitmap->getWidth(),
	};
}

static void TextFunc(void* stream, const char* text, int len)
{
	std::string* sstream = (std::string*)stream;
	*sstream += std::string(text, len);
}

/**
 * Extract text of a page (no OCR)
 * @param mode: 0: reading order, 1: physical, 2: simple 1 column, 3: simple 1 column2, 4: table, 5: line printer, 6: raw order
 */
std::string PdfExtract::ExtractText(int page, int pageEnd, int mode, double fixedLineSp, double fixedPitch)
{
	TextOutputControl textOutControl;
	textOutControl.fixedLineSpacing = fixedLineSp;
	textOutControl.fixedPitch       = fixedPitch;
	textOutControl.mode             = (TextOutputMode)mode;

	UnicodeMap*   umap   = globalParams->getTextEncoding();
	std::string   stream = "";
	TextOutputDev textOut(TextFunc, (void*)&stream, &textOutControl);

	const auto doc     = (PDFDoc*)pdfDoc;
	const int  numPage = doc->getNumPages();

	page    = std::clamp(page, 1, numPage);
	pageEnd = (pageEnd < 0) ? numPage : std::min(page, numPage);
	if (page < 1) page = 1;
	if (pageEnd > numPage) pageEnd = std::min(page, numPage);

	globalParams->setTextEOL("unix");
	// globalParams->setTextPageBreaks(gFalse);

	doc->displayPages(&textOut, page, pageEnd, 72, 72, 0, gFalse, gTrue, gFalse);

	if (umap) umap->decRefCnt();
	return stream;
}

int PdfExtract::GetNumPages()
{
	return pdfDoc ? ((PDFDoc*)pdfDoc)->getNumPages() : 0;
}

void PdfExtract::OpenPdf(const std::filesystem::path& filename)
{
	ClosePdf();
	pdfDoc = new PDFDoc((char*)filename.string().c_str());
}
