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

	QWidget*               viewport                  = nullptr; //
	QScrollBar*            hScrollBar                = nullptr; //
	QScrollBar*            vScrollBar                = nullptr; //
	int                    displayDpi                = 0;       //
	double                 scaleFactor               = 0;       //
	bool                   dragging                  = false;   //
	bool                   panning                   = false;   //
	int                    panMX                     = 0;       //
	int                    panMY                     = 0;       //
	bool                   inUpdateScrollbars        = false;   //
	int                    oldFirstPage              = -1;      //
	int                    oldMidPage                = -1;      //
	LinkAction*            linkAction                = nullptr; // mouse cursor is over this link
	LinkAction*            lastLinkAction            = nullptr; // getLinkInfo() caches an action
	QString                lastLinkActionInfo        = "";      //
	QDateTime              modTime                   = {};      // last modification time of PDF file
	QtPDFUpdateCbk         updateCbk                 = nullptr; //
	void*                  updateCbkData             = nullptr; //
	QtPDFMidPageChangedCbk midPageChangedCbk         = nullptr; //
	void*                  midPageChangedCbkData     = nullptr; //
	QtPDFLoadCbk           preLoadCbk                = nullptr; //
	void*                  preLoadCbkData            = nullptr; //
	QtPDFLoadCbk           postLoadCbk               = nullptr; //
	void*                  postLoadCbkData           = nullptr; //
	QtPDFActionCbk         actionCbk                 = nullptr; //
	void*                  actionCbkData             = nullptr; //
	QtPDFLinkCbk           linkCbk                   = nullptr; //
	void*                  linkCbkData               = nullptr; //
	QtPDFSelectDoneCbk     selectDoneCbk             = nullptr; //
	void*                  selectDoneCbkData         = nullptr; //
	QtPDFPaintDoneCbk      paintDoneCbk              = nullptr; //
	void*                  paintDoneCbkData          = nullptr; //
	// optional features default to on
	bool                   hyperlinksEnabled         = true; //
	bool                   externalHyperlinksEnabled = true; //
	bool                   selectEnabled             = true; //
	bool                   panEnabled                = true; //
	bool                   showPasswordDialog        = true; //
};
