//========================================================================
//
// DisplayState.cc
//
// Copyright 2014 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include <stdlib.h>
#include "gmempp.h"
#include "GList.h"
#include "TileMap.h"
#include "TileCache.h"
#include "TileCompositor.h"
#include "DisplayState.h"

//------------------------------------------------------------------------
// DisplayState
//------------------------------------------------------------------------

DisplayState::DisplayState(int maxTileWidthA, int maxTileHeightA, int tileCacheSizeA, int nWorkerThreadsA, SplashColorMode colorModeA, int bitmapRowPadA)
{
	maxTileWidth   = maxTileWidthA;
	maxTileHeight  = maxTileHeightA;
	tileCacheSize  = tileCacheSizeA;
	nWorkerThreads = nWorkerThreadsA;
	colorMode      = colorModeA;
	bitmapRowPad   = bitmapRowPadA;

	for (int i = 0; i < splashColorModeNComps[colorMode]; ++i)
	{
		paperColor[i] = 0xff;
		matteColor[i] = 0x80;
	}
	if (colorMode == splashModeRGB8 || colorMode == splashModeBGR8)
	{
		selectColor[0] = 0x80;
		selectColor[1] = 0x80;
		selectColor[2] = 0xff;
	}
	else
	{
		for (int i = 0; i < splashColorModeNComps[colorMode]; ++i)
			selectColor[i] = 0xa0;
	}
}

DisplayState::~DisplayState()
{
}

void DisplayState::setPaperColor(SplashColorPtr paperColorA)
{
	splashColorCopy(paperColor, paperColorA);
	tileCache->paperColorChanged();
	tileCompositor->paperColorChanged();
}

void DisplayState::setMatteColor(SplashColorPtr matteColorA)
{
	splashColorCopy(matteColor, matteColorA);
	tileCompositor->matteColorChanged();
}

void DisplayState::setSelectColor(SplashColorPtr selectColorA)
{
	splashColorCopy(selectColor, selectColorA);
	tileCompositor->selectColorChanged();
}

void DisplayState::setReverseVideo(bool reverseVideoA)
{
	if (reverseVideo != reverseVideoA)
	{
		reverseVideo = reverseVideoA;
		tileCache->reverseVideoChanged();
		tileCompositor->reverseVideoChanged();
	}
}

void DisplayState::setDoc(PDFDoc* docA)
{
	doc = docA;
	tileMap->docChanged();
	tileCache->docChanged();
	tileCompositor->docChanged();
}

void DisplayState::setWindowSize(int winWA, int winHA)
{
	if (winW != winWA || winH != winHA)
	{
		winW = winWA;
		winH = winHA;
		tileMap->windowSizeChanged();
		tileCompositor->windowSizeChanged();
	}
}

void DisplayState::setDisplayMode(DisplayMode displayModeA)
{
	if (displayMode != displayModeA)
	{
		displayMode = displayModeA;
		tileMap->displayModeChanged();
		tileCompositor->displayModeChanged();
	}
}

void DisplayState::setZoom(double zoomA)
{
	if (zoom != zoomA)
	{
		zoom = zoomA;
		tileMap->zoomChanged();
		tileCompositor->zoomChanged();
	}
}

void DisplayState::setRotate(int rotateA)
{
	if (rotate != rotateA)
	{
		rotate = rotateA;
		tileMap->rotateChanged();
		tileCompositor->rotateChanged();
	}
}

void DisplayState::setScrollPosition(int scrollPageA, int scrollXA, int scrollYA)
{
	if (scrollPage != scrollPageA || scrollX != scrollXA || scrollY != scrollYA)
	{
		scrollPage = scrollPageA;
		scrollX    = scrollXA;
		scrollY    = scrollYA;
		tileMap->scrollPositionChanged();
		tileCompositor->scrollPositionChanged();
	}
}

void DisplayState::setSelection(int selectPage, double selectX0, double selectY0, double selectX1, double selectY1)
{
	SelectRect rect(selectPage, selectX0, selectY0, selectX1, selectY1);
	std::vector<SelectRect> rects;
	rects.push_back(rect);
	setSelection(rects);
}

void DisplayState::setSelection(std::vector<SelectRect>& selectRectsA)
{
	if (selectRects.empty() && selectRectsA.empty())
		return;
	if (selectRects.size() && selectRectsA.size() && selectRects.size() == selectRectsA.size())
	{
		int i;
		for (i = 0; i < selectRects.size(); ++i)
		{
			auto& rect  = selectRects.at(i);
			auto& rectA = selectRectsA.at(i);
			if (rect != rectA)
				break;
		}
		if (i == selectRects.size())
		{
			selectRectsA.clear();
			return;
		}
	}

	selectRects = selectRectsA;
	tileCompositor->selectionChanged();
}

void DisplayState::clearSelection()
{
	std::vector<SelectRect> rects;
	setSelection(rects);
}

int DisplayState::getNumSelectRects()
{
	if (selectRects.empty()) return 0;
	return (int)selectRects.size();
}

SelectRect* DisplayState::getSelectRect(int idx)
{
	return &selectRects.at(idx);
}

void DisplayState::optionalContentChanged()
{
	tileCache->optionalContentChanged();
	tileCompositor->optionalContentChanged();
}

void DisplayState::forceRedraw()
{
	tileMap->forceRedraw();
	tileCache->forceRedraw();
	tileCompositor->forceRedraw();
}
