//========================================================================
//
// XpdfApp.h
//
// Copyright 2015 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include <QApplication>
#include <QColor>
#include <QDateTime>
#include <QSessionManager>

class GList;
class XpdfViewer;

//------------------------------------------------------------------------

struct XpdfSavedPageNumber
{
	XpdfSavedPageNumber()
	    : pageNumber(1)
	{
	}

	XpdfSavedPageNumber(const QString& fileNameA, int pageNumberA)
	    : fileName(fileNameA)
	    , pageNumber(pageNumberA)
	{
	}

	QString fileName   = ""; //
	int     pageNumber = 0;  //
};

#define maxSavedPageNumbers 100

//------------------------------------------------------------------------

#define maxZoomValues 100

//------------------------------------------------------------------------
// XpdfApp
//------------------------------------------------------------------------

class XpdfApp : public QApplication
{
	Q_OBJECT

public:
	XpdfApp(int& argc, char** argv);
	virtual ~XpdfApp();

	int getNumViewers() { return TO_INT(viewers.size()); }

	XpdfViewer* newWindow(bool fullScreen = false, const char* remoteServerName = nullptr, int x = -1, int y = -1, int width = -1, int height = -1);

	bool openInNewWindow(QString fileName, int page = 1, QString dest = QString(), int rotate = 0, QString password = QString(), bool fullScreen = false, const char* remoteServerName = nullptr);

	void closeWindowOrQuit(XpdfViewer* viewer);

	// Called just before closing one or more PDF files.
	void startUpdatePagesFile();
	void updatePagesFile(const QString& fileName, int pageNumber);
	void finishUpdatePagesFile();

	// Return the saved page number for [fileName].
	int getSavedPageNumber(const QString& fileName);

	void quit();

	// Save the current session state. For managed sessions, {id} is the
	// session ID.
	void saveSession(const char* id, bool interactive);

	// Load the last saved session. For managed sessions, {id} is the
	// session ID to load.
	void loadSession(const char* id, bool interactive);

	//--- for use by XpdfViewer

	int getErrorEventType() { return errorEventType; }

	const QColor& getPaperColor() { return paperColor; }

	const QColor& getMatteColor() { return matteColor; }

	const QColor& getFullScreenMatteColor() { return fsMatteColor; }

	const QColor& getSelectionColor() { return selectionColor; }

	bool getReverseVideo() { return reverseVideo; }

	double getZoomScaleFactor() { return zoomScaleFactor; }

	int getNZoomValues() { return nZoomValues; }

	int getZoomValue(int idx) { return zoomValues[idx]; }

private slots:

	void saveSessionSlot(QSessionManager& sessionMgr);

private:
	void readPagesFile();
	void writePagesFile();

	int                      errorEventType                        = 0;     //
	QColor                   paperColor                            = {};    //
	QColor                   matteColor                            = {};    //
	QColor                   fsMatteColor                          = {};    //
	QColor                   selectionColor                        = {};    //
	bool                     reverseVideo                          = false; //
	double                   zoomScaleFactor                       = 0;     //
	int                      zoomValues[maxZoomValues]             = {};    //
	int                      nZoomValues                           = 0;     //
	std::vector<XpdfViewer*> viewers                               = {};    //
	QString                  savedPagesFileName                    = "";    //
	QDateTime                savedPagesFileTimestamp               = {};    //
	XpdfSavedPageNumber      savedPageNumbers[maxSavedPageNumbers] = {};    //
	bool                     savedPagesFileChanged                 = false; //
};
