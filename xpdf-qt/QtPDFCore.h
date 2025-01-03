//========================================================================
//
// QtPDFCore.h
//
// Copyright 2009-2014 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include <QDateTime>
#include "SplashTypes.h"
#include "PDFCore.h"

class BaseStream;
class PDFDoc;
class LinkAction;
class QWidget;
class QScrollBar;

//------------------------------------------------------------------------
// callbacks
//------------------------------------------------------------------------

typedef void (*QtPDFUpdateCbk)(void* data, const std::string& fileName, int pageNum, int numPages, const char* linkLabel);

typedef void (*QtPDFMidPageChangedCbk)(void* data, int pageNum);

typedef void (*QtPDFLoadCbk)(void* data);

typedef void (*QtPDFActionCbk)(void* data, const char* action);

typedef void (*QtPDFLinkCbk)(void* data, const char* type, const char* dest, int page);

typedef void (*QtPDFSelectDoneCbk)(void* data);

typedef void (*QtPDFPaintDoneCbk)(void* data, bool finished);

//------------------------------------------------------------------------
// QtPDFCore
//------------------------------------------------------------------------

class QtPDFCore : public PDFCore
{
public:
	// Create viewer core in <viewportA>.
	QtPDFCore(QWidget* viewportA, QScrollBar* hScrollBarA, QScrollBar* vScrollBarA, SplashColorPtr paperColor, SplashColorPtr matteColor, bool reverseVideo);

	~QtPDFCore();

	//----- loadFile / displayPage / displayDest

	// Load a new file.  Returns pdfOk or error code.
	virtual int loadFile(const std::string& fileName, const std::string& ownerPassword = nullptr, const std::string& userPassword = nullptr);

#ifdef _WIN32
	// Load a new file.  Returns pdfOk or error code.
	virtual int loadFile(wchar_t* fileName, int fileNameLen, const std::string& ownerPassword = nullptr, const std::string& userPassword = nullptr);
#endif

	// Load a new file, via a Stream instead of a file name.  Returns
	// pdfOk or error code.
	virtual int loadFile(BaseStream* stream, const std::string& ownerPassword = nullptr, const std::string& userPassword = nullptr);

	// Load an already-created PDFDoc object.
	virtual void loadDoc(PDFDoc* docA);

	// Reload the current file.  This only works if the PDF was loaded
	// via a file.  Returns pdfOk or error code.
	virtual int reload();

	// Called after any update is complete.
	virtual void finishUpdate(bool addToHist, bool checkForChangedFile);

	//----- panning and selection

	void    startPan(int wx, int wy);
	void    endPan(int wx, int wy);
	void    startSelection(int wx, int wy, bool extend);
	void    endSelection(int wx, int wy);
	void    mouseMove(int wx, int wy);
	void    selectWord(int wx, int wy);
	void    selectLine(int wx, int wy);
	QString getSelectedTextQString();
	void    copySelection(bool toClipboard);

	//----- hyperlinks

	bool doAction(LinkAction* action);

	LinkAction* getLinkAction() { return linkAction; }

	QString     getLinkInfo(LinkAction* action);
	std::string mungeURL(const std::string& url);

	//----- find

	virtual bool find(char* s, bool caseSensitive, bool next, bool backward, bool wholeWord, bool onePageOnly);
	virtual bool findU(Unicode* u, int len, bool caseSensitive, bool next, bool backward, bool wholeWord, bool onePageOnly);

	//----- password dialog

	virtual std::string getPassword();

	//----- misc access

	virtual void setBusyCursor(bool busy);
	void         doSetCursor(const QCursor& cursor);
	void         doUnsetCursor();
	void         takeFocus();
	QSize        getBestSize();

	int getDisplayDpi() { return displayDpi; }

	double getScaleFactor() { return scaleFactor; }

	void enableHyperlinks(bool on) { hyperlinksEnabled = on; }

	bool getHyperlinksEnabled() { return hyperlinksEnabled; }

	void enableExternalHyperlinks(bool on) { externalHyperlinksEnabled = on; }

	bool getExternalHyperlinksEnabled() { return externalHyperlinksEnabled; }

	void enableSelect(bool on) { selectEnabled = on; }

	void enablePan(bool on) { panEnabled = on; }

	void setShowPasswordDialog(bool show) { showPasswordDialog = show; }

	void setUpdateCbk(QtPDFUpdateCbk cbk, void* data)
	{
		updateCbk     = cbk;
		updateCbkData = data;
	}

	void setMidPageChangedCbk(QtPDFMidPageChangedCbk cbk, void* data)
	{
		midPageChangedCbk     = cbk;
		midPageChangedCbkData = data;
	}

	void setPreLoadCbk(QtPDFLoadCbk cbk, void* data)
	{
		preLoadCbk     = cbk;
		preLoadCbkData = data;
	}

	void setPostLoadCbk(QtPDFLoadCbk cbk, void* data)
	{
		postLoadCbk     = cbk;
		postLoadCbkData = data;
	}

	void setActionCbk(QtPDFActionCbk cbk, void* data)
	{
		actionCbk     = cbk;
		actionCbkData = data;
	}

	void setLinkCbk(QtPDFLinkCbk cbk, void* data)
	{
		linkCbk     = cbk;
		linkCbkData = data;
	}

	void setSelectDoneCbk(QtPDFSelectDoneCbk cbk, void* data)
	{
		selectDoneCbk     = cbk;
		selectDoneCbkData = data;
	}

	void setPaintDoneCbk(QtPDFPaintDoneCbk cbk, void* data)
	{
		paintDoneCbk     = cbk;
		paintDoneCbkData = data;
	}

	//----- GUI events
	void         resizeEvent();
	void         paintEvent(int x, int y, int w, int h);
	void         scrollEvent();
	virtual void tick();

	static double computeScaleFactor();
	static int    computeDisplayDpi();

private:
	//----- hyperlinks
	void doLinkCbk(LinkAction* action);
	void runCommand(const std::string& cmdFmt, const std::string& arg);

	//----- PDFCore callbacks
	virtual void invalidate(int x, int y, int w, int h);
	virtual void updateScrollbars();
	virtual bool checkForNewFile();
	virtual void preLoad();
	virtual void postLoad();

	QWidget*    viewport;   //
	QScrollBar* hScrollBar; //
	QScrollBar* vScrollBar; //

	int    displayDpi;         //
	double scaleFactor;        //
	bool   dragging;           //
	bool   panning;            //
	int    panMX;              //
	int    panMY;              //
	bool   inUpdateScrollbars; //
	int    oldFirstPage;       //
	int    oldMidPage;         //

	LinkAction* linkAction;         // mouse cursor is over this link
	LinkAction* lastLinkAction;     // getLinkInfo() caches an action
	QString     lastLinkActionInfo; //
	QDateTime   modTime;            // last modification time of PDF file

	QtPDFUpdateCbk         updateCbk;             //
	void*                  updateCbkData;         //
	QtPDFMidPageChangedCbk midPageChangedCbk;     //
	void*                  midPageChangedCbkData; //
	QtPDFLoadCbk           preLoadCbk;            //
	void*                  preLoadCbkData;        //
	QtPDFLoadCbk           postLoadCbk;           //
	void*                  postLoadCbkData;       //
	QtPDFActionCbk         actionCbk;             //
	void*                  actionCbkData;         //
	QtPDFLinkCbk           linkCbk;               //
	void*                  linkCbkData;           //
	QtPDFSelectDoneCbk     selectDoneCbk;         //
	void*                  selectDoneCbkData;     //
	QtPDFPaintDoneCbk      paintDoneCbk;          //
	void*                  paintDoneCbkData;      //

	bool hyperlinksEnabled;         //
	bool externalHyperlinksEnabled; //
	bool selectEnabled;             //
	bool panEnabled;                //
	bool showPasswordDialog;        //
};
