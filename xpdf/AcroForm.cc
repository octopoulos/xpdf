//========================================================================
//
// AcroForm.cc
//
// Copyright 2012 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include <stdlib.h>
#include <math.h>
#include "gmem.h"
#include "gmempp.h"
#include "GList.h"
#include "GlobalParams.h"
#include "Error.h"
#include "Object.h"
#include "PDFDoc.h"
#include "TextString.h"
#include "Gfx.h"
#include "GfxFont.h"
#include "OptionalContent.h"
#include "Annot.h"
#include "Lexer.h"
#include "XFAScanner.h"
#include "UTF8.h"
#include "PDF417Barcode.h"
#include "AcroForm.h"

//------------------------------------------------------------------------

#define acroFormFlagReadOnly          (1 << 0)  // all
#define acroFormFlagRequired          (1 << 1)  // all
#define acroFormFlagNoExport          (1 << 2)  // all
#define acroFormFlagMultiline         (1 << 12) // text
#define acroFormFlagPassword          (1 << 13) // text
#define acroFormFlagNoToggleToOff     (1 << 14) // button
#define acroFormFlagRadio             (1 << 15) // button
#define acroFormFlagPushbutton        (1 << 16) // button
#define acroFormFlagCombo             (1 << 17) // choice
#define acroFormFlagEdit              (1 << 18) // choice
#define acroFormFlagSort              (1 << 19) // choice
#define acroFormFlagFileSelect        (1 << 20) // text
#define acroFormFlagMultiSelect       (1 << 21) // choice
#define acroFormFlagDoNotSpellCheck   (1 << 22) // text, choice
#define acroFormFlagDoNotScroll       (1 << 23) // text
#define acroFormFlagComb              (1 << 24) // text
#define acroFormFlagRadiosInUnison    (1 << 25) // button
#define acroFormFlagRichText          (1 << 25) // text
#define acroFormFlagCommitOnSelChange (1 << 26) // choice

#define acroFormQuadLeft   0
#define acroFormQuadCenter 1
#define acroFormQuadRight  2

#define acroFormVAlignTop               0
#define acroFormVAlignMiddle            1
#define acroFormVAlignMiddleNoDescender 2
#define acroFormVAlignBottom            3

#define annotFlagHidden 0x0002
#define annotFlagPrint  0x0004
#define annotFlagNoView 0x0020

// distance of Bezier control point from center for circle approximation
// = (4 * (sqrt(2) - 1) / 3) * r
#define bezierCircle 0.55228475

// limit recursive field-parent lookups to avoid infinite loops
#define maxFieldObjectDepth 50

//------------------------------------------------------------------------

// 5 bars + 5 spaces -- each can be wide (1) or narrow (0)
// (there are always exactly 3 wide elements;
// the last space is always narrow)
static uint8_t code3Of9Data[128][10] = {
	{0,  0, 0, 0, 0, 0, 0, 0, 0, 0}, // 0x00
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 0x10
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 1, 1, 0, 0, 0, 1, 0, 0, 0}, // ' ' = 0x20
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 1, 0, 1, 0, 1, 0, 0, 0, 0}, // '$' = 0x24
	{ 0, 0, 0, 1, 0, 1, 0, 1, 0, 0}, // '%' = 0x25
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 1, 0, 0, 1, 0, 1, 0, 0, 0}, // '*' = 0x2a
	{ 0, 1, 0, 0, 0, 1, 0, 1, 0, 0}, // '+' = 0x2b
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 1, 0, 0, 0, 0, 1, 0, 1, 0}, // '-' = 0x2d
	{ 1, 1, 0, 0, 0, 0, 1, 0, 0, 0}, // '.' = 0x2e
	{ 0, 1, 0, 1, 0, 0, 0, 1, 0, 0}, // '/' = 0x2f
	{ 0, 0, 0, 1, 1, 0, 1, 0, 0, 0}, // '0' = 0x30
	{ 1, 0, 0, 1, 0, 0, 0, 0, 1, 0}, // '1'
	{ 0, 0, 1, 1, 0, 0, 0, 0, 1, 0}, // '2'
	{ 1, 0, 1, 1, 0, 0, 0, 0, 0, 0}, // '3'
	{ 0, 0, 0, 1, 1, 0, 0, 0, 1, 0}, // '4'
	{ 1, 0, 0, 1, 1, 0, 0, 0, 0, 0}, // '5'
	{ 0, 0, 1, 1, 1, 0, 0, 0, 0, 0}, // '6'
	{ 0, 0, 0, 1, 0, 0, 1, 0, 1, 0}, // '7'
	{ 1, 0, 0, 1, 0, 0, 1, 0, 0, 0}, // '8'
	{ 0, 0, 1, 1, 0, 0, 1, 0, 0, 0}, // '9'
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 0x40
	{ 1, 0, 0, 0, 0, 1, 0, 0, 1, 0}, // 'A' = 0x41
	{ 0, 0, 1, 0, 0, 1, 0, 0, 1, 0}, // 'B'
	{ 1, 0, 1, 0, 0, 1, 0, 0, 0, 0}, // 'C'
	{ 0, 0, 0, 0, 1, 1, 0, 0, 1, 0}, // 'D'
	{ 1, 0, 0, 0, 1, 1, 0, 0, 0, 0}, // 'E'
	{ 0, 0, 1, 0, 1, 1, 0, 0, 0, 0}, // 'F'
	{ 0, 0, 0, 0, 0, 1, 1, 0, 1, 0}, // 'G'
	{ 1, 0, 0, 0, 0, 1, 1, 0, 0, 0}, // 'H'
	{ 0, 0, 1, 0, 0, 1, 1, 0, 0, 0}, // 'I'
	{ 0, 0, 0, 0, 1, 1, 1, 0, 0, 0}, // 'J'
	{ 1, 0, 0, 0, 0, 0, 0, 1, 1, 0}, // 'K'
	{ 0, 0, 1, 0, 0, 0, 0, 1, 1, 0}, // 'L'
	{ 1, 0, 1, 0, 0, 0, 0, 1, 0, 0}, // 'M'
	{ 0, 0, 0, 0, 1, 0, 0, 1, 1, 0}, // 'N'
	{ 1, 0, 0, 0, 1, 0, 0, 1, 0, 0}, // 'O'
	{ 0, 0, 1, 0, 1, 0, 0, 1, 0, 0}, // 'P' = 0x50
	{ 0, 0, 0, 0, 0, 0, 1, 1, 1, 0}, // 'Q'
	{ 1, 0, 0, 0, 0, 0, 1, 1, 0, 0}, // 'R'
	{ 0, 0, 1, 0, 0, 0, 1, 1, 0, 0}, // 'S'
	{ 0, 0, 0, 0, 1, 0, 1, 1, 0, 0}, // 'T'
	{ 1, 1, 0, 0, 0, 0, 0, 0, 1, 0}, // 'U'
	{ 0, 1, 1, 0, 0, 0, 0, 0, 1, 0}, // 'V'
	{ 1, 1, 1, 0, 0, 0, 0, 0, 0, 0}, // 'W'
	{ 0, 1, 0, 0, 1, 0, 0, 0, 1, 0}, // 'X'
	{ 1, 1, 0, 0, 1, 0, 0, 0, 0, 0}, // 'Y'
	{ 0, 1, 1, 0, 1, 0, 0, 0, 0, 0}, // 'Z'
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 0x60
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 0x70
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

// 3 bars + 3 spaces -- each can be 1, 2, 3, or 4 units wide
static uint8_t code128Data[107][6] = {
	{2,  1, 2, 2, 2, 2},
	{ 2, 2, 2, 1, 2, 2},
	{ 2, 2, 2, 2, 2, 1},
	{ 1, 2, 1, 2, 2, 3},
	{ 1, 2, 1, 3, 2, 2},
	{ 1, 3, 1, 2, 2, 2},
	{ 1, 2, 2, 2, 1, 3},
	{ 1, 2, 2, 3, 1, 2},
	{ 1, 3, 2, 2, 1, 2},
	{ 2, 2, 1, 2, 1, 3},
	{ 2, 2, 1, 3, 1, 2},
	{ 2, 3, 1, 2, 1, 2},
	{ 1, 1, 2, 2, 3, 2},
	{ 1, 2, 2, 1, 3, 2},
	{ 1, 2, 2, 2, 3, 1},
	{ 1, 1, 3, 2, 2, 2},
	{ 1, 2, 3, 1, 2, 2},
	{ 1, 2, 3, 2, 2, 1},
	{ 2, 2, 3, 2, 1, 1},
	{ 2, 2, 1, 1, 3, 2},
	{ 2, 2, 1, 2, 3, 1},
	{ 2, 1, 3, 2, 1, 2},
	{ 2, 2, 3, 1, 1, 2},
	{ 3, 1, 2, 1, 3, 1},
	{ 3, 1, 1, 2, 2, 2},
	{ 3, 2, 1, 1, 2, 2},
	{ 3, 2, 1, 2, 2, 1},
	{ 3, 1, 2, 2, 1, 2},
	{ 3, 2, 2, 1, 1, 2},
	{ 3, 2, 2, 2, 1, 1},
	{ 2, 1, 2, 1, 2, 3},
	{ 2, 1, 2, 3, 2, 1},
	{ 2, 3, 2, 1, 2, 1},
	{ 1, 1, 1, 3, 2, 3},
	{ 1, 3, 1, 1, 2, 3},
	{ 1, 3, 1, 3, 2, 1},
	{ 1, 1, 2, 3, 1, 3},
	{ 1, 3, 2, 1, 1, 3},
	{ 1, 3, 2, 3, 1, 1},
	{ 2, 1, 1, 3, 1, 3},
	{ 2, 3, 1, 1, 1, 3},
	{ 2, 3, 1, 3, 1, 1},
	{ 1, 1, 2, 1, 3, 3},
	{ 1, 1, 2, 3, 3, 1},
	{ 1, 3, 2, 1, 3, 1},
	{ 1, 1, 3, 1, 2, 3},
	{ 1, 1, 3, 3, 2, 1},
	{ 1, 3, 3, 1, 2, 1},
	{ 3, 1, 3, 1, 2, 1},
	{ 2, 1, 1, 3, 3, 1},
	{ 2, 3, 1, 1, 3, 1},
	{ 2, 1, 3, 1, 1, 3},
	{ 2, 1, 3, 3, 1, 1},
	{ 2, 1, 3, 1, 3, 1},
	{ 3, 1, 1, 1, 2, 3},
	{ 3, 1, 1, 3, 2, 1},
	{ 3, 3, 1, 1, 2, 1},
	{ 3, 1, 2, 1, 1, 3},
	{ 3, 1, 2, 3, 1, 1},
	{ 3, 3, 2, 1, 1, 1},
	{ 3, 1, 4, 1, 1, 1},
	{ 2, 2, 1, 4, 1, 1},
	{ 4, 3, 1, 1, 1, 1},
	{ 1, 1, 1, 2, 2, 4},
	{ 1, 1, 1, 4, 2, 2},
	{ 1, 2, 1, 1, 2, 4},
	{ 1, 2, 1, 4, 2, 1},
	{ 1, 4, 1, 1, 2, 2},
	{ 1, 4, 1, 2, 2, 1},
	{ 1, 1, 2, 2, 1, 4},
	{ 1, 1, 2, 4, 1, 2},
	{ 1, 2, 2, 1, 1, 4},
	{ 1, 2, 2, 4, 1, 1},
	{ 1, 4, 2, 1, 1, 2},
	{ 1, 4, 2, 2, 1, 1},
	{ 2, 4, 1, 2, 1, 1},
	{ 2, 2, 1, 1, 1, 4},
	{ 4, 1, 3, 1, 1, 1},
	{ 2, 4, 1, 1, 1, 2},
	{ 1, 3, 4, 1, 1, 1},
	{ 1, 1, 1, 2, 4, 2},
	{ 1, 2, 1, 1, 4, 2},
	{ 1, 2, 1, 2, 4, 1},
	{ 1, 1, 4, 2, 1, 2},
	{ 1, 2, 4, 1, 1, 2},
	{ 1, 2, 4, 2, 1, 1},
	{ 4, 1, 1, 2, 1, 2},
	{ 4, 2, 1, 1, 1, 2},
	{ 4, 2, 1, 2, 1, 1},
	{ 2, 1, 2, 1, 4, 1},
	{ 2, 1, 4, 1, 2, 1},
	{ 4, 1, 2, 1, 2, 1},
	{ 1, 1, 1, 1, 4, 3},
	{ 1, 1, 1, 3, 4, 1},
	{ 1, 3, 1, 1, 4, 1},
	{ 1, 1, 4, 1, 1, 3},
	{ 1, 1, 4, 3, 1, 1},
	{ 4, 1, 1, 1, 1, 3},
	{ 4, 1, 1, 3, 1, 1},
	{ 1, 1, 3, 1, 4, 1},
	{ 1, 1, 4, 1, 3, 1},
	{ 3, 1, 1, 1, 4, 1},
	{ 4, 1, 1, 1, 3, 1},
	{ 2, 1, 1, 4, 1, 2}, // start code A
	{ 2, 1, 1, 2, 1, 4}, // start code B
	{ 2, 1, 1, 2, 3, 2}, // start code C
	{ 2, 3, 3, 1, 1, 1}  // stop code (without final bar)
};

//------------------------------------------------------------------------

// map an annotation ref to a page number
class AcroFormAnnotPage
{
public:
	AcroFormAnnotPage(int annotNumA, int annotGenA, int pageNumA)
	{
		annotNum = annotNumA;
		annotGen = annotGenA;
		pageNum  = pageNumA;
	}

	int annotNum;
	int annotGen;
	int pageNum;
};

//------------------------------------------------------------------------
// AcroForm
//------------------------------------------------------------------------

AcroForm* AcroForm::load(PDFDoc* docA, Catalog* catalog, Object* acroFormObjA)
{
	Object         acroFormObj2;
	AcroForm*      acroForm;
	AcroFormField* field;
	Object         xfaObj, fieldsObj, annotsObj, annotRef, annotObj, obj1, obj2;
	char*          touchedObjs;
	int            pageNum, i, j;

	touchedObjs = (char*)gmalloc(docA->getXRef()->getNumObjects());
	memset(touchedObjs, 0, docA->getXRef()->getNumObjects());

	// this is the normal case: acroFormObj is a dictionary, as expected
	if (acroFormObjA->isDict())
	{
		acroForm = new AcroForm(docA, acroFormObjA);

		if (globalParams->getEnableXFA())
		{
			if (!acroFormObjA->dictLookup("XFA", &xfaObj)->isNull())
			{
				acroForm->xfaScanner = XFAScanner::load(&xfaObj);
				if (!catalog->getNeedsRendering())
					acroForm->isStaticXFA = true;
			}
			xfaObj.free();
		}

		if (acroFormObjA->dictLookup("NeedAppearances", &obj1)->isBool())
			acroForm->needAppearances = obj1.getBool();
		obj1.free();

		acroForm->buildAnnotPageList(catalog);

		if (!acroFormObjA->dictLookup("Fields", &obj1)->isArray())
		{
			if (!obj1.isNull()) error(errSyntaxError, -1, "AcroForm Fields entry is wrong type");
			obj1.free();
			delete acroForm;
			gfree(touchedObjs);
			return nullptr;
		}
		for (i = 0; i < obj1.arrayGetLength(); ++i)
		{
			obj1.arrayGetNF(i, &obj2);
			acroForm->scanField(&obj2, touchedObjs);
			obj2.free();
		}
		obj1.free();

		// scan the annotations, looking for Widget-type annots that are
		// not attached to the AcroForm object
		for (pageNum = 1; pageNum <= catalog->getNumPages(); ++pageNum)
		{
			if (catalog->getPage(pageNum)->getAnnots(&annotsObj)->isArray())
			{
				for (i = 0; i < annotsObj.arrayGetLength(); ++i)
				{
					if (annotsObj.arrayGetNF(i, &annotRef)->isRef())
					{
						for (j = 0; j < acroForm->fields->getLength(); ++j)
						{
							field = (AcroFormField*)acroForm->fields->get(j);
							if (field->fieldRef.isRef())
							{
								if (field->fieldRef.getRefNum() == annotRef.getRefNum() && field->fieldRef.getRefGen() == annotRef.getRefGen())
									break;
							}
						}
						if (j == acroForm->fields->getLength())
						{
							annotRef.fetch(acroForm->doc->getXRef(), &annotObj);
							if (annotObj.isDict())
							{
								if (annotObj.dictLookup("Subtype", &obj1)->isName("Widget"))
									acroForm->scanField(&annotRef, touchedObjs);
								obj1.free();
							}
							annotObj.free();
						}
					}
					annotRef.free();
				}
			}
			annotsObj.free();
		}

		// if acroFormObjA is a null object, but there are Widget-type
		// annots, we still create an AcroForm
	}
	else
	{
		// create an empty dict for acroFormObj
		acroFormObj2.initDict(docA->getXRef());
		acroForm = new AcroForm(docA, &acroFormObj2);
		acroFormObj2.free();

		acroForm->buildAnnotPageList(catalog);

		// scan the annotations, looking for any Widget-type annots
		for (pageNum = 1; pageNum <= catalog->getNumPages(); ++pageNum)
		{
			if (catalog->getPage(pageNum)->getAnnots(&annotsObj)->isArray())
			{
				for (i = 0; i < annotsObj.arrayGetLength(); ++i)
				{
					if (annotsObj.arrayGetNF(i, &annotRef)->isRef())
					{
						annotRef.fetch(acroForm->doc->getXRef(), &annotObj);
						if (annotObj.isDict())
						{
							if (annotObj.dictLookup("Subtype", &obj1)->isName("Widget"))
								acroForm->scanField(&annotRef, touchedObjs);
							obj1.free();
						}
						annotObj.free();
					}
					annotRef.free();
				}
			}
			annotsObj.free();
		}

		if (acroForm->fields->getLength() == 0)
		{
			delete acroForm;
			acroForm = nullptr;
		}
	}

	gfree(touchedObjs);

	return acroForm;
}

AcroForm::AcroForm(PDFDoc* docA, Object* acroFormObjA)
{
	doc = docA;
	acroFormObjA->copy(&acroFormObj);
	needAppearances = false;
	annotPages      = new GList();
	fields          = new GList();
}

AcroForm::~AcroForm()
{
	acroFormObj.free();
	deleteGList(annotPages, AcroFormAnnotPage);
	deleteGList(fields, AcroFormField);
	delete xfaScanner;
}

const char* AcroForm::getType()
{
	return isStaticXFA ? "static XFA" : "AcroForm";
}

void AcroForm::buildAnnotPageList(Catalog* catalog)
{
	Object annotsObj, annotObj;
	int    pageNum, i;

	for (pageNum = 1; pageNum <= catalog->getNumPages(); ++pageNum)
	{
		if (catalog->getPage(pageNum)->getAnnots(&annotsObj)->isArray())
		{
			for (i = 0; i < annotsObj.arrayGetLength(); ++i)
			{
				if (annotsObj.arrayGetNF(i, &annotObj)->isRef())
					annotPages->append(new AcroFormAnnotPage(annotObj.getRefNum(), annotObj.getRefGen(), pageNum));
				annotObj.free();
			}
		}
		annotsObj.free();
	}
	//~ sort the list
}

int AcroForm::lookupAnnotPage(Object* annotRef)
{
	AcroFormAnnotPage* annotPage;
	int                num, gen, i;

	if (!annotRef->isRef())
		return 0;
	num = annotRef->getRefNum();
	gen = annotRef->getRefGen();
	//~ use bin search
	for (i = 0; i < annotPages->getLength(); ++i)
	{
		annotPage = (AcroFormAnnotPage*)annotPages->get(i);
		if (annotPage->annotNum == num && annotPage->annotGen == gen)
			return annotPage->pageNum;
	}
	return 0;
}

void AcroForm::scanField(Object* fieldRef, char* touchedObjs)
{
	AcroFormField* field;
	Object         fieldObj, kidsObj, kidRef, kidObj, subtypeObj;
	bool           isTerminal;
	int            i;

	// check for an object loop
	if (fieldRef->isRef())
	{
		if (fieldRef->getRefNum() < 0 || fieldRef->getRefNum() >= doc->getXRef()->getNumObjects() || touchedObjs[fieldRef->getRefNum()])
			return;
		touchedObjs[fieldRef->getRefNum()] = 1;
	}

	fieldRef->fetch(doc->getXRef(), &fieldObj);
	if (!fieldObj.isDict())
	{
		error(errSyntaxError, -1, "AcroForm field object is wrong type");
		fieldObj.free();
		return;
	}

	// if this field has a Kids array, and all of the kids have a Parent
	// reference (i.e., they're all form fields, not widget
	// annotations), then this is a non-terminal field, and we need to
	// scan the kids
	isTerminal = true;
	if (fieldObj.dictLookup("Kids", &kidsObj)->isArray())
	{
		isTerminal = false;
		for (i = 0; !isTerminal && i < kidsObj.arrayGetLength(); ++i)
		{
			kidsObj.arrayGet(i, &kidObj);
			if (kidObj.isDict())
			{
				if (kidObj.dictLookup("Parent", &subtypeObj)->isNull())
					isTerminal = true;
				subtypeObj.free();
			}
			kidObj.free();
		}
		if (!isTerminal)
		{
			for (i = 0; !isTerminal && i < kidsObj.arrayGetLength(); ++i)
			{
				kidsObj.arrayGetNF(i, &kidRef);
				scanField(&kidRef, touchedObjs);
				kidRef.free();
			}
		}
	}
	kidsObj.free();

	if (isTerminal)
	{
		if ((field = AcroFormField::load(this, fieldRef)))
			fields->append(field);
	}

	fieldObj.free();
}

void AcroForm::draw(int pageNum, Gfx* gfx, bool printing)
{
	for (int i = 0; i < fields->getLength(); ++i)
		((AcroFormField*)fields->get(i))->draw(pageNum, gfx, printing);
}

int AcroForm::getNumFields()
{
	return fields->getLength();
}

AcroFormField* AcroForm::getField(int idx)
{
	return (AcroFormField*)fields->get(idx);
}

AcroFormField* AcroForm::findField(int pg, double x, double y)
{
	AcroFormField* field;
	double         llx, lly, urx, ury;

	for (int i = 0; i < fields->getLength(); ++i)
	{
		field = (AcroFormField*)fields->get(i);
		if (field->getPageNum() == pg)
		{
			field->getBBox(&llx, &lly, &urx, &ury);
			if (llx <= x && x <= urx && lly <= y && y <= ury)
				return field;
		}
	}
	return nullptr;
}

int AcroForm::findFieldIdx(int pg, double x, double y)
{
	AcroFormField* field;
	double         llx, lly, urx, ury;

	for (int i = 0; i < fields->getLength(); ++i)
	{
		field = (AcroFormField*)fields->get(i);
		if (field->getPageNum() == pg)
		{
			field->getBBox(&llx, &lly, &urx, &ury);
			if (llx <= x && x <= urx && lly <= y && y <= ury)
				return i;
		}
	}
	return -1;
}

//------------------------------------------------------------------------
// AcroFormField
//------------------------------------------------------------------------

AcroFormField* AcroFormField::load(AcroForm* acroFormA, Object* fieldRefA)
{
	TextString*       nameA;
	uint32_t          flagsA;
	bool              haveFlags, typeFromParentA;
	Object            fieldObjA, parentObj, parentObj2, obj1, obj2;
	AcroFormFieldType typeA;
	XFAField*         xfaFieldA;
	AcroFormField*    field;
	int               depth, i0, i1;

	fieldRefA->fetch(acroFormA->doc->getXRef(), &fieldObjA);

	//----- get field info

	if (fieldObjA.dictLookup("T", &obj1)->isString())
		nameA = new TextString(obj1.getString());
	else
		nameA = new TextString();
	obj1.free();

	std::string typeStr;
	if (fieldObjA.dictLookup("FT", &obj1)->isName())
	{
		typeStr         = obj1.getName();
		typeFromParentA = false;
	}
	else
	{
		typeFromParentA = true;
	}
	obj1.free();

	if (fieldObjA.dictLookup("Ff", &obj1)->isInt())
	{
		flagsA    = (uint32_t)obj1.getInt();
		haveFlags = true;
	}
	else
	{
		flagsA    = 0;
		haveFlags = false;
	}
	obj1.free();

	//----- get info from parent non-terminal fields

	fieldObjA.dictLookup("Parent", &parentObj);
	depth = 0;
	while (parentObj.isDict() && depth < maxFieldObjectDepth)
	{
		if (parentObj.dictLookup("T", &obj1)->isString())
		{
			if (nameA->getLength())
				nameA->insert(0, (Unicode)'.');
			nameA->insert(0, obj1.getString());
		}
		obj1.free();

		if (typeStr.empty())
		{
			if (parentObj.dictLookup("FT", &obj1)->isName())
				typeStr = obj1.getName();
			obj1.free();
		}

		if (!haveFlags)
		{
			if (parentObj.dictLookup("Ff", &obj1)->isInt())
			{
				flagsA    = (uint32_t)obj1.getInt();
				haveFlags = true;
			}
			obj1.free();
		}

		parentObj.dictLookup("Parent", &parentObj2);
		parentObj.free();
		parentObj = parentObj2;

		++depth;
	}
	parentObj.free();

	if (typeStr.empty())
	{
		error(errSyntaxError, -1, "Missing type in AcroForm field");
		goto err1;
	}

	//----- get static XFA info

	xfaFieldA = nullptr;
	if (acroFormA->xfaScanner)
	{
		// convert field name to UTF-8, and remove segments that start
		// with '#' -- to match the XFA field name
		auto xfaName = nameA->toUTF8();
		i0           = 0;
		while (i0 < xfaName.size())
		{
			i1 = i0;
			while (i1 < xfaName.size())
			{
				if (xfaName.at(i1) == '.')
				{
					++i1;
					break;
				}
				++i1;
			}
			if (xfaName.at(i0) == '#')
				xfaName.erase(i0, i1 - i0);
			else
				i0 = i1;
		}
		xfaFieldA = acroFormA->xfaScanner->findField(xfaName);
	}

	//----- check for a radio button

	// this is a kludge: if we see a Btn-type field with kids, and the
	// Ff entry is missing, assume the kids are radio buttons
	if (typeFromParentA && typeStr == "Btn" && !haveFlags)
		flagsA = acroFormFlagRadio;

	//----- determine field type

	if (typeStr == "Btn")
	{
		if (flagsA & acroFormFlagPushbutton)
			typeA = acroFormFieldPushbutton;
		else if (flagsA & acroFormFlagRadio)
			typeA = acroFormFieldRadioButton;
		else
			typeA = acroFormFieldCheckbox;
	}
	else if (typeStr == "Tx")
	{
		if (xfaFieldA && xfaFieldA->getBarcodeInfo())
			typeA = acroFormFieldBarcode;
		else if (flagsA & acroFormFlagFileSelect)
			typeA = acroFormFieldFileSelect;
		else if (flagsA & acroFormFlagMultiline)
			typeA = acroFormFieldMultilineText;
		else
			typeA = acroFormFieldText;
	}
	else if (typeStr == "Ch")
	{
		if (flagsA & acroFormFlagCombo)
			typeA = acroFormFieldComboBox;
		else
			typeA = acroFormFieldListBox;
	}
	else if (typeStr == "Sig")
	{
		typeA = acroFormFieldSignature;
	}
	else
	{
		error(errSyntaxError, -1, "Invalid type in AcroForm field");
		goto err1;
	}

	field = new AcroFormField(acroFormA, fieldRefA, &fieldObjA, typeA, nameA, flagsA, typeFromParentA, xfaFieldA);
	fieldObjA.free();
	return field;

err1:
	delete nameA;
	fieldObjA.free();
	return nullptr;
}

AcroFormField::AcroFormField(AcroForm* acroFormA, Object* fieldRefA, Object* fieldObjA, AcroFormFieldType typeA, TextString* nameA, uint32_t flagsA, bool typeFromParentA, XFAField* xfaFieldA)
{
	acroForm = acroFormA;
	fieldRefA->copy(&fieldRef);
	fieldObjA->copy(&fieldObj);
	type           = typeA;
	name           = nameA;
	flags          = flagsA;
	typeFromParent = typeFromParentA;
	xfaField       = xfaFieldA;
}

AcroFormField::~AcroFormField()
{
	fieldRef.free();
	fieldObj.free();
	delete name;
}

int AcroFormField::getPageNum()
{
	Object kidsObj, annotRef;
	int    pageNum;

	pageNum = 0;
	if (fieldObj.dictLookup("Kids", &kidsObj)->isArray())
	{
		if (kidsObj.arrayGetLength() > 0)
		{
			kidsObj.arrayGetNF(0, &annotRef);
			pageNum = acroForm->lookupAnnotPage(&annotRef);
			annotRef.free();
		}
	}
	else
	{
		pageNum = acroForm->lookupAnnotPage(&fieldRef);
	}
	kidsObj.free();
	return pageNum;
}

const char* AcroFormField::getType()
{
	switch (type)
	{
	case acroFormFieldPushbutton: return "PushButton";
	case acroFormFieldRadioButton: return "RadioButton";
	case acroFormFieldCheckbox: return "Checkbox";
	case acroFormFieldFileSelect: return "FileSelect";
	case acroFormFieldMultilineText: return "MultilineText";
	case acroFormFieldText: return "Text";
	case acroFormFieldBarcode: return "Barcode";
	case acroFormFieldComboBox: return "ComboBox";
	case acroFormFieldListBox: return "ListBox";
	case acroFormFieldSignature: return "Signature";
	default: return nullptr;
	}
}

Unicode* AcroFormField::getName(int* length)
{
	Unicode *u, *ret;
	int      n;

	u   = name->getUnicode();
	n   = name->getLength();
	ret = (Unicode*)gmallocn(n, sizeof(Unicode));
	memcpy(ret, u, n * sizeof(Unicode));
	*length = n;
	return ret;
}

Unicode* AcroFormField::getValue(int* length)
{
	Object      obj1, obj2;
	Unicode*    u = nullptr;
	TextString* ts;
	int         n, i;

	*length = 0;

	// if this field has a counterpart in the XFA form, take the value
	// from the XFA field (NB: an XFA field with no value overrides the
	// AcroForm value)
	if (xfaField)
	{
		if (xfaField->getValue().size())
			u = utf8ToUnicode(xfaField->getValue(), length);

		// no XFA form - take the AcroForm value
	}
	else
	{
		fieldLookup("V", &obj1);
		if (obj1.isName())
		{
			char* s = obj1.getName();
			n       = (int)strlen(s);
			u       = (Unicode*)gmallocn(n, sizeof(Unicode));
			for (i = 0; i < n; ++i)
				u[i] = s[i] & 0xff;
			*length = n;
		}
		else if (obj1.isString())
		{
			ts = new TextString(obj1.getString());
			n  = ts->getLength();
			u  = (Unicode*)gmallocn(n, sizeof(Unicode));
			memcpy(u, ts->getUnicode(), n * sizeof(Unicode));
			*length = n;
			delete ts;
		}
		else if (obj1.isDict())
		{
			obj1.dictLookup("Contents", &obj2);
			if (obj2.isString())
			{
				const auto gs = obj2.getString();
				n             = TO_INT(gs.size());
				u             = (Unicode*)gmallocn(n, sizeof(Unicode));
				for (i = 0; i < n; ++i)
					u[i] = gs.at(i) & 0xff;
				*length = n;
			}
			obj2.free();
		}
		obj1.free();
	}

	return u;
}

void AcroFormField::getBBox(double* llx, double* lly, double* urx, double* ury)
{
	Object annotObj, rectObj, numObj;
	double t;

	*llx = *lly = *urx = *ury = 0;

	if (getAnnotObj(&annotObj)->isDict())
	{
		annotObj.dictLookup("Rect", &rectObj);
		if (rectObj.isArray() && rectObj.arrayGetLength() == 4)
		{
			rectObj.arrayGet(0, &numObj);
			if (numObj.isNum())
				*llx = numObj.getNum();
			numObj.free();
			rectObj.arrayGet(1, &numObj);
			if (numObj.isNum())
				*lly = numObj.getNum();
			numObj.free();
			rectObj.arrayGet(2, &numObj);
			if (numObj.isNum())
				*urx = numObj.getNum();
			numObj.free();
			rectObj.arrayGet(3, &numObj);
			if (numObj.isNum())
				*ury = numObj.getNum();
			numObj.free();
		}
		rectObj.free();
	}
	annotObj.free();

	if (*llx > *urx)
	{
		t    = *llx;
		*llx = *urx;
		*urx = t;
	}
	if (*lly > *ury)
	{
		t    = *lly;
		*lly = *ury;
		*ury = t;
	}
}

void AcroFormField::getFont(Ref* fontID, double* fontSize)
{
	Object daObj;
	double tfSize, m2, m3;
	int    tfPos, tmPos;

	fontID->num = fontID->gen = -1;
	*fontSize                 = 0;

	if (fieldLookup("DA", &daObj)->isString())
	{
		// parse the default appearance string
		const auto daToks = tokenize(daObj.getString());
		tfPos = tmPos = -1;
		for (int i = 2; i < daToks.size(); ++i)
			if (daToks.at(i) == "Tf")
				tfPos = i - 2;
			else if (i >= 6 && daToks.at(i) == "Tm")
				tmPos = i - 6;

		// handle the Tf operator
		if (tfPos >= 0)
		{
			const char* fontTag = daToks.at(tfPos).c_str();
			if (*fontTag == '/')
				++fontTag;
			*fontID = findFontName(fontTag);
			tfSize  = atof(daToks.at(tfPos + 1).c_str());
		}
		else
		{
			tfSize = 1;
		}

		// handle the Tm operator
		if (tmPos >= 0)
		{
			// transformed font size = sqrt(m[2]^2 + m[3]^2) * size
			m2 = atof(daToks.at(tfPos + 2).c_str());
			m3 = atof(daToks.at(tfPos + 3).c_str());
			tfSize *= sqrt(m2 * m2 + m3 * m3);
		}
		*fontSize = tfSize;
	}

	daObj.free();
}

Ref AcroFormField::findFontName(const char* fontTag)
{
	Object drObj, fontDictObj, fontObj, baseFontObj;
	Ref    fontID;
	bool   found;

	fontID.num = fontID.gen = -1;
	found                   = false;

	if (fieldObj.dictLookup("DR", &drObj)->isDict())
	{
		if (drObj.dictLookup("Font", &fontDictObj)->isDict())
		{
			if (fontDictObj.dictLookupNF(fontTag, &fontObj)->isRef())
			{
				fontID = fontObj.getRef();
				found  = true;
			}
			fontObj.free();
		}
		fontDictObj.free();
	}
	drObj.free();
	if (found)
		return fontID;

	if (acroForm->acroFormObj.dictLookup("DR", &drObj)->isDict())
	{
		if (drObj.dictLookup("Font", &fontDictObj)->isDict())
		{
			if (fontDictObj.dictLookupNF(fontTag, &fontObj)->isRef())
				fontID = fontObj.getRef();
			fontObj.free();
		}
		fontDictObj.free();
	}
	drObj.free();
	return fontID;
}

void AcroFormField::getColor(double* red, double* green, double* blue)
{
	Object daObj;

	*red = *green = *blue = 0;

	if (fieldLookup("DA", &daObj)->isString())
	{
		// parse the default appearance string
		const auto daToks = tokenize(daObj.getString());
		for (int i = 1; i < daToks.size(); ++i)
		{
			// handle the g operator
			if (daToks.at(i) == "g")
			{
				*red = *green = *blue = atof(daToks.at(i - 1).c_str());
				break;

				// handle the rg operator
			}
			else if (i >= 3 && daToks.at(i) == "rg")
			{
				*red   = atof(daToks.at(i - 3).c_str());
				*green = atof(daToks.at(i - 2).c_str());
				*blue  = atof(daToks.at(i - 1).c_str());
				break;
			}
		}
	}

	daObj.free();
}

int AcroFormField::getMaxLen()
{
	Object obj;
	int    len;

	if (fieldLookup("MaxLen", &obj)->isInt())
		len = obj.getInt();
	else
		len = -1;
	obj.free();
	return len;
}

void AcroFormField::draw(int pageNum, Gfx* gfx, bool printing)
{
	Object kidsObj, annotRef, annotObj;
	int    i;

	// find the annotation object(s)
	if (fieldObj.dictLookup("Kids", &kidsObj)->isArray())
	{
		for (i = 0; i < kidsObj.arrayGetLength(); ++i)
		{
			kidsObj.arrayGetNF(i, &annotRef);
			annotRef.fetch(acroForm->doc->getXRef(), &annotObj);
			drawAnnot(pageNum, gfx, printing, &annotRef, &annotObj);
			annotObj.free();
			annotRef.free();
		}
	}
	else
	{
		drawAnnot(pageNum, gfx, printing, &fieldRef, &fieldObj);
	}
	kidsObj.free();
}

void AcroFormField::drawAnnot(int pageNum, Gfx* gfx, bool printing, Object* annotRef, Object* annotObj)
{
	Object obj1, obj2;
	double xMin, yMin, xMax, yMax, t;
	int    annotFlags;
	bool   oc, render;

	if (!annotObj->isDict())
		return;

	//----- get the page number

	// the "P" (page) field in annotations is optional, so we can't
	// depend on it here
	if (acroForm->lookupAnnotPage(annotRef) != pageNum)
		return;

	//----- check annotation flags

	if (annotObj->dictLookup("F", &obj1)->isInt())
		annotFlags = obj1.getInt();
	else
		annotFlags = 0;
	obj1.free();
	if ((annotFlags & annotFlagHidden) || (printing && !(annotFlags & annotFlagPrint)) || (!printing && (annotFlags & annotFlagNoView)))
		return;

	//----- check the optional content entry

	annotObj->dictLookupNF("OC", &obj1);
	if (acroForm->doc->getOptionalContent()->evalOCObject(&obj1, &oc) && !oc)
	{
		obj1.free();
		return;
	}
	obj1.free();

	//----- get the bounding box

	if (annotObj->dictLookup("Rect", &obj1)->isArray() && obj1.arrayGetLength() == 4)
	{
		xMin = yMin = xMax = yMax = 0;
		if (obj1.arrayGet(0, &obj2)->isNum())
			xMin = obj2.getNum();
		obj2.free();
		if (obj1.arrayGet(1, &obj2)->isNum())
			yMin = obj2.getNum();
		obj2.free();
		if (obj1.arrayGet(2, &obj2)->isNum())
			xMax = obj2.getNum();
		obj2.free();
		if (obj1.arrayGet(3, &obj2)->isNum())
			yMax = obj2.getNum();
		obj2.free();
		if (xMin > xMax)
		{
			t    = xMin;
			xMin = xMax;
			xMax = t;
		}
		if (yMin > yMax)
		{
			t    = yMin;
			yMin = yMax;
			yMax = t;
		}
	}
	else
	{
		error(errSyntaxError, -1, "Bad bounding box for annotation");
		obj1.free();
		return;
	}
	obj1.free();

	//----- draw it

	render = false;
	if (acroForm->needAppearances)
	{
		render = true;
	}
	else if (xfaField && xfaField->getValue().size())
	{
		render = true;
	}
	else
	{
		if (!annotObj->dictLookup("AP", &obj1)->isDict())
			render = true;
		obj1.free();
	}
	if (render)
		drawNewAppearance(gfx, annotObj->getDict(), xMin, yMin, xMax, yMax);
	else
		drawExistingAppearance(gfx, annotObj->getDict(), xMin, yMin, xMax, yMax);
}

// Draw the existing appearance stream for a single annotation
// attached to this field.
void AcroFormField::drawExistingAppearance(Gfx* gfx, Dict* annot, double xMin, double yMin, double xMax, double yMax)
{
	Object apObj, asObj, appearance, obj1;

	//----- get the appearance stream

	if (annot->lookup("AP", &apObj)->isDict())
	{
		apObj.dictLookup("N", &obj1);
		if (obj1.isDict())
		{
			if (annot->lookup("AS", &asObj)->isName())
				obj1.dictLookupNF(asObj.getName(), &appearance);
			else if (obj1.dictGetLength() == 1)
				obj1.dictGetValNF(0, &appearance);
			else
				obj1.dictLookupNF("Off", &appearance);
			asObj.free();
		}
		else
		{
			apObj.dictLookupNF("N", &appearance);
		}
		obj1.free();
	}
	apObj.free();

	//----- draw it

	if (!appearance.isNone())
	{
		gfx->drawAnnot(&appearance, nullptr, xMin, yMin, xMax, yMax);
		appearance.free();
	}
}

// Regenerate the appearance for this field, and draw it.
void AcroFormField::drawNewAppearance(Gfx* gfx, Dict* annot, double xMin, double yMin, double xMax, double yMax)
{
	Object          appearance, mkObj, ftObj, appearDict, drObj, apObj, asObj;
	Object          resources, fontResources, defaultFont, gfxStateDict;
	Object          obj1, obj2, obj3, obj4;
	Dict*           mkDict;
	MemStream*      appearStream;
	GfxFontDict*    fontDict;
	bool            hasCaption;
	double          dx, dy, r;
	bool            done;
	bool*           selection;
	AnnotBorderType borderType;
	double          borderWidth;
	double*         borderDash;
	int             borderDashLength, rot, quadding, vAlign, comb, nOptions, topIdx, i;

	std::string appearBuf;
	std::string da;
	std::string val;

#if 0 //~debug
	appearBuf += fmt::format("1 1 0 rg 0 0 {:.4f} {:.4f} re f\n", xMax - xMin, yMax - yMin);
#endif
#if 0 //~debug
	appearBuf += fmt::format("1 1 0 RG 0 0 {:.4f} {:.4f} re s\n", xMax - xMin, yMax - yMin);
#endif

	// get the appearance characteristics (MK) dictionary
	if (annot->lookup("MK", &mkObj)->isDict())
		mkDict = mkObj.getDict();
	else
		mkDict = nullptr;

	// draw the background
	if (mkDict)
	{
		if (mkDict->lookup("BG", &obj1)->isArray() && obj1.arrayGetLength() > 0)
		{
			setColor(obj1.getArray(), true, 0, appearBuf);
			appearBuf += fmt::format("0 0 {:.4f} {:.4f} re f\n", xMax - xMin, yMax - yMin);
		}
		obj1.free();
	}

	// get the field type
	fieldLookup("FT", &ftObj);

	// draw the border
	borderType       = annotBorderSolid;
	borderWidth      = 1;
	borderDash       = nullptr;
	borderDashLength = 0;
	if (annot->lookup("BS", &obj1)->isDict())
	{
		if (obj1.dictLookup("S", &obj2)->isName())
		{
			if (obj2.isName("S"))
				borderType = annotBorderSolid;
			else if (obj2.isName("D"))
				borderType = annotBorderDashed;
			else if (obj2.isName("B"))
				borderType = annotBorderBeveled;
			else if (obj2.isName("I"))
				borderType = annotBorderInset;
			else if (obj2.isName("U"))
				borderType = annotBorderUnderlined;
		}
		obj2.free();
		if (obj1.dictLookup("W", &obj2)->isNum())
			borderWidth = obj2.getNum();
		obj2.free();
		if (obj1.dictLookup("D", &obj2)->isArray())
		{
			borderDashLength = obj2.arrayGetLength();
			borderDash       = (double*)gmallocn(borderDashLength, sizeof(double));
			for (i = 0; i < borderDashLength; ++i)
			{
				if (obj2.arrayGet(i, &obj3)->isNum())
					borderDash[i] = obj3.getNum();
				else
					borderDash[i] = 1;
				obj3.free();
			}
		}
		obj2.free();
	}
	else
	{
		obj1.free();
		if (annot->lookup("Border", &obj1)->isArray())
		{
			if (obj1.arrayGetLength() >= 3)
			{
				if (obj1.arrayGet(2, &obj2)->isNum())
					borderWidth = obj2.getNum();
				obj2.free();
				if (obj1.arrayGetLength() >= 4)
				{
					if (obj1.arrayGet(3, &obj2)->isArray())
					{
						borderType       = annotBorderDashed;
						borderDashLength = obj2.arrayGetLength();
						borderDash       = (double*)gmallocn(borderDashLength, sizeof(double));
						for (i = 0; i < borderDashLength; ++i)
						{
							if (obj2.arrayGet(i, &obj3)->isNum())
								borderDash[i] = obj3.getNum();
							else
								borderDash[i] = 1;
							obj3.free();
						}
					}
					else
					{
						// Adobe draws no border at all if the last element is of
						// the wrong type.
						borderWidth = 0;
					}
					obj2.free();
				}
			}
		}
	}
	obj1.free();
	if (mkDict)
	{
		if (borderWidth > 0)
		{
			mkDict->lookup("BC", &obj1);
			if (!(obj1.isArray() && obj1.arrayGetLength() > 0))
			{
				obj1.free();
				mkDict->lookup("BG", &obj1);
			}
			if (obj1.isArray() && obj1.arrayGetLength() > 0)
			{
				dx = xMax - xMin;
				dy = yMax - yMin;

				// radio buttons with no caption have a round border
				hasCaption = mkDict->lookup("CA", &obj2)->isString();
				obj2.free();
				if (ftObj.isName("Btn") && (flags & acroFormFlagRadio) && !hasCaption)
				{
					r = 0.5 * (dx < dy ? dx : dy);
					switch (borderType)
					{
					case annotBorderDashed:
						appearBuf += "[";
						for (i = 0; i < borderDashLength; ++i)
							appearBuf += fmt::format(" {:.4f}", borderDash[i]);
						appearBuf += "] 0 d\n";
						// fall through to the solid case
					case annotBorderSolid:
					case annotBorderUnderlined:
						appearBuf += fmt::format("{:.4f} w\n", borderWidth);
						setColor(obj1.getArray(), false, 0, appearBuf);
						drawCircle(0.5 * dx, 0.5 * dy, r - 0.5 * borderWidth, "s", appearBuf);
						break;
					case annotBorderBeveled:
					case annotBorderInset:
						appearBuf += fmt::format("{:.4f} w\n", 0.5 * borderWidth);
						setColor(obj1.getArray(), false, 0, appearBuf);
						drawCircle(0.5 * dx, 0.5 * dy, r - 0.25 * borderWidth, "s", appearBuf);
						setColor(obj1.getArray(), false, borderType == annotBorderBeveled ? 1 : -1, appearBuf);
						drawCircleTopLeft(0.5 * dx, 0.5 * dy, r - 0.75 * borderWidth, appearBuf);
						setColor(obj1.getArray(), false, borderType == annotBorderBeveled ? -1 : 1, appearBuf);
						drawCircleBottomRight(0.5 * dx, 0.5 * dy, r - 0.75 * borderWidth, appearBuf);
						break;
					}
				}
				else
				{
					switch (borderType)
					{
					case annotBorderDashed:
						appearBuf += "[";
						for (i = 0; i < borderDashLength; ++i)
							appearBuf += fmt::format(" {:.4f}", borderDash[i]);
						appearBuf += "] 0 d\n";
						// fall through to the solid case
					case annotBorderSolid:
						appearBuf += fmt::format("{:.4f} w\n", borderWidth);
						setColor(obj1.getArray(), false, 0, appearBuf);
						appearBuf += fmt::format("{:.4f} {:.4f} {:.4f} {:.4f} re s\n", 0.5 * borderWidth, 0.5 * borderWidth, dx - borderWidth, dy - borderWidth);
						break;
					case annotBorderBeveled:
					case annotBorderInset:
						setColor(obj1.getArray(), true, borderType == annotBorderBeveled ? 1 : -1, appearBuf);
						appearBuf += "0 0 m\n";
						appearBuf += fmt::format("0 {:.4f} l\n", dy);
						appearBuf += fmt::format("{:.4f} {:.4f} l\n", dx, dy);
						appearBuf += fmt::format("{:.4f} {:.4f} l\n", dx - borderWidth, dy - borderWidth);
						appearBuf += fmt::format("{:.4f} {:.4f} l\n", borderWidth, dy - borderWidth);
						appearBuf += fmt::format("{:.4f} {:.4f} l\n", borderWidth, borderWidth);
						appearBuf += "f\n";
						setColor(obj1.getArray(), true, borderType == annotBorderBeveled ? -1 : 1, appearBuf);
						appearBuf += "0 0 m\n";
						appearBuf += fmt::format("{:.4f} 0 l\n", dx);
						appearBuf += fmt::format("{:.4f} {:.4f} l\n", dx, dy);
						appearBuf += fmt::format("{:.4f} {:.4f} l\n", dx - borderWidth, dy - borderWidth);
						appearBuf += fmt::format("{:.4f} {:.4f} l\n", dx - borderWidth, borderWidth);
						appearBuf += fmt::format("{:.4f} {:.4f} l\n", borderWidth, borderWidth);
						appearBuf += "f\n";
						break;
					case annotBorderUnderlined:
						appearBuf += fmt::format("{:.4f} w\n", borderWidth);
						setColor(obj1.getArray(), false, 0, appearBuf);
						appearBuf += fmt::format("0 0 m {:.4f} 0 l s\n", dx);
						break;
					}

					// clip to the inside of the border
					appearBuf += fmt::format("{:.4f} {:.4f} {:.4f} {:.4f} re W n\n", borderWidth, borderWidth, dx - 2 * borderWidth, dy - 2 * borderWidth);
				}
			}
			obj1.free();
		}
	}
	gfree(borderDash);

	// get the resource dictionary
	buildDefaultResourceDict(&drObj);

	// build the font dictionary
	if (drObj.isDict() && drObj.dictLookup("Font", &obj1)->isDict())
		fontDict = new GfxFontDict(acroForm->doc->getXRef(), nullptr, obj1.getDict());
	else
		fontDict = nullptr;
	obj1.free();

	// get the default appearance string
	if (fieldLookup("DA", &obj1)->isString())
		da = obj1.getString();
	obj1.free();

	// get the rotation value
	rot = 0;
	if (mkDict)
	{
		if (mkDict->lookup("R", &obj1)->isInt())
			rot = obj1.getInt();
		obj1.free();
	}

	// get the appearance state
	annot->lookup("AP", &apObj);
	annot->lookup("AS", &asObj);
	std::string appearanceState;
	if (asObj.isName())
	{
		appearanceState = asObj.getName();
	}
	else if (apObj.isDict())
	{
		apObj.dictLookup("N", &obj1);
		if (obj1.isDict() && obj1.dictGetLength() == 1)
			appearanceState = obj1.dictGetKey(0);
		obj1.free();
	}
	if (appearanceState.empty())
		appearanceState = "Off";
	asObj.free();
	apObj.free();

	int      valueLength;
	Unicode* value = getValue(&valueLength);

	// draw the field contents
	if (ftObj.isName("Btn"))
	{
		std::string caption;
		if (mkDict)
		{
			if (mkDict->lookup("CA", &obj1)->isString())
				caption = obj1.getString();
			obj1.free();
		}
		// radio button
		if (flags & acroFormFlagRadio)
		{
			//~ Acrobat doesn't draw a caption if there is no AP dict (?)
			if (value && unicodeStringEqual(value, valueLength, appearanceState.c_str()))
			{
				if (caption.size())
				{
					drawText(caption, da, fontDict, false, 0, acroFormQuadCenter, acroFormVAlignMiddleNoDescender, false, true, rot, 0, 0, xMax - xMin, yMax - yMin, borderWidth, false, appearBuf);
				}
				else if (mkDict)
				{
					if (mkDict->lookup("BC", &obj2)->isArray() && obj2.arrayGetLength() > 0)
					{
						dx = xMax - xMin;
						dy = yMax - yMin;
						setColor(obj2.getArray(), true, 0, appearBuf);
						drawCircle(0.5 * dx, 0.5 * dy, 0.2 * (dx < dy ? dx : dy), "f", appearBuf);
					}
					obj2.free();
				}
			}
			// pushbutton
		}
		else if (flags & acroFormFlagPushbutton)
		{
			if (caption.size())
				drawText(caption, da, fontDict, false, 0, acroFormQuadCenter, acroFormVAlignMiddle, false, false, rot, 0, 0, xMax - xMin, yMax - yMin, borderWidth, false, appearBuf);
			// checkbox
		}
		else if (value && !(unicodeStringEqual(value, valueLength, "Off") || unicodeStringEqual(value, valueLength, "No") || unicodeStringEqual(value, valueLength, "0") || valueLength == 0))
		{
			if (caption.empty())
				caption = "3"; // ZapfDingbats checkmark
			drawText(caption, da, fontDict, false, 0, acroFormQuadCenter, acroFormVAlignMiddleNoDescender, false, true, rot, 0, 0, xMax - xMin, yMax - yMin, borderWidth, false, appearBuf);
		}
	}
	else if (ftObj.isName("Tx"))
	{
		XFAFieldBarcodeInfo* barcodeInfo = xfaField ? xfaField->getBarcodeInfo() : (XFAFieldBarcodeInfo*)nullptr;
		if (value)
		{
			//~ value strings can be Unicode
			const auto valueLatin1 = unicodeToLatin1(value, valueLength);
			if (barcodeInfo)
			{
				drawBarcode(valueLatin1, da, fontDict, rot, xMin, yMin, xMax, yMax, barcodeInfo, appearBuf);
			}
			else
			{
				if (fieldLookup("Q", &obj2)->isInt())
					quadding = obj2.getInt();
				else
					quadding = acroFormQuadLeft;
				obj2.free();
				vAlign          = (flags & acroFormFlagMultiline) ? acroFormVAlignTop : acroFormVAlignMiddle;
				auto layoutInfo = xfaField ? xfaField->getLayoutInfo() : (XFAFieldLayoutInfo*)nullptr;
				if (layoutInfo)
				{
					switch (layoutInfo->hAlign)
					{
					case xfaFieldLayoutHAlignLeft:
					default:
						quadding = acroFormQuadLeft;
						break;
					case xfaFieldLayoutHAlignCenter:
						quadding = acroFormQuadCenter;
						break;
					case xfaFieldLayoutHAlignRight:
						quadding = acroFormQuadRight;
						break;
					}
					switch (layoutInfo->vAlign)
					{
					case xfaFieldLayoutVAlignTop:
					default:
						vAlign = acroFormVAlignTop;
						break;
					case xfaFieldLayoutVAlignMiddle:
						vAlign = acroFormVAlignMiddle;
						break;
					case xfaFieldLayoutVAlignBottom:
						vAlign = acroFormVAlignBottom;
						break;
					}
				}
				comb = 0;
				if (flags & acroFormFlagComb)
				{
					if (fieldLookup("MaxLen", &obj2)->isInt())
						comb = obj2.getInt();
					obj2.free();
				}
				XFAFieldPictureInfo* pictureInfo = xfaField ? xfaField->getPictureInfo() : (XFAFieldPictureInfo*)nullptr;
				auto                 value2      = valueLatin1;
				if (pictureInfo)
				{
					switch (pictureInfo->subtype)
					{
					case xfaFieldPictureDateTime:
						value2 = pictureFormatDateTime(valueLatin1, pictureInfo->format);
						break;
					case xfaFieldPictureNumeric:
						value2 = pictureFormatNumber(valueLatin1, pictureInfo->format);
						break;
					case xfaFieldPictureText:
						value2 = pictureFormatText(valueLatin1, pictureInfo->format);
						break;
					}
				}
				drawText(value2, da, fontDict, flags & acroFormFlagMultiline, comb, quadding, vAlign, true, false, rot, 0, 0, xMax - xMin, yMax - yMin, borderWidth, false, appearBuf);
			}
		}
	}
	else if (ftObj.isName("Ch"))
	{
		//~ value/option strings can be Unicode
		if (fieldLookup("Q", &obj1)->isInt())
			quadding = obj1.getInt();
		else
			quadding = acroFormQuadLeft;
		obj1.free();
		vAlign                         = acroFormVAlignMiddle;
		XFAFieldLayoutInfo* layoutInfo = xfaField ? xfaField->getLayoutInfo()
		                                          : (XFAFieldLayoutInfo*)nullptr;
		if (layoutInfo)
		{
			switch (layoutInfo->hAlign)
			{
			case xfaFieldLayoutHAlignLeft:
			default:
				quadding = acroFormQuadLeft;
				break;
			case xfaFieldLayoutHAlignCenter:
				quadding = acroFormQuadCenter;
				break;
			case xfaFieldLayoutHAlignRight:
				quadding = acroFormQuadRight;
				break;
			}
			switch (layoutInfo->vAlign)
			{
			case xfaFieldLayoutVAlignTop:
			default:
				vAlign = acroFormVAlignTop;
				break;
			case xfaFieldLayoutVAlignMiddle:
				vAlign = acroFormVAlignMiddle;
				break;
			case xfaFieldLayoutVAlignBottom:
				vAlign = acroFormVAlignBottom;
				break;
			}
		}
		// combo box
		if (flags & acroFormFlagCombo)
		{
			if (value)
			{
				val = unicodeToLatin1(value, valueLength);
				if (fieldObj.dictLookup("Opt", &obj2)->isArray())
				{
					for (i = 0, done = false; i < obj2.arrayGetLength() && !done; ++i)
					{
						obj2.arrayGet(i, &obj3);
						if (obj3.isArray() && obj3.arrayGetLength() == 2)
						{
							if (obj3.arrayGet(0, &obj4)->isString() && obj4.getString() == val)
							{
								obj4.free();
								if (obj3.arrayGet(1, &obj4)->isString())
									val = obj4.getString();
								done = true;
							}
							obj4.free();
						}
						obj3.free();
					}
				}
				obj2.free();
				drawText(val, da, fontDict, false, 0, quadding, vAlign, true, false, rot, 0, 0, xMax - xMin, yMax - yMin, borderWidth, false, appearBuf);
				//~ Acrobat draws a popup icon on the right side
			}
			// list box
		}
		else
		{
			if (fieldObj.dictLookup("Opt", &obj1)->isArray())
			{
				nOptions = obj1.arrayGetLength();
				// get the option text
				VEC_STR texts;
				texts.resize(nOptions);
				for (i = 0; i < nOptions; ++i)
				{
					obj1.arrayGet(i, &obj2);
					if (obj2.isString())
					{
						texts[i] = obj2.getString();
					}
					else if (obj2.isArray() && obj2.arrayGetLength() == 2)
					{
						if (obj2.arrayGet(1, &obj3)->isString())
							texts[i] = obj3.getString();
						obj3.free();
					}
					obj2.free();
				}
				// get the selected option(s)
				selection = (bool*)gmallocn(nOptions, sizeof(bool));
				//~ need to use the I field in addition to the V field
				for (i = 0; i < nOptions; ++i)
					selection[i] = unicodeStringEqual(value, valueLength, texts[i]);
				// get the top index
				if (fieldObj.dictLookup("TI", &obj2)->isInt())
				{
					topIdx = obj2.getInt();
					if (topIdx < 0 || topIdx >= nOptions)
						topIdx = 0;
				}
				else
				{
					topIdx = 0;
				}
				obj2.free();
				// draw the text
				drawListBox(texts, selection, nOptions, topIdx, da, fontDict, quadding, xMin, yMin, xMax, yMax, borderWidth, appearBuf);
				gfree(selection);
			}
			obj1.free();
		}
	}
	else if (ftObj.isName("Sig"))
	{
		//~ check to see if background is already drawn
		gfxStateDict.initDict(acroForm->doc->getXRef());
		obj1.initReal(0.5);
		gfxStateDict.dictAdd(copyString("ca"), &obj1);
		appearBuf += "/GS1 gs\n";
		appearBuf += fmt::format("0.7 0.7 1 rg 0 0 {:.2f} {:.2f} re f\n", xMax - xMin, yMax - yMin);
		const std::string caption = "SIGN HERE";
		da                        = "/Helv 10 Tf 1 0 0 rg";
		drawText(caption, da, fontDict, false, 0, acroFormQuadLeft, acroFormVAlignMiddle, false, false, rot, 0, 0, xMax - xMin, yMax - yMin, borderWidth, false, appearBuf);
	}
	else
	{
		error(errSyntaxError, -1, "Unknown field type");
	}

	gfree(value);

	// build the appearance stream dictionary
	appearDict.initDict(acroForm->doc->getXRef());
	appearDict.dictAdd(copyString("Length"), obj1.initInt(TO_INT(appearBuf.size())));
	appearDict.dictAdd(copyString("Subtype"), obj1.initName("Form"));
	obj1.initArray(acroForm->doc->getXRef());
	obj1.arrayAdd(obj2.initReal(0));
	obj1.arrayAdd(obj2.initReal(0));
	obj1.arrayAdd(obj2.initReal(xMax - xMin));
	obj1.arrayAdd(obj2.initReal(yMax - yMin));
	appearDict.dictAdd(copyString("BBox"), &obj1);

	// set the resource dictionary; add a default font
	if (drObj.isDict())
		drObj.copy(&resources);
	else
		resources.initDict(acroForm->doc->getXRef());
	drObj.free();
	fontResources.initDict(acroForm->doc->getXRef());
	if (resources.dictLookup("Font", &obj1)->isDict())
	{
		for (i = 0; i < obj1.dictGetLength(); ++i)
		{
			obj1.dictGetValNF(i, &obj2);
			fontResources.dictAdd(copyString(obj1.dictGetKey(i)), &obj2);
		}
	}
	obj1.free();
	defaultFont.initDict(acroForm->doc->getXRef());
	defaultFont.dictAdd(copyString("Type"), obj1.initName("Font"));
	defaultFont.dictAdd(copyString("Subtype"), obj1.initName("Type1"));
	defaultFont.dictAdd(copyString("BaseFont"), obj1.initName("Helvetica"));
	defaultFont.dictAdd(copyString("Encoding"), obj1.initName("WinAnsiEncoding"));
	fontResources.dictAdd(copyString("xpdf_default_font"), &defaultFont);
	resources.dictAdd(copyString("Font"), &fontResources);
	if (gfxStateDict.isDict())
	{
		obj1.initDict(acroForm->doc->getXRef());
		obj1.dictAdd(copyString("GS1"), &gfxStateDict);
		resources.dictAdd(copyString("ExtGState"), &obj1);
	}
	appearDict.dictAdd(copyString("Resources"), &resources);

	// build the appearance stream
	appearStream = new MemStream(appearBuf.data(), 0, TO_UINT32(appearBuf.size()), &appearDict);
	appearance.initStream(appearStream);

	// draw it
	gfx->drawAnnot(&appearance, nullptr, xMin, yMin, xMax, yMax);

	appearance.free();
	if (fontDict) delete fontDict;
	ftObj.free();
	mkObj.free();
}

// Set the current fill or stroke color, based on <a> (which should
// have 1, 3, or 4 elements).  If <adjust> is +1, color is brightened;
// if <adjust> is -1, color is darkened; otherwise color is not
// modified.
void AcroFormField::setColor(Array* a, bool fill, int adjust, std::string& appearBuf)
{
	Object obj1;
	double color[4];
	int    nComps, i;

	nComps = a->getLength();
	if (nComps > 4)
		nComps = 4;
	for (i = 0; i < nComps && i < 4; ++i)
	{
		if (a->get(i, &obj1)->isNum())
			color[i] = obj1.getNum();
		else
			color[i] = 0;
		obj1.free();
	}
	if (nComps == 4)
		adjust = -adjust;
	if (adjust > 0)
		for (i = 0; i < nComps; ++i)
			color[i] = 0.5 * color[i] + 0.5;
	else if (adjust < 0)
		for (i = 0; i < nComps; ++i)
			color[i] = 0.5 * color[i];
	if (nComps == 4)
		appearBuf += fmt::format("{:.2f} {:.2f} {:.2f} {:.2f} {:c}\n", color[0], color[1], color[2], color[3], fill ? 'k' : 'K');
	else if (nComps == 3)
		appearBuf += fmt::format("{:.2f} {:.2f} {:.2f} {:s}\n", color[0], color[1], color[2], fill ? "rg" : "RG");
	else
		appearBuf += fmt::format("{:.2f} {:c}\n", color[0], fill ? 'g' : 'G');
}

// Draw the variable text or caption for a field.
void AcroFormField::drawText(const std::string& text, const std::string& da, GfxFontDict* fontDict, bool multiline, int comb, int quadding, int vAlign, bool txField, bool forceZapfDingbats, int rot, double x, double y, double width, double height, double border, bool whiteBackground, std::string& appearBuf)
{
	VEC_STR  daToks;
	GfxFont* font;
	double   dx, dy;
	double   fontSize, fontSize2, topBorder, xx, xPrev, yy, w, wMax;
	double   offset, offset2, charWidth, ascent, descent;
	int      tfPos, tmPos, nLines, i, j, k, c;

	//~ if there is no MK entry, this should use the existing content stream,
	//~ and only replace the marked content portion of it
	//~ (this is only relevant for Tx fields)

	// check for a Unicode string
	//~ this currently drops all non-Latin1 characters
	std::string text2;
	if (text.size() >= 2 && text.at(0) == '\xfe' && text.at(1) == '\xff')
	{
		std::string text2;
		for (i = 2; i + 1 < text.size(); i += 2)
		{
			c = ((text.at(i) & 0xff) << 8) + (text.at(i + 1) & 0xff);
			if (c <= 0xff)
				text2 += (char)c;
			else
				text2 += '?';
		}
	}
	else
	{
		text2 = text;
	}
	if (text2.size() == 0)
		return;

	// parse the default appearance string
	tfPos = tmPos = -1;
	if (da.size())
	{
		daToks = tokenize(da);
		for (i = 2; i < daToks.size(); ++i)
			if (daToks.at(i) == "Tf")
				tfPos = i - 2;
			else if (i >= 6 && daToks.at(i) == "Tm")
				tmPos = i - 6;
	}
	else
	{
	}

	// force ZapfDingbats
	//~ this should create the font if needed (?)
	if (forceZapfDingbats)
	{
		if (tfPos >= 0)
		{
			const auto& tok = daToks.at(tfPos);
			if (tok != "/ZaDb")
				daToks[tfPos] = "/ZaDb";
		}
	}

	// get the font and font size
	font     = nullptr;
	fontSize = 0;
	if (tfPos >= 0)
	{
		const auto& tok = daToks.at(tfPos);
		if (tok.size() >= 1 && tok.at(0) == '/')
		{
			if (!fontDict || !(font = fontDict->lookup(tok.c_str() + 1)))
			{
				error(errSyntaxError, -1, "Unknown font in field's DA string");
				daToks[tfPos] = "/xpdf_default_font";
			}
		}
		else
		{
			error(errSyntaxError, -1, "Invalid font name in 'Tf' operator in field's DA string");
		}
		{
			const auto& tok = daToks.at(tfPos + 1);
			fontSize        = atof(tok.c_str());
		}
	}
	else
	{
		error(errSyntaxError, -1, "Missing 'Tf' operator in field's DA string");
		fontSize = 0;
		tfPos    = TO_INT(daToks.size());
		daToks.push_back("/xpdf_default_font");
		daToks.push_back("10");
		daToks.push_back("Tf");
	}

	// setup
	if (txField)
		appearBuf += "/Tx BMC\n";
	appearBuf += "q\n";
	if (rot == 90)
	{
		appearBuf += fmt::format("0 1 -1 0 {:.4f} 0 cm\n", width);
		dx = height;
		dy = width;
	}
	else if (rot == 180)
	{
		appearBuf += fmt::format("-1 0 0 -1 {:.4f} {:.4f} cm\n", width, height);
		dx = width;
		dy = height;
	}
	else if (rot == 270)
	{
		appearBuf += fmt::format("0 -1 1 0 0 {:.4f} cm\n", height);
		dx = height;
		dy = width;
	}
	else
	{ // assume rot == 0
		dx = width;
		dy = height;
	}

	// multi-line text
	if (multiline)
	{
		// note: the comb flag is ignored in multiline mode

		wMax = dx - 2 * border - 4;

		// this is a kludge that appears to match Adobe's behavior
		if (height > 15)
			topBorder = 5;
		else
			topBorder = 2;

		// compute font autosize
		if (fontSize == 0)
		{
			for (fontSize = 10; fontSize > 1; --fontSize)
			{
				yy = dy - topBorder;
				w  = 0;
				i  = 0;
				while (i < text2.size())
				{
					getNextLine(text2, i, font, fontSize, wMax, &j, &w, &k);
					i = k;
					yy -= fontSize;
				}
				// approximate the descender for the last line
				if (yy >= 0.25 * fontSize && w <= wMax)
					break;
			}
			if (tfPos >= 0)
				daToks[tfPos + 1] = fmt::format("{:.2f}", fontSize);
		}

		// starting y coordinate
		nLines = 0;
		i      = 0;
		while (i < text2.size())
		{
			getNextLine(text2, i, font, fontSize, wMax, &j, &w, &k);
			i = k;
			++nLines;
		}
		if (font)
		{
			ascent  = font->getDeclaredAscent() * fontSize;
			descent = font->getDescent() * fontSize;
		}
		else
		{
			ascent  = 0.75 * fontSize;
			descent = -0.25 * fontSize;
		}
		switch (vAlign)
		{
		case acroFormVAlignTop:
		default:
			yy = dy - ascent - topBorder;
			break;
		case acroFormVAlignMiddle:
			yy = 0.5 * (dy - nLines * fontSize) + (nLines - 1) * fontSize - descent;
			break;
		case acroFormVAlignMiddleNoDescender:
			yy = 0.5 * (dy - nLines * fontSize) + (nLines - 1) * fontSize;
			break;
		case acroFormVAlignBottom:
			yy = (nLines - 1) * fontSize - descent;
			break;
		}
		// if the field is shorter than a line of text, Acrobat positions
		// the text relative to the bottom edge
		if (dy < fontSize + topBorder)
			yy = 2 - descent;
		// each line of text starts with a Td operator that moves down a
		// line -- so move up a line here
		yy += fontSize;

		appearBuf += "BT\n";

		// set the font matrix
		if (tmPos >= 0)
		{
			daToks[tmPos + 4] = fmt::format("{:.4f}", x);
			daToks[tmPos + 5] = fmt::format("{:.4f}", y + yy);
		}

		// write the DA string
		if (daToks.size())
			for (i = 0; i < daToks.size(); ++i)
			{
				appearBuf += daToks.at(i);
				appearBuf += ' ';
			}

		// write the font matrix (if not part of the DA string)
		if (tmPos < 0)
			appearBuf += fmt::format("1 0 0 1 {:.4f} {:.4f} Tm\n", x, y + yy);

		// write a series of lines of text
		i     = 0;
		xPrev = 0;
		while (i < text2.size())
		{
			getNextLine(text2, i, font, fontSize, wMax, &j, &w, &k);

			// compute text start position
			switch (quadding)
			{
			case acroFormQuadLeft:
			default:
				xx = border + 2;
				break;
			case acroFormQuadCenter:
				xx = (dx - w) / 2;
				break;
			case acroFormQuadRight:
				xx = dx - border - 2 - w;
				break;
			}

			// draw the line
			appearBuf += fmt::format("{:.4f} {:.4f} Td\n", xx - xPrev, -fontSize);
			appearBuf += '(';
			for (; i < j; ++i)
			{
				c = text2.at(i) & 0xff;
				if (c == '(' || c == ')' || c == '\\')
				{
					appearBuf += '\\';
					appearBuf += (char)c;
				}
				else if (c < 0x20 || c >= 0x80)
				{
					appearBuf += fmt::format("\\{:03o}", c);
				}
				else
				{
					appearBuf += (char)c;
				}
			}
			appearBuf += ") Tj\n";

			// next line
			i     = k;
			xPrev = xx;
		}

		appearBuf += "ET\n";

		// single-line text
	}
	else
	{
		//~ replace newlines with spaces? - what does Acrobat do?

		// comb formatting
		if (comb > 0)
		{
			// compute comb spacing
			w = dx / comb;

			// compute font autosize
			if (fontSize == 0)
			{
				fontSize = dy - 2 * border;
				if (w < fontSize)
					fontSize = w;
				fontSize = floor(fontSize);
				if (fontSize > 10)
					fontSize = 10;
				if (tfPos >= 0)
					daToks[tfPos + 1] = fmt::format("{:.4f}", fontSize);
			}

			// compute text start position
			switch (quadding)
			{
			case acroFormQuadLeft:
			default:
				xx = 0;
				break;
			case acroFormQuadCenter:
				xx = ((comb - text2.size()) / 2) * w;
				break;
			case acroFormQuadRight:
				xx = (comb - text2.size()) * w;
				break;
			}
			if (font)
			{
				ascent  = font->getDeclaredAscent() * fontSize;
				descent = font->getDescent() * fontSize;
			}
			else
			{
				ascent  = 0.75 * fontSize;
				descent = -0.25 * fontSize;
			}
			switch (vAlign)
			{
			case acroFormVAlignTop:
			default:
				yy = dy - ascent;
				break;
			case acroFormVAlignMiddle:
				yy = 0.5 * (dy - ascent - descent);
				break;
			case acroFormVAlignMiddleNoDescender:
				yy = 0.5 * (dy - ascent);
				break;
			case acroFormVAlignBottom:
				yy = -descent;
				break;
			}

			appearBuf += "BT\n";

			// set the font matrix
			if (tmPos >= 0)
			{
				daToks[tmPos + 4] = fmt::format("{:.4f}", x + xx);
				daToks[tmPos + 5] = fmt::format("{:.4f}", y + yy);
			}

			// write the DA string
			if (daToks.size())
				for (i = 0; i < daToks.size(); ++i)
				{
					appearBuf += daToks.at(i);
					appearBuf += ' ';
				}

			// write the font matrix (if not part of the DA string)
			if (tmPos < 0)
				appearBuf += fmt::format("1 0 0 1 {:.4f} {:.4f} Tm\n", x + xx, y + yy);

			// write the text string
			offset = 0;
			for (i = 0; i < text2.size(); ++i)
			{
				c = text2.at(i) & 0xff;
				if (c >= 0x20 && c < 0x80)
				{
					if (font && !font->isCIDFont())
					{
						charWidth = ((Gfx8BitFont*)font)->getWidth((uint8_t)c) * fontSize;
					}
					else
					{
						// otherwise, make a crude estimate
						charWidth = 0.5 * fontSize;
					}
					offset2 = 0.5 * (w - charWidth);
					appearBuf += fmt::format("{:.4f} 0 Td\n", offset + offset2);
					if (c == '(' || c == ')' || c == '\\')
						appearBuf += fmt::format("(\\{}) Tj\n", c);
					else
						appearBuf += fmt::format("({}) Tj\n", c);
					offset = w - offset2;
				}
				else
				{
					offset += w;
				}
			}

			appearBuf += "ET\n";

			// regular (non-comb) formatting
		}
		else
		{
			// compute string width
			if (font && !font->isCIDFont())
			{
				w = 0;
				for (i = 0; i < text2.size(); ++i)
					w += ((Gfx8BitFont*)font)->getWidth(text2.at(i));
			}
			else
			{
				// otherwise, make a crude estimate
				w = text2.size() * 0.5;
			}

			// compute font autosize
			if (fontSize == 0)
			{
				fontSize  = dy - 2 * border;
				fontSize2 = (dx - 4 - 2 * border) / w;
				if (fontSize2 < fontSize)
					fontSize = fontSize2;
				fontSize = floor(fontSize);
				if (fontSize > 10)
					fontSize = 10;
				if (tfPos >= 0)
					daToks[tfPos + 1] = fmt::format("{:.4f}", fontSize);
			}

			// compute text start position
			w *= fontSize;
			switch (quadding)
			{
			case acroFormQuadLeft:
			default:
				xx = border + 2;
				break;
			case acroFormQuadCenter:
				xx = (dx - w) / 2;
				break;
			case acroFormQuadRight:
				xx = dx - border - 2 - w;
				break;
			}
			if (font)
			{
				ascent  = font->getDeclaredAscent() * fontSize;
				descent = font->getDescent() * fontSize;
			}
			else
			{
				ascent  = 0.75 * fontSize;
				descent = -0.25 * fontSize;
			}
			switch (vAlign)
			{
			case acroFormVAlignTop:
			default:
				yy = dy - ascent;
				break;
			case acroFormVAlignMiddle:
				yy = 0.5 * (dy - ascent - descent);
				break;
			case acroFormVAlignMiddleNoDescender:
				yy = 0.5 * (dy - ascent);
				break;
			case acroFormVAlignBottom:
				yy = -descent;
				break;
			}

			if (whiteBackground)
				appearBuf += fmt::format("q 1 g {:.4f} {:.4f} {:.4f} {:.4f} re f Q\n", xx - 0.25 * fontSize, yy - 0.35 * fontSize, w + 0.5 * fontSize, 1.2 * fontSize);

			appearBuf += "BT\n";

			// set the font matrix
			if (tmPos >= 0)
			{
				daToks[tmPos + 4] = fmt::format("{:.4f}", x + xx);
				daToks[tmPos + 5] = fmt::format("{:.4f}", y + yy);
			}

			// write the DA string
			if (daToks.size())
				for (i = 0; i < daToks.size(); ++i)
				{
					appearBuf += daToks.at(i);
					appearBuf += ' ';
				}

			// write the font matrix (if not part of the DA string)
			if (tmPos < 0)
				appearBuf += fmt::format("1 0 0 1 {:.4f} {:.4f} Tm\n", x + xx, y + yy);

			// write the text string
			appearBuf += '(';
			for (i = 0; i < text2.size(); ++i)
			{
				c = text2.at(i) & 0xff;
				if (c == '(' || c == ')' || c == '\\')
				{
					appearBuf += '\\';
					appearBuf += (char)c;
				}
				else if (c < 0x20 || c >= 0x80)
				{
					appearBuf += fmt::format("\\{:03o}", c);
				}
				else
				{
					appearBuf += (char)c;
				}
			}
			appearBuf += ") Tj\n";
		}

		appearBuf += "ET\n";
	}

	// cleanup
	appearBuf += "Q\n";
	if (txField)
		appearBuf += "EMC\n";
}

// Draw the variable text or caption for a field.
void AcroFormField::drawListBox(const VEC_STR& texts, bool* selection, int nOptions, int topIdx, const std::string& da, GfxFontDict* fontDict, int quadding, double xMin, double yMin, double xMax, double yMax, double border, std::string& appearBuf)
{
	VEC_STR daToks;
	double  fontSize, fontSize2, x, y, w, wMax;
	int     tfPos, tmPos;

	//~ if there is no MK entry, this should use the existing content stream,
	//~ and only replace the marked content portion of it
	//~ (this is only relevant for Tx fields)

	// parse the default appearance string
	tfPos = tmPos = -1;
	if (da.size())
	{
		daToks = tokenize(da);
		for (int i = 2; i < daToks.size(); ++i)
			if (i >= 2 && daToks.at(i) == "Tf")
				tfPos = i - 2;
			else if (i >= 6 && daToks.at(i) == "Tm")
				tmPos = i - 6;
	}
	else
	{
	}

	// get the font and font size
	GfxFont* font = nullptr;
	fontSize      = 0;
	if (tfPos >= 0)
	{
		auto tok = daToks.at(tfPos);
		if (tok.size() >= 1 && tok.at(0) == '/')
		{
			if (!fontDict || !(font = fontDict->lookup(tok.c_str() + 1)))
			{
				error(errSyntaxError, -1, "Unknown font in field's DA string");
				daToks[tfPos] = "/xpdf_default_font";
			}
		}
		else
		{
			error(errSyntaxError, -1, "Invalid font name in 'Tf' operator in field's DA string");
		}
		tok      = daToks.at(tfPos + 1);
		fontSize = atof(tok.c_str());
	}
	else
	{
		error(errSyntaxError, -1, "Missing 'Tf' operator in field's DA string");
	}

	// compute font autosize
	if (fontSize == 0)
	{
		wMax = 0;
		for (int i = 0; i < nOptions; ++i)
		{
			if (font && !font->isCIDFont())
			{
				w = 0;
				for (int j = 0; j < texts[i].size(); ++j)
					w += ((Gfx8BitFont*)font)->getWidth(texts[i].at(j));
			}
			else
			{
				// otherwise, make a crude estimate
				w = texts[i].size() * 0.5;
			}
			if (w > wMax)
				wMax = w;
		}
		fontSize  = yMax - yMin - 2 * border;
		fontSize2 = (xMax - xMin - 4 - 2 * border) / wMax;
		if (fontSize2 < fontSize)
			fontSize = fontSize2;
		fontSize = floor(fontSize);
		if (fontSize > 10)
			fontSize = 10;
		if (tfPos >= 0)
			daToks[tfPos + 1] = fmt::format("{:.4f}", fontSize);
	}

	// draw the text
	y = yMax - yMin - 1.1 * fontSize;
	for (int i = topIdx; i < nOptions; ++i)
	{
		// setup
		appearBuf += "q\n";

		// draw the background if selected
		if (selection[i])
		{
			appearBuf += "0 g f\n";
			appearBuf += fmt::format("{:.4f} {:.4f} {:.4f} {:.4f} re f\n", border, y - 0.2 * fontSize, xMax - xMin - 2 * border, 1.1 * fontSize);
		}

		// setup
		appearBuf += "BT\n";

		// compute string width
		if (font && !font->isCIDFont())
		{
			w = 0;
			for (int j = 0; j < texts[i].size(); ++j)
				w += ((Gfx8BitFont*)font)->getWidth(texts[i].at(j));
		}
		else
		{
			// otherwise, make a crude estimate
			w = texts[i].size() * 0.5;
		}

		// compute text start position
		w *= fontSize;
		switch (quadding)
		{
		case acroFormQuadLeft:
		default:
			x = border + 2;
			break;
		case acroFormQuadCenter:
			x = (xMax - xMin - w) / 2;
			break;
		case acroFormQuadRight:
			x = xMax - xMin - border - 2 - w;
			break;
		}

		// set the font matrix
		if (tmPos >= 0)
		{
			daToks[tmPos + 4] = fmt::format("{:.4f}", x);
			daToks[tmPos + 5] = fmt::format("{:.4f}", y);
		}

		// write the DA string
		if (daToks.size())
			for (int j = 0; j < daToks.size(); ++j)
			{
				appearBuf += daToks.at(j);
				appearBuf += ' ';
			}

		// write the font matrix (if not part of the DA string)
		if (tmPos < 0)
			appearBuf += fmt::format("1 0 0 1 {:.4f} {:.4f} Tm\n", x, y);

		// change the text color if selected
		if (selection[i])
			appearBuf += "1 g\n";

		// write the text string
		appearBuf += '(';
		for (int j = 0; j < texts[i].size(); ++j)
		{
			const char c = texts[i].at(j) & 0xff;
			if (c == '(' || c == ')' || c == '\\')
			{
				appearBuf += '\\';
				appearBuf += (char)c;
			}
			else if (c < 0x20 || c >= 0x80)
			{
				appearBuf += fmt::format("\\{:03o}", c);
			}
			else
			{
				appearBuf += (char)c;
			}
		}
		appearBuf += ") Tj\n";

		// cleanup
		appearBuf += "ET\n";
		appearBuf += "Q\n";

		// next line
		y -= 1.1 * fontSize;
	}
}

// Figure out how much text will fit on the next line.  Returns:
// *end = one past the last character to be included
// *width = width of the characters start .. end-1
// *next = index of first character on the following line
void AcroFormField::getNextLine(const std::string& text, int start, GfxFont* font, double fontSize, double wMax, int* end, double* width, int* next)
{
	double w, dw;
	int    j, k, c;

	// figure out how much text will fit on the line
	//~ what does Adobe do with tabs?
	w = 0;
	for (j = start; j < text.size() && w <= wMax; ++j)
	{
		c = text.at(j) & 0xff;
		if (c == 0x0a || c == 0x0d)
			break;
		if (font && !font->isCIDFont())
		{
			dw = ((Gfx8BitFont*)font)->getWidth((uint8_t)c) * fontSize;
		}
		else
		{
			// otherwise, make a crude estimate
			dw = 0.5 * fontSize;
		}
		w += dw;
	}
	if (w > wMax)
	{
		for (k = j; k > start && text.at(k - 1) != ' '; --k)
			;
		for (; k > start && text.at(k - 1) == ' '; --k)
			;
		if (k > start)
			j = k;
		if (j == start)
		{
			// handle the pathological case where the first character is
			// too wide to fit on the line all by itself
			j = start + 1;
		}
	}
	*end = j;

	// compute the width
	w = 0;
	for (k = start; k < j; ++k)
	{
		if (font && !font->isCIDFont())
		{
			dw = ((Gfx8BitFont*)font)->getWidth(text.at(k)) * fontSize;
		}
		else
		{
			// otherwise, make a crude estimate
			dw = 0.5 * fontSize;
		}
		w += dw;
	}
	*width = w;

	// next line
	while (j < text.size() && text.at(j) == ' ')
		++j;
	if (j < text.size() && text.at(j) == 0x0d)
		++j;
	if (j < text.size() && text.at(j) == 0x0a)
		++j;
	*next = j;
}

// Draw an (approximate) circle of radius <r> centered at (<cx>, <cy>).
// <cmd> is used to draw the circle ("f", "s", or "b").
void AcroFormField::drawCircle(double cx, double cy, double r, const char* cmd, std::string& appearBuf)
{
	appearBuf += fmt::format("{:.4f} {:.4f} m\n", cx + r, cy);
	appearBuf += fmt::format("{:.4f} {:.4f} {:.4f} {:.4f} {:.4f} {:.4f} c\n", cx + r, cy + bezierCircle * r, cx + bezierCircle * r, cy + r, cx, cy + r);
	appearBuf += fmt::format("{:.4f} {:.4f} {:.4f} {:.4f} {:.4f} {:.4f} c\n", cx - bezierCircle * r, cy + r, cx - r, cy + bezierCircle * r, cx - r, cy);
	appearBuf += fmt::format("{:.4f} {:.4f} {:.4f} {:.4f} {:.4f} {:.4f} c\n", cx - r, cy - bezierCircle * r, cx - bezierCircle * r, cy - r, cx, cy - r);
	appearBuf += fmt::format("{:.4f} {:.4f} {:.4f} {:.4f} {:.4f} {:.4f} c\n", cx + bezierCircle * r, cy - r, cx + r, cy - bezierCircle * r, cx + r, cy);
	appearBuf += fmt::format("{}\n", cmd);
}

// Draw the top-left half of an (approximate) circle of radius <r>
// centered at (<cx>, <cy>).
void AcroFormField::drawCircleTopLeft(double cx, double cy, double r, std::string& appearBuf)
{
	double r2;

	r2 = r / sqrt(2.0);
	appearBuf += fmt::format("{:.4f} {:.4f} m\n", cx + r2, cy + r2);
	appearBuf += fmt::format("{:.4f} {:.4f} {:.4f} {:.4f} {:.4f} {:.4f} c\n", cx + (1 - bezierCircle) * r2, cy + (1 + bezierCircle) * r2, cx - (1 - bezierCircle) * r2, cy + (1 + bezierCircle) * r2, cx - r2, cy + r2);
	appearBuf += fmt::format("{:.4f} {:.4f} {:.4f} {:.4f} {:.4f} {:.4f} c\n", cx - (1 + bezierCircle) * r2, cy + (1 - bezierCircle) * r2, cx - (1 + bezierCircle) * r2, cy - (1 - bezierCircle) * r2, cx - r2, cy - r2);
	appearBuf += "S\n";
}

// Draw the bottom-right half of an (approximate) circle of radius <r>
// centered at (<cx>, <cy>).
void AcroFormField::drawCircleBottomRight(double cx, double cy, double r, std::string& appearBuf)
{
	double r2;

	r2 = r / sqrt(2.0);
	appearBuf += fmt::format("{:.4f} {:.4f} m\n", cx - r2, cy - r2);
	appearBuf += fmt::format("{:.4f} {:.4f} {:.4f} {:.4f} {:.4f} {:.4f} c\n", cx - (1 - bezierCircle) * r2, cy - (1 + bezierCircle) * r2, cx + (1 - bezierCircle) * r2, cy - (1 + bezierCircle) * r2, cx + r2, cy - r2);
	appearBuf += fmt::format("{:.4f} {:.4f} {:.4f} {:.4f} {:.4f} {:.4f} c\n", cx + (1 + bezierCircle) * r2, cy - (1 - bezierCircle) * r2, cx + (1 + bezierCircle) * r2, cy + (1 - bezierCircle) * r2, cx + r2, cy + r2);
	appearBuf += "S\n";
}

void AcroFormField::drawBarcode(const std::string& value, const std::string& da, GfxFontDict* fontDict, int rot, double xMin, double yMin, double xMax, double yMax, XFAFieldBarcodeInfo* barcodeInfo, std::string& appearBuf)
{
	//--- handle rotation
	double w, h;
	appearBuf += "q\n";
	switch (rot)
	{
	case 0:
	default:
		w = xMax - xMin;
		h = yMax - yMin;
		break;
	case 90:
		appearBuf += fmt::format("0 1 -1 0 {:.4f} 0 cm\n", xMax - xMin);
		w = yMax - yMin;
		h = xMax - xMin;
		break;
	case 180:
		appearBuf += fmt::format("0 -1 1 0 0 {:.4f} cm\n", yMax - yMin);
		w = yMax - yMin;
		h = xMax - xMin;
		break;
	case 270:
		appearBuf += fmt::format("0 -1 1 0 0 {:.4f} cm\n", yMax - yMin);
		w = yMax - yMin;
		h = xMax - xMin;
		break;
	}

	//--- get the font size
	double fontSize = 0.2 * h;
	if (da.size())
	{
		const auto daToks = tokenize(da);
		for (int i = 2; i < daToks.size(); ++i)
		{
			if (daToks.at(i) == "Tf")
			{
				fontSize = atof(daToks.at(i - 1).c_str());
				break;
			}
		}
	}

	//--- compute the embedded text type position
	bool   doText          = true;
	double yText           = 0;
	int    vAlign          = acroFormVAlignTop;
	double yBarcode        = 0;
	double hBarcode        = 0;
	bool   whiteBackground = false;
	//~ this uses an estimate of the font baseline position
	if (barcodeInfo->textLocation == "above")
	{
		yText    = h;
		vAlign   = acroFormVAlignTop;
		yBarcode = 0;
		hBarcode = h - fontSize;
	}
	else if (barcodeInfo->textLocation == "belowEmbedded")
	{
		yText           = 0;
		vAlign          = acroFormVAlignBottom;
		yBarcode        = 0;
		hBarcode        = h;
		whiteBackground = true;
	}
	else if (barcodeInfo->textLocation == "aboveEmbedded")
	{
		yText           = h;
		vAlign          = acroFormVAlignTop;
		yBarcode        = 0;
		hBarcode        = h;
		whiteBackground = true;
	}
	else if (barcodeInfo->textLocation == "none")
	{
		doText = false;
	}
	else
	{ // default is "below"
		yText    = 0;
		vAlign   = acroFormVAlignBottom;
		yBarcode = fontSize;
		hBarcode = h - fontSize;
	}
	double wText = w;

	//--- remove extraneous start/stop chars
	std::string value2 = value;
	if (barcodeInfo->barcodeType == "code3Of9")
	{
		if (value2.size() >= 1 && value2.at(0) == '*') value2.erase(0, 1);
		if (value2.size() >= 1 && value2.back() == '*') value2.pop_back();
	}

	//--- draw the bar code
	if (barcodeInfo->barcodeType == "code3Of9")
	{
		if (!barcodeInfo->dataLength)
		{
			error(errSyntaxError, -1, "Missing 'dataLength' attribute in barcode field");
			return;
		}
		appearBuf += "0 g\n";
		const double wNarrow = w / ((7 + 3 * barcodeInfo->wideNarrowRatio) * (barcodeInfo->dataLength + 2));
		double       xx      = 0;
		for (int i = -1; i <= value2.size(); ++i)
		{
			int c;
			if (i < 0 || i >= value2.size())
				c = '*';
			else
				c = value2.at(i) & 0x7f;
			for (int j = 0; j < 10; j += 2)
			{
				appearBuf += fmt::format("{:.4f} {:.4f} {:.4f} {:.4f} re f\n", xx, yBarcode, (code3Of9Data[c][j] ? barcodeInfo->wideNarrowRatio : 1) * wNarrow, hBarcode);
				xx += ((code3Of9Data[c][j] ? barcodeInfo->wideNarrowRatio : 1) + (code3Of9Data[c][j + 1] ? barcodeInfo->wideNarrowRatio : 1)) * wNarrow;
			}
		}
		// center the text on the drawn barcode (not the max length barcode)
		wText = (value2.size() + 2) * (7 + 3 * barcodeInfo->wideNarrowRatio) * wNarrow;
	}
	else if (barcodeInfo->barcodeType == "code128B")
	{
		if (!barcodeInfo->dataLength)
		{
			error(errSyntaxError, -1, "Missing 'dataLength' attribute in barcode field");
			return;
		}
		appearBuf += "0 g\n";
		const double wNarrow  = w / (11 * (barcodeInfo->dataLength + 3) + 2);
		double       xx       = 0;
		int          checksum = 0;
		for (int i = -1; i <= value2.size() + 1; ++i)
		{
			int c;
			if (i == -1)
			{
				// start code B
				c = 104;
				checksum += c;
			}
			else if (i == value2.size())
			{
				// checksum
				c = checksum % 103;
			}
			else if (i == value2.size() + 1)
			{
				// stop code
				c = 106;
			}
			else
			{
				c = value2.at(i) & 0xff;
				if (c >= 32 && c <= 127)
					c -= 32;
				else
					c = 0;
				checksum += (i + 1) * c;
			}
			for (int j = 0; j < 6; j += 2)
			{
				appearBuf += fmt::format("{:.4f} {:.4f} {:.4f} {:.4f} re f\n", xx, yBarcode, code128Data[c][j] * wNarrow, hBarcode);
				xx += (code128Data[c][j] + code128Data[c][j + 1]) * wNarrow;
			}
		}
		// final bar of the stop code
		appearBuf += fmt::format("{:.4f} {:.4f} {:.4f} {:.4f} re f\n", xx, yBarcode, 2 * wNarrow, hBarcode);
		// center the text on the drawn barcode (not the max length barcode)
		wText = (11 * (value2.size() + 3) + 2) * wNarrow;
	}
	else if (barcodeInfo->barcodeType == "pdf417")
	{
		drawPDF417Barcode(w, h, barcodeInfo->moduleWidth, barcodeInfo->moduleHeight, barcodeInfo->errorCorrectionLevel, value2, appearBuf);
		doText = false;
	}
	else
	{
		error(errSyntaxError, -1, "Unimplemented barcode type '{}' in barcode field", barcodeInfo->barcodeType);
	}
	//~ add other barcode types here

	//--- draw the embedded text
	if (doText)
		drawText(value2, da, fontDict, false, 0, acroFormQuadCenter, vAlign, false, false, 0, 0, yText, wText, yText + fontSize, 0, whiteBackground, appearBuf);

	appearBuf += "Q\n";
}

VEC_STR AcroFormField::tokenize(const std::string& s)
{
	VEC_STR toks;
	int     j;

	int i = 0;
	while (i < s.size())
	{
		while (i < s.size() && Lexer::isSpace(s.at(i)))
			++i;
		if (i < s.size())
		{
			for (j = i + 1; j < s.size() && !Lexer::isSpace(s.at(j)); ++j)
				;
			toks.push_back(std::string(s, i, j - i));
			i = j;
		}
	}
	return toks;
}

Object* AcroFormField::getResources(Object* res)
{
	Object kidsObj, annotObj, obj1;
	int    i;

	if (acroForm->needAppearances)
	{
		fieldLookup("DR", res);
	}
	else
	{
		res->initArray(acroForm->doc->getXRef());
		// find the annotation object(s)
		if (fieldObj.dictLookup("Kids", &kidsObj)->isArray())
		{
			for (i = 0; i < kidsObj.arrayGetLength(); ++i)
			{
				kidsObj.arrayGet(i, &annotObj);
				if (annotObj.isDict())
				{
					if (getAnnotResources(annotObj.getDict(), &obj1)->isDict())
						res->arrayAdd(&obj1);
					else
						obj1.free();
				}
				annotObj.free();
			}
		}
		else
		{
			if (getAnnotResources(fieldObj.getDict(), &obj1)->isDict())
				res->arrayAdd(&obj1);
			else
				obj1.free();
		}
		kidsObj.free();
	}

	return res;
}

Object* AcroFormField::getFieldRef(Object* ref)
{
	return fieldRef.copy(ref);
}

Object* AcroFormField::getValueObj(Object* val)
{
	return fieldLookup("V", val);
}

Object* AcroFormField::getParentRef(Object* parent)
{
	return fieldObj.dictLookupNF("Parent", parent);
}

// Get the first annotation object associated with this field.
Object* AcroFormField::getAnnotObj(Object* annotObj)
{
	Object kidsObj;

	if (fieldObj.dictLookup("Kids", &kidsObj)->isArray())
		if (kidsObj.arrayGetLength() > 0)
			kidsObj.arrayGet(0, annotObj);
		else
			annotObj->initNull();
	else
		fieldObj.copy(annotObj);
	kidsObj.free();
	return annotObj;
}

Object* AcroFormField::getAnnotResources(Dict* annot, Object* res)
{
	Object apObj, asObj, appearance, obj1;

	// get the appearance stream
	if (annot->lookup("AP", &apObj)->isDict())
	{
		apObj.dictLookup("N", &obj1);
		if (obj1.isDict())
		{
			if (annot->lookup("AS", &asObj)->isName())
				obj1.dictLookup(asObj.getName(), &appearance);
			else if (obj1.dictGetLength() == 1)
				obj1.dictGetVal(0, &appearance);
			else
				obj1.dictLookup("Off", &appearance);
			asObj.free();
		}
		else
		{
			obj1.copy(&appearance);
		}
		obj1.free();
	}
	apObj.free();

	if (appearance.isStream())
		appearance.streamGetDict()->lookup("Resources", res);
	else
		res->initNull();
	appearance.free();

	return res;
}

// Merge the field and AcroForm DR objects.
void AcroFormField::buildDefaultResourceDict(Object* dr)
{
	Object formDR, fieldDR, resDict, newResDict, resObj;
	char * resType, *resName;
	int    i, j;

	// NB: we need to deep-copy the dictionaries here, because multiple
	// threads can be sharing these objects.

	dr->initDict(acroForm->doc->getXRef());

	acroForm->acroFormObj.dictLookup("DR", &formDR);
	if (formDR.isDict())
	{
		for (i = 0; i < formDR.dictGetLength(); ++i)
		{
			resType = formDR.dictGetKey(i);
			formDR.dictGetVal(i, &resDict);
			if (resDict.isDict())
			{
				newResDict.initDict(acroForm->doc->getXRef());
				dr->dictAdd(copyString(resType), &newResDict);
				for (j = 0; j < resDict.dictGetLength(); ++j)
				{
					resName = resDict.dictGetKey(j);
					resDict.dictGetValNF(j, &resObj);
					newResDict.dictAdd(copyString(resName), &resObj);
				}
			}
			resDict.free();
		}
	}
	formDR.free();

	fieldObj.dictLookup("DR", &fieldDR);
	if (fieldDR.isDict())
	{
		for (i = 0; i < fieldDR.dictGetLength(); ++i)
		{
			resType = fieldDR.dictGetKey(i);
			fieldDR.dictGetVal(i, &resDict);
			if (resDict.isDict())
			{
				dr->dictLookup(resType, &newResDict);
				if (!newResDict.isDict())
				{
					newResDict.free();
					newResDict.initDict(acroForm->doc->getXRef());
				}
				dr->dictAdd(copyString(resType), &newResDict);
				for (j = 0; j < resDict.dictGetLength(); ++j)
				{
					resName = resDict.dictGetKey(j);
					resDict.dictGetValNF(j, &resObj);
					newResDict.dictAdd(copyString(resName), &resObj);
				}
			}
			resDict.free();
		}
	}
	fieldDR.free();
}

// Look up an inheritable field dictionary entry.
Object* AcroFormField::fieldLookup(const char* key, Object* obj)
{
	return fieldLookup(fieldObj.getDict(), key, obj);
}

Object* AcroFormField::fieldLookup(Dict* dict, const char* key, Object* obj)
{
	Object parent, parent2;
	int    depth;

	if (!dict->lookup(key, obj)->isNull())
		return obj;
	obj->free();

	dict->lookup("Parent", &parent)->isDict();
	depth = 0;
	while (parent.isDict() && depth < maxFieldObjectDepth)
	{
		if (!parent.dictLookup(key, obj)->isNull())
		{
			parent.free();
			return obj;
		}
		obj->free();
		parent.dictLookup("Parent", &parent2);
		parent.free();
		parent = parent2;
		++depth;
	}
	parent.free();

	// some fields don't specify a parent, so we check the AcroForm
	// dictionary just in case
	acroForm->acroFormObj.dictLookup(key, obj);
	return obj;
}

Unicode* AcroFormField::utf8ToUnicode(const std::string& s, int* unicodeLength)
{
	int     n = 0;
	int     i = 0;
	Unicode u;
	while (getUTF8(s, &i, &u))
		++n;
	Unicode* uVec = (Unicode*)gmallocn(n, sizeof(Unicode));
	n             = 0;
	i             = 0;
	while (getUTF8(s, &i, &uVec[n]))
		++n;
	*unicodeLength = n;
	return uVec;
}

std::string AcroFormField::unicodeToLatin1(Unicode* u, int unicodeLength)
{
	std::string s;
	for (int i = 0; i < unicodeLength; ++i)
		if (u[i] <= 0xff)
			s += (char)u[i];
	return s;
}

bool AcroFormField::unicodeStringEqual(Unicode* u, int unicodeLength, const std::string& s)
{
	if (s.size() != unicodeLength)
		return false;
	for (int i = 0; i < unicodeLength; ++i)
		if ((Unicode)(s.at(i) & 0xff) != u[i])
			return false;
	return true;
}

bool AcroFormField::unicodeStringEqual(Unicode* u, int unicodeLength, const char* s)
{
	for (int i = 0; i < unicodeLength; ++i)
		if (!s[i] || (Unicode)(s[i] & 0xff) != u[i])
			return false;
	return true;
}

//------------------------------------------------------------------------
// 'picture' formatting
//------------------------------------------------------------------------

class PictureNode
{
public:
	virtual ~PictureNode() {}

	virtual bool isLiteral() { return false; }

	virtual bool isSign() { return false; }

	virtual bool isDigit() { return false; }

	virtual bool isDecPt() { return false; }

	virtual bool isSeparator() { return false; }

	virtual bool isYear() { return false; }

	virtual bool isMonth() { return false; }

	virtual bool isDay() { return false; }

	virtual bool isHour() { return false; }

	virtual bool isMinute() { return false; }

	virtual bool isSecond() { return false; }

	virtual bool isChar() { return false; }
};

class PictureLiteral : public PictureNode
{
public:
	PictureLiteral(const std::string& sA) { s = sA; }

	virtual ~PictureLiteral() {}

	virtual bool isLiteral() { return true; }

	std::string s;
};

class PictureSign : public PictureNode
{
public:
	PictureSign(char cA) { c = cA; }

	virtual bool isSign() { return true; }

	char c;
};

class PictureDigit : public PictureNode
{
public:
	PictureDigit(char cA)
	{
		c   = cA;
		pos = 0;
	}

	virtual bool isDigit() { return true; }

	char c;
	int  pos;
};

class PictureDecPt : public PictureNode
{
public:
	PictureDecPt() {}

	virtual bool isDecPt() { return true; }
};

class PictureSeparator : public PictureNode
{
public:
	PictureSeparator() {}

	virtual bool isSeparator() { return true; }
};

class PictureYear : public PictureNode
{
public:
	PictureYear(int nDigitsA) { nDigits = nDigitsA; }

	virtual bool isYear() { return true; }

	int nDigits;
};

class PictureMonth : public PictureNode
{
public:
	PictureMonth(int nDigitsA) { nDigits = nDigitsA; }

	virtual bool isMonth() { return true; }

	int nDigits;
};

class PictureDay : public PictureNode
{
public:
	PictureDay(int nDigitsA) { nDigits = nDigitsA; }

	virtual bool isDay() { return true; }

	int nDigits;
};

class PictureHour : public PictureNode
{
public:
	PictureHour(bool is24HourA, int nDigitsA)
	{
		is24Hour = is24HourA;
		nDigits  = nDigitsA;
	}

	virtual bool isHour() { return true; }

	bool is24Hour;
	int  nDigits;
};

class PictureMinute : public PictureNode
{
public:
	PictureMinute(int nDigitsA) { nDigits = nDigitsA; }

	virtual bool isMinute() { return true; }

	int nDigits;
};

class PictureSecond : public PictureNode
{
public:
	PictureSecond(int nDigitsA) { nDigits = nDigitsA; }

	virtual bool isSecond() { return true; }

	int nDigits;
};

class PictureChar : public PictureNode
{
public:
	PictureChar() {}

	virtual bool isChar() { return true; }
};

std::string AcroFormField::pictureFormatDateTime(const std::string& value, const std::string& picture)
{
	int year, month, day, hour, min, sec;
	int u, n, i, j;

	const int len = TO_INT(value.size());
	if (len == 0) return value;

	//--- parse the value

	// expected format is yyyy(-mm(-dd)?)?Thh(:mm(:ss)?)?
	// where:
	// - the '-'s and ':'s are optional
	// - the 'T' is literal
	// - we're ignoring optional time zone info at the end
	// (if the value is not in this canonical format, we just punt and
	// return the value string)
	//~ another option would be to parse the value following the
	//~   <ui><picture> element
	year = month = day = hour = min = sec = 0;
	i                                     = 0;
	if (!(i + 4 <= len && isValidInt(value, i, 4)))
		return value;
	year = convertInt(value, i, 4);
	i += 4;
	if (i < len && value.at(i) == '-')
		++i;
	if (i + 2 <= len && isValidInt(value, i, 2))
	{
		month = convertInt(value, i, 2);
		i += 2;
		if (i < len && value.at(i) == '-')
			++i;
		if (i + 2 <= len && isValidInt(value, i, 2))
		{
			day = convertInt(value, i, 2);
			i += 2;
		}
	}
	if (i < len)
	{
		if (value.at(i) != 'T')
			return value;
		++i;
		if (!(i + 2 <= len && isValidInt(value, i, 2)))
			return value;
		hour = convertInt(value, i, 2);
		i += 2;
		if (i < len && value.at(i) == ':')
			++i;
		if (i + 2 <= len && isValidInt(value, i, 2))
		{
			min = convertInt(value, i, 2);
			i += 2;
			if (i < len && value.at(i) == ':')
				++i;
			if (i + 2 <= len && isValidInt(value, i, 2))
			{
				sec = convertInt(value, i, 2);
				i += 2;
			}
		}
	}
	if (i < len)
		return value;

	//--- skip the category and locale in the picture
	int picStart = 0;
	int picEnd   = TO_INT(picture.size());
	for (int i = 0; i < TO_INT(picture.size()); ++i)
	{
		const char c = picture.at(i);
		if (c == '{')
		{
			picStart = i + 1;
			for (picEnd = picStart; picEnd < picture.size() && picture.at(picEnd) != '}'; ++picEnd)
				;
			break;
		}
		else if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '(' || c == ')'))
		{
			break;
		}
	}

	//--- parse the picture
	std::vector<PictureNode> pictures;
	i = picStart;
	while (i < picEnd)
	{
		char c = picture.at(i);
		++i;
		if (c == '\'')
		{
			std::string s;
			while (i < picEnd)
			{
				c = picture.at(i);
				if (c == '\'')
				{
					++i;
					if (i < picEnd && picture.at(i) == '\'')
					{
						s += '\'';
						++i;
					}
					else
					{
						break;
					}
				}
				else if (c == '\\')
				{
					++i;
					if (i == picEnd)
						break;
					c = picture.at(i);
					++i;
					if (c == 'u' && i + 4 <= picEnd)
					{
						u = 0;
						for (j = 0; j < 4; ++j, ++i)
						{
							c = picture.at(i);
							u <<= 4;
							if (c >= '0' && c <= '9')
								u += c - '0';
							else if (c >= 'a' && c <= 'f')
								u += c - 'a' + 10;
							else if (c >= 'A' && c <= 'F')
								u += c - 'A' + 10;
						}
						//~ this should convert to UTF-8 (?)
						if (u <= 0xff)
							s += (char)u;
					}
					else
					{
						s += c;
					}
				}
				else
				{
					s += c;
				}
			}
			pictures.push_back(PictureLiteral(s));
		}
		else if (c == ',' || c == '-' || c == ':' || c == '/' || c == '.' || c == ' ')
		{
			std::string s;
			s += c;
			pictures.push_back(PictureLiteral(s));
		}
		else if (c == 'Y')
		{
			for (n = 1; n < 4 && i < picEnd && picture.at(i) == 'Y'; ++n, ++i)
				;
			pictures.push_back(PictureYear(n));
		}
		else if (c == 'M')
		{
			for (n = 1; n < 2 && i < picEnd && picture.at(i) == 'M'; ++n, ++i)
				;
			pictures.push_back(PictureMonth(n));
		}
		else if (c == 'D')
		{
			for (n = 1; n < 2 && i < picEnd && picture.at(i) == 'D'; ++n, ++i)
				;
			pictures.push_back(PictureDay(n));
		}
		else if (c == 'h')
		{
			for (n = 1; n < 2 && i < picEnd && picture.at(i) == 'h'; ++n, ++i)
				;
			pictures.push_back(PictureHour(false, n));
		}
		else if (c == 'H')
		{
			for (n = 1; n < 2 && i < picEnd && picture.at(i) == 'H'; ++n, ++i)
				;
			pictures.push_back(PictureHour(true, n));
		}
		else if (c == 'M')
		{
			for (n = 1; n < 2 && i < picEnd && picture.at(i) == 'M'; ++n, ++i)
				;
			pictures.push_back(PictureMinute(n));
		}
		else if (c == 'S')
		{
			for (n = 1; n < 2 && i < picEnd && picture.at(i) == 'S'; ++n, ++i)
				;
			pictures.push_back(PictureSecond(n));
		}
	}

	//--- generate formatted text
	std::string ret;
	for (auto& pic : pictures)
	{
		auto node = (PictureNode*)&pic;
		if (node->isLiteral())
		{
			ret += ((PictureLiteral*)node)->s;
		}
		else if (node->isYear())
		{
			if (((PictureYear*)node)->nDigits == 2)
				if (year >= 1930 && year < 2030)
					ret += fmt::format("{:02}", year % 100);
				else
					ret += "??";
			else
				ret += fmt::format("{:04}", year);
		}
		else if (node->isMonth())
		{
			if (((PictureMonth*)node)->nDigits == 1)
				ret += fmt::format("{}", month);
			else
				ret += fmt::format("{:02}", month);
		}
		else if (node->isDay())
		{
			if (((PictureDay*)node)->nDigits == 1)
				ret += fmt::format("{}", day);
			else
				ret += fmt::format("{:02}", day);
		}
		else if (node->isHour())
		{
			if (((PictureHour*)node)->is24Hour)
			{
				n = hour;
			}
			else
			{
				n = hour % 12;
				if (n == 0)
					n = 12;
			}
			if (((PictureHour*)node)->nDigits == 1)
				ret += fmt::format("{}", n);
			else
				ret += fmt::format("{:02}", n);
		}
		else if (node->isMinute())
		{
			if (((PictureMinute*)node)->nDigits == 1)
				ret += fmt::format("{}", min);
			else
				ret += fmt::format("{:02}", min);
		}
		else if (node->isSecond())
		{
			if (((PictureSecond*)node)->nDigits == 1)
				ret += fmt::format("{}", sec);
			else
				ret += fmt::format("{:02}", sec);
		}
	}

	return ret;
}

std::string AcroFormField::pictureFormatNumber(const std::string& value, const std::string& picture)
{
	const int len = TO_INT(value.size());
	if (!len) return value;

	//--- parse the value

	// -nnnn.nnnn0000
	//  ^   ^    ^   ^
	//  |   |    |   +-- len
	//  |   |    +------ trailingZero
	//  |   +----------- decPt
	//  +--------------- start
	int  start = 0;
	bool neg   = false;
	if (value.at(start) == '-')
	{
		neg = true;
		++start;
	}
	else if (value.at(start) == '+')
	{
		++start;
	}

	int decPt, trailingZero;
	for (decPt = start; decPt < len && value.at(decPt) != '.'; ++decPt)
		;
	for (trailingZero = len; trailingZero > decPt && value.at(trailingZero - 1) == '0'; --trailingZero)
		;

	//--- skip the category and locale in the picture

	int picStart = 0;
	int picEnd   = TO_INT(picture.size());
	for (int i = 0; i < TO_INT(picture.size()); ++i)
	{
		const char c = picture.at(i);
		if (c == '{')
		{
			picStart = i + 1;
			for (picEnd = picStart; picEnd < picture.size() && picture.at(picEnd) != '}'; ++picEnd)
				;
			break;
		}
		else if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '(' || c == ')'))
		{
			break;
		}
	}

	//--- parse the picture
	std::vector<PictureNode> pictures;
	int                      i = picStart;
	while (i < picEnd)
	{
		char c = picture.at(i);
		++i;
		if (c == '\'')
		{
			std::string s;
			while (i < picEnd)
			{
				c = picture.at(i);
				if (c == '\'')
				{
					++i;
					if (i < picEnd && picture.at(i) == '\'')
					{
						s += '\'';
						++i;
					}
					else
					{
						break;
					}
				}
				else if (c == '\\')
				{
					++i;
					if (i == picEnd)
						break;
					c = picture.at(i);
					++i;
					if (c == 'u' && i + 4 <= picEnd)
					{
						int u = 0;
						for (int j = 0; j < 4; ++j, ++i)
						{
							c = picture.at(i);
							u <<= 4;
							if (c >= '0' && c <= '9')
								u += c - '0';
							else if (c >= 'a' && c <= 'F')
								u += c - 'a' + 10;
							else if (c >= 'A' && c <= 'F')
								u += c - 'A' + 10;
						}
						//~ this should convert to UTF-8 (?)
						if (u <= 0xff)
							s += (char)u;
					}
					else
					{
						s += c;
					}
				}
				else
				{
					s += c;
					++i;
				}
			}
			pictures.push_back(PictureLiteral(s));
		}
		else if (c == '-' || c == ':' || c == '/' || c == ' ')
		{
			std::string s;
			s += c;
			pictures.push_back(PictureLiteral(s));
		}
		else if (c == 's' || c == 'S')
		{
			pictures.push_back(PictureSign(c));
		}
		else if (c == 'Z' || c == 'z' || c == '8' || c == '9')
		{
			pictures.push_back(PictureDigit(c));
		}
		else if (c == '.')
		{
			pictures.push_back(PictureDecPt());
		}
		else if (c == ',')
		{
			pictures.push_back(PictureSeparator());
		}
	}
	for (i = 0; auto& pic : pictures)
	{
		auto node = (PictureNode*)&pic;
		if (node->isDecPt()) break;
		++i;
	}
	int pos = 0;
	for (int j = i - 1; j >= 0; --j)
	{
		auto node = (PictureNode*)&pictures[j];
		if (node->isDigit())
		{
			((PictureDigit*)node)->pos = pos;
			++pos;
		}
	}
	pos = -1;
	for (int j = i + 1; j < pictures.size(); ++j)
	{
		auto node = (PictureNode*)&pictures[j];
		if (node->isDigit())
		{
			((PictureDigit*)node)->pos = pos;
			--pos;
		}
	}

	//--- generate formatted text
	std::string ret;
	bool        haveDigits = false;
	for (int i = -1; auto& pic : pictures)
	{
		++i;
		auto node = (PictureNode*)&pic;
		if (node->isLiteral())
		{
			ret += ((PictureLiteral*)node)->s;
		}
		else if (node->isSign())
		{
			if (((PictureSign*)node)->c == 'S')
				ret += (neg ? '-' : ' ');
			else if (neg)
				ret += '-';
		}
		else if (node->isDigit())
		{
			const int  pos = ((PictureDigit*)node)->pos;
			const char c   = ((PictureDigit*)node)->c;
			if (pos >= 0 && pos < decPt - start)
			{
				ret += value.at(decPt - 1 - pos);
				haveDigits = true;
			}
			else if (pos < 0 && -pos <= trailingZero - decPt - 1)
			{
				ret += value.at(decPt - pos);
				haveDigits = true;
			}
			else if (c == '8' && pos < 0 && -pos <= len - decPt - 1)
			{
				ret += '0';
				haveDigits = true;
			}
			else if (c == '9')
			{
				ret += '0';
				haveDigits = true;
			}
			else if (c == 'Z' && pos >= 0)
			{
				ret += ' ';
			}
		}
		else if (node->isDecPt())
		{
			if (!(i + 1 < pictures.size() && ((PictureNode*)&pictures[i + 1])->isDigit() && ((PictureDigit*)&pictures[i + 1])->c == 'z') || trailingZero > decPt + 1)
				ret += '.';
		}
		else if (node->isSeparator())
		{
			if (haveDigits)
				ret += ',';
		}
	}

	return ret;
}

std::string AcroFormField::pictureFormatText(const std::string& value, const std::string& picture)
{
	const int len = TO_INT(value.size());
	if (len == 0) return value;

	//--- skip the category and locale in the picture

	int picStart = 0;
	int picEnd   = TO_INT(picture.size());
	for (int i = 0; i < picture.size(); ++i)
	{
		const char c = picture.at(i);
		if (c == '{')
		{
			picStart = i + 1;
			for (picEnd = picStart; picEnd < picture.size() && picture.at(picEnd) != '}'; ++picEnd)
				;
			break;
		}
		else if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '(' || c == ')'))
		{
			break;
		}
	}

	//--- parse the picture
	std::vector<PictureNode> pictures;
	int                      i = picStart;
	while (i < picEnd)
	{
		char c = picture.at(i);
		++i;
		if (c == '\'')
		{
			std::string s;
			while (i < picEnd)
			{
				c = picture.at(i);
				if (c == '\'')
				{
					++i;
					if (i < picEnd && picture.at(i) == '\'')
					{
						s += '\'';
						++i;
					}
					else
					{
						break;
					}
				}
				else if (c == '\\')
				{
					++i;
					if (i == picEnd)
						break;
					c = picture.at(i);
					++i;
					if (c == 'u' && i + 4 <= picEnd)
					{
						int u = 0;
						for (int j = 0; j < 4; ++j, ++i)
						{
							c = picture.at(i);
							u <<= 4;
							if (c >= '0' && c <= '9')
								u += c - '0';
							else if (c >= 'a' && c <= 'F')
								u += c - 'a' + 10;
							else if (c >= 'A' && c <= 'F')
								u += c - 'A' + 10;
						}
						//~ this should convert to UTF-8 (?)
						if (u <= 0xff)
							s += (char)u;
					}
					else
					{
						s += c;
					}
				}
				else
				{
					s += c;
					++i;
				}
			}
			pictures.push_back(PictureLiteral(s));
		}
		else if (c == ',' || c == '-' || c == ':' || c == '/' || c == '.' || c == ' ')
		{
			std::string s;
			s += c;
			pictures.push_back(PictureLiteral(s));
		}
		else if (c == 'A' || c == 'X' || c == 'O' || c == '0' || c == '9')
		{
			pictures.push_back(PictureChar());
		}
	}

	//--- generate formatted text

	std::string ret;
	for (int j = 0; const auto& pic : pictures)
	{
		const auto& node = (PictureNode*)&pic;
		if (node->isLiteral())
		{
			ret += ((PictureLiteral*)node)->s;
		}
		else if (node->isChar())
		{
			// if there are more chars in the picture than in the value,
			// Adobe renders the value as-is, without picture formatting
			if (j >= value.size())
			{
				ret = value;
				break;
			}
			ret += value.at(j);
			++j;
		}
	}

	return ret;
}

bool AcroFormField::isValidInt(const std::string& s, int start, int len)
{
	for (int i = 0; i < len; ++i)
		if (!(start + i < s.size() && s.at(start + i) >= '0' && s.at(start + i) <= '9'))
			return false;
	return true;
}

int AcroFormField::convertInt(const std::string& s, int start, int len)
{
	int x = 0;
	for (int i = 0; i < len && start + i < s.size(); ++i)
	{
		const char c = s.at(start + i);
		if (c < '0' || c > '9') break;
		x = x * 10 + (c - '0');
	}
	return x;
}
