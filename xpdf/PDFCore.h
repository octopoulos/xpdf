//========================================================================
//
// PDFCore.h
//
// Copyright 2004-2014 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include <stdlib.h>
#include <atomic>
#include "SplashTypes.h"
#include "CharTypes.h"
#include "DisplayState.h"
#include "TextOutputDev.h"

class GList;
class SplashBitmap;
class SplashPattern;
class BaseStream;
class PDFDoc;
class Links;
class LinkDest;
class LinkAction;
class Annot;
class AcroFormField;
class TextPage;
class HighlightFile;
class OptionalContentGroup;
class TileMap;
class TileCache;
class TileCompositor;
class PDFCore;

//------------------------------------------------------------------------
// PDFHistory
//------------------------------------------------------------------------

struct PDFHistory
{
	std::string fileName = ""; //
	int         page     = 0;  //
};

#define pdfHistorySize 50

//------------------------------------------------------------------------
// SelectMode
//------------------------------------------------------------------------

enum SelectMode
{
	selectModeBlock,
	selectModeLinear
};

//------------------------------------------------------------------------
// FindResult
//------------------------------------------------------------------------

struct FindResult
{
	FindResult(int pageA, double xMinA, double yMinA, double xMaxA, double yMaxA)
	    : page(pageA)
	    , xMin(xMinA)
	    , yMin(yMinA)
	    , xMax(xMaxA)
	    , yMax(yMaxA)
	{
	}

	int    page = 0; //
	double xMin = 0; //
	double yMin = 0; //
	double xMax = 0; //
	double yMax = 0; //
};

//------------------------------------------------------------------------
// AsyncFindAll
//------------------------------------------------------------------------

class AsyncFindAll
{
public:
	AsyncFindAll(PDFCore* coreA)
	    : core(coreA)
	    , canceled(false)
	{
	}

	void reset() { canceled = false; }

	// Run the search, returning a list of FindResults -- same as
	// PDFCore::findAll().  This can be run on a separate thread.  If
	// cancel() is called while the search is running, this function
	// returns null.
	GList* run(PDFDoc* doc, Unicode* u, int len, bool caseSensitive, bool wholeWord, int firstPage, int lastPage);

	// Cancel a running search, causing run() to return null.
	void cancel() { canceled = true; }

private:
	PDFCore*          core     = nullptr; //
	std::atomic<bool> canceled = false;   //
};

//------------------------------------------------------------------------
// PDFCore
//------------------------------------------------------------------------

class PDFCore
{
public:
	PDFCore(SplashColorMode colorMode, int bitmapRowPad, bool reverseVideo, SplashColorPtr paperColor);
	virtual ~PDFCore();

	//----- loadFile / displayPage / displayDest

	// Load a new file.  Returns pdfOk or error code.
	virtual int loadFile(const std::string& fileName, const std::string& ownerPassword = "", const std::string& userPassword = "");

	// Load a new file, via a Stream instead of a file name.  Returns
	// pdfOk or error code.
	virtual int loadFile(BaseStream* stream, const std::string& ownerPassword = "", const std::string& userPassword = "");

	// Load an already-created PDFDoc object.
	virtual void loadDoc(PDFDoc* docA);

	// Reload the current file.  This only works if the PDF was loaded
	// via a file.  Returns pdfOk or error code.
	virtual int reload();

	// Clear out the current document, if any.
	virtual void clear();

	// Same as clear(), but returns the PDFDoc object instead of
	// deleting it.
	virtual PDFDoc* takeDoc(bool redraw);

	// Display (or redisplay) the specified page.  If <scrollToTop> is
	// set, the window is vertically scrolled to the top; if
	// <scrollToBottom> is set, the window is vertically scrolled to the
	// bottom; otherwise, no scrolling is done.  If <addToHist> is set,
	// this page change is added to the history list.
	virtual void displayPage(int page, bool scrollToTop, bool scrollToBottom, bool addToHist = true);

	// Display a link destination.
	virtual void displayDest(LinkDest* dest);

	// Called before any update is started.
	virtual void startUpdate();

	// Called after any update is complete.  Subclasses can check for
	// changes in the display parameters here.
	virtual void finishUpdate(bool addToHist, bool checkForChangedFile);

	//----- page/position changes

	virtual bool gotoNextPage(int inc, bool top);
	virtual bool gotoPrevPage(int dec, bool top, bool bottom);
	virtual bool gotoNamedDestination(const std::string& dest);
	virtual bool goForward();
	virtual bool goBackward();
	virtual void scrollLeft(int nCols = 16);
	virtual void scrollRight(int nCols = 16);
	virtual void scrollUp(int nLines = 16, bool snapToPage = false);
	virtual void scrollUpPrevPage(int nLines = 16);
	virtual void scrollDown(int nLines = 16, bool snapToPage = false);
	virtual void scrollDownNextPage(int nLines = 16);
	virtual void scrollPageUp();
	virtual void scrollPageDown();
	virtual void scrollTo(int x, int y, bool snapToPage = false);
	virtual void scrollToLeftEdge();
	virtual void scrollToRightEdge();
	virtual void scrollToTopEdge();
	virtual void scrollToBottomEdge();
	virtual void scrollToTopLeft();
	virtual void scrollToBottomRight();
	// Scroll so that (page, x, y) is centered in the window.
	virtual void scrollToCentered(int page, double x, double y);
	virtual void setZoom(double zoom);
	virtual void zoomToRect(int page, double ulx, double uly, double lrx, double lry);
	virtual void zoomCentered(double zoom);
	virtual void zoomToCurrentWidth();
	virtual void setRotate(int rotate);
	virtual void setDisplayMode(DisplayMode mode);
	virtual void setOCGState(OptionalContentGroup* ocg, bool ocgState);

	//----- selection

	// Selection mode.
	SelectMode getSelectMode() { return selectMode; }

	void setSelectMode(SelectMode mode);

	// Selection color.
	SplashColorPtr getSelectionColor();
	void           setSelectionColor(SplashColor color);

	// Modify the selection.  These functions use device coordinates.
	void setSelection(int page, int x0, int y0, int x1, int y1);
	void setLinearSelection(int page, TextPosition* pos0, TextPosition* pos1);
	void clearSelection();
	void startSelectionDrag(int pg, int x, int y);
	void moveSelectionDrag(int pg, int x, int y);
	void finishSelectionDrag();
	void selectWord(int pg, int x, int y);
	void selectLine(int pg, int x, int y);

	// Retrieve the current selection.  This function uses user
	// coordinates.  Returns false if there is no selection.
	bool getSelection(int* pg, double* ulx, double* uly, double* lrx, double* lry);
	bool hasSelection();

	// Text extraction.
	void        setTextExtractionMode(TextOutputMode mode);
	bool        getDiscardDiagonalText();
	void        setDiscardDiagonalText(bool discard);
	std::string extractText(int pg, double xMin, double yMin, double xMax, double yMax);
	std::string getSelectedText();

	//----- find

	virtual bool find(char* s, bool caseSensitive, bool next, bool backward, bool wholeWord, bool onePageOnly);
	virtual bool findU(Unicode* u, int len, bool caseSensitive, bool next, bool backward, bool wholeWord, bool onePageOnly);
	GList*       findAll(Unicode* u, int len, bool caseSensitive, bool wholeWord, int firstPage, int lastPage);

	//----- coordinate conversion

	// user space: per-page, as defined by PDF file; unit = point
	// device space: (0,0) is upper-left corner of a page; unit = pixel
	// window space: (0,0) is upper-left corner of drawing area; unit = pixel

	bool cvtWindowToUser(int xw, int yw, int* pg, double* xu, double* yu);
	bool cvtWindowToDev(int xw, int yw, int* pg, int* xd, int* yd);
	bool cvtUserToWindow(int pg, double xy, double yu, int* xw, int* yw);
	void cvtUserToDev(int pg, double xu, double yu, int* xd, int* yd);
	bool cvtDevToWindow(int pg, int xd, int yd, int* xw, int* yw);
	void cvtDevToUser(int pg, int xd, int yd, double* xu, double* yu);
	void getWindowPageRange(int x, int y, int w, int h, int* firstPage, int* lastPage);

	//----- password dialog

	virtual std::string getPassword() { return nullptr; }

	//----- misc access

	PDFDoc* getDoc() { return doc; }

	int          getPageNum();
	int          getMidPageNum();
	double       getZoom();
	double       getZoomDPI(int page);
	int          getRotate();
	DisplayMode  getDisplayMode();
	virtual void setPaperColor(SplashColorPtr paperColor);
	virtual void setMatteColor(SplashColorPtr matteColor);
	virtual void setReverseVideo(bool reverseVideo);

	bool canGoBack() { return historyBLen > 1; }

	bool canGoForward() { return historyFLen > 0; }

	int            getScrollX();
	int            getScrollY();
	int            getWindowWidth();
	int            getWindowHeight();
	virtual void   setBusyCursor(bool busy) = 0;
	LinkAction*    findLink(int pg, double x, double y);
	Annot*         findAnnot(int pg, double x, double y);
	int            findAnnotIdx(int pg, double x, double y);
	AcroFormField* findFormField(int pg, double x, double y);
	int            findFormFieldIdx(int pg, double x, double y);
	AcroFormField* getFormField(int idx);
	bool           overText(int pg, double x, double y);
	void           forceRedraw();
	void           setTileDoneCbk(void (*cbk)(void* data), void* data);

protected:
	//--- calls from PDFCore subclass

	// Set the window size (when the window is resized).
	void setWindowSize(int winWidthA, int winHeightA);

	// Get the current window bitmap.  If <wholeWindow> is true, the
	// full window is being redrawn -- this is used to end incremental
	// updates when the rasterization is done.
	SplashBitmap* getWindowBitmap(bool wholeWindow);

	// Returns true if the last call to getWindowBitmap() returned a
	// finished bitmap; or false if the bitmap was still being
	// rasterized.
	bool isBitmapFinished() { return bitmapFinished; }

	// This should be called periodically (typically every ~0.1 seconds)
	// to do incremental updates.  If an update is required, it will
	// trigger a call to invalidate().
	virtual void tick();

	//--- callbacks to PDFCore subclass

	// Subclasses can return true here to force PDFCore::finishUpdate()
	// to always invalidate the window.  This is necessary to avoid
	// flickering on some backends.
	virtual bool alwaysInvalidateOnUpdate() { return false; }

	// Invalidate the specified rectangle (in window coordinates).
	virtual void invalidate(int x, int y, int w, int h) = 0;

	// Update the scrollbars.
	virtual void updateScrollbars() = 0;

	// This returns true if the PDF file has changed on disk (if it can
	// be checked).
	virtual bool checkForNewFile() { return false; }

	// This is called just before a PDF file is loaded.
	virtual void preLoad() {}

	// This is called just after a PDF file is loaded.
	virtual void postLoad() {}

	// This is called just before deleting the PDFDoc.  The PDFCore
	// subclass must shut down any secondary threads that are using the
	// PDFDoc pointer.
	virtual void aboutToDeleteDoc() {}

	//--- internal

	int  loadFile2(PDFDoc* newDoc);
	void addToHistory();
	void clearPage();
	void loadLinks(int pg);
	void loadText(int pg);
	void getSelectionBBox(int* wxMin, int* wyMin, int* wxMax, int* wyMax);
	void getSelectRectListBBox(const std::vector<SelectRect>& rects, int* wxMin, int* wyMin, int* wxMax, int* wyMax);
	void checkInvalidate(int x, int y, int w, int h);
	void invalidateWholeWindow();

	friend class AsyncFindAll;

	PDFDoc*           doc                     = nullptr;         //
	int               linksPage               = 0;               // cached links for one page
	Links*            links                   = nullptr;         //
	int               textPage                = 0;               // cached extracted text for one page
	double            textDPI                 = 0;               //
	int               textRotate              = 0;               //
	TextOutputControl textOutCtrl             = {};              //
	TextPage*         text                    = nullptr;         //
	DisplayState*     state                   = nullptr;         //
	TileMap*          tileMap                 = nullptr;         //
	TileCache*        tileCache               = nullptr;         //
	TileCompositor*   tileCompositor          = nullptr;         //
	bool              bitmapFinished          = false;           //
	SelectMode        selectMode              = selectModeBlock; //
	int               selectPage              = 0;               // page of current selection
	int               selectStartX            = 0;               // for block mode: start point of current
	int               selectStartY            = 0;               // selection, in device coords
	TextPosition      selectStartPos          = {};              // for linear mode: start position of current selection
	PDFHistory        history[pdfHistorySize] = {};              // page history queue
	int               historyCur              = 0;               // currently displayed page
	int               historyBLen             = 0;               // number of valid entries backward from current entry
	int               historyFLen             = 0;               // number of valid entries forward from current entry
};
