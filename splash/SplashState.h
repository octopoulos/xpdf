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

	SplashCoord            matrix[6]          = {};                    //
	SplashPattern*         strokePattern      = nullptr;               //
	SplashPattern*         fillPattern        = nullptr;               //
	SplashScreen*          screen             = nullptr;               //
	SplashBlendFunc        blendFunc          = nullptr;               //
	SplashCoord            strokeAlpha        = 1;                     //
	SplashCoord            fillAlpha          = 1;                     //
	SplashCoord            lineWidth          = 1;                     //
	int                    lineCap            = splashLineCapButt;     //
	int                    lineJoin           = splashLineJoinMiter;   //
	SplashCoord            miterLimit         = 10;                    //
	SplashCoord            flatness           = 1;                     //
	SplashCoord*           lineDash           = nullptr;               //
	int                    lineDashLength     = 0;                     //
	SplashCoord            lineDashPhase      = 0;                     //
	SplashStrokeAdjustMode strokeAdjust       = splashStrokeAdjustOff; //
	SplashClip*            clip               = nullptr;               //
	bool                   clipIsShared       = false;                 //
	SplashBitmap*          softMask           = nullptr;               //
	bool                   deleteSoftMask     = false;                 //
	bool                   inNonIsolatedGroup = false;                 //
	bool                   inKnockoutGroup    = false;                 //
	uint8_t*               rgbTransferR       = nullptr;               //
	uint8_t*               rgbTransferG       = nullptr;               //
	uint8_t*               rgbTransferB       = nullptr;               //
	uint8_t*               grayTransfer       = nullptr;               //
#if SPLASH_CMYK
	uint8_t* cmykTransferC = nullptr;
	uint8_t* cmykTransferM = nullptr;
	uint8_t* cmykTransferY = nullptr;
	uint8_t* cmykTransferK = nullptr;
#endif
	bool         transferIsShared         = false;      //
	uint32_t     overprintMask            = 0xffffffff; //
	bool         enablePathSimplification = false;      //
	SplashState* next                     = nullptr;    // used by Splash class

	friend class Splash;
};
