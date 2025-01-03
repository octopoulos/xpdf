//========================================================================
//
// XpdfWidget.cc
//
// Copyright 2009 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>

#ifdef _WIN32
#	include <windows.h>
#endif
#include <QApplication>
#include <QMutex>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QTimer>
#include <QAbstractScrollArea>
#include <QGesture>
#include <QGestureEvent>
#if XPDFWIDGET_PRINTING
#	include <QPrinter>
#	include <QPrintDialog>
#endif
#include "gmem.h"
#include "gmempp.h"
#include "gfile.h"
#include "GList.h"
#include "SplashBitmap.h"
#include "config.h"
#include "GlobalParams.h"
#include "PDFDoc.h"
#include "Object.h"
#include "SplashOutputDev.h"
#include "Outline.h"
#include "OptionalContent.h"
#include "Link.h"
#include "Annot.h"
#include "AcroForm.h"
#include "TextString.h"
#include "QtPDFCore.h"
#include "XpdfWidget.h"
#if XPDFWIDGET_PRINTING
#	include "XpdfWidgetPrint.h"
#endif

//------------------------------------------------------------------------

// Time (in ms) between incremental updates.
#define incrementalUpdateInterval 100

//------------------------------------------------------------------------

QMutex XpdfWidget::initMutex;

//------------------------------------------------------------------------

XpdfWidget::XpdfWidget(const QColor& paperColor, const QColor& matteColor, bool reverseVideo, QWidget* parentA)
    : QAbstractScrollArea(parentA)
{
	setup(paperColor, matteColor, reverseVideo);
}

XpdfWidget::XpdfWidget(QWidget* parentA, const QColor& paperColor, const QColor& matteColor, bool reverseVideo)
    : QAbstractScrollArea(parentA)
{
	setup(paperColor, matteColor, reverseVideo);
}

void XpdfWidget::setup(const QColor& paperColor, const QColor& matteColor, bool reverseVideo)
{
	SplashColor paperColor2, matteColor2;

	init();

	paperColor2[0] = (uint8_t)paperColor.red();
	paperColor2[1] = (uint8_t)paperColor.green();
	paperColor2[2] = (uint8_t)paperColor.blue();
	matteColor2[0] = (uint8_t)matteColor.red();
	matteColor2[1] = (uint8_t)matteColor.green();
	matteColor2[2] = (uint8_t)matteColor.blue();
	try
	{
		core = new QtPDFCore(viewport(), horizontalScrollBar(), verticalScrollBar(), paperColor2, matteColor2, (bool)reverseVideo);
		core->setUpdateCbk(updateCbk, this);
		core->setMidPageChangedCbk(midPageChangedCbk, this);
		core->setPreLoadCbk(preLoadCbk, this);
		core->setPostLoadCbk(postLoadCbk, this);
		core->setLinkCbk(linkCbk, this);
		core->setSelectDoneCbk(selectDoneCbk, this);
		core->setPaintDoneCbk(paintDoneCbk, this);
		core->setTileDoneCbk(tileDoneCbk, this);
		connect(this, SIGNAL(tileDone()), viewport(), SLOT(update()));
		scaleFactor = core->getScaleFactor();
	}
	catch (GMemException e)
	{
		//~ what should this do?
		core = nullptr;
	}

#if XPDFWIDGET_PRINTING
	printerForDialog = nullptr;
	printDialog      = nullptr;
	printHDPI = printVDPI = 0;
#endif

	keyPassthrough     = false;
	mousePassthrough   = false;
	lastMousePressX[0] = lastMousePressX[1] = lastMousePressX[2] = 0;
	lastMousePressY[0] = lastMousePressY[1] = lastMousePressY[2] = 0;
	lastMousePressTime[0] = lastMousePressTime[1] = lastMousePressTime[2] = 0;
	lastMouseEventWasPress                                                = false;

	viewport()->installEventFilter(this);
	touchPanEnabled  = false;
	touchZoomEnabled = false;
	pinchZoomStart   = 100;

	tickTimer = new QTimer(this);
	connect(tickTimer, SIGNAL(timeout()), this, SLOT(tick()));
	tickTimer->start(incrementalUpdateInterval);
}

XpdfWidget::~XpdfWidget()
{
#if XPDFWIDGET_PRINTING
	delete printerForDialog;
	delete printDialog;
#endif
	delete tickTimer;
	delete core;
}

void XpdfWidget::init(const QString& configFileName)
{
#ifdef _WIN32
	char        buf[512];
	std::string dir, path;
	const char* configFileName2;
	int         n;
#endif

	initMutex.lock();
	if (!globalParams)
	{
		try
		{
#ifdef _WIN32
			// get the executable directory
			n = GetModuleFileNameA(nullptr, buf, sizeof(buf));
			if (n <= 0 || n >= sizeof(buf))
			{
				// error or path too long for buffer - just use the current dir
				buf[0] = '\0';
			}
			dir = grabPath(buf);

			// load the config file
			path = nullptr;
			if (configFileName.isEmpty())
			{
				path            = appendToPath(dir, xpdfSysConfigFile);
				configFileName2 = path.c_str();
			}
			else
			{
				configFileName2 = (char*)configFileName.toLocal8Bit().constData();
			}
			globalParams = std::make_shared<GlobalParams>(configFileName2);
			globalParams->setBaseDir(dir.c_str());
			globalParams->setErrQuiet(true);

			// set up the base fonts
			appendToPath(dir, "t1fonts");
			globalParams->setupBaseFonts(dir.c_str());
#else
			globalParams = std::make_shared<GlobalParams>((char*)configFileName.toLocal8Bit().constData());
			globalParams->setErrQuiet(true);
			globalParams->setupBaseFonts(nullptr);
#endif
		}
		catch (GMemException e)
		{
			// there's no way to return an error code here
			return;
		}
	}
	initMutex.unlock();
}

void XpdfWidget::setConfig(const QString& command)
{
	init();
	try
	{
		const std::string fileName = "(none)";
		globalParams->parseLine((char*)command.toLocal8Bit().constData(), fileName, 1);
	}
	catch (GMemException e)
	{
	}
}

void XpdfWidget::enableHyperlinks(bool on)
{
	try
	{
		core->enableHyperlinks((bool)on);
	}
	catch (GMemException e)
	{
	}
}

void XpdfWidget::enableExternalHyperlinks(bool on)
{
	try
	{
		core->enableExternalHyperlinks((bool)on);
	}
	catch (GMemException e)
	{
	}
}

void XpdfWidget::enableSelect(bool on)
{
	try
	{
		core->enableSelect((bool)on);
	}
	catch (GMemException e)
	{
	}
}

void XpdfWidget::enablePan(bool on)
{
	try
	{
		core->enablePan((bool)on);
	}
	catch (GMemException e)
	{
	}
}

void XpdfWidget::enableTouchPan(bool on)
{
	touchPanEnabled = on;
	if (touchPanEnabled)
		viewport()->grabGesture(Qt::PanGesture);
	else
		viewport()->ungrabGesture(Qt::PanGesture);
}

void XpdfWidget::enableTouchZoom(bool on)
{
	touchZoomEnabled = on;
	if (touchZoomEnabled)
		viewport()->grabGesture(Qt::PinchGesture);
	else
		viewport()->ungrabGesture(Qt::PinchGesture);
}

void XpdfWidget::showPasswordDialog(bool showDlg)
{
	try
	{
		core->setShowPasswordDialog((bool)showDlg);
	}
	catch (GMemException e)
	{
	}
}

void XpdfWidget::setMatteColor(const QColor& matteColor)
{
	SplashColor matteColor2;

	matteColor2[0] = (uint8_t)matteColor.red();
	matteColor2[1] = (uint8_t)matteColor.green();
	matteColor2[2] = (uint8_t)matteColor.blue();
	try
	{
		core->setMatteColor(matteColor2);
	}
	catch (GMemException e)
	{
	}
}

void XpdfWidget::setReverseVideo(bool reverse)
{
	try
	{
		core->setReverseVideo((bool)reverse);
	}
	catch (GMemException e)
	{
	}
}

void XpdfWidget::setCursor(const QCursor& cursor)
{
	core->doSetCursor(cursor);
}

void XpdfWidget::unsetCursor()
{
	core->doUnsetCursor();
}

XpdfWidget::ErrorCode XpdfWidget::loadFile(const QString& fileName, const QString& password)
{
	std::string passwordStr;
	std::string fileNameStr;
	int         err;

	try
	{
		if (!password.isEmpty()) passwordStr = password.toStdString();
		fileNameStr = fileName.toStdString();
		err         = core->loadFile(fileNameStr, passwordStr, passwordStr);
		if (!err) core->displayPage(1, true, false);
		return (ErrorCode)err;
	}
	catch (GMemException e)
	{
		return pdfErrOutOfMemory;
	}
}

XpdfWidget::ErrorCode XpdfWidget::loadMem(const char* buffer, uint32_t bufferLength, const QString& password)
{
	Object     obj;
	MemStream* stream;
	int        err;

	try
	{
		obj.initNull();
		stream = new MemStream((char*)buffer, 0, bufferLength, &obj);
		err    = core->loadFile(stream, password.toStdString(), password.toStdString());
		if (!err)
			core->displayPage(1, true, false);
		return (ErrorCode)err;
	}
	catch (GMemException e)
	{
		return pdfErrOutOfMemory;
	}
}

XpdfWidget::ErrorCode XpdfWidget::readDoc(XpdfDocHandle* docPtr, const QString& fileName, const QString& password)
{
	ErrorCode err;
	try
	{
		const auto doc = new PDFDoc(fileName.toStdString(), password.toStdString(), password.toStdString(), core);
		if (doc->isOk())
		{
			*docPtr = doc;
			err     = pdfOk;
		}
		else
		{
			err = (ErrorCode)doc->getErrorCode();
			delete doc;
		}
	}
	catch (GMemException e)
	{
		err = pdfErrOutOfMemory;
	}
	return err;
}

XpdfWidget::ErrorCode XpdfWidget::loadDoc(XpdfDocHandle doc)
{
	if (!doc)
		return pdfErrNoHandle;
	core->loadDoc((PDFDoc*)doc);
	core->displayPage(1, true, false, true);
	return pdfOk;
}

void XpdfWidget::freeDoc(XpdfDocHandle doc)
{
	if (!doc)
		return;
	delete (PDFDoc*)doc;
}

XpdfWidget::ErrorCode XpdfWidget::reload()
{
	try
	{
		return (ErrorCode)core->reload();
	}
	catch (GMemException e)
	{
		return pdfErrOutOfMemory;
	}
}

void XpdfWidget::closeFile()
{
	try
	{
		core->clear();
	}
	catch (GMemException e)
	{
	}
}

XpdfWidget::ErrorCode XpdfWidget::saveAs(const QString& fileName)
{
	try
	{
		if (!core->getDoc())
			return pdfErrNoHandle;
		if (!core->getDoc()->saveAs(fileName.toStdString()))
			return pdfErrOpenFile;
		return pdfOk;
	}
	catch (GMemException e)
	{
		return pdfErrOutOfMemory;
	}
}

bool XpdfWidget::hasOpenDocument() const
{
	try
	{
		return core->getDoc() != nullptr;
	}
	catch (GMemException e)
	{
		return false;
	}
}

QString XpdfWidget::getFileName() const
{
	try
	{
		if (!core->getDoc() || core->getDoc()->getFileName().empty()) return QString();
		return QString::fromLocal8Bit(core->getDoc()->getFileName().c_str());
	}
	catch (GMemException e)
	{
		return QString();
	}
}

int XpdfWidget::getNumPages() const
{
	try
	{
		if (!core->getDoc())
			return -1;
		return core->getDoc()->getNumPages();
	}
	catch (GMemException e)
	{
		return -1;
	}
}

int XpdfWidget::getCurrentPage() const
{
	try
	{
		return core->getPageNum();
	}
	catch (GMemException e)
	{
		return -1;
	}
}

int XpdfWidget::getMidPage() const
{
	try
	{
		return core->getMidPageNum();
	}
	catch (GMemException e)
	{
		return -1;
	}
}

void XpdfWidget::gotoPage(int pageNum)
{
	try
	{
		core->displayPage(pageNum, true, false);
	}
	catch (GMemException e)
	{
	}
}

void XpdfWidget::gotoFirstPage()
{
	try
	{
		core->displayPage(1, true, false);
	}
	catch (GMemException e)
	{
	}
}

void XpdfWidget::gotoLastPage()
{
	try
	{
		if (!core->getDoc())
			return;
		core->displayPage(core->getDoc()->getNumPages(), true, false);
	}
	catch (GMemException e)
	{
	}
}

void XpdfWidget::gotoNextPage(bool scrollToTop)
{
	try
	{
		core->gotoNextPage(1, scrollToTop);
	}
	catch (GMemException e)
	{
	}
}

void XpdfWidget::gotoPreviousPage(bool scrollToTop)
{
	try
	{
		core->gotoPrevPage(1, scrollToTop, false);
	}
	catch (GMemException e)
	{
	}
}

bool XpdfWidget::gotoNamedDestination(const QString& dest)
{
	try
	{
		if (!core->gotoNamedDestination(dest.toStdString()))
			return false;
		return true;
	}
	catch (GMemException e)
	{
		return false;
	}
}

void XpdfWidget::goForward()
{
	try
	{
		core->goForward();
	}
	catch (GMemException e)
	{
	}
}

void XpdfWidget::goBackward()
{
	try
	{
		core->goBackward();
	}
	catch (GMemException e)
	{
	}
}

void XpdfWidget::scrollPageUp()
{
	try
	{
		core->scrollPageUp();
	}
	catch (GMemException e)
	{
	}
}

void XpdfWidget::scrollPageDown()
{
	try
	{
		core->scrollPageDown();
	}
	catch (GMemException e)
	{
	}
}

void XpdfWidget::scrollTo(int xx, int yy)
{
	try
	{
		core->scrollTo(xx, yy);
	}
	catch (GMemException e)
	{
	}
}

void XpdfWidget::scrollBy(int dx, int dy)
{
	try
	{
		core->scrollTo(core->getScrollX() + dx, core->getScrollY() + dy);
	}
	catch (GMemException e)
	{
	}
}

int XpdfWidget::getScrollX() const
{
	try
	{
		return core->getScrollX();
	}
	catch (GMemException e)
	{
		return 0;
	}
}

int XpdfWidget::getScrollY() const
{
	try
	{
		return core->getScrollY();
	}
	catch (GMemException e)
	{
		return 0;
	}
}

void XpdfWidget::setZoom(double zoom)
{
	try
	{
		core->setZoom(zoom);
	}
	catch (GMemException e)
	{
	}
}

double XpdfWidget::getZoom() const
{
	try
	{
		return core->getZoom();
	}
	catch (GMemException e)
	{
		return 0;
	}
}

double XpdfWidget::getZoomPercent(int page) const
{
	double zoom;

	try
	{
		if (!core->getDoc() || page < 1 || page > core->getDoc()->getNumPages())
			return 0;
		zoom = core->getZoom();
		if (zoom <= 0)
			zoom = 100 * core->getZoomDPI(page) / 72;
		return zoom;
	}
	catch (GMemException e)
	{
		return 0;
	}
}

void XpdfWidget::zoomToRect(int page, double xMin, double yMin, double xMax, double yMax)
{
	try
	{
		core->zoomToRect(page, xMin, yMin, xMax, yMax);
	}
	catch (GMemException e)
	{
	}
}

void XpdfWidget::zoomCentered(double zoom)
{
	try
	{
		core->zoomCentered(zoom);
	}
	catch (GMemException e)
	{
	}
}

void XpdfWidget::zoomToCurrentWidth()
{
	try
	{
		core->zoomToCurrentWidth();
	}
	catch (GMemException e)
	{
	}
}

void XpdfWidget::setRotate(int rotate)
{
	if (!(rotate == 0 || rotate == 90 || rotate == 180 || rotate == 270))
		return;
	try
	{
		core->setRotate(rotate);
	}
	catch (GMemException e)
	{
	}
}

int XpdfWidget::getRotate() const
{
	try
	{
		return core->getRotate();
	}
	catch (GMemException e)
	{
		return 0;
	}
}

void XpdfWidget::setContinuousMode(bool continuous)
{
	try
	{
		core->setDisplayMode(continuous ? displayContinuous : displaySingle);
	}
	catch (GMemException e)
	{
	}
}

bool XpdfWidget::getContinuousMode() const
{
	try
	{
		return core->getDisplayMode() == displayContinuous;
	}
	catch (GMemException e)
	{
		return false;
	}
}

void XpdfWidget::setDisplayMode(XpdfWidget::DisplayMode mode)
{
	try
	{
		core->setDisplayMode((::DisplayMode)mode);
	}
	catch (GMemException e)
	{
	}
}

XpdfWidget::DisplayMode XpdfWidget::getDisplayMode()
{
	try
	{
		return (XpdfWidget::DisplayMode)core->getDisplayMode();
	}
	catch (GMemException e)
	{
		return pdfDisplaySingle;
	}
}

bool XpdfWidget::mouseOverLink()
{
	QPoint pt;
	int    page;
	double xx, yy;

	try
	{
		if (core->getHyperlinksEnabled())
		{
			return core->getLinkAction() != nullptr;
		}
		else
		{
			pt = mapFromGlobal(QCursor::pos());
			if (!convertWindowToPDFCoords(pt.x(), pt.y(), &page, &xx, &yy))
				return false;
			return core->findLink(page, xx, yy) != nullptr;
		}
	}
	catch (GMemException e)
	{
		return false;
	}
}

bool XpdfWidget::onLink(int page, double xx, double yy)
{
	try
	{
		if (!core->getDoc())
			return false;
		if (page < 1 || page > core->getDoc()->getNumPages())
			return false;
		return core->findLink(page, xx, yy) != nullptr;
	}
	catch (GMemException e)
	{
		return false;
	}
}

QString XpdfWidget::getLinkInfo(int page, double xx, double yy)
{
	LinkAction* action;

	try
	{
		if (!core->getDoc())
			return QString();
		if (page < 1 || page > core->getDoc()->getNumPages())
			return QString();
		if (!(action = core->findLink(page, xx, yy)))
			return QString();
		return core->getLinkInfo(action);
	}
	catch (GMemException e)
	{
		return QString();
	}
}

QString XpdfWidget::getMouseLinkInfo()
{
	try
	{
		if (!core->getLinkAction())
			return QString();
		return core->getLinkInfo(core->getLinkAction());
	}
	catch (GMemException e)
	{
		return QString();
	}
}

bool XpdfWidget::gotoLinkAt(int page, double xx, double yy)
{
	LinkAction* action;

	try
	{
		if (!core->getDoc())
			return false;
		if (page < 1 || page > core->getDoc()->getNumPages())
			return false;
		if ((action = core->findLink(page, xx, yy)))
		{
			if (!core->doAction(action))
				return false;
		}
	}
	catch (GMemException e)
	{
	}
	return true;
}

bool XpdfWidget::getLinkTarget(int page, double xx, double yy, QString& targetFileName, int& targetPage, QString& targetDest)
{
	LinkAction* action;
	LinkDest*   dest;
	std::string fileName, namedDest, path;
	Ref         pageRef;
	const char* s;

	try
	{
		if (!core->getDoc())
			return false;
		if (page < 1 || page > core->getDoc()->getNumPages())
			return false;
		if (!(action = core->findLink(page, xx, yy)))
			return false;
		switch (action->getKind())
		{
		case actionGoTo:
			if (core->getDoc()->getFileName().empty())
				return false;
			targetFileName = core->getDoc()->getFileName().c_str();
			if ((dest = ((LinkGoTo*)action)->getDest()))
			{
				if (dest->isPageRef())
				{
					pageRef    = dest->getPageRef();
					targetPage = core->getDoc()->findPage(pageRef.num, pageRef.gen);
				}
				else
				{
					targetPage = dest->getPageNum();
				}
				targetDest = "";
			}
			else if ((namedDest = ((LinkGoTo*)action)->getNamedDest()).size())
			{
				targetDest = namedDest.c_str();
				targetPage = 1;
			}
			return true;
		case actionGoToR:
			s = ((LinkGoToR*)action)->getFileName().c_str();
			if (isAbsolutePath(s))
			{
				targetFileName = s;
			}
			else
			{
				if (core->getDoc()->getFileName().empty())
					return false;
				path           = appendToPath(grabPath(core->getDoc()->getFileName().c_str()), s);
				targetFileName = path.c_str();
			}
			if ((dest = ((LinkGoToR*)action)->getDest()))
			{
				if (dest->isPageRef())
					return false;
				targetPage = dest->getPageNum();
				targetDest = "";
			}
			else if ((namedDest = ((LinkGoToR*)action)->getNamedDest()).size())
			{
				targetDest = namedDest.c_str();
				targetPage = 1;
			}
			return true;
		case actionLaunch:
			fileName = ((LinkLaunch*)action)->getFileName();
			s        = fileName.c_str();
			if (!(fileName.size() >= 4 && (!strcmp(s + fileName.size() - 4, ".pdf") || !strcmp(s + fileName.size() - 4, ".PDF"))))
				return false;
			if (isAbsolutePath(s))
			{
				targetFileName = s;
			}
			else
			{
				if (core->getDoc()->getFileName().empty())
					return false;
				path           = appendToPath(grabPath(core->getDoc()->getFileName().c_str()), s);
				targetFileName = path.c_str();
			}
			targetPage = 1;
			targetDest = "";
			return true;
		default:
			return false;
		}
	}
	catch (GMemException e)
	{
		return false;
	}
}

XpdfAnnotHandle XpdfWidget::onAnnot(int page, double xx, double yy)
{
	try
	{
		if (!core->getDoc())
			return nullptr;
		return (XpdfAnnotHandle)core->findAnnot(page, xx, yy);
	}
	catch (GMemException e)
	{
		return nullptr;
	}
}

QString XpdfWidget::getAnnotType(XpdfAnnotHandle annot)
{
	try
	{
		return ((Annot*)annot)->getType().c_str();
	}
	catch (GMemException e)
	{
		return QString();
	}
}

QString XpdfWidget::getAnnotContent(XpdfAnnotHandle annot)
{
	QString     s;
	Object      annotObj, contentsObj;
	TextString* ts;
	Unicode*    u;

	try
	{
		if (((Annot*)annot)->getObject(&annotObj)->isDict())
		{
			if (annotObj.dictLookup("Contents", &contentsObj)->isString())
			{
				ts = new TextString(contentsObj.getString());
				u  = ts->getUnicode();
				for (int i = 0; i < ts->getLength(); ++i)
					s.append((QChar)u[i]);
			}
			contentsObj.free();
		}
		annotObj.free();
		return s;
	}
	catch (GMemException e)
	{
		return QString();
	}
}

XpdfFormFieldHandle XpdfWidget::onFormField(int page, double xx, double yy)
{
	try
	{
		if (!core->getDoc())
			return nullptr;
		return (XpdfFormFieldHandle)core->findFormField(page, xx, yy);
	}
	catch (GMemException e)
	{
		return nullptr;
	}
}

QString XpdfWidget::getFormFieldType(XpdfFormFieldHandle field)
{
	try
	{
		return ((AcroFormField*)field)->getType();
	}
	catch (GMemException e)
	{
		return QString();
	}
}

QString XpdfWidget::getFormFieldName(XpdfFormFieldHandle field)
{
	Unicode* u;
	QString  s;
	int      length;

	try
	{
		u = ((AcroFormField*)field)->getName(&length);
		for (int i = 0; i < length; ++i)
			s.append((QChar)u[i]);
		gfree(u);
		return s;
	}
	catch (GMemException e)
	{
		return QString();
	}
}

QString XpdfWidget::getFormFieldValue(XpdfFormFieldHandle field)
{
	Unicode* u;
	QString  s;
	int      length;

	try
	{
		u = ((AcroFormField*)field)->getValue(&length);
		for (int i = 0; i < length; ++i)
			s.append((QChar)u[i]);
		gfree(u);
		return s;
	}
	catch (GMemException e)
	{
		return QString();
	}
}

void XpdfWidget::getFormFieldBBox(XpdfFormFieldHandle field, int* pageNum, double* xMin, double* yMin, double* xMax, double* yMax)
{
	try
	{
		*pageNum = ((AcroFormField*)field)->getPageNum();
		((AcroFormField*)field)->getBBox(xMin, yMin, xMax, yMax);
	}
	catch (GMemException e)
	{
	}
}

bool XpdfWidget::convertWindowToPDFCoords(int winX, int winY, int* page, double* pdfX, double* pdfY)
{
	try
	{
		return core->cvtWindowToUser(winX, winY, page, pdfX, pdfY);
	}
	catch (GMemException e)
	{
		return false;
	}
}

void XpdfWidget::convertPDFToWindowCoords(int page, double pdfX, double pdfY, int* winX, int* winY)
{
	try
	{
		core->cvtUserToWindow(page, pdfX, pdfY, winX, winY);
	}
	catch (GMemException e)
	{
	}
}

void XpdfWidget::enableRedraw(bool enable)
{
	setUpdatesEnabled(enable);
}

void XpdfWidget::getPageBox(int page, const QString& box, double* xMin, double* yMin, double* xMax, double* yMax) const
{
	PDFRectangle* r;

	*xMin = *yMin = *xMax = *yMax = 0;
	try
	{
		if (!core->getDoc())
			return;
		if (page < 1 || page > core->getDoc()->getNumPages())
			return;
		if (!box.compare("media", Qt::CaseInsensitive))
			r = core->getDoc()->getCatalog()->getPage(page)->getMediaBox();
		else if (!box.compare("crop", Qt::CaseInsensitive))
			r = core->getDoc()->getCatalog()->getPage(page)->getCropBox();
		else if (!box.compare("bleed", Qt::CaseInsensitive))
			r = core->getDoc()->getCatalog()->getPage(page)->getBleedBox();
		else if (!box.compare("trim", Qt::CaseInsensitive))
			r = core->getDoc()->getCatalog()->getPage(page)->getTrimBox();
		else if (!box.compare("art", Qt::CaseInsensitive))
			r = core->getDoc()->getCatalog()->getPage(page)->getArtBox();
		else
			return;
		*xMin = r->x1;
		*yMin = r->y1;
		*xMax = r->x2;
		*yMax = r->y2;
	}
	catch (GMemException e)
	{
	}
}

double XpdfWidget::getPageWidth(int page) const
{
	try
	{
		if (!core->getDoc())
			return 0;
		if (page < 1 || page > core->getDoc()->getNumPages())
			return 0;
		return core->getDoc()->getPageCropWidth(page);
	}
	catch (GMemException e)
	{
		return 0;
	}
}

double XpdfWidget::getPageHeight(int page) const
{
	try
	{
		if (!core->getDoc())
			return 0;
		if (page < 1 || page > core->getDoc()->getNumPages())
			return 0;
		return core->getDoc()->getPageCropHeight(page);
	}
	catch (GMemException e)
	{
		return 0;
	}
}

int XpdfWidget::getPageRotation(int page) const
{
	try
	{
		if (!core->getDoc())
			return 0;
		if (page < 1 || page > core->getDoc()->getNumPages())
			return 0;
		return core->getDoc()->getCatalog()->getPage(page)->getRotate();
	}
	catch (GMemException e)
	{
		return 0;
	}
}

bool XpdfWidget::hasSelection()
{
	try
	{
		return (bool)core->hasSelection();
	}
	catch (GMemException e)
	{
		return false;
	}
}

bool XpdfWidget::getCurrentSelection(int* page, double* x0, double* y0, double* x1, double* y1) const
{
	try
	{
		return (bool)core->getSelection(page, x0, y0, x1, y1);
	}
	catch (GMemException e)
	{
		return false;
	}
}

void XpdfWidget::setCurrentSelection(int page, double x0, double y0, double x1, double y1)
{
	int ulx, uly, lrx, lry, t;

	try
	{
		core->cvtUserToDev(page, x0, y0, &ulx, &uly);
		core->cvtUserToDev(page, x1, y1, &lrx, &lry);
		if (ulx > lrx)
		{
			t   = ulx;
			ulx = lrx;
			lrx = t;
		}
		if (uly > lry)
		{
			t   = uly;
			uly = lry;
			lry = t;
		}
		core->setSelection(page, ulx, uly, lrx, lry);
	}
	catch (GMemException e)
	{
	}
}

void XpdfWidget::clearSelection()
{
	try
	{
		core->clearSelection();
	}
	catch (GMemException e)
	{
	}
}

bool XpdfWidget::isBlockSelectMode()
{
	try
	{
		return core->getSelectMode() == selectModeBlock;
	}
	catch (GMemException e)
	{
		return false;
	}
}

bool XpdfWidget::isLinearSelectMode()
{
	try
	{
		return core->getSelectMode() == selectModeLinear;
	}
	catch (GMemException e)
	{
		return false;
	}
}

void XpdfWidget::setBlockSelectMode()
{
	try
	{
		core->setSelectMode(selectModeBlock);
	}
	catch (GMemException e)
	{
	}
}

void XpdfWidget::setLinearSelectMode()
{
	try
	{
		core->setSelectMode(selectModeLinear);
	}
	catch (GMemException e)
	{
	}
}

void XpdfWidget::setSelectionColor(const QColor& selectionColor)
{
	SplashColor col;

	try
	{
		col[0] = (uint8_t)selectionColor.red();
		col[1] = (uint8_t)selectionColor.green();
		col[2] = (uint8_t)selectionColor.blue();
		core->setSelectionColor(col);
	}
	catch (GMemException e)
	{
	}
}

void XpdfWidget::forceRedraw()
{
	try
	{
		core->forceRedraw();
	}
	catch (GMemException e)
	{
	}
}

#if XPDFWIDGET_PRINTING

bool XpdfWidget::okToPrint() const
{
	try
	{
		if (!core->getDoc())
			return false;
		return (bool)core->getDoc()->okToPrint();
	}
	catch (GMemException e)
	{
		return false;
	}
}

XpdfWidget::ErrorCode XpdfWidget::print(bool showDialog)
{
	GString*  defaultPrinter;
	ErrorCode err;

	try
	{
		if (!core->getDoc())
			return pdfErrNoHandle;
		if (!printerForDialog)
		{
			printerForDialog = new QPrinter(QPrinter::HighResolution);
			if ((defaultPrinter = globalParams->getDefaultPrinter()))
			{
				printerForDialog->setPrinterName(
				    QString::fromUtf8(defaultPrinter.c_str()));
				delete defaultPrinter;
			}
		}
		printerForDialog->setFromTo(1, core->getDoc()->getNumPages());
		if (showDialog)
		{
			if (!printDialog)
				printDialog = new QPrintDialog(printerForDialog, this);
			if (printDialog->exec() != QDialog::Accepted)
				return pdfErrPrinting;
		}
		printCanceled = false;
		err           = printPDF(core->getDoc(), printerForDialog, printHDPI, printVDPI, this);
		return err;
	}
	catch (GMemException e)
	{
		return pdfErrOutOfMemory;
	}
}

XpdfWidget::ErrorCode XpdfWidget::print(QPrinter* prt)
{
	try
	{
		if (!core->getDoc())
			return pdfErrNoHandle;
		printCanceled = false;
		return printPDF(core->getDoc(), prt, printHDPI, printVDPI, this);
	}
	catch (GMemException e)
	{
		return pdfErrOutOfMemory;
	}
}

void XpdfWidget::updatePrintStatus(int nextPage, int firstPage, int lastPage)
{
	emit printStatus(nextPage, firstPage, lastPage);
}

void XpdfWidget::setPrintDPI(int hDPI, int vDPI)
{
	printHDPI = hDPI;
	printVDPI = vDPI;
}

#endif // XPDFWIDGET_PRINTING

QImage XpdfWidget::convertPageToImage(int page, double dpi, bool transparent, ImageColorMode color)
{
	try
	{
		PDFDoc* doc = core->getDoc();
		if (!doc)
			return QImage();
		if (page < 1 || page > doc->getNumPages())
			return QImage();
		SplashColorMode mode;
		SplashColor     paperColor;
		QImage::Format  format;
		if (color == pdfImageColorMono)
		{
			mode          = splashModeMono1;
			paperColor[0] = 0xff;
			format        = QImage::Format_Mono;
		}
		else if (color == pdfImageColorGray)
		{
			mode          = splashModeMono8;
			paperColor[0] = 0xff;
			format        = QImage::Format_Grayscale8;
		}
		else
		{
			mode          = splashModeRGB8;
			paperColor[0] = paperColor[1] = paperColor[2] = 0xff;
			if (transparent)
				format = QImage::Format_ARGB32;
			else
				format = QImage::Format_RGB888;
		}
		if (format == QImage::Format_ARGB32)
		{
			SplashOutputDev* out = new SplashOutputDev(mode, 1, false, paperColor);
			out->setNoComposite(true);
			out->startDoc(doc->getXRef());
			doc->displayPage(out, page, dpi, dpi, core->getRotate(), false, true, false);
			SplashBitmap* bitmap = out->getBitmap();
			QImage        img(bitmap->getWidth(), bitmap->getHeight(), format);
			uint8_t*      pix   = bitmap->getDataPtr();
			uint8_t*      alpha = bitmap->getAlphaPtr();
			uint32_t*     argb  = (uint32_t*)img.bits();
			for (int y = 0; y < bitmap->getHeight(); ++y)
			{
				for (int x = 0; x < bitmap->getWidth(); ++x)
				{
					*argb = (*alpha << 24) | (pix[0] << 16) | (pix[1] << 8) | pix[2];
					pix += 3;
					++alpha;
					++argb;
				}
			}
			delete out;
			return img;
		}
		else
		{
			SplashOutputDev* out = new SplashOutputDev(mode, 4, false, paperColor);
			out->startDoc(doc->getXRef());
			doc->displayPage(out, page, dpi, dpi, core->getRotate(), false, true, false);
			SplashBitmap* bitmap = out->getBitmap();
			QImage*       img    = new QImage((const uchar*)bitmap->getDataPtr(), bitmap->getWidth(), bitmap->getHeight(), format);
			// force a copy
			QImage        img2(img->copy());
			delete img;
			delete out;
			return img2;
		}
	}
	catch (GMemException e)
	{
		return QImage();
	}
}

QImage XpdfWidget::convertRegionToImage(int page, double x0, double y0, double x1, double y1, double dpi, bool transparent, ImageColorMode color)
{
	try
	{
		PDFDoc* doc = core->getDoc();
		if (!doc)
			return QImage();
		if (page < 1 || page > doc->getNumPages())
			return QImage();

		if (x0 > x1)
		{
			double t = x0;
			x0       = x1;
			x1       = t;
		}
		if (y0 > y1)
		{
			double t = y0;
			y0       = y1;
			y1       = t;
		}
		PDFRectangle* box = doc->getCatalog()->getPage(page)->getCropBox();
		int           rot = doc->getPageRotate(page);
		double        k   = dpi / 72.0;
		int           sliceX, sliceY, sliceW, sliceH;
		if (rot == 90)
		{
			sliceX = (int)(k * (y0 - box->y1));
			sliceY = (int)(k * (x0 - box->x1));
			sliceW = (int)(k * (y1 - y0));
			sliceH = (int)(k * (x1 - x0));
		}
		else if (rot == 180)
		{
			sliceX = (int)(k * (box->x2 - x1));
			sliceY = (int)(k * (y0 - box->y1));
			sliceW = (int)(k * (x1 - x0));
			sliceH = (int)(k * (y1 - y0));
		}
		else if (rot == 270)
		{
			sliceX = (int)(k * (box->y2 - y1));
			sliceY = (int)(k * (box->x2 - x1));
			sliceW = (int)(k * (y1 - y0));
			sliceH = (int)(k * (x1 - x0));
		}
		else
		{
			sliceX = (int)(k * (x0 - box->x1));
			sliceY = (int)(k * (box->y2 - y1));
			sliceW = (int)(k * (x1 - x0));
			sliceH = (int)(k * (y1 - y0));
		}

		SplashColorMode mode;
		SplashColor     paperColor;
		QImage::Format  format;
		if (color == pdfImageColorMono)
		{
			mode          = splashModeMono1;
			paperColor[0] = 0xff;
			format        = QImage::Format_Mono;
		}
		else if (color == pdfImageColorGray)
		{
			mode          = splashModeMono8;
			paperColor[0] = 0xff;
			format        = QImage::Format_Grayscale8;
		}
		else
		{
			mode          = splashModeRGB8;
			paperColor[0] = paperColor[1] = paperColor[2] = 0xff;
			if (transparent)
				format = QImage::Format_ARGB32;
			else
				format = QImage::Format_RGB888;
		}
		if (format == QImage::Format_ARGB32)
		{
			SplashOutputDev* out = new SplashOutputDev(mode, 1, false, paperColor);
			out->setNoComposite(true);
			out->startDoc(doc->getXRef());
			doc->displayPageSlice(out, page, dpi, dpi, core->getRotate(), false, true, false, sliceX, sliceY, sliceW, sliceH);
			SplashBitmap* bitmap = out->getBitmap();
			QImage        img(bitmap->getWidth(), bitmap->getHeight(), format);
			uint8_t*      pix   = bitmap->getDataPtr();
			uint8_t*      alpha = bitmap->getAlphaPtr();
			uint32_t*     argb  = (uint32_t*)img.bits();
			for (int y = 0; y < bitmap->getHeight(); ++y)
			{
				for (int x = 0; x < bitmap->getWidth(); ++x)
				{
					*argb = (*alpha << 24) | (pix[0] << 16) | (pix[1] << 8) | pix[2];
					pix += 3;
					++alpha;
					++argb;
				}
			}
			delete out;
			return img;
		}
		else
		{
			SplashOutputDev* out = new SplashOutputDev(mode, 4, false, paperColor);
			out->startDoc(doc->getXRef());
			doc->displayPageSlice(out, page, dpi, dpi, core->getRotate(), false, true, false, sliceX, sliceY, sliceW, sliceH);
			SplashBitmap* bitmap = out->getBitmap();
			QImage*       img    = new QImage((const uchar*)bitmap->getDataPtr(), bitmap->getWidth(), bitmap->getHeight(), format);
			// force a copy
			QImage        img2(img->copy());
			delete img;
			delete out;
			return img2;
		}
	}
	catch (GMemException e)
	{
		return QImage();
	}
}

QImage XpdfWidget::getThumbnail(int page)
{
	Object            thumbObj, decodeObj, colorSpaceObj, obj;
	Dict*             thumbDict;
	GfxColorSpace*    colorSpace;
	GfxImageColorMap* colorMap;
	ImageStream*      imgStream;
	uint8_t *         line, *rgb;
	int               w, h, bpc, yy;

	try
	{
		if (!core->getDoc())
			return QImage();
		if (page < 1 || page > core->getDoc()->getNumPages())
			return QImage();

		// get the thumbnail image object
		if (!core->getDoc()->getCatalog()->getPage(page)->getThumbnail(&thumbObj)->isStream())
		{
			thumbObj.free();
			return QImage();
		}

		// get the image parameters
		thumbDict = thumbObj.streamGetDict();
		if (!thumbDict->lookup("Width", &obj)->isInt())
		{
			obj.free();
			thumbObj.free();
			return QImage();
		}
		w = obj.getInt();
		obj.free();
		if (!thumbDict->lookup("Height", &obj)->isInt())
		{
			obj.free();
			thumbObj.free();
			return QImage();
		}
		h = obj.getInt();
		obj.free();
		if (!thumbDict->lookup("BitsPerComponent", &obj)->isInt())
		{
			obj.free();
			thumbObj.free();
			return QImage();
		}
		bpc = obj.getInt();
		obj.free();

		// create the color space and color map
		thumbDict->lookup("Decode", &decodeObj);
		thumbDict->lookup("ColorSpace", &colorSpaceObj);
		colorSpace = GfxColorSpace::parse(&colorSpaceObj);
		colorMap   = new GfxImageColorMap(bpc, &decodeObj, colorSpace);
		colorSpaceObj.free();
		decodeObj.free();
		imgStream = new ImageStream(thumbObj.getStream(), w, colorSpace->getNComps(), bpc);

		// create the QImage, and read the image data
		QImage img(w, h, QImage::Format_RGB888);
		rgb = (uint8_t*)gmallocn(w, 3);
		imgStream->reset();
		for (yy = 0; yy < h; ++yy)
		{
			line = imgStream->getLine();
			colorMap->getRGBByteLine(line, rgb, w, gfxRenderingIntentPerceptual);
			memcpy(img.scanLine(yy), rgb, 3 * w);
		}
		gfree(rgb);

		delete colorMap;
		delete imgStream;
		thumbObj.free();

		return img;
	}
	catch (GMemException e)
	{
		return QImage();
	}
}

bool XpdfWidget::okToExtractText() const
{
	try
	{
		if (!core->getDoc())
			return false;
		return (bool)core->getDoc()->okToCopy();
	}
	catch (GMemException e)
	{
		return false;
	}
}

void XpdfWidget::setTextEncoding(const QString& encodingName)
{
	init();
	try
	{
		globalParams->setTextEncoding((char*)encodingName.toLatin1().constData());
	}
	catch (GMemException e)
	{
	}
}

QString XpdfWidget::extractText(int page, double x0, double y0, double x1, double y1)
{
	QString     ret;
	std::string s;

	try
	{
		if (!core->getDoc()) return QString();
		if ((s = core->extractText(page, x0, y0, x1, y1)).empty()) return QString();

		const auto enc     = globalParams->getTextEncodingName();
		const bool twoByte = (enc == "UCS-2");
		if (twoByte)
			for (int i = 0; i + 1 < s.size(); i += 2)
				ret.append((QChar)(((s.at(i) & 0xff) << 8) + (s.at(i + 1) & 0xff)));
		else
			ret.append(s.c_str());
		return ret;
	}
	catch (GMemException e)
	{
		return QString();
	}
}

QString XpdfWidget::getSelectedText()
{
	try
	{
		return core->getSelectedTextQString();
	}
	catch (GMemException e)
	{
		return "";
	}
}

void XpdfWidget::copySelection()
{
	try
	{
		core->copySelection(true);
	}
	catch (GMemException e)
	{
	}
}

bool XpdfWidget::find(const QString& text, int flags)
{
	Unicode* u;
	bool     ret;

	try
	{
		if (!core->getDoc()) return false;
		const int len = TO_INT(text.length());
		u             = (Unicode*)gmallocn(len, sizeof(Unicode));
		for (int i = 0; i < len; ++i)
			u[i] = (Unicode)text[i].unicode();
		ret = (bool)core->findU(u, len, (flags & findCaseSensitive) ? true : false, (flags & findNext) ? true : false, (flags & findBackward) ? true : false, (flags & findWholeWord) ? true : false, (flags & findOnePageOnly) ? true : false);
		gfree(u);
		return ret;
	}
	catch (GMemException e)
	{
		return false;
	}
}

QVector<XpdfFindResult> XpdfWidget::findAll(const QString& text, int firstPage, int lastPage, int flags)
{
	QVector<XpdfFindResult> v;
	try
	{
		if (!core->getDoc())
			return v;
		int      len = (int)text.length();
		Unicode* u   = (Unicode*)gmallocn(len, sizeof(Unicode));
		for (int i = 0; i < len; ++i)
			u[i] = (Unicode)text[i].unicode();
		GList* results = core->findAll(u, len, (flags & findCaseSensitive) ? true : false, (flags & findWholeWord) ? true : false, firstPage, lastPage);
		gfree(u);
		for (int i = 0; i < results->getLength(); ++i)
		{
			FindResult* result = (FindResult*)results->get(i);
			v.append(XpdfFindResult(result->page, result->xMin, result->yMin, result->xMax, result->yMax));
		}
		deleteGList(results, FindResult);
		return v;
	}
	catch (GMemException e)
	{
		return v;
	}
}

bool XpdfWidget::hasPageLabels()
{
	try
	{
		if (!core->getDoc())
			return false;
		return core->getDoc()->getCatalog()->hasPageLabels();
	}
	catch (GMemException e)
	{
		return false;
	}
}

QString XpdfWidget::getPageLabelFromPageNum(int pageNum)
{
	try
	{
		if (!core->getDoc())
			return QString();
		TextString* ts = core->getDoc()->getCatalog()->getPageLabel(pageNum);
		if (!ts)
			return QString();
		QString  qs;
		Unicode* u = ts->getUnicode();
		for (int i = 0; i < ts->getLength(); ++i)
			qs.append((QChar)u[i]);
		delete ts;
		return qs;
	}
	catch (GMemException e)
	{
		return QString();
	}
}

int XpdfWidget::getPageNumFromPageLabel(QString pageLabel)
{
	try
	{
		if (!core->getDoc())
			return -1;
		TextString* ts = new TextString();
		for (int i = 0; i < pageLabel.size(); ++i)
			ts->append((Unicode)pageLabel.at(i).unicode());
		int pg = core->getDoc()->getCatalog()->getPageNumFromPageLabel(ts);
		delete ts;
		return pg;
	}
	catch (GMemException e)
	{
		return -1;
	}
}

int XpdfWidget::getOutlineNumChildren(XpdfOutlineHandle outline)
{
	GList* items;

	try
	{
		if (!core->getDoc())
			return 0;
		if (outline)
		{
			((OutlineItem*)outline)->open();
			items = ((OutlineItem*)outline)->getKids();
		}
		else
		{
			items = core->getDoc()->getOutline()->getItems();
		}
		if (!items)
			return 0;
		return items->getLength();
	}
	catch (GMemException e)
	{
		return 0;
	}
}

XpdfOutlineHandle XpdfWidget::getOutlineChild(XpdfOutlineHandle outline, int idx)
{
	GList* items;

	try
	{
		if (!core->getDoc())
			return nullptr;
		if (outline)
		{
			((OutlineItem*)outline)->open();
			items = ((OutlineItem*)outline)->getKids();
		}
		else
		{
			items = core->getDoc()->getOutline()->getItems();
		}
		if (!items)
			return nullptr;
		if (idx < 0 || idx >= items->getLength())
			return nullptr;
		return (XpdfOutlineHandle)items->get(idx);
	}
	catch (GMemException e)
	{
		return nullptr;
	}
}

XpdfOutlineHandle XpdfWidget::getOutlineParent(XpdfOutlineHandle outline)
{
	if (!outline)
		return nullptr;
	return (XpdfOutlineHandle)((OutlineItem*)outline)->getParent();
}

QString XpdfWidget::getOutlineTitle(XpdfOutlineHandle outline)
{
	Unicode* title;
	QString  s;

	if (!outline)
		return QString();
	try
	{
		title              = ((OutlineItem*)outline)->getTitle();
		const int titleLen = ((OutlineItem*)outline)->getTitleLength();
		for (int i = 0; i < titleLen; ++i)
			s.append((QChar)title[i]);
		return s;
	}
	catch (GMemException e)
	{
		return QString();
	}
}

bool XpdfWidget::getOutlineStartsOpen(XpdfOutlineHandle outline)
{
	if (!outline)
		return false;
	try
	{
		return (bool)((OutlineItem*)outline)->isOpen();
	}
	catch (GMemException e)
	{
		return false;
	}
}

int XpdfWidget::getOutlineTargetPage(XpdfOutlineHandle outline)
{
	try
	{
		if (!outline || !core->getDoc())
			return 0;
		return core->getDoc()->getOutlineTargetPage((OutlineItem*)outline);
	}
	catch (GMemException e)
	{
		return 0;
	}
}

void XpdfWidget::gotoOutlineTarget(XpdfOutlineHandle outline)
{
	if (!outline)
		return;
	try
	{
		if (((OutlineItem*)outline)->getAction())
			core->doAction(((OutlineItem*)outline)->getAction());
	}
	catch (GMemException e)
	{
	}
}

int XpdfWidget::getNumLayers() const
{
	try
	{
		if (!core->getDoc())
			return 0;
		return core->getDoc()->getOptionalContent()->getNumOCGs();
	}
	catch (GMemException e)
	{
		return 0;
	}
}

XpdfLayerHandle XpdfWidget::getLayer(int idx) const
{
	try
	{
		if (!core->getDoc() || idx < 0 || idx >= core->getDoc()->getOptionalContent()->getNumOCGs())
			return nullptr;
		return (XpdfLayerHandle)core->getDoc()->getOptionalContent()->getOCG(idx);
	}
	catch (GMemException e)
	{
		return nullptr;
	}
}

QString XpdfWidget::getLayerName(XpdfLayerHandle layer) const
{
	Unicode* name;
	QString  s;

	if (!layer)
		return QString();
	try
	{
		name              = ((OptionalContentGroup*)layer)->getName();
		const int nameLen = ((OptionalContentGroup*)layer)->getNameLength();
		for (int i = 0; i < nameLen; ++i)
			s.append((QChar)name[i]);
		return s;
	}
	catch (GMemException e)
	{
		return QString();
	}
}

bool XpdfWidget::getLayerVisibility(XpdfLayerHandle layer) const
{
	if (!layer)
		return false;
	try
	{
		return (bool)((OptionalContentGroup*)layer)->getState();
	}
	catch (GMemException e)
	{
		return false;
	}
}

void XpdfWidget::setLayerVisibility(XpdfLayerHandle layer, bool visibility)
{
	try
	{
		if (!core->getDoc() || !layer)
			return;
		core->setOCGState(((OptionalContentGroup*)layer), (bool)visibility);
	}
	catch (GMemException e)
	{
		return;
	}
}

int XpdfWidget::getLayerViewState(XpdfLayerHandle layer) const
{
	int s;

	if (!layer)
		return 0;
	try
	{
		s = ((OptionalContentGroup*)layer)->getViewState();
		return (s == ocUsageOn) ? 1 : (s == ocUsageOff) ? 0
		                                                : -1;
	}
	catch (GMemException e)
	{
		return 0;
	}
}

int XpdfWidget::getLayerPrintState(XpdfLayerHandle layer) const
{
	int s;

	if (!layer)
		return 0;
	try
	{
		s = ((OptionalContentGroup*)layer)->getPrintState();
		return (s == ocUsageOn) ? 1 : (s == ocUsageOff) ? 0
		                                                : -1;
	}
	catch (GMemException e)
	{
		return 0;
	}
}

XpdfLayerOrderHandle XpdfWidget::getLayerOrderRoot() const
{
	try
	{
		if (!core->getDoc())
			return nullptr;
		return (XpdfLayerOrderHandle)
		    core->getDoc()
		        ->getOptionalContent()
		        ->getDisplayRoot();
	}
	catch (GMemException e)
	{
		return nullptr;
	}
}

bool XpdfWidget::getLayerOrderIsName(XpdfLayerOrderHandle order) const
{
	if (!order)
		return false;
	try
	{
		return ((OCDisplayNode*)order)->getOCG() == nullptr;
	}
	catch (GMemException e)
	{
		return false;
	}
}

QString XpdfWidget::getLayerOrderName(XpdfLayerOrderHandle order) const
{
	Unicode* name;
	QString  s;

	if (!order)
		return QString();
	try
	{
		name              = ((OCDisplayNode*)order)->getName();
		const int nameLen = ((OCDisplayNode*)order)->getNameLength();
		for (int i = 0; i < nameLen; ++i)
			s.append((QChar)name[i]);
		return s;
	}
	catch (GMemException e)
	{
		return QString();
	}
}

XpdfLayerHandle XpdfWidget::getLayerOrderLayer(XpdfLayerOrderHandle order)
{
	if (!order)
		return nullptr;
	try
	{
		return (XpdfLayerHandle)((OCDisplayNode*)order)->getOCG();
	}
	catch (GMemException e)
	{
		return nullptr;
	}
}

int XpdfWidget::getLayerOrderNumChildren(XpdfLayerOrderHandle order)
{
	if (!order)
		return 0;
	try
	{
		return ((OCDisplayNode*)order)->getNumChildren();
	}
	catch (GMemException e)
	{
		return 0;
	}
}

XpdfLayerOrderHandle XpdfWidget::getLayerOrderChild(XpdfLayerOrderHandle order, int idx)
{
	if (!order)
		return nullptr;
	try
	{
		if (idx < 0 || idx >= ((OCDisplayNode*)order)->getNumChildren())
			return nullptr;
		return (XpdfLayerOrderHandle)((OCDisplayNode*)order)->getChild(idx);
	}
	catch (GMemException e)
	{
		return nullptr;
	}
}

XpdfLayerOrderHandle XpdfWidget::getLayerOrderParent(
    XpdfLayerOrderHandle order)
{
	if (!order)
		return nullptr;
	try
	{
		return (XpdfLayerOrderHandle)((OCDisplayNode*)order)->getParent();
	}
	catch (GMemException e)
	{
		return nullptr;
	}
}

int XpdfWidget::getNumEmbeddedFiles()
{
	try
	{
		if (!core->getDoc())
			return 0;
		return core->getDoc()->getNumEmbeddedFiles();
	}
	catch (GMemException e)
	{
		return 0;
	}
}

QString XpdfWidget::getEmbeddedFileName(int idx)
{
	Unicode* name;
	QString  s;

	try
	{
		if (!core->getDoc() || idx < 0 || idx >= core->getDoc()->getNumEmbeddedFiles())
			return "";
		name              = core->getDoc()->getEmbeddedFileName(idx);
		const int nameLen = core->getDoc()->getEmbeddedFileNameLength(idx);
		for (int i = 0; i < nameLen; ++i)
			s.append((QChar)name[i]);
		return s;
	}
	catch (GMemException e)
	{
		return 0;
	}
}

bool XpdfWidget::saveEmbeddedFile(int idx, QString fileName)
{
	try
	{
		if (!core->getDoc() || idx < 0 || idx >= core->getDoc()->getNumEmbeddedFiles())
			return false;
		return core->getDoc()->saveEmbeddedFile(idx, fileName.toLocal8Bit().constData());
	}
	catch (GMemException e)
	{
		return false;
	}
}

QSize XpdfWidget::sizeHint() const
{
	try
	{
		return core->getBestSize();
	}
	catch (GMemException e)
	{
	}
	return QSize(612, 792);
}

//------------------------------------------------------------------------
// callbacks from QtPDFCore
//------------------------------------------------------------------------

void XpdfWidget::updateCbk(void* data, const std::string& fileName, int pageNum, int numPages, const char* linkLabel)
{
	XpdfWidget* xpdf = (XpdfWidget*)data;

	if (fileName.size())
	{
		if (pageNum >= 0)
			emit xpdf->pageChange(pageNum);
	}
}

void XpdfWidget::midPageChangedCbk(void* data, int pageNum)
{
	XpdfWidget* xpdf = (XpdfWidget*)data;

	emit xpdf->midPageChange(pageNum);
}

void XpdfWidget::preLoadCbk(void* data)
{
	XpdfWidget* xpdf = (XpdfWidget*)data;

	emit xpdf->preLoad();
}

void XpdfWidget::postLoadCbk(void* data)
{
	XpdfWidget* xpdf = (XpdfWidget*)data;

	emit xpdf->postLoad();
}

void XpdfWidget::linkCbk(void* data, const char* type, const char* dest, int page)
{
	XpdfWidget* xpdf = (XpdfWidget*)data;

	emit xpdf->linkClick(type, dest, page);
}

void XpdfWidget::selectDoneCbk(void* data)
{
	XpdfWidget* xpdf = (XpdfWidget*)data;

	emit xpdf->selectDone();
}

void XpdfWidget::paintDoneCbk(void* data, bool finished)
{
	XpdfWidget* xpdf = (XpdfWidget*)data;

	emit xpdf->paintDone(finished);
}

// NB: this function is called on a worker thread.
void XpdfWidget::tileDoneCbk(void* data)
{
	XpdfWidget* xpdf = (XpdfWidget*)data;

	emit xpdf->tileDone();
}

//------------------------------------------------------------------------
// events from the QAbstractScrollArea
//------------------------------------------------------------------------

void XpdfWidget::paintEvent(QPaintEvent* eventA)
{
	core->paintEvent(eventA->rect().left(), eventA->rect().top(), eventA->rect().width(), eventA->rect().height());
}

void XpdfWidget::resizeEvent(QResizeEvent* eventA)
{
	core->resizeEvent();
	emit resized();
}

void XpdfWidget::scrollContentsBy(int dx, int dy)
{
	core->scrollEvent();
}

void XpdfWidget::keyPressEvent(QKeyEvent* e)
{
	int key;

	if (!keyPassthrough)
	{
		key = e->key();
		if (key == Qt::Key_Left)
		{
			core->scrollLeft();
			return;
		}
		else if (key == Qt::Key_Right)
		{
			core->scrollRight();
			return;
		}
		else if (key == Qt::Key_Up)
		{
			core->scrollUp();
			return;
		}
		else if (key == Qt::Key_Down)
		{
			core->scrollDown();
			return;
		}
		else if (key == Qt::Key_PageUp)
		{
			core->scrollPageUp();
			return;
		}
		else if (key == Qt::Key_PageDown || key == Qt::Key_Space)
		{
			core->scrollPageDown();
			return;
		}
	}
	emit keyPress(e);
}

void XpdfWidget::mousePressEvent(QMouseEvent* e)
{
	int x, y;

	lastMousePressX[0]     = lastMousePressX[1];
	lastMousePressY[0]     = lastMousePressY[1];
	lastMousePressTime[0]  = lastMousePressTime[1];
	lastMousePressX[1]     = lastMousePressX[2];
	lastMousePressY[1]     = lastMousePressY[2];
	lastMousePressTime[1]  = lastMousePressTime[2];
	lastMousePressX[2]     = e->pos().x();
	lastMousePressY[2]     = e->pos().y();
	lastMousePressTime[2]  = e->timestamp();
	lastMouseEventWasPress = true;
	if (!mousePassthrough)
	{
		x = (int)(e->pos().x() * scaleFactor);
		y = (int)(e->pos().y() * scaleFactor);
		if (e->button() == Qt::LeftButton)
			core->startSelection(x, y, e->modifiers() & Qt::ShiftModifier);
		else if (e->button() == Qt::MiddleButton)
			core->startPan(x, y);
	}
	emit mousePress(e);
}

void XpdfWidget::mouseReleaseEvent(QMouseEvent* e)
{
	// some versions of Qt drop mouse press events in double-clicks (?)
	if (!lastMouseEventWasPress) mousePressEvent(e);
	lastMouseEventWasPress = false;

	int x = 0;
	int y = 0;
	if (!mousePassthrough)
	{
		x = (int)(e->pos().x() * scaleFactor);
		y = (int)(e->pos().y() * scaleFactor);
		if (e->button() == Qt::LeftButton)
			core->endSelection(x, y);
		else if (e->button() == Qt::MiddleButton)
			core->endPan(x, y);
	}
	emit mouseRelease(e);

	// double and triple clicks have to be "quick" and "nearby";
	// single clicks just have to be "nearby"
	ulong maxTime     = (ulong)QApplication::doubleClickInterval();
	int   maxDistance = QApplication::startDragDistance();
	if (e->timestamp() - lastMousePressTime[0] < 2 * maxTime && abs(e->pos().x() - lastMousePressX[0]) + abs(e->pos().y() - lastMousePressY[0]) <= maxDistance)
	{
		if (!mousePassthrough && e->button() == Qt::LeftButton)
			core->selectLine(x, y);
		emit mouseTripleClick(e);
	}
	else if (e->timestamp() - lastMousePressTime[1] < maxTime && abs(e->pos().x() - lastMousePressX[1]) + abs(e->pos().y() - lastMousePressY[1]) <= maxDistance)
	{
		if (!mousePassthrough && e->button() == Qt::LeftButton)
			core->selectWord(x, y);
		emit mouseDoubleClick(e);
	}
	else if (abs(e->pos().x() - lastMousePressX[2]) + abs(e->pos().y() - lastMousePressY[2]) <= maxDistance)
	{
		emit mouseClick(e);
	}
}

void XpdfWidget::mouseMoveEvent(QMouseEvent* e)
{
	const int x = (int)(e->pos().x() * scaleFactor);
	const int y = (int)(e->pos().y() * scaleFactor);
	core->mouseMove(x, y);
	emit mouseMove(e);
}

void XpdfWidget::wheelEvent(QWheelEvent* e)
{
	if (!mousePassthrough)
		QAbstractScrollArea::wheelEvent(e);
	emit mouseWheel(e);
}

bool XpdfWidget::eventFilter(QObject* obj, QEvent* event)
{
	QGestureEvent* gestureEvent;
	QPanGesture*   panGesture;
	QPinchGesture* pinchGesture;
	double         z;

	if (obj == viewport() && event->type() == QEvent::Gesture)
	{
		gestureEvent = (QGestureEvent*)event;
		if (touchPanEnabled && (panGesture = (QPanGesture*)gestureEvent->gesture(Qt::PanGesture)))
		{
			core->scrollTo(core->getScrollX() - (int)panGesture->delta().x(), core->getScrollY() - (int)panGesture->delta().y());
			gestureEvent->accept();
			return true;
		}
		else if (touchZoomEnabled && (pinchGesture = (QPinchGesture*)gestureEvent->gesture(Qt::PinchGesture)))
		{
			if (pinchGesture->changeFlags() & QPinchGesture::ScaleFactorChanged)
			{
				if (pinchGesture->state() == Qt::GestureStarted)
				{
					pinchZoomStart = getZoomPercent(core->getMidPageNum());
				}
				else
				{
					z = pinchZoomStart * pinchGesture->totalScaleFactor();
					if (z < 10)
						z = 10;
					else if (z > 800)
						z = 800;
					core->zoomCentered(z);
				}
			}
			gestureEvent->accept();
			return true;
		}
	}
	return QAbstractScrollArea::eventFilter(obj, event);
}

void XpdfWidget::tick()
{
	core->tick();
}
