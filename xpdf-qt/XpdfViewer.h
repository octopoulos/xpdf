//========================================================================
//
// XpdfViewer.h
//
// Copyright 2015 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include <QDialog>
#include <QIcon>
#include <QLocalServer>
#include <QMainWindow>
#include <QToolButton>
#include "Error.h"
#include "XpdfWidget.h"

class GList;
class PropertyListAnimation;
class QComboBox;
class QDialog;
class QHBoxLayout;
class QInputEvent;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QMenu;
class QModelIndex;
class QProgressDialog;
class QSignalMapper;
class QSplitter;
class QStackedLayout;
class QStackedWidget;
class QTextBrowser;
class QTimer;
class QToolBar;
class QToolButton;
class XpdfApp;
class XpdfMenuButton;
class XpdfTabInfo;
class XpdfViewer;

//------------------------------------------------------------------------

struct XpdfViewerCmd
{
	const char* name;
	int         nArgs;
	bool        requiresDoc;
	bool        requiresEvent;
	void (XpdfViewer::*func)(const VEC_STR& args, QInputEvent* event);
};

//------------------------------------------------------------------------
// XpdfMenuButton
//------------------------------------------------------------------------

class XpdfMenuButton : public QToolButton
{
	Q_OBJECT

public:
	XpdfMenuButton(QMenu* menuA);

private slots:

	void btnPressed();

private:
	QMenu* menu;
};

//------------------------------------------------------------------------
// XpdfErrorWindow
//------------------------------------------------------------------------

class XpdfErrorWindow : public QWidget
{
	Q_OBJECT

public:
	XpdfErrorWindow(XpdfViewer* viewerA, int errorEventTypeA);
	virtual ~XpdfErrorWindow();
	virtual QSize sizeHint() const;
	virtual void  closeEvent(QCloseEvent* event);
	virtual void  keyPressEvent(QKeyEvent* event);
	virtual void  customEvent(QEvent* event);

private slots:

	void clearBtnPressed();

private:
	static void errorCbk(void* data, ErrorCategory category, int pos, const char* msg);
	static void dummyErrorCbk(void* data, ErrorCategory category, int pos, const char* msg);

	XpdfViewer*  viewer;
	int          errorEventType;
	QListWidget* list;
	QSize        lastSize;
};

//------------------------------------------------------------------------
// XpdfViewer
//------------------------------------------------------------------------

class XpdfViewer : public QMainWindow
{
	Q_OBJECT

public:
	XpdfViewer(XpdfApp* appA, bool fullScreen);

	static XpdfViewer* create(XpdfApp* app, QString fileName, int page, QString destName, int rot, QString password, bool fullScreen);

	virtual ~XpdfViewer();

	virtual QSize sizeHint() const;

	void tweakSize();

	// Open a file in the current tab.  Returns a boolean indicating
	// success.
	bool open(QString fileName, int page, QString destName, int rot, QString password);

	// Open a file in a new tab.  Returns a boolean indicating success.
	bool openInNewTab(QString fileName, int page, QString destName, int rot, QString password, bool switchToTab);

	// Check that [fileName] is open in the current tab -- if not, open
	// it.  In either case, switch to [page] or [destName].  Returns a
	// boolean indicating success.
	bool checkOpen(QString fileName, int page, QString destName, QString password);

	virtual QMenu* createPopupMenu();

	// Start up the remote server socket.
	void startRemoteServer(const QString& remoteServerName);

	// Execute a command [cmd], with [event] for context.
	void execCmd(const char* cmd, QInputEvent* event);

	// Used by XpdfApp::saveSession() to save session info for one
	// window.
	void saveSession(FILE* out, int format);

	// Used by XpdfApp::loadSession() to load a session for one window.
	void loadSession(FILE* in, int format);

	// Returns true if this viewer contains a single empty tab.
	bool isEmpty();

public slots:

	bool close();

private slots:

	void remoteServerConnection();
	void remoteServerRead();

	void pdfResized();
	void pdfPaintDone(bool finished);
	void preLoad();
	void postLoad();
	void keyPress(QKeyEvent* e);
	void mousePress(QMouseEvent* e);
	void mouseRelease(QMouseEvent* e);
	void mouseClick(QMouseEvent* e);
	void mouseDoubleClick(QMouseEvent* e);
	void mouseTripleClick(QMouseEvent* e);
	void mouseWheel(QWheelEvent* e);
	void mouseMove(QMouseEvent* e);
	void pageChange(int pg);
	void sidebarSplitterMoved(int pos, int index);
#if XPDFWIDGET_PRINTING
	void printStatus(int nextPage, int firstPage, int lastPage);
	void cancelPrint();
#endif

	void openMenuAction();
	void openInNewWinMenuAction();
	void reloadMenuAction();
	void saveAsMenuAction();
	void loadSessionMenuAction();
	void saveImageMenuAction();
#if XPDFWIDGET_PRINTING
	void printMenuAction();
#endif
	void quitMenuAction();
	void copyMenuAction();
	void singlePageModeMenuAction();
	void continuousModeMenuAction();
	void sideBySideSingleModeMenuAction();
	void sideBySideContinuousModeMenuAction();
	void horizontalContinuousModeMenuAction();
	void fullScreenMenuAction(bool checked);
	void rotateClockwiseMenuAction();
	void rotateCounterclockwiseMenuAction();
	void zoomToSelectionMenuAction();
	void toggleToolbarMenuAction(bool checked);
	void toggleSidebarMenuAction(bool checked);
	void viewPageLabelsMenuAction(bool checked);
	void documentInfoMenuAction();
	void newTabMenuAction();
	void newWindowMenuAction();
	void closeTabMenuAction();
	void closeWindowMenuAction();
	void openErrorWindowMenuAction();
	void helpMenuAction();
	void keyBindingsMenuAction();
	void aboutMenuAction();

	void popupMenuAction(int idx);

	void toggleSidebarButtonPressed();
	void pageNumberChanged();
	void backButtonPressed();
	void forwardButtonPressed();
	void zoomOutButtonPressed();
	void zoomInButtonPressed();
	void zoomIndexChanged(int idx);
	void zoomEditingFinished();
	void fitWidthButtonPressed();
	void fitPageButtonPressed();
	void selectModeButtonPressed();
	void statusIndicatorPressed();
	void findTextChanged();
	void findNextButtonPressed();
	void findPrevButtonPressed();
	void newTabButtonPressed();

	void switchTab(QListWidgetItem* current, QListWidgetItem* previous);
	void tabsReordered(const QModelIndex& srcParent, int srcStart, int srcEnd, const QModelIndex& destParent, int destRow);
	void infoComboBoxChanged(int idx);
	void outlineItemClicked(const QModelIndex& idx);
	void layerItemClicked(const QModelIndex& idx);
	void attachmentSaveClicked(int idx);

	void clearFindError();

private:
	friend class XpdfErrorWindow;

	//--- commands
	int  mouseX(QInputEvent* event);
	int  mouseY(QInputEvent* event);
	void cmdAbout(const VEC_STR& args, QInputEvent* event);
	void cmdBlockSelectMode(const VEC_STR& args, QInputEvent* event);
	void cmdCheckOpenFile(const VEC_STR& args, QInputEvent* event);
	void cmdCheckOpenFileAtDest(const VEC_STR& args, QInputEvent* event);
	void cmdCheckOpenFileAtPage(const VEC_STR& args, QInputEvent* event);
	void cmdCloseTabOrQuit(const VEC_STR& args, QInputEvent* event);
	void cmdCloseSidebar(const VEC_STR& args, QInputEvent* event);
	void cmdCloseSidebarMoveResizeWin(const VEC_STR& args, QInputEvent* event);
	void cmdCloseSidebarResizeWin(const VEC_STR& args, QInputEvent* event);
	void cmdCloseWindowOrQuit(const VEC_STR& args, QInputEvent* event);
	void cmdContinuousMode(const VEC_STR& args, QInputEvent* event);
	void cmdCopy(const VEC_STR& args, QInputEvent* event);
	void cmdCopyLinkTarget(const VEC_STR& args, QInputEvent* event);
	void cmdEndPan(const VEC_STR& args, QInputEvent* event);
	void cmdEndSelection(const VEC_STR& args, QInputEvent* event);
	void cmdExpandSidebar(const VEC_STR& args, QInputEvent* event);
	void cmdFind(const VEC_STR& args, QInputEvent* event);
	void cmdFindFirst(const VEC_STR& args, QInputEvent* event);
	void cmdFindNext(const VEC_STR& args, QInputEvent* event);
	void cmdFindPrevious(const VEC_STR& args, QInputEvent* event);
	void cmdFocusToDocWin(const VEC_STR& args, QInputEvent* event);
	void cmdFocusToPageNum(const VEC_STR& args, QInputEvent* event);
	void cmdFollowLink(const VEC_STR& args, QInputEvent* event);
	void cmdFollowLinkInNewTab(const VEC_STR& args, QInputEvent* event);
	void cmdFollowLinkInNewTabNoSel(const VEC_STR& args, QInputEvent* event);
	void cmdFollowLinkInNewWin(const VEC_STR& args, QInputEvent* event);
	void cmdFollowLinkInNewWinNoSel(const VEC_STR& args, QInputEvent* event);
	void cmdFollowLinkNoSel(const VEC_STR& args, QInputEvent* event);
	void cmdFullScreenMode(const VEC_STR& args, QInputEvent* event);
	void cmdGoBackward(const VEC_STR& args, QInputEvent* event);
	void cmdGoForward(const VEC_STR& args, QInputEvent* event);
	void cmdGotoDest(const VEC_STR& args, QInputEvent* event);
	void cmdGotoLastPage(const VEC_STR& args, QInputEvent* event);
	void cmdGotoPage(const VEC_STR& args, QInputEvent* event);
	void cmdHelp(const VEC_STR& args, QInputEvent* event);
	void cmdHideMenuBar(const VEC_STR& args, QInputEvent* event);
	void cmdHideToolbar(const VEC_STR& args, QInputEvent* event);
	void cmdHorizontalContinuousMode(const VEC_STR& args, QInputEvent* event);
	void cmdLinearSelectMode(const VEC_STR& args, QInputEvent* event);
	void cmdLoadSession(const VEC_STR& args, QInputEvent* event);
	void cmdLoadTabState(const VEC_STR& args, QInputEvent* event);
	void cmdNewTab(const VEC_STR& args, QInputEvent* event);
	void cmdNewWindow(const VEC_STR& args, QInputEvent* event);
	void cmdNextPage(const VEC_STR& args, QInputEvent* event);
	void cmdNextPageNoScroll(const VEC_STR& args, QInputEvent* event);
	void cmdNextTab(const VEC_STR& args, QInputEvent* event);
	void cmdOpen(const VEC_STR& args, QInputEvent* event);
	void cmdOpenErrorWindow(const VEC_STR& args, QInputEvent* event);
	void cmdOpenFile(const VEC_STR& args, QInputEvent* event);
	void cmdOpenFile2(const VEC_STR& args, QInputEvent* event);
	void cmdOpenFileAtDest(const VEC_STR& args, QInputEvent* event);
	void cmdOpenFileAtDestIn(const VEC_STR& args, QInputEvent* event);
	void cmdOpenFileAtPage(const VEC_STR& args, QInputEvent* event);
	void cmdOpenFileAtPageIn(const VEC_STR& args, QInputEvent* event);
	void cmdOpenFileIn(const VEC_STR& args, QInputEvent* event);
	void cmdOpenIn(const VEC_STR& args, QInputEvent* event);
	void cmdOpenSidebar(const VEC_STR& args, QInputEvent* event);
	void cmdOpenSidebarMoveResizeWin(const VEC_STR& args, QInputEvent* event);
	void cmdOpenSidebarResizeWin(const VEC_STR& args, QInputEvent* event);
	void cmdPageDown(const VEC_STR& args, QInputEvent* event);
	void cmdPageUp(const VEC_STR& args, QInputEvent* event);
	void cmdPostPopupMenu(const VEC_STR& args, QInputEvent* event);
	void cmdPrevPage(const VEC_STR& args, QInputEvent* event);
	void cmdPrevPageNoScroll(const VEC_STR& args, QInputEvent* event);
	void cmdPrevTab(const VEC_STR& args, QInputEvent* event);
#if XPDFWIDGET_PRINTING
	void cmdPrint(const VEC_STR& args, QInputEvent* event);
#endif
	void cmdQuit(const VEC_STR& args, QInputEvent* event);
	void cmdRaise(const VEC_STR& args, QInputEvent* event);
	void cmdReload(const VEC_STR& args, QInputEvent* event);
	void cmdRotateCW(const VEC_STR& args, QInputEvent* event);
	void cmdRotateCCW(const VEC_STR& args, QInputEvent* event);
	void cmdRun(const VEC_STR& args, QInputEvent* event);
	void cmdSaveAs(const VEC_STR& args, QInputEvent* event);
	void cmdSaveImage(const VEC_STR& args, QInputEvent* event);
	void cmdSaveSession(const VEC_STR& args, QInputEvent* event);
	void cmdSaveTabState(const VEC_STR& args, QInputEvent* event);
	void cmdScrollDown(const VEC_STR& args, QInputEvent* event);
	void cmdScrollDownNextPage(const VEC_STR& args, QInputEvent* event);
	void cmdScrollLeft(const VEC_STR& args, QInputEvent* event);
	void cmdScrollOutlineDown(const VEC_STR& args, QInputEvent* event);
	void cmdScrollOutlineUp(const VEC_STR& args, QInputEvent* event);
	void cmdScrollRight(const VEC_STR& args, QInputEvent* event);
	void cmdScrollToBottomEdge(const VEC_STR& args, QInputEvent* event);
	void cmdScrollToBottomRight(const VEC_STR& args, QInputEvent* event);
	void cmdScrollToLeftEdge(const VEC_STR& args, QInputEvent* event);
	void cmdScrollToRightEdge(const VEC_STR& args, QInputEvent* event);
	void cmdScrollToTopEdge(const VEC_STR& args, QInputEvent* event);
	void cmdScrollToTopLeft(const VEC_STR& args, QInputEvent* event);
	void cmdScrollUp(const VEC_STR& args, QInputEvent* event);
	void cmdScrollUpPrevPage(const VEC_STR& args, QInputEvent* event);
	void cmdSelectLine(const VEC_STR& args, QInputEvent* event);
	void cmdSelectWord(const VEC_STR& args, QInputEvent* event);
	void cmdSetSelection(const VEC_STR& args, QInputEvent* event);
	void cmdShowAttachmentsPane(const VEC_STR& args, QInputEvent* event);
	void cmdShowDocumentInfo(const VEC_STR& args, QInputEvent* event);
	void cmdShowKeyBindings(const VEC_STR& args, QInputEvent* event);
	void cmdShowLayersPane(const VEC_STR& args, QInputEvent* event);
	void cmdShowMenuBar(const VEC_STR& args, QInputEvent* event);
	void cmdShowOutlinePane(const VEC_STR& args, QInputEvent* event);
	void cmdShowToolbar(const VEC_STR& args, QInputEvent* event);
	void cmdShrinkSidebar(const VEC_STR& args, QInputEvent* event);
	void cmdSideBySideContinuousMode(const VEC_STR& args, QInputEvent* event);
	void cmdSideBySideSingleMode(const VEC_STR& args, QInputEvent* event);
	void cmdSinglePageMode(const VEC_STR& args, QInputEvent* event);
	void cmdStartExtendedSelection(const VEC_STR& args, QInputEvent* event);
	void cmdStartPan(const VEC_STR& args, QInputEvent* event);
	void cmdStartSelection(const VEC_STR& args, QInputEvent* event);
	void cmdToggleContinuousMode(const VEC_STR& args, QInputEvent* event);
	void cmdToggleFullScreenMode(const VEC_STR& args, QInputEvent* event);
	void cmdToggleMenuBar(const VEC_STR& args, QInputEvent* event);
	void cmdToggleSelectMode(const VEC_STR& args, QInputEvent* event);
	void cmdToggleSidebar(const VEC_STR& args, QInputEvent* event);
	void cmdToggleSidebarMoveResizeWin(const VEC_STR& args, QInputEvent* event);
	void cmdToggleSidebarResizeWin(const VEC_STR& args, QInputEvent* event);
	void cmdToggleToolbar(const VEC_STR& args, QInputEvent* event);
	void cmdViewPageLabels(const VEC_STR& args, QInputEvent* event);
	void cmdViewPageNumbers(const VEC_STR& args, QInputEvent* event);
	void cmdWindowMode(const VEC_STR& args, QInputEvent* event);
	void cmdZoomFitPage(const VEC_STR& args, QInputEvent* event);
	void cmdZoomFitWidth(const VEC_STR& args, QInputEvent* event);
	void cmdZoomIn(const VEC_STR& args, QInputEvent* event);
	void cmdZoomOut(const VEC_STR& args, QInputEvent* event);
	void cmdZoomPercent(const VEC_STR& args, QInputEvent* event);
	void cmdZoomToSelection(const VEC_STR& args, QInputEvent* event);
	int  getFindCaseFlag();
	int  scaleScroll(int delta);
	void followLink(QInputEvent* event, bool onlyIfNoSel, bool newTab, bool newWindow);

	//--- GUI events
	int          getModifiers(Qt::KeyboardModifiers qtMods);
	int          getContext(Qt::KeyboardModifiers qtMods);
	int          getMouseButton(Qt::MouseButton qtBtn);
	virtual void keyPressEvent(QKeyEvent* e);
	virtual void dragEnterEvent(QDragEnterEvent* e);
	virtual void dropEvent(QDropEvent* e);
	virtual bool eventFilter(QObject* watched, QEvent* event);

	//--- GUI setup
	void            createWindow();
	void            createToolBar();
	QToolButton*    addToolBarButton(const QIcon& icon, const char* slot, const char* tip);
	XpdfMenuButton* addToolBarMenuButton(const QIcon& icon, const char* tip, QMenu* menu);
	void            addToolBarSeparator();
	void            addToolBarSpacing(int w);
	void            addToolBarStretch();
	void            createMainMenu();
	void            createXpdfPopupMenu();
	QWidget*        createTabPane();
	QWidget*        createInfoPane();
	void            updateInfoPane();
	void            destroyWindow();
	void            enterFullScreenMode();
	void            exitFullScreenMode();
	void            addTab();
	void            closeTab(XpdfTabInfo* tab);
	void            gotoTab(int idx);
	void            updateModeInfo();
	void            updateZoomInfo();
	void            updateSelectModeInfo();
	void            updateDocInfo();
	void            updatePageNumberOrLabel(int pg);
	void            updateOutline(int pg);
	void            setOutlineOpenItems(const QModelIndex& idx);
	void            fillAttachmentList();
	void            statusIndicatorStart();
	void            statusIndicatorStop();
	void            statusIndicatorOk();
	void            statusIndicatorError();
	void            showFindError();
	void            createDocumentInfoDialog();
	void            updateDocumentInfoDialog(XpdfWidget* view);
	QString         createDocumentInfoMetadataHTML(XpdfWidget* view);
	QString         createDocumentInfoFontsHTML(XpdfWidget* view);
	void            createKeyBindingsDialog();
	QString         createKeyBindingsHTML();
	void            createAboutDialog();
	void            execSaveImageDialog();

	static XpdfViewerCmd cmdTab[];

	XpdfApp* app;

	// menu
	QMenuBar* mainMenu;
	QMenu*    displayModeSubmenu;
	QAction*  fullScreenMenuItem;
	QAction*  toggleToolbarMenuItem;
	QAction*  toggleSidebarMenuItem;
	QAction*  viewPageLabelsMenuItem;

	// popup menu
	QMenu*         popupMenu;
	QSignalMapper* popupMenuSignalMapper;

	// toolbar
	int                    toolBarFontSize; // used for HiDPI scaling
	QToolBar*              toolBar;
	QLineEdit*             pageNumber;
	QLabel*                pageCount;
	QComboBox*             zoomComboBox;
	QToolButton*           fitWidthBtn;
	QToolButton*           fitPageBtn;
	QToolButton*           selectModeBtn;
	PropertyListAnimation* indicatorAnimation;
	QList<QVariant>        indicatorIcons;
	QList<QVariant>        indicatorErrIcons;
	QLineEdit*             findEdit;
	QAction*               findCaseInsensitiveAction;
	QAction*               findCaseSensitiveAction;
	QAction*               findSmartCaseAction;
	QAction*               findWholeWordsAction;

	// sidebar pane
	QSplitter*      sidebarSplitter;
	int             initialSidebarWidth;
	int             sidebarWidth;
	QListWidget*    tabList;
	QComboBox*      infoComboBox;
	QStackedLayout* infoStack;

	// viewer pane
	QStackedWidget* viewerStack;

	QLabel* linkTargetBar;
	QString linkTargetInfo;

	GList*       tabInfo; // [XpdfTabInfo]
	XpdfTabInfo* currentTab;
	XpdfTabInfo* lastOpenedTab;

	double scaleFactor;

	XpdfWidget::DisplayMode fullScreenPreviousDisplayMode;
	double                  fullScreenPreviousZoom;

	QTimer*          findErrorTimer;
	XpdfErrorWindow* errorWindow;
	QDialog*         documentInfoDialog;
	QTextBrowser*    documentInfoMetadataTab;
	QTextBrowser*    documentInfoFontsTab;
	QDialog*         keyBindingsDialog;
	QDialog*         aboutDialog;
#if XPDFWIDGET_PRINTING
	QProgressDialog* printStatusDialog;
#endif

	QString       lastFileOpened;
	QLocalServer* remoteServer;
};
