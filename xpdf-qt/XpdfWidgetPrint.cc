//========================================================================
//
// XpdfWidgetPrint.cc
//
// Copyright 2012 Glyph & Cog, LLC
//
//========================================================================

#if XPDFWIDGET_PRINTING

#	include <aconf.h>

#	include <stdlib.h>
#	include <QPrinter>
#	include "gfile.h"
#	include "PDFDoc.h"
#	include "ErrorCodes.h"
#	include "XpdfWidget.h"

#	if defined(_WIN32)
#	elif defined(__APPLE__)
#		include <CoreFoundation/CoreFoundation.h>
#		include <ApplicationServices/ApplicationServices.h>
#	elif defined(__linux__)
#		include "PSOutputDev.h"
#		include <cups/cups.h>
#	endif

#	include "gmempp.h"

//------------------------------------------------------------------------
// Windows
//------------------------------------------------------------------------

#	if defined(_WIN32)

//------------------------------------------------------------------------
// Mac OS X
//------------------------------------------------------------------------

#	elif defined(__APPLE__)

XpdfWidget::ErrorCode printPDF(PDFDoc* doc, QPrinter* prt, int hDPI, int vDPI, XpdfWidget* widget)
{
	std::string              pdfFileName;
	CFStringRef              s;
	CFURLRef                 url;
	CGPDFDocumentRef         pdfDoc;
	CGPDFPageRef             pdfPage;
	std::string              printerName;
	CFArrayRef               printerList, pageFormatList;
	char                     prtName[512];
	PMPrinter                printer;
	PMPrintSession           session;
	PMPageFormat             pageFormat;
	PMPrintSettings          printSettings;
	PMRect                   paperRect;
	CGRect                   paperRect2;
	CGContextRef             ctx;
	CGAffineTransform        pageTransform;
	QPrinter::ColorMode      colorMode;
	QSize                    paperSize;
	QPrinter::PaperSource    paperSource;
	QPageLayout::Orientation pageOrientation;
	FILE*                    f;
	bool                     deletePDFFile;
	int                      startPage, endPage, pg, i;

	//--- get PDF file name

	deletePDFFile = false;
	if (doc->getFileName())
	{
		pdfFileName = doc->getFileName();
	}
	else
	{
		if (!openTempFile(&pdfFileName, &f, "wb", ".pdf"))
			goto err0;
		fclose(f);
		deletePDFFile = true;
		if (!doc->saveAs(pdfFileName))
			goto err1;
	}

	//--- load the PDF file

	s   = CFStringCreateWithCString(nullptr, pdfFileName.c_str(), kCFStringEncodingUTF8);
	url = CFURLCreateWithFileSystemPath(nullptr, s, kCFURLPOSIXPathStyle, false);
	CFRelease(s);
	pdfDoc = CGPDFDocumentCreateWithURL(url);
	CFRelease(url);
	if (!pdfDoc)
		goto err1;

	//--- get page range

	startPage = prt->fromPage();
	endPage   = prt->toPage();
	if (startPage == 0)
		startPage = 1;
	if (endPage == 0)
		endPage = doc->getNumPages();
	if (startPage > endPage)
	{
		CFRelease(pdfDoc);
		if (deletePDFFile)
			unlink(pdfFileName.c_str());
		return XpdfWidget::pdfErrBadPageNum;
	}

	//--- get other parameters

	colorMode       = prt->colorMode();
	paperSize       = prt->pageLayout().pageSize().sizePoints();
	paperSource     = prt->paperSource();
	pageOrientation = prt->pageLayout().orientation();

	//--- create the Session and PrintSettings

	if (PMCreateSession(&session))
		goto err2;
	if (PMCreatePrintSettings(&printSettings))
		goto err3;
	if (PMSessionDefaultPrintSettings(session, printSettings))
		goto err4;
	s = CFStringCreateWithCString(nullptr, pdfFileName.c_str(), kCFStringEncodingUTF8);
	PMPrintSettingsSetJobName(printSettings, s);
	CFRelease(s);

	//--- set up for print-to-file

	if (!prt->outputFileName().isEmpty())
	{
		s   = CFStringCreateWithCString(nullptr, prt->outputFileName().toUtf8().constData(), kCFStringEncodingUTF8);
		url = CFURLCreateWithFileSystemPath(nullptr, s, kCFURLPOSIXPathStyle, false);
		CFRelease(s);
		if (PMSessionSetDestination(session, printSettings, kPMDestinationFile, kPMDocumentFormatPDF, url))
		{
			CFRelease(url);
			goto err4;
		}
		CFRelease(url);

		//--- set the printer
	}
	else
	{
		if (PMServerCreatePrinterList(kPMServerLocal, &printerList))
			goto err4;
		printer     = nullptr;
		printerName = prt->printerName().toStdString();
		for (i = 0; i < CFArrayGetCount(printerList); ++i)
		{
			printer = (PMPrinter)CFArrayGetValueAtIndex(printerList, i);
#		if QT_VERSION >= 0x050000
			s = PMPrinterGetID(printer);
#		else
			s = PMPrinterGetName(printer);
#		endif
			if (CFStringGetCString(s, prtName, sizeof(prtName), kCFStringEncodingUTF8))
			{
				if (!strcmp(prtName, printerName.c_str()))
					break;
			}
		}
		delete printerName;
		if (i >= CFArrayGetCount(printerList))
		{
			CFRelease(printerList);
			PMRelease(printSettings);
			PMRelease(session);
			CFRelease(pdfDoc);
			return XpdfWidget::pdfErrBadPrinter;
		}
		if (PMSessionSetCurrentPMPrinter(session, printer))
		{
			CFRelease(printerList);
			goto err4;
		}
		CFRelease(printerList);
	}

	//--- set color mode

#		if 0
  if (colorMode == QPrinter::GrayScale) {
    // this is deprecated, with no replacement
    PMSetColorMode(printSettings, kPMGray);
  }
#		endif

	//--- set paper size

	if (PMSessionGetCurrentPrinter(session, &printer))
		goto err4;
	if (PMSessionCreatePageFormatList(session, printer, &pageFormatList))
		goto err4;
	pageFormat = nullptr;
	for (i = 0; i < CFArrayGetCount(pageFormatList); ++i)
	{
		pageFormat = (PMPageFormat)CFArrayGetValueAtIndex(pageFormatList, i);
		PMGetUnadjustedPaperRect(pageFormat, &paperRect);
		if (fabs((paperRect.right - paperRect.left) - paperSize.width()) < 2 && fabs((paperRect.bottom - paperRect.top) - paperSize.height()) < 2)
		{
			PMRetain(pageFormat);
			break;
		}
		pageFormat = nullptr;
	}
	CFRelease(pageFormatList);
	if (!pageFormat)
	{
		if (PMCreatePageFormat(&pageFormat))
			goto err4;
		if (PMSessionDefaultPageFormat(session, pageFormat))
			goto err5;
	}

	//--- set page orientation

	PMGetAdjustedPaperRect(pageFormat, &paperRect);
	if (pageOrientation == QPageLayout::Landscape)
	{
		PMSetOrientation(pageFormat, kPMLandscape, kPMUnlocked);
		paperRect2 = CGRectMake(paperRect.top, paperRect.left, paperRect.bottom - paperRect.top, paperRect.right - paperRect.left);
	}
	else
	{
		PMSetOrientation(pageFormat, kPMPortrait, kPMUnlocked);
		paperRect2 = CGRectMake(paperRect.left, paperRect.top, paperRect.right - paperRect.left, paperRect.bottom - paperRect.top);
	}

	//--- print

	if (PMSetPageRange(printSettings, startPage, endPage))
		goto err5;
	if (PMSessionBeginCGDocumentNoDialog(session, printSettings, pageFormat))
		goto err5;
	for (pg = startPage; pg <= endPage; ++pg)
	{
		widget->updatePrintStatus(pg, startPage, endPage);
		if (widget->isPrintCanceled())
			goto err6;
		if (PMSessionBeginPageNoDialog(session, pageFormat, nullptr))
			goto err6;
		if (PMSessionGetCGGraphicsContext(session, &ctx))
			goto err6;
		if (!(pdfPage = CGPDFDocumentGetPage(pdfDoc, pg)))
			goto err6;
		pageTransform = CGPDFPageGetDrawingTransform(pdfPage, kCGPDFMediaBox, paperRect2, 0, true);
		CGContextSaveGState(ctx);
		CGContextConcatCTM(ctx, pageTransform);
		CGContextDrawPDFPage(ctx, pdfPage);
		CGContextRestoreGState(ctx);
		if (PMSessionEndPageNoDialog(session))
			goto err6;
	}
	widget->updatePrintStatus(endPage + 1, startPage, endPage);
	PMSessionEndDocumentNoDialog(session);
	PMRelease(pageFormat);
	PMRelease(printSettings);
	PMRelease(session);

	CFRelease(pdfDoc);
	if (deletePDFFile)
		unlink(pdfFileName.c_str());
	delete pdfFileName;

	return XpdfWidget::pdfOk;

err6:
	PMSessionEndDocumentNoDialog(session);
err5:
	PMRelease(pageFormat);
err4:
	PMRelease(printSettings);
err3:
	PMRelease(session);
err2:
	CFRelease(pdfDoc);
err1:
	if (deletePDFFile)
		unlink(pdfFileName.c_str());
	delete pdfFileName;
err0:
	return XpdfWidget::pdfErrPrinting;
}

//------------------------------------------------------------------------
// Linux
//------------------------------------------------------------------------

#	elif defined(__linux__)

static void fileOut(void* stream, const char* data, int len)
{
	fwrite(data, 1, len, (FILE*)stream);
}

XpdfWidget::ErrorCode printPDF(PDFDoc* doc, QPrinter* prt, int hDPI, int vDPI, XpdfWidget* widget)
{
	int                   startPage, endPage;
	QPageSize::PageSizeId paperSize;
	QSize                 paperSizePts;
	QPrinter::PaperSource paperSource;
	QPrinter::DuplexMode  duplex;
	std::string           psFileName;
	FILE*                 psFile;
	PSOutputDev*          psOut;
	std::string           printerName;
	cups_dest_t *         dests, *dest;
	cups_option_t*        options;
	const char *          paperSizeStr, *paperSourceStr;
	std::string           s;
	int                   nDests, nOptions;
	int                   pg;

	//--- get page range

	startPage = prt->fromPage();
	endPage   = prt->toPage();
	if (startPage == 0)
		startPage = 1;
	if (endPage == 0)
		endPage = doc->getNumPages();
	if (startPage > endPage)
		return XpdfWidget::pdfErrBadPageNum;

	//--- get other parameters

	paperSize    = prt->pageLayout().pageSize().id();
	paperSizePts = prt->pageLayout().pageSize().sizePoints();
	paperSource  = prt->paperSource();
	duplex       = prt->duplex();

	//--- print to file

	if (!prt->outputFileName().isEmpty())
	{
		psFileName.clear(); // don't delete the PS file
		if (!(psFile = fopen(prt->outputFileName().toUtf8().constData(), "wb")))
			goto err0;

		//--- open temporary PS file
	}
	else
	{
		if (!openTempFile(&psFileName, &psFile, "wb", ".ps"))
			goto err0;
	}

	//--- generate the PS file

	globalParams->setPSPaperWidth((int)(paperSizePts.width() + 0.5));
	globalParams->setPSPaperHeight((int)(paperSizePts.height() + 0.5));

	widget->updatePrintStatus(startPage, startPage, endPage);
	psOut = new PSOutputDev(fileOut, psFile, doc, startPage, endPage, psModePS);
	if (!psOut->isOk())
	{
		delete psOut;
		goto err1;
	}
	for (pg = startPage; pg <= endPage; ++pg)
	{
		if (widget->isPrintCanceled())
		{
			delete psOut;
			fclose(psFile);
			goto err1;
		}
		doc->displayPage(psOut, pg, 72, 72, 0, !globalParams->getPSUseCropBoxAsPage(), true, true);
		widget->updatePrintStatus(pg + 1, startPage, endPage);
	}
	delete psOut;
	fclose(psFile);

	//--- print the PS file

	if (psFileName)
	{
		if (!prt->printerName().isEmpty())
		{
			printerName = prt->printerName().toStdString();
		}
		else
		{
			nDests = cupsGetDests(&dests);
			if (!(dest = cupsGetDest(nullptr, nullptr, nDests, dests)))
			{
				unlink(psFileName.c_str());
				cupsFreeDests(nDests, dests);
				return XpdfWidget::pdfErrBadPrinter;
			}
			printerName = dest->name;
			cupsFreeDests(nDests, dests);
		}

		options  = nullptr;
		nOptions = 0;

		switch (paperSize)
		{
		case QPageSize::A4: paperSizeStr = "A4"; break;
		case QPageSize::Comm10E: paperSizeStr = "COM10"; break;
		case QPageSize::DLE: paperSizeStr = "DL"; break;
		case QPageSize::Legal: paperSizeStr = "Legal"; break;
		case QPageSize::Letter: paperSizeStr = "Letter"; break;
		default: paperSizeStr = nullptr; break;
		}
		switch (paperSource)
		{
		case QPrinter::LargeCapacity: paperSourceStr = "LargeCapacity"; break;
		case QPrinter::Lower: paperSourceStr = "Lower"; break;
		default: paperSourceStr = nullptr; break;
		}
		if (paperSizeStr && paperSourceStr)
		{
			const auto s = fmt::format("{},{}", paperSizeStr, paperSourceStr);
			cupsAddOption("media", s.c_str(), nOptions, &options);
			++nOptions;
		}
		else if (paperSizeStr)
		{
			cupsAddOption("media", paperSizeStr, nOptions, &options);
			++nOptions;
		}
		else if (paperSourceStr)
		{
			cupsAddOption("media", paperSourceStr, nOptions, &options);
			++nOptions;
		}

		switch (duplex)
		{
		case QPrinter::DuplexNone:
			cupsAddOption("sides", "one-sided", nOptions, &options);
			++nOptions;
			break;
		case QPrinter::DuplexAuto:
			break;
		case QPrinter::DuplexLongSide:
			cupsAddOption("sides", "two-sided-long-edge", nOptions, &options);
			++nOptions;
			break;
		case QPrinter::DuplexShortSide:
			cupsAddOption("sides", "two-sided-short-edge", nOptions, &options);
			++nOptions;
			break;
		}

		if (!cupsPrintFile(printerName.c_str(), psFileName.c_str(), doc->getFileName() ? doc->getFileName().c_str() : "Xpdf printing", nOptions, options))
		{
			cupsFreeOptions(nOptions, options);
			goto err1;
		}
		cupsFreeOptions(nOptions, options);

		unlink(psFileName.c_str());
	}

	return XpdfWidget::pdfOk;

err1:
	if (psFileName.size())
		unlink(psFileName.c_str());
err0:
	return XpdfWidget::pdfErrPrinting;
}

#	endif

#endif // XPDFWIDGET_PRINTING
