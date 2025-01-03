//========================================================================
//
// QtPDFCore.cc
//
// Copyright 2009-2014 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include <math.h>
#include <string.h>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFileInfo>
#include <QImage>
#include <QInputDialog>
#include <QMessageBox>
#include <QPainter>
#include <QProcess>
#include <QScreen>
#include <QScrollBar>
#include <QStyle>
#include <QUrl>
#include <QWidget>
#include "gmem.h"
#include "gmempp.h"
#include "gfile.h"
#include "GList.h"
#include "Error.h"
#include "GlobalParams.h"
#include "PDFDoc.h"
#include "Link.h"
#include "ErrorCodes.h"
#include "GfxState.h"
#include "PSOutputDev.h"
#include "TextOutputDev.h"
#include "SplashBitmap.h"
#include "DisplayState.h"
#include "TileMap.h"
#include "QtPDFCore.h"

//------------------------------------------------------------------------
// QtPDFCore
//------------------------------------------------------------------------

QtPDFCore::QtPDFCore(QWidget* viewportA, QScrollBar* hScrollBarA, QScrollBar* vScrollBarA, SplashColorPtr paperColor, SplashColorPtr matteColor, bool reverseVideo)
    : PDFCore(splashModeRGB8, 4, reverseVideo, paperColor)
{
	viewport   = viewportA;
	hScrollBar = hScrollBarA;
	vScrollBar = vScrollBarA;
	hScrollBar->setRange(0, 0);
	hScrollBar->setSingleStep(16);
	vScrollBar->setRange(0, 0);
	vScrollBar->setSingleStep(16);
	viewport->setMouseTracking(true);
	state->setMatteColor(matteColor);
	scaleFactor = computeScaleFactor();
	displayDpi  = computeDisplayDpi();
}

QtPDFCore::~QtPDFCore()
{
}

double QtPDFCore::computeScaleFactor()
{
	// get Qt's HiDPI scale factor
	QGuiApplication* app    = (QGuiApplication*)QGuiApplication::instance();
	QScreen*         screen = app->primaryScreen();
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
	return screen->devicePixelRatio();
#else
	return 1;
#endif
}

int QtPDFCore::computeDisplayDpi()
{
	QGuiApplication* app    = (QGuiApplication*)QGuiApplication::instance();
	QScreen*         screen = app->primaryScreen();
	return (int)(screen->logicalDotsPerInch() * computeScaleFactor());
}

//------------------------------------------------------------------------
// loadFile / displayPage / displayDest
//------------------------------------------------------------------------

int QtPDFCore::loadFile(const std::string& fileName, const std::string& ownerPassword, const std::string& userPassword)
{
	int err = PDFCore::loadFile(fileName, ownerPassword, userPassword);
	if (err == errNone)
	{
		// save the modification time
		modTime = QFileInfo(doc->getFileName().c_str()).lastModified();

		// update the parent window
		if (updateCbk)
			(*updateCbk)(updateCbkData, doc->getFileName(), -1, doc->getNumPages(), nullptr);
		oldFirstPage = oldMidPage = -1;
	}
	return err;
}

int QtPDFCore::loadFile(BaseStream* stream, const std::string& ownerPassword, const std::string& userPassword)
{
	int err;

	err = PDFCore::loadFile(stream, ownerPassword, userPassword);
	if (err == errNone)
	{
		// no file
		modTime = QDateTime();

		// update the parent window
		if (updateCbk)
			(*updateCbk)(updateCbkData, doc->getFileName(), -1, doc->getNumPages(), nullptr);
		oldFirstPage = oldMidPage = -1;
	}
	return err;
}

void QtPDFCore::loadDoc(PDFDoc* docA)
{
	PDFCore::loadDoc(docA);

	// save the modification time
	if (doc->getFileName().size())
		modTime = QFileInfo(doc->getFileName().c_str()).lastModified();
	else
		modTime = QDateTime();

	// update the parent window
	if (updateCbk)
		(*updateCbk)(updateCbkData, doc->getFileName(), -1, doc->getNumPages(), nullptr);
	oldFirstPage = oldMidPage = -1;
}

int QtPDFCore::reload()
{
	int err;

	err = PDFCore::reload();
	if (err == errNone)
	{
		// save the modification time
		modTime = QFileInfo(doc->getFileName().c_str()).lastModified();

		// update the parent window
		if (updateCbk)
			(*updateCbk)(updateCbkData, doc->getFileName(), -1, doc->getNumPages(), nullptr);
		oldFirstPage = oldMidPage = -1;
	}
	return err;
}

void QtPDFCore::finishUpdate(bool addToHist, bool checkForChangedFile)
{
	int firstPage, midPage;

	PDFCore::finishUpdate(addToHist, checkForChangedFile);
	firstPage = getPageNum();
	if (doc && firstPage != oldFirstPage && updateCbk)
		(*updateCbk)(updateCbkData, nullptr, firstPage, -1, "");
	oldFirstPage = firstPage;
	midPage      = getMidPageNum();
	if (doc && midPage != oldMidPage && midPageChangedCbk)
		(*midPageChangedCbk)(midPageChangedCbkData, midPage);
	oldMidPage = midPage;

	linkAction     = nullptr;
	lastLinkAction = nullptr;
}

//------------------------------------------------------------------------
// panning and selection
//------------------------------------------------------------------------

void QtPDFCore::startPan(int wx, int wy)
{
	if (!panEnabled)
		return;
	panning = true;
	panMX   = wx;
	panMY   = wy;
}

void QtPDFCore::endPan(int wx, int wy)
{
	panning = false;
}

void QtPDFCore::startSelection(int wx, int wy, bool extend)
{
	int pg, x, y;

	takeFocus();
	if (!doc || doc->getNumPages() == 0 || !selectEnabled)
		return;
	if (!cvtWindowToDev(wx, wy, &pg, &x, &y))
		return;
	if (extend && hasSelection())
		moveSelectionDrag(pg, x, y);
	else
		startSelectionDrag(pg, x, y);
	if (getSelectMode() == selectModeBlock)
		doSetCursor(Qt::CrossCursor);
	dragging = true;
}

void QtPDFCore::endSelection(int wx, int wy)
{
	LinkAction* action;
	int         pg, x, y;
	double      xu, yu;
	bool        ok;

	if (!doc || doc->getNumPages() == 0)
		return;
	ok = cvtWindowToDev(wx, wy, &pg, &x, &y);
	if (dragging)
	{
		dragging = false;
		doUnsetCursor();
		if (ok)
			moveSelectionDrag(pg, x, y);
		finishSelectionDrag();
		if (selectDoneCbk)
			(*selectDoneCbk)(selectDoneCbkData);
#ifndef NO_TEXT_SELECT
		if (hasSelection())
			copySelection(false);
#endif
	}
	if (ok)
	{
		if (hasSelection())
		{
			action = nullptr;
		}
		else
		{
			cvtDevToUser(pg, x, y, &xu, &yu);
			action = findLink(pg, xu, yu);
		}
		if (linkCbk && action)
			doLinkCbk(action);
		if (hyperlinksEnabled && action)
			doAction(action);
	}
}

void QtPDFCore::mouseMove(int wx, int wy)
{
	LinkAction* action;
	int         pg, x, y;
	double      xu, yu;
	const char* s;
	bool        ok, mouseOverText;

	if (!doc || doc->getNumPages() == 0)
		return;
	ok = cvtWindowToDev(wx, wy, &pg, &x, &y);
	if (dragging)
	{
		if (ok)
			moveSelectionDrag(pg, x, y);
	}
	else
	{
		cvtDevToUser(pg, x, y, &xu, &yu);

		// check for a link
		action = nullptr;
		if (hyperlinksEnabled && ok)
			action = findLink(pg, xu, yu);

		// check for text
		mouseOverText = false;
		if (!action && getSelectMode() == selectModeLinear && ok)
			mouseOverText = overText(pg, x, y);

		// update the cursor
		if (action)
			doSetCursor(Qt::PointingHandCursor);
		else if (mouseOverText)
			doSetCursor(Qt::IBeamCursor);
		else
			doUnsetCursor();

		// update the link info
		if (action != linkAction)
		{
			linkAction = action;
			if (updateCbk)
			{
				//~ should pass a QString to updateCbk()
				if (linkAction)
					s = getLinkInfo(linkAction).toLocal8Bit().constData();
				else
					s = "";
				(*updateCbk)(updateCbkData, nullptr, -1, -1, s);
			}
		}
	}

	if (panning)
	{
		scrollTo(getScrollX() - (wx - panMX), getScrollY() - (wy - panMY));
		panMX = wx;
		panMY = wy;
	}
}

void QtPDFCore::selectWord(int wx, int wy)
{
	int pg, x, y;

	takeFocus();
	if (!doc || doc->getNumPages() == 0 || !selectEnabled)
		return;
	if (getSelectMode() != selectModeLinear)
		return;
	if (!cvtWindowToDev(wx, wy, &pg, &x, &y))
		return;
	PDFCore::selectWord(pg, x, y);
#ifndef NO_TEXT_SELECT
	if (hasSelection())
		copySelection(false);
#endif
}

void QtPDFCore::selectLine(int wx, int wy)
{
	int pg, x, y;

	takeFocus();
	if (!doc || doc->getNumPages() == 0 || !selectEnabled)
		return;
	if (getSelectMode() != selectModeLinear)
		return;
	if (!cvtWindowToDev(wx, wy, &pg, &x, &y))
		return;
	PDFCore::selectLine(pg, x, y);
#ifndef NO_TEXT_SELECT
	if (hasSelection())
		copySelection(false);
#endif
}

void QtPDFCore::doLinkCbk(LinkAction* action)
{
	LinkDest*   dest;
	std::string namedDest;
	Ref         pageRef;
	int         pg;
	std::string cmd, params;
	const char* s;

	if (!linkCbk)
		return;

	switch (action->getKind())
	{
	case actionGoTo:
		dest = nullptr;
		if ((dest = ((LinkGoTo*)action)->getDest()))
			dest = dest->copy();
		else if ((namedDest = ((LinkGoTo*)action)->getNamedDest()).size())
			dest = doc->findDest(namedDest);
		pg = 0;
		if (dest)
		{
			if (dest->isPageRef())
			{
				pageRef = dest->getPageRef();
				pg      = doc->findPage(pageRef.num, pageRef.gen);
			}
			else
			{
				pg = dest->getPageNum();
			}
			delete dest;
		}
		(*linkCbk)(linkCbkData, "goto", nullptr, pg);
		break;

	case actionGoToR:
		(*linkCbk)(linkCbkData, "pdf", ((LinkGoToR*)action)->getFileName().c_str(), 0);
		break;

	case actionLaunch:
		cmd = ((LinkLaunch*)action)->getFileName();
		s   = cmd.c_str();
		if (strcmp(s + cmd.size() - 4, ".pdf") && strcmp(s + cmd.size() - 4, ".PDF") && (params = ((LinkLaunch*)action)->getParams()).size())
		{
			cmd += ' ';
			cmd += params;
		}
		(*linkCbk)(linkCbkData, "launch", cmd.c_str(), 0);
		break;

	case actionURI:
		(*linkCbk)(linkCbkData, "url", ((LinkURI*)action)->getURI().c_str(), 0);
		break;

	case actionNamed:
		(*linkCbk)(linkCbkData, "named", ((LinkNamed*)action)->getName().c_str(), 0);
		break;

	case actionMovie:
	case actionJavaScript:
	case actionSubmitForm:
	case actionHide:
	case actionUnknown:
		(*linkCbk)(linkCbkData, "unknown", nullptr, 0);
		break;
	}
}

QString QtPDFCore::getSelectedTextQString()
{
	std::string s;
	QString     qs;

	if (!doc->okToCopy())
		return "";
	if ((s = getSelectedText()).empty())
		return "";

	const auto enc = globalParams->getTextEncodingName();
	if (enc == "UTF-8")
		qs = QString::fromUtf8(s.c_str());
	else if (enc == "UCS-2")
		for (int i = 0; i + 1 < s.size(); i += 2)
			qs.append((QChar)(((s.at(i) & 0xff) << 8) + (s.at(i + 1) & 0xff)));
	else
		qs = QString(s.c_str());

	return qs;
}

void QtPDFCore::copySelection(bool toClipboard)
{
	QString qs;

	// only X11 has the copy-on-select behavior
	if (!toClipboard && !QApplication::clipboard()->supportsSelection())
		return;
	if (!doc->okToCopy())
		return;
	if (hasSelection())
		QApplication::clipboard()->setText(getSelectedTextQString(), toClipboard ? QClipboard::Clipboard : QClipboard::Selection);
}

//------------------------------------------------------------------------
// hyperlinks
//------------------------------------------------------------------------

bool QtPDFCore::doAction(LinkAction* action)
{
	LinkActionKind kind;
	LinkDest*      dest;
	std::string    namedDest;
	std::string    fileName, fileName2, params;
	std::string    cmd;
	std::string    actionName;
	Object         movieAnnot, obj1, obj2;
	std::string    msg;

	switch (kind = action->getKind())
	{
	// GoTo / GoToR action
	case actionGoTo:
	case actionGoToR:
		if (kind == actionGoTo)
		{
			dest = nullptr;
			if ((dest = ((LinkGoTo*)action)->getDest()))
				dest = dest->copy();
			else if ((namedDest = ((LinkGoTo*)action)->getNamedDest()).size())
				namedDest = namedDest;
		}
		else
		{
			if (!externalHyperlinksEnabled) return false;
			dest = nullptr;
			if ((dest = ((LinkGoToR*)action)->getDest()))
				dest = dest->copy();
			else if ((namedDest = ((LinkGoToR*)action)->getNamedDest()).size())
				namedDest = namedDest;
			const char* s = ((LinkGoToR*)action)->getFileName().c_str();
			if (isAbsolutePath(s))
				fileName = s;
			else
				fileName = appendToPath(grabPath(doc->getFileName().c_str()), s);
			if (loadFile(fileName) != errNone)
			{
				if (dest) delete dest;
				return false;
			}
		}
		if (namedDest.size())
			dest = doc->findDest(namedDest);
		if (dest)
		{
			displayDest(dest);
			delete dest;
		}
		else
		{
			if (kind == actionGoToR)
				displayPage(1, false, false, true);
		}
		break;

	// Launch action
	case actionLaunch:
		if (!externalHyperlinksEnabled) return false;
		fileName = ((LinkLaunch*)action)->getFileName();
		{
			const char* s = fileName.c_str();
			if (fileName.size() >= 4 && (!strcmp(s + fileName.size() - 4, ".pdf") || !strcmp(s + fileName.size() - 4, ".PDF")))
			{
				if (isAbsolutePath(s))
					;
				else
					fileName = appendToPath(grabPath(doc->getFileName().c_str()), s);
				if (loadFile(fileName) != errNone)
					return false;
				displayPage(1, false, false, true);
			}
			else
			{
				cmd = fileName;
				if ((params = ((LinkLaunch*)action)->getParams()).size())
				{
					cmd += ' ';
					cmd += params;
				}
				if (globalParams->getLaunchCommand().size())
				{
					cmd.insert(0, " ");
					cmd.insert(0, globalParams->getLaunchCommand());
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
					QString     cmdStr(cmd.c_str());
					QStringList tokens = QProcess::splitCommand(cmdStr);
					if (!tokens.isEmpty())
					{
						QString program = tokens[0];
						tokens.removeFirst();
						QProcess::startDetached(program, tokens);
					}
#else
					QProcess::startDetached(cmd.c_str());
#endif
				}
				else
				{
					msg = "About to execute the command:\n";
					msg += cmd;
					if (QMessageBox::question(viewport, "PDF Launch Link", msg.c_str(), QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Ok) == QMessageBox::Ok)
					{
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
						QString     cmdStr(cmd.c_str());
						QStringList tokens = QProcess::splitCommand(cmdStr);
						if (!tokens.isEmpty())
						{
							QString program = tokens[0];
							tokens.removeFirst();
							QProcess::startDetached(program, tokens);
						}
#else
						QProcess::startDetached(cmd.c_str());
#endif
					}
				}
			}
		}
		break;

	// URI action
	case actionURI:
		if (!externalHyperlinksEnabled) return false;
		QDesktopServices::openUrl(QUrl(((LinkURI*)action)->getURI().c_str(), QUrl::TolerantMode));
		break;

	// Named action
	case actionNamed:
		actionName = ((LinkNamed*)action)->getName();
		if (actionName == "NextPage")
		{
			gotoNextPage(1, true);
		}
		else if (actionName == "PrevPage")
		{
			gotoPrevPage(1, true, false);
		}
		else if (actionName == "FirstPage")
		{
			displayPage(1, true, false, true);
		}
		else if (actionName == "LastPage")
		{
			displayPage(doc->getNumPages(), true, false, true);
		}
		else if (actionName == "GoBack")
		{
			goBackward();
		}
		else if (actionName == "GoForward")
		{
			goForward();
		}
		else if (actionName == "Quit")
		{
			if (actionCbk)
				(*actionCbk)(actionCbkData, actionName.c_str());
		}
		else
		{
			error(errSyntaxError, -1, "Unknown named action: '{}'", actionName);
			return false;
		}
		break;

	// Movie action
	case actionMovie:
		if (!externalHyperlinksEnabled)
			return false;
		if ((cmd = globalParams->getMovieCommand()).empty())
		{
			error(errConfig, -1, "No movieCommand defined in config file");
			return false;
		}
		if (((LinkMovie*)action)->hasAnnotRef())
		{
			doc->getXRef()->fetch(((LinkMovie*)action)->getAnnotRef()->num, ((LinkMovie*)action)->getAnnotRef()->gen, &movieAnnot);
		}
		else
		{
			//~ need to use the correct page num here
			doc->getCatalog()->getPage(tileMap->getFirstPage())->getAnnots(&obj1);
			if (obj1.isArray())
			{
				for (int i = 0; i < obj1.arrayGetLength(); ++i)
				{
					if (obj1.arrayGet(i, &movieAnnot)->isDict())
					{
						if (movieAnnot.dictLookup("Subtype", &obj2)->isName("Movie"))
						{
							obj2.free();
							break;
						}
						obj2.free();
					}
					movieAnnot.free();
				}
				obj1.free();
			}
		}
		if (movieAnnot.isDict())
		{
			if (movieAnnot.dictLookup("Movie", &obj1)->isDict())
			{
				if (obj1.dictLookup("F", &obj2))
				{
					if ((fileName = LinkAction::getFileSpecName(&obj2)).size())
					{
						if (!isAbsolutePath(fileName.c_str()))
						{
							fileName2 = appendToPath(grabPath(doc->getFileName().c_str()), fileName.c_str());
							fileName  = fileName2;
						}
						runCommand(cmd, fileName);
					}
					obj2.free();
				}
				obj1.free();
			}
		}
		movieAnnot.free();
		break;

	// unimplemented actions
	case actionJavaScript:
	case actionSubmitForm:
	case actionHide:
		return false;

	// unknown action type
	case actionUnknown:
		error(errSyntaxError, -1, "Unknown link action type: '{}'", ((LinkUnknown*)action)->getAction());
		return false;
	}

	return true;
}

QString QtPDFCore::getLinkInfo(LinkAction* action)
{
	LinkDest*   dest;
	std::string namedDest;
	Ref         pageRef;
	int         pg;
	QString     info;

	if (action == lastLinkAction && !lastLinkActionInfo.isEmpty())
		return lastLinkActionInfo;

	switch (action->getKind())
	{
	case actionGoTo:
		dest = nullptr;
		if ((dest = ((LinkGoTo*)action)->getDest()))
			dest = dest->copy();
		else if ((namedDest = ((LinkGoTo*)action)->getNamedDest()).size())
			dest = doc->findDest(namedDest);
		pg = 0;
		if (dest)
		{
			if (dest->isPageRef())
			{
				pageRef = dest->getPageRef();
				pg      = doc->findPage(pageRef.num, pageRef.gen);
			}
			else
			{
				pg = dest->getPageNum();
			}
			delete dest;
		}
		if (pg)
			info = QString("[page ") + QString::number(pg) + QString("]");
		else
			info = "[internal]";
		break;
	case actionGoToR:
		info = QString(((LinkGoToR*)action)->getFileName().c_str());
		break;
	case actionLaunch:
		info = QString(((LinkLaunch*)action)->getFileName().c_str());
		break;
	case actionURI:
		info = QString(((LinkURI*)action)->getURI().c_str());
		break;
	case actionNamed:
		info = QString(((LinkNamed*)action)->getName().c_str());
		break;
	case actionMovie:
		info = "[movie]";
		break;
	case actionJavaScript:
	case actionSubmitForm:
	case actionHide:
	case actionUnknown:
	default:
		info = "[unknown]";
		break;
	}

	lastLinkAction     = action;
	lastLinkActionInfo = info;

	return info;
}

// Run a command, given a <cmdFmt> string with one '%s' in it, and an
// <arg> string to insert in place of the '%s'.
void QtPDFCore::runCommand(const std::string& cmdFmt, const std::string& arg)
{
	std::string cmd;
	const char* s;

	if ((s = strstr(cmdFmt.c_str(), "%s")))
	{
		cmd = mungeURL(arg);
		cmd.insert(0, cmdFmt.c_str(), (int)(s - cmdFmt.c_str()));
		cmd.append(s + 2);
	}
	else
	{
		cmd = cmdFmt;
	}
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
	QString     cmdStr(cmd.c_str());
	QStringList tokens = QProcess::splitCommand(cmdStr);
	if (!tokens.isEmpty())
	{
		QString program = tokens[0];
		tokens.removeFirst();
		QProcess::startDetached(program, tokens);
	}
#else
	QProcess::startDetached(cmd.c_str());
#endif
}

// Escape any characters in a URL which might cause problems when calling system().
std::string QtPDFCore::mungeURL(const std::string& url)
{
	static const char* allowed =
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	    "abcdefghijklmnopqrstuvwxyz"
	    "0123456789"
	    "-_.~/?:@&=+,#%";

	std::string newURL;
	for (int i = 0; i < url.size(); ++i)
	{
		const char c = url.at(i);
		if (strchr(allowed, c))
			newURL += c;
		else
			newURL += fmt::format("%{:02x}", c & 0xff);
	}
	return newURL;
}

//------------------------------------------------------------------------
// find
//------------------------------------------------------------------------

bool QtPDFCore::find(char* s, bool caseSensitive, bool next, bool backward, bool wholeWord, bool onePageOnly)
{
	if (!PDFCore::find(s, caseSensitive, next, backward, wholeWord, onePageOnly))
		return false;
#ifndef NO_TEXT_SELECT
	copySelection(false);
#endif
	return true;
}

bool QtPDFCore::findU(Unicode* u, int len, bool caseSensitive, bool next, bool backward, bool wholeWord, bool onePageOnly)
{
	if (!PDFCore::findU(u, len, caseSensitive, next, backward, wholeWord, onePageOnly))
		return false;
#ifndef NO_TEXT_SELECT
	copySelection(false);
#endif
	return true;
}

//------------------------------------------------------------------------
// misc access
//------------------------------------------------------------------------

void QtPDFCore::setBusyCursor(bool busy)
{
	if (busy)
		doSetCursor(Qt::WaitCursor);
	else
		doUnsetCursor();
}

void QtPDFCore::doSetCursor(const QCursor& cursor)
{
#ifndef QT_NO_CURSOR
	viewport->setCursor(cursor);
#endif
}

void QtPDFCore::doUnsetCursor()
{
#ifndef QT_NO_CURSOR
	viewport->unsetCursor();
#endif
}

void QtPDFCore::takeFocus()
{
	viewport->setFocus(Qt::OtherFocusReason);
}

QSize QtPDFCore::getBestSize()
{
	DisplayMode mode;
	double      zoomPercent;
	int         w, h, pg, rot;

	if (!doc || doc->getNumPages() == 0)
	{
		//~ what should this return?
		return QSize(612, 792);
	}
	mode        = state->getDisplayMode();
	pg          = tileMap->getFirstPage();
	rot         = (state->getRotate() + doc->getPageRotate(pg)) % 360;
	zoomPercent = state->getZoom();
	if (zoomPercent < 0)
	{
		zoomPercent = globalParams->getDefaultFitZoom();
		if (zoomPercent <= 0)
		{
			zoomPercent = (int)((100 * displayDpi) / 72.0 + 0.5);
			if (zoomPercent < 100)
				zoomPercent = 100;
		}
	}
	if (rot == 90 || rot == 270)
	{
		w = (int)(doc->getPageCropHeight(pg) * 0.01 * zoomPercent + 0.5);
		h = (int)(doc->getPageCropWidth(pg) * 0.01 * zoomPercent + 0.5);
	}
	else
	{
		w = (int)(doc->getPageCropWidth(pg) * 0.01 * zoomPercent + 0.5);
		h = (int)(doc->getPageCropHeight(pg) * 0.01 * zoomPercent + 0.5);
	}
	if (mode == displayContinuous)
	{
		w += QApplication::style()->pixelMetric(QStyle::PM_ScrollBarExtent);
		h += tileMap->getContinuousPageSpacing();
	}
	else if (mode == displaySideBySideContinuous)
	{
		w = w * 2 + tileMap->getHorizContinuousPageSpacing() + QApplication::style()->pixelMetric(QStyle::PM_ScrollBarExtent);
		h += tileMap->getContinuousPageSpacing();
	}
	else if (mode == displayHorizontalContinuous)
	{
		w += tileMap->getHorizContinuousPageSpacing();
		h += QApplication::style()->pixelMetric(QStyle::PM_ScrollBarExtent);
	}
	else if (mode == displaySideBySideSingle)
	{
		w = w * 2 + tileMap->getHorizContinuousPageSpacing();
	}
	//~ these additions are a kludge to make this work -- 2 pixels are
	//~   padding in the QAbstractScrollArea; not sure where the rest go
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
	w += 6;
	h += 2;
#else
	w += 10;
	h += 4;
#endif
	return QSize((int)(w / scaleFactor), (int)(h / scaleFactor));
}

//------------------------------------------------------------------------
// GUI code
//------------------------------------------------------------------------

void QtPDFCore::resizeEvent()
{
	setWindowSize((int)(viewport->width() * scaleFactor), (int)(viewport->height() * scaleFactor));
}

void QtPDFCore::paintEvent(int x, int y, int w, int h)
{
	SplashBitmap* bitmap;
	bool          wholeWindow;

	QPainter painter(viewport);
	wholeWindow = x == 0 && y == 0 && w == viewport->width() && h == viewport->height();
	bitmap      = getWindowBitmap(wholeWindow);
	QImage image(bitmap->getDataPtr(), bitmap->getWidth(), bitmap->getHeight(), QImage::Format_RGB888);
	if (scaleFactor == 1)
		painter.drawImage(QRect(x, y, w, h), image, QRect(x, y, w, h));
	else
		painter.drawImage(QRectF(x, y, w, h), image, QRectF(x * scaleFactor, y * scaleFactor, w * scaleFactor, h * scaleFactor));
	if (paintDoneCbk)
		(*paintDoneCbk)(paintDoneCbkData, (bool)isBitmapFinished());
}

void QtPDFCore::scrollEvent()
{
	// avoid loops, e.g., scrollTo -> finishUpdate -> updateScrollbars ->
	// hScrollbar.setValue -> scrollContentsBy -> scrollEvent -> scrollTo
	if (inUpdateScrollbars)
		return;
	scrollTo(hScrollBar->value(), vScrollBar->value());
}

void QtPDFCore::tick()
{
	PDFCore::tick();
}

void QtPDFCore::invalidate(int x, int y, int w, int h)
{
	int xx, yy, ww, hh;

	if (scaleFactor == 1)
	{
		viewport->update(x, y, w, h);
	}
	else
	{
		xx = (int)(x / scaleFactor);
		yy = (int)(y / scaleFactor);
		ww = (int)ceil((x + w) / scaleFactor) - xx;
		hh = (int)ceil((y + h) / scaleFactor) - yy;
		viewport->update(xx, yy, ww, hh);
	}
}

void QtPDFCore::updateScrollbars()
{
	int  winW, winH, horizLimit, vertLimit, horizMax, vertMax;
	bool vScrollBarVisible, hScrollBarVisible;

	inUpdateScrollbars = true;

	winW = state->getWinW();
	winH = state->getWinH();
	tileMap->getScrollLimits(&horizLimit, &vertLimit);

	if (horizLimit > winW)
		horizMax = horizLimit - winW;
	else
		horizMax = 0;
	if (vertLimit > winH)
		vertMax = vertLimit - winH;
	else
		vertMax = 0;

	// Problem case A: in fixed zoom, there is a case where the page
	// just barely fits in the window; if the scrollbars are visible,
	// they reduce the available window size enough that they are
	// necessary, i.e., the scrollbars are only necessary if they're
	// visible -- so check for that situation and force the scrollbars
	// to be hidden.
	// NB: {h,v}ScrollBar->isVisible() are unreliable at startup, so
	//     we compare the viewport size to the ScrollArea size (with
	//     some slack for margins)
	vScrollBarVisible = viewport->parentWidget()->width() - viewport->width() > 8;
	hScrollBarVisible = viewport->parentWidget()->height() - viewport->height() > 8;
	if (state->getZoom() >= 0 && vScrollBarVisible && hScrollBarVisible && horizMax <= vScrollBar->width() && vertMax <= hScrollBar->height())
	{
		horizMax = 0;
		vertMax  = 0;
	}

	// Problem case B: in fit-to-width mode, with the vertical scrollbar
	// visible, if the window is just tall enough to fit the page, then
	// the vertical scrollbar will be hidden, resulting in a wider
	// window, resulting in a taller page (because of fit-to-width),
	// resulting in the scrollbar being unhidden, in an infinite loop --
	// so always force the vertical scroll bar to be visible in
	// fit-to-width mode (and in fit-to-page cases where the vertical
	// scrollbar is potentially visible).
	if (state->getZoom() == zoomWidth || (state->getZoom() == zoomPage && (state->getDisplayMode() == displayContinuous || state->getDisplayMode() == displaySideBySideContinuous)))
	{
		if (vertMax == 0)
			vertMax = 1;

		// Problem case C: same as case B, but for fit-to-height mode and
		// the horizontal scrollbar.
	}
	else if (state->getZoom() == zoomHeight || (state->getZoom() == zoomPage && state->getDisplayMode() == displayHorizontalContinuous))
	{
		if (horizMax == 0)
			horizMax = 1;
	}

	hScrollBar->setMaximum(horizMax);
	hScrollBar->setPageStep(winW);
	hScrollBar->setValue(state->getScrollX());

	vScrollBar->setMaximum(vertMax);
	vScrollBar->setPageStep(winH);
	vScrollBar->setValue(state->getScrollY());

	inUpdateScrollbars = false;
}

bool QtPDFCore::checkForNewFile()
{
	QDateTime newModTime;

	if (doc->getFileName().size())
	{
		newModTime = QFileInfo(doc->getFileName().c_str()).lastModified();
		if (newModTime != modTime)
		{
			modTime = newModTime;
			return true;
		}
	}
	return false;
}

void QtPDFCore::preLoad()
{
	if (preLoadCbk)
		(*preLoadCbk)(preLoadCbkData);
}

void QtPDFCore::postLoad()
{
	if (postLoadCbk)
		(*postLoadCbk)(postLoadCbkData);
}

//------------------------------------------------------------------------
// password dialog
//------------------------------------------------------------------------

std::string QtPDFCore::getPassword()
{
	bool ok;
	if (!showPasswordDialog) return "";
	const auto s = QInputDialog::getText(viewport, "PDF Password", "This document requires a password", QLineEdit::Password, "", &ok, Qt::Dialog);
	if (ok)
		return s.toStdString();
	else
		return "";
}
