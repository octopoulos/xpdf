//========================================================================
//
// XpdfViewer.cc
//
// Copyright 2015 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include <math.h>
#include <QAbstractItemModel>
#include <QAction>
#include <QActionGroup>
#include <QButtonGroup>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocalSocket>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QProcess>
#include <QProgressDialog>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRadioButton>
#include <QScreen>
#include <QScrollBar>
#include <QSignalMapper>
#include <QSplitter>
#include <QStackedLayout>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextBrowser>
#include <QTimer>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>
#include "GString.h" // Round2
#include "GList.h"
#include "GlobalParams.h"
#include "QtPDFCore.h"
#include "PDFDoc.h"
#include "AcroForm.h"
#include "Annot.h"
#include "TextString.h"
#include "Zoox.h"
#include "XpdfApp.h"
#include "XpdfViewer.h"
#include "gmempp.h"

//------------------------------------------------------------------------

static const char* aboutHTML =
    "<h3><img align=\"middle\" src=\"qrc:/xpdf-icon\"> "
    "Xpdf"
    "</h3>"
    "Version " xpdfVersion
    "<br>"
    "<br>"
    "<a href=\"http://www.xpdfreader.com/\">www.xpdfreader.com</a><br>"
    "<br>"
    "Based on the <b>XpdfWidget/Qt</b> toolkit from Glyph & Cog.<br>"
    "For information on commercial licensing:<br>"
    "<a href=\"http://www.glyphandcog.com/XpdfWidgetQt.html\">www.glyphandcog.com/XpdfWidgetQt.html</a><br>"
    "<br>" xpdfCopyright
    ".<br>"
    "Xpdf is licensed under the GNU General Public License (GPL), version 2 or 3.<br>"
    "<hr><br>"
    "The PDF data structures, operators, and specification are documented in ISO 32000-2:2020.<br>"
    "<br>"
    "XpdfReader uses the following open source libraries:"
    "<ul>"
    "<li>FreeType is copyright 2006-2023 David Turner, Robert Wilhelm, and Werner Lemberg.  FreeType is used here under the terms of the FreeType Project License."
    "<li>The Qt Toolkit is Copyright 2015 The Qt Company Ltd.  Qt is used here under the terms of the LGPL v2.1."
    "</ul>";

const char* helpURL = "http://www.xpdfreader.com/help";

//------------------------------------------------------------------------

#define maxZoom 2000

// Maximum number of errors to show in the error window.  Beyond this
// limit, old errors are removed from the window.
#define errorWindowMaxErrors 100

//------------------------------------------------------------------------
// command table
//------------------------------------------------------------------------

#define cmdMaxArgs 8

// |- requires -|
// command                nArgs  doc     event   function
XpdfViewerCmd XpdfViewer::cmdTab[] = {
	{"about",	                   0, false, false, &XpdfViewer::cmdAbout                     },
	{ "blockSelectMode",            0, false, false, &XpdfViewer::cmdBlockSelectMode           },
	{ "checkOpenFile",              1, false, false, &XpdfViewer::cmdCheckOpenFile             },
	{ "checkOpenFileAtDest",        2, false, false, &XpdfViewer::cmdCheckOpenFileAtDest       },
	{ "checkOpenFileAtPage",        2, false, false, &XpdfViewer::cmdCheckOpenFileAtPage       },
	{ "closeSidebar",               0, false, false, &XpdfViewer::cmdCloseSidebar              },
	{ "closeSidebarMoveResizeWin",  0, false, false, &XpdfViewer::cmdCloseSidebarMoveResizeWin },
	{ "closeSidebarResizeWin",      0, false, false, &XpdfViewer::cmdCloseSidebarResizeWin     },
	{ "closeTabOrQuit",             0, false, false, &XpdfViewer::cmdCloseTabOrQuit            },
	{ "closeWindowOrQuit",          0, false, false, &XpdfViewer::cmdCloseWindowOrQuit         },
	{ "continuousMode",             0, false, false, &XpdfViewer::cmdContinuousMode            },
	{ "copy",	                   0, true,  false, &XpdfViewer::cmdCopy                      },
	{ "copyLinkTarget",             0, true,  false, &XpdfViewer::cmdCopyLinkTarget            },
#if 0  // for debugging
  { "debug1",                  0, true,  true,  &XpdfViewer::cmdDebug1 },
#endif
	{ "endPan",	                 0, true,  true,  &XpdfViewer::cmdEndPan                    },
	{ "endSelection",               0, true,  true,  &XpdfViewer::cmdEndSelection              },
	{ "expandSidebar",              1, false, false, &XpdfViewer::cmdExpandSidebar             },
	{ "find",	                   0, true,  false, &XpdfViewer::cmdFind                      },
	{ "findFirst",                  0, true,  false, &XpdfViewer::cmdFindFirst                 },
	{ "findNext",	               0, true,  false, &XpdfViewer::cmdFindNext                  },
	{ "findPrevious",               0, true,  false, &XpdfViewer::cmdFindPrevious              },
	{ "focusToDocWin",              0, false, false, &XpdfViewer::cmdFocusToDocWin             },
	{ "focusToPageNum",             0, false, false, &XpdfViewer::cmdFocusToPageNum            },
	{ "followLink",                 0, true,  true,  &XpdfViewer::cmdFollowLink                },
	{ "followLinkInNewTab",         0, true,  true,  &XpdfViewer::cmdFollowLinkInNewTab        },
	{ "followLinkInNewTabNoSel",    0, true,  true,  &XpdfViewer::cmdFollowLinkInNewTabNoSel   },
	{ "followLinkInNewWin",         0, true,  true,  &XpdfViewer::cmdFollowLinkInNewWin        },
	{ "followLinkInNewWinNoSel",    0, true,  true,  &XpdfViewer::cmdFollowLinkInNewWinNoSel   },
	{ "followLinkNoSel",            0, true,  true,  &XpdfViewer::cmdFollowLinkNoSel           },
	{ "fullScreenMode",             0, false, false, &XpdfViewer::cmdFullScreenMode            },
	{ "goBackward",                 0, false, false, &XpdfViewer::cmdGoBackward                },
	{ "goForward",                  0, false, false, &XpdfViewer::cmdGoForward                 },
	{ "gotoDest",	               1, true,  false, &XpdfViewer::cmdGotoDest                  },
	{ "gotoLastPage",               0, true,  false, &XpdfViewer::cmdGotoLastPage              },
 //~   { "gotoLastPageNoScroll",    0, true,  false, &XpdfViewer::cmdGotoLastPageNoScroll },
	{ "gotoPage",	               1, true,  false, &XpdfViewer::cmdGotoPage                  },
 //~   { "gotoPageNoScroll",        1, true,  false, &XpdfViewer::cmdGotoPageNoScroll },
	{ "help",	                   0, false, false, &XpdfViewer::cmdHelp                      },
	{ "hideMenuBar",                0, false, false, &XpdfViewer::cmdHideMenuBar               },
	{ "hideToolbar",                0, false, false, &XpdfViewer::cmdHideToolbar               },
	{ "horizontalContinuousMode",   0, false, false, &XpdfViewer::cmdHorizontalContinuousMode  },
	{ "linearSelectMode",           0, false, false, &XpdfViewer::cmdLinearSelectMode          },
	{ "loadSession",                0, false, false, &XpdfViewer::cmdLoadSession               },
	{ "loadTabState",               0, false, false, &XpdfViewer::cmdLoadTabState              },
	{ "newTab",	                 0, false, false, &XpdfViewer::cmdNewTab                    },
	{ "newWindow",                  0, false, false, &XpdfViewer::cmdNewWindow                 },
	{ "nextPage",	               0, true,  false, &XpdfViewer::cmdNextPage                  },
	{ "nextPageNoScroll",           0, true,  false, &XpdfViewer::cmdNextPageNoScroll          },
	{ "nextTab",	                0, false, false, &XpdfViewer::cmdNextTab                   },
	{ "open",	                   0, false, false, &XpdfViewer::cmdOpen                      },
	{ "openErrorWindow",            0, false, false, &XpdfViewer::cmdOpenErrorWindow           },
	{ "openFile",	               1, false, false, &XpdfViewer::cmdOpenFile                  },
	{ "openFile2",                  5, false, false, &XpdfViewer::cmdOpenFile2                 },
	{ "openFileAtDest",             2, false, false, &XpdfViewer::cmdOpenFileAtDest            },
	{ "openFileAtDestIn",           3, false, false, &XpdfViewer::cmdOpenFileAtDestIn          },
	{ "openFileAtPage",             2, false, false, &XpdfViewer::cmdOpenFileAtPage            },
	{ "openFileAtPageIn",           3, false, false, &XpdfViewer::cmdOpenFileAtPageIn          },
	{ "openFileIn",                 2, false, false, &XpdfViewer::cmdOpenFileIn                },
	{ "openIn",	                 1, false, false, &XpdfViewer::cmdOpenIn                    },
	{ "openSidebar",                0, false, false, &XpdfViewer::cmdOpenSidebar               },
	{ "openSidebarMoveResizeWin",   0, false, false, &XpdfViewer::cmdOpenSidebarMoveResizeWin  },
	{ "openSidebarResizeWin",       0, false, false, &XpdfViewer::cmdOpenSidebarResizeWin      },
	{ "pageDown",	               0, true,  false, &XpdfViewer::cmdPageDown                  },
	{ "pageUp",	                 0, true,  false, &XpdfViewer::cmdPageUp                    },
	{ "postPopupMenu",              0, false, true,  &XpdfViewer::cmdPostPopupMenu             },
	{ "prevPage",	               0, true,  false, &XpdfViewer::cmdPrevPage                  },
	{ "prevPageNoScroll",           0, true,  false, &XpdfViewer::cmdPrevPageNoScroll          },
	{ "prevTab",	                0, false, false, &XpdfViewer::cmdPrevTab                   },
#if XPDFWIDGET_PRINTING
	{ "print",	                  0, true,  false, &XpdfViewer::cmdPrint                     },
#endif
	{ "quit",	                   0, false, false, &XpdfViewer::cmdQuit                      },
	{ "raise",	                  0, false, false, &XpdfViewer::cmdRaise                     },
 //~   { "redraw",                  0, true,  false, &XpdfViewer::cmdRedraw },
	{ "reload",	                 0, true,  false, &XpdfViewer::cmdReload                    },
	{ "rotateCCW",                  0, true,  false, &XpdfViewer::cmdRotateCCW                 },
	{ "rotateCW",	               0, true,  false, &XpdfViewer::cmdRotateCW                  },
	{ "run",	                    1, false, false, &XpdfViewer::cmdRun                       },
	{ "saveAs",	                 0, true,  false, &XpdfViewer::cmdSaveAs                    },
	{ "saveImage",                  0, true,  false, &XpdfViewer::cmdSaveImage                 },
	{ "saveSession",                0, false, false, &XpdfViewer::cmdSaveSession               },
	{ "saveTabState",               0, false, false, &XpdfViewer::cmdSaveTabState              },
	{ "scrollDown",                 1, true,  false, &XpdfViewer::cmdScrollDown                },
	{ "scrollDownNextPage",         1, true,  false, &XpdfViewer::cmdScrollDownNextPage        },
	{ "scrollLeft",                 1, true,  false, &XpdfViewer::cmdScrollLeft                },
	{ "scrollOutlineDown",          1, true,  false, &XpdfViewer::cmdScrollOutlineDown         },
	{ "scrollOutlineUp",            1, true,  false, &XpdfViewer::cmdScrollOutlineUp           },
	{ "scrollRight",                1, true,  false, &XpdfViewer::cmdScrollRight               },
	{ "scrollToBottomEdge",         0, true,  false, &XpdfViewer::cmdScrollToBottomEdge        },
	{ "scrollToBottomRight",        0, true,  false, &XpdfViewer::cmdScrollToBottomRight       },
	{ "scrollToLeftEdge",           0, true,  false, &XpdfViewer::cmdScrollToLeftEdge          },
	{ "scrollToRightEdge",          0, true,  false, &XpdfViewer::cmdScrollToRightEdge         },
	{ "scrollToTopEdge",            0, true,  false, &XpdfViewer::cmdScrollToTopEdge           },
	{ "scrollToTopLeft",            0, true,  false, &XpdfViewer::cmdScrollToTopLeft           },
	{ "scrollUp",	               1, true,  false, &XpdfViewer::cmdScrollUp                  },
	{ "scrollUpPrevPage",           1, true,  false, &XpdfViewer::cmdScrollUpPrevPage          },
	{ "selectLine",                 0, true,  true,  &XpdfViewer::cmdSelectLine                },
	{ "selectWord",                 0, true,  true,  &XpdfViewer::cmdSelectWord                },
	{ "setSelection",               5, true,  false, &XpdfViewer::cmdSetSelection              },
	{ "showAttachmentsPane",        0, false, false, &XpdfViewer::cmdShowAttachmentsPane       },
	{ "showDocumentInfo",           0, true,  false, &XpdfViewer::cmdShowDocumentInfo          },
	{ "showKeyBindings",            0, false, false, &XpdfViewer::cmdShowKeyBindings           },
	{ "showLayersPane",             0, false, false, &XpdfViewer::cmdShowLayersPane            },
	{ "showMenuBar",                0, false, false, &XpdfViewer::cmdShowMenuBar               },
	{ "showOutlinePane",            0, false, false, &XpdfViewer::cmdShowOutlinePane           },
	{ "showToolbar",                0, false, false, &XpdfViewer::cmdShowToolbar               },
	{ "shrinkSidebar",              1, false, false, &XpdfViewer::cmdShrinkSidebar             },
	{ "sideBySideContinuousMode",   0, false, false, &XpdfViewer::cmdSideBySideContinuousMode  },
	{ "sideBySideSingleMode",       0, false, false, &XpdfViewer::cmdSideBySideSingleMode      },
	{ "singlePageMode",             0, false, false, &XpdfViewer::cmdSinglePageMode            },
	{ "startExtendedSelection",     0, true,  true,  &XpdfViewer::cmdStartExtendedSelection    },
	{ "startPan",	               0, true,  true,  &XpdfViewer::cmdStartPan                  },
	{ "startSelection",             0, true,  true,  &XpdfViewer::cmdStartSelection            },
	{ "toggleContinuousMode",       0, false, false, &XpdfViewer::cmdToggleContinuousMode      },
	{ "toggleFullScreenMode",       0, false, false, &XpdfViewer::cmdToggleFullScreenMode      },
	{ "toggleMenuBar",              0, false, false, &XpdfViewer::cmdToggleMenuBar             },
	{ "toggleSelectMode",           0, false, false, &XpdfViewer::cmdToggleSelectMode          },
	{ "toggleSidebar",              0, false, false, &XpdfViewer::cmdToggleSidebar             },
	{ "toggleSidebarMoveResizeWin", 0, false, false, &XpdfViewer::cmdToggleSidebarMoveResizeWin},
	{ "toggleSidebarResizeWin",     0, false, false, &XpdfViewer::cmdToggleSidebarResizeWin    },
	{ "toggleToolbar",              0, false, false, &XpdfViewer::cmdToggleToolbar             },
	{ "viewPageLabels",             0, false, false, &XpdfViewer::cmdViewPageLabels            },
	{ "viewPageNumbers",            0, false, false, &XpdfViewer::cmdViewPageNumbers           },
	{ "windowMode",                 0, false, false, &XpdfViewer::cmdWindowMode                },
	{ "zoomFitPage",                0, false, false, &XpdfViewer::cmdZoomFitPage               },
	{ "zoomFitWidth",               0, false, false, &XpdfViewer::cmdZoomFitWidth              },
	{ "zoomIn",	                 0, false, false, &XpdfViewer::cmdZoomIn                    },
	{ "zoomOut",	                0, false, false, &XpdfViewer::cmdZoomOut                   },
	{ "zoomPercent",                1, false, false, &XpdfViewer::cmdZoomPercent               },
	{ "zoomToSelection",            0, true,  false, &XpdfViewer::cmdZoomToSelection           }
};

#define nCmds (sizeof(cmdTab) / sizeof(XpdfViewerCmd))

//------------------------------------------------------------------------
// XpdfMenuButton
//------------------------------------------------------------------------

XpdfMenuButton::XpdfMenuButton(QMenu* menuA)
{
	menu = menuA;
	connect(this, SIGNAL(pressed()), this, SLOT(btnPressed()));
}

void XpdfMenuButton::btnPressed()
{
	QSize  menuSize = menu->sizeHint();
	QPoint pos      = mapToGlobal(QPoint(width(), height()));
	pos -= QPoint(menuSize.width(), 0);
	menu->exec(pos);
	setDown(false);
	setAttribute(Qt::WA_UnderMouse, false);
}

//------------------------------------------------------------------------
// XpdfErrorWindow
//------------------------------------------------------------------------

class XpdfErrorEvent : public QEvent
{
public:
	XpdfErrorEvent(int eventType, QString msgA)
	    : QEvent((Type)eventType)
	    , msg(msgA)
	{
	}

	QString getMessage() { return msg; }

private:
	QString msg;
};

XpdfErrorWindow::XpdfErrorWindow(XpdfViewer* viewerA, int errorEventTypeA)
{
	viewer         = viewerA;
	errorEventType = errorEventTypeA;

	QVBoxLayout* topLayout = new QVBoxLayout();

	QHBoxLayout* btnLayout = new QHBoxLayout();
	topLayout->addLayout(btnLayout);

	QPushButton* clearBtn = new QPushButton("Clear");
	connect(clearBtn, SIGNAL(clicked()), this, SLOT(clearBtnPressed()));
	btnLayout->addWidget(clearBtn);

	btnLayout->addStretch(1);

	list = new QListWidget();
	topLayout->addWidget(list);

	setLayout(topLayout);

#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
	lastSize = QSize(list->fontMetrics().horizontalAdvance("m") * 50, list->fontMetrics().lineSpacing() * 16);
#else
	lastSize = QSize(list->fontMetrics().width("m") * 50, list->fontMetrics().lineSpacing() * 16);
#endif

	setErrorCallback(&errorCbk, this);
}

XpdfErrorWindow::~XpdfErrorWindow()
{
	// If the user quits while a page is rendering, we want any
	// subsequent error messages to vanish -- they shouldn't try to
	// update the error window (which has been deleted), and they
	// shouldn't get dumped to stderr.
	setErrorCallback(dummyErrorCbk, nullptr);
}

QSize XpdfErrorWindow::sizeHint() const
{
	return lastSize;
}

void XpdfErrorWindow::clearBtnPressed()
{
	list->clear();
	viewer->statusIndicatorOk();
}

void XpdfErrorWindow::closeEvent(QCloseEvent* event)
{
	lastSize = size();
}

void XpdfErrorWindow::keyPressEvent(QKeyEvent* event)
{
	if (event->key() == Qt::Key_Escape)
		close();
}

void XpdfErrorWindow::errorCbk(void* data, ErrorCategory category, int pos, const char* msg)
{
	XpdfErrorWindow* errWin = (XpdfErrorWindow*)data;
	std::string      s;
	if (pos >= 0)
		s = fmt::format("{} ({}): {}", errorCategoryNames[category], pos, msg);
	else
		s = fmt::format("{}: {}", errorCategoryNames[category], msg);
	XpdfApp::postEvent(errWin, new XpdfErrorEvent(errWin->errorEventType, s.c_str()));
}

void XpdfErrorWindow::dummyErrorCbk(void* data, ErrorCategory category, int pos, const char* msg)
{
}

void XpdfErrorWindow::customEvent(QEvent* event)
{
	XpdfErrorEvent* errEvent;

	if (event->type() == errorEventType)
	{
		errEvent = (XpdfErrorEvent*)event;
		if (list->count() < errorWindowMaxErrors)
		{
			list->addItem(errEvent->getMessage());
			list->scrollToBottom();
			viewer->statusIndicatorError();
		}
		else if (list->count() == errorWindowMaxErrors)
		{
			list->addItem("... additional errors not logged ...");
			list->scrollToBottom();
			viewer->statusIndicatorError();
		}
	}
}

//------------------------------------------------------------------------
// ZoomValidator
//------------------------------------------------------------------------

class ZoomValidator : public QValidator
{
public:
	ZoomValidator(QObject* parent = nullptr)
	    : QValidator(parent)
	{
	}

	virtual State validate(QString& input, int& pos) const;
	virtual void  fixup(QString& input) const;
};

QValidator::State ZoomValidator::validate(QString& input, int& pos) const
{
	QChar c;

	const int n = (int)input.length();
	if (n == 0)
		return QValidator::Intermediate;
	for (int i = 0; i < n - 1; ++i)
	{
		c = input[i];
		if (c < '0' || c > '9')
			return QValidator::Invalid;
	}
	c = input[n - 1];
	if (c == '%')
	{
		if (n > 1)
			return QValidator::Acceptable;
		return QValidator::Intermediate;
	}
	if (c < '0' || c > '9')
		return QValidator::Invalid;
	return QValidator::Intermediate;
}

void ZoomValidator::fixup(QString& input) const
{
	if (!input.endsWith("%"))
		input.append('%');
}

//------------------------------------------------------------------------
// PropertyListAnimation
//------------------------------------------------------------------------

class PropertyListAnimation : public QPropertyAnimation
{
public:
	PropertyListAnimation(QObject* target, const QByteArray& propertyName, QList<QVariant> valueListA, QObject* parent = 0)
	    : QPropertyAnimation(target, propertyName, parent)
	    , valueList(valueListA)
	{
	}

	virtual QVariant interpolated(const QVariant& from, const QVariant& to, qreal progress) const;

	QList<QVariant> values() { return valueList; }

	void setValues(QList<QVariant> valueListA);

private:
	QList<QVariant> valueList;
};

QVariant PropertyListAnimation::interpolated(const QVariant& from, const QVariant& to, qreal progress) const
{
	int i;

	i = (int)(progress * (int)valueList.size());
	if (i < 0)
		i = 0;
	else if (i >= valueList.size())
		i = (int)valueList.size() - 1;
	return valueList[i];
}

void PropertyListAnimation::setValues(QList<QVariant> valueListA)
{
	qreal progress;

	valueList = valueListA;
	progress  = easingCurve().valueForProgress(qreal(currentTime()) / qreal(totalDuration()));
	updateCurrentValue(interpolated(0, 0, progress));
}

//------------------------------------------------------------------------
// OutlineModel
//------------------------------------------------------------------------

class OutlineModel : public QAbstractItemModel
{
public:
	OutlineModel(XpdfWidget* pdfA);
	virtual QModelIndex index(int row, int column, const QModelIndex& par = QModelIndex()) const;
	virtual QModelIndex parent(const QModelIndex& idx) const;
	virtual int         rowCount(const QModelIndex& par = QModelIndex()) const;
	virtual int         columnCount(const QModelIndex& par = QModelIndex()) const;
	virtual QVariant    data(const QModelIndex& idx, int role) const;

	QModelIndex findPageIndex(int pg, QTreeView* tree, const QModelIndex& idx = QModelIndex());

	void beginOpenNewDoc();
	void endOpenNewDoc();

private:
	int         getItemRow(XpdfOutlineHandle item) const;
	XpdfWidget* pdf;
};

OutlineModel::OutlineModel(XpdfWidget* pdfA)
{
	pdf = pdfA;
}

QModelIndex OutlineModel::index(int row, int column, const QModelIndex& par) const
{
	XpdfOutlineHandle item;
	int               nChildren;

	if (par.isValid())
	{
		// children of an outline item
		item = (XpdfOutlineHandle)par.internalPointer();
	}
	else
	{
		// root outline items
		item = nullptr;
	}
	nChildren = pdf->getOutlineNumChildren(item);
	if (row < 0 || row >= nChildren || column != 0)
		return QModelIndex();
	return createIndex(row, 0, pdf->getOutlineChild(item, row));
}

QModelIndex OutlineModel::parent(const QModelIndex& idx) const
{
	XpdfOutlineHandle item, par;
	int               row;

	if (!idx.isValid())
		return QModelIndex();
	item = (XpdfOutlineHandle)idx.internalPointer();
	if (!(par = pdf->getOutlineParent(item)))
		return QModelIndex();
	if ((row = getItemRow(par)) < 0)
		return QModelIndex();
	return createIndex(row, 0, par);
}

int OutlineModel::rowCount(const QModelIndex& par) const
{
	XpdfOutlineHandle item;

	if (par.isValid())
	{
		// children of an outline item
		item = (XpdfOutlineHandle)par.internalPointer();
	}
	else
	{
		// root outline items
		item = nullptr;
	}
	return pdf->getOutlineNumChildren(item);
}

int OutlineModel::columnCount(const QModelIndex& par) const
{
	return 1;
}

QVariant OutlineModel::data(const QModelIndex& idx, int role) const
{
	XpdfOutlineHandle item;

	if (role != Qt::DisplayRole)
		return QVariant();
	item = (XpdfOutlineHandle)idx.internalPointer();
	return QVariant(pdf->getOutlineTitle(item));
}

int OutlineModel::getItemRow(XpdfOutlineHandle item) const
{
	XpdfOutlineHandle par;

	par                 = pdf->getOutlineParent(item);
	const int nChildren = pdf->getOutlineNumChildren(par);
	for (int i = 0; i < nChildren; ++i)
		if (item == pdf->getOutlineChild(par, i))
			return i;
	return -1;
}

QModelIndex OutlineModel::findPageIndex(int pg, QTreeView* tree, const QModelIndex& idx)
{
	QModelIndex       childIdx, lastIdx;
	XpdfOutlineHandle thisItem, childItem;
	int               thisPage, lastPage, childPage;

	if (idx.isValid())
	{
		thisItem = (XpdfOutlineHandle)idx.internalPointer();
		thisPage = pdf->getOutlineTargetPage(thisItem);
		if (thisPage == pg)
			return idx;
		if (thisPage > pg)
			return QModelIndex();
		if (!tree->isExpanded(idx))
			return idx;
	}
	else
	{
		thisPage = 0;
	}

	lastPage = thisPage;
	lastIdx  = idx;
	for (int i = 0; i < rowCount(idx); ++i)
	{
		childIdx = findPageIndex(pg, tree, index(i, 0, idx));
		if (!childIdx.isValid())
			break;
		childItem = (XpdfOutlineHandle)childIdx.internalPointer();
		childPage = pdf->getOutlineTargetPage(childItem);
		if (childPage > lastPage)
		{
			lastPage = childPage;
			lastIdx  = childIdx;
		}
	}
	if (lastPage == 0)
		return QModelIndex();
	return lastIdx;
}

void OutlineModel::beginOpenNewDoc()
{
	beginResetModel();
}

void OutlineModel::endOpenNewDoc()
{
	endResetModel();
}

//------------------------------------------------------------------------
// LayerModel
//------------------------------------------------------------------------

class LayerModel : public QAbstractItemModel
{
public:
	LayerModel(XpdfWidget* pdfA);
	virtual QModelIndex   index(int row, int column, const QModelIndex& par = QModelIndex()) const;
	virtual QModelIndex   parent(const QModelIndex& idx) const;
	virtual int           rowCount(const QModelIndex& par = QModelIndex()) const;
	virtual int           columnCount(const QModelIndex& par = QModelIndex()) const;
	virtual QVariant      data(const QModelIndex& idx, int role) const;
	virtual bool          setData(const QModelIndex& idx, const QVariant& value, int role);
	virtual Qt::ItemFlags flags(const QModelIndex& idx) const;

	void beginOpenNewDoc();
	void endOpenNewDoc();

private:
	int getOrderRow(XpdfLayerOrderHandle order) const;

	XpdfWidget* pdf;
};

LayerModel::LayerModel(XpdfWidget* pdfA)
{
	pdf = pdfA;
}

QModelIndex LayerModel::index(int row, int column, const QModelIndex& par) const
{
	XpdfLayerOrderHandle order;
	int                  nChildren;

	if (par.isValid())
	{
		// children of a layer display order tree node
		order = (XpdfLayerOrderHandle)par.internalPointer();
	}
	else
	{
		// children of display order tree root
		order = pdf->getLayerOrderRoot();
	}
	nChildren = pdf->getLayerOrderNumChildren(order);
	if (row < 0 || row >= nChildren || column != 0)
		return QModelIndex();
	return createIndex(row, 0, pdf->getLayerOrderChild(order, row));
}

QModelIndex LayerModel::parent(const QModelIndex& idx) const
{
	XpdfLayerOrderHandle order, par;
	int                  row;

	if (!idx.isValid())
		return QModelIndex();
	order = (XpdfLayerOrderHandle)idx.internalPointer();
	if (!(par = pdf->getLayerOrderParent(order)))
		return QModelIndex();
	if ((row = getOrderRow(par)) < 0)
		return QModelIndex();
	return createIndex(row, 0, par);
}

int LayerModel::rowCount(const QModelIndex& par) const
{
	XpdfLayerOrderHandle order;

	if (par.isValid())
	{
		// children of a layer display order tree node
		order = (XpdfLayerOrderHandle)par.internalPointer();
	}
	else
	{
		// children of display order tree root
		order = pdf->getLayerOrderRoot();
	}
	return pdf->getLayerOrderNumChildren(order);
}

int LayerModel::columnCount(const QModelIndex& par) const
{
	return 1;
}

QVariant LayerModel::data(const QModelIndex& idx, int role) const
{
	XpdfLayerOrderHandle order;
	XpdfLayerHandle      layer;

	if (!idx.isValid())
		return false;
	if (role == Qt::DisplayRole)
	{
		order = (XpdfLayerOrderHandle)idx.internalPointer();
		if (pdf->getLayerOrderIsName(order))
			return QVariant(pdf->getLayerOrderName(order));
		else
			return QVariant(pdf->getLayerName(pdf->getLayerOrderLayer(order)));
	}
	else if (role == Qt::CheckStateRole)
	{
		order = (XpdfLayerOrderHandle)idx.internalPointer();
		layer = pdf->getLayerOrderLayer(order);
		if (!layer)
			return QVariant();
		return pdf->getLayerVisibility(layer) ? Qt::Checked : Qt::Unchecked;
	}
	else
	{
		return QVariant();
	}
}

bool LayerModel::setData(const QModelIndex& idx, const QVariant& value, int role)
{
	XpdfLayerOrderHandle order;
	XpdfLayerHandle      layer;
	bool                 vis;

	if (!idx.isValid())
		return false;
	if (role != Qt::CheckStateRole)
		return false;
	order = (XpdfLayerOrderHandle)idx.internalPointer();
	layer = pdf->getLayerOrderLayer(order);
	if (!layer)
		return false;
	vis = value == Qt::Checked;
	if (vis != pdf->getLayerVisibility(layer))
		pdf->setLayerVisibility(layer, vis);
	emit dataChanged(idx, idx);
	return true;
}

Qt::ItemFlags LayerModel::flags(const QModelIndex& idx) const
{
	if (!idx.isValid())
		return Qt::NoItemFlags;
	// NB: this does not include Qt::ItemIsUserCheckable because we use
	// the QTreeView::clicked signal to toggle the checkbox -- which
	// handles clicks anywhere in the item, including in the checkbox
	return Qt::ItemIsEnabled;
}

int LayerModel::getOrderRow(XpdfLayerOrderHandle order) const
{
	XpdfLayerOrderHandle par;

	par = pdf->getLayerOrderParent(order);
	if (par)
	{
		const int nChildren = pdf->getLayerOrderNumChildren(par);
		for (int i = 0; i < nChildren; ++i)
			if (order == pdf->getLayerOrderChild(par, i))
				return i;
		return -1;
	}
	else
	{
		return 0;
	}
}

void LayerModel::beginOpenNewDoc()
{
	beginResetModel();
}

void LayerModel::endOpenNewDoc()
{
	endResetModel();
}

//------------------------------------------------------------------------
// XpdfTabInfo
//------------------------------------------------------------------------

class XpdfTabInfo
{
public:
	XpdfTabInfo(QListWidgetItem* listItemA, XpdfWidget* pdfA, QTreeView* outlineTreeA, QTreeView* layerTreeA, QTableWidget* attachmentListA)
	    : listItem(listItemA)
	    , pdf(pdfA)
	    , outlineTree(outlineTreeA)
	    , layerTree(layerTreeA)
	    , attachmentList(attachmentListA)
	{
	}

	QListWidgetItem* listItem;
	XpdfWidget*      pdf;
	QTreeView*       outlineTree;
	QTreeView*       layerTree;
	QTableWidget*    attachmentList;
};

//------------------------------------------------------------------------
// XpdfViewer
//------------------------------------------------------------------------

XpdfViewer::XpdfViewer(XpdfApp* appA, bool fullScreen)
{
	setAttribute(Qt::WA_DeleteOnClose, true);
	app = appA;
	createWindow();
	if (fullScreen)
	{
		move(0, 0);
		enterFullScreenMode();
	}
	remoteServer = nullptr;
}

XpdfViewer* XpdfViewer::create(XpdfApp* app, QString fileName, int page, QString destName, int rot, QString password, bool fullScreen)
{
	XpdfViewer* viewer;

	viewer = new XpdfViewer(app, fullScreen);
	if (!viewer->open(fileName, page, destName, rot, password))
	{
		viewer->close();
		delete viewer;
		return nullptr;
	}
	return viewer;
}

XpdfViewer::~XpdfViewer()
{
	destroyWindow();
}

// QSplitter::sizeHint() doesn't take into account collapsed children,
// which leads to an incorrect window size when we start with
// initialSidebarState=no.  So sizeHint() is reimplemented to subtract
// out the sidebar width if needed.  This also handles the case where
// initialSidebarWidth is set.
QSize XpdfViewer::sizeHint() const
{
	int toolBarWidth, mainWidth, height;

	toolBarWidth     = toolBar->sizeHint().width();
	mainWidth        = sidebarSplitter->sizeHint().width();
	QList<int> sizes = sidebarSplitter->sizes();
	if (sizes[0] == 0 && sizes[1] > 0)
	{
		mainWidth -= sidebarSplitter->widget(0)->sizeHint().width();
	}
	else if (initialSidebarWidth > 0)
	{
		mainWidth -= sidebarSplitter->widget(0)->sizeHint().width();
		mainWidth += initialSidebarWidth;
	}
	height = QMainWindow::sizeHint().height();
	return QSize(qMax(toolBarWidth, mainWidth), height);
}

// Tweak the window size before showing a new window.  This does two
// things:
// (1) By default, Qt won't allow windows larger than 2/3 of the
//     screen size.  This function kludges around that by resizing to
//     the sizeHint, with a max of 60 pixels smaller than the screen
//     size.
// (2) Sets the sidebar splitter sizes.  This has to be done after the
//     PDF file is loaded and before the window is shown.
void XpdfViewer::tweakSize()
{
	if (initialSidebarWidth > 0)
	{
		QList<int> sidebarSplitterSizes = sidebarSplitter->sizes();
		sidebarSplitterSizes[0]         = initialSidebarWidth;
		sidebarSplitterSizes[1]         = sidebarSplitter->widget(1)->sizeHint().width();
		sidebarSplitter->setSizes(sidebarSplitterSizes);
	}

	QSize hint   = sizeHint();
	QRect screen = QGuiApplication::primaryScreen()->availableGeometry();
	int   w      = hint.width();
	int   h      = hint.height();
	if (w > screen.width() - 60) w = screen.width() - 60;
	if (h > screen.height() - 60) h = screen.height() - 60;
	resize(w, h);
}

bool XpdfViewer::close()
{
	if (tabInfo->getLength() > 0)
	{
		app->startUpdatePagesFile();
		for (int i = 0; i < tabInfo->getLength(); ++i)
		{
			XpdfTabInfo* tab = (XpdfTabInfo*)tabInfo->get(i);
			app->updatePagesFile(tab->pdf->getFileName(), tab->pdf->getMidPage());
		}
		app->finishUpdatePagesFile();
	}
	return QMainWindow::close();
}

//------------------------------------------------------------------------

bool XpdfViewer::open(QString fileName, int page, QString destName, int rot, QString password)
{
	XpdfWidget::ErrorCode err;

	if (currentTab->pdf->hasOpenDocument())
	{
		app->startUpdatePagesFile();
		app->updatePagesFile(currentTab->pdf->getFileName(), currentTab->pdf->getMidPage());
		app->finishUpdatePagesFile();
	}
	err = currentTab->pdf->loadFile(fileName, password);
	if (err != XpdfWidget::pdfOk)
	{
		QMessageBox::warning(nullptr, "Xpdf Error", "Couldn't open file '" + fileName + "'");
		return false;
	}
	if (!destName.isEmpty())
	{
		currentTab->pdf->gotoNamedDestination(destName);
	}
	else
	{
		if (page < 0)
			page = app->getSavedPageNumber(fileName);
		currentTab->pdf->gotoPage(page);
	}
	if (rot != 0)
		currentTab->pdf->setRotate(rot);
	// after opening a document, focus goes to the XpdfWidget
	currentTab->pdf->setFocus(Qt::OtherFocusReason);
	lastOpenedTab  = currentTab;
	lastFileOpened = fileName;
	return true;
}

bool XpdfViewer::openInNewTab(QString fileName, int page, QString destName, int rot, QString password, bool switchToTab)
{
	bool ok;
	int  oldTabIndex;

	oldTabIndex = tabList->currentRow();
	addTab();
	updateModeInfo();
	ok = open(fileName, page, destName, rot, password);
	if (!ok)
	{
		if (tabInfo->getLength() > 1)
			closeTab(currentTab);
		return false;
	}
	lastOpenedTab = currentTab;
	if (!switchToTab)
		tabList->setCurrentRow(oldTabIndex);
	lastFileOpened = fileName;
	return true;
}

bool XpdfViewer::checkOpen(QString fileName, int page, QString destName, QString password)
{
	XpdfWidget::ErrorCode err;

	if (fileName != currentTab->pdf->getFileName())
	{
		if (currentTab->pdf->hasOpenDocument())
		{
			app->startUpdatePagesFile();
			app->updatePagesFile(currentTab->pdf->getFileName(), currentTab->pdf->getMidPage());
			app->finishUpdatePagesFile();
		}
		err = currentTab->pdf->loadFile(fileName, password);
		if (err != XpdfWidget::pdfOk)
		{
			QMessageBox::warning(nullptr, "Xpdf Error", "Couldn't open file '" + fileName + "'");
			return false;
		}
	}
	if (!destName.isEmpty())
	{
		currentTab->pdf->gotoNamedDestination(destName);
	}
	else
	{
		if (page < 0)
			page = app->getSavedPageNumber(fileName);
		currentTab->pdf->gotoPage(page);
	}
	// after opening a document, focus goes to the XpdfWidget
	currentTab->pdf->setFocus(Qt::OtherFocusReason);
	lastFileOpened = fileName;
	lastOpenedTab  = currentTab;
	return true;
}

// Disable the default popup menu (which shows a toggle to hide the
// toolbar).
QMenu* XpdfViewer::createPopupMenu()
{
	return nullptr;
}

void XpdfViewer::saveSession(FILE* out, int format)
{
	fprintf(out, "%d\n", tabInfo->getLength());
	for (int i = 0; i < tabInfo->getLength(); ++i)
	{
		XpdfWidget* pdf      = ((XpdfTabInfo*)tabInfo->get(i))->pdf;
		QString     fileName = pdf->getFileName();
		if (!fileName.isEmpty())
		{
			fprintf(out, "%s\n", fileName.toUtf8().constData());
			char displayModeChar;
			switch (pdf->getDisplayMode())
			{
			case XpdfWidget::pdfDisplaySingle:
				displayModeChar = 's';
				break;
			case XpdfWidget::pdfDisplayContinuous:
				displayModeChar = 'c';
				break;
			case XpdfWidget::pdfDisplaySideBySideSingle:
				displayModeChar = 'b';
				break;
			case XpdfWidget::pdfDisplaySideBySideContinuous:
				displayModeChar = 'B';
				break;
			case XpdfWidget::pdfDisplayHorizontalContinuous:
				displayModeChar = 'h';
				break;
			default:
				displayModeChar = 'c';
				break;
			}
			fprintf(out, "%c %d %g %d %d %d\n", displayModeChar, pdf->getMidPage(), pdf->getZoom(), pdf->getRotate(), pdf->getScrollX(), pdf->getScrollY());
		}
	}
}

void XpdfViewer::loadSession(FILE* in, int format)
{
	char line1[1024], line2[1024];
	if (!fgets(line1, sizeof(line1), in))
		return;
	int nTabs;
	if (sscanf(line1, "%d", &nTabs) != 1)
		return;

	bool first = true;
	for (int i = 0; i < nTabs; ++i)
	{
		if (!fgets(line1, sizeof(line1), in) || !fgets(line2, sizeof(line2), in))
			return;
		size_t n = strlen(line1);
		if (n > 0 && line1[n - 1] == '\n')
			line1[--n] = '\0';
		if (n > 0 && line1[n - 1] == '\r')
			line1[--n] = '\0';
		char   displayModeChar;
		int    page, rotate, scrollX, scrollY;
		double zoom;
		if (sscanf(line2, "%c %d %lf %d %d %d", &displayModeChar, &page, &zoom, &rotate, &scrollX, &scrollY) != 6)
			return;
		bool ok;
		if (first && !currentTab->pdf->hasOpenDocument())
			ok = open(line1, page, "", rotate, "");
		else
			ok = openInNewTab(line1, page, "", rotate, "", false);
		if (ok)
		{
			XpdfWidget* pdf = lastOpenedTab->pdf;
			switch (displayModeChar)
			{
			case 's':
				pdf->setDisplayMode(XpdfWidget::pdfDisplaySingle);
				break;
			case 'c':
				pdf->setDisplayMode(XpdfWidget::pdfDisplayContinuous);
				break;
			case 'b':
				pdf->setDisplayMode(XpdfWidget::pdfDisplaySideBySideSingle);
				break;
			case 'B':
				pdf->setDisplayMode(XpdfWidget::pdfDisplaySideBySideContinuous);
				break;
			case 'h':
				pdf->setDisplayMode(XpdfWidget::pdfDisplayHorizontalContinuous);
				break;
			default: break;
			}
			pdf->setRotate(rotate);
			pdf->setZoom(zoom);
			pdf->scrollTo(scrollX, scrollY);
		}
		first = false;
	}
}

bool XpdfViewer::isEmpty()
{
	return tabInfo->getLength() == 1 && ((XpdfTabInfo*)tabInfo->get(0))->pdf->getFileName().isEmpty();
}

//------------------------------------------------------------------------
// remote server
//------------------------------------------------------------------------

void XpdfViewer::startRemoteServer(const QString& remoteServerName)
{
	remoteServer = new QLocalServer(this);
	connect(remoteServer, SIGNAL(newConnection()), this, SLOT(remoteServerConnection()));
	if (!remoteServer->listen("xpdf_" + remoteServerName))
		error(errIO, -1, "Couldn't set up the remote server socket");
}

void XpdfViewer::remoteServerConnection()
{
	QLocalSocket* sock;

	sock = remoteServer->nextPendingConnection();
	connect(sock, SIGNAL(readyRead()), this, SLOT(remoteServerRead()));
}

void XpdfViewer::remoteServerRead()
{
	QLocalSocket* sock;
	char          buf[1024];
	qint64        n;

	sock = (QLocalSocket*)sender();
	while (sock->canReadLine())
	{
		n = sock->readLine(buf, sizeof(buf));
		if (n > 0)
		{
			if (buf[n - 1] == '\n')
				buf[n - 1] = '\0';
			execCmd(buf, nullptr);
		}
	}
}

//------------------------------------------------------------------------
// commands
//------------------------------------------------------------------------

void XpdfViewer::execCmd(const char* cmd, QInputEvent* event)
{
	std::string name;
	VEC_STR     args;
	const char *p0, *p1;
	int         nArgs;
	int         a, b, m, cmp;

	//----- parse the command
	args.resize(cmdMaxArgs);
	nArgs = 0;
	for (int i = 0; i < cmdMaxArgs; ++i)
		args[i] = nullptr;
	p0 = cmd;
	for (p1 = p0; *p1 && isalnum(*p1); ++p1)
		;
	if (p1 == p0) goto err1;

	name.assign(p0, (int)(p1 - p0));
	if (*p1 == '(')
	{
		while (nArgs < cmdMaxArgs)
		{
			++p1;
			while (*p1 && *p1 != ',' && *p1 != ')')
			{
				if (*p1 == '\x01' && p1[1]) ++p1;
				args[nArgs] = *p1++;
			}
			++nArgs;
			if (*p1 != ',')
				break;
		}
		if (*p1 != ')') goto err1;
		++p1;
	}
	if (*p1)
		goto err1;

	//----- find the command
	a   = -1;
	b   = nCmds;
	cmp = 0;
	// invariant: cmdTab[a].name < name < cmdTab[b].name
	while (b - a > 1)
	{
		m   = (a + b) / 2;
		cmp = strcmp(cmdTab[m].name, name.c_str());
		if (cmp < 0)
			a = m;
		else if (cmp > 0)
			b = m;
		else
			a = b = m;
	}
	if (cmp != 0)
		goto err1;

	//----- execute the command
	if (nArgs != cmdTab[a].nArgs || (cmdTab[a].requiresEvent && !event))
		goto err1;
	if (cmdTab[a].requiresDoc && !currentTab->pdf->hasOpenDocument())
	{
		// don't issue an error message for this -- it happens, e.g., when
		// clicking in a window with no open PDF file
		goto err2;
	}
	(this->*cmdTab[a].func)(args, event);

	//----- clean up
	return;

err1:
	error(errConfig, -1, "Invalid command syntax: '{0:s}'", cmd);
err2:
	return;
}

int XpdfViewer::mouseX(QInputEvent* event)
{
	QEvent::Type eventType;

	if (!event)
		return 0;
	eventType = event->type();
	if (eventType == QEvent::MouseButtonPress || eventType == QEvent::MouseButtonRelease || eventType == QEvent::MouseButtonDblClick || eventType == QEvent::MouseMove)
	{
		return (int)(((QMouseEvent*)event)->pos().x() * scaleFactor);
	}
	else if (eventType == QEvent::Wheel)
	{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
		return (int)(((QWheelEvent*)event)->position().x() * scaleFactor);
#else
		return (int)(((QWheelEvent*)event)->x() * scaleFactor);
#endif
	}
	else
	{
		return 0;
	}
}

int XpdfViewer::mouseY(QInputEvent* event)
{
	QEvent::Type eventType;

	if (!event)
		return 0;
	eventType = event->type();
	if (eventType == QEvent::MouseButtonPress || eventType == QEvent::MouseButtonRelease || eventType == QEvent::MouseButtonDblClick || eventType == QEvent::MouseMove)
	{
		return (int)(((QMouseEvent*)event)->pos().y() * scaleFactor);
	}
	else if (eventType == QEvent::Wheel)
	{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
		return (int)(((QWheelEvent*)event)->position().y() * scaleFactor);
#else
		return (int)(((QWheelEvent*)event)->y() * scaleFactor);
#endif
	}
	else
	{
		return 0;
	}
}

void XpdfViewer::cmdAbout(const VEC_STR& args, QInputEvent* event)
{
	if (!aboutDialog) createAboutDialog();
	aboutDialog->show();
	aboutDialog->raise();
}

void XpdfViewer::cmdBlockSelectMode(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->setBlockSelectMode();
	updateSelectModeInfo();
}

void XpdfViewer::cmdCheckOpenFile(const VEC_STR& args, QInputEvent* event)
{
	checkOpen(args[0].c_str(), -1, "", "");
}

void XpdfViewer::cmdCheckOpenFileAtDest(const VEC_STR& args, QInputEvent* event)
{
	checkOpen(args[0].c_str(), 1, args[1].c_str(), "");
}

void XpdfViewer::cmdCheckOpenFileAtPage(const VEC_STR& args, QInputEvent* event)
{
	checkOpen(args[0].c_str(), atoi(args[1].c_str()), "", "");
}

void XpdfViewer::cmdCloseSidebar(const VEC_STR& args, QInputEvent* event)
{
	QList<int> sizes = sidebarSplitter->sizes();
	if (sizes[0] == 0)
		return;
	sidebarWidth = sizes[0];
	sizes[0]     = 0;
	sizes[1] += sidebarWidth;
	sidebarSplitter->setSizes(sizes);
	toggleSidebarMenuItem->setChecked(false);
}

void XpdfViewer::cmdCloseSidebarMoveResizeWin(const VEC_STR& args, QInputEvent* event)
{
	int        newWidth;
	QList<int> sizes = sidebarSplitter->sizes();
	if (sizes[0] == 0) return;
	sidebarWidth = sizes[0];
	newWidth     = width() - sidebarWidth;
	sizes[0]     = 0;
	sidebarSplitter->setSizes(sizes);
	setGeometry(geometry().x() + sidebarWidth, geometry().y(), newWidth, height());
	toggleSidebarMenuItem->setChecked(false);
}

void XpdfViewer::cmdCloseSidebarResizeWin(const VEC_STR& args, QInputEvent* event)
{
	int        newWidth;
	QList<int> sizes = sidebarSplitter->sizes();
	if (sizes[0] == 0) return;
	sidebarWidth = sizes[0];
	newWidth     = width() - sidebarWidth;
	sizes[0]     = 0;
	sidebarSplitter->setSizes(sizes);
	resize(newWidth, height());
	toggleSidebarMenuItem->setChecked(false);
}

void XpdfViewer::cmdCloseTabOrQuit(const VEC_STR& args, QInputEvent* event)
{
	closeTab(currentTab);
	if (tabInfo->getLength() == 0)
	{
		app->closeWindowOrQuit(this);
		return;
	}
}

void XpdfViewer::cmdCloseWindowOrQuit(const VEC_STR& args, QInputEvent* event)
{
	app->closeWindowOrQuit(this);
}

void XpdfViewer::cmdContinuousMode(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->setDisplayMode(XpdfWidget::pdfDisplayContinuous);
	updateModeInfo();
}

void XpdfViewer::cmdCopy(const VEC_STR& args, QInputEvent* event)
{
	if (!currentTab->pdf->okToExtractText())
	{
		QMessageBox::warning(nullptr, "Xpdf Error", "This PDF file does not allow copying text");
		return;
	}
	currentTab->pdf->copySelection();
}

void XpdfViewer::cmdCopyLinkTarget(const VEC_STR& args, QInputEvent* event)
{
	QApplication::clipboard()->setText(linkTargetInfo);
}

void XpdfViewer::cmdEndPan(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->getCore()->endPan(mouseX(event), mouseY(event));
}

void XpdfViewer::cmdEndSelection(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->getCore()->endSelection(mouseX(event), mouseY(event));
}

void XpdfViewer::cmdExpandSidebar(const VEC_STR& args, QInputEvent* event)
{
	QList<int> sizes   = sidebarSplitter->sizes();
	int        nPixels = atoi(args[0].c_str());
	if (nPixels > sizes[1])
		nPixels = sizes[1];
	sizes[0] += nPixels;
	sizes[1] -= nPixels;
	sidebarSplitter->setSizes(sizes);
	toggleSidebarMenuItem->setChecked(true);
}

void XpdfViewer::cmdFind(const VEC_STR& args, QInputEvent* event)
{
	clearFindError();
	findEdit->setFocus(Qt::OtherFocusReason);
	findEdit->selectAll();
}

void XpdfViewer::cmdFindFirst(const VEC_STR& args, QInputEvent* event)
{
	clearFindError();
	int flags = getFindCaseFlag();
	if (findWholeWordsAction->isChecked())
		flags |= XpdfWidget::findWholeWord;
	if (!currentTab->pdf->find(findEdit->text(), flags))
		showFindError();
}

void XpdfViewer::cmdFindNext(const VEC_STR& args, QInputEvent* event)
{
	clearFindError();
	int flags = XpdfWidget::findNext | getFindCaseFlag();
	if (findWholeWordsAction->isChecked())
		flags |= XpdfWidget::findWholeWord;
	if (!currentTab->pdf->find(findEdit->text(), flags))
		showFindError();
}

void XpdfViewer::cmdFindPrevious(const VEC_STR& args, QInputEvent* event)
{
	clearFindError();
	int flags = XpdfWidget::findBackward | XpdfWidget::findNext | getFindCaseFlag();
	if (findWholeWordsAction->isChecked())
		flags |= XpdfWidget::findWholeWord;
	if (!currentTab->pdf->find(findEdit->text(), flags))
		showFindError();
}

void XpdfViewer::cmdFocusToDocWin(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->setFocus(Qt::OtherFocusReason);
}

void XpdfViewer::cmdFocusToPageNum(const VEC_STR& args, QInputEvent* event)
{
	pageNumber->setFocus(Qt::OtherFocusReason);
	pageNumber->selectAll();
}

void XpdfViewer::cmdFollowLink(const VEC_STR& args, QInputEvent* event)
{
	followLink(event, false, false, false);
}

void XpdfViewer::cmdFollowLinkInNewTab(const VEC_STR& args, QInputEvent* event)
{
	followLink(event, false, true, false);
}

void XpdfViewer::cmdFollowLinkInNewTabNoSel(const VEC_STR& args, QInputEvent* event)
{
	followLink(event, true, true, false);
}

void XpdfViewer::cmdFollowLinkInNewWin(const VEC_STR& args, QInputEvent* event)
{
	followLink(event, false, false, true);
}

void XpdfViewer::cmdFollowLinkInNewWinNoSel(const VEC_STR& args, QInputEvent* event)
{
	followLink(event, true, false, true);
}

void XpdfViewer::cmdFollowLinkNoSel(const VEC_STR& args, QInputEvent* event)
{
	followLink(event, true, false, false);
}

void XpdfViewer::cmdFullScreenMode(const VEC_STR& args, QInputEvent* event)
{
	if (!(windowState() & Qt::WindowFullScreen))
		enterFullScreenMode();
}

void XpdfViewer::cmdGotoDest(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->gotoNamedDestination(args[0].c_str());
	updateZoomInfo();
}

void XpdfViewer::cmdGotoLastPage(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->gotoLastPage();
}

void XpdfViewer::cmdGoBackward(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->goBackward();
}

void XpdfViewer::cmdGoForward(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->goForward();
}

void XpdfViewer::cmdGotoPage(const VEC_STR& args, QInputEvent* event)
{
	const int pg = atoi(args[0].c_str());
	if (pg < 1 || pg > currentTab->pdf->getNumPages())
		return;
	currentTab->pdf->gotoPage(pg);
}

void XpdfViewer::cmdHelp(const VEC_STR& args, QInputEvent* event)
{
	QDesktopServices::openUrl(QUrl(helpURL, QUrl::TolerantMode));
}

void XpdfViewer::cmdHideMenuBar(const VEC_STR& args, QInputEvent* event)
{
	mainMenu->hide();
}

void XpdfViewer::cmdHideToolbar(const VEC_STR& args, QInputEvent* event)
{
	toolBar->hide();
	toggleToolbarMenuItem->setChecked(false);
}

void XpdfViewer::cmdHorizontalContinuousMode(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->setDisplayMode(XpdfWidget::pdfDisplayHorizontalContinuous);
}

void XpdfViewer::cmdLinearSelectMode(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->setLinearSelectMode();
	updateSelectModeInfo();
}

void XpdfViewer::cmdLoadSession(const VEC_STR& args, QInputEvent* event)
{
	app->loadSession(nullptr, true);
}

void XpdfViewer::cmdLoadTabState(const VEC_STR& args, QInputEvent* event)
{
	const auto path = globalParams->getTabStateFile();
	FILE*      f    = openFile(path.c_str(), "rb");
	if (!f)
	{
		const auto msg = fmt::format("Couldn't read the tab file '{}'", path);
		QMessageBox::warning(nullptr, "Xpdf Error", msg.c_str());
		return;
	}

	char line1[1024], line2[1024];
	if (!fgets(line1, sizeof(line1), f))
	{
		fclose(f);
		return;
	}
	size_t n = strlen(line1);
	if (n > 0 && line1[n - 1] == '\n')
		line1[--n] = '\0';
	if (n > 0 && line1[n - 1] == '\r')
		line1[--n] = '\0';
	int format = 1;
	if (!strcmp(line1, "xpdf-tabstate-2"))
		format = 2;

	bool first = true;
	while (((first && format == 1) || fgets(line1, sizeof(line1), f)) && fgets(line2, sizeof(line2), f))
	{
		n = strlen(line1);
		if (n > 0 && line1[n - 1] == '\n')
			line1[--n] = '\0';
		if (n > 0 && line1[n - 1] == '\r')
			line1[--n] = '\0';
		n = strlen(line2);
		if (n > 0 && line2[n - 1] == '\n')
			line2[--n] = '\0';
		if (n > 0 && line2[n - 1] == '\r')
			line2[--n] = '\0';
		char   displayModeChar;
		int    page, rotate, scrollX, scrollY;
		double zoom;
		if (format == 2)
		{
			sscanf(line2, "%c %d %lf %d %d %d", &displayModeChar, &page, &zoom, &rotate, &scrollX, &scrollY);
		}
		else
		{
			page            = atoi(line2);
			rotate          = 0;
			// the rest are unused, but set to make gcc happy
			displayModeChar = 'c';
			zoom            = zoomWidth;
			scrollX = scrollY = 0;
		}
		bool ok;
		if (first && !currentTab->pdf->hasOpenDocument())
			ok = open(line1, page, "", rotate, "");
		else
			ok = openInNewTab(line1, page, "", rotate, "", false);
		if (ok && format == 2)
		{
			XpdfWidget* pdf = lastOpenedTab->pdf;
			switch (displayModeChar)
			{
			case 's':
				pdf->setDisplayMode(XpdfWidget::pdfDisplaySingle);
				break;
			case 'c':
				pdf->setDisplayMode(XpdfWidget::pdfDisplayContinuous);
				break;
			case 'b':
				pdf->setDisplayMode(XpdfWidget::pdfDisplaySideBySideSingle);
				break;
			case 'B':
				pdf->setDisplayMode(XpdfWidget::pdfDisplaySideBySideContinuous);
				break;
			case 'h':
				pdf->setDisplayMode(XpdfWidget::pdfDisplayHorizontalContinuous);
				break;
			default: break;
			}
			pdf->setRotate(rotate);
			pdf->setZoom(zoom);
			pdf->scrollTo(scrollX, scrollY);
		}
		first = false;
	}

	fclose(f);
}

void XpdfViewer::cmdNewTab(const VEC_STR& args, QInputEvent* event)
{
	addTab();
	updateModeInfo();
	updateDocInfo();
}

void XpdfViewer::cmdNewWindow(const VEC_STR& args, QInputEvent* event)
{
	app->newWindow();
}

void XpdfViewer::cmdNextPage(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->gotoNextPage();
}

void XpdfViewer::cmdNextPageNoScroll(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->gotoNextPage(false);
}

void XpdfViewer::cmdNextTab(const VEC_STR& args, QInputEvent* event)
{
	if (tabInfo->getLength() == 1)
		return;
	for (int i = 0; i < tabInfo->getLength(); ++i)
	{
		if ((XpdfTabInfo*)tabInfo->get(i) == currentTab)
		{
			++i;
			if (i >= tabInfo->getLength())
				i = 0;
			tabList->setCurrentRow(i);
			return;
		}
	}
}

void XpdfViewer::cmdOpen(const VEC_STR& args, QInputEvent* event)
{
	QString startFile, fileName;
	QDir    startDir;

	if (!(startFile = currentTab->pdf->getFileName()).isEmpty())
	{
		startDir = QDir(startFile);
		startDir.cdUp();
	}
	else if (!lastFileOpened.isEmpty())
	{
		startDir = QDir(lastFileOpened);
		startDir.cdUp();
	}
	else
	{
		startDir = QDir(".");
	}
	fileName = QFileDialog::getOpenFileName(this, "Open PDF File", startDir.canonicalPath(), "PDF files (*.pdf)");
	if (fileName.isEmpty())
		return;
	open(fileName, -1, "", 0, "");
}

void XpdfViewer::cmdOpenErrorWindow(const VEC_STR& args, QInputEvent* event)
{
	errorWindow->show();
	errorWindow->raise();
}

void XpdfViewer::cmdOpenFile(const VEC_STR& args, QInputEvent* event)
{
	open(args[0].c_str(), -1, "", 0, "");
}

void XpdfViewer::cmdOpenFile2(const VEC_STR& args, QInputEvent* event)
{
	const char* file = args[0].c_str();
	int         page;
	if (args[1].size() == 0)
		page = -1;
	else
		page = atoi(args[1].c_str());
	const char* dest     = args[2].c_str();
	const char* password = args[3].c_str();
	const auto  location = args[4];
	if (location == "win")
		app->openInNewWindow(file, page, dest, 0, password);
	else if (location == "tab")
		openInNewTab(file, page, dest, 0, password, true);
	else if (location == "check")
		checkOpen(file, page, dest, password);
	else
		open(file, page, dest, 0, password);
}

void XpdfViewer::cmdOpenFileAtDest(const VEC_STR& args, QInputEvent* event)
{
	open(args[0].c_str(), 1, args[1].c_str(), 0, "");
}

void XpdfViewer::cmdOpenFileAtDestIn(const VEC_STR& args, QInputEvent* event)
{
	if (args[2] == "win")
		app->openInNewWindow(args[0].c_str(), 1, args[1].c_str());
	else if (args[2] == "tab")
		openInNewTab(args[0].c_str(), 1, args[1].c_str(), 0, "", true);
	else
		open(args[0].c_str(), 1, args[1].c_str(), 0, "");
}

void XpdfViewer::cmdOpenFileAtPage(const VEC_STR& args, QInputEvent* event)
{
	open(args[0].c_str(), atoi(args[1].c_str()), "", 0, "");
}

void XpdfViewer::cmdOpenFileAtPageIn(const VEC_STR& args, QInputEvent* event)
{
	if (args[2] == "win")
		app->openInNewWindow(args[0].c_str(), atoi(args[1].c_str()));
	else if (args[2] == "tab")
		openInNewTab(args[0].c_str(), atoi(args[1].c_str()), "", 0, "", true);
	else
		open(args[0].c_str(), atoi(args[1].c_str()), "", 0, "");
}

void XpdfViewer::cmdOpenFileIn(const VEC_STR& args, QInputEvent* event)
{
	if (args[1] == "win")
		app->openInNewWindow(args[0].c_str(), -1);
	else if (args[1] == "tab")
		openInNewTab(args[0].c_str(), -1, "", 0, "", true);
	else
		open(args[0].c_str(), -1, "", 0, "");
}

void XpdfViewer::cmdOpenIn(const VEC_STR& args, QInputEvent* event)
{
	QString startFile, fileName;
	QDir    startDir;

	if (!(startFile = currentTab->pdf->getFileName()).isEmpty())
	{
		startDir = QDir(startFile);
		startDir.cdUp();
	}
	else if (!lastFileOpened.isEmpty())
	{
		startDir = QDir(lastFileOpened);
		startDir.cdUp();
	}
	else
	{
		startDir = QDir(".");
	}
	fileName = QFileDialog::getOpenFileName(this, "Open PDF File", startDir.canonicalPath(), "PDF files (*.pdf)");
	if (fileName.isEmpty())
		return;
	if (args[0] == "win")
		app->openInNewWindow(fileName, -1);
	else if (args[0] == "tab")
		openInNewTab(fileName, -1, "", 0, "", true);
	else
		open(fileName, -1, "", 0, "");
}

void XpdfViewer::cmdOpenSidebar(const VEC_STR& args, QInputEvent* event)
{
	QList<int> sizes = sidebarSplitter->sizes();
	if (sizes[0] > 0) return;
	sizes[0] = sidebarWidth;
	sizes[1] -= sidebarWidth;
	sidebarSplitter->setSizes(sizes);
	toggleSidebarMenuItem->setChecked(true);
}

void XpdfViewer::cmdOpenSidebarMoveResizeWin(const VEC_STR& args, QInputEvent* event)
{
	int        newWidth;
	QList<int> sizes = sidebarSplitter->sizes();
	if (sizes[0] > 0) return;
	sizes[0] = sidebarWidth;
	newWidth = width() + sidebarWidth;
	sidebarSplitter->setSizes(sizes);
	setGeometry(geometry().x() - sidebarWidth, geometry().y(), newWidth, height());
	toggleSidebarMenuItem->setChecked(true);
}

void XpdfViewer::cmdOpenSidebarResizeWin(const VEC_STR& args, QInputEvent* event)
{
	int        newWidth;
	QList<int> sizes = sidebarSplitter->sizes();
	if (sizes[0] > 0) return;
	sizes[0] = sidebarWidth;
	newWidth = width() + sidebarWidth;
	sidebarSplitter->setSizes(sizes);
	resize(newWidth, height());
	toggleSidebarMenuItem->setChecked(true);
}

void XpdfViewer::cmdPageDown(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->scrollPageDown();
}

void XpdfViewer::cmdPageUp(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->scrollPageUp();
}

void XpdfViewer::cmdPostPopupMenu(const VEC_STR& args, QInputEvent* event)
{
	if (!popupMenu) return;
	popupMenu->exec(currentTab->pdf->mapToGlobal(QPoint(mouseX(event), mouseY(event))));
}

void XpdfViewer::cmdPrevPage(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->gotoPreviousPage();
}

void XpdfViewer::cmdPrevPageNoScroll(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->gotoPreviousPage(false);
}

void XpdfViewer::cmdPrevTab(const VEC_STR& args, QInputEvent* event)
{
	if (tabInfo->getLength() == 1)
		return;
	for (int i = 0; i < tabInfo->getLength(); ++i)
	{
		if ((XpdfTabInfo*)tabInfo->get(i) == currentTab)
		{
			--i;
			if (i < 0)
				i = tabInfo->getLength() - 1;
			tabList->setCurrentRow(i);
			return;
		}
	}
}

#if XPDFWIDGET_PRINTING
void XpdfViewer::cmdPrint(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->print(true);
}
#endif

void XpdfViewer::cmdQuit(const VEC_STR& args, QInputEvent* event)
{
	app->quit();
}

void XpdfViewer::cmdRaise(const VEC_STR& args, QInputEvent* event)
{
	show();
	activateWindow();
	raise();
}

void XpdfViewer::cmdReload(const VEC_STR& args, QInputEvent* event)
{
	if (currentTab->pdf->reload() != XpdfWidget::pdfOk)
		QMessageBox::warning(nullptr, "Xpdf Error", "Couldn't reload file");
}

void XpdfViewer::cmdRotateCW(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->setRotate((currentTab->pdf->getRotate() + 90) % 360);
}

void XpdfViewer::cmdRotateCCW(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->setRotate((currentTab->pdf->getRotate() + 270) % 360);
}

static QString mungeURL(QString url)
{
	static const char* allowed =
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	    "abcdefghijklmnopqrstuvwxyz"
	    "0123456789"
	    "-_.~/?:@&=+,#%";
	QString newURL;
	char    buf[4];

	for (int i = 0; i < url.length(); ++i)
	{
		const char c = url.at(i).toLatin1();
		if (strchr(allowed, c))
		{
			newURL += c;
		}
		else
		{
			snprintf(buf, sizeof(buf), "%%%02x", c & 0xff);
			newURL += buf;
		}
	}
	return newURL;
}

void XpdfViewer::cmdRun(const VEC_STR& args, QInputEvent* event)
{
	QString     s;
	QPoint      pt;
	double      selLRX, selLRY, selURX, selURY, mX, mY;
	int         selPage, mPage;
	bool        gotSel, gotMouse;
	const char *p, *q;
	char        c0, c1;

	gotSel = gotMouse = false;
	selPage = mPage = 0;
	selURX = selURY = selLRX = selLRY = mX = mY = 0;

	std::string cmd;
	std::string fmt = args[0];
	int         i   = 0;
	while (i < fmt.size())
	{
		c0 = fmt.at(i);
		if (c0 == '%' && i + 1 < fmt.size())
		{
			c1 = fmt.at(i + 1);
			switch (c1)
			{
			case 'f':
				cmd += currentTab->pdf->getFileName().toStdString();
				break;
			case 'b':
				p = currentTab->pdf->getFileName().toStdString().c_str();
				if ((q = strrchr(p, '.')))
					cmd.append(p, (int)(q - p));
				else
					cmd += p;
				break;
			case 'u':
				s = mungeURL(linkTargetInfo);
				cmd += s.toStdString();
				break;
			case 'p':
				cmd += fmt::format("{}", currentTab->pdf->getMidPage());
				break;
			case 'x':
			case 'y':
			case 'X':
			case 'Y':
				if (!gotSel)
				{
					if (!currentTab->pdf->getCurrentSelection(&selPage, &selURX, &selURY, &selLRX, &selLRY))
					{
						selPage = 0;
						selURX = selURY = selLRX = selLRY = 0;
					}
					gotSel = true;
				}
				cmd += fmt::format("{0:.2g}", (c1 == 'x') ? selURX : ((c1 == 'y') ? selURY : ((c1 == 'X') ? selLRX : selLRY)));
				break;
			case 'i':
			case 'j':
			case 'k':
				if (!gotMouse)
				{
					if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::MouseButtonDblClick || event->type() == QEvent::MouseMove || event->type() == QEvent::Wheel)
					{
						currentTab->pdf->convertWindowToPDFCoords(mouseX(event), mouseY(event), &mPage, &mX, &mY);
					}
					else
					{
						pt = currentTab->pdf->mapFromGlobal(QCursor::pos());
						currentTab->pdf->convertWindowToPDFCoords(pt.x(), pt.y(), &mPage, &mX, &mY);
					}
					gotMouse = true;
				}
				if (c1 == 'i')
					cmd += fmt::format("{}", mPage);
				else
					cmd += fmt::format("{}", Round2((c1 == 'j') ? mX : mY));
				break;
			default:
				cmd += c1;
				break;
			}
			i += 2;
		}
		else
		{
			cmd += c0;
			++i;
		}
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

void XpdfViewer::cmdSaveAs(const VEC_STR& args, QInputEvent* event)
{
	QString startFile, fileName;
	QDir    startDir;

	if (!(startFile = currentTab->pdf->getFileName()).isEmpty())
		startDir = QDir(startFile);
	else
		startDir = QDir(".");
	fileName = QFileDialog::getSaveFileName(this, "Save PDF File", startDir.canonicalPath(), "PDF files (*.pdf)");
	if (fileName.isEmpty())
		return;
	currentTab->pdf->saveAs(fileName);
}

void XpdfViewer::cmdSaveImage(const VEC_STR& args, QInputEvent* event)
{
	execSaveImageDialog();
}

void XpdfViewer::cmdSaveSession(const VEC_STR& args, QInputEvent* event)
{
	app->saveSession(nullptr, true);
}

void XpdfViewer::cmdSaveTabState(const VEC_STR& args, QInputEvent* event)
{
	const auto path = globalParams->getTabStateFile();
	FILE*      f    = openFile(path.c_str(), "wb");
	if (!f)
	{
		const auto msg = fmt::format("Couldn't write the tab file '{}'", path);
		QMessageBox::warning(nullptr, "Xpdf Error", msg.c_str());
		return;
	}

	fprintf(f, "xpdf-tabstate-2\n");

	for (int i = 0; i < tabInfo->getLength(); ++i)
	{
		XpdfWidget* pdf      = ((XpdfTabInfo*)tabInfo->get(i))->pdf;
		QString     fileName = pdf->getFileName();
		if (!fileName.isEmpty())
		{
			fprintf(f, "%s\n", fileName.toUtf8().constData());
			char displayModeChar;
			switch (pdf->getDisplayMode())
			{
			case XpdfWidget::pdfDisplaySingle:
				displayModeChar = 's';
				break;
			case XpdfWidget::pdfDisplayContinuous:
				displayModeChar = 'c';
				break;
			case XpdfWidget::pdfDisplaySideBySideSingle:
				displayModeChar = 'b';
				break;
			case XpdfWidget::pdfDisplaySideBySideContinuous:
				displayModeChar = 'B';
				break;
			case XpdfWidget::pdfDisplayHorizontalContinuous:
				displayModeChar = 'h';
				break;
			default:
				displayModeChar = 'c';
				break;
			}
			fprintf(f, "%c %d %g %d %d %d\n", displayModeChar, pdf->getMidPage(), pdf->getZoom(), pdf->getRotate(), pdf->getScrollX(), pdf->getScrollY());
		}
	}

	fclose(f);
}

void XpdfViewer::cmdScrollDown(const VEC_STR& args, QInputEvent* event)
{
	int dy = scaleScroll(atoi(args[0].c_str()));
	currentTab->pdf->scrollBy(0, dy);
}

void XpdfViewer::cmdScrollDownNextPage(const VEC_STR& args, QInputEvent* event)
{
	int dy = scaleScroll(atoi(args[0].c_str()));
	currentTab->pdf->getCore()->scrollDownNextPage(dy);
}

void XpdfViewer::cmdScrollLeft(const VEC_STR& args, QInputEvent* event)
{
	int dx = scaleScroll(atoi(args[0].c_str()));
	currentTab->pdf->scrollBy(-dx, 0);
}

void XpdfViewer::cmdScrollOutlineDown(const VEC_STR& args, QInputEvent* event)
{
	QScrollBar* sb = currentTab->outlineTree->verticalScrollBar();
	sb->setValue(sb->value() + atoi(args[0].c_str()));
}

void XpdfViewer::cmdScrollOutlineUp(const VEC_STR& args, QInputEvent* event)
{
	QScrollBar* sb = currentTab->outlineTree->verticalScrollBar();
	sb->setValue(sb->value() - atoi(args[0].c_str()));
}

void XpdfViewer::cmdScrollRight(const VEC_STR& args, QInputEvent* event)
{
	int dx = scaleScroll(atoi(args[0].c_str()));
	currentTab->pdf->scrollBy(dx, 0);
}

void XpdfViewer::cmdScrollToBottomEdge(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->getCore()->scrollToBottomEdge();
}

void XpdfViewer::cmdScrollToBottomRight(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->getCore()->scrollToBottomRight();
}

void XpdfViewer::cmdScrollToLeftEdge(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->getCore()->scrollToLeftEdge();
}

void XpdfViewer::cmdScrollToRightEdge(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->getCore()->scrollToRightEdge();
}

void XpdfViewer::cmdScrollToTopEdge(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->getCore()->scrollToTopEdge();
}

void XpdfViewer::cmdScrollToTopLeft(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->getCore()->scrollToTopLeft();
}

void XpdfViewer::cmdScrollUp(const VEC_STR& args, QInputEvent* event)
{
	int dy = scaleScroll(atoi(args[0].c_str()));
	currentTab->pdf->scrollBy(0, -dy);
}

void XpdfViewer::cmdScrollUpPrevPage(const VEC_STR& args, QInputEvent* event)
{
	int dy = scaleScroll(atoi(args[0].c_str()));
	currentTab->pdf->getCore()->scrollUpPrevPage(dy);
}

void XpdfViewer::cmdSelectLine(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->getCore()->selectLine(mouseX(event), mouseY(event));
}

void XpdfViewer::cmdSelectWord(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->getCore()->selectWord(mouseX(event), mouseY(event));
}

void XpdfViewer::cmdSetSelection(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->setCurrentSelection(atoi(args[0].c_str()), atof(args[1].c_str()), atof(args[2].c_str()), atof(args[3].c_str()), atof(args[4].c_str()));
}

void XpdfViewer::cmdShowAttachmentsPane(const VEC_STR& args, QInputEvent* event)
{
	infoComboBox->setCurrentIndex(2);
}

void XpdfViewer::cmdShowDocumentInfo(const VEC_STR& args, QInputEvent* event)
{
	if (!currentTab->pdf->hasOpenDocument())
		return;
	updateDocumentInfoDialog(currentTab->pdf);
	documentInfoDialog->show();
	documentInfoDialog->raise();
}

void XpdfViewer::cmdShowKeyBindings(const VEC_STR& args, QInputEvent* event)
{
	if (!keyBindingsDialog)
		createKeyBindingsDialog();
	keyBindingsDialog->show();
	keyBindingsDialog->raise();
}

void XpdfViewer::cmdShowLayersPane(const VEC_STR& args, QInputEvent* event)
{
	infoComboBox->setCurrentIndex(1);
}

void XpdfViewer::cmdShowMenuBar(const VEC_STR& args, QInputEvent* event)
{
	mainMenu->show();
}

void XpdfViewer::cmdShowOutlinePane(const VEC_STR& args, QInputEvent* event)
{
	infoComboBox->setCurrentIndex(0);
}

void XpdfViewer::cmdShowToolbar(const VEC_STR& args, QInputEvent* event)
{
	toolBar->show();
	toggleToolbarMenuItem->setChecked(true);
}

void XpdfViewer::cmdShrinkSidebar(const VEC_STR& args, QInputEvent* event)
{
	QList<int> sizes = sidebarSplitter->sizes();
	if (sizes[0] == 0)
		return;
	int nPixels = atoi(args[0].c_str());
	if (sizes[0] - nPixels < sidebarSplitter->widget(0)->minimumSizeHint().width())
	{
		cmdCloseSidebar(args, event);
		return;
	}
	sizes[0] -= nPixels;
	sizes[1] += nPixels;
	sidebarSplitter->setSizes(sizes);
}

void XpdfViewer::cmdSideBySideContinuousMode(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->setDisplayMode(XpdfWidget::pdfDisplaySideBySideContinuous);
}

void XpdfViewer::cmdSideBySideSingleMode(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->setDisplayMode(XpdfWidget::pdfDisplaySideBySideSingle);
}

void XpdfViewer::cmdSinglePageMode(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->setDisplayMode(XpdfWidget::pdfDisplaySingle);
	updateModeInfo();
}

void XpdfViewer::cmdStartExtendedSelection(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->getCore()->startSelection(mouseX(event), mouseY(event), true);
}

void XpdfViewer::cmdStartPan(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->getCore()->startPan(mouseX(event), mouseY(event));
}

void XpdfViewer::cmdStartSelection(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->getCore()->startSelection(mouseX(event), mouseY(event), false);
}

void XpdfViewer::cmdToggleContinuousMode(const VEC_STR& args, QInputEvent* event)
{
	XpdfWidget::DisplayMode mode = currentTab->pdf->getDisplayMode();
	if (mode == XpdfWidget::pdfDisplaySingle)
		currentTab->pdf->setDisplayMode(XpdfWidget::pdfDisplayContinuous);
	else
		currentTab->pdf->setDisplayMode(XpdfWidget::pdfDisplaySingle);
	updateModeInfo();
}

void XpdfViewer::cmdToggleFullScreenMode(const VEC_STR& args, QInputEvent* event)
{
	if (windowState() & Qt::WindowFullScreen)
		exitFullScreenMode();
	else
		enterFullScreenMode();
}

void XpdfViewer::cmdToggleMenuBar(const VEC_STR& args, QInputEvent* event)
{
	if (mainMenu->isVisible())
		cmdHideMenuBar(args, event);
	else
		cmdShowMenuBar(args, event);
}

void XpdfViewer::cmdToggleSelectMode(const VEC_STR& args, QInputEvent* event)
{
	if (currentTab->pdf->isBlockSelectMode())
		currentTab->pdf->setLinearSelectMode();
	else
		currentTab->pdf->setBlockSelectMode();
	updateSelectModeInfo();
}

void XpdfViewer::cmdToggleSidebar(const VEC_STR& args, QInputEvent* event)
{
	QList<int> sizes = sidebarSplitter->sizes();
	if (sizes[0] == 0)
		cmdOpenSidebar(args, event);
	else
		cmdCloseSidebar(args, event);
}

void XpdfViewer::cmdToggleSidebarMoveResizeWin(const VEC_STR& args, QInputEvent* event)
{
	QList<int> sizes = sidebarSplitter->sizes();
	if (sizes[0] == 0)
		cmdOpenSidebarMoveResizeWin(args, event);
	else
		cmdCloseSidebarMoveResizeWin(args, event);
}

void XpdfViewer::cmdToggleSidebarResizeWin(const VEC_STR& args, QInputEvent* event)
{
	QList<int> sizes = sidebarSplitter->sizes();
	if (sizes[0] == 0)
		cmdOpenSidebarResizeWin(args, event);
	else
		cmdCloseSidebarResizeWin(args, event);
}

void XpdfViewer::cmdToggleToolbar(const VEC_STR& args, QInputEvent* event)
{
	if (toolBar->isVisible())
		cmdHideToolbar(args, event);
	else
		cmdShowToolbar(args, event);
}

void XpdfViewer::cmdViewPageLabels(const VEC_STR& args, QInputEvent* event)
{
	viewPageLabelsMenuItem->setChecked(true);
	updatePageNumberOrLabel(currentTab->pdf->getMidPage());
}

void XpdfViewer::cmdViewPageNumbers(const VEC_STR& args, QInputEvent* event)
{
	viewPageLabelsMenuItem->setChecked(false);
	updatePageNumberOrLabel(currentTab->pdf->getMidPage());
}

void XpdfViewer::cmdWindowMode(const VEC_STR& args, QInputEvent* event)
{
	if (windowState() & Qt::WindowFullScreen)
		exitFullScreenMode();
}

void XpdfViewer::cmdZoomFitPage(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->zoomCentered(XpdfWidget::zoomToPage);
	updateZoomInfo();
}

void XpdfViewer::cmdZoomFitWidth(const VEC_STR& args, QInputEvent* event)
{
	currentTab->pdf->zoomCentered(XpdfWidget::zoomToWidth);
	updateZoomInfo();
}

void XpdfViewer::cmdZoomIn(const VEC_STR& args, QInputEvent* event)
{
	const double zsf = app->getZoomScaleFactor();
	const double z   = currentTab->pdf->getZoomPercent(currentTab->pdf->getMidPage()) / zsf;
	for (int i = 0; i < app->getNZoomValues(); ++i)
	{
		// the 1.0001 factor is to allow for floating point jitter when
		// multiplying and dividing by zoomScaleFactor
		if (app->getZoomValue(i) > 1.0001 * z)
		{
			currentTab->pdf->zoomCentered(app->getZoomValue(i) * zsf);
			zoomComboBox->setCurrentIndex(i);
			updateZoomInfo();
			break;
		}
	}
}

void XpdfViewer::cmdZoomOut(const VEC_STR& args, QInputEvent* event)
{
	const double zsf = app->getZoomScaleFactor();
	const double z   = currentTab->pdf->getZoomPercent(currentTab->pdf->getMidPage()) / zsf;
	for (int i = app->getNZoomValues() - 1; i >= 0; --i)
	{
		// the 0.9999 factor is to allow for floating point jitter when
		// multiplying and dividing by zoomScaleFactor
		if (app->getZoomValue(i) < 0.9999 * z)
		{
			currentTab->pdf->zoomCentered(app->getZoomValue(i) * zsf);
			zoomComboBox->setCurrentIndex(i);
			updateZoomInfo();
			break;
		}
	}
}

void XpdfViewer::cmdZoomPercent(const VEC_STR& args, QInputEvent* event)
{
	QString zoomStr;

	int z = (int)floor(atof(args[0].c_str()) + 0.5);
	if (z > maxZoom) z = maxZoom;
	currentTab->pdf->zoomCentered(z * app->getZoomScaleFactor());
	updateZoomInfo();
}

void XpdfViewer::cmdZoomToSelection(const VEC_STR& args, QInputEvent* event)
{
	double x0, y0, x1, y1, rx, ry, z, w, h, t;
	int    pg, xx0, yy0, xx1, yy1;

	if (currentTab->pdf->getCurrentSelection(&pg, &x0, &y0, &x1, &y1))
	{
		z = currentTab->pdf->getZoomPercent(pg) / app->getZoomScaleFactor();
		currentTab->pdf->getCore()->cvtUserToDev(pg, x0, y0, &xx0, &yy0);
		currentTab->pdf->getCore()->cvtUserToDev(pg, x1, y1, &xx1, &yy1);
		rx = (double)currentTab->pdf->getCore()->getWindowWidth() / (double)abs(xx1 - xx0);
		ry = (double)currentTab->pdf->getCore()->getWindowHeight() / (double)abs(yy1 - yy0);
		z *= rx < ry ? rx : ry;
		if (z > maxZoom)
		{
			w  = 0.5 * (z / maxZoom) * (x1 - x0);
			h  = 0.5 * (z / maxZoom) * (y1 - y0);
			t  = 0.5 * (x0 + x1);
			x0 = t - w;
			x1 = t + w;
			t  = 0.5 * (y0 + y1);
			y0 = t - h;
			y1 = t + h;
		}
		currentTab->pdf->zoomToRect(pg, x0, y0, x1, y1);
		updateZoomInfo();
	}
}

// Check the find settings menu items and the find text to determine
// the effective case sensitivity setting.
int XpdfViewer::getFindCaseFlag()
{
	// could use QString::isLower(), but it's only available in Qt 5.12+
	if (findCaseSensitiveAction->isChecked())
	{
		return XpdfWidget::findCaseSensitive;
	}
	else if (findSmartCaseAction->isChecked())
	{
		QString s = findEdit->text();
		for (int i = 0; i < s.length(); ++i)
		{
			QChar c = s[i];
			if (c != c.toLower())
				return XpdfWidget::findCaseSensitive;
		}
		return 0;
	}
	else
	{
		return 0;
	}
}

int XpdfViewer::scaleScroll(int delta)
{
	int scaledDelta;

	scaledDelta = (delta * currentTab->pdf->getCore()->getDisplayDpi()) / 96;
	if (delta >= 0)
	{
		if (scaledDelta < delta)
			scaledDelta = delta;
	}
	else
	{
		if (scaledDelta > delta)
			scaledDelta = delta;
	}
	return scaledDelta;
}

void XpdfViewer::followLink(QInputEvent* event, bool onlyIfNoSel, bool newTab, bool newWindow)
{
	int     pg, targetPage;
	double  x, y;
	QString targetFileName, targetDest;

	if (onlyIfNoSel && currentTab->pdf->hasSelection())
		return;
	if (!currentTab->pdf->convertWindowToPDFCoords(mouseX(event), mouseY(event), &pg, &x, &y))
		return;
	if ((newTab || newWindow) && currentTab->pdf->getLinkTarget(pg, x, y, targetFileName, targetPage, targetDest))
		if (newTab)
			openInNewTab(targetFileName, targetPage, targetDest, 0, "", true);
		else
			app->openInNewWindow(targetFileName, targetPage, targetDest, 0, "");
	else if (!currentTab->pdf->gotoLinkAt(pg, x, y))
		QMessageBox::warning(nullptr, "Xpdf Error", "Couldn't follow link: '" + currentTab->pdf->getLinkInfo(pg, x, y) + "'");
}

//------------------------------------------------------------------------
// GUI events
//------------------------------------------------------------------------

void XpdfViewer::pdfResized()
{
	updateZoomInfo();
}

void XpdfViewer::pdfPaintDone(bool finished)
{
	if (finished)
		statusIndicatorStop();
	else
		statusIndicatorStart();
}

void XpdfViewer::preLoad()
{
	((OutlineModel*)currentTab->outlineTree->model())->beginOpenNewDoc();
	((LayerModel*)currentTab->layerTree->model())->beginOpenNewDoc();
}

void XpdfViewer::postLoad()
{
	((LayerModel*)currentTab->layerTree->model())->endOpenNewDoc();
	((OutlineModel*)currentTab->outlineTree->model())->endOpenNewDoc();
	setOutlineOpenItems(QModelIndex());
	fillAttachmentList();
	updateDocInfo();
}

void XpdfViewer::keyPress(QKeyEvent* e)
{
	int       keyCode = 0;
	const int mods    = getModifiers(e->modifiers());
	const int qtKey   = e->key();

	if (qtKey >= 0x20 && qtKey <= 0xfe)
	{
		keyCode = qtKey;
		if (keyCode >= 'A' && keyCode <= 'Z' && !(mods & xpdfKeyModShift))
			keyCode += 'a' - 'A';
	}
	else if (qtKey == Qt::Key_Tab || qtKey == Qt::Key_Backtab)
	{
		// backtab = shift-tab
		keyCode = xpdfKeyCodeTab;
	}
	else if (qtKey == Qt::Key_Return)
	{
		keyCode = xpdfKeyCodeReturn;
	}
	else if (qtKey == Qt::Key_Enter)
	{
		keyCode = xpdfKeyCodeEnter;
	}
	else if (qtKey == Qt::Key_Backspace)
	{
		keyCode = xpdfKeyCodeBackspace;
	}
	else if (qtKey == Qt::Key_Insert)
	{
		keyCode = xpdfKeyCodeInsert;
	}
	else if (qtKey == Qt::Key_Delete)
	{
		keyCode = xpdfKeyCodeDelete;
	}
	else if (qtKey == Qt::Key_Home)
	{
		keyCode = xpdfKeyCodeHome;
	}
	else if (qtKey == Qt::Key_End)
	{
		keyCode = xpdfKeyCodeEnd;
	}
	else if (qtKey == Qt::Key_PageUp)
	{
		keyCode = xpdfKeyCodePgUp;
	}
	else if (qtKey == Qt::Key_PageDown)
	{
		keyCode = xpdfKeyCodePgDn;
	}
	else if (qtKey == Qt::Key_Left)
	{
		keyCode = xpdfKeyCodeLeft;
	}
	else if (qtKey == Qt::Key_Right)
	{
		keyCode = xpdfKeyCodeRight;
	}
	else if (qtKey == Qt::Key_Up)
	{
		keyCode = xpdfKeyCodeUp;
	}
	else if (qtKey == Qt::Key_Down)
	{
		keyCode = xpdfKeyCodeDown;
	}
	else if (qtKey == Qt::Key_Escape)
	{
		keyCode = xpdfKeyCodeEsc;
	}
	else if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F35)
	{
		keyCode = xpdfKeyCodeF1 + (qtKey - Qt::Key_F1);
	}
	else
	{
		return;
	}

	for (const auto& cmd : globalParams->getKeyBinding(keyCode, mods, getContext(e->modifiers())))
		execCmd(cmd.c_str(), e);
}

void XpdfViewer::mousePress(QMouseEvent* e)
{
	if (const int btn = getMouseButton(e->button()))
	{
		const int keyCode = xpdfKeyCodeMousePress1 + (btn - 1);
		for (const auto& cmd : globalParams->getKeyBinding(keyCode, getModifiers(e->modifiers()), getContext(e->modifiers())))
			execCmd(cmd.c_str(), e);
	}
}

void XpdfViewer::mouseRelease(QMouseEvent* e)
{
	if (const int btn = getMouseButton(e->button()))
	{
		const int keyCode = xpdfKeyCodeMouseRelease1 + (btn - 1);
		for (const auto& cmd : globalParams->getKeyBinding(keyCode, getModifiers(e->modifiers()), getContext(e->modifiers())))
			execCmd(cmd.c_str(), e);
	}
}

void XpdfViewer::mouseClick(QMouseEvent* e)
{
	if (const int btn = getMouseButton(e->button()))
	{
		const int keyCode = xpdfKeyCodeMouseClick1 + (btn - 1);
		for (const auto& cmd : globalParams->getKeyBinding(keyCode, getModifiers(e->modifiers()), getContext(e->modifiers())))
			execCmd(cmd.c_str(), e);
	}
}

void XpdfViewer::mouseDoubleClick(QMouseEvent* e)
{
	if (const int btn = getMouseButton(e->button()))
	{
		const int keyCode = xpdfKeyCodeMouseDoubleClick1 + (btn - 1);
		for (const auto& cmd : globalParams->getKeyBinding(keyCode, getModifiers(e->modifiers()), getContext(e->modifiers())))
			execCmd(cmd.c_str(), e);
	}
}

void XpdfViewer::mouseTripleClick(QMouseEvent* e)
{
	if (const int btn = getMouseButton(e->button()))
	{
		const int keyCode = xpdfKeyCodeMouseTripleClick1 + (btn - 1);
		for (const auto& cmd : globalParams->getKeyBinding(keyCode, getModifiers(e->modifiers()), getContext(e->modifiers())))
			execCmd(cmd.c_str(), e);
	}
}

void XpdfViewer::mouseWheel(QWheelEvent* e)
{
	int keyCode;

	// for historical reasons xpdf uses X11 button numbering for mouse
	// wheel events
	QPoint delta = e->angleDelta();
	if (delta.y() > 0)
		keyCode = xpdfKeyCodeMousePress4;
	else if (delta.y() < 0)
		keyCode = xpdfKeyCodeMousePress5;
	else if (delta.x() > 0)
		keyCode = xpdfKeyCodeMousePress6;
	else if (delta.x() < 0)
		keyCode = xpdfKeyCodeMousePress7;
	else
		return;

	for (const auto& cmd : globalParams->getKeyBinding(keyCode, getModifiers(e->modifiers()), getContext(e->modifiers())))
		execCmd(cmd.c_str(), e);
}

void XpdfViewer::mouseMove(QMouseEvent* e)
{
	int     pg, xx;
	double  x, y;
	QString info;

	currentTab->pdf->convertWindowToPDFCoords(mouseX(e), mouseY(e), &pg, &x, &y);
	info = currentTab->pdf->getLinkInfo(pg, x, y);
	if (info == linkTargetInfo)
	{
		// QtPDFCore sets the cursor in linear text selection mode
		if (!linkTargetInfo.isEmpty())
			currentTab->pdf->setCursor(Qt::PointingHandCursor);
		return;
	}
	linkTargetInfo = info;
	if (linkTargetInfo.isEmpty())
	{
		currentTab->pdf->unsetCursor();
		linkTargetBar->hide();
	}
	else
	{
		currentTab->pdf->setCursor(Qt::PointingHandCursor);
		linkTargetBar->setText(linkTargetInfo);
		linkTargetBar->resize(linkTargetBar->sizeHint());
		if (mouseX(e) > viewerStack->width() / 2)
		{
			xx = viewerStack->x();
		}
		else
		{
			xx = width() - linkTargetBar->width();
			if (xx < viewerStack->x())
				xx = viewerStack->x();
		}
		linkTargetBar->move(xx, height() - linkTargetBar->height());
		linkTargetBar->show();
	}
}

int XpdfViewer::getModifiers(Qt::KeyboardModifiers qtMods)
{
	int mods = 0;
	if (qtMods & Qt::ShiftModifier) mods |= xpdfKeyModShift;
	if (qtMods & Qt::ControlModifier) mods |= xpdfKeyModCtrl;
	if (qtMods & Qt::AltModifier) mods |= xpdfKeyModAlt;
	return mods;
}

int XpdfViewer::getContext(Qt::KeyboardModifiers qtMods)
{
	XpdfWidget::DisplayMode mode;
	bool                    contin;
	int                     context;

	mode    = currentTab->pdf->getDisplayMode();
	contin  = mode == XpdfWidget::pdfDisplayContinuous || mode == XpdfWidget::pdfDisplaySideBySideContinuous || mode == XpdfWidget::pdfDisplayHorizontalContinuous;
	context = ((windowState() & Qt::WindowFullScreen) ? xpdfKeyContextFullScreen
	                                                  : xpdfKeyContextWindow)
	    | (contin ? xpdfKeyContextContinuous
	              : xpdfKeyContextSinglePage)
	    | (currentTab->pdf->mouseOverLink() ? xpdfKeyContextOverLink
	                                        : xpdfKeyContextOffLink)
	    | xpdfKeyContextScrLockOff;
	return context;
}

int XpdfViewer::getMouseButton(Qt::MouseButton qtBtn)
{
	if (qtBtn & Qt::LeftButton) return 1;
	if (qtBtn & Qt::MiddleButton) return 2;
	if (qtBtn & Qt::RightButton) return 3;
	return 0;
}

// Grab any keyboard events that filter down to the window, and feed
// them to the main key processing function.
void XpdfViewer::keyPressEvent(QKeyEvent* e)
{
	keyPress(e);
}

void XpdfViewer::dragEnterEvent(QDragEnterEvent* e)
{
	if (e->mimeData()->hasUrls() && e->mimeData()->urls().front().isLocalFile())
		e->acceptProposedAction();
}

void XpdfViewer::dropEvent(QDropEvent* e)
{
	if (e->mimeData()->hasUrls())
	{
		QUrl url = e->mimeData()->urls().front();
		if (url.isLocalFile())
			openInNewTab(url.toLocalFile(), -1, "", 0, "", true);
	}
}

bool XpdfViewer::eventFilter(QObject* watched, QEvent* event)
{
	// if the user clicks in the find edit box, clear the find error
	// indicator (if any)
	if (watched == findEdit && event->type() == QEvent::MouseButtonPress)
		clearFindError();
	return false;
}

void XpdfViewer::pageChange(int pg)
{
	updatePageNumberOrLabel(pg);
	updateZoomInfo();
	updateOutline(pg);
}

void XpdfViewer::sidebarSplitterMoved(int pos, int index)
{
	toggleSidebarMenuItem->setChecked(pos > 0);
}

#if XPDFWIDGET_PRINTING
void XpdfViewer::printStatus(int nextPage, int firstPage, int lastPage)
{
	if (!printStatusDialog)
	{
		printStatusDialog = new QProgressDialog("Printing...", "Cancel", firstPage, lastPage + 1, this);
		printStatusDialog->setWindowModality(Qt::WindowModal);
		printStatusDialog->setMinimumDuration(0);
		printStatusDialog->setAutoClose(false);
		printStatusDialog->setAutoReset(false);
		connect(printStatusDialog, SIGNAL(canceled()), this, SLOT(cancelPrint()));
		printStatusDialog->move(
		    pos().x() + (width() - printStatusDialog->width()) / 2,
		    pos().y() + (height() - printStatusDialog->height()) / 2);
		printStatusDialog->show();
	}
	printStatusDialog->setValue(nextPage);
	if (nextPage > lastPage)
	{
		printStatusDialog->cancel();
		delete printStatusDialog;
		printStatusDialog = nullptr;
	}
	QApplication::processEvents();
}

void XpdfViewer::cancelPrint()
{
	currentTab->pdf->cancelPrint();
}
#endif

//------------------------------------------------------------------------
// menu/toolbar actions
//------------------------------------------------------------------------

void XpdfViewer::openMenuAction()
{
	execCmd("open", nullptr);
}

void XpdfViewer::openInNewWinMenuAction()
{
	execCmd("openIn(win)", nullptr);
}

void XpdfViewer::reloadMenuAction()
{
	execCmd("reload", nullptr);
}

void XpdfViewer::saveAsMenuAction()
{
	execCmd("saveAs", nullptr);
}

void XpdfViewer::loadSessionMenuAction()
{
	execCmd("loadSession", nullptr);
}

void XpdfViewer::saveImageMenuAction()
{
	execCmd("saveImage", nullptr);
}

#if XPDFWIDGET_PRINTING
void XpdfViewer::printMenuAction()
{
	execCmd("print", nullptr);
}
#endif

void XpdfViewer::quitMenuAction()
{
	execCmd("quit", nullptr);
}

void XpdfViewer::copyMenuAction()
{
	execCmd("copy", nullptr);
}

void XpdfViewer::singlePageModeMenuAction()
{
	execCmd("singlePageMode", nullptr);
}

void XpdfViewer::continuousModeMenuAction()
{
	execCmd("continuousMode", nullptr);
}

void XpdfViewer::sideBySideSingleModeMenuAction()
{
	execCmd("sideBySideSingleMode", nullptr);
}

void XpdfViewer::sideBySideContinuousModeMenuAction()
{
	execCmd("sideBySideContinuousMode", nullptr);
}

void XpdfViewer::horizontalContinuousModeMenuAction()
{
	execCmd("horizontalContinuousMode", nullptr);
}

void XpdfViewer::fullScreenMenuAction(bool checked)
{
	execCmd(checked ? "fullScreenMode" : "windowMode", nullptr);
}

void XpdfViewer::rotateClockwiseMenuAction()
{
	execCmd("rotateCW", nullptr);
}

void XpdfViewer::rotateCounterclockwiseMenuAction()
{
	execCmd("rotateCCW", nullptr);
}

void XpdfViewer::zoomToSelectionMenuAction()
{
	execCmd("zoomToSelection", nullptr);
}

void XpdfViewer::toggleToolbarMenuAction(bool checked)
{
	execCmd(checked ? "showToolbar" : "hideToolbar", nullptr);
}

void XpdfViewer::toggleSidebarMenuAction(bool checked)
{
	execCmd(checked ? "openSidebar" : "closeSidebar", nullptr);
}

void XpdfViewer::viewPageLabelsMenuAction(bool checked)
{
	execCmd(checked ? "viewPageLabels" : "viewPageNumbers", nullptr);
}

void XpdfViewer::documentInfoMenuAction()
{
	execCmd("showDocumentInfo", nullptr);
}

void XpdfViewer::newTabMenuAction()
{
	execCmd("newTab", nullptr);
}

void XpdfViewer::newWindowMenuAction()
{
	execCmd("newWindow", nullptr);
}

void XpdfViewer::closeTabMenuAction()
{
	execCmd("closeTabOrQuit", nullptr);
}

void XpdfViewer::closeWindowMenuAction()
{
	execCmd("closeWindowOrQuit", nullptr);
}

void XpdfViewer::openErrorWindowMenuAction()
{
	execCmd("openErrorWindow", nullptr);
}

void XpdfViewer::helpMenuAction()
{
	execCmd("help", nullptr);
}

void XpdfViewer::keyBindingsMenuAction()
{
	execCmd("showKeyBindings", nullptr);
}

void XpdfViewer::aboutMenuAction()
{
	execCmd("about", nullptr);
}

void XpdfViewer::popupMenuAction(int idx)
{
	const auto& cmd = globalParams->getPopupMenuCmd(idx);
	for (const auto& cmdStr : cmd->cmds)
		execCmd(cmdStr.c_str(), nullptr);
}

void XpdfViewer::toggleSidebarButtonPressed()
{
	execCmd("toggleSidebar", nullptr);
}

void XpdfViewer::pageNumberChanged()
{
	int pg;
	if (viewPageLabelsMenuItem->isChecked() && currentTab->pdf->hasPageLabels())
	{
		pg = currentTab->pdf->getPageNumFromPageLabel(pageNumber->text());
		if (pg <= 0)
			return;
	}
	else
	{
		pg = pageNumber->text().toInt();
	}
	const auto cmd = fmt::format("gotoPage({})", pg);
	execCmd(cmd.c_str(), nullptr);
	// after moving to a new page, focus goes to the XpdfWidget
	currentTab->pdf->setFocus(Qt::OtherFocusReason);
}

void XpdfViewer::backButtonPressed()
{
	execCmd("goBackward", nullptr);
}

void XpdfViewer::forwardButtonPressed()
{
	execCmd("goForward", nullptr);
}

void XpdfViewer::zoomOutButtonPressed()
{
	execCmd("zoomOut", nullptr);
}

void XpdfViewer::zoomInButtonPressed()
{
	execCmd("zoomIn", nullptr);
}

void XpdfViewer::zoomIndexChanged(int idx)
{
	QString zoomText = zoomComboBox->itemText(idx);
	QString z;
	if (zoomText.endsWith("%"))
		z = zoomText.left(zoomText.size() - 1);
	else
		z = zoomText;
	const auto cmd = fmt::format("zoomPercent({})", z.toLatin1().constData());
	execCmd(cmd.c_str(), nullptr);
}

void XpdfViewer::zoomEditingFinished()
{
	QString z = zoomComboBox->currentText();
	if (z.endsWith("%"))
		z = z.left(z.size() - 1);

	const auto cmd = fmt::format("zoomPercent({})", z.toLatin1().constData());
	execCmd(cmd.c_str(), nullptr);
}

void XpdfViewer::fitWidthButtonPressed()
{
	execCmd("zoomFitWidth", nullptr);
}

void XpdfViewer::fitPageButtonPressed()
{
	execCmd("zoomFitPage", nullptr);
}

void XpdfViewer::selectModeButtonPressed()
{
	execCmd("toggleSelectMode", nullptr);
}

void XpdfViewer::statusIndicatorPressed()
{
	execCmd("openErrorWindow", nullptr);
}

void XpdfViewer::findTextChanged()
{
	execCmd("findFirst", nullptr);
}

void XpdfViewer::findNextButtonPressed()
{
	execCmd("findNext", nullptr);
}

void XpdfViewer::findPrevButtonPressed()
{
	execCmd("findPrevious", nullptr);
}

void XpdfViewer::newTabButtonPressed()
{
	execCmd("newTab", nullptr);
}

void XpdfViewer::switchTab(QListWidgetItem* current, QListWidgetItem* previous)
{
	for (int i = 0; i < tabInfo->getLength(); ++i)
	{
		const auto tab = (XpdfTabInfo*)tabInfo->get(i);
		if (tab->listItem == current)
		{
			gotoTab(i);
			return;
		}
	}
}

void XpdfViewer::tabsReordered(const QModelIndex& srcParent, int srcStart, int srcEnd, const QModelIndex& destParent, int destRow)
{
	// these conditions should always be true, but check just in case
	if (srcStart == srcEnd && srcStart >= 0 && srcStart < tabInfo->getLength() && destRow >= 0 && destRow <= tabInfo->getLength())
	{
		XpdfTabInfo* tab = (XpdfTabInfo*)tabInfo->del(srcStart);
		int          i   = destRow;
		if (i >= srcStart)
			--i;
		tabInfo->insert(i, tab);
	}
}

void XpdfViewer::infoComboBoxChanged(int idx)
{
	updateInfoPane();
}

void XpdfViewer::outlineItemClicked(const QModelIndex& idx)
{
	currentTab->pdf->gotoOutlineTarget((XpdfOutlineHandle)idx.internalPointer());
	updateModeInfo();
}

void XpdfViewer::layerItemClicked(const QModelIndex& idx)
{
	if (idx.data(Qt::CheckStateRole) == Qt::Checked)
		currentTab->layerTree->model()->setData(idx, Qt::Unchecked, Qt::CheckStateRole);
	else
		currentTab->layerTree->model()->setData(idx, Qt::Checked, Qt::CheckStateRole);
}

void XpdfViewer::attachmentSaveClicked(int idx)
{
	QString fileName;

	fileName = QFileDialog::getSaveFileName(this, "Save Attachment");
	if (fileName.isEmpty())
		return;
	currentTab->pdf->saveEmbeddedFile(idx, fileName);
}

//------------------------------------------------------------------------
// GUI setup
//------------------------------------------------------------------------

void XpdfViewer::createWindow()
{
	errorWindow = new XpdfErrorWindow(this, app->getErrorEventType());

	setWindowIcon(QIcon(":/xpdf-icon"));

	setAcceptDrops(true);

	createMainMenu();

	createXpdfPopupMenu();

	createToolBar();
	addToolBar(toolBar);
	setUnifiedTitleAndToolBarOnMac(true);
	if (globalParams->getInitialToolbarState())
	{
		toggleToolbarMenuItem->setChecked(true);
	}
	else
	{
		toolBar->hide();
		toggleToolbarMenuItem->setChecked(false);
	}

	sidebarSplitter = new QSplitter(Qt::Horizontal);
	setCentralWidget(sidebarSplitter);
	connect(sidebarSplitter, SIGNAL(splitterMoved(int, int)), this, SLOT(sidebarSplitterMoved(int, int)));

	QSplitter* vSplitter = new QSplitter(Qt::Vertical);
	sidebarSplitter->addWidget(vSplitter);

	QWidget* tabPane = createTabPane();
	vSplitter->addWidget(tabPane);

	QWidget* infoPane = createInfoPane();
	vSplitter->addWidget(infoPane);

	QList<int> vSplitterSizes;
	vSplitterSizes.append(200);
	vSplitterSizes.append(600);
	vSplitter->setSizes(vSplitterSizes);

	viewerStack = new QStackedWidget();
	sidebarSplitter->addWidget(viewerStack);

	initialSidebarWidth             = 0;
	QList<int> sidebarSplitterSizes = sidebarSplitter->sizes();
	if (globalParams->getInitialSidebarState())
	{
		toggleSidebarMenuItem->setChecked(true);
		initialSidebarWidth = globalParams->getInitialSidebarWidth();
	}
	else
	{
		sidebarSplitterSizes[0] = 0;
		sidebarSplitterSizes[1] = 1;
		sidebarSplitter->setSizes(sidebarSplitterSizes);
		toggleSidebarMenuItem->setChecked(false);
	}
	// note: this is just an arbitrary initial value for sidebarWidth;
	// it will be updated by open/close/toggleSidebar
	sidebarWidth = 200;

	linkTargetBar = new QLabel(this);
	linkTargetBar->setStyleSheet("padding:2px; background:#00ffff;");
	linkTargetBar->setAttribute(Qt::WA_TransparentForMouseEvents, true);

	findErrorTimer = new QTimer(this);
	findErrorTimer->setSingleShot(true);
	connect(findErrorTimer, SIGNAL(timeout()), this, SLOT(clearFindError()));

	documentInfoDialog = nullptr;
	keyBindingsDialog  = nullptr;
	aboutDialog        = nullptr;
#if XPDFWIDGET_PRINTING
	printStatusDialog = nullptr;
#endif

	scaleFactor = 1;

	tabInfo = new GList();
	addTab();
	updateModeInfo();
	updateDocInfo();
}

void XpdfViewer::createToolBar()
{
	QString zoomVal;

	toolBar = new QToolBar();
	toolBar->setFloatable(false);
	toolBar->setMovable(false);

	//--- toolbar icon size
	pageNumber      = new QLineEdit();
	toolBarFontSize = pageNumber->sizeHint().height();
	toolBar->setIconSize(QSize(toolBarFontSize - 2, toolBarFontSize - 2));
	//~ not sure why the magic "+6" is needed

	//--- toggle sidebar button
	addToolBarButton(QIcon(":/toggleSidebar-button"), SLOT(toggleSidebarButtonPressed()), "show/hide sidebar");

	//--- status indicator
	QToolButton* indicatorBtn = addToolBarButton(QIcon(":/indicator-icon0"), SLOT(statusIndicatorPressed()), "click to open error window");
	indicatorIcons.append(QIcon(":/indicator-icon0"));
	indicatorIcons.append(QIcon(":/indicator-icon1"));
	indicatorIcons.append(QIcon(":/indicator-icon2"));
	indicatorIcons.append(QIcon(":/indicator-icon3"));
	indicatorIcons.append(QIcon(":/indicator-icon4"));
	indicatorIcons.append(QIcon(":/indicator-icon5"));
	indicatorIcons.append(QIcon(":/indicator-icon6"));
	indicatorIcons.append(QIcon(":/indicator-icon7"));
	indicatorErrIcons.append(QIcon(":/indicator-icon-err0"));
	indicatorErrIcons.append(QIcon(":/indicator-icon-err1"));
	indicatorErrIcons.append(QIcon(":/indicator-icon-err2"));
	indicatorErrIcons.append(QIcon(":/indicator-icon-err3"));
	indicatorErrIcons.append(QIcon(":/indicator-icon-err4"));
	indicatorErrIcons.append(QIcon(":/indicator-icon-err5"));
	indicatorErrIcons.append(QIcon(":/indicator-icon-err6"));
	indicatorErrIcons.append(QIcon(":/indicator-icon-err7"));
	indicatorAnimation = new PropertyListAnimation(indicatorBtn, "icon", indicatorIcons);
	indicatorAnimation->setDuration(1000);
	indicatorAnimation->setLoopCount(-1);
	indicatorAnimation->setStartValue(indicatorIcons[0]);
	indicatorAnimation->setEndValue(indicatorIcons[7]);
	indicatorAnimation->start();
	indicatorAnimation->pause();

	//--- selection mode toggle
	selectModeBtn = addToolBarButton(QIcon(":/selectModeLinear-button"), SLOT(selectModeButtonPressed()), "toggle selection mode");

	addToolBarSeparator();

	//--- page number and page count
	// note: the pageNumber widget was created earlier because we need
	// to look at its font size
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
	pageNumber->setFixedWidth(
	    pageNumber->fontMetrics().horizontalAdvance("00000") + 6);
#else
	pageNumber->setFixedWidth(pageNumber->fontMetrics().width("00000") + 6);
#endif
	pageNumber->setToolTip("current page number");
	toolBar->addWidget(pageNumber);
	addToolBarSpacing(2);
	toolBar->addWidget(new QLabel("/"));
	addToolBarSpacing(2);
	connect(pageNumber, SIGNAL(returnPressed()), this, SLOT(pageNumberChanged()));
	pageCount = new QLabel("");
	pageCount->setToolTip("page count");
	toolBar->addWidget(pageCount);
	addToolBarSpacing(4);

	//--- back / forward buttons
	addToolBarButton(QIcon(":/back-button"), SLOT(backButtonPressed()), "back to previous view");
	addToolBarButton(QIcon(":/forward-button"), SLOT(forwardButtonPressed()), "forward to next view");

	addToolBarSeparator();

	//--- zoom controls
	addToolBarButton(QIcon(":/zoomOut-button"), SLOT(zoomOutButtonPressed()), "zoom out");
	addToolBarButton(QIcon(":/zoomIn-button"), SLOT(zoomInButtonPressed()), "zoom in");
	addToolBarSpacing(4);
	zoomComboBox = new QComboBox();
	zoomComboBox->setToolTip("change zoom level");
	for (int i = 0; i < app->getNZoomValues(); ++i)
	{
		zoomVal.setNum(app->getZoomValue(i));
		zoomVal.append('%');
		zoomComboBox->addItem(zoomVal);
	}
	zoomComboBox->setEditable(true);
	zoomComboBox->setInsertPolicy(QComboBox::NoInsert);
	zoomComboBox->setValidator(new ZoomValidator(this));
	connect(zoomComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(zoomIndexChanged(int)));
	// this could use the editingFinished signal, but that's emitted
	// every time the popup is opened and closed, which causes the zoom
	// level to be reset
	connect(zoomComboBox->lineEdit(), SIGNAL(returnPressed()), this, SLOT(zoomEditingFinished()));
	toolBar->addWidget(zoomComboBox);
	addToolBarSpacing(4);
	fitWidthBtn = addToolBarButton(QIcon(":/fitWidth-button"), SLOT(fitWidthButtonPressed()), "fit page width to window");
	fitPageBtn  = addToolBarButton(QIcon(":/fitPage-button"), SLOT(fitPageButtonPressed()), "fit page to window");

	addToolBarSeparator();

	//--- find controls
	addToolBarStretch();
	findEdit = new QLineEdit();
	findEdit->setPlaceholderText("find");
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
	findEdit->setFixedWidth(20 * findEdit->fontMetrics().horizontalAdvance("0"));
#else
	findEdit->setFixedWidth(20 * findEdit->fontMetrics().width("0"));
#endif
	findEdit->installEventFilter(this);
	toolBar->addWidget(findEdit);
	connect(findEdit, SIGNAL(returnPressed()), this, SLOT(findTextChanged()));
	connect(findEdit, SIGNAL(cursorPositionChanged(int, int)), this, SLOT(clearFindError()));
	connect(findEdit, SIGNAL(selectionChanged()), this, SLOT(clearFindError()));
	connect(findEdit, SIGNAL(textChanged(const QString&)), this, SLOT(clearFindError()));
	addToolBarButton(QIcon(":/findNext-button"), SLOT(findNextButtonPressed()), "find next occurrence");
	addToolBarButton(QIcon(":/findPrevious-button"), SLOT(findPrevButtonPressed()), "find previous occurrence");
	QMenu*        findSettingsMenu = new QMenu(this);
	QActionGroup* findCaseGroup    = new QActionGroup(findSettingsMenu);
	findCaseInsensitiveAction      = findSettingsMenu->addAction("case insensitive");
	findCaseInsensitiveAction->setCheckable(true);
	findCaseGroup->addAction(findCaseInsensitiveAction);
	findCaseSensitiveAction = findSettingsMenu->addAction("case sensitive");
	findCaseSensitiveAction->setCheckable(true);
	findCaseGroup->addAction(findCaseSensitiveAction);
	findSmartCaseAction = findSettingsMenu->addAction("smart case");
	findSmartCaseAction->setCheckable(true);
	findSmartCaseAction->setChecked(true);
	findCaseGroup->addAction(findSmartCaseAction);
	findSettingsMenu->addSeparator();
	findWholeWordsAction = findSettingsMenu->addAction("whole words");
	findWholeWordsAction->setCheckable(true);
	addToolBarMenuButton(QIcon(":/findSettings-button"), "change find settings", findSettingsMenu);
}

QToolButton* XpdfViewer::addToolBarButton(const QIcon& icon, const char* slot, const char* tip)
{
	QAction* action = new QAction(icon, "", this);
	action->setToolTip(tip);
	QToolButton* button = new QToolButton();
	button->setDefaultAction(action);
	button->setAutoRaise(true);
	toolBar->addWidget(button);
	connect(action, SIGNAL(triggered()), this, slot);
	return button;
}

XpdfMenuButton* XpdfViewer::addToolBarMenuButton(const QIcon& icon, const char* tip, QMenu* menu)
{
	QAction* action = new QAction(icon, "", this);
	action->setToolTip(tip);
	XpdfMenuButton* button = new XpdfMenuButton(menu);
	button->setDefaultAction(action);
	button->setAutoRaise(true);
	button->setToolTip(tip);
	toolBar->addWidget(button);
	return button;
}

void XpdfViewer::addToolBarSeparator()
{
	addToolBarSpacing(8);
	toolBar->addSeparator();
	addToolBarSpacing(8);
}

void XpdfViewer::addToolBarSpacing(int w)
{
	QWidget* space = new QWidget();
	space->setFixedWidth((toolBarFontSize * w) / 20);
	toolBar->addWidget(space);
}

void XpdfViewer::addToolBarStretch()
{
	QWidget* stretch = new QWidget();
	stretch->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	toolBar->addWidget(stretch);
}

void XpdfViewer::createMainMenu()
{
	mainMenu = menuBar();

	QMenu* fileSubmenu = mainMenu->addMenu("&File");
	fileSubmenu->addAction("&Open...", this, SLOT(openMenuAction()));
	fileSubmenu->addAction("Open in new window...", this, SLOT(openInNewWinMenuAction()));
	fileSubmenu->addAction("Reload", this, SLOT(reloadMenuAction()));
	fileSubmenu->addAction("&Save as...", this, SLOT(saveAsMenuAction()));
	fileSubmenu->addAction("Load last session", this, SLOT(loadSessionMenuAction()));
	fileSubmenu->addSeparator();
	fileSubmenu->addAction("Save image...", this, SLOT(saveImageMenuAction()));
#if XPDFWIDGET_PRINTING
	fileSubmenu->addSeparator();
	fileSubmenu->addAction("&Print...", this, SLOT(printMenuAction()));
#endif
	fileSubmenu->addSeparator();
#ifdef _WIN32
	fileSubmenu->addAction("E&xit", this, SLOT(quitMenuAction()));
#else
	fileSubmenu->addAction("&Quit", this, SLOT(quitMenuAction()));
#endif

	QMenu* editSubmenu = mainMenu->addMenu("&Edit");
	editSubmenu->addAction("Copy", this, SLOT(copyMenuAction()));

	QMenu* viewSubmenu    = mainMenu->addMenu("&View");
	toggleToolbarMenuItem = viewSubmenu->addAction("Toolbar", this, SLOT(toggleToolbarMenuAction(bool)));
	toggleToolbarMenuItem->setCheckable(true);
	toggleSidebarMenuItem = viewSubmenu->addAction("Sidebar", this, SLOT(toggleSidebarMenuAction(bool)));
	toggleSidebarMenuItem->setCheckable(true);
	viewPageLabelsMenuItem = viewSubmenu->addAction("Page labels", this, SLOT(viewPageLabelsMenuAction(bool)));
	viewPageLabelsMenuItem->setCheckable(true);
	viewSubmenu->addSeparator();
	displayModeSubmenu             = new QMenu(this);
	QActionGroup* displayModeGroup = new QActionGroup(this);
	QAction*      action;
	action = displayModeSubmenu->addAction(
	    "Single page",
	    this, SLOT(singlePageModeMenuAction()));
	action->setCheckable(true);
	displayModeGroup->addAction(action);
	action = displayModeSubmenu->addAction(
	    "Continuous",
	    this, SLOT(continuousModeMenuAction()));
	action->setCheckable(true);
	displayModeGroup->addAction(action);
	action = displayModeSubmenu->addAction(
	    "Side-by-side single",
	    this, SLOT(sideBySideSingleModeMenuAction()));
	action->setCheckable(true);
	displayModeGroup->addAction(action);
	action = displayModeSubmenu->addAction(
	    "Side-by-side continuous",
	    this, SLOT(sideBySideContinuousModeMenuAction()));
	action->setCheckable(true);
	displayModeGroup->addAction(action);
	action = displayModeSubmenu->addAction(
	    "Horizontal continuous",
	    this, SLOT(horizontalContinuousModeMenuAction()));
	action->setCheckable(true);
	displayModeGroup->addAction(action);
	viewSubmenu->addAction("Display mode")->setMenu(displayModeSubmenu);
	fullScreenMenuItem = viewSubmenu->addAction("Full screen", this, SLOT(fullScreenMenuAction(bool)));
	fullScreenMenuItem->setCheckable(true);
	viewSubmenu->addSeparator();
	viewSubmenu->addAction("Rotate clockwise", this, SLOT(rotateClockwiseMenuAction()));
	viewSubmenu->addAction("Rotate counterclockwise", this, SLOT(rotateCounterclockwiseMenuAction()));
	viewSubmenu->addSeparator();
	viewSubmenu->addAction("Zoom to selection", this, SLOT(zoomToSelectionMenuAction()));

	QMenu* toolsSubmenu = mainMenu->addMenu("&Tools");
	toolsSubmenu->addAction("Document info", this, SLOT(documentInfoMenuAction()));

	QMenu* windowSubmenu = mainMenu->addMenu("&Window");
	windowSubmenu->addAction("New tab", this, SLOT(newTabMenuAction()));
	windowSubmenu->addAction("New window", this, SLOT(newWindowMenuAction()));
	windowSubmenu->addSeparator();
	windowSubmenu->addAction("Close tab", this, SLOT(closeTabMenuAction()));
	windowSubmenu->addAction("Close window", this, SLOT(closeWindowMenuAction()));
	windowSubmenu->addSeparator();
	windowSubmenu->addAction("Open error window...", this, SLOT(openErrorWindowMenuAction()));

	QMenu* helpSubmenu = mainMenu->addMenu("&Help");
	helpSubmenu->addAction("Help...", this, SLOT(helpMenuAction()));
	helpSubmenu->addAction("Key bindings...", this, SLOT(keyBindingsMenuAction()));
	helpSubmenu->addAction("About XpdfReader...", this, SLOT(aboutMenuAction()));
}

// This can't be named createPopupMenu because there's a QMainWindow
// function of that name.
void XpdfViewer::createXpdfPopupMenu()
{
	PopupMenuCmd* cmd;
	QAction*      action;

	popupMenu             = new QMenu(this);
	popupMenuSignalMapper = new QSignalMapper(this);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
	connect(popupMenuSignalMapper, SIGNAL(mappedInt(int)), this, SLOT(popupMenuAction(int)));
#else
	connect(popupMenuSignalMapper, SIGNAL(mapped(int)), this, SLOT(popupMenuAction(int)));
#endif

	const int n = globalParams->getNumPopupMenuCmds();
	if (n == 0)
	{
		popupMenu->addAction("use 'popupMenuCmd' to add items to this menu");
		popupMenu->addAction("see the xpdfrc(5) documentation");
	}
	else
	{
		for (int i = 0; i < n; ++i)
		{
			cmd    = globalParams->getPopupMenuCmd(i);
			action = popupMenu->addAction(cmd->label.c_str(), popupMenuSignalMapper, SLOT(map()));
			popupMenuSignalMapper->setMapping(action, i);
		}
	}
}

QWidget* XpdfViewer::createTabPane()
{
	QWidget* tabPane = new QWidget();

	QVBoxLayout* tabPaneLayout = new QVBoxLayout();
	tabPaneLayout->setContentsMargins(0, 0, 0, 0);
	tabPaneLayout->setSpacing(0);
	tabPane->setLayout(tabPaneLayout);

	tabList = new QListWidget();
	tabList->setSelectionMode(QAbstractItemView::SingleSelection);
	tabList->setDragEnabled(true);
	tabList->setDragDropMode(QAbstractItemView::InternalMove);
	tabList->viewport()->setAcceptDrops(true);
	tabList->setDropIndicatorShown(true);
	connect(tabList, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), this, SLOT(switchTab(QListWidgetItem*, QListWidgetItem*)));
	connect(tabList->model(), SIGNAL(rowsMoved(const QModelIndex&, int, int, const QModelIndex, int)), this, SLOT(tabsReordered(const QModelIndex&, int, int, const QModelIndex&, int)));
	tabPaneLayout->addWidget(tabList);

	QPushButton* newTabBtn = new QPushButton("+ tab");
	connect(newTabBtn, SIGNAL(clicked()), this, SLOT(newTabButtonPressed()));
	tabPaneLayout->addWidget(newTabBtn);

	return tabPane;
}

QWidget* XpdfViewer::createInfoPane()
{
	QWidget* infoPane = new QWidget();

	QVBoxLayout* infoLayout = new QVBoxLayout();
	infoLayout->setContentsMargins(0, 0, 0, 0);
	infoLayout->setSpacing(0);
	infoPane->setLayout(infoLayout);

	// NB: order here must match order in updateInfoPane().
	infoComboBox = new QComboBox();
	infoComboBox->setEditable(false);
	infoComboBox->addItem("outline");
	infoComboBox->addItem("layers");
	infoComboBox->addItem("attachments");
	infoLayout->addWidget(infoComboBox);
	connect(infoComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(infoComboBoxChanged(int)));

	infoStack = new QStackedLayout();
	infoLayout->addLayout(infoStack);

	return infoPane;
}

void XpdfViewer::updateInfoPane()
{
	// NB: order here must match order in createInfoPane().
	switch (infoComboBox->currentIndex())
	{
	case 0:
		infoStack->setCurrentWidget(currentTab->outlineTree);
		break;
	case 1:
		infoStack->setCurrentWidget(currentTab->layerTree);
		break;
	case 2:
		infoStack->setCurrentWidget(currentTab->attachmentList);
		break;
	}
}

void XpdfViewer::destroyWindow()
{
	delete errorWindow;

	// QTreeView doesn't take ownership of the model, so we need to
	// explicitly delete those
	for (int i = 0; i < tabInfo->getLength(); ++i)
	{
		delete ((XpdfTabInfo*)tabInfo->get(i))->outlineTree->model();
		delete ((XpdfTabInfo*)tabInfo->get(i))->layerTree->model();
	}

	deleteGList(tabInfo, XpdfTabInfo);

	delete indicatorAnimation;
}

void XpdfViewer::enterFullScreenMode()
{
	mainMenu->hide();
	toolBar->hide();
	sidebarSplitter->widget(0)->hide();

	fullScreenPreviousDisplayMode = currentTab->pdf->getDisplayMode();
	currentTab->pdf->setDisplayMode(XpdfWidget::pdfDisplaySingle);
	updateModeInfo();

	fullScreenPreviousZoom = currentTab->pdf->getZoom();
	currentTab->pdf->setZoom(XpdfWidget::zoomToPage);
	updateZoomInfo();

	currentTab->pdf->setMatteColor(app->getFullScreenMatteColor());
	currentTab->pdf->setFrameStyle(QFrame::NoFrame);

	showFullScreen();

	fullScreenMenuItem->setChecked(true);
}

void XpdfViewer::exitFullScreenMode()
{
	mainMenu->show();
	toolBar->show();
	sidebarSplitter->widget(0)->show();

	currentTab->pdf->setDisplayMode(fullScreenPreviousDisplayMode);
	updateModeInfo();

	currentTab->pdf->setZoom(fullScreenPreviousZoom);
	updateZoomInfo();

	currentTab->pdf->setMatteColor(app->getMatteColor());
	currentTab->pdf->setFrameStyle(QFrame::Sunken | QFrame::StyledPanel);

	showNormal();

	fullScreenMenuItem->setChecked(false);
}

void XpdfViewer::addTab()
{
	QListWidgetItem* listItem;
	XpdfWidget*      pdf;
	QTreeView *      outlineTree, *layerTree;
	QTableWidget*    attachmentList;

	pdf = new XpdfWidget(nullptr, app->getPaperColor(), app->getMatteColor(), app->getReverseVideo());
	pdf->setSelectionColor(app->getSelectionColor());
	pdf->enableHyperlinks(false);
	pdf->setKeyPassthrough(true);
	pdf->setMousePassthrough(true);

	const auto& initialSelectMode = globalParams->getInitialSelectMode();
	if (initialSelectMode == "block")
		pdf->setBlockSelectMode();
	else
		pdf->setLinearSelectMode();

	connect(pdf, SIGNAL(resized()), this, SLOT(pdfResized()));
	connect(pdf, SIGNAL(paintDone(bool)), this, SLOT(pdfPaintDone(bool)));
	connect(pdf, SIGNAL(preLoad()), this, SLOT(preLoad()));
	connect(pdf, SIGNAL(postLoad()), this, SLOT(postLoad()));
	connect(pdf, SIGNAL(keyPress(QKeyEvent*)), this, SLOT(keyPress(QKeyEvent*)));
	connect(pdf, SIGNAL(mousePress(QMouseEvent*)), this, SLOT(mousePress(QMouseEvent*)));
	connect(pdf, SIGNAL(mouseRelease(QMouseEvent*)), this, SLOT(mouseRelease(QMouseEvent*)));
	connect(pdf, SIGNAL(mouseClick(QMouseEvent*)), this, SLOT(mouseClick(QMouseEvent*)));
	connect(pdf, SIGNAL(mouseDoubleClick(QMouseEvent*)), this, SLOT(mouseDoubleClick(QMouseEvent*)));
	connect(pdf, SIGNAL(mouseTripleClick(QMouseEvent*)), this, SLOT(mouseTripleClick(QMouseEvent*)));
	connect(pdf, SIGNAL(mouseWheel(QWheelEvent*)), this, SLOT(mouseWheel(QWheelEvent*)));
	connect(pdf, SIGNAL(mouseMove(QMouseEvent*)), this, SLOT(mouseMove(QMouseEvent*)));
	connect(pdf, SIGNAL(midPageChange(int)), this, SLOT(pageChange(int)));
#if XPDFWIDGET_PRINTING
	connect(pdf, SIGNAL(printStatus(int, int, int)), this, SLOT(printStatus(int, int, int)));
#endif
	viewerStack->addWidget(pdf);
	viewerStack->setCurrentWidget(pdf);
	// after adding a tab, focus goes to the XpdfWidget
	pdf->setFocus(Qt::OtherFocusReason);

	//--- create tab pane item
	listItem = new QListWidgetItem();
	tabList->addItem(listItem);
	tabList->setCurrentItem(listItem);

	//--- create outline view
	outlineTree = new QTreeView();
	outlineTree->setModel(new OutlineModel(pdf));
	outlineTree->setHeaderHidden(true);
	outlineTree->setUniformRowHeights(true);
	outlineTree->setSelectionMode(QAbstractItemView::SingleSelection);
	connect(outlineTree, SIGNAL(clicked(const QModelIndex&)), this, SLOT(outlineItemClicked(const QModelIndex&)));
	infoStack->addWidget(outlineTree);

	//--- create layer view
	layerTree = new QTreeView();
	layerTree->setModel(new LayerModel(pdf));
	layerTree->setHeaderHidden(true);
	layerTree->setUniformRowHeights(true);
	layerTree->setSelectionMode(QAbstractItemView::NoSelection);
	connect(layerTree, SIGNAL(clicked(const QModelIndex&)), this, SLOT(layerItemClicked(const QModelIndex&)));
	infoStack->addWidget(layerTree);

	//--- create attachment list
	attachmentList = new QTableWidget(4, 2);
	attachmentList->horizontalHeader()->hide();
	attachmentList->verticalHeader()->hide();
	attachmentList->setShowGrid(false);
	attachmentList->setWordWrap(false);
	attachmentList->setSelectionMode(QAbstractItemView::NoSelection);
	infoStack->addWidget(attachmentList);

	currentTab = new XpdfTabInfo(listItem, pdf, outlineTree, layerTree, attachmentList);
	tabInfo->append(currentTab);

	updateInfoPane();

	scaleFactor = pdf->getCore()->getScaleFactor();
}

void XpdfViewer::closeTab(XpdfTabInfo* tab)
{
	app->startUpdatePagesFile();
	app->updatePagesFile(tab->pdf->getFileName(), tab->pdf->getMidPage());
	app->finishUpdatePagesFile();

	int i = 0;
	for (; i < tabInfo->getLength(); ++i)
		if ((XpdfTabInfo*)tabInfo->get(i) == tab)
			break;
	if (i == tabInfo->getLength())
	{
		// this shouldn't happen
		return;
	}
	tabInfo->del(i);

	for (int i = 0; i < tabList->count(); ++i)
	{
		if (tabList->item(i) == tab->listItem)
		{
			delete tabList->takeItem(i);
			break;
		}
	}

	infoStack->removeWidget(tab->outlineTree);
	delete tab->outlineTree->model();
	delete tab->outlineTree;
	infoStack->removeWidget(tab->layerTree);
	delete tab->layerTree->model();
	delete tab->layerTree;
	infoStack->removeWidget(tab->attachmentList);

	viewerStack->removeWidget(tab->pdf);
	tab->pdf->closeFile();
	delete tab->pdf;

	delete tab;

	if (tabInfo->getLength() > 0)
	{
		updateModeInfo();
		updateDocInfo();
	}
}

void XpdfViewer::gotoTab(int idx)
{
	XpdfTabInfo* tab;

	tab        = (XpdfTabInfo*)tabInfo->get(idx);
	currentTab = tab;
	viewerStack->setCurrentWidget(currentTab->pdf);
	// after switching tabs, focus goes to the XpdfWidget
	currentTab->pdf->setFocus(Qt::OtherFocusReason);
	updateInfoPane();
	updateModeInfo();
	updateDocInfo();
}

// Update the display mode checkboxes, based on the current XpdfWidget
// settings.
void XpdfViewer::updateModeInfo()
{
	switch (currentTab->pdf->getDisplayMode())
	{
	case XpdfWidget::pdfDisplaySingle:
		displayModeSubmenu->actions()[0]->setChecked(true);
		break;
	case XpdfWidget::pdfDisplayContinuous:
		displayModeSubmenu->actions()[1]->setChecked(true);
		break;
	case XpdfWidget::pdfDisplaySideBySideSingle:
		displayModeSubmenu->actions()[2]->setChecked(true);
		break;
	case XpdfWidget::pdfDisplaySideBySideContinuous:
		displayModeSubmenu->actions()[3]->setChecked(true);
		break;
	case XpdfWidget::pdfDisplayHorizontalContinuous:
		displayModeSubmenu->actions()[4]->setChecked(true);
		break;
	}
}

// Update the displayed zoom percentage, based on the current
// XpdfWidget settings.
void XpdfViewer::updateZoomInfo()
{
	int     pg;
	double  z;
	QString zoomStr;

	if (currentTab->pdf->hasOpenDocument())
		pg = currentTab->pdf->getMidPage();
	else
		pg = 1;
	z = (int)floor(currentTab->pdf->getZoomPercent(pg) / app->getZoomScaleFactor() + 0.5);
	zoomStr.setNum(z);
	zoomStr.append('%');
	zoomComboBox->setEditText(zoomStr);

	z = currentTab->pdf->getZoom();
	if (z == XpdfWidget::zoomToWidth)
	{
		fitWidthBtn->setIcon(QIcon(":/fitWidthOn-button"));
		fitPageBtn->setIcon(QIcon(":/fitPage-button"));
	}
	else if (z == XpdfWidget::zoomToPage)
	{
		fitWidthBtn->setIcon(QIcon(":/fitWidth-button"));
		fitPageBtn->setIcon(QIcon(":/fitPageOn-button"));
	}
	else
	{
		fitWidthBtn->setIcon(QIcon(":/fitWidth-button"));
		fitPageBtn->setIcon(QIcon(":/fitPage-button"));
	}
}

void XpdfViewer::updateSelectModeInfo()
{
	if (currentTab->pdf->isBlockSelectMode())
		selectModeBtn->setIcon(QIcon(":/selectModeBlock-button"));
	else
		selectModeBtn->setIcon(QIcon(":/selectModeLinear-button"));
}

// This is called when:
//   - when the GUI is initially created
//   - a document is opened or closed
//   - a tab switch happens
// It updates all visible info related to the document.
void XpdfViewer::updateDocInfo()
{
	//--- window title
	QString windowTitle;
	if (currentTab->pdf->hasOpenDocument())
	{
		windowTitle = currentTab->pdf->getFileName();
		windowTitle += " - XpdfReader";
	}
	else
	{
		windowTitle = "XpdfReader";
	}
	setWindowTitle(windowTitle);

	//--- tab title
	QString tabTitle;
	if (currentTab->pdf->hasOpenDocument())
	{
		tabTitle = currentTab->pdf->getFileName();
		int i    = (int)tabTitle.lastIndexOf('/');
#ifdef _WIN32
		int j = (int)tabTitle.lastIndexOf('\\');
		if (j > i)
			i = j;
#endif
		if (i >= 0)
			tabTitle = tabTitle.mid(i + 1) + " [" + tabTitle.left(i + 1) + "]";
	}
	else
	{
		tabTitle = "(empty)";
	}
	currentTab->listItem->setText(tabTitle);
	currentTab->listItem->setToolTip(tabTitle);

	//--- page number
	updatePageNumberOrLabel(currentTab->pdf->getMidPage());

	//--- page count
	QString nPages;
	if (currentTab->pdf->hasOpenDocument())
		nPages.setNum(currentTab->pdf->getNumPages());
	pageCount->setText(nPages);

	//--- zoom
	// NB: in fit-{page,width,height} modes zoom percent depends on page
	// size, so we need to update whenever a new doc is loaded
	updateZoomInfo();

	//--- selection mode
	updateSelectModeInfo();

	//--- hide the link target bar
	currentTab->pdf->unsetCursor();
	linkTargetBar->hide();
	linkTargetInfo = QString();
}

void XpdfViewer::updatePageNumberOrLabel(int pg)
{
	QString qs;

	if (!viewPageLabelsMenuItem->isChecked() || !currentTab->pdf->getCore()->getDoc() || (qs = currentTab->pdf->getPageLabelFromPageNum(pg)).isEmpty())
		qs.setNum(pg);
	pageNumber->setText(qs);
}

void XpdfViewer::updateOutline(int pg)
{
	QModelIndex idx;

	idx = ((OutlineModel*)currentTab->outlineTree->model())
	          ->findPageIndex(pg, currentTab->outlineTree);
	if (idx.isValid())
		currentTab->outlineTree->setCurrentIndex(idx);
}

void XpdfViewer::setOutlineOpenItems(const QModelIndex& idx)
{
	OutlineModel*     model;
	XpdfOutlineHandle item;
	QModelIndex       child;

	if (idx.isValid())
	{
		item = (XpdfOutlineHandle)idx.internalPointer();
		if (!currentTab->pdf->getOutlineStartsOpen(item))
			return;
		currentTab->outlineTree->expand(idx);
	}
	model       = (OutlineModel*)currentTab->outlineTree->model();
	const int n = model->rowCount(idx);
	for (int i = 0; i < n; ++i)
	{
		child = model->index(i, 0, idx);
		setOutlineOpenItems(child);
	}
}

void XpdfViewer::fillAttachmentList()
{
	QButtonGroup*     btnGroup;
	QPushButton*      saveBtn;
	QTableWidgetItem* item;

	const int n = currentTab->pdf->getNumEmbeddedFiles();
	currentTab->attachmentList->setRowCount(n);
	currentTab->attachmentList->setColumnCount(2);
	btnGroup = new QButtonGroup(currentTab->attachmentList);
	for (int i = 0; i < n; ++i)
	{
		saveBtn = new QPushButton("save");
		saveBtn->setStyleSheet("padding-left:4px; padding-right:4px;");
		btnGroup->addButton(saveBtn, i);
		btnGroup->setId(saveBtn, i);
		currentTab->attachmentList->setCellWidget(i, 0, saveBtn);
		item = new QTableWidgetItem(currentTab->pdf->getEmbeddedFileName(i));
		currentTab->attachmentList->setItem(i, 1, item);
	}
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
	connect(btnGroup, SIGNAL(idClicked(int)), this, SLOT(attachmentSaveClicked(int)));
#else
	connect(btnGroup, SIGNAL(buttonClicked(int)), this, SLOT(attachmentSaveClicked(int)));
#endif
	currentTab->attachmentList->resizeRowsToContents();
	currentTab->attachmentList->resizeColumnsToContents();
}

void XpdfViewer::statusIndicatorStart()
{
	if (indicatorAnimation->state() == QAbstractAnimation::Paused)
		indicatorAnimation->resume();
}

void XpdfViewer::statusIndicatorStop()
{
	if (indicatorAnimation->state() == QAbstractAnimation::Running)
	{
		indicatorAnimation->pause();
		indicatorAnimation->setCurrentTime(0);
	}
}

void XpdfViewer::statusIndicatorOk()
{
	if (indicatorAnimation->values() != indicatorIcons)
		indicatorAnimation->setValues(indicatorIcons);
}

void XpdfViewer::statusIndicatorError()
{
	if (indicatorAnimation->values() != indicatorErrIcons)
		indicatorAnimation->setValues(indicatorErrIcons);
}

void XpdfViewer::showFindError()
{
	findEdit->setStyleSheet("background: #ff8080;");
	findErrorTimer->start(1000);
}

void XpdfViewer::clearFindError()
{
	findErrorTimer->stop();
	findEdit->setStyleSheet("");
}

void XpdfViewer::createDocumentInfoDialog()
{
	documentInfoDialog = new QDialog(this);
	documentInfoDialog->setWindowTitle("XpdfReader Document Info");

	QVBoxLayout* vbox = new QVBoxLayout();
	documentInfoDialog->setLayout(vbox);

	QTabWidget* tabs = new QTabWidget();
	vbox->addWidget(tabs);

	documentInfoMetadataTab = new QTextBrowser();
	documentInfoMetadataTab->setLineWrapMode(QTextEdit::NoWrap);
	tabs->addTab(documentInfoMetadataTab, "Metadata");

	documentInfoFontsTab = new QTextBrowser();
	documentInfoFontsTab->setLineWrapMode(QTextEdit::NoWrap);
	tabs->addTab(documentInfoFontsTab, "Fonts");
}

void XpdfViewer::updateDocumentInfoDialog(XpdfWidget* view)
{
	if (!view->hasOpenDocument())
		return;

	if (!documentInfoDialog)
		createDocumentInfoDialog();

	documentInfoDialog->setWindowTitle(view->getFileName() + " - XpdfReader Document Info");
	documentInfoMetadataTab->setHtml(createDocumentInfoMetadataHTML(view));
	documentInfoFontsTab->setHtml(createDocumentInfoFontsHTML(view));

	//--- tweak the dialog size
	int w  = documentInfoMetadataTab->contentsMargins().left() + (int)documentInfoMetadataTab->document()->idealWidth() + documentInfoMetadataTab->contentsMargins().right() + 50;
	int w1 = documentInfoFontsTab->contentsMargins().left() + (int)documentInfoFontsTab->document()->idealWidth() + documentInfoFontsTab->contentsMargins().right() + 50;
	if (w1 > w)
		w = w1;
	int h = 20 * documentInfoMetadataTab->fontMetrics().lineSpacing() + 75;
	documentInfoDialog->resize(w, h);
}

static QString parseInfoDate(const std::string& s)
{
	const char* p = s.c_str();
	if (p[0] == 'D' && p[1] == ':')
		p += 2;
	int year, mon, day, hour, min, sec, n;
	if ((n = sscanf(p, "%4d%2d%2d%2d%2d%2d", &year, &mon, &day, &hour, &min, &sec)) < 1)
		return QString();
	switch (n)
	{
	case 1: mon = 1;
	case 2: day = 1;
	case 3: hour = 0;
	case 4: min = 0;
	case 5: sec = 0;
	}
	struct tm tmStruct;
	tmStruct.tm_year  = year - 1900;
	tmStruct.tm_mon   = mon - 1;
	tmStruct.tm_mday  = day;
	tmStruct.tm_hour  = hour;
	tmStruct.tm_min   = min;
	tmStruct.tm_sec   = sec;
	tmStruct.tm_wday  = -1;
	tmStruct.tm_yday  = -1;
	tmStruct.tm_isdst = -1;
	// compute the tm_wday and tm_yday fields
	char buf[256];
	if (!(mktime(&tmStruct) != (time_t)-1 && strftime(buf, sizeof(buf), "%c", &tmStruct)))
		return QString();
	return QString(buf);
}

static QString parseXMPDate(const std::string& s)
{
	int         year, mon, day, hour, min, sec, tz;
	char        buf[256];
	const char* p = s.c_str();
	if (isdigit(p[0]) && isdigit(p[1]) && isdigit(p[2]) && isdigit(p[3]))
	{
		buf[0] = p[0];
		buf[1] = p[1];
		buf[2] = p[2];
		buf[3] = p[3];
		buf[4] = '\0';
		year   = atoi(buf);
		p += 4;
	}
	else
	{
		return QString();
	}
	mon = day = 1;
	hour = min = sec = 0;
	tz               = 2000;
	if (p[0] == '-' && isdigit(p[1]) && isdigit(p[2]))
	{
		buf[0] = p[1];
		buf[1] = p[2];
		buf[2] = '\0';
		mon    = atoi(buf);
		p += 3;
		if (p[0] == '-' && isdigit(p[1]) && isdigit(p[2]))
		{
			buf[0] = p[1];
			buf[1] = p[2];
			buf[2] = '\0';
			day    = atoi(buf);
			p += 3;
			if (p[0] == 'T' && isdigit(p[1]) && isdigit(p[2]) && p[3] == ':' && isdigit(p[4]) && isdigit(p[5]))
			{
				buf[0] = p[1];
				buf[1] = p[2];
				buf[2] = '\0';
				hour   = atoi(buf);
				buf[0] = p[4];
				buf[1] = p[5];
				buf[2] = '\0';
				min    = atoi(buf);
				p += 6;
				if (p[0] == ':' && isdigit(p[1]) && isdigit(p[2]))
				{
					buf[0] = p[1];
					buf[1] = p[2];
					buf[2] = '\0';
					sec    = atoi(buf);
					if (p[0] == '.' && isdigit(p[1]))
						p += 2;
				}
				if ((p[0] == '+' || p[0] == '-') && isdigit(p[1]) && isdigit(p[2]) && p[3] == ':' && isdigit(p[4]) && isdigit(p[5]))
				{
					buf[0] = p[1];
					buf[1] = p[2];
					buf[2] = '\0';
					tz     = atoi(buf);
					buf[0] = p[4];
					buf[1] = p[5];
					buf[2] = '\0';
					tz     = tz * 60 + atoi(buf);
					tz     = tz * 60;
					if (p[0] == '-')
						tz = -tz;
				}
			}
		}
	}

	struct tm tmStruct;
	tmStruct.tm_year  = year - 1900;
	tmStruct.tm_mon   = mon - 1;
	tmStruct.tm_mday  = day;
	tmStruct.tm_hour  = hour;
	tmStruct.tm_min   = min;
	tmStruct.tm_sec   = sec;
	tmStruct.tm_wday  = -1;
	tmStruct.tm_yday  = -1;
	tmStruct.tm_isdst = -1;
	// compute the tm_wday and tm_yday fields
	//~ this ignores the timezone
	if (!(mktime(&tmStruct) != (time_t)-1 && strftime(buf, sizeof(buf), "%c", &tmStruct)))
		return QString();
	return QString(buf);
}

static QString createInfoString(Object* infoDict, const char* infoKey, ZxDoc* xmp, const char* xmpKey1, const char* xmpKey2, bool parseDate)
{
	//-- check the XMP metadata
	if (xmp)
	{
		ZxElement* rdf = xmp->getRoot();
		if (rdf->isElement("x:xmpmeta"))
			rdf = rdf->findFirstChildElement("rdf:RDF");
		if (rdf && rdf->isElement("rdf:RDF"))
		{
			for (ZxNode* node = rdf->getFirstChild(); node; node = node->getNextChild())
			{
				if (node->isElement("rdf:Description"))
				{
					ZxElement* elem;
					if ((elem = node->findFirstChildElement(xmpKey1)) || (xmpKey2 && (elem = node->findFirstChildElement(xmpKey2))))
					{
						ZxElement* child;
						ZxNode*    node2;
						if ((child = elem->findFirstChildElement("rdf:Alt")) || (child = elem->findFirstChildElement("rdf:Seq")))
						{
							if ((node2 = child->findFirstChildElement("rdf:li")))
								node2 = node2->getFirstChild();
						}
						else
						{
							node2 = elem->getFirstChild();
						}
						if (node2 && node2->isCharData())
						{
							QString value;
							if (parseDate)
								value = parseXMPDate(((ZxCharData*)node2)->getData());
							if (value.isEmpty())
								value = QString::fromUtf8(((ZxCharData*)node2)->getData().c_str());
							return QString("<tr><td>") + infoKey + ":&nbsp;</td><td>" + value + "</td></tr>\n";
						}
					}
				}
			}
		}
	}

	//-- check the info dictionary
	if (infoDict->isDict())
	{
		Object obj;
		if (infoDict->dictLookup(infoKey, &obj)->isString())
		{
			QString value;
			if (parseDate)
				value = parseInfoDate(obj.getString());
			if (value.isEmpty())
			{
				TextString* ts = new TextString(obj.getString());
				value          = QString::fromUtf8(ts->toUTF8().c_str());
				delete ts;
			}
			obj.free();
			return QString("<tr><td>") + infoKey + ":&nbsp;</td><td>" + value + "</td></tr>\n";
		}
		obj.free();
	}

	return QString();
}

QString XpdfViewer::createDocumentInfoMetadataHTML(XpdfWidget* view)
{
	PDFDoc* doc = view->getCore()->getDoc();
	QString html("<table>\n");

	// doc info
	Object info;
	doc->getDocInfo(&info);
	ZxDoc* xmp;
	if (const auto& metadata = doc->readMetadata(); metadata.size())
		xmp = ZxDoc::loadMem(metadata.c_str(), metadata.size());
	else
		xmp = nullptr;
	html += createInfoString(&info, "Title", xmp, "dc:title", nullptr, false);
	html += createInfoString(&info, "Subject", xmp, "dc:description", nullptr, false);
	html += createInfoString(&info, "Keywords", xmp, "pdf:Keywords", nullptr, false);
	html += createInfoString(&info, "Author", xmp, "dc:creator", nullptr, false);
	html += createInfoString(&info, "Creator", xmp, "xmp:CreatorTool", nullptr, false);
	html += createInfoString(&info, "Producer", xmp, "pdf:Producer", nullptr, false);
	html += createInfoString(&info, "CreationDate", xmp, "xap:CreateDate", "xmp:CreateDate", true);
	html += createInfoString(&info, "ModDate", xmp, "xap:ModifyDate", "xmp:ModifyDate", true);
	if (xmp) delete xmp;
	info.free();

	// tagging info
	if (doc->getStructTreeRoot()->isDict())
		html += "<tr><td>Tagged:&nbsp;</td><td>yes</td></tr>\n";
	else
		html += "<tr><td>Tagged:&nbsp;</td><td>no</td></tr>\n";

	// form info
	Object* acroForm = doc->getCatalog()->getAcroForm();
	if (acroForm->isDict())
	{
		Object xfa;
		acroForm->dictLookup("XFA", &xfa);
		if (xfa.isStream() || xfa.isArray())
			if (doc->getCatalog()->getNeedsRendering())
				html += "<tr><td>Form:&nbsp;</td><td>dynamic XFA</td></tr>\n";
			else
				html += "<tr><td>Form:&nbsp;</td><td>static XFA</td></tr>\n";
		else
			html += "<tr><td>Form:&nbsp;</td><td>AcroForm</td></tr>\n";
		xfa.free();
	}
	else
	{
		html += "<tr><td>Form:&nbsp;</td><td>none</td></tr>\n";
	}

	// page count
	html += QString("<tr><td>Pages:&nbsp;</td><td>%1</td></tr>\n")
	            .arg(doc->getNumPages());

	// encryption info
	if (doc->isEncrypted())
	{
		int            permFlags, keyLength, encVersion;
		bool           ownerPasswordOk;
		CryptAlgorithm encAlgorithm;
		doc->getXRef()->getEncryption(&permFlags, &ownerPasswordOk, &keyLength, &encVersion, &encAlgorithm);
		html += QString("<tr><td>Encrypted:&nbsp;</td><td>%1 %2-bit</td></tr>\n")
		            .arg(encAlgorithm == cryptRC4 ? "RC4" : "AES")
		            .arg(keyLength * 8);
		html += QString("<tr><td>Permissions:&nbsp;</td><td>print:%1 copy:%2 change:%3 addNotes:%4</td></tr>\n")
		            .arg(doc->okToPrint(true) ? "yes" : "no")
		            .arg(doc->okToCopy(true) ? "yes" : "no")
		            .arg(doc->okToChange(true) ? "yes" : "no")
		            .arg(doc->okToAddNotes(true) ? "yes" : "no");
	}
	else
	{
		html += QString("<tr><td>Encrypted:&nbsp;</td><td>no</td></tr>\n");
	}

	// page size
	html += QString("<tr><td>Page size:&nbsp;</td><td>%1 x %2 pts (rotated %3 degrees)</td></tr>\n")
	            .arg(doc->getPageCropWidth(1), 0, 'g')
	            .arg(doc->getPageCropHeight(1), 0, 'g')
	            .arg(doc->getPageRotate(1));

	// linearization info
	html += QString("<tr><td>Optimized:&nbsp;</td><td>%1</td></tr>\n")
	            .arg(doc->isLinearized() ? "yes" : "no");

	// PDF version
	html += QString("<tr><td>PDF version:&nbsp;</td><td>%1</td></tr>\n")
	            .arg(doc->getPDFVersion(), 0, 'f', 1);

	html += "</table>\n";

	return html;
}

static bool scanFontsCheckObject(Object* in, Object* out, PDFDoc* doc, char* seenObjs)
{
	if (!in->isRef())
	{
		in->copy(out);
		return true;
	}
	int objNum = in->getRefNum();
	if (objNum < 0 || objNum >= doc->getXRef()->getNumObjects())
	{
		out->initNull();
		return true;
	}
	if (seenObjs[objNum])
	{
		out->initNull();
		return false;
	}
	seenObjs[objNum] = (char)1;
	in->fetch(doc->getXRef(), out);
	return true;
}

static QString scanFont(Dict* fontDict, PDFDoc* doc)
{
	Ref ref;
	ref.num = ref.gen = 0;
	GfxFont* font     = GfxFont::makeFont(doc->getXRef(), "F0", ref, fontDict);
	if (!font)
		return QString();

	// font name
	const auto name = font->getName();

	// font type
	const char* type;
	switch (font->getType())
	{
	case fontUnknownType:
	default: type = "unknown"; break;
	case fontType1: type = "Type 1"; break;
	case fontType1C: type = "Type 1C"; break;
	case fontType1COT: type = "Type 1C (OT)"; break;
	case fontType3: type = "Type 3"; break;
	case fontTrueType: type = "TrueType"; break;
	case fontTrueTypeOT: type = "TrueType (OT)"; break;
	case fontCIDType0: type = "CID Type 0"; break;
	case fontCIDType0C: type = "CID Type 0C"; break;
	case fontCIDType0COT: type = "CID Type 0C (OT)"; break;
	case fontCIDType2: type = "CID TrueType"; break;
	case fontCIDType2OT: type = "CID TrueType (OT)"; break;
	}

	// check for an embedded font
	bool emb;
	if (font->getType() == fontType3)
	{
		emb = true;
	}
	else
	{
		Ref embRef;
		emb = font->getEmbeddedFontID(&embRef);
	}

	// problematic for text extraction
	bool prob = font->problematicForUnicode();

	QString html = QString("<tr><td>%1&nbsp;</td><td>%2&nbsp;</td><td align=\"center\">%3</td><td align=\"center\">%4</td></tr>\n")
	                   .arg(name.size() ? name.c_str() : "[none]")
	                   .arg(type)
	                   .arg(emb ? "yes" : "no")
	                   .arg(prob ? "X" : "");

	delete font;

	return html;
}

static QString scanFonts(Dict* resDict, PDFDoc* doc, char* seenObjs);

static QString scanFonts(Object* obj, PDFDoc* doc, char* seenObjs)
{
	QString html;
	Object  obj2;
	if (scanFontsCheckObject(obj, &obj2, doc, seenObjs) && obj2.isDict())
		html += scanFonts(obj2.getDict(), doc, seenObjs);
	obj2.free();
	return html;
}

static QString scanFonts(Dict* resDict, PDFDoc* doc, char* seenObjs)
{
	QString html;

	// scan the fonts in this resource dictionary
	Object fontDict1, fontDict2;
	resDict->lookupNF("Font", &fontDict1);
	if (scanFontsCheckObject(&fontDict1, &fontDict2, doc, seenObjs) && fontDict2.isDict())
	{
		for (int i = 0; i < fontDict2.dictGetLength(); ++i)
		{
			Object font1, font2;
			fontDict2.dictGetValNF(i, &font1);
			if (scanFontsCheckObject(&font1, &font2, doc, seenObjs) && font2.isDict())
				html += scanFont(font2.getDict(), doc);
			font2.free();
			font1.free();
		}
	}
	fontDict2.free();
	fontDict1.free();

	// recursively scan any resource dictionaries in XObjects in this
	// resource dictionary
	Object xObjDict1, xObjDict2;
	resDict->lookupNF("XObject", &xObjDict1);
	if (scanFontsCheckObject(&xObjDict1, &xObjDict2, doc, seenObjs) && xObjDict2.isDict())
	{
		for (int i = 0; i < xObjDict2.dictGetLength(); ++i)
		{
			Object xObj1, xObj2;
			xObjDict2.dictGetValNF(i, &xObj1);
			if (scanFontsCheckObject(&xObj1, &xObj2, doc, seenObjs) && xObj2.isStream())
			{
				Object resObj;
				xObj2.streamGetDict()->lookupNF("Resources", &resObj);
				html += scanFonts(&resObj, doc, seenObjs);
				resObj.free();
			}
			xObj2.free();
			xObj1.free();
		}
	}
	xObjDict2.free();
	xObjDict1.free();

	// recursively scan any resource dictionaries in Patterns in this
	// resource dictionary
	Object patternDict1, patternDict2;
	resDict->lookupNF("Pattern", &patternDict1);
	if (scanFontsCheckObject(&patternDict1, &patternDict2, doc, seenObjs) && patternDict2.isDict())
	{
		for (int i = 0; i < patternDict2.dictGetLength(); ++i)
		{
			Object pattern1, pattern2;
			patternDict2.dictGetValNF(i, &pattern1);
			if (scanFontsCheckObject(&pattern1, &pattern2, doc, seenObjs) && pattern2.isStream())
			{
				Object resObj;
				pattern2.streamGetDict()->lookupNF("Resources", &resObj);
				html += scanFonts(&resObj, doc, seenObjs);
				resObj.free();
			}
			pattern2.free();
			pattern1.free();
		}
	}
	patternDict2.free();
	patternDict1.free();

	// recursively scan any resource dictionaries in ExtGStates in this
	// resource dictionary
	Object gsDict1, gsDict2;
	resDict->lookupNF("ExtGState", &gsDict1);
	if (scanFontsCheckObject(&gsDict1, &gsDict2, doc, seenObjs) && gsDict2.isDict())
	{
		for (int i = 0; i < gsDict2.dictGetLength(); ++i)
		{
			Object gs1, gs2;
			gsDict2.dictGetValNF(i, &gs1);
			if (scanFontsCheckObject(&gs1, &gs2, doc, seenObjs) && gs2.isDict())
			{
				Object smask1, smask2;
				gs2.dictLookupNF("SMask", &smask1);
				if (scanFontsCheckObject(&smask1, &smask2, doc, seenObjs) && smask2.isDict())
				{
					Object smaskGroup1, smaskGroup2;
					smask2.dictLookupNF("G", &smaskGroup1);
					if (scanFontsCheckObject(&smaskGroup1, &smaskGroup2, doc, seenObjs) && smaskGroup2.isStream())
					{
						Object resObj;
						smaskGroup2.streamGetDict()->lookupNF("Resources", &resObj);
						html += scanFonts(&resObj, doc, seenObjs);
						resObj.free();
					}
					smaskGroup2.free();
					smaskGroup1.free();
				}
				smask2.free();
				smask1.free();
			}
			gs2.free();
			gs1.free();
		}
	}
	gsDict2.free();
	gsDict1.free();

	return html;
}

QString XpdfViewer::createDocumentInfoFontsHTML(XpdfWidget* view)
{
	PDFDoc* doc = view->getCore()->getDoc();

	QString html("<table>\n");
	html += "<tr bgcolor=\"#aaffaa\"><th>&nbsp;name&nbsp;</th><th>&nbsp;type&nbsp;</th><th>&nbsp;embedded&nbsp;</th><th>&nbsp;problematic&nbsp;</th></tr>\n";

	int   numObjects = doc->getXRef()->getNumObjects();
	char* seenObjs   = (char*)gmalloc(numObjects);
	memset(seenObjs, 0, numObjects);

	Annots* annots = doc->getAnnots();
	for (int pg = 1; pg <= doc->getNumPages(); ++pg)
	{
		Page* page    = doc->getCatalog()->getPage(pg);
		Dict* resDict = page->getResourceDict();
		if (resDict)
			html += scanFonts(resDict, doc, seenObjs);
		Object obj1, obj2;
		int    nAnnots = annots->getNumAnnots(pg);
		for (int i = 0; i < nAnnots; ++i)
		{
			if (annots->getAnnot(pg, i)->getAppearance(&obj1)->isStream())
			{
				obj1.streamGetDict()->lookupNF("Resources", &obj2);
				html += scanFonts(&obj2, doc, seenObjs);
				obj2.free();
			}
			obj1.free();
		}
	}
	AcroForm* form = doc->getCatalog()->getForm();
	if (form)
	{
		Object obj1, obj2;
		for (int i = 0; i < form->getNumFields(); ++i)
		{
			form->getField(i)->getResources(&obj1);
			if (obj1.isArray())
			{
				for (int j = 0; j < obj1.arrayGetLength(); ++j)
				{
					obj1.arrayGetNF(j, &obj2);
					html += scanFonts(&obj2, doc, seenObjs);
					obj2.free();
				}
			}
			else if (obj1.isDict())
			{
				html += scanFonts(obj1.getDict(), doc, seenObjs);
			}
			obj1.free();
		}
	}

	gfree(seenObjs);

	html += "</table>\n";
	return html;
}

void XpdfViewer::createKeyBindingsDialog()
{
	keyBindingsDialog = new QDialog(this);
	keyBindingsDialog->setWindowTitle("XpdfReader Key Bindings");

	QVBoxLayout* vbox = new QVBoxLayout();
	keyBindingsDialog->setLayout(vbox);

	QString html = createKeyBindingsHTML();

	QTextBrowser* text = new QTextBrowser();
	text->setHtml(html);
	text->setReadOnly(true);
	text->setMinimumSize(QSize(500, 300));
	vbox->addWidget(text);

	QHBoxLayout* btnBox = new QHBoxLayout();
	vbox->addLayout(btnBox);

	QPushButton* closeBtn = new QPushButton("Close");
	closeBtn->setDefault(true);
	btnBox->addStretch(1);
	btnBox->addWidget(closeBtn);
	btnBox->addStretch(1);
	connect(closeBtn, SIGNAL(clicked()), keyBindingsDialog, SLOT(accept()));
}

QString XpdfViewer::createKeyBindingsHTML()
{
	QString html;
	html += "<h3>Key Bindings:</h3>\n";
	html += "<ul>\n";
	html += "<li><i>modifiers-key (context): command / command / ...</i>\n";
	html += "<br>";
	for (const auto& binding : globalParams->getAllKeyBindings())
	{
		html += "<li>";
		if (binding.mods & xpdfKeyModShift)
			html += "shift-";
		if (binding.mods & xpdfKeyModCtrl)
			html += "ctrl-";
		if (binding.mods & xpdfKeyModAlt)
			html += "alt-";
		if (binding.code == 0x20)
		{
			html += "space";
		}
		else if (binding.code >= 0x21 && binding.code <= 0xfe)
		{
			html += (QChar)binding.code;
		}
		else if (binding.code >= xpdfKeyCodeMousePress1 && binding.code <= xpdfKeyCodeMousePress32)
		{
			html += QString("mousePress%1").arg(binding.code - xpdfKeyCodeMousePress1 + 1);
		}
		else if (binding.code >= xpdfKeyCodeMouseRelease1 && binding.code <= xpdfKeyCodeMouseRelease32)
		{
			html += QString("mouseRelease%1").arg(binding.code - xpdfKeyCodeMouseRelease1 + 1);
		}
		else if (binding.code >= xpdfKeyCodeMouseClick1 && binding.code <= xpdfKeyCodeMouseClick32)
		{
			html += QString("mouseClick%1").arg(binding.code - xpdfKeyCodeMouseClick1 + 1);
		}
		else if (binding.code >= xpdfKeyCodeMouseDoubleClick1 && binding.code <= xpdfKeyCodeMouseDoubleClick32)
		{
			html += QString("mouseDoubleClick%1").arg(binding.code - xpdfKeyCodeMouseDoubleClick1 + 1);
		}
		else if (binding.code >= xpdfKeyCodeMouseTripleClick1 && binding.code <= xpdfKeyCodeMouseTripleClick32)
		{
			html += QString("mouseTripleClick%1").arg(binding.code - xpdfKeyCodeMouseTripleClick1 + 1);
		}
		else if (binding.code >= xpdfKeyCodeF1 && binding.code <= xpdfKeyCodeF35)
		{
			html += QString("f%1").arg(binding.code - xpdfKeyCodeF1 + 1);
		}
		else
		{
			switch (binding.code)
			{
			case xpdfKeyCodeTab: html += "tab"; break;
			case xpdfKeyCodeReturn: html += "return"; break;
			case xpdfKeyCodeEnter: html += "enter"; break;
			case xpdfKeyCodeBackspace: html += "backspace"; break;
			case xpdfKeyCodeEsc: html += "esc"; break;
			case xpdfKeyCodeInsert: html += "insert"; break;
			case xpdfKeyCodeDelete: html += "delete"; break;
			case xpdfKeyCodeHome: html += "home"; break;
			case xpdfKeyCodeEnd: html += "end"; break;
			case xpdfKeyCodePgUp: html += "pgup"; break;
			case xpdfKeyCodePgDn: html += "pgdn"; break;
			case xpdfKeyCodeLeft: html += "left"; break;
			case xpdfKeyCodeRight: html += "right"; break;
			case xpdfKeyCodeUp: html += "up"; break;
			case xpdfKeyCodeDown: html += "down"; break;
			default: html += "[unknown]"; break;
			}
		}
		html += " (";
		if (binding.context == xpdfKeyContextAny)
		{
			html += "any";
		}
		else
		{
			QString mods = "";
			if (binding.context & xpdfKeyContextFullScreen)
			{
				if (!mods.isEmpty()) mods += ",";
				mods += "fullScreen";
			}
			if (binding.context & xpdfKeyContextWindow)
			{
				if (!mods.isEmpty()) mods += ",";
				mods += "window";
			}
			if (binding.context & xpdfKeyContextContinuous)
			{
				if (!mods.isEmpty()) mods += ",";
				mods += "continuous";
			}
			if (binding.context & xpdfKeyContextSinglePage)
			{
				if (!mods.isEmpty()) mods += ",";
				mods += "singlePage";
			}
			if (binding.context & xpdfKeyContextOverLink)
			{
				if (!mods.isEmpty()) mods += ",";
				mods += "overLink";
			}
			if (binding.context & xpdfKeyContextOffLink)
			{
				if (!mods.isEmpty()) mods += ",";
				mods += "offLink";
			}
			if (binding.context & xpdfKeyContextOutline)
			{
				if (!mods.isEmpty()) mods += ",";
				mods += "outline";
			}
			if (binding.context & xpdfKeyContextMainWin)
			{
				if (!mods.isEmpty()) mods += ",";
				mods += "mainWin";
			}
			if (binding.context & xpdfKeyContextScrLockOn)
			{
				if (!mods.isEmpty()) mods += ",";
				mods += "scrLockOn";
			}
			if (binding.context & xpdfKeyContextScrLockOff)
			{
				if (!mods.isEmpty()) mods += ",";
				mods += "scrLockOff";
			}
			html += mods;
		}
		html += "): ";

		for (int j = -1; const auto& cmd : binding.cmds)
		{
			if (++j > 0) html += " / ";
			html += cmd.c_str();
		}
		html += "\n";
	}
	html += "</ul>\n";
	return html;
}

void XpdfViewer::createAboutDialog()
{
	aboutDialog = new QDialog(this);
	aboutDialog->setWindowTitle("About XpdfReader");

	QVBoxLayout* vbox = new QVBoxLayout();
	aboutDialog->setLayout(vbox);

	QTextBrowser* text = new QTextBrowser();
	text->setOpenExternalLinks(true);
	text->setHtml(aboutHTML);
	text->setReadOnly(true);
	text->setMinimumSize(QSize(500, 300));
	vbox->addWidget(text);

	QHBoxLayout* btnBox = new QHBoxLayout();
	vbox->addLayout(btnBox);

	QPushButton* closeBtn = new QPushButton("Close");
	closeBtn->setDefault(true);
	btnBox->addStretch(1);
	btnBox->addWidget(closeBtn);
	btnBox->addStretch(1);
	connect(closeBtn, SIGNAL(clicked()), aboutDialog, SLOT(accept()));
}

#define nSaveImageFormats 3

static struct
{
	const char* comboBoxText;
	const char* fileFilter;
	const char* qImageFormat;
} saveImageFormats[nSaveImageFormats] = {
	{"JPEG",  "JPEG files (*.jpg)",  "JPEG"},
	{ "PNG",  "PNG files (*.png)",   "PNG" },
	{ "TIFF", "TIFF files (*.tiff)", "TIFF"}
};

void XpdfViewer::execSaveImageDialog()
{
	QDialog* dialog = new QDialog();
	dialog->setWindowTitle("XpdfReader: Save Image");

	QVBoxLayout* vbox = new QVBoxLayout();
	dialog->setLayout(vbox);

	QGridLayout* grid = new QGridLayout();
	vbox->addLayout(grid);

	grid->addWidget(new QLabel("Region:"), 0, 0);

	QHBoxLayout* regionBox = new QHBoxLayout();
	grid->addLayout(regionBox, 0, 1);

	QButtonGroup* regionBtnGroup = new QButtonGroup(dialog);

	QRadioButton* pageBtn = new QRadioButton("Page:");
	regionBtnGroup->addButton(pageBtn);
	regionBox->addWidget(pageBtn);
	pageBtn->setChecked(true);

	QLineEdit* pageEdit = new QLineEdit();
	regionBox->addWidget(pageEdit);
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
	pageEdit->setFixedWidth(8 * pageEdit->fontMetrics().horizontalAdvance("0"));
#else
	pageEdit->setFixedWidth(8 * pageEdit->fontMetrics().width("0"));
#endif
	int pg = currentTab->pdf->getMidPage();
	pageEdit->setText(QString().setNum(pg));
	connect(pageEdit, SIGNAL(textChanged(const QString&)), pageBtn, SLOT(click()));
	connect(pageEdit, SIGNAL(cursorPositionChanged(int, int)), pageBtn, SLOT(click()));
	connect(pageEdit, SIGNAL(selectionChanged()), pageBtn, SLOT(click()));

	regionBox->addSpacing(20);

	QRadioButton* selectionBtn = new QRadioButton("Current selection");
	regionBtnGroup->addButton(selectionBtn);
	regionBox->addWidget(selectionBtn);
	selectionBtn->setEnabled(currentTab->pdf->hasSelection());

	grid->addWidget(new QLabel("Color:"), 1, 0);

	QHBoxLayout* colorBox = new QHBoxLayout();
	grid->addLayout(colorBox, 1, 1);

	QComboBox* colorCombo = new QComboBox();
	colorBox->addWidget(colorCombo);
	colorCombo->setEditable(false);
	colorCombo->addItem("RGB");
	colorCombo->addItem("Gray");
	colorCombo->addItem("Monochrome");
	colorCombo->setCurrentIndex(0);

	colorBox->addStretch();

	grid->addWidget(new QLabel("Resolution:"), 2, 0);

	QHBoxLayout* resolutionBox = new QHBoxLayout();
	grid->addLayout(resolutionBox, 2, 1);

	QLineEdit* resolutionEdit = new QLineEdit();
	resolutionBox->addWidget(resolutionEdit);
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
	resolutionEdit->setFixedWidth(8 * pageEdit->fontMetrics().horizontalAdvance("0"));
#else
	resolutionEdit->setFixedWidth(8 * pageEdit->fontMetrics().width("0"));
#endif
	int r = (int)floor(currentTab->pdf->getZoomPercent(pg) * 0.72 + 0.5);
	resolutionEdit->setText(QString().setNum(r));

	resolutionBox->addWidget(new QLabel("dpi"));

	grid->addWidget(new QLabel("Format:"), 3, 0);

	QHBoxLayout* formatBox = new QHBoxLayout();
	grid->addLayout(formatBox, 3, 1);

	QComboBox* formatCombo = new QComboBox();
	formatBox->addWidget(formatCombo);
	formatCombo->setEditable(false);
	for (int i = 0; i < nSaveImageFormats; ++i)
		formatCombo->addItem(saveImageFormats[i].comboBoxText);
	formatCombo->setCurrentIndex(0);

	formatBox->addStretch();

	QHBoxLayout* btnBox = new QHBoxLayout();
	vbox->addLayout(btnBox);

	btnBox->addStretch();

	QPushButton* cancelBtn = new QPushButton("Cancel");
	btnBox->addWidget(cancelBtn);
	connect(cancelBtn, SIGNAL(clicked()), dialog, SLOT(reject()));

	QPushButton* okBtn = new QPushButton("Ok");
	btnBox->addWidget(okBtn);
	okBtn->setDefault(true);
	connect(okBtn, SIGNAL(clicked()), dialog, SLOT(accept()));

	dialog->setModal(true);

	if (dialog->exec() == QDialog::Accepted)
	{
		double res       = resolutionEdit->text().toDouble();
		bool   wholePage = pageBtn->isChecked();
		int    page;
		double x0, y0, x1, y1;
		if (wholePage)
		{
			page = pageEdit->text().toInt();
			if (page < 1 || page > currentTab->pdf->getNumPages())
				page = 1;
			x0 = y0 = x1 = y1 = 0;
		}
		else
		{
			currentTab->pdf->getCurrentSelection(&page, &x0, &y0, &x1, &y1);
		}
		int                        fmt = formatCombo->currentIndex();
		XpdfWidget::ImageColorMode color;
		if (colorCombo->currentIndex() == 1)
		{
			color = XpdfWidget::pdfImageColorGray;
		}
		else if (colorCombo->currentIndex() == 2)
		{
			color = XpdfWidget::pdfImageColorMono;
			if (strcmp(saveImageFormats[fmt].qImageFormat, "TIFF"))
				color = XpdfWidget::pdfImageColorGray;
		}
		else
		{
			color = XpdfWidget::pdfImageColorRGB;
		}
		QString fileName = QFileDialog::getSaveFileName(this, "Save Image", QString(), saveImageFormats[fmt].fileFilter);
		if (!fileName.isEmpty())
		{
			QImage img;
			if (wholePage)
				img = currentTab->pdf->convertPageToImage(page, res, false, color);
			else
				img = currentTab->pdf->convertRegionToImage(page, x0, y0, x1, y1, res, false, color);
			img.save(fileName, saveImageFormats[fmt].qImageFormat);
		}
	}

	delete dialog;
}
