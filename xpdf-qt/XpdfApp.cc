//========================================================================
//
// XpdfApp.cc
//
// Copyright 2015 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include <stdlib.h>
#include <stdio.h>
#include <QFileInfo>
#include <QLocalSocket>
#include <QMessageBox>
#include "config.h"
#include "GList.h"
#include "gfile.h"
#include "GlobalParams.h"
#include "QtPDFCore.h"
#include "XpdfViewer.h"
#include "XpdfApp.h"
#include "gmempp.h"

#include <CLI/CLI.hpp>

//------------------------------------------------------------------------
// XpdfApp
//------------------------------------------------------------------------

static void mungeOpenFileName(const char* fileName, std::string& cmd);

XpdfApp::XpdfApp(int& argc, char** argv)
    : QApplication(argc, argv)
{
	XpdfViewer*   viewer;
	QLocalSocket* sock;
	QString       sockName;
	const char *  fileName, *dest;
	bool          ok;

	setApplicationName("XpdfReader");
	setApplicationVersion(xpdfVersion);

	CLI_BEGIN("XpdfReader");

	// name, type, default, implicit, needApp, help
	// clang-format off
	CLI_OPTION(Antialias    , std::string, "" , "", 0, "enable font anti-aliasing: yes, no");
	CLI_OPTION(CfgFileName  , std::string, "" , "", 0, "configuration file to use in place of .xpdfrc");
	CLI_OPTION(FsMatteColor , std::string, "" , "", 0, "color of matte background in full-screen mode");
	CLI_OPTION(FullScreen   , int        , 0  , 1 , 0, "run in full-screen (presentation) mode");
	CLI_OPTION(InitialZoom  , std::string, 0  , 1 , 0, "initial zoom level (percent, 'page', 'width')");
	CLI_OPTION(MatteColor   , std::string, "" , "", 0, "color of matte background");
	CLI_OPTION(Open         , int        , 0  , 1 , 0, "open file using a default remote server");
	CLI_OPTION(PaperColor   , std::string, "" , "", 0, "color of paper background");
	CLI_OPTION(Password     , std::string, "" , "", 0, "password (for encrypted files)");
	CLI_OPTION(PrintCommands, int        , 0  , 1 , 0, "print commands as they're executed");
	CLI_OPTION(PrintVersion , int        , 0  , 1 , 0, "print copyright and version info");
	CLI_OPTION(RemoteServer , std::string, "" , "", 0, "remote server mode - remaining args are commands");
	CLI_OPTION(ReverseVideo , int        , 0  , 1 , 0, "reverse video");
	CLI_OPTION(Rotate       , int        , 0  , 1 , 0, "initial page rotation: 0, 90, 180, or 270");
	CLI_OPTION(TabStateFile , std::string, "", "" , 0, "file for saving/loading tab state");
	CLI_OPTION(TextEncName  , std::string, "", "" , 0, "output text encoding name");
	CLI_OPTION(VecAntialias , std::string, "" ,"" , 0, "enable vector anti-aliasing: yes, no");
	// clang-format on

	cli.parse(argc, argv);
	CLI_NEEDAPP();

	//--- set up GlobalParams; handle command line arguments
	GlobalParams::defaultTextEncoding = "UCS-2";
	globalParams                      = std::make_shared<GlobalParams>(CfgFileName.c_str());
#ifdef _WIN32
	QString dir = applicationDirPath();
	globalParams->setBaseDir(dir.toLocal8Bit().constData());
	dir += "/t1fonts";
	globalParams->setupBaseFonts(dir.toLocal8Bit().constData());
#else
	globalParams->setupBaseFonts(nullptr);
#endif
	if (CLI_STRING(InitialZoom))
		globalParams->setInitialZoom(InitialZoom.c_str());
	if (CLI_STRING(PaperColor))
		paperColor = QColor(PaperColor.c_str());
	else
		paperColor = QColor(globalParams->getPaperColor().c_str());
	reverseVideo = ReverseVideo;
	if (reverseVideo)
		this->paperColor = QColor(255 - paperColor.red(), 255 - paperColor.green(), 255 - paperColor.blue());
	if (CLI_STRING(MatteColor))
		matteColor = QColor(MatteColor.c_str());
	else
		matteColor = QColor(globalParams->getMatteColor().c_str());
	if (CLI_STRING(FsMatteColor))
		fsMatteColor = QColor(FsMatteColor.c_str());
	else
		fsMatteColor = QColor(globalParams->getFullScreenMatteColor().c_str());
	selectionColor = QColor(globalParams->getSelectionColor().c_str());
	if (CLI_STRING(Antialias))
	{
		if (!globalParams->setAntialias(Antialias.c_str()))
			fprintf(stderr, "Bad '-aa' value on command line\n");
	}
	if (CLI_STRING(VecAntialias))
	{
		if (!globalParams->setVectorAntialias(VecAntialias.c_str()))
			fprintf(stderr, "Bad '-aaVector' value on command line\n");
	}
	if (CLI_STRING(TextEncName))
		globalParams->setTextEncoding(TextEncName.c_str());
	if (CLI_STRING(TabStateFile))
		globalParams->setTabStateFile(TabStateFile.c_str());
	if (PrintCommands)
		globalParams->setPrintCommands(true);
	zoomScaleFactor = globalParams->getZoomScaleFactor();
	if (zoomScaleFactor != 1)
	{
		if (zoomScaleFactor <= 0)
			zoomScaleFactor = QtPDFCore::computeDisplayDpi() / 72.0;
		auto   initialZoomStr = globalParams->getInitialZoom();
		double initialZoom    = atoi(initialZoomStr.c_str());
		if (initialZoom > 0)
		{
			initialZoomStr = fmt::format("{}", (int)(initialZoom * zoomScaleFactor));
			globalParams->setInitialZoom(initialZoomStr.c_str());
		}
		int defaultFitZoom = globalParams->getDefaultFitZoom();
		if (defaultFitZoom > 0)
			globalParams->setDefaultFitZoom((int)(defaultFitZoom * zoomScaleFactor));
	}

	nZoomValues = 0;
	for (const auto& val : globalParams->getZoomValues())
		zoomValues[nZoomValues++] = atoi(val.c_str());

	errorEventType = QEvent::registerEventType();

#ifndef DISABLE_SESSION_MANAGEMENT
	//--- session management
	connect(this, SIGNAL(saveStateRequest(QSessionManager&)), this, SLOT(saveSessionSlot(QSessionManager&)), Qt::DirectConnection);
	if (isSessionRestored())
	{
		loadSession(sessionId().toLocal8Bit().constData(), false);
		return;
	}
#endif

	//--- remote server mode
	if (CLI_STRING(RemoteServer))
	{
		sock     = new QLocalSocket(this);
		sockName = "xpdf_";
		sockName += RemoteServer.c_str();
		sock->connectToServer(sockName, QIODevice::WriteOnly);
		if (sock->waitForConnected(5000))
		{
			for (const auto& remain : cli.remaining())
			{
				sock->write(remain.c_str());
				sock->write("\n");
			}
			while (sock->bytesToWrite())
				sock->waitForBytesWritten(5000);
			delete sock;
			::exit(0);
		}
		else
		{
			delete sock;
			viewer = newWindow(false, RemoteServer.c_str());
			for (const auto& remain : cli.remaining())
				viewer->execCmd(remain.c_str(), nullptr);
			return;
		}
	}

	//--- default remote server
	// TODO: use for (const auto& remain : cli.remaining())
	if (Open)
	{
		sock     = new QLocalSocket(this);
		sockName = "xpdf_default";
		sock->connectToServer(sockName, QIODevice::WriteOnly);
		if (sock->waitForConnected(5000))
		{
			if (argc >= 2)
			{
				std::string cmd = "openFileIn(";
				mungeOpenFileName(argv[1], cmd);
				cmd += ",tab)\nraise\n";
				sock->write(cmd.c_str());
				while (sock->bytesToWrite())
					sock->waitForBytesWritten(5000);
			}
			delete sock;
			::exit(0);
		}
		else
		{
			delete sock;
			if (argc >= 2)
			{
				// on Windows: xpdf.cc converts command line args to UTF-8
				// on Linux: command line args are in the local 8-bit charset
#ifdef _WIN32
				QString qFileName = QString::fromUtf8(argv[1]);
#else
				QString qFileName = QString::fromLocal8Bit(argv[1]);
#endif
				openInNewWindow(qFileName, 1, "", Rotate, Password.c_str(), FullScreen, "default");
			}
			else
			{
				newWindow(FullScreen, "default");
			}
			return;
		}
	}

	//--- load PDF file(s) requested on the command line
	// TODO: use for (const auto& remain : cli.remaining())
	if (argc >= 2)
	{
		int i = 1;
		while (i < argc)
		{
			int pg = -1;
			dest   = "";
			if (i + 1 < argc && argv[i + 1][0] == ':')
			{
				fileName = argv[i];
				pg       = atoi(argv[i + 1] + 1);
				i += 2;
			}
			else if (i + 1 < argc && argv[i + 1][0] == '+')
			{
				fileName = argv[i];
				dest     = argv[i + 1] + 1;
				i += 2;
			}
			else
			{
				fileName = argv[i];
				++i;
			}
			// on Windows: xpdf.cc converts command line args to UTF-8
			// on Linux: command line args are in the local 8-bit charset
#ifdef _WIN32
			QString qFileName = QString::fromUtf8(fileName);
#else
			QString qFileName = QString::fromLocal8Bit(fileName);
#endif
			if (viewers.size())
				ok = viewers[0]->openInNewTab(qFileName, pg, dest, Rotate, Password.c_str(), false);
			else
				ok = openInNewWindow(qFileName, pg, dest, Rotate, Password.c_str(), FullScreen);
		}
	}
	else
	{
		newWindow(FullScreen);
	}
}

// Process the file name for the "-open" flag: convert a relative path
// to absolute, and add escape chars as needed.  Append the modified
// name to [cmd].
static void mungeOpenFileName(const char* fileName, std::string& cmd)
{
	std::string path = fileName;
	makePathAbsolute(path);
	for (int i = 0; i < path.size(); ++i)
	{
		const char c = path.at(i);
		if (c == '(' || c == ')' || c == ',' || c == '\x01')
			cmd += '\x01';
		cmd += c;
	}
}

XpdfApp::~XpdfApp()
{
}

XpdfViewer* XpdfApp::newWindow(bool fullScreen, const char* remoteServerName, int x, int y, int width, int height)
{
	XpdfViewer* viewer = new XpdfViewer(this, fullScreen);
	viewers.push_back(viewer);
	if (remoteServerName)
		viewer->startRemoteServer(remoteServerName);
	if (width > 0 && height > 0)
	{
		viewer->resize(width, height);
		viewer->move(x, y);
	}
	else
	{
		viewer->tweakSize();
	}
	viewer->show();
	return viewer;
}

bool XpdfApp::openInNewWindow(QString fileName, int page, QString dest, int rotate, QString password, bool fullScreen, const char* remoteServerName)
{
	XpdfViewer* viewer = XpdfViewer::create(this, fileName, page, dest, rotate, password, fullScreen);
	if (!viewer)
		return false;
	viewers.push_back(viewer);
	if (remoteServerName)
		viewer->startRemoteServer(remoteServerName);
	viewer->tweakSize();
	viewer->show();
	return true;
}

void XpdfApp::closeWindowOrQuit(XpdfViewer* viewer)
{
	viewer->close();
	for (auto it = viewers.begin(); it != viewers.end();)
		if (*it == viewer)
			it = viewers.erase(it);
		else
			++it;
}

void XpdfApp::quit()
{
	if (globalParams->getSaveSessionOnQuit())
		saveSession(nullptr, false);
	while (viewers.size())
	{
		XpdfViewer* viewer = viewers.front();
		viewer->close();
		// delete viewer??
		viewers.erase(viewers.begin());
	}
	QApplication::quit();
}

void XpdfApp::saveSession(const char* id, bool interactive)
{
	auto path = globalParams->getSessionFile();
	if (id)
	{
#if 1
		// We use a single session save file for session manager sessions
		// -- this prevents using multiple sessions, but it also avoids
		// dealing with stale session save files.
		//
		// We can't use the same save file for both session manager
		// sessions and save-on-quit sessions, because the session manager
		// sends a 'save' request immediately on starting, which will
		// overwrite the last save-on-quit session.
		path += ".managed";
#else
		path->append('.');
		path->append(id);
#endif
	}
	FILE* out = openFile(path.c_str(), "wb");
	if (!out)
	{
		if (interactive)
		{
			const auto msg = fmt::format("Couldn't write the session file '{}'", path);
			QMessageBox::warning(nullptr, "Xpdf Error", msg.c_str());
		}
		return;
	}

	fprintf(out, "xpdf-session-1\n");
	for (int i = 0; i < viewers.size(); ++i)
	{
		XpdfViewer* viewer = (XpdfViewer*)viewers.at(i);
		fprintf(out, "window %d %d %d %d\n", viewer->x(), viewer->y(), viewer->width(), viewer->height());
		viewer->saveSession(out, 1);
	}

	fclose(out);
}

void XpdfApp::loadSession(const char* id, bool interactive)
{
	auto path = globalParams->getSessionFile();
	if (id)
	{
#if 1
		// see comment in XpdfApp::saveSession
		path += ".managed";
#else
		path->append('.');
		path->append(id);
#endif
	}
	FILE* in = openFile(path.c_str(), "rb");
	if (!in)
	{
		if (interactive)
		{
			const auto msg = fmt::format("Couldn't read the session file '{}'", path);
			QMessageBox::warning(nullptr, "Xpdf Error", msg.c_str());
		}
		return;
	}

	char line[1024];
	if (!fgets(line, sizeof(line), in))
	{
		fclose(in);
		return;
	}
	size_t n = strlen(line);
	if (n > 0 && line[n - 1] == '\n')
		line[--n] = '\0';
	if (n > 0 && line[n - 1] == '\r')
		line[--n] = '\0';
	if (strcmp(line, "xpdf-session-1"))
	{
		fclose(in);
		return;
	}

	// if this function is called explicitly (e.g., bound to a key), and
	// there is a single empty viewer (because the user has just started
	// xpdf), we want to close that viewer
	XpdfViewer* viewerToClose = nullptr;
	if (viewers.size() == 1 && viewers[0]->isEmpty())
		viewerToClose = viewers[0];

	while (fgets(line, sizeof(line), in))
	{
		int x, y, width, height;
		if (sscanf(line, "window %d %d %d %d\n", &x, &y, &width, &height) != 4)
		{
			fclose(in);
			return;
		}

		XpdfViewer* viewer = newWindow(false, nullptr, x, y, width, height);
		viewer->loadSession(in, 1);

		if (viewerToClose)
		{
			closeWindowOrQuit(viewerToClose);
			viewerToClose = nullptr;
		}
	}

	fclose(in);
}

void XpdfApp::saveSessionSlot(QSessionManager& sessionMgr)
{
	// Removing the saveSessionSlot function/slot would be better, but
	// that causes problems with moc/cmake -- so just comment out the
	// guts. In any case, this function should never even be called if
	// DISABLE_SESSION_MANAGEMENT is defined.
#ifndef DISABLE_SESSION_MANAGEMENT
	saveSession(sessionMgr.sessionId().toLocal8Bit().constData(), false);
#endif
}

//------------------------------------------------------------------------

void XpdfApp::startUpdatePagesFile()
{
	if (!globalParams->getSavePageNumbers())
		return;
	readPagesFile();
	savedPagesFileChanged = false;
}

void XpdfApp::updatePagesFile(const QString& fileName, int pageNumber)
{
	if (!globalParams->getSavePageNumbers())
		return;
	if (fileName.isEmpty())
		return;
	QString canonicalFileName = QFileInfo(fileName).canonicalFilePath();
	if (canonicalFileName.isEmpty())
		return;
	XpdfSavedPageNumber s(canonicalFileName, pageNumber);
	for (int i = 0; i < maxSavedPageNumbers; ++i)
	{
		XpdfSavedPageNumber next = savedPageNumbers[i];
		savedPageNumbers[i]      = s;
		if (next.fileName == canonicalFileName)
			break;
		s = next;
	}
	savedPagesFileChanged = true;
}

void XpdfApp::finishUpdatePagesFile()
{
	if (!globalParams->getSavePageNumbers())
		return;
	if (savedPagesFileChanged)
		writePagesFile();
}

int XpdfApp::getSavedPageNumber(const QString& fileName)
{
	if (!globalParams->getSavePageNumbers())
		return 1;
	readPagesFile();
	QString canonicalFileName = QFileInfo(fileName).canonicalFilePath();
	if (canonicalFileName.isEmpty())
		return 1;
	for (int i = 0; i < maxSavedPageNumbers; ++i)
		if (savedPageNumbers[i].fileName == canonicalFileName)
			return savedPageNumbers[i].pageNumber;
	return 1;
}

void XpdfApp::readPagesFile()
{
	// construct the file name (first time only)
	if (savedPagesFileName.isEmpty())
	{
		const auto& s = globalParams->getPagesFile();
#ifdef _WIN32
		savedPagesFileName = QString::fromLocal8Bit(s.c_str());
#else
		savedPagesFileName = QString::fromUtf8(s.c_str());
#endif
	}

	// no change since last read, so no need to re-read
	if (savedPagesFileTimestamp.isValid() && QFileInfo(savedPagesFileName).lastModified() == savedPagesFileTimestamp)
		return;

	// mark all entries invalid
	for (int i = 0; i < maxSavedPageNumbers; ++i)
	{
		savedPageNumbers[i].fileName.clear();
		savedPageNumbers[i].pageNumber = 1;
	}

	// read the file
	FILE* f = openFile(savedPagesFileName.toUtf8().constData(), "rb");
	if (!f)
		return;
	char buf[1024];
	if (!fgets(buf, sizeof(buf), f) || strcmp(buf, "xpdf.pages-1\n") != 0)
	{
		fclose(f);
		return;
	}
	int i = 0;
	while (i < maxSavedPageNumbers && fgets(buf, sizeof(buf), f))
	{
		int n = (int)strlen(buf);
		if (n > 0 && buf[n - 1] == '\n')
			buf[n - 1] = '\0';
		char* p = buf;
		while (*p != ' ' && *p)
			++p;
		if (!*p)
			continue;
		*p++                           = '\0';
		savedPageNumbers[i].pageNumber = atoi(buf);
		savedPageNumbers[i].fileName   = QString::fromUtf8(p);
		++i;
	}
	fclose(f);

	// save the timestamp
	savedPagesFileTimestamp = QFileInfo(savedPagesFileName).lastModified();
}

void XpdfApp::writePagesFile()
{
	if (savedPagesFileName.isEmpty())
		return;
	FILE* f = openFile(savedPagesFileName.toUtf8().constData(), "wb");
	if (!f)
		return;
	fprintf(f, "xpdf.pages-1\n");
	for (int i = 0; i < maxSavedPageNumbers; ++i)
		if (!savedPageNumbers[i].fileName.isEmpty())
			fprintf(f, "%d %s\n", savedPageNumbers[i].pageNumber, savedPageNumbers[i].fileName.toUtf8().constData());
	fclose(f);
	savedPagesFileTimestamp = QFileInfo(savedPagesFileName).lastModified();
}
