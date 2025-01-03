//========================================================================
//
// SplashState.h
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include "SplashTypes.h"

class SplashPattern;
class SplashScreen;
class SplashClip;
class SplashBitmap;
class SplashPath;

//------------------------------------------------------------------------
// line cap values
//------------------------------------------------------------------------

#define splashLineCapButt       0
#define splashLineCapRound      1
#define splashLineCapProjecting 2

//------------------------------------------------------------------------
// line join values
//------------------------------------------------------------------------

#define splashLineJoinMiter 0
#define splashLineJoinRound 1
#define splashLineJoinBevel 2

//------------------------------------------------------------------------
// SplashState
//------------------------------------------------------------------------

class SplashState
{
public:
	// Create a new state object, initialized with default settings.
	SplashState(int width, int height, bool vectorAntialias, SplashScreenParams* screenParams);
	SplashState(int width, int height, bool vectorAntialias, SplashScreen* screenA);

	// Copy a state object.
	SplashState* copy() { return new SplashState(this); }

	~SplashState();

	// Set the stroke pattern.  This does not copy <strokePatternA>.
	void setStrokePattern(SplashPattern* strokePatternA);

	// Set the fill pattern.  This does not copy <fillPatternA>.
	void setFillPattern(SplashPattern* fillPatternA);

	// Set the screen.  This does not copy <screenA>.
	void setScreen(SplashScreen* screenA);

	// Set the line dash pattern.  This copies the <lineDashA> array.
	void setLineDash(SplashCoord* lineDashA, int lineDashLengthA, SplashCoord lineDashPhaseA);

	// Returns true if the current line dash pattern contains one or
	// more zero-length "on" sections (dashes).
	bool lineDashContainsZeroLengthDashes();

	void        clipResetToRect(SplashCoord x0, SplashCoord y0, SplashCoord x1, SplashCoord y1);
	SplashError clipToRect(SplashCoord x0, SplashCoord y0, SplashCoord x1, SplashCoord y1);
	SplashError clipToPath(SplashPath* path, bool eo);

	// Set the soft mask bitmap.
	void setSoftMask(SplashBitmap* softMaskA, bool deleteBitmap = true);

	// Set the transfer function.
	void setTransfer(uint8_t* red, uint8_t* green, uint8_t* blue, uint8_t* gray);

private:
	SplashState(SplashState* state);

	SplashCoord            matrix[6];          //
	SplashPattern*         strokePattern;      //
	SplashPattern*         fillPattern;        //
	SplashScreen*          screen;             //
	SplashBlendFunc        blendFunc;          //
	SplashCoord            strokeAlpha;        //
	SplashCoord            fillAlpha;          //
	SplashCoord            lineWidth;          //
	int                    lineCap;            //
	int                    lineJoin;           //
	SplashCoord            miterLimit;         //
	SplashCoord            flatness;           //
	SplashCoord*           lineDash;           //
	int                    lineDashLength;     //
	SplashCoord            lineDashPhase;      //
	SplashStrokeAdjustMode strokeAdjust;       //
	SplashClip*            clip;               //
	bool                   clipIsShared;       //
	SplashBitmap*          softMask;           //
	bool                   deleteSoftMask;     //
	bool                   inNonIsolatedGroup; //
	bool                   inKnockoutGroup;    //
	uint8_t*               rgbTransferR;       //
	uint8_t*               rgbTransferG;       //
	uint8_t*               rgbTransferB;       //
	uint8_t*               grayTransfer;       //
#if SPLASH_CMYK
	uint8_t* cmykTransferC;
	uint8_t* cmykTransferM;
	uint8_t* cmykTransferY;
	uint8_t* cmykTransferK;
#endif
	bool         transferIsShared;         //
	uint32_t     overprintMask;            //
	bool         enablePathSimplification; //
	SplashState* next;                     // used by Splash class

	friend class Splash;
};
