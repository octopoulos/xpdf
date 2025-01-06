//========================================================================
//
// PSOutputDev.cc
//
// Copyright 1996-2013 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <signal.h>
#include <math.h>
#include "gmempp.h"
#include "GString.h" // Round4, Round6
#include "GList.h"
#include "config.h"
#include "GlobalParams.h"
#include "Object.h"
#include "Error.h"
#include "Function.h"
#include "Gfx.h"
#include "GfxState.h"
#include "GfxFont.h"
#include "UnicodeMap.h"
#include "FoFiType1C.h"
#include "FoFiTrueType.h"
#include "Catalog.h"
#include "Page.h"
#include "Stream.h"
#include "Annot.h"
#include "PDFDoc.h"
#include "XRef.h"
#include "PreScanOutputDev.h"
#include "CharCodeToUnicode.h"
#include "AcroForm.h"
#include "TextString.h"
#if HAVE_SPLASH
#	include "Splash.h"
#	include "SplashBitmap.h"
#	include "SplashOutputDev.h"
#endif
#include "PSOutputDev.h"

// the MSVC math.h doesn't define this
#ifndef M_PI
#	define M_PI 3.14159265358979323846
#endif

//------------------------------------------------------------------------
// PostScript prolog and setup
//------------------------------------------------------------------------

// The '~' escapes mark prolog code that is emitted only in certain
// levels:
//
//   ~[123][ngs]
//      ^   ^----- n=psLevel_, g=psLevel_Gray, s=psLevel_Sep
//      +----- 1=psLevel1__, 2=psLevel2__, 3=psLevel3__

static const char* prolog[] = {
	"/xpdf 75 dict def xpdf begin",
	"% PDF special state",
	"/pdfDictSize 15 def",
	"~1ns",
	"/pdfStates 64 array def",
	"  0 1 63 {",
	"    pdfStates exch pdfDictSize dict",
	"    dup /pdfStateIdx 3 index put",
	"    put",
	"  } for",
	"~123ngs",
	"/bdef { bind def } bind def",
	"/pdfSetup {",
	"  /pdfDuplex exch def",
	"  /setpagedevice where {",
	"    pop 2 dict begin",
	"      /Policies 1 dict dup begin /PageSize 6 def end def",
	"      pdfDuplex { /Duplex true def } if",
	"    currentdict end setpagedevice",
	"  } if",
	"  /pdfPageW 0 def",
	"  /pdfPageH 0 def",
	"} def",
	"/pdfSetupPaper {",
	"  2 copy pdfPageH ne exch pdfPageW ne or {",
	"    /pdfPageH exch def",
	"    /pdfPageW exch def",
	"    /setpagedevice where {",
	"      pop 3 dict begin",
	"        /PageSize [pdfPageW pdfPageH] def",
	"        pdfDuplex { /Duplex true def } if",
	"        /ImagingBBox null def",
	"      currentdict end setpagedevice",
	"    } if",
	"  } {",
	"    pop pop",
	"  } ifelse",
	"} def",
	"~1ns",
	"/pdfOpNames [",
	"  /pdfFill /pdfStroke /pdfLastFill /pdfLastStroke",
	"  /pdfTextMat /pdfFontSize /pdfCharSpacing /pdfTextRender",
	"  /pdfTextRise /pdfWordSpacing /pdfHorizScaling /pdfTextPath",
	"  /pdfTextClipPath",
	"] def",
	"~123ngs",
	"/pdfStartPage {",
	"~1ns",
	"  pdfStates 0 get begin",
	"~23ngs",
	"  pdfDictSize dict begin",
	"~23n",
	"  /pdfFillCS [] def",
	"  /pdfFillXform {} def",
	"  /pdfStrokeCS [] def",
	"  /pdfStrokeXform {} def",
	"~1n",
	"  /pdfFill 0 def",
	"  /pdfStroke 0 def",
	"~1s",
	"  /pdfFill [0 0 0 1] def",
	"  /pdfStroke [0 0 0 1] def",
	"~23g",
	"  /pdfFill 0 def",
	"  /pdfStroke 0 def",
	"~23ns",
	"  /pdfFill [0] def",
	"  /pdfStroke [0] def",
	"  /pdfFillOP false def",
	"  /pdfStrokeOP false def",
	"~123ngs",
	"  /pdfLastFill false def",
	"  /pdfLastStroke false def",
	"  /pdfTextMat [1 0 0 1 0 0] def",
	"  /pdfFontSize 0 def",
	"  /pdfCharSpacing 0 def",
	"  /pdfTextRender 0 def",
	"  /pdfTextRise 0 def",
	"  /pdfWordSpacing 0 def",
	"  /pdfHorizScaling 1 def",
	"  /pdfTextPath [] def",
	"  /pdfTextClipPath [] def",
	"} def",
	"/pdfEndPage { end } def",
	"~23s",
	"% separation convention operators",
	"/findcmykcustomcolor where {",
	"  pop",
	"}{",
	"  /findcmykcustomcolor { 5 array astore } def",
	"} ifelse",
	"/setcustomcolor where {",
	"  pop",
	"}{",
	"  /setcustomcolor {",
	"    exch",
	"    [ exch /Separation exch dup 4 get exch /DeviceCMYK exch",
	"      0 4 getinterval cvx",
	"      [ exch /dup load exch { mul exch dup } /forall load",
	"        /pop load dup ] cvx",
	"    ] setcolorspace setcolor",
	"  } def",
	"} ifelse",
	"/customcolorimage where {",
	"  pop",
	"}{",
	"  /customcolorimage {",
	"    gsave",
	"    [ exch /Separation exch dup 4 get exch /DeviceCMYK exch",
	"      0 4 getinterval",
	"      [ exch /dup load exch { mul exch dup } /forall load",
	"        /pop load dup ] cvx",
	"    ] setcolorspace",
	"    10 dict begin",
	"      /ImageType 1 def",
	"      /DataSource exch def",
	"      /ImageMatrix exch def",
	"      /BitsPerComponent exch def",
	"      /Height exch def",
	"      /Width exch def",
	"      /Decode [1 0] def",
	"    currentdict end",
	"    image",
	"    grestore",
	"  } def",
	"} ifelse",
	"~123ngs",
	"% PDF color state",
	"~1n",
	"/g { dup /pdfFill exch def setgray",
	"     /pdfLastFill true def /pdfLastStroke false def } def",
	"/G { dup /pdfStroke exch def setgray",
	"     /pdfLastStroke true def /pdfLastFill false def } def",
	"/fCol {",
	"  pdfLastFill not {",
	"    pdfFill setgray",
	"    /pdfLastFill true def /pdfLastStroke false def",
	"  } if",
	"} def",
	"/sCol {",
	"  pdfLastStroke not {",
	"    pdfStroke setgray",
	"    /pdfLastStroke true def /pdfLastFill false def",
	"  } if",
	"} def",
	"~1s",
	"/k { 4 copy 4 array astore /pdfFill exch def setcmykcolor",
	"     /pdfLastFill true def /pdfLastStroke false def } def",
	"/K { 4 copy 4 array astore /pdfStroke exch def setcmykcolor",
	"     /pdfLastStroke true def /pdfLastFill false def } def",
	"/fCol {",
	"  pdfLastFill not {",
	"    pdfFill aload pop setcmykcolor",
	"    /pdfLastFill true def /pdfLastStroke false def",
	"  } if",
	"} def",
	"/sCol {",
	"  pdfLastStroke not {",
	"    pdfStroke aload pop setcmykcolor",
	"    /pdfLastStroke true def /pdfLastFill false def",
	"  } if",
	"} def",
	"~23n",
	"/cs { /pdfFillXform exch def dup /pdfFillCS exch def",
	"      setcolorspace } def",
	"/CS { /pdfStrokeXform exch def dup /pdfStrokeCS exch def",
	"      setcolorspace } def",
	"/sc { pdfLastFill not {",
	"        pdfFillCS setcolorspace pdfFillOP setoverprint",
	"      } if",
	"      dup /pdfFill exch def aload pop pdfFillXform setcolor",
	"      /pdfLastFill true def /pdfLastStroke false def } def",
	"/SC { pdfLastStroke not {",
	"        pdfStrokeCS setcolorspace pdfStrokeOP setoverprint",
	"      } if",
	"      dup /pdfStroke exch def aload pop pdfStrokeXform setcolor",
	"      /pdfLastStroke true def /pdfLastFill false def } def",
	"/op { /pdfFillOP exch def",
	"      pdfLastFill { pdfFillOP setoverprint } if } def",
	"/OP { /pdfStrokeOP exch def",
	"      pdfLastStroke { pdfStrokeOP setoverprint } if } def",
	"/fCol {",
	"  pdfLastFill not {",
	"    pdfFillCS setcolorspace",
	"    pdfFill aload pop pdfFillXform setcolor",
	"    pdfFillOP setoverprint",
	"    /pdfLastFill true def /pdfLastStroke false def",
	"  } if",
	"} def",
	"/sCol {",
	"  pdfLastStroke not {",
	"    pdfStrokeCS setcolorspace",
	"    pdfStroke aload pop pdfStrokeXform setcolor",
	"    pdfStrokeOP setoverprint",
	"    /pdfLastStroke true def /pdfLastFill false def",
	"  } if",
	"} def",
	"~23g",
	"/g { dup /pdfFill exch def setgray",
	"     /pdfLastFill true def /pdfLastStroke false def } def",
	"/G { dup /pdfStroke exch def setgray",
	"     /pdfLastStroke true def /pdfLastFill false def } def",
	"/fCol {",
	"  pdfLastFill not {",
	"    pdfFill setgray",
	"    /pdfLastFill true def /pdfLastStroke false def",
	"  } if",
	"} def",
	"/sCol {",
	"  pdfLastStroke not {",
	"    pdfStroke setgray",
	"    /pdfLastStroke true def /pdfLastFill false def",
	"  } if",
	"} def",
	"~23s",
	"/k { 4 copy 4 array astore /pdfFill exch def setcmykcolor",
	"     pdfFillOP setoverprint",
	"     /pdfLastFill true def /pdfLastStroke false def } def",
	"/K { 4 copy 4 array astore /pdfStroke exch def setcmykcolor",
	"     pdfStrokeOP setoverprint",
	"     /pdfLastStroke true def /pdfLastFill false def } def",
	"/ck { 6 copy 6 array astore /pdfFill exch def",
	"      findcmykcustomcolor exch setcustomcolor",
	"      pdfFillOP setoverprint",
	"      /pdfLastFill true def /pdfLastStroke false def } def",
	"/CK { 6 copy 6 array astore /pdfStroke exch def",
	"      findcmykcustomcolor exch setcustomcolor",
	"      pdfStrokeOP setoverprint",
	"      /pdfLastStroke true def /pdfLastFill false def } def",
	"/op { /pdfFillOP exch def",
	"      pdfLastFill { pdfFillOP setoverprint } if } def",
	"/OP { /pdfStrokeOP exch def",
	"      pdfLastStroke { pdfStrokeOP setoverprint } if } def",
	"/fCol {",
	"  pdfLastFill not {",
	"    pdfFill aload length 4 eq {",
	"      setcmykcolor",
	"    }{",
	"      findcmykcustomcolor exch setcustomcolor",
	"    } ifelse",
	"    pdfFillOP setoverprint",
	"    /pdfLastFill true def /pdfLastStroke false def",
	"  } if",
	"} def",
	"/sCol {",
	"  pdfLastStroke not {",
	"    pdfStroke aload length 4 eq {",
	"      setcmykcolor",
	"    }{",
	"      findcmykcustomcolor exch setcustomcolor",
	"    } ifelse",
	"    pdfStrokeOP setoverprint",
	"    /pdfLastStroke true def /pdfLastFill false def",
	"  } if",
	"} def",
	"~3ns",
	"/opm {",
	"  /setoverprintmode where { pop setoverprintmode } { pop } ifelse",
	"} def",
	"~123ngs",
	"% build a font",
	"/pdfMakeFont {",
	"  4 3 roll findfont",
	"  4 2 roll matrix scale makefont",
	"  dup length dict begin",
	"    { 1 index /FID ne { def } { pop pop } ifelse } forall",
	"    /Encoding exch def",
	"    currentdict",
	"  end",
	"  definefont pop",
	"} def",
	"/pdfMakeFont16 {",
	"  exch findfont",
	"  dup length dict begin",
	"    { 1 index /FID ne { def } { pop pop } ifelse } forall",
	"    /WMode exch def",
	"    currentdict",
	"  end",
	"  definefont pop",
	"} def",
	"~3ngs",
	"/pdfMakeFont16L3 {",
	"  1 index /CIDFont resourcestatus {",
	"    pop pop 1 index /CIDFont findresource /CIDFontType known",
	"  } {",
	"    false",
	"  } ifelse",
	"  {",
	"    0 eq { /Identity-H } { /Identity-V } ifelse",
	"    exch 1 array astore composefont pop",
	"  } {",
	"    pdfMakeFont16",
	"  } ifelse",
	"} def",
	"~123ngs",
	"% graphics state operators",
	"~1ns",
	"/q {",
	"  gsave",
	"  pdfOpNames length 1 sub -1 0 { pdfOpNames exch get load } for",
	"  pdfStates pdfStateIdx 1 add get begin",
	"  pdfOpNames { exch def } forall",
	"} def",
	"/Q { end grestore } def",
	"~23ngs",
	"/q { gsave pdfDictSize dict begin } def",
	"/Q {",
	"  end grestore",
	"} def",
	"~123ngs",
	"/cm { concat } def",
	"/d { setdash } def",
	"/i { setflat } def",
	"/j { setlinejoin } def",
	"/J { setlinecap } def",
	"/M { setmiterlimit } def",
	"/w { setlinewidth } def",
	"% path segment operators",
	"/m { moveto } def",
	"/l { lineto } def",
	"/c { curveto } def",
	"/re { 4 2 roll moveto 1 index 0 rlineto 0 exch rlineto",
	"      neg 0 rlineto closepath } def",
	"/h { closepath } def",
	"% path painting operators",
	"/S { sCol stroke } def",
	"/Sf { fCol stroke } def",
	"/f { fCol fill } def",
	"/f* { fCol eofill } def",
	"% clipping operators",
	"/W { clip newpath } bdef",
	"/W* { eoclip newpath } bdef",
	"/Ws { strokepath clip newpath } bdef",
	"% text state operators",
	"/Tc { /pdfCharSpacing exch def } def",
	"/Tf { dup /pdfFontSize exch def",
	"      dup pdfHorizScaling mul exch matrix scale",
	"      pdfTextMat matrix concatmatrix dup 4 0 put dup 5 0 put",
	"      exch findfont exch makefont setfont } def",
	"/Tr { /pdfTextRender exch def } def",
	"/Ts { /pdfTextRise exch def } def",
	"/Tw { /pdfWordSpacing exch def } def",
	"/Tz { /pdfHorizScaling exch def } def",
	"% text positioning operators",
	"/Td { pdfTextMat transform moveto } def",
	"/Tm { /pdfTextMat exch def } def",
	"% text string operators",
	"/xyshow where {",
	"  pop",
	"  /xyshow2 {",
	"    dup length array",
	"    0 2 2 index length 1 sub {",
	"      2 index 1 index 2 copy get 3 1 roll 1 add get",
	"      pdfTextMat dtransform",
	"      4 2 roll 2 copy 6 5 roll put 1 add 3 1 roll dup 4 2 roll put",
	"    } for",
	"    exch pop",
	"    xyshow",
	"  } def",
	"}{",
	"  /xyshow2 {",
	"    currentfont /FontType get 0 eq {",
	"      0 2 3 index length 1 sub {",
	"        currentpoint 4 index 3 index 2 getinterval show moveto",
	"        2 copy get 2 index 3 2 roll 1 add get",
	"        pdfTextMat dtransform rmoveto",
	"      } for",
	"    } {",
	"      0 1 3 index length 1 sub {",
	"        currentpoint 4 index 3 index 1 getinterval show moveto",
	"        2 copy 2 mul get 2 index 3 2 roll 2 mul 1 add get",
	"        pdfTextMat dtransform rmoveto",
	"      } for",
	"    } ifelse",
	"    pop pop",
	"  } def",
	"} ifelse",
	"/cshow where {",
	"  pop",
	"  /xycp {", // xycharpath
	"    0 3 2 roll",
	"    {",
	"      pop pop currentpoint 3 2 roll",
	"      1 string dup 0 4 3 roll put false charpath moveto",
	"      2 copy get 2 index 2 index 1 add get",
	"      pdfTextMat dtransform rmoveto",
	"      2 add",
	"    } exch cshow",
	"    pop pop",
	"  } def",
	"}{",
	"  /xycp {", // xycharpath
	"    currentfont /FontType get 0 eq {",
	"      0 2 3 index length 1 sub {",
	"        currentpoint 4 index 3 index 2 getinterval false charpath moveto",
	"        2 copy get 2 index 3 2 roll 1 add get",
	"        pdfTextMat dtransform rmoveto",
	"      } for",
	"    } {",
	"      0 1 3 index length 1 sub {",
	"        currentpoint 4 index 3 index 1 getinterval false charpath moveto",
	"        2 copy 2 mul get 2 index 3 2 roll 2 mul 1 add get",
	"        pdfTextMat dtransform rmoveto",
	"      } for",
	"    } ifelse",
	"    pop pop",
	"  } def",
	"} ifelse",
	"/Tj {",
	"  fCol", // because stringwidth has to draw Type 3 chars
	"  0 pdfTextRise pdfTextMat dtransform rmoveto",
	"  xyshow2",
	"  0 pdfTextRise neg pdfTextMat dtransform rmoveto",
	"} def",
	"/TjS {",
	"  fCol", // because stringwidth has to draw Type 3 chars
	"  0 pdfTextRise pdfTextMat dtransform rmoveto",
	"  currentfont /FontType get 3 eq { fCol } { sCol } ifelse",
	"  xycp currentpoint stroke moveto",
	"  0 pdfTextRise neg pdfTextMat dtransform rmoveto",
	"} def",
	"/TjFS {",
	"  2 copy currentpoint 4 2 roll Tj moveto TjS",
	"} def",
	"/TjSave {",
	"  fCol", // because stringwidth has to draw Type 3 chars
	"  0 pdfTextRise pdfTextMat dtransform rmoveto",
	"  xycp",
	"  /pdfTextPath [ pdfTextPath aload pop",
	"    {/moveto cvx}",
	"    {/lineto cvx}",
	"    {/curveto cvx}",
	"    {/closepath cvx}",
	"  pathforall ] def",
	"  currentpoint newpath moveto",
	"  0 pdfTextRise neg pdfTextMat dtransform rmoveto",
	"} def",
	"/Tj3 {",
	"  pdfTextRender 3 and 3 ne {",
	"    fCol", // because stringwidth has to draw Type 3 chars
	"    0 pdfTextRise pdfTextMat dtransform rmoveto",
	"    xyshow2",
	"    0 pdfTextRise neg pdfTextMat dtransform rmoveto",
	"  } {",
	"    pop pop",
	"  } ifelse",
	"} def",
	"/TJm { 0.001 mul pdfFontSize mul pdfHorizScaling mul neg 0",
	"       pdfTextMat dtransform rmoveto } def",
	"/TJmV { 0.001 mul pdfFontSize mul neg 0 exch",
	"        pdfTextMat dtransform rmoveto } def",
	"/Tfill { fCol pdfTextPath cvx exec fill } def",
	"/Tstroke { sCol pdfTextPath cvx exec stroke } def",
	"/Tclip { pdfTextPath cvx exec clip newpath } def",
	"/Tstrokeclip { pdfTextPath cvx exec strokepath clip newpath } def",
	"/Tclear { /pdfTextPath [] def } def",
	"/Tsave {",
	"  /pdfTextClipPath [ pdfTextClipPath aload pop pdfTextPath aload pop ] def",
	"} def",
	"/Tclip2 {",
	"  pdfTextClipPath cvx exec clip newpath",
	"  /pdfTextClipPath [] def",
	"} def",
	"~1ns",
	"% Level 1 image operators",
	"~1n",
	"/pdfIm1 {",
	"  /pdfImBuf1 4 index string def",
	"  { currentfile pdfImBuf1 readhexstring pop } image",
	"} def",
	"~1s",
	"/pdfIm1Sep {",
	"  /pdfImBuf1 4 index string def",
	"  /pdfImBuf2 4 index string def",
	"  /pdfImBuf3 4 index string def",
	"  /pdfImBuf4 4 index string def",
	"  { currentfile pdfImBuf1 readhexstring pop }",
	"  { currentfile pdfImBuf2 readhexstring pop }",
	"  { currentfile pdfImBuf3 readhexstring pop }",
	"  { currentfile pdfImBuf4 readhexstring pop }",
	"  true 4 colorimage",
	"} def",
	"~1ns",
	"/pdfImM1 {",
	"  fCol /pdfImBuf1 4 index 7 add 8 idiv string def",
	"  { currentfile pdfImBuf1 readhexstring pop } imagemask",
	"} def",
	"/pdfImStr {",
	"  2 copy exch length lt {",
	"    2 copy get exch 1 add exch",
	"  } {",
	"    ()",
	"  } ifelse",
	"} def",
	"/pdfImM1a {",
	"  { pdfImStr } imagemask",
	"  pop pop",
	"} def",
	"~23ngs",
	"% Level 2/3 image operators",
	"/pdfImBuf 100 string def",
	"/pdfImStr {",
	"  2 copy exch length lt {",
	"    2 copy get exch 1 add exch",
	"  } {",
	"    ()",
	"  } ifelse",
	"} def",
	"/skipEOD {",
	"  { currentfile pdfImBuf readline",
	"    not { pop exit } if",
	"    (%-EOD-) eq { exit } if } loop",
	"} def",
	"/pdfIm { image skipEOD } def",
	"~3ngs",
	"/pdfMask {",
	"  /ReusableStreamDecode filter",
	"  skipEOD",
	"  /maskStream exch def",
	"} def",
	"/pdfMaskEnd { maskStream closefile } def",
	"/pdfMaskInit {",
	"  /maskArray exch def",
	"  /maskIdx 0 def",
	"} def",
	"/pdfMaskSrc {",
	"  maskIdx maskArray length lt {",
	"    maskArray maskIdx get",
	"    /maskIdx maskIdx 1 add def",
	"  } {",
	"    ()",
	"  } ifelse",
	"} def",
	"~23s",
	"/pdfImSep {",
	"  findcmykcustomcolor exch",
	"  dup /Width get /pdfImBuf1 exch string def",
	"  dup /Decode get aload pop 1 index sub /pdfImDecodeRange exch def",
	"  /pdfImDecodeLow exch def",
	"  begin Width Height BitsPerComponent ImageMatrix DataSource end",
	"  /pdfImData exch def",
	"  { pdfImData pdfImBuf1 readstring pop",
	"    0 1 2 index length 1 sub {",
	"      1 index exch 2 copy get",
	"      pdfImDecodeRange mul 255 div pdfImDecodeLow add round cvi",
	"      255 exch sub put",
	"    } for }",
	"  6 5 roll customcolorimage",
	"  skipEOD",
	"} def",
	"~23ngs",
	"/pdfImM { fCol imagemask skipEOD } def",
	"/pr {",
	"  4 2 roll exch 5 index div exch 4 index div moveto",
	"  exch 3 index div dup 0 rlineto",
	"  exch 2 index div 0 exch rlineto",
	"  neg 0 rlineto",
	"  closepath",
	"} def",
	"/pdfImClip { gsave clip } def",
	"/pdfImClipEnd { grestore } def",
	"~23ns",
	"% shading operators",
	"/colordelta {",
	"  false 0 1 3 index length 1 sub {",
	"    dup 4 index exch get 3 index 3 2 roll get sub abs 0.004 gt {",
	"      pop true",
	"    } if",
	"  } for",
	"  exch pop exch pop",
	"} def",
	"/funcCol { func n array astore } def",
	"/funcSH {",
	"  dup 0 eq {",
	"    true",
	"  } {",
	"    dup 6 eq {",
	"      false",
	"    } {",
	"      4 index 4 index funcCol dup",
	"      6 index 4 index funcCol dup",
	"      3 1 roll colordelta 3 1 roll",
	"      5 index 5 index funcCol dup",
	"      3 1 roll colordelta 3 1 roll",
	"      6 index 8 index funcCol dup",
	"      3 1 roll colordelta 3 1 roll",
	"      colordelta or or or",
	"    } ifelse",
	"  } ifelse",
	"  {",
	"    1 add",
	"    4 index 3 index add 0.5 mul exch 4 index 3 index add 0.5 mul exch",
	"    6 index 6 index 4 index 4 index 4 index funcSH",
	"    2 index 6 index 6 index 4 index 4 index funcSH",
	"    6 index 2 index 4 index 6 index 4 index funcSH",
	"    5 3 roll 3 2 roll funcSH pop pop",
	"  } {",
	"    pop 3 index 2 index add 0.5 mul 3 index  2 index add 0.5 mul",
	"~23n",
	"    funcCol sc",
	"~23s",
	"    funcCol aload pop k",
	"~23ns",
	"    dup 4 index exch mat transform m",
	"    3 index 3 index mat transform l",
	"    1 index 3 index mat transform l",
	"    mat transform l pop pop h f*",
	"  } ifelse",
	"} def",
	"/axialCol {",
	"  dup 0 lt {",
	"    pop t0",
	"  } {",
	"    dup 1 gt {",
	"      pop t1",
	"    } {",
	"      dt mul t0 add",
	"    } ifelse",
	"  } ifelse",
	"  func n array astore",
	"} def",
	"/axialSH {",
	"  dup 2 lt {",
	"    true",
	"  } {",
	"    dup 8 eq {",
	"      false",
	"    } {",
	"      2 index axialCol 2 index axialCol colordelta",
	"    } ifelse",
	"  } ifelse",
	"  {",
	"    1 add 3 1 roll 2 copy add 0.5 mul",
	"    dup 4 3 roll exch 4 index axialSH",
	"    exch 3 2 roll axialSH",
	"  } {",
	"    pop 2 copy add 0.5 mul",
	"~23n",
	"    axialCol sc",
	"~23s",
	"    axialCol aload pop k",
	"~23ns",
	"    exch dup dx mul x0 add exch dy mul y0 add",
	"    3 2 roll dup dx mul x0 add exch dy mul y0 add",
	"    dx abs dy abs ge {",
	"      2 copy yMin sub dy mul dx div add yMin m",
	"      yMax sub dy mul dx div add yMax l",
	"      2 copy yMax sub dy mul dx div add yMax l",
	"      yMin sub dy mul dx div add yMin l",
	"      h f*",
	"    } {",
	"      exch 2 copy xMin sub dx mul dy div add xMin exch m",
	"      xMax sub dx mul dy div add xMax exch l",
	"      exch 2 copy xMax sub dx mul dy div add xMax exch l",
	"      xMin sub dx mul dy div add xMin exch l",
	"      h f*",
	"    } ifelse",
	"  } ifelse",
	"} def",
	"/radialCol {",
	"  dup t0 lt {",
	"    pop t0",
	"  } {",
	"    dup t1 gt {",
	"      pop t1",
	"    } if",
	"  } ifelse",
	"  func n array astore",
	"} def",
	"/radialSH {",
	"  dup 0 eq {",
	"    true",
	"  } {",
	"    dup 8 eq {",
	"      false",
	"    } {",
	"      2 index dt mul t0 add radialCol",
	"      2 index dt mul t0 add radialCol colordelta",
	"    } ifelse",
	"  } ifelse",
	"  {",
	"    1 add 3 1 roll 2 copy add 0.5 mul",
	"    dup 4 3 roll exch 4 index radialSH",
	"    exch 3 2 roll radialSH",
	"  } {",
	"    pop 2 copy add 0.5 mul dt mul t0 add",
	"~23n",
	"    radialCol sc",
	"~23s",
	"    radialCol aload pop k",
	"~23ns",
	"    encl {",
	"      exch dup dx mul x0 add exch dup dy mul y0 add exch dr mul r0 add",
	"      0 360 arc h",
	"      dup dx mul x0 add exch dup dy mul y0 add exch dr mul r0 add",
	"      360 0 arcn h f",
	"    } {",
	"      2 copy",
	"      dup dx mul x0 add exch dup dy mul y0 add exch dr mul r0 add",
	"      a1 a2 arcn",
	"      dup dx mul x0 add exch dup dy mul y0 add exch dr mul r0 add",
	"      a2 a1 arcn h",
	"      dup dx mul x0 add exch dup dy mul y0 add exch dr mul r0 add",
	"      a1 a2 arc",
	"      dup dx mul x0 add exch dup dy mul y0 add exch dr mul r0 add",
	"      a2 a1 arc h f",
	"    } ifelse",
	"  } ifelse",
	"} def",
	"~123ngs",
	"end",
	nullptr
};

static const char* minLineWidthProlog[] = {
	"/pdfDist { dup dtransform dup mul exch dup mul add 0.5 mul sqrt } def",
	"/pdfIDist { dup idtransform dup mul exch dup mul add 0.5 mul sqrt } def",
	"/pdfMinLineDist pdfMinLineWidth pdfDist def",
	"/setlinewidth {",
	"  dup pdfDist pdfMinLineDist lt {",
	"    pop pdfMinLineDist pdfIDist",
	"  } if",
	"  setlinewidth",
	"} bind def",
	nullptr
};

static const char* cmapProlog[] = {
	"/CIDInit /ProcSet findresource begin",
	"10 dict begin",
	"  begincmap",
	"  /CMapType 1 def",
	"  /CMapName /Identity-H def",
	"  /CIDSystemInfo 3 dict dup begin",
	"    /Registry (Adobe) def",
	"    /Ordering (Identity) def",
	"    /Supplement 0 def",
	"  end def",
	"  1 begincodespacerange",
	"    <0000> <ffff>",
	"  endcodespacerange",
	"  0 usefont",
	"  1 begincidrange",
	"    <0000> <ffff> 0",
	"  endcidrange",
	"  endcmap",
	"  currentdict CMapName exch /CMap defineresource pop",
	"end",
	"10 dict begin",
	"  begincmap",
	"  /CMapType 1 def",
	"  /CMapName /Identity-V def",
	"  /CIDSystemInfo 3 dict dup begin",
	"    /Registry (Adobe) def",
	"    /Ordering (Identity) def",
	"    /Supplement 0 def",
	"  end def",
	"  /WMode 1 def",
	"  1 begincodespacerange",
	"    <0000> <ffff>",
	"  endcodespacerange",
	"  0 usefont",
	"  1 begincidrange",
	"    <0000> <ffff> 0",
	"  endcidrange",
	"  endcmap",
	"  currentdict CMapName exch /CMap defineresource pop",
	"end",
	"end",
	nullptr
};

//------------------------------------------------------------------------
// Fonts
//------------------------------------------------------------------------

struct PSSubstFont
{
	std::string psName = ""; // PostScript name
	double      mWidth = 0;  // width of 'm' character
};

// NB: must be in same order as base14SubstFonts in GfxFont.cc
static PSSubstFont psBase14SubstFonts[14] = {
	{"Courier",                0.600},
	{ "Courier-Oblique",       0.600},
	{ "Courier-Bold",          0.600},
	{ "Courier-BoldOblique",   0.600},
	{ "Helvetica",             0.833},
	{ "Helvetica-Oblique",     0.833},
	{ "Helvetica-Bold",        0.889},
	{ "Helvetica-BoldOblique", 0.889},
	{ "Times-Roman",           0.788},
	{ "Times-Italic",          0.722},
	{ "Times-Bold",            0.833},
	{ "Times-BoldItalic",      0.778},
 // the last two are never used for substitution
	{ "Symbol",	            0    },
	{ "ZapfDingbats",          0    }
};

class PSFontInfo
{
public:
	PSFontInfo(Ref fontIDA)
	{
		fontID = fontIDA;
	}

	Ref             fontID; //
	sPSFontFileInfo ff;     // pointer to font file info; nullptr indicates font mapping failed
};

PSFontFileInfo::PSFontFileInfo(const std::string& psNameA, GfxFontType typeA, PSFontFileLocation locA)
{
	psName        = psNameA;
	type          = typeA;
	loc           = locA;
	embFontID.num = -1;
	embFontID.gen = -1;
}

PSFontFileInfo::~PSFontFileInfo()
{
	if (codeToGID) gfree(codeToGID);
}

//------------------------------------------------------------------------
// process colors
//------------------------------------------------------------------------

#define psProcessCyan    1
#define psProcessMagenta 2
#define psProcessYellow  4
#define psProcessBlack   8
#define psProcessCMYK    15

//------------------------------------------------------------------------
// PSOutCustomColor
//------------------------------------------------------------------------

class PSOutCustomColor
{
public:
	PSOutCustomColor(double cA, double mA, double yA, double kA, std::string_view nameA);
	~PSOutCustomColor();

	double            c    = 0;       //
	double            m    = 0;       //
	double            y    = 0;       //
	double            k    = 0;       //
	std::string       name = "";      //
	PSOutCustomColor* next = nullptr; //
};

PSOutCustomColor::PSOutCustomColor(double cA, double mA, double yA, double kA, std::string_view nameA)
{
	c    = cA;
	m    = mA;
	y    = yA;
	k    = kA;
	name = nameA;
	next = nullptr;
}

PSOutCustomColor::~PSOutCustomColor()
{
}

//------------------------------------------------------------------------

struct PSOutImgClipRect
{
	int x0, x1, y0, y1;
};

//------------------------------------------------------------------------

struct PSOutPaperSize
{
	PSOutPaperSize(int wA, int hA)
	{
		w = wA;
		h = hA;
	}

	int w, h;
};

//------------------------------------------------------------------------
// DeviceNRecoder
//------------------------------------------------------------------------

class DeviceNRecoder : public FilterStream
{
public:
	DeviceNRecoder(Stream* strA, int widthA, int heightA, GfxImageColorMap* colorMapA);
	virtual ~DeviceNRecoder();
	virtual Stream* copy();

	virtual StreamKind getKind() { return strWeird; }

	virtual void reset();
	virtual void close();

	virtual int getChar()
	{
		return (bufIdx >= bufSize && !fillBuf()) ? EOF : buf[bufIdx++];
	}

	virtual int lookChar()
	{
		return (bufIdx >= bufSize && !fillBuf()) ? EOF : buf[bufIdx];
	}

	virtual std::string getPSFilter(int psLevel, const char* indent, bool okToReadStream)
	{
		return "";
	}

	virtual bool isBinary(bool last = true) { return true; }

	virtual bool isEncoder() { return true; }

private:
	bool fillBuf();

	int               width, height;
	GfxImageColorMap* colorMap;
	Function*         func;
	ImageStream*      imgStr;
	int               buf[gfxColorMaxComps];
	int               pixelIdx;
	int               bufIdx;
	int               bufSize;
};

DeviceNRecoder::DeviceNRecoder(Stream* strA, int widthA, int heightA, GfxImageColorMap* colorMapA)
    : FilterStream(strA)
{
	width    = widthA;
	height   = heightA;
	colorMap = colorMapA;
	imgStr   = nullptr;
	pixelIdx = 0;
	bufIdx   = gfxColorMaxComps;
	bufSize  = ((GfxDeviceNColorSpace*)colorMap->getColorSpace())->getAlt()->getNComps();
	func     = ((GfxDeviceNColorSpace*)colorMap->getColorSpace())->getTintTransformFunc();
}

DeviceNRecoder::~DeviceNRecoder()
{
	if (str->isEncoder())
		delete str;
}

Stream* DeviceNRecoder::copy()
{
	error(errInternal, -1, "Called copy() on DeviceNRecoder");
	return nullptr;
}

void DeviceNRecoder::reset()
{
	imgStr = new ImageStream(str, width, colorMap->getNumPixelComps(), colorMap->getBits());
	imgStr->reset();
}

void DeviceNRecoder::close()
{
	delete imgStr;
	imgStr = nullptr;
	str->close();
}

bool DeviceNRecoder::fillBuf()
{
	uint8_t  pixBuf[gfxColorMaxComps];
	GfxColor color;
	double   x[gfxColorMaxComps], y[gfxColorMaxComps];
	int      i;

	if (pixelIdx >= width * height)
		return false;
	imgStr->getPixel(pixBuf);
	colorMap->getColor(pixBuf, &color);
	for (i = 0; i < ((GfxDeviceNColorSpace*)colorMap->getColorSpace())->getNComps(); ++i)
		x[i] = colToDbl(color.c[i]);
	func->transform(x, y);
	for (i = 0; i < bufSize; ++i)
		buf[i] = (int)(y[i] * 255 + 0.5);
	bufIdx = 0;
	++pixelIdx;
	return true;
}

//------------------------------------------------------------------------
// GrayRecoder
//------------------------------------------------------------------------

class GrayRecoder : public FilterStream
{
public:
	GrayRecoder(Stream* strA, int widthA, int heightA, GfxImageColorMap* colorMapA);
	virtual ~GrayRecoder();
	virtual Stream* copy();

	virtual StreamKind getKind() { return strWeird; }

	virtual void reset();
	virtual void close();

	virtual int getChar()
	{
		return (bufIdx >= width && !fillBuf()) ? EOF : buf[bufIdx++];
	}

	virtual int lookChar()
	{
		return (bufIdx >= width && !fillBuf()) ? EOF : buf[bufIdx];
	}

	virtual std::string getPSFilter(int psLevel, const char* indent, bool okToReadStream)
	{
		return "";
	}

	virtual bool isBinary(bool last = true) { return true; }

	virtual bool isEncoder() { return true; }

private:
	bool fillBuf();

	int               width, height;
	GfxImageColorMap* colorMap;
	ImageStream*      imgStr;
	uint8_t*          buf;
	int               bufIdx;
};

GrayRecoder::GrayRecoder(Stream* strA, int widthA, int heightA, GfxImageColorMap* colorMapA)
    : FilterStream(strA)
{
	width    = widthA;
	height   = heightA;
	colorMap = colorMapA;
	imgStr   = nullptr;
	buf      = (uint8_t*)gmalloc(width);
	bufIdx   = width;
}

GrayRecoder::~GrayRecoder()
{
	gfree(buf);
	if (str->isEncoder())
		delete str;
}

Stream* GrayRecoder::copy()
{
	error(errInternal, -1, "Called copy() on GrayRecoder");
	return nullptr;
}

void GrayRecoder::reset()
{
	imgStr = new ImageStream(str, width, colorMap->getNumPixelComps(), colorMap->getBits());
	imgStr->reset();
}

void GrayRecoder::close()
{
	delete imgStr;
	imgStr = nullptr;
	str->close();
}

bool GrayRecoder::fillBuf()
{
	uint8_t* line;

	if (!(line = imgStr->getLine()))
	{
		bufIdx = width;
		return false;
	}
	//~ this should probably use the rendering intent from the image
	//~   dict, or from the content stream
	colorMap->getGrayByteLine(line, buf, width, gfxRenderingIntentRelativeColorimetric);
	bufIdx = 0;
	return true;
}

//------------------------------------------------------------------------
// ColorKeyToMaskEncoder
//------------------------------------------------------------------------

class ColorKeyToMaskEncoder : public FilterStream
{
public:
	ColorKeyToMaskEncoder(Stream* strA, int widthA, int heightA, GfxImageColorMap* colorMapA, int* maskColorsA);
	virtual ~ColorKeyToMaskEncoder();
	virtual Stream* copy();

	virtual StreamKind getKind() { return strWeird; }

	virtual void reset();
	virtual void close();

	virtual int getChar()
	{
		return (bufIdx >= bufSize && !fillBuf()) ? EOF : buf[bufIdx++];
	}

	virtual int lookChar()
	{
		return (bufIdx >= bufSize && !fillBuf()) ? EOF : buf[bufIdx];
	}

	virtual std::string getPSFilter(int psLevel, const char* indent, bool okToReadStream)
	{
		return "";
	}

	virtual bool isBinary(bool last = true) { return true; }

	virtual bool isEncoder() { return true; }

private:
	bool fillBuf();

	int               width, height;
	GfxImageColorMap* colorMap;
	int               numComps;
	int*              maskColors;
	ImageStream*      imgStr;
	uint8_t*          buf;
	int               bufIdx;
	int               bufSize;
};

ColorKeyToMaskEncoder::ColorKeyToMaskEncoder(Stream* strA, int widthA, int heightA, GfxImageColorMap* colorMapA, int* maskColorsA)
    : FilterStream(strA)
{
	width      = widthA;
	height     = heightA;
	colorMap   = colorMapA;
	numComps   = colorMap->getNumPixelComps();
	maskColors = maskColorsA;
	imgStr     = nullptr;
	bufSize    = (width + 7) / 8;
	buf        = (uint8_t*)gmalloc(bufSize);
	bufIdx     = width;
}

ColorKeyToMaskEncoder::~ColorKeyToMaskEncoder()
{
	gfree(buf);
	if (str->isEncoder())
		delete str;
}

Stream* ColorKeyToMaskEncoder::copy()
{
	error(errInternal, -1, "Called copy() on ColorKeyToMaskEncoder");
	return nullptr;
}

void ColorKeyToMaskEncoder::reset()
{
	imgStr = new ImageStream(str, width, colorMap->getNumPixelComps(), colorMap->getBits());
	imgStr->reset();
}

void ColorKeyToMaskEncoder::close()
{
	delete imgStr;
	imgStr = nullptr;
	str->close();
}

bool ColorKeyToMaskEncoder::fillBuf()
{
	uint8_t *line, *linePtr, *bufPtr;
	uint8_t  byte;
	int      x, xx, i;

	if (!(line = imgStr->getLine()))
	{
		bufIdx = width;
		return false;
	}
	linePtr = line;
	bufPtr  = buf;
	for (x = 0; x < width; x += 8)
	{
		byte = 0;
		for (xx = 0; xx < 8; ++xx)
		{
			byte = (uint8_t)(byte << 1);
			if (x + xx < width)
			{
				for (i = 0; i < numComps; ++i)
					if (linePtr[i] < maskColors[2 * i] || linePtr[i] > maskColors[2 * i + 1])
						break;
				if (i >= numComps)
					byte |= 1;
				linePtr += numComps;
			}
			else
			{
				byte |= 1;
			}
		}
		*bufPtr++ = byte;
	}
	bufIdx = 0;
	return true;
}

//------------------------------------------------------------------------
// PSOutputDev
//------------------------------------------------------------------------

extern "C"
{
typedef void (*SignalFunc)(int);
}

static void outputToFile(void* stream, const char* data, size_t len)
{
	fwrite(data, 1, len, (FILE*)stream);
}

PSOutputDev::PSOutputDev(const char* fileName, PDFDoc* docA, int firstPageA, int lastPageA, PSOutMode modeA, int imgLLXA, int imgLLYA, int imgURXA, int imgURYA, bool manualCtrlA, PSOutCustomCodeCbk customCodeCbkA, void* customCodeCbkDataA, bool honorUserUnitA, bool fileNameIsUTF8)
{
	FILE*      f;
	PSFileType fileTypeA;

	customCodeCbk     = customCodeCbkA;
	customCodeCbkData = customCodeCbkDataA;
	fontInfo          = new GList();

	// open file or pipe
	if (!strcmp(fileName, "-"))
	{
		fileTypeA = psStdout;
		f         = stdout;
	}
	else if (fileName[0] == '|')
	{
		fileTypeA = psPipe;
#ifdef HAVE_POPEN
#	ifndef _WIN32
		signal(SIGPIPE, (SignalFunc)SIG_IGN);
#	endif
		if (!(f = popen(fileName + 1, "w")))
		{
			error(errIO, -1, "Couldn't run print command '{}'", fileName);
			ok = false;
			return;
		}
#else
		error(errIO, -1, "Print commands are not supported ('{}')", fileName);
		ok = false;
		return;
#endif
	}
	else
	{
		fileTypeA = psFile;
		if (fileNameIsUTF8)
			f = openFile(fileName, "w");
		else
			f = fopen(fileName, "w");
		if (!f)
		{
			error(errIO, -1, "Couldn't open PostScript file '{}'", fileName);
			ok = false;
			return;
		}
	}

	init(outputToFile, f, fileTypeA, docA, firstPageA, lastPageA, modeA, imgLLXA, imgLLYA, imgURXA, imgURYA, manualCtrlA, honorUserUnitA);
}

PSOutputDev::PSOutputDev(PSOutputFunc outputFuncA, void* outputStreamA, PDFDoc* docA, int firstPageA, int lastPageA, PSOutMode modeA, int imgLLXA, int imgLLYA, int imgURXA, int imgURYA, bool manualCtrlA, PSOutCustomCodeCbk customCodeCbkA, void* customCodeCbkDataA, bool honorUserUnitA)
{
	customCodeCbk     = customCodeCbkA;
	customCodeCbkData = customCodeCbkDataA;
	fontInfo          = new GList();

	init(outputFuncA, outputStreamA, psGeneric, docA, firstPageA, lastPageA, modeA, imgLLXA, imgLLYA, imgURXA, imgURYA, manualCtrlA, honorUserUnitA);
}

void PSOutputDev::init(PSOutputFunc outputFuncA, void* outputStreamA, PSFileType fileTypeA, PDFDoc* docA, int firstPageA, int lastPageA, PSOutMode modeA, int imgLLXA, int imgLLYA, int imgURXA, int imgURYA, bool manualCtrlA, bool honorUserUnitA)
{
	Catalog*        catalog;
	Page*           page;
	PDFRectangle*   box;
	PSOutPaperSize* size;
	double          userUnit;
	int             pg, w, h, i;

	// initialize
	ok           = true;
	outputFunc   = outputFuncA;
	outputStream = outputStreamA;
	fileType     = fileTypeA;
	doc          = docA;
	xref         = doc->getXRef();
	catalog      = doc->getCatalog();
	if ((firstPage = firstPageA) < 1)
		firstPage = 1;
	if ((lastPage = lastPageA) > doc->getNumPages())
		lastPage = doc->getNumPages();
	level         = globalParams->getPSLevel();
	mode          = modeA;
	honorUserUnit = honorUserUnitA;
	paperWidth    = globalParams->getPSPaperWidth();
	paperHeight   = globalParams->getPSPaperHeight();
	imgLLX        = imgLLXA;
	imgLLY        = imgLLYA;
	imgURX        = imgURXA;
	imgURY        = imgURYA;
	if (imgLLX == 0 && imgURX == 0 && imgLLY == 0 && imgURY == 0)
		globalParams->getPSImageableArea(&imgLLX, &imgLLY, &imgURX, &imgURY);
	if (paperWidth < 0 || paperHeight < 0)
	{
		paperMatch = true;
		paperSizes = new GList();
		paperWidth = paperHeight = 1; // in case the document has zero pages
		for (pg = firstPage; pg <= lastPage; ++pg)
		{
			page = catalog->getPage(pg);
			if (honorUserUnit)
				userUnit = page->getUserUnit();
			else
				userUnit = 1;
			if (globalParams->getPSUseCropBoxAsPage())
			{
				w = (int)ceil(page->getCropWidth() * userUnit);
				h = (int)ceil(page->getCropHeight() * userUnit);
			}
			else
			{
				w = (int)ceil(page->getMediaWidth() * userUnit);
				h = (int)ceil(page->getMediaHeight() * userUnit);
			}
			for (i = 0; i < paperSizes->getLength(); ++i)
			{
				size = (PSOutPaperSize*)paperSizes->get(i);
				if (size->w == w && size->h == h)
					break;
			}
			if (i == paperSizes->getLength())
				paperSizes->append(new PSOutPaperSize(w, h));
			if (w > paperWidth) paperWidth = w;
			if (h > paperHeight) paperHeight = h;
		}
		// NB: img{LLX,LLY,URX,URY} will be set by startPage()
	}
	else
	{
		paperMatch = false;
	}
	preload    = globalParams->getPSPreload();
	manualCtrl = manualCtrlA;
	if (mode == psModeForm) lastPage = firstPage;
	processColors = 0;
	inType3Char   = false;

#if OPI_SUPPORT
	// initialize OPI nesting levels
	opi13Nest = 0;
	opi20Nest = 0;
#endif

	tx0              = -1;
	ty0              = -1;
	xScale0          = 0;
	yScale0          = 0;
	rotate0          = -1;
	clipLLX0         = 0;
	clipLLY0         = 0;
	clipURX0         = -1;
	clipURY0         = -1;
	expandSmallPages = globalParams->getPSExpandSmaller();

	// initialize font lists, etc.
	for (i = 0; i < 14; ++i)
	{
		auto ff = std::make_shared<PSFontFileInfo>(psBase14SubstFonts[i].psName, fontType1, psFontFileResident);
		fontFileInfo.emplace(ff->psName, ff);
	}
	for (const auto& name : globalParams->getPSResidentFonts())
	{
		if (!fontFileInfo.contains(name))
		{
			auto ff = std::make_shared<PSFontFileInfo>(name, fontType1, psFontFileResident);
			fontFileInfo.try_emplace(ff->psName, ff);
		}
	}

	imgIDLen   = 0;
	imgIDSize  = 0;
	formIDLen  = 0;
	formIDSize = 0;

	noStateChanges    = false;
	saveStack         = new GList();
	numTilingPatterns = 0;
	nextFunc          = 0;

	// initialize embedded font resource comment list
	embFontList.clear();

	if (!manualCtrl)
	{
		// this check is needed in case the document has zero pages
		if (firstPage <= catalog->getNumPages())
		{
			writeHeader(catalog->getPage(firstPage)->getMediaBox(), catalog->getPage(firstPage)->getCropBox(), catalog->getPage(firstPage)->getRotate());
		}
		else
		{
			box = new PDFRectangle(0, 0, 1, 1);
			writeHeader(box, box, 0);
			delete box;
		}
		if (mode != psModeForm)
			writePS("%%BeginProlog\n");
		writeXpdfProcset();
		if (mode != psModeForm)
		{
			writePS("%%EndProlog\n");
			writePS("%%BeginSetup\n");
		}
		writeDocSetup(catalog);
		if (mode != psModeForm)
			writePS("%%EndSetup\n");
	}

	// initialize sequential page number
	seqPage = 1;
}

PSOutputDev::~PSOutputDev()
{
	PSOutCustomColor* cc;

	if (ok)
	{
		if (!manualCtrl)
		{
			writePS("%%Trailer\n");
			writeTrailer();
			if (mode != psModeForm)
				writePS("%%EOF\n");
		}
		if (fileType == psFile)
			fclose((FILE*)outputStream);
#ifdef HAVE_POPEN
		else if (fileType == psPipe)
		{
			pclose((FILE*)outputStream);
#	ifndef _WIN32
			signal(SIGPIPE, (SignalFunc)SIG_DFL);
#	endif
		}
#endif
	}
	gfree(rasterizePage);
	if (paperSizes) deleteGList(paperSizes, PSOutPaperSize);
	deleteGList(fontInfo, PSFontInfo);
	gfree(imgIDs);
	gfree(formIDs);
	if (saveStack) delete saveStack;

	while (customColors)
	{
		cc           = customColors;
		customColors = cc->next;
		delete cc;
	}
}

bool PSOutputDev::checkIO()
{
	if (fileType == psFile || fileType == psPipe || fileType == psStdout)
	{
		if (ferror((FILE*)outputStream))
		{
			error(errIO, -1, "Error writing to PostScript file");
			return false;
		}
	}
	return true;
}

void PSOutputDev::writeHeader(PDFRectangle* mediaBox, PDFRectangle* cropBox, int pageRotate)
{
	Object          info, obj1;
	PSOutPaperSize* size;
	double          x1, y1, x2, y2;
	int             i;

	switch (mode)
	{
	case psModePS:
		writePS("%!PS-Adobe-3.0\n");
		break;
	case psModeEPS:
		writePS("%!PS-Adobe-3.0 EPSF-3.0\n");
		break;
	case psModeForm:
		writePS("%!PS-Adobe-3.0 Resource-Form\n");
		break;
	}

	writePSFmt("%XpdfVersion: {}\n", xpdfVersion);
	xref->getDocInfo(&info);
	if (info.isDict() && info.dictLookup("Creator", &obj1)->isString())
	{
		writePS("%%Creator: ");
		writePSTextLine(obj1.getString());
	}
	obj1.free();
	if (info.isDict() && info.dictLookup("Title", &obj1)->isString())
	{
		writePS("%%Title: ");
		writePSTextLine(obj1.getString());
	}
	obj1.free();
	info.free();
	writePSFmt("%%LanguageLevel: {}\n", (level >= psLevel3) ? 3 : ((level >= psLevel2) ? 2 : 1));
	if (level == psLevel1Sep || level == psLevel2Sep || level == psLevel3Sep)
	{
		writePS("%%DocumentProcessColors: (atend)\n");
		writePS("%%DocumentCustomColors: (atend)\n");
	}
	writePS("%%DocumentSuppliedResources: (atend)\n");

	switch (mode)
	{
	case psModePS:
		if (paperMatch)
		{
			for (i = 0; i < paperSizes->getLength(); ++i)
			{
				size = (PSOutPaperSize*)paperSizes->get(i);
				writePSFmt("%%{} {}x{} {} {} 0 () ()\n", i == 0 ? "DocumentMedia:" : "+", size->w, size->h, size->w, size->h);
			}
		}
		else
		{
			writePSFmt("%%DocumentMedia: plain {} {} 0 () ()\n", paperWidth, paperHeight);
		}
		writePSFmt("%%BoundingBox: 0 0 {} {}\n", paperWidth, paperHeight);
		writePSFmt("%%Pages: {}\n", lastPage - firstPage + 1);
		writePS("%%EndComments\n");
		if (!paperMatch)
		{
			writePS("%%BeginDefaults\n");
			writePS("%%PageMedia: plain\n");
			writePS("%%EndDefaults\n");
		}
		break;
	case psModeEPS:
		epsX1 = cropBox->x1;
		epsY1 = cropBox->y1;
		epsX2 = cropBox->x2;
		epsY2 = cropBox->y2;
		if (pageRotate == 0 || pageRotate == 180)
		{
			x1 = epsX1;
			y1 = epsY1;
			x2 = epsX2;
			y2 = epsY2;
		}
		else
		{ // pageRotate == 90 || pageRotate == 270
			x1 = 0;
			y1 = 0;
			x2 = epsY2 - epsY1;
			y2 = epsX2 - epsX1;
		}
		writePSFmt("%%BoundingBox: {} {} {} {}\n", (int)floor(x1), (int)floor(y1), (int)ceil(x2), (int)ceil(y2));
		if (floor(x1) != ceil(x1) || floor(y1) != ceil(y1) || floor(x2) != ceil(x2) || floor(y2) != ceil(y2))
			writePSFmt("%%HiResBoundingBox: {} {} {} {}\n", Round6(x1), Round6(y1), Round6(x2), Round6(y2));
		writePS("%%EndComments\n");
		break;
	case psModeForm:
		writePS("%%EndComments\n");
		writePS("32 dict dup begin\n");
		writePSFmt("/BBox [{} {} {} {}] def\n", (int)floor(mediaBox->x1), (int)floor(mediaBox->y1), (int)ceil(mediaBox->x2), (int)ceil(mediaBox->y2));
		writePS("/FormType 1 def\n");
		writePS("/Matrix [1 0 0 1 0 0] def\n");
		break;
	}
}

void PSOutputDev::writeXpdfProcset()
{
	bool lev1, lev2, lev3, nonSep, gray, sep;

	writePSFmt("%%BeginResource: procset xpdf {} 0\n", xpdfVersion);
	writePSFmt("%%Copyright: {}\n", xpdfCopyright);
	lev1 = lev2 = lev3 = nonSep = gray = sep = true;
	for (const char** p = prolog; *p; ++p)
	{
		if ((*p)[0] == '~')
		{
			lev1 = lev2 = lev3 = nonSep = gray = sep = false;
			for (const char* q = *p + 1; *q; ++q)
			{
				switch (*q)
				{
				case '1': lev1 = true; break;
				case '2': lev2 = true; break;
				case '3': lev3 = true; break;
				case 'g': gray = true; break;
				case 'n': nonSep = true; break;
				case 's': sep = true; break;
				}
			}
		}
		else if ((level == psLevel1 && lev1 && nonSep) || (level == psLevel1Sep && lev1 && sep) || (level == psLevel2 && lev2 && nonSep) || (level == psLevel2Gray && lev2 && gray) || (level == psLevel2Sep && lev2 && sep) || (level == psLevel3 && lev3 && nonSep) || (level == psLevel3Gray && lev3 && gray) || (level == psLevel3Sep && lev3 && sep))
		{
			writePSFmt("{}\n", *p);
		}
	}
	if (const double w = globalParams->getPSMinLineWidth(); w > 0)
	{
		writePSFmt("/pdfMinLineWidth {} def\n", Round4(w));
		for (const char** p = p = minLineWidthProlog; *p; ++p)
			writePSFmt("{}\n", *p);
	}
	writePS("%%EndResource\n");

	if (level >= psLevel3)
		for (const char** p = cmapProlog; *p; ++p)
			writePSFmt("{}\n", *p);
}

void PSOutputDev::writeDocSetup(Catalog* catalog)
{
	Page*     page;
	Dict*     resDict;
	AcroForm* form;
	Object    obj1, obj2, obj3;
	bool      needDefaultFont;

	// check to see which pages will be rasterized
	if (firstPage <= lastPage)
	{
		rasterizePage = (char*)gmalloc(lastPage - firstPage + 1);
		for (int pg = firstPage; pg <= lastPage; ++pg)
			rasterizePage[pg - firstPage] = (char)checkIfPageNeedsToBeRasterized(pg);
	}
	else
	{
		rasterizePage = nullptr;
	}

	visitedResources = (char*)gmalloc(xref->getNumObjects());
	memset(visitedResources, 0, xref->getNumObjects());

	if (mode == psModeForm)
	{
		// swap the form and xpdf dicts
		writePS("xpdf end begin dup begin\n");
	}
	else
	{
		writePS("xpdf begin\n");
	}
	Annots* annots  = doc->getAnnots();
	needDefaultFont = false;
	for (int pg = firstPage; pg <= lastPage; ++pg)
	{
		if (rasterizePage[pg - firstPage])
			continue;
		page = catalog->getPage(pg);
		if ((resDict = page->getResourceDict()))
			setupResources(resDict);
		int nAnnots = annots->getNumAnnots(pg);
		if (nAnnots > 0)
			needDefaultFont = true;
		for (int i = 0; i < nAnnots; ++i)
		{
			if (annots->getAnnot(pg, i)->getAppearance(&obj1)->isStream())
			{
				obj1.streamGetDict()->lookup("Resources", &obj2);
				if (obj2.isDict())
					setupResources(obj2.getDict());
				obj2.free();
			}
			obj1.free();
		}
	}
	if ((form = catalog->getForm()))
	{
		if (form->getNumFields() > 0)
			needDefaultFont = true;
		for (int i = 0; i < form->getNumFields(); ++i)
		{
			form->getField(i)->getResources(&obj1);
			if (obj1.isArray())
			{
				for (int j = 0; j < obj1.arrayGetLength(); ++j)
				{
					obj1.arrayGet(j, &obj2);
					if (obj2.isDict())
						setupResources(obj2.getDict());
					obj2.free();
				}
			}
			else if (obj1.isDict())
			{
				setupResources(obj1.getDict());
			}
			obj1.free();
		}
	}
	if (needDefaultFont)
		setupDefaultFont();
	if (mode != psModeForm)
	{
		if (mode != psModeEPS && !manualCtrl)
		{
			writePSFmt("{} pdfSetup\n", globalParams->getPSDuplex() ? "true" : "false");
			if (!paperMatch)
				writePSFmt("{} {} pdfSetupPaper\n", paperWidth, paperHeight);
		}
#if OPI_SUPPORT
		if (globalParams->getPSOPI())
			writePS("/opiMatrix matrix currentmatrix def\n");
#endif
	}
	if (customCodeCbk)
	{
		std::string s = (*customCodeCbk)(this, psOutCustomDocSetup, 0, customCodeCbkData);
		if (s.size())
			writePS(s.c_str());
	}
	if (mode != psModeForm)
		writePS("end\n");

	gfree(visitedResources);
	visitedResources = nullptr;
}

void PSOutputDev::writePageTrailer()
{
	if (mode != psModeForm)
		writePS("pdfEndPage\n");
}

void PSOutputDev::writeTrailer()
{
	PSOutCustomColor* cc;

	if (mode == psModeForm)
	{
		writePS("/Foo exch /Form defineresource pop\n");
	}
	else
	{
		writePS("%%DocumentSuppliedResources:\n");
		writePS(embFontList.c_str());
		if (level == psLevel1Sep || level == psLevel2Sep || level == psLevel3Sep)
		{
			writePS("%%DocumentProcessColors:");
			if (processColors & psProcessCyan)
				writePS(" Cyan");
			if (processColors & psProcessMagenta)
				writePS(" Magenta");
			if (processColors & psProcessYellow)
				writePS(" Yellow");
			if (processColors & psProcessBlack)
				writePS(" Black");
			writePS("\n");
			writePS("%%DocumentCustomColors:");
			for (cc = customColors; cc; cc = cc->next)
			{
				writePS(" ");
				writePSString(cc->name);
			}
			writePS("\n");
			writePS("%%CMYKCustomColor:\n");
			for (cc = customColors; cc; cc = cc->next)
			{
				writePSFmt("%%+ {} {} {} {} ", Round4(cc->c), Round4(cc->m), Round4(cc->y), Round4(cc->k));
				writePSString(cc->name);
				writePS("\n");
			}
		}
	}
}

bool PSOutputDev::checkIfPageNeedsToBeRasterized(int pg)
{
	PreScanOutputDev* scan;
	bool              rasterize;

	if (globalParams->getPSAlwaysRasterize())
	{
		rasterize = true;
	}
	else
	{
		scan = new PreScanOutputDev();
		//~ this could depend on the printing flag, e.g., if an annotation
		//~   uses transparency --> need to pass the printing flag into
		//~   constructor, init, writeDocSetup
		doc->getCatalog()->getPage(pg)->display(scan, 72, 72, 0, true, true, true);
		rasterize = scan->usesTransparency() || scan->usesPatternImageMask();
		delete scan;
		if (rasterize && globalParams->getPSNeverRasterize())
		{
			error(errSyntaxWarning, -1,
			      "PDF page uses transparency and the psNeverRasterize option is "
			      "set - output may not be correct");
			rasterize = false;
		}
	}
	return rasterize;
}

void PSOutputDev::setupResources(Dict* resDict)
{
	Object xObjDict, xObjRef, xObj, patDict, patRef, pat;
	Object gsDict, gsRef, gs, smask, smaskGroup, resObj;
	Ref    ref0;
	bool   skip;
	int    i;

	setupFonts(resDict);
	setupImages(resDict);

	//----- recursively scan XObjects
	resDict->lookup("XObject", &xObjDict);
	if (xObjDict.isDict())
	{
		for (i = 0; i < xObjDict.dictGetLength(); ++i)
		{
			// check for an already-visited XObject
			skip = false;
			if ((xObjDict.dictGetValNF(i, &xObjRef)->isRef()))
			{
				ref0 = xObjRef.getRef();
				if (ref0.num < 0 || ref0.num >= xref->getNumObjects())
				{
					skip = true;
				}
				else
				{
					skip                       = (bool)visitedResources[ref0.num];
					visitedResources[ref0.num] = 1;
				}
			}
			if (!skip)
			{
				// process the XObject's resource dictionary
				xObjDict.dictGetVal(i, &xObj);
				if (xObj.isStream())
				{
					xObj.streamGetDict()->lookup("Resources", &resObj);
					if (resObj.isDict())
						setupResources(resObj.getDict());
					resObj.free();
				}
				xObj.free();
			}

			xObjRef.free();
		}
	}
	xObjDict.free();

	//----- recursively scan Patterns
	resDict->lookup("Pattern", &patDict);
	if (patDict.isDict())
	{
		inType3Char = true;
		for (i = 0; i < patDict.dictGetLength(); ++i)
		{
			// check for an already-visited Pattern
			skip = false;
			if ((patDict.dictGetValNF(i, &patRef)->isRef()))
			{
				ref0 = patRef.getRef();
				if (ref0.num < 0 || ref0.num >= xref->getNumObjects())
				{
					skip = true;
				}
				else
				{
					skip                       = (bool)visitedResources[ref0.num];
					visitedResources[ref0.num] = 1;
				}
			}
			if (!skip)
			{
				// process the Pattern's resource dictionary
				patDict.dictGetVal(i, &pat);
				if (pat.isStream())
				{
					pat.streamGetDict()->lookup("Resources", &resObj);
					if (resObj.isDict())
						setupResources(resObj.getDict());
					resObj.free();
				}
				pat.free();
			}

			patRef.free();
		}
		inType3Char = false;
	}
	patDict.free();

	//----- recursively scan SMask transparency groups in ExtGState dicts
	resDict->lookup("ExtGState", &gsDict);
	if (gsDict.isDict())
	{
		for (i = 0; i < gsDict.dictGetLength(); ++i)
		{
			// check for an already-visited ExtGState
			skip = false;
			if ((gsDict.dictGetValNF(i, &gsRef)->isRef()))
			{
				ref0 = gsRef.getRef();
				if (ref0.num < 0 || ref0.num >= xref->getNumObjects())
				{
					skip = true;
				}
				else
				{
					skip                       = (bool)visitedResources[ref0.num];
					visitedResources[ref0.num] = 1;
				}
			}
			if (!skip)
			{
				// process the ExtGState's SMask's transparency group's resource dict
				if (gsDict.dictGetVal(i, &gs)->isDict())
				{
					if (gs.dictLookup("SMask", &smask)->isDict())
					{
						if (smask.dictLookup("G", &smaskGroup)->isStream())
						{
							smaskGroup.streamGetDict()->lookup("Resources", &resObj);
							if (resObj.isDict())
								setupResources(resObj.getDict());
							resObj.free();
						}
						smaskGroup.free();
					}
					smask.free();
				}
				gs.free();
			}

			gsRef.free();
		}
	}
	gsDict.free();

	setupForms(resDict);
}

void PSOutputDev::setupFonts(Dict* resDict)
{
	Object       obj1, obj2;
	Ref          r;
	GfxFontDict* gfxFontDict;
	GfxFont*     font;
	int          i;

	gfxFontDict = nullptr;
	resDict->lookupNF("Font", &obj1);
	if (obj1.isRef())
	{
		obj1.fetch(xref, &obj2);
		if (obj2.isDict())
		{
			r           = obj1.getRef();
			gfxFontDict = new GfxFontDict(xref, &r, obj2.getDict());
		}
		obj2.free();
	}
	else if (obj1.isDict())
	{
		gfxFontDict = new GfxFontDict(xref, nullptr, obj1.getDict());
	}
	if (gfxFontDict)
	{
		for (i = 0; i < gfxFontDict->getNumFonts(); ++i)
			if ((font = gfxFontDict->getFont(i)))
				setupFont(font, resDict);
		delete gfxFontDict;
	}
	obj1.free();
}

void PSOutputDev::setupFont(GfxFont* font, Dict* parentResDict)
{
	PSFontInfo* fi;
	GfxFontLoc* fontLoc;
	bool        subst;
	char        buf[16];
	UnicodeMap* uMap;
	double      xs, ys;
	int         code;
	double      w1, w2;
	int         i, j;

	// check if font is already set up
	for (i = 0; i < fontInfo->getLength(); ++i)
	{
		fi = (PSFontInfo*)fontInfo->get(i);
		if (fi->fontID.num == font->getID()->num && fi->fontID.gen == font->getID()->gen)
			return;
	}

	// add fontInfo entry
	fi = new PSFontInfo(*font->getID());
	fontInfo->append(fi);

	xs = ys = 1;
	subst   = false;

	if (font->getType() == fontType3)
	{
		fi->ff = setupType3Font(font, parentResDict);
	}
	else
	{
		if ((fontLoc = font->locateFont(xref, true)))
		{
			switch (fontLoc->locType)
			{
			case gfxFontLocEmbedded:
				switch (fontLoc->fontType)
				{
				case fontType1:
					fi->ff = setupEmbeddedType1Font(font, &fontLoc->embFontID);
					break;
				case fontType1C:
					fi->ff = setupEmbeddedType1CFont(font, &fontLoc->embFontID);
					break;
				case fontType1COT:
					fi->ff = setupEmbeddedOpenTypeT1CFont(font, &fontLoc->embFontID);
					break;
				case fontTrueType:
				case fontTrueTypeOT:
					fi->ff = setupEmbeddedTrueTypeFont(font, &fontLoc->embFontID);
					break;
				case fontCIDType0C:
					fi->ff = setupEmbeddedCIDType0Font(font, &fontLoc->embFontID);
					break;
				case fontCIDType2:
				case fontCIDType2OT:
					//~ should check to see if font actually uses vertical mode
					fi->ff = setupEmbeddedCIDTrueTypeFont(font, &fontLoc->embFontID, true);
					break;
				case fontCIDType0COT:
					fi->ff = setupEmbeddedOpenTypeCFFFont(font, &fontLoc->embFontID);
					break;
				default:
					break;
				}
				break;
			case gfxFontLocExternal:
				//~ add cases for other external 16-bit fonts
				switch (fontLoc->fontType)
				{
				case fontType1:
					fi->ff = setupExternalType1Font(font, fontLoc->path.string());
					break;
				case fontType1COT:
					fi->ff = setupExternalOpenTypeT1CFont(font, fontLoc->path.string());
					break;
				case fontTrueType:
				case fontTrueTypeOT:
					fi->ff = setupExternalTrueTypeFont(font, fontLoc->path.string(), fontLoc->fontNum);
					break;
				case fontCIDType2:
				case fontCIDType2OT:
					//~ should check to see if font actually uses vertical mode
					fi->ff = setupExternalCIDTrueTypeFont(font, fontLoc->path.string(), fontLoc->fontNum, true);
					break;
				case fontCIDType0COT:
					fi->ff = setupExternalOpenTypeCFFFont(font, fontLoc->path.string());
					break;
				default:
					break;
				}
				break;
			case gfxFontLocResident:
				if (!fontFileInfo.contains(fontLoc->path.string()))
				{
					// handle psFontPassthrough
					fi->ff = std::make_shared<PSFontFileInfo>(fontLoc->path.string(), fontLoc->fontType, psFontFileResident);
					fontFileInfo.emplace(fi->ff->psName, fi->ff);
				}
				break;
			}
		}

		if (!fi->ff)
		{
			if (font->isCIDFont())
				error(errSyntaxError, -1, "Couldn't find a font for '{}' ('{}' character collection)", font->getName(), ((GfxCIDFont*)font)->getCollection());
			else
				error(errSyntaxError, -1, "Couldn't find a font for '{}'", font->getName());
			delete fontLoc;
			return;
		}

		// scale substituted 8-bit fonts
		if (fontLoc->locType == gfxFontLocResident && fontLoc->substIdx >= 0)
		{
			std::string charName;
			subst = true;
			for (code = 0; code < 256; ++code)
				if ((charName = ((Gfx8BitFont*)font)->getCharName(code)).size() && charName[0] == 'm' && charName[1] == '\0')
					break;
			if (code < 256)
				w1 = ((Gfx8BitFont*)font)->getWidth((uint8_t)code);
			else
				w1 = 0;
			w2 = psBase14SubstFonts[fontLoc->substIdx].mWidth;
			xs = w1 / w2;
			if (xs < 0.1)
				xs = 1;
		}

		// handle encodings for substituted CID fonts
		if (fontLoc->locType == gfxFontLocResident && fontLoc->fontType >= fontCIDType0)
		{
			subst = true;
			if (fi->ff->encoding.empty())
			{
				if ((uMap = globalParams->getUnicodeMap(fontLoc->encoding)))
				{
					fi->ff->encoding = fontLoc->encoding;
					uMap->decRefCnt();
				}
				else
				{
					error(errSyntaxError, -1, "Couldn't find Unicode map for 16-bit font encoding '{}'", fontLoc->encoding);
				}
			}
		}

		delete fontLoc;
	}

	// generate PostScript code to set up the font
	if (font->isCIDFont())
	{
		if (level >= psLevel3)
			writePSFmt("/F{}_{} /{} {} pdfMakeFont16L3\n", font->getID()->num, font->getID()->gen, fi->ff->psName, font->getWMode());
		else
			writePSFmt("/F{}_{} /{} {} pdfMakeFont16\n", font->getID()->num, font->getID()->gen, fi->ff->psName, font->getWMode());
	}
	else
	{
		writePSFmt("/F{}_{} /{} {} {}\n", font->getID()->num, font->getID()->gen, fi->ff->psName, Round6(xs), Round6(ys));
		for (i = 0; i < 256; i += 8)
		{
			writePS((char*)((i == 0) ? "[ " : "  "));
			for (j = 0; j < 8; ++j)
			{
				std::string charName;
				if (font->getType() == fontTrueType && !subst && !((Gfx8BitFont*)font)->getHasEncoding())
				{
					snprintf(buf, sizeof(buf), "c%02x", i + j);
					charName = buf;
				}
				else
				{
					charName = ((Gfx8BitFont*)font)->getCharName(i + j);
				}
				writePS("/");
				writePSName(charName.size() ? charName : ".notdef");
				// the empty name is legal in PDF and PostScript, but PostScript uses a double-slash (//...) for "immediately evaluated names",
				// so we need to add a space character here
				if (charName == "") writePS(" ");
			}
			writePS((i == 256 - 8) ? (char*)"]\n" : (char*)"\n");
		}
		writePS("pdfMakeFont\n");
	}
}

sPSFontFileInfo PSOutputDev::setupEmbeddedType1Font(GfxFont* font, Ref* id)
{
	std::string psName, origFont, cleanFont;
	Object      refObj, strObj, obj1, obj2;
	Dict*       dict;
	char        buf[4096];
	bool        rename;
	int         length1, length2, n;

	// check if font is already embedded
	if (font->getEmbeddedFontName().empty())
	{
		rename = true;
	}
	else if (auto it = fontFileInfo.find(font->getEmbeddedFontName()); it != fontFileInfo.end())
	{
		auto& ff = it->second;
		if (ff->loc == psFontFileEmbedded && ff->embFontID.num == id->num && ff->embFontID.gen == id->gen)
			return ff;
		rename = true;
	}
	else
	{
		rename = false;
	}

	// generate name
	// (this assumes that the PS font name matches the PDF font name)
	if (rename)
		psName = makePSFontName(font, id);
	else
		psName = font->getEmbeddedFontName();

	// get the font stream and info
	refObj.initRef(id->num, id->gen);
	refObj.fetch(xref, &strObj);
	refObj.free();
	if (!strObj.isStream())
	{
		error(errSyntaxError, -1, "Embedded font file object is not a stream");
		goto err1;
	}
	if (!(dict = strObj.streamGetDict()))
	{
		error(errSyntaxError, -1, "Embedded font stream is missing its dictionary");
		goto err1;
	}
	dict->lookup("Length1", &obj1);
	dict->lookup("Length2", &obj2);
	if (!obj1.isInt() || !obj2.isInt())
	{
		error(errSyntaxError, -1, "Missing length fields in embedded font stream dictionary");
		obj1.free();
		obj2.free();
		goto err1;
	}
	length1 = obj1.getInt();
	length2 = obj2.getInt();
	obj1.free();
	obj2.free();

	// read the font file
	strObj.streamReset();
	while ((n = strObj.streamGetBlock(buf, sizeof(buf))) > 0)
		origFont.append(buf, n);
	strObj.streamClose();
	strObj.free();

	// beginning comment
	writePSFmt("%%BeginResource: font {}\n", psName);
	embFontList += "%%+ font ";
	embFontList += psName;
	embFontList += "\n";

	// clean up the font file
	cleanFont = fixType1Font(origFont, length1, length2);
	if (rename)
		renameType1Font(cleanFont, psName);
	writePSBlock(cleanFont.c_str(), cleanFont.size());

	// ending comment
	writePS("%%EndResource\n");

	{
		auto ff       = std::make_shared<PSFontFileInfo>(psName, font->getType(), psFontFileEmbedded);
		ff->embFontID = *id;
		fontFileInfo.emplace(ff->psName, ff);
		return ff;
	}

err1:
	strObj.free();
	return nullptr;
}

sPSFontFileInfo PSOutputDev::setupExternalType1Font(GfxFont* font, const std::string& fileName)
{
	static char hexChar[17] = "0123456789abcdef";
	std::string psName;
	FILE*       fontFile;
	int         buf[6];
	int         c, n, i;

	if (font->getName().size())
	{
		// check if font is already embedded
		if (auto it = fontFileInfo.find(font->getName()); it != fontFileInfo.end()) return it->second;
		// this assumes that the PS font name matches the PDF font name
		psName = font->getName();
	}
	else
	{
		// generate name
		//~ this won't work -- the PS font name won't match
		psName = makePSFontName(font, font->getID());
	}

	// beginning comment
	writePSFmt("%%BeginResource: font {}\n", psName);
	embFontList += "%%+ font ";
	embFontList += psName;
	embFontList += "\n";

	// open the font file
	if (!(fontFile = fopen(fileName.c_str(), "rb")))
	{
		error(errIO, -1, "Couldn't open external font file");
		return nullptr;
	}

	// check for PFB format
	buf[0] = fgetc(fontFile);
	buf[1] = fgetc(fontFile);
	if (buf[0] == 0x80 && buf[1] == 0x01)
	{
		while (1)
		{
			for (i = 2; i < 6; ++i)
				buf[i] = fgetc(fontFile);
			if (buf[2] == EOF || buf[3] == EOF || buf[4] == EOF || buf[5] == EOF)
				break;
			n = buf[2] + (buf[3] << 8) + (buf[4] << 16) + (buf[5] << 24);
			if (buf[1] == 0x01)
			{
				for (i = 0; i < n; ++i)
				{
					if ((c = fgetc(fontFile)) == EOF)
						break;
					writePSChar((char)c);
				}
			}
			else
			{
				for (i = 0; i < n; ++i)
				{
					if ((c = fgetc(fontFile)) == EOF)
						break;
					writePSChar(hexChar[(c >> 4) & 0x0f]);
					writePSChar(hexChar[c & 0x0f]);
					if (i % 32 == 31)
						writePSChar('\n');
				}
			}
			buf[0] = fgetc(fontFile);
			buf[1] = fgetc(fontFile);
			if (buf[0] == EOF || buf[1] == EOF || (buf[0] == 0x80 && buf[1] == 0x03))
			{
				break;
			}
			else if (!(buf[0] == 0x80 && (buf[1] == 0x01 || buf[1] == 0x02)))
			{
				error(errSyntaxError, -1, "Invalid PFB header in external font file");
				break;
			}
		}
		writePSChar('\n');

		// plain text (PFA) format
	}
	else
	{
		writePSChar((char)buf[0]);
		writePSChar((char)buf[1]);
		while ((c = fgetc(fontFile)) != EOF)
			writePSChar((char)c);
	}

	fclose(fontFile);

	// ending comment
	writePS("%%EndResource\n");

	auto ff         = std::make_shared<PSFontFileInfo>(psName, font->getType(), psFontFileExternal);
	ff->extFileName = fileName;
	fontFileInfo.emplace(ff->psName, ff);
	return ff;
}

sPSFontFileInfo PSOutputDev::setupEmbeddedType1CFont(GfxFont* font, Ref* id)
{
	char*       fontBuf;
	int         fontLen;
	FoFiType1C* ffT1C;

	// check if font is already embedded
	for (const auto& [key, ff] : fontFileInfo)
		if (ff->loc == psFontFileEmbedded && ff->embFontID.num == id->num && ff->embFontID.gen == id->gen)
			return ff;

	// generate name
	const auto psName = makePSFontName(font, id);

	// beginning comment
	writePSFmt("%%BeginResource: font {}\n", psName);
	embFontList += "%%+ font ";
	embFontList += psName;
	embFontList += "\n";

	// convert it to a Type 1 font
	if ((fontBuf = font->readEmbFontFile(xref, &fontLen)))
	{
		if ((ffT1C = FoFiType1C::make(fontBuf, fontLen)))
		{
			ffT1C->convertToType1(psName.c_str(), {}, true, outputFunc, outputStream);
			delete ffT1C;
		}
		gfree(fontBuf);
	}

	// ending comment
	writePS("%%EndResource\n");

	auto ff       = std::make_shared<PSFontFileInfo>(psName, font->getType(), psFontFileEmbedded);
	ff->embFontID = *id;
	fontFileInfo.emplace(ff->psName, ff);
	return ff;
}

sPSFontFileInfo PSOutputDev::setupEmbeddedOpenTypeT1CFont(GfxFont* font, Ref* id)
{
	char*         fontBuf;
	int           fontLen;
	FoFiTrueType* ffTT;

	// check if font is already embedded
	for (const auto& [key, ff] : fontFileInfo)
		if (ff->loc == psFontFileEmbedded && ff->embFontID.num == id->num && ff->embFontID.gen == id->gen)
			return ff;

	// generate name
	const auto psName = makePSFontName(font, id);

	// beginning comment
	writePSFmt("%%BeginResource: font {}\n", psName);
	embFontList += "%%+ font ";
	embFontList += psName;
	embFontList += "\n";

	// convert it to a Type 1 font
	if ((fontBuf = font->readEmbFontFile(xref, &fontLen)))
	{
		if ((ffTT = FoFiTrueType::make(fontBuf, fontLen, 0, true)))
		{
			if (ffTT->isOpenTypeCFF())
				ffTT->convertToType1(psName.c_str(), {}, true, outputFunc, outputStream);
			delete ffTT;
		}
		gfree(fontBuf);
	}

	// ending comment
	writePS("%%EndResource\n");

	auto ff       = std::make_shared<PSFontFileInfo>(psName, font->getType(), psFontFileEmbedded);
	ff->embFontID = *id;
	fontFileInfo.emplace(ff->psName, ff);
	return ff;
}

sPSFontFileInfo PSOutputDev::setupExternalOpenTypeT1CFont(GfxFont* font, const std::string& fileName)
{
	std::string   psName;
	FoFiTrueType* ffTT;

	if (font->getName().size())
	{
		// check if font is already embedded
		if (auto it = fontFileInfo.find(font->getName()); it != fontFileInfo.end()) return it->second;
		// this assumes that the PS font name matches the PDF font name
		psName = font->getName();
	}
	else
	{
		// generate name
		psName = makePSFontName(font, font->getID());
	}

	// beginning comment
	writePSFmt("%%BeginResource: font {}\n", psName);
	embFontList += "%%+ font ";
	embFontList += psName;
	embFontList += "\n";

	// convert it to a Type 1 font
	if ((ffTT = FoFiTrueType::load(fileName.c_str(), 0, true)))
	{
		if (ffTT->isOpenTypeCFF())
			ffTT->convertToType1(psName.c_str(), {}, true, outputFunc, outputStream);
		delete ffTT;
	}

	// ending comment
	writePS("%%EndResource\n");

	auto ff         = std::make_shared<PSFontFileInfo>(psName, font->getType(), psFontFileExternal);
	ff->extFileName = fileName;
	fontFileInfo.emplace(ff->psName, ff);
	return ff;
}

sPSFontFileInfo PSOutputDev::setupEmbeddedTrueTypeFont(GfxFont* font, Ref* id)
{
	char*         fontBuf;
	int           fontLen;
	FoFiTrueType* ffTT;
	int*          codeToGID;

	// get the code-to-GID mapping
	if (!(fontBuf = font->readEmbFontFile(xref, &fontLen))) return nullptr;
	if (!(ffTT = FoFiTrueType::make(fontBuf, fontLen, 0)))
	{
		gfree(fontBuf);
		return nullptr;
	}
	codeToGID = ((Gfx8BitFont*)font)->getCodeToGIDMap(ffTT);

	// check if font is already embedded
	for (const auto& [key, ff] : fontFileInfo)
		if (ff->loc == psFontFileEmbedded && ff->type == font->getType() && ff->embFontID.num == id->num && ff->embFontID.gen == id->gen && ff->codeToGIDLen == 256 && !memcmp(ff->codeToGID, codeToGID, 256 * sizeof(int)))
		{
			gfree(codeToGID);
			delete ffTT;
			gfree(fontBuf);
			return ff;
		}

	// generate name
	const auto psName = makePSFontName(font, id);

	// beginning comment
	writePSFmt("%%BeginResource: font {}\n", psName);
	embFontList += "%%+ font ";
	embFontList += psName;
	embFontList += "\n";

	// convert it to a Type 42 font
	ffTT->convertToType42(psName.c_str(), ((Gfx8BitFont*)font)->getHasEncoding() ? ((Gfx8BitFont*)font)->getEncoding() : VEC_STR {}, codeToGID, outputFunc, outputStream);
	delete ffTT;
	gfree(fontBuf);

	// ending comment
	writePS("%%EndResource\n");

	auto ff          = std::make_shared<PSFontFileInfo>(psName, font->getType(), psFontFileEmbedded);
	ff->embFontID    = *id;
	ff->codeToGID    = codeToGID;
	ff->codeToGIDLen = 256;
	fontFileInfo.emplace(ff->psName, ff);
	return ff;
}

sPSFontFileInfo PSOutputDev::setupExternalTrueTypeFont(GfxFont* font, const std::string& fileName, int fontNum)
{
	FoFiTrueType* ffTT;
	int*          codeToGID;

	// get the code-to-GID mapping
	if (!(ffTT = FoFiTrueType::load(fileName.c_str(), fontNum))) return nullptr;
	codeToGID = ((Gfx8BitFont*)font)->getCodeToGIDMap(ffTT);

	// check if font is already embedded
	for (const auto& [key, ff] : fontFileInfo)
		if (ff->loc == psFontFileExternal && ff->type == font->getType() && ff->extFileName == fileName && ff->codeToGIDLen == 256 && !memcmp(ff->codeToGID, codeToGID, 256 * sizeof(int)))
		{
			gfree(codeToGID);
			delete ffTT;
			return ff;
		}

	// generate name
	const auto psName = makePSFontName(font, font->getID());

	// beginning comment
	writePSFmt("%%BeginResource: font {}\n", psName);
	embFontList += "%%+ font ";
	embFontList += psName;
	embFontList += "\n";

	// convert it to a Type 42 font
	ffTT->convertToType42(psName.c_str(), ((Gfx8BitFont*)font)->getHasEncoding() ? ((Gfx8BitFont*)font)->getEncoding() : VEC_STR {}, codeToGID, outputFunc, outputStream);
	delete ffTT;

	// ending comment
	writePS("%%EndResource\n");

	auto ff          = std::make_shared<PSFontFileInfo>(psName, font->getType(), psFontFileExternal);
	ff->extFileName  = fileName;
	ff->codeToGID    = codeToGID;
	ff->codeToGIDLen = 256;
	fontFileInfo.emplace(ff->psName, ff);
	return ff;
}

sPSFontFileInfo PSOutputDev::setupEmbeddedCIDType0Font(GfxFont* font, Ref* id)
{
	char*       fontBuf;
	int         fontLen;
	FoFiType1C* ffT1C;

	// check if font is already embedded
	for (const auto& [key, ff] : fontFileInfo)
		if (ff->loc == psFontFileEmbedded && ff->embFontID.num == id->num && ff->embFontID.gen == id->gen)
			return ff;

	// generate name
	const auto psName = makePSFontName(font, id);

	// beginning comment
	writePSFmt("%%BeginResource: font {}\n", psName);
	embFontList += "%%+ font ";
	embFontList += psName;
	embFontList += "\n";

	// convert it to a Type 0 font
	if ((fontBuf = font->readEmbFontFile(xref, &fontLen)))
	{
		if ((ffT1C = FoFiType1C::make(fontBuf, fontLen)))
		{
			if (globalParams->getPSLevel() >= psLevel3)
			{
				// Level 3: use a CID font
				ffT1C->convertToCIDType0(psName.c_str(), ((GfxCIDFont*)font)->getCIDToGID(), ((GfxCIDFont*)font)->getCIDToGIDLen(), outputFunc, outputStream);
			}
			else
			{
				// otherwise: use a non-CID composite font
				ffT1C->convertToType0(psName.c_str(), ((GfxCIDFont*)font)->getCIDToGID(), ((GfxCIDFont*)font)->getCIDToGIDLen(), outputFunc, outputStream);
			}
			delete ffT1C;
		}
		gfree(fontBuf);
	}

	// ending comment
	writePS("%%EndResource\n");

	auto ff       = std::make_shared<PSFontFileInfo>(psName, font->getType(), psFontFileEmbedded);
	ff->embFontID = *id;
	fontFileInfo.emplace(ff->psName, ff);
	return ff;
}

sPSFontFileInfo PSOutputDev::setupEmbeddedCIDTrueTypeFont(GfxFont* font, Ref* id, bool needVerticalMetrics)
{
	char*         fontBuf;
	int           fontLen;
	FoFiTrueType* ffTT;
	int*          codeToGID;
	int           codeToGIDLen;

	// get the code-to-GID mapping
	codeToGID    = ((GfxCIDFont*)font)->getCIDToGID();
	codeToGIDLen = ((GfxCIDFont*)font)->getCIDToGIDLen();

	// check if font is already embedded
	for (const auto& [key, ff] : fontFileInfo)
		if (ff->loc == psFontFileEmbedded && ff->type == font->getType() && ff->embFontID.num == id->num && ff->embFontID.gen == id->gen && ff->codeToGIDLen == codeToGIDLen && ((!ff->codeToGID && !codeToGID) || (ff->codeToGID && codeToGID && !memcmp(ff->codeToGID, codeToGID, codeToGIDLen * sizeof(int)))))
			return ff;

	// generate name
	const auto psName = makePSFontName(font, id);

	// beginning comment
	writePSFmt("%%BeginResource: font {}\n", psName);
	embFontList += "%%+ font ";
	embFontList += psName;
	embFontList += "\n";

	// convert it to a Type 0 font
	if ((fontBuf = font->readEmbFontFile(xref, &fontLen)))
	{
		if ((ffTT = FoFiTrueType::make(fontBuf, fontLen, 0)))
		{
			if (globalParams->getPSLevel() >= psLevel3)
			{
				// Level 3: use a CID font
				ffTT->convertToCIDType2(psName.c_str(), codeToGID, codeToGIDLen, needVerticalMetrics, outputFunc, outputStream);
			}
			else
			{
				// otherwise: use a non-CID composite font
				ffTT->convertToType0(psName.c_str(), codeToGID, codeToGIDLen, needVerticalMetrics, outputFunc, outputStream);
			}
			delete ffTT;
		}
		gfree(fontBuf);
	}

	// ending comment
	writePS("%%EndResource\n");

	auto ff       = std::make_shared<PSFontFileInfo>(psName, font->getType(), psFontFileEmbedded);
	ff->embFontID = *id;
	if (codeToGIDLen)
	{
		ff->codeToGID = (int*)gmallocn(codeToGIDLen, sizeof(int));
		memcpy(ff->codeToGID, codeToGID, codeToGIDLen * sizeof(int));
		ff->codeToGIDLen = codeToGIDLen;
	}
	fontFileInfo.emplace(ff->psName, ff);
	return ff;
}

sPSFontFileInfo PSOutputDev::setupExternalCIDTrueTypeFont(GfxFont* font, const std::string& fileName, int fontNum, bool needVerticalMetrics)
{
	std::string        psName;
	FoFiTrueType*      ffTT;
	int*               codeToGID;
	int                codeToGIDLen;
	CharCodeToUnicode* ctu;
	Unicode            uBuf[8];
	int                cmap, cmapPlatform, cmapEncoding, code;

	// create a code-to-GID mapping, via Unicode
	if (!(ffTT = FoFiTrueType::load(fileName.c_str(), fontNum))) return nullptr;
	if (!(ctu = ((GfxCIDFont*)font)->getToUnicode()))
	{
		error(errSyntaxError, -1, "Couldn't find a mapping to Unicode for font '{}'", font->getName());
		delete ffTT;
		return nullptr;
	}
	// look for a Unicode cmap
	for (cmap = 0; cmap < ffTT->getNumCmaps(); ++cmap)
	{
		cmapPlatform = ffTT->getCmapPlatform(cmap);
		cmapEncoding = ffTT->getCmapEncoding(cmap);
		if ((cmapPlatform == 3 && cmapEncoding == 1) || (cmapPlatform == 0 && cmapEncoding <= 4))
			break;
	}
	if (cmap >= ffTT->getNumCmaps())
	{
		error(errSyntaxError, -1, "Couldn't find a Unicode cmap in font '{}'", font->getName().size());
		ctu->decRefCnt();
		delete ffTT;
		return nullptr;
	}
	// map CID -> Unicode -> GID
	if (ctu->isIdentity())
		codeToGIDLen = 65536;
	else
		codeToGIDLen = ctu->getLength();
	codeToGID = (int*)gmallocn(codeToGIDLen, sizeof(int));
	for (code = 0; code < codeToGIDLen; ++code)
		if (ctu->mapToUnicode(code, uBuf, 8) > 0)
			codeToGID[code] = ffTT->mapCodeToGID(cmap, uBuf[0]);
		else
			codeToGID[code] = 0;
	ctu->decRefCnt();

	// check if font is already embedded
	for (const auto& [key, ff] : fontFileInfo)
		if (ff->loc == psFontFileExternal && ff->type == font->getType() && ff->extFileName == fileName && ff->codeToGIDLen == codeToGIDLen && ff->codeToGID && !memcmp(ff->codeToGID, codeToGID, codeToGIDLen * sizeof(int)))
		{
			gfree(codeToGID);
			delete ffTT;
			return ff;
		}

	// check for embedding permission
	if (ffTT->getEmbeddingRights() < 1)
	{
		error(errSyntaxError, -1, "TrueType font '{}' does not allow embedding", font->getName());
		gfree(codeToGID);
		delete ffTT;
		return nullptr;
	}

	// generate name
	psName = makePSFontName(font, font->getID());

	// beginning comment
	writePSFmt("%%BeginResource: font {}\n", psName);
	embFontList += "%%+ font ";
	embFontList += psName;
	embFontList += "\n";

	// convert it to a Type 0 font
	//~ this should use fontNum to load the correct font
	if (globalParams->getPSLevel() >= psLevel3)
	{
		// Level 3: use a CID font
		ffTT->convertToCIDType2(psName.c_str(), codeToGID, codeToGIDLen, needVerticalMetrics, outputFunc, outputStream);
	}
	else
	{
		// otherwise: use a non-CID composite font
		ffTT->convertToType0(psName.c_str(), codeToGID, codeToGIDLen, needVerticalMetrics, outputFunc, outputStream);
	}
	delete ffTT;

	// ending comment
	writePS("%%EndResource\n");

	auto ff          = std::make_shared<PSFontFileInfo>(psName, font->getType(), psFontFileExternal);
	ff->extFileName  = fileName;
	ff->codeToGID    = codeToGID;
	ff->codeToGIDLen = codeToGIDLen;
	fontFileInfo.emplace(ff->psName, ff);
	return ff;
}

sPSFontFileInfo PSOutputDev::setupEmbeddedOpenTypeCFFFont(GfxFont* font, Ref* id)
{
	char*         fontBuf;
	int           fontLen;
	FoFiTrueType* ffTT;
	int           n;

	// check if font is already embedded
	for (const auto& [key, ff] : fontFileInfo)
		if (ff->loc == psFontFileEmbedded && ff->embFontID.num == id->num && ff->embFontID.gen == id->gen)
			return ff;

	// generate name
	const auto psName = makePSFontName(font, id);

	// beginning comment
	writePSFmt("%%BeginResource: font {}\n", psName);
	embFontList += "%%+ font ";
	embFontList += psName;
	embFontList += "\n";

	// convert it to a Type 0 font
	if ((fontBuf = font->readEmbFontFile(xref, &fontLen)))
	{
		if ((ffTT = FoFiTrueType::make(fontBuf, fontLen, 0, true)))
		{
			if (ffTT->isOpenTypeCFF())
			{
				if (globalParams->getPSLevel() >= psLevel3)
				{
					// Level 3: use a CID font
					ffTT->convertToCIDType0(psName.c_str(), ((GfxCIDFont*)font)->getCIDToGID(), ((GfxCIDFont*)font)->getCIDToGIDLen(), outputFunc, outputStream);
				}
				else
				{
					// otherwise: use a non-CID composite font
					ffTT->convertToType0(psName.c_str(), ((GfxCIDFont*)font)->getCIDToGID(), ((GfxCIDFont*)font)->getCIDToGIDLen(), outputFunc, outputStream);
				}
			}
			delete ffTT;
		}
		gfree(fontBuf);
	}

	// ending comment
	writePS("%%EndResource\n");

	auto ff       = std::make_shared<PSFontFileInfo>(psName, font->getType(), psFontFileEmbedded);
	ff->embFontID = *id;
	if ((n = ((GfxCIDFont*)font)->getCIDToGIDLen()))
	{
		ff->codeToGID = (int*)gmallocn(n, sizeof(int));
		memcpy(ff->codeToGID, ((GfxCIDFont*)font)->getCIDToGID(), n * sizeof(int));
		ff->codeToGIDLen = n;
	}
	fontFileInfo.emplace(ff->psName, ff);
	return ff;
}

// This assumes an OpenType CFF font that has a Unicode cmap (in the OpenType section), and a CFF blob that uses an identity CID-to-GID mapping.
sPSFontFileInfo PSOutputDev::setupExternalOpenTypeCFFFont(GfxFont* font, const std::string& fileName)
{
	std::string        psName;
	FoFiTrueType*      ffTT;
	CharCodeToUnicode* ctu;
	Unicode            uBuf[8];
	int*               codeToGID;
	int                codeToGIDLen;
	int                cmap, cmapPlatform, cmapEncoding, code;

	// create a code-to-GID mapping, via Unicode
	if (!(ffTT = FoFiTrueType::load(fileName.c_str(), 0, true))) return nullptr;
	if (!ffTT->isOpenTypeCFF())
	{
		delete ffTT;
		return nullptr;
	}
	if (!(ctu = ((GfxCIDFont*)font)->getToUnicode()))
	{
		error(errSyntaxError, -1, "Couldn't find a mapping to Unicode for font '{}'", font->getName());
		delete ffTT;
		return nullptr;
	}
	// look for a Unicode cmap
	for (cmap = 0; cmap < ffTT->getNumCmaps(); ++cmap)
	{
		cmapPlatform = ffTT->getCmapPlatform(cmap);
		cmapEncoding = ffTT->getCmapEncoding(cmap);
		if ((cmapPlatform == 3 && cmapEncoding == 1) || (cmapPlatform == 0 && cmapEncoding <= 4))
			break;
	}
	if (cmap >= ffTT->getNumCmaps())
	{
		error(errSyntaxError, -1, "Couldn't find a Unicode cmap in font '{}'", font->getName());
		ctu->decRefCnt();
		delete ffTT;
		return nullptr;
	}
	// map CID -> Unicode -> GID
	if (ctu->isIdentity())
		codeToGIDLen = 65536;
	else
		codeToGIDLen = ctu->getLength();
	codeToGID = (int*)gmallocn(codeToGIDLen, sizeof(int));
	for (code = 0; code < codeToGIDLen; ++code)
		if (ctu->mapToUnicode(code, uBuf, 8) > 0)
			codeToGID[code] = ffTT->mapCodeToGID(cmap, uBuf[0]);
		else
			codeToGID[code] = 0;
	ctu->decRefCnt();

	// check if font is already embedded
	for (const auto& [key, ff] : fontFileInfo)
		if (ff->loc == psFontFileExternal && ff->type == font->getType() && ff->extFileName == fileName && ff->codeToGIDLen == codeToGIDLen && ff->codeToGID && !memcmp(ff->codeToGID, codeToGID, codeToGIDLen * sizeof(int)))
		{
			gfree(codeToGID);
			delete ffTT;
			return ff;
		}

	// generate name
	psName = makePSFontName(font, font->getID());

	// beginning comment
	writePSFmt("%%BeginResource: font {}\n", psName);
	embFontList += "%%+ font ";
	embFontList += psName.c_str();
	embFontList += "\n";

	// convert it to a Type 0 font
	if (globalParams->getPSLevel() >= psLevel3)
	{
		// Level 3: use a CID font
		ffTT->convertToCIDType0(psName.c_str(), codeToGID, codeToGIDLen, outputFunc, outputStream);
	}
	else
	{
		// otherwise: use a non-CID composite font
		ffTT->convertToType0(psName.c_str(), codeToGID, codeToGIDLen, outputFunc, outputStream);
	}
	delete ffTT;

	// ending comment
	writePS("%%EndResource\n");

	auto ff          = std::make_shared<PSFontFileInfo>(psName, font->getType(), psFontFileExternal);
	ff->extFileName  = fileName;
	ff->codeToGID    = codeToGID;
	ff->codeToGIDLen = codeToGIDLen;
	fontFileInfo.emplace(ff->psName, ff);
	return ff;
}

sPSFontFileInfo PSOutputDev::setupType3Font(GfxFont* font, Dict* parentResDict)
{
	Dict*        resDict;
	Dict*        charProcs;
	Object       charProc;
	Gfx*         gfx;
	PDFRectangle box;
	double*      m;

	// generate name
	const auto psName = fmt::format("T3_{}_{}", font->getID()->num, font->getID()->gen);

	// set up resources used by font
	if ((resDict = ((Gfx8BitFont*)font)->getResources()))
	{
		inType3Char = true;
		setupResources(resDict);
		inType3Char = false;
	}
	else
	{
		resDict = parentResDict;
	}

	// beginning comment
	writePSFmt("%%BeginResource: font {}\n", psName);
	embFontList += "%%+ font ";
	embFontList += psName;
	embFontList += "\n";

	// font dictionary
	writePS("8 dict begin\n");
	writePS("/FontType 3 def\n");
	m = font->getFontMatrix();
	writePSFmt("/FontMatrix [{} {} {} {} {} {}] def\n", Round6(m[0]), Round6(m[1]), Round6(m[2]), Round6(m[3]), Round6(m[4]), Round6(m[5]));
	m = font->getFontBBox();
	writePSFmt("/FontBBox [{} {} {} {}] def\n", Round6(m[0]), Round6(m[1]), Round6(m[2]), Round6(m[3]));
	writePS("/Encoding 256 array def\n");
	writePS("  0 1 255 { Encoding exch /.notdef put } for\n");
	writePS("/BuildGlyph {\n");
	writePS("  exch /CharProcs get exch\n");
	writePS("  2 copy known not { pop /.notdef } if\n");
	writePS("  get exec\n");
	writePS("} bind def\n");
	writePS("/BuildChar {\n");
	writePS("  1 index /Encoding get exch get\n");
	writePS("  1 index /BuildGlyph get exec\n");
	writePS("} bind def\n");
	if ((charProcs = ((Gfx8BitFont*)font)->getCharProcs()))
	{
		writePSFmt("/CharProcs {} dict def\n", charProcs->getLength());
		writePS("CharProcs begin\n");
		box.x1      = m[0];
		box.y1      = m[1];
		box.x2      = m[2];
		box.y2      = m[3];
		gfx         = new Gfx(doc, this, resDict, &box, nullptr);
		inType3Char = true;
		for (int i = 0; i < charProcs->getLength(); ++i)
		{
			t3FillColorOnly = false;
			t3Cacheable     = false;
			t3NeedsRestore  = false;
			writePS("/");
			writePSName(charProcs->getKey(i));
			writePS(" {\n");
			gfx->display(charProcs->getValNF(i, &charProc));
			charProc.free();
			if (t3String.size())
			{
				std::string buf;
				if (t3Cacheable)
					buf = fmt::format("{} {} {} {} {} {} setcachedevice\n", Round6(t3WX), Round6(t3WY), Round6(t3LLX), Round6(t3LLY), Round6(t3URX), Round6(t3URY));
				else
					buf = fmt::format("{} {} setcharwidth\n", Round6(t3WX), Round6(t3WY));
				(*outputFunc)(outputStream, buf.c_str(), buf.size());
				(*outputFunc)(outputStream, t3String.c_str(), t3String.size());
			}
			if (t3NeedsRestore)
				(*outputFunc)(outputStream, "Q\n", 2);
			writePS("} def\n");
		}
		inType3Char = false;
		delete gfx;
		writePS("end\n");
	}
	writePS("currentdict end\n");
	writePSFmt("/{} exch definefont pop\n", psName);

	// ending comment
	writePS("%%EndResource\n");

	auto ff = std::make_shared<PSFontFileInfo>(psName, font->getType(), psFontFileEmbedded);
	fontFileInfo.emplace(ff->psName, ff);
	return ff;
}

// Make a unique PS font name, based on the names given in the PDF
// font object, and an object ID (font file object for
std::string PSOutputDev::makePSFontName(GfxFont* font, Ref* id)
{
	std::string psName;
	std::string s;

	if (s = font->getEmbeddedFontName(); s.size())
	{
		psName = filterPSName(s);
		if (!fontFileInfo.contains(psName)) return psName;
	}
	if ((s = font->getName()).size())
	{
		psName = filterPSName(s);
		if (!fontFileInfo.contains(psName)) return psName;
	}
	psName = fmt::format("FF{}_{}", id->num, id->gen);
	if ((s = font->getEmbeddedFontName()).size())
	{
		s = filterPSName(s);
		psName += '_';
		psName += s;
	}
	else if (s = font->getName(); s.size())
	{
		s = filterPSName(s);
		psName += '_';
		psName += s;
	}
	return psName;
}

std::string PSOutputDev::fixType1Font(const std::string& font, int length1, int length2)
{
	std::string out, binSection;

	const auto fontData = (uint8_t*)font.c_str();
	const int  fontSize = TO_INT(font.size());

	// check for PFB
	const bool pfb = (fontSize >= 6 && fontData[0] == 0x80 && fontData[1] == 0x01);
	if (pfb)
	{
		if (!splitType1PFB(fontData, fontSize, out, binSection))
			return copyType1PFB(fontData, fontSize);
	}
	else if (!splitType1PFA(fontData, fontSize, length1, length2, out, binSection))
	{
		return copyType1PFA(fontData, fontSize);
	}

	out += '\n';

	binSection = asciiHexDecodeType1EexecSection(binSection);

	if (!fixType1EexecSection(binSection, out))
		return pfb ? copyType1PFB(fontData, fontSize) : copyType1PFA(fontData, fontSize);

	for (int i = 0; i < 8; ++i)
		out += "0000000000000000000000000000000000000000000000000000000000000000\n";
	out += "cleartomark\n";

	return out;
}

// Split a Type 1 font in PFA format into a text section and a binary
// section.
bool PSOutputDev::splitType1PFA(uint8_t* font, int fontSize, int length1, int length2, std::string& textSection, std::string& binSection)
{
	int textLength, binStart, binLength, lastSpace, i;

	//--- extract the text section

	// Length1 is correct, and the text section ends with whitespace
	if (length1 <= fontSize && length1 >= 18 && !memcmp(font + length1 - 18, "currentfile eexec", 17))
	{
		textLength = length1 - 1;

		// Length1 is correct, but the trailing whitespace is missing
	}
	else if (length1 <= fontSize && length1 >= 17 && !memcmp(font + length1 - 17, "currentfile eexec", 17))
	{
		textLength = length1;

		// Length1 is incorrect
	}
	else
	{
		for (textLength = 17; textLength <= fontSize; ++textLength)
			if (!memcmp(font + textLength - 17, "currentfile eexec", 17))
				break;
		if (textLength > fontSize)
			return false;
	}

	textSection.append((char*)font, textLength);

	//--- skip whitespace between the text section and the binary section

	for (i = 0, binStart = textLength; i < 8 && binStart < fontSize; ++i, ++binStart)
		if (font[binStart] != ' ' && font[binStart] != '\t' && font[binStart] != '\n' && font[binStart] != '\r')
			break;
	if (i == 8)
		return false;

	//--- extract binary section

	// if we see "0000", assume Length2 is correct
	// (if Length2 is too long, it will be corrected by fixType1EexecSection)
	if (length2 > 0 && length2 < INT_MAX - 4 && binStart <= fontSize - length2 - 4 && !memcmp(font + binStart + length2, "0000", 4))
	{
		binLength = length2;
	}
	else
	{
		// look for "0000" near the end of the font (note that there can
		// be intervening "\n", "\r\n", etc.), then search backward
		if (fontSize - binStart < 512)
			return false;
		if (!memcmp(font + fontSize - 256, "0000", 4) || !memcmp(font + fontSize - 255, "0000", 4) || !memcmp(font + fontSize - 254, "0000", 4) || !memcmp(font + fontSize - 253, "0000", 4) || !memcmp(font + fontSize - 252, "0000", 4) || !memcmp(font + fontSize - 251, "0000", 4))
		{
			i         = fontSize - 252;
			lastSpace = -1;
			while (i >= binStart)
			{
				if (font[i] == ' ' || font[i] == '\t' || font[i] == '\n' || font[i] == '\r')
				{
					lastSpace = i;
					--i;
				}
				else if (font[i] == '0')
				{
					--i;
				}
				else
				{
					break;
				}
			}
			if (lastSpace < 0)
				return false;
			// check for the case where the newline/space is missing between
			// the binary section and the first set of 64 '0' chars
			if (lastSpace - binStart > 64 && !memcmp(font + lastSpace - 64, "0000000000000000000000000000000000000000000000000000000000000000", 64))
				binLength = lastSpace - 64 - binStart;
			else
				binLength = lastSpace - binStart;

			// couldn't find zeros after binary section -- assume they're
			// missing and the binary section extends to the end of the file
		}
		else
		{
			binLength = fontSize - binStart;
		}
	}

	binSection.append((char*)(font + binStart), binLength);
	return true;
}

// Split a Type 1 font in PFB format into a text section and a binary section.
bool PSOutputDev::splitType1PFB(uint8_t* font, int fontSize, std::string& textSection, std::string& binSection)
{
	uint8_t* p;
	int      state, remain, len, n;

	// states:
	// 0: text section
	// 1: binary section
	// 2: trailer section
	// 3: eof

	state  = 0;
	p      = font;
	remain = fontSize;
	while (remain >= 2)
	{
		if (p[0] != 0x80)
			return false;
		switch (state)
		{
		case 0:
			if (p[1] == 0x02)
				state = 1;
			else if (p[1] != 0x01)
				return false;
			break;
		case 1:
			if (p[1] == 0x01)
				state = 2;
			else if (p[1] != 0x02)
				return false;
			break;
		case 2:
			if (p[1] == 0x03)
				state = 3;
			else if (p[1] != 0x01)
				return false;
			break;
		default: // shouldn't happen
			return false;
		}
		if (state == 3)
			break;

		if (remain < 6)
			break;
		len = p[2] + (p[3] << 8) + (p[4] << 16) + (p[5] << 24);
		if (len < 0 || len > remain - 6)
			return false;

		switch (state)
		{
		case 0:
			textSection.append((char*)(p + 6), len);
			break;
		case 1:
			binSection.append((char*)(p + 6), len);
			break;
		case 2:
			// we don't use the trailer
			break;
		default: // shouldn't happen
			return false;
		}

		p += len + 6;
		remain -= len + 6;
	}

	if (state != 3)
		return false;

	n = TO_INT(textSection.size());
	if (n >= 18 && !memcmp(textSection.c_str() + n - 18, "currentfile eexec", 17))
	{
		// remove the trailing whitespace
		textSection.erase(n - 1, 1);
	}
	else if (n >= 17 && !memcmp(textSection.c_str() + n - 17, "currentfile eexec", 17))
	{
		// missing whitespace at end -- leave as-is
	}
	else
	{
		return false;
	}

	return true;
}

// If <in> is ASCIIHex-encoded, decode it, delete <in>, and return the
// binary version.  Else return <in> unchanged.
std::string PSOutputDev::asciiHexDecodeType1EexecSection(const std::string& in)
{
	std::string out;
	char        c;
	uint8_t     byte;
	int         state, i;

	state = 0;
	byte  = 0;
	for (i = 0; i < in.size(); ++i)
	{
		c = in.at(i);
		if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
			continue;
		if (c >= '0' && c <= '9')
			byte = (uint8_t)(byte + (c - '0'));
		else if (c >= 'A' && c <= 'F')
			byte = (uint8_t)(byte + (c - 'A' + 10));
		else if (c >= 'a' && c <= 'f')
			byte = (uint8_t)(byte + (c - 'a' + 10));
		else
			return in;
		if (state == 0)
		{
			byte  = (uint8_t)(byte << 4);
			state = 1;
		}
		else
		{
			out += (char)byte;
			state = 0;
			byte  = 0;
		}
	}
	return out;
}

bool PSOutputDev::fixType1EexecSection(std::string& binSection, std::string& out)
{
	static char hexChars[17] = "0123456789abcdef";
	uint8_t     buf[16], buf2[16];
	uint8_t     byte;
	int         r, i, j;

	// eexec-decode the binary section, keeping the last 16 bytes
	r = 55665;
	for (i = 0; i < binSection.size(); ++i)
	{
		byte        = (uint8_t)binSection.at(i);
		buf[i & 15] = byte ^ (uint8_t)(r >> 8);
		r           = ((r + byte) * 52845 + 22719) & 0xffff;
	}
	for (j = 0; j < 16; ++j)
		buf2[j] = buf[(i + j) & 15];

	// look for 'closefile'
	for (i = 0; i <= 16 - 9; ++i)
		if (!memcmp(buf2 + i, "closefile", 9))
			break;
	if (i > 16 - 9)
		return false;
	// three cases:
	// - short: missing space after "closefile" (i == 16 - 9)
	// - correct: exactly one space after "closefile" (i == 16 - 10)
	// - long: extra chars after "closefile" (i < 16 - 10)
	if (i == 16 - 9)
		binSection += (char)((uint8_t)'\n' ^ (uint8_t)(r >> 8));
	else if (i < 16 - 10)
		binSection.erase(binSection.size() - (16 - 10 - i), 16 - 10 - i);

	// ASCIIHex encode
	for (i = 0; i < binSection.size(); i += 32)
	{
		for (j = 0; j < 32 && i + j < binSection.size(); ++j)
		{
			byte = (uint8_t)binSection.at(i + j);
			out += hexChars[(byte >> 4) & 0x0f];
			out += hexChars[byte & 0x0f];
		}
		out += '\n';
	}

	return true;
}

// The Type 1 cleanup code failed -- assume it's a valid PFA-format
// font and copy it to the output.
std::string PSOutputDev::copyType1PFA(uint8_t* font, int fontSize)
{
	error(errSyntaxWarning, -1, "Couldn't parse embedded Type 1 font");

	std::string out((char*)font, fontSize);
	// append a newline to avoid problems where the original font
	// doesn't end with one
	out += '\n';
	return out;
}

// The Type 1 cleanup code failed -- assume it's a valid PFB-format
// font, decode the PFB blocks, and copy them to the output.
std::string PSOutputDev::copyType1PFB(uint8_t* font, int fontSize)
{
	static char hexChars[17] = "0123456789abcdef";
	int         remain, len, i, j;

	error(errSyntaxWarning, -1, "Couldn't parse embedded Type 1 (PFB) font");

	std::string out;
	uint8_t*    p = font;
	remain        = fontSize;
	while (remain >= 6 && p[0] == 0x80 && (p[1] == 0x01 || p[1] == 0x02))
	{
		len = p[2] + (p[3] << 8) + (p[4] << 16) + (p[5] << 24);
		if (len < 0 || len > remain - 6)
			break;
		if (p[1] == 0x01)
		{
			out.append((char*)(p + 6), len);
		}
		else
		{
			for (i = 0; i < len; i += 32)
			{
				for (j = 0; j < 32 && i + j < len; ++j)
				{
					out += hexChars[(p[6 + i + j] >> 4) & 0x0f];
					out += hexChars[p[6 + i + j] & 0x0f];
				}
				out += '\n';
			}
		}
		p += len + 6;
		remain -= len + 6;
	}
	// append a newline to avoid problems where the original font
	// doesn't end with one
	out += '\n';
	return out;
}

void PSOutputDev::renameType1Font(std::string& font, const std::string& name)
{
	const char *p1, *p2;
	int         i;

	if (!(p1 = strstr(font.c_str(), "\n/FontName")) && !(p1 = strstr(font.c_str(), "\r/FontName")))
		return;
	p1 += 10;
	while (*p1 == ' ' || *p1 == '\t' || *p1 == '\n' || *p1 == '\r')
		++p1;
	if (*p1 != '/')
		return;
	++p1;
	p2 = p1;
	while (*p2 && *p2 != ' ' && *p2 != '\t' && *p2 != '\n' && *p2 != '\r')
		++p2;
	if (!*p2)
		return;
	i = (int)(p1 - font.c_str());
	font.erase(i, (int)(p2 - p1));
	font.insert(i, name);
}

void PSOutputDev::setupDefaultFont()
{
	writePS("/xpdf_default_font /Helvetica 1 1 ISOLatin1Encoding pdfMakeFont\n");
}

void PSOutputDev::setupImages(Dict* resDict)
{
	Object xObjDict, xObj, xObjRef, subtypeObj, maskObj, maskRef;
	Ref    imgID;
	int    i, j;

	if (!(mode == psModeForm || inType3Char || preload))
		return;

	resDict->lookup("XObject", &xObjDict);
	if (xObjDict.isDict())
	{
		for (i = 0; i < xObjDict.dictGetLength(); ++i)
		{
			xObjDict.dictGetValNF(i, &xObjRef);
			xObjDict.dictGetVal(i, &xObj);
			if (xObj.isStream())
			{
				xObj.streamGetDict()->lookup("Subtype", &subtypeObj);
				if (subtypeObj.isName("Image"))
				{
					if (xObjRef.isRef())
					{
						imgID = xObjRef.getRef();
						for (j = 0; j < imgIDLen; ++j)
							if (imgIDs[j].num == imgID.num && imgIDs[j].gen == imgID.gen)
								break;
						if (j == imgIDLen)
						{
							if (imgIDLen >= imgIDSize)
							{
								if (imgIDSize == 0)
									imgIDSize = 64;
								else
									imgIDSize *= 2;
								imgIDs = (Ref*)greallocn(imgIDs, imgIDSize, sizeof(Ref));
							}
							imgIDs[imgIDLen++] = imgID;
							setupImage(imgID, xObj.getStream(), false, nullptr);
							if (level >= psLevel3)
							{
								xObj.streamGetDict()->lookup("Mask", &maskObj);
								if (maskObj.isStream())
									setupImage(imgID, maskObj.getStream(), true, nullptr);
								else if (level == psLevel3Gray && maskObj.isArray())
									setupImage(imgID, xObj.getStream(), false, maskObj.getArray());
								maskObj.free();
							}
						}
					}
					else
					{
						error(errSyntaxError, -1, "Image in resource dict is not an indirect reference");
					}
				}
				subtypeObj.free();
			}
			xObj.free();
			xObjRef.free();
		}
	}
	xObjDict.free();
}

void PSOutputDev::setupImage(Ref id, Stream* str, bool mask, Array* colorKeyMask)
{
	StreamColorSpaceMode csMode;
	GfxColorSpace*       colorSpace;
	GfxImageColorMap*    colorMap;
	int                  maskColors[2 * gfxColorMaxComps];
	Object               obj1;
	bool                 imageMask, useLZW, useRLE, useCompressed, useASCIIHex;
	std::string          s;
	int                  c, width, height, bits, size, line, col, i;

	// check for mask
	str->getDict()->lookup("ImageMask", &obj1);
	if (obj1.isBool())
		imageMask = obj1.getBool();
	else
		imageMask = false;
	obj1.free();

	// get image size
	str->getDict()->lookup("Width", &obj1);
	if (!obj1.isInt() || obj1.getInt() <= 0)
	{
		error(errSyntaxError, -1, "Invalid Width in image");
		obj1.free();
		return;
	}
	width = obj1.getInt();
	obj1.free();
	str->getDict()->lookup("Height", &obj1);
	if (!obj1.isInt() || obj1.getInt() <= 0)
	{
		error(errSyntaxError, -1, "Invalid Height in image");
		obj1.free();
		return;
	}
	height = obj1.getInt();
	obj1.free();

	// build the color map
	if (mask || imageMask)
	{
		colorMap = nullptr;
	}
	else
	{
		bits   = 0;
		csMode = streamCSNone;
		str->getImageParams(&bits, &csMode);
		if (bits == 0)
		{
			str->getDict()->lookup("BitsPerComponent", &obj1);
			if (!obj1.isInt())
			{
				error(errSyntaxError, -1, "Invalid BitsPerComponent in image");
				obj1.free();
				return;
			}
			bits = obj1.getInt();
			obj1.free();
		}
		str->getDict()->lookup("ColorSpace", &obj1);
		if (!obj1.isNull())
			colorSpace = GfxColorSpace::parse(&obj1);
		else if (csMode == streamCSDeviceGray)
			colorSpace = GfxColorSpace::create(csDeviceGray);
		else if (csMode == streamCSDeviceRGB)
			colorSpace = GfxColorSpace::create(csDeviceRGB);
		else if (csMode == streamCSDeviceCMYK)
			colorSpace = GfxColorSpace::create(csDeviceCMYK);
		else
			colorSpace = nullptr;
		obj1.free();
		if (!colorSpace)
		{
			error(errSyntaxError, -1, "Invalid ColorSpace in image");
			return;
		}
		str->getDict()->lookup("Decode", &obj1);
		colorMap = new GfxImageColorMap(bits, &obj1, colorSpace);
		obj1.free();
	}

	// filters
	str->disableDecompressionBombChecking();
	if (level < psLevel2)
	{
		useLZW = useRLE = false;
		useCompressed   = false;
		useASCIIHex     = true;
	}
	else
	{
		if (colorKeyMask)
		{
			if (globalParams->getPSUncompressPreloadedImages())
			{
				useLZW = useRLE = false;
			}
			else if (globalParams->getPSLZW())
			{
				useLZW = true;
				useRLE = false;
			}
			else
			{
				useRLE = true;
				useLZW = false;
			}
			useCompressed = false;
		}
		else if (colorMap && (colorMap->getColorSpace()->getMode() == csDeviceN || level == psLevel2Gray || level == psLevel3Gray))
		{
			if (globalParams->getPSLZW())
			{
				useLZW = true;
				useRLE = false;
			}
			else
			{
				useRLE = true;
				useLZW = false;
			}
			useCompressed = false;
		}
		else if (globalParams->getPSUncompressPreloadedImages())
		{
			useLZW = useRLE = false;
			useCompressed   = false;
		}
		else
		{
			s = str->getPSFilter(level < psLevel3 ? 2 : 3, "", true);
			if (s.size())
			{
				if (globalParams->getPSLZW() && !str->hasStrongCompression())
				{
					useRLE        = false;
					useLZW        = true;
					useCompressed = false;
				}
				else
				{
					useLZW = useRLE = false;
					useCompressed   = true;
				}
			}
			else
			{
				if (globalParams->getPSLZW())
				{
					useLZW = true;
					useRLE = false;
				}
				else
				{
					useRLE = true;
					useLZW = false;
				}
				useCompressed = false;
			}
		}
		useASCIIHex = globalParams->getPSASCIIHex();
	}
	if (useCompressed)
		str = str->getUndecodedStream();
	if (colorKeyMask)
	{
		memset(maskColors, 0, sizeof(maskColors));
		for (int i = 0; i < colorKeyMask->getLength() && i < 2 * gfxColorMaxComps; ++i)
		{
			colorKeyMask->get(i, &obj1);
			if (obj1.isInt())
				maskColors[i] = obj1.getInt();
			obj1.free();
		}
		str = new ColorKeyToMaskEncoder(str, width, height, colorMap, maskColors);
	}
	else if (colorMap && (level == psLevel2Gray || level == psLevel3Gray))
	{
		str = new GrayRecoder(str, width, height, colorMap);
	}
	else if (colorMap && colorMap->getColorSpace()->getMode() == csDeviceN)
	{
		str = new DeviceNRecoder(str, width, height, colorMap);
	}
	if (useLZW)
		str = new LZWEncoder(str);
	else if (useRLE)
		str = new RunLengthEncoder(str);
	if (useASCIIHex)
		str = new ASCIIHexEncoder(str);
	else
		str = new ASCII85Encoder(str);

	// compute image data size
	str->reset();
	col = size = 0;
	do
	{
		do
		{
			c = str->getChar();
		}
		while (c == '\n' || c == '\r');
		if (c == (useASCIIHex ? '>' : '~') || c == EOF)
			break;
		if (c == 'z')
		{
			++col;
		}
		else
		{
			++col;
			for (i = 1; i <= (useASCIIHex ? 1 : 4); ++i)
			{
				do
				{
					c = str->getChar();
				}
				while (c == '\n' || c == '\r');
				if (c == (useASCIIHex ? '>' : '~') || c == EOF)
					break;
				++col;
			}
		}
		if (col > 225)
		{
			++size;
			col = 0;
		}
	}
	while (c != (useASCIIHex ? '>' : '~') && c != EOF);
	// add one entry for the final line of data; add another entry
	// because the LZWDecode/RunLengthDecode filter may read past the end
	++size;
	if (useLZW || useRLE)
		++size;
	writePSFmt("{} array dup /{}Data_{}_{} exch def\n", size, (mask || colorKeyMask) ? "Mask" : "Im", id.num, id.gen);
	str->close();

	// write the data into the array
	str->reset();
	line = col = 0;
	writePS((char*)(useASCIIHex ? "dup 0 <" : "dup 0 <~"));
	do
	{
		do
		{
			c = str->getChar();
		}
		while (c == '\n' || c == '\r');
		if (c == (useASCIIHex ? '>' : '~') || c == EOF)
			break;
		if (c == 'z')
		{
			writePSChar((char)c);
			++col;
		}
		else
		{
			writePSChar((char)c);
			++col;
			for (i = 1; i <= (useASCIIHex ? 1 : 4); ++i)
			{
				do
				{
					c = str->getChar();
				}
				while (c == '\n' || c == '\r');
				if (c == (useASCIIHex ? '>' : '~') || c == EOF)
					break;
				writePSChar((char)c);
				++col;
			}
		}
		// each line is: "dup nnnnn <~...data...~> put<eol>"
		// so max data length = 255 - 20 = 235
		// chunks are 1 or 4 bytes each, so we have to stop at 232
		// but make it 225 just to be safe
		if (col > 225)
		{
			writePS((char*)(useASCIIHex ? "> put\n" : "~> put\n"));
			++line;
			if (useASCIIHex)
				writePSFmt("dup {} <", line);
			else
				writePSFmt("dup {} <~", line);
			col = 0;
		}
	}
	while (c != (useASCIIHex ? '>' : '~') && c != EOF);
	writePS((char*)(useASCIIHex ? "> put\n" : "~> put\n"));
	if (useLZW || useRLE)
	{
		++line;
		writePSFmt("{} <> put\n", line);
	}
	else
	{
		writePS("pop\n");
	}
	str->close();

	delete str;

	if (colorMap)
		delete colorMap;
}

void PSOutputDev::setupForms(Dict* resDict)
{
	Object xObjDict, xObj, xObjRef, subtypeObj;
	int    i;

	if (!preload)
		return;

	resDict->lookup("XObject", &xObjDict);
	if (xObjDict.isDict())
	{
		for (i = 0; i < xObjDict.dictGetLength(); ++i)
		{
			xObjDict.dictGetValNF(i, &xObjRef);
			xObjDict.dictGetVal(i, &xObj);
			if (xObj.isStream())
			{
				xObj.streamGetDict()->lookup("Subtype", &subtypeObj);
				if (subtypeObj.isName("Form"))
				{
					if (xObjRef.isRef())
						setupForm(&xObjRef, &xObj);
					else
						error(errSyntaxError, -1, "Form in resource dict is not an indirect reference");
				}
				subtypeObj.free();
			}
			xObj.free();
			xObjRef.free();
		}
	}
	xObjDict.free();
}

void PSOutputDev::setupForm(Object* strRef, Object* strObj)
{
	Dict *       dict, *resDict;
	Object       matrixObj, bboxObj, resObj, obj1;
	double       m[6], bbox[4];
	PDFRectangle box;
	Gfx*         gfx;
	int          i;

	// check if form is already defined
	for (i = 0; i < formIDLen; ++i)
		if (formIDs[i].num == strRef->getRefNum() && formIDs[i].gen == strRef->getRefGen())
			return;

	// add entry to formIDs list
	if (formIDLen >= formIDSize)
	{
		if (formIDSize == 0)
			formIDSize = 64;
		else
			formIDSize *= 2;
		formIDs = (Ref*)greallocn(formIDs, formIDSize, sizeof(Ref));
	}
	formIDs[formIDLen++] = strRef->getRef();

	dict = strObj->streamGetDict();

	// get bounding box
	dict->lookup("BBox", &bboxObj);
	if (!bboxObj.isArray())
	{
		bboxObj.free();
		error(errSyntaxError, -1, "Bad form bounding box");
		return;
	}
	for (i = 0; i < 4; ++i)
	{
		bboxObj.arrayGet(i, &obj1);
		bbox[i] = obj1.getNum();
		obj1.free();
	}
	bboxObj.free();

	// get matrix
	dict->lookup("Matrix", &matrixObj);
	if (matrixObj.isArray())
	{
		for (i = 0; i < 6; ++i)
		{
			matrixObj.arrayGet(i, &obj1);
			m[i] = obj1.getNum();
			obj1.free();
		}
	}
	else
	{
		m[0] = 1;
		m[1] = 0;
		m[2] = 0;
		m[3] = 1;
		m[4] = 0;
		m[5] = 0;
	}
	matrixObj.free();

	// get resources
	dict->lookup("Resources", &resObj);
	resDict = resObj.isDict() ? resObj.getDict() : (Dict*)nullptr;

	writePSFmt("/f_{}_{} {{\n", strRef->getRefNum(), strRef->getRefGen());
	writePS("q\n");
	writePSFmt("[{} {} {} {} {} {}] cm\n", Round6(m[0]), Round6(m[1]), Round6(m[2]), Round6(m[3]), Round6(m[4]), Round6(m[5]));

	box.x1 = bbox[0];
	box.y1 = bbox[1];
	box.x2 = bbox[2];
	box.y2 = bbox[3];
	gfx    = new Gfx(doc, this, resDict, &box, &box);
	gfx->display(strRef);
	delete gfx;

	writePS("Q\n");
	writePS("} def\n");

	resObj.free();
}

bool PSOutputDev::checkPageSlice(Page* page, double hDPI, double vDPI, int rotateA, bool useMediaBox, bool crop, int sliceX, int sliceY, int sliceW, int sliceH, bool printing, bool (*abortCheckCbk)(void* data), void* abortCheckCbkData)
{
#if HAVE_SPLASH
	bool             mono;
	bool             useLZW;
	double           dpi;
	SplashOutputDev* splashOut;
	SplashColor      paperColor;
	PDFRectangle     box;
	GfxState*        state;
	SplashBitmap*    bitmap;
	Stream *         str0, *str;
	Object           obj;
	uint8_t*         p;
	char             buf[4096];
	double           userUnit, hDPI2, vDPI2;
	double           m0, m1, m2, m3, m4, m5;
	int              nStripes, stripeH, stripeY;
	int              w, h, x, y, i, n;
#endif

	const int pg = page->getNum();
	if (!(pg >= firstPage && pg <= lastPage && rasterizePage[pg - firstPage]))
		return true;

#if HAVE_SPLASH
	// get the rasterization parameters
	dpi    = globalParams->getPSRasterResolution();
	mono   = globalParams->getPSRasterMono() || level == psLevel1 || level == psLevel2Gray || level == psLevel3Gray;
	useLZW = globalParams->getPSLZW();

	// get the UserUnit
	if (honorUserUnit)
		userUnit = page->getUserUnit();
	else
		userUnit = 1;

	// start the PS page
	page->makeBox(userUnit * dpi, userUnit * dpi, rotateA, useMediaBox, false, sliceX, sliceY, sliceW, sliceH, &box, &crop);
	rotateA += page->getRotate();
	if (rotateA >= 360)
		rotateA -= 360;
	else if (rotateA < 0)
		rotateA += 360;
	state = new GfxState(dpi, dpi, &box, rotateA, false);
	startPage(page->getNum(), state);
	delete state;

	// set up the SplashOutputDev
	if (mono)
	{
		paperColor[0] = 0xff;
		splashOut     = new SplashOutputDev(splashModeMono8, 1, false, paperColor, false, globalParams->getAntialiasPrinting());
#	if SPLASH_CMYK
	}
	else if (level == psLevel1Sep)
	{
		paperColor[0] = paperColor[1] = paperColor[2] = paperColor[3] = 0;
		splashOut                                                     = new SplashOutputDev(splashModeCMYK8, 1, false, paperColor, false, globalParams->getAntialiasPrinting());
#	endif
	}
	else
	{
		paperColor[0] = paperColor[1] = paperColor[2] = 0xff;
		splashOut                                     = new SplashOutputDev(splashModeRGB8, 1, false, paperColor, false, globalParams->getAntialiasPrinting());
	}
	splashOut->startDoc(xref);

	// break the page into stripes
	// NB: startPage() has already multiplied xScale and yScale by UserUnit
	hDPI2 = xScale * dpi;
	vDPI2 = yScale * dpi;
	if (sliceW < 0 || sliceH < 0)
	{
		if (useMediaBox)
			box = *page->getMediaBox();
		else
			box = *page->getCropBox();
		sliceX = sliceY = 0;
		sliceW          = (int)((box.x2 - box.x1) * hDPI2 / 72.0);
		if (sliceW == 0)
			sliceW = 1;
		sliceH = (int)((box.y2 - box.y1) * vDPI2 / 72.0);
		if (sliceH == 0)
			sliceH = 1;
	}
	nStripes = (int)ceil(((double)sliceW * (double)sliceH) / (double)globalParams->getPSRasterSliceSize());
	stripeH  = (sliceH + nStripes - 1) / nStripes;

	// render the stripes
	for (stripeY = sliceY; stripeY < sliceH; stripeY += stripeH)
	{
		// rasterize a stripe
		page->makeBox(hDPI2, vDPI2, 0, useMediaBox, false, sliceX, stripeY, sliceW, stripeH, &box, &crop);
		m0 = box.x2 - box.x1;
		m1 = 0;
		m2 = 0;
		m3 = box.y2 - box.y1;
		m4 = box.x1;
		m5 = box.y1;
		page->displaySlice(splashOut, hDPI2, vDPI2, (360 - page->getRotate()) % 360, useMediaBox, crop, sliceX, stripeY, sliceW, stripeH, printing, abortCheckCbk, abortCheckCbkData);

		// draw the rasterized image
		bitmap = splashOut->getBitmap();
		w      = bitmap->getWidth();
		h      = bitmap->getHeight();
		writePS("gsave\n");
		writePSFmt("[{} {} {} {} {} {}] concat\n", Round6(m0), Round6(m1), Round6(m2), Round6(m3), Round6(m4), Round6(m5));
		switch (level)
		{
		case psLevel1:
			writePSFmt("{} {} 8 [{} 0 0 {} 0 {}] pdfIm1\n", w, h, w, -h, h);
			p = bitmap->getDataPtr() + (h - 1) * bitmap->getRowSize();
			i = 0;
			for (y = 0; y < h; ++y)
			{
				for (x = 0; x < w; ++x)
				{
					writePSFmt("{:02x}", *p++);
					if (++i == 32)
					{
						writePSChar('\n');
						i = 0;
					}
				}
			}
			if (i != 0)
				writePSChar('\n');
			break;
		case psLevel1Sep:
		{
#	if SPLASH_CMYK
			writePSFmt("{} {} 8 [{} 0 0 {} 0 {}] pdfIm1Sep\n", w, h, w, -h, h);
			p              = bitmap->getDataPtr() + (h - 1) * bitmap->getRowSize();
			i              = 0;
			uint8_t col[4] = {};
			for (y = 0; y < h; ++y)
			{
				for (comp = 0; comp < 4; ++comp)
				{
					for (x = 0; x < w; ++x)
					{
						writePSFmt("{:02x}", p[4 * x + comp]);
						col[comp] |= p[4 * x + comp];
						if (++i == 32)
						{
							writePSChar('\n');
							i = 0;
						}
					}
				}
				p -= bitmap->getRowSize();
			}
			if (i != 0)
				writePSChar('\n');
			if (col[0]) processColors |= psProcessCyan;
			if (col[1]) processColors |= psProcessMagenta;
			if (col[2]) processColors |= psProcessYellow;
			if (col[3]) processColors |= psProcessBlack;
				// if !SPLASH_CMYK: fall through
#	endif
			break;
		}
		case psLevel2:
		case psLevel2Gray:
		case psLevel2Sep:
		case psLevel3:
		case psLevel3Gray:
		case psLevel3Sep:
			if (mono)
				writePS("/DeviceGray setcolorspace\n");
			else
				writePS("/DeviceRGB setcolorspace\n");
			writePS("<<\n  /ImageType 1\n");
			writePSFmt("  /Width {}\n", bitmap->getWidth());
			writePSFmt("  /Height {}\n", bitmap->getHeight());
			writePSFmt("  /ImageMatrix [{} 0 0 {} 0 {}]\n", w, -h, h);
			writePS("  /BitsPerComponent 8\n");
			if (mono)
				writePS("  /Decode [0 1]\n");
			else
				writePS("  /Decode [0 1 0 1 0 1]\n");
			writePS("  /DataSource currentfile\n");
			if (globalParams->getPSASCIIHex())
				writePS("    /ASCIIHexDecode filter\n");
			else
				writePS("    /ASCII85Decode filter\n");
			if (useLZW)
				writePS("    /LZWDecode filter\n");
			else
				writePS("    /RunLengthDecode filter\n");
			writePS(">>\n");
			writePS("image\n");
			obj.initNull();
			p    = bitmap->getDataPtr() + (h - 1) * bitmap->getRowSize();
			str0 = new MemStream((char*)p, 0, w * h * (mono ? 1 : 3), &obj);
			if (useLZW)
				str = new LZWEncoder(str0);
			else
				str = new RunLengthEncoder(str0);
			if (globalParams->getPSASCIIHex())
				str = new ASCIIHexEncoder(str);
			else
				str = new ASCII85Encoder(str);
			str->reset();
			while ((n = str->getBlock(buf, sizeof(buf))) > 0)
				writePSBlock(buf, n);
			str->close();
			delete str;
			delete str0;
			writePSChar('\n');
			processColors |= mono ? psProcessBlack : psProcessCMYK;
			break;
		}
		writePS("grestore\n");
	}

	delete splashOut;

	// finish the PS page
	endPage();

	return false;

#else  // HAVE_SPLASH

	error(errSyntaxWarning, -1, "PDF page uses transparency and PSOutputDev was built without the Splash rasterizer - output may not be correct");
	return true;
#endif // HAVE_SPLASH
}

void PSOutputDev::startPage(int pageNum, GfxState* state)
{
	Page*  page;
	double userUnit;
	int    x1, y1, x2, y2, width, height, t;
	int    imgWidth, imgHeight, imgWidth2, imgHeight2;
	bool   landscape;

	page = doc->getCatalog()->getPage(pageNum);
	if (honorUserUnit)
		userUnit = page->getUserUnit();
	else
		userUnit = 1;

	if (mode == psModePS)
	{
		writePSFmt("%%Page: {} {}\n", pageNum, seqPage);
		if (paperMatch)
		{
			imgLLX = imgLLY = 0;
			if (globalParams->getPSUseCropBoxAsPage())
			{
				imgURX = (int)ceil(page->getCropWidth() * userUnit);
				imgURY = (int)ceil(page->getCropHeight() * userUnit);
			}
			else
			{
				imgURX = (int)ceil(page->getMediaWidth() * userUnit);
				imgURY = (int)ceil(page->getMediaHeight() * userUnit);
			}
			if (state->getRotate() == 90 || state->getRotate() == 270)
			{
				t      = imgURX;
				imgURX = imgURY;
				imgURY = t;
			}
			writePSFmt("%%PageMedia: {}x{}\n", imgURX, imgURY);
			writePSFmt("%%PageBoundingBox: 0 0 {} {}\n", imgURX, imgURY);
		}
		writePS("%%BeginPageSetup\n");
	}
	if (mode != psModeForm)
		writePS("xpdf begin\n");

	// set up paper size for paper=match mode
	// NB: this must be done *before* the saveState() for overlays.
	if (mode == psModePS && paperMatch)
		writePSFmt("{} {} pdfSetupPaper\n", imgURX, imgURY);

	// underlays
	if (underlayCbk)
		(*underlayCbk)(this, underlayCbkData);
	if (overlayCbk)
		saveState(nullptr);

	switch (mode)
	{
	case psModePS:
		// rotate, translate, and scale page
		imgWidth  = imgURX - imgLLX;
		imgHeight = imgURY - imgLLY;
		x1        = (int)floor(state->getX1());
		y1        = (int)floor(state->getY1());
		x2        = (int)ceil(state->getX2());
		y2        = (int)ceil(state->getY2());
		width     = x2 - x1;
		height    = y2 - y1;
		tx = ty = 0;
		// rotation and portrait/landscape mode
		if (paperMatch)
		{
			rotate    = (360 - state->getRotate()) % 360;
			landscape = false;
		}
		else if (rotate0 >= 0)
		{
			rotate    = (360 - rotate0) % 360;
			landscape = false;
		}
		else
		{
			rotate              = (360 - state->getRotate()) % 360;
			double scaledWidth  = width * userUnit;
			double scaledHeight = height * userUnit;
			if (xScale0 > 0 && yScale0 > 0)
			{
				scaledWidth *= xScale0;
				scaledHeight *= yScale0;
			}
			if (rotate == 0 || rotate == 180)
			{
				if ((scaledWidth < scaledHeight && imgWidth > imgHeight && scaledHeight > imgHeight) || (scaledWidth > scaledHeight && imgWidth < imgHeight && scaledWidth > imgWidth))
				{
					rotate += 90;
					landscape = true;
				}
				else
				{
					landscape = false;
				}
			}
			else
			{ // rotate == 90 || rotate == 270
				if ((scaledHeight < scaledWidth && imgWidth > imgHeight && scaledWidth > imgHeight) || (scaledHeight > scaledWidth && imgWidth < imgHeight && scaledHeight > imgWidth))
				{
					rotate    = 270 - rotate;
					landscape = true;
				}
				else
				{
					landscape = false;
				}
			}
		}
		writePSFmt("%%PageOrientation: {}\n", landscape ? "Landscape" : "Portrait");
		writePS("pdfStartPage\n");
		if (rotate == 0)
		{
			imgWidth2  = imgWidth;
			imgHeight2 = imgHeight;
		}
		else if (rotate == 90)
		{
			writePS("90 rotate\n");
			ty         = -imgWidth;
			imgWidth2  = imgHeight;
			imgHeight2 = imgWidth;
		}
		else if (rotate == 180)
		{
			writePS("180 rotate\n");
			imgWidth2  = imgWidth;
			imgHeight2 = imgHeight;
			tx         = -imgWidth;
			ty         = -imgHeight;
		}
		else
		{ // rotate == 270
			writePS("270 rotate\n");
			tx         = -imgHeight;
			imgWidth2  = imgHeight;
			imgHeight2 = imgWidth;
		}
		// shrink or expand
		if (xScale0 > 0 && yScale0 > 0)
		{
			xScale = xScale0 * userUnit;
			yScale = yScale0 * userUnit;
		}
		else if ((globalParams->getPSShrinkLarger() && (width * userUnit > imgWidth2 || height * userUnit > imgHeight2)) || (expandSmallPages && (width * userUnit < imgWidth2 && height * userUnit < imgHeight2)))
		{
			xScale = (double)imgWidth2 / (double)width;
			yScale = (double)imgHeight2 / (double)height;
			if (yScale < xScale)
				xScale = yScale;
			else
				yScale = xScale;
		}
		else
		{
			xScale = yScale = userUnit;
		}
		// deal with odd bounding boxes or clipping
		if (clipLLX0 < clipURX0 && clipLLY0 < clipURY0)
		{
			tx -= xScale * clipLLX0;
			ty -= yScale * clipLLY0;
		}
		else
		{
			tx -= xScale * x1;
			ty -= yScale * y1;
		}
		// offset or center
		if (tx0 >= 0 && ty0 >= 0)
		{
			if (rotate == 0)
			{
				tx += tx0;
				ty += ty0;
			}
			else if (rotate == 90)
			{
				tx += ty0;
				ty += (imgWidth - yScale * height) - tx0;
			}
			else if (rotate == 180)
			{
				tx += (imgWidth - xScale * width) - tx0;
				ty += (imgHeight - yScale * height) - ty0;
			}
			else
			{ // rotate == 270
				tx += (imgHeight - xScale * width) - ty0;
				ty += tx0;
			}
		}
		else if (globalParams->getPSCenter())
		{
			if (clipLLX0 < clipURX0 && clipLLY0 < clipURY0)
			{
				tx += (imgWidth2 - xScale * (clipURX0 - clipLLX0)) / 2;
				ty += (imgHeight2 - yScale * (clipURY0 - clipLLY0)) / 2;
			}
			else
			{
				tx += (imgWidth2 - xScale * width) / 2;
				ty += (imgHeight2 - yScale * height) / 2;
			}
		}
		tx += (rotate == 0 || rotate == 180) ? imgLLX : imgLLY;
		ty += (rotate == 0 || rotate == 180) ? imgLLY : -imgLLX;
		if (tx != 0 || ty != 0)
			writePSFmt("{} {} translate\n", Round6(tx), Round6(ty));
		if (xScale != 1 || yScale != 1)
			writePSFmt("{:.4f} {:.4f} scale\n", xScale, yScale);
		if (clipLLX0 < clipURX0 && clipLLY0 < clipURY0)
			writePSFmt("{} {} {} {} re W\n", Round6(clipLLX0), Round6(clipLLY0), Round6(clipURX0 - clipLLX0), Round6(clipURY0 - clipLLY0));
		else
			writePSFmt("{} {} {} {} re W\n", x1, y1, x2 - x1, y2 - y1);

		++seqPage;
		break;

	case psModeEPS:
		writePS("pdfStartPage\n");
		tx = ty = 0;
		rotate  = (360 - state->getRotate()) % 360;
		if (rotate == 0)
		{
		}
		else if (rotate == 90)
		{
			writePS("90 rotate\n");
			tx = -epsX1;
			ty = -epsY2;
		}
		else if (rotate == 180)
		{
			writePS("180 rotate\n");
			tx = -(epsX1 + epsX2);
			ty = -(epsY1 + epsY2);
		}
		else
		{ // rotate == 270
			writePS("270 rotate\n");
			tx = -epsX2;
			ty = -epsY1;
		}
		if (tx != 0 || ty != 0)
			writePSFmt("{} {} translate\n", Round6(tx), Round6(ty));
		xScale = yScale = 1;
		break;

	case psModeForm:
		writePS("/PaintProc {\n");
		writePS("begin xpdf begin\n");
		writePS("pdfStartPage\n");
		tx = ty = 0;
		xScale = yScale = 1;
		rotate          = 0;
		break;
	}

	if (level == psLevel2Gray || level == psLevel3Gray)
		writePS("/DeviceGray setcolorspace\n");

	if (customCodeCbk)
	{
		std::string s = (*customCodeCbk)(this, psOutCustomPageSetup, pageNum, customCodeCbkData);
		if (s.size())
			writePS(s.c_str());
	}

	if (mode == psModePS)
		writePS("%%EndPageSetup\n");

	noStateChanges = false;
}

void PSOutputDev::endPage()
{
	if (overlayCbk)
	{
		restoreState(nullptr);
		(*overlayCbk)(this, overlayCbkData);
	}

	if (mode == psModeForm)
	{
		writePS("pdfEndPage\n");
		writePS("end end\n");
		writePS("} def\n");
		writePS("end end\n");
	}
	else
	{
		if (!manualCtrl)
			writePS("showpage\n");
		writePS("%%PageTrailer\n");
		writePageTrailer();
		writePS("end\n");
	}
}

void PSOutputDev::saveState(GfxState* state)
{
	// The noStateChanges and saveStack fields are used to implement an
	// optimization to reduce gsave/grestore nesting.  The idea is to
	// look for sequences like this:
	//   q  q AAA Q BBB Q     (where AAA and BBB are sequences of operations)
	// and transform them to:
	//   q AAA Q q BBB Q
	if (noStateChanges)
	{
		// any non-nullptr pointer will work here
		saveStack->append(this);
	}
	else
	{
		saveStack->append((PSOutputDev*)nullptr);
		writePS("q\n");
		noStateChanges = true;
	}
}

void PSOutputDev::restoreState(GfxState* state)
{
	if (saveStack->getLength())
	{
		writePS("Q\n");
		if (saveStack->del(saveStack->getLength() - 1))
		{
			writePS("q\n");
			noStateChanges = true;
		}
		else
		{
			noStateChanges = false;
		}
	}
}

void PSOutputDev::updateCTM(GfxState* state, double m11, double m12, double m21, double m22, double m31, double m32)
{
	if (m11 == 1 && m12 == 0 && m21 == 0 && m22 == 1 && m31 == 0 && m32 == 0)
		return;
	if (fabs(m11 * m22 - m12 * m21) < 1e-10)
	{
		// avoid a singular (or close-to-singular) matrix
		writePSFmt("[0.00001 0 0 0.00001 {} {}] cm\n", Round6(m31), Round6(m32));
	}
	else
	{
		writePSFmt("[{} {} {} {} {} {}] cm\n", Round6(m11), Round6(m12), Round6(m21), Round6(m22), Round6(m31), Round6(m32));
	}
	noStateChanges = false;
}

void PSOutputDev::updateLineDash(GfxState* state)
{
	double* dash;
	double  start;
	int     length, i;

	state->getLineDash(&dash, &length, &start);
	writePS("[");
	for (i = 0; i < length; ++i)
		writePSFmt("{}{}", Round6(dash[i] < 0 ? 0 : dash[i]), (i == length - 1) ? "" : " ");
	writePSFmt("] {} d\n", Round6(start));
	noStateChanges = false;
}

void PSOutputDev::updateFlatness(GfxState* state)
{
	writePSFmt("{} i\n", Round4(state->getFlatness()));
	noStateChanges = false;
}

void PSOutputDev::updateLineJoin(GfxState* state)
{
	writePSFmt("{} j\n", state->getLineJoin());
	noStateChanges = false;
}

void PSOutputDev::updateLineCap(GfxState* state)
{
	writePSFmt("{} J\n", state->getLineCap());
	noStateChanges = false;
}

void PSOutputDev::updateMiterLimit(GfxState* state)
{
	writePSFmt("{} M\n", Round4(state->getMiterLimit()));
	noStateChanges = false;
}

void PSOutputDev::updateLineWidth(GfxState* state)
{
	writePSFmt("{} w\n", Round6(state->getLineWidth()));
	noStateChanges = false;
}

void PSOutputDev::updateFillColorSpace(GfxState* state)
{
	switch (level)
	{
	case psLevel1:
	case psLevel1Sep:
		break;
	case psLevel2:
	case psLevel3:
		if (state->getFillColorSpace()->getMode() != csPattern)
		{
			dumpColorSpaceL2(state, state->getFillColorSpace(), true, false, false);
			writePS(" cs\n");
			noStateChanges = false;
		}
		break;
	case psLevel2Gray:
	case psLevel3Gray:
	case psLevel2Sep:
	case psLevel3Sep:
		break;
	}
}

void PSOutputDev::updateStrokeColorSpace(GfxState* state)
{
	switch (level)
	{
	case psLevel1:
	case psLevel1Sep:
		break;
	case psLevel2:
	case psLevel3:
		if (state->getStrokeColorSpace()->getMode() != csPattern)
		{
			dumpColorSpaceL2(state, state->getStrokeColorSpace(), true, false, false);
			writePS(" CS\n");
			noStateChanges = false;
		}
		break;
	case psLevel2Gray:
	case psLevel3Gray:
	case psLevel2Sep:
	case psLevel3Sep:
		break;
	}
}

void PSOutputDev::updateFillColor(GfxState* state)
{
	GfxColor                 color;
	GfxColor*                colorPtr;
	GfxGray                  gray;
	GfxCMYK                  cmyk;
	GfxSeparationColorSpace* sepCS;
	double                   c, m, y, k;
	int                      i;

	switch (level)
	{
	case psLevel1:
	case psLevel2Gray:
	case psLevel3Gray:
		state->getFillGray(&gray);
		writePSFmt("{} g\n", Round4(colToDbl(gray)));
		break;
	case psLevel1Sep:
		state->getFillCMYK(&cmyk);
		c = colToDbl(cmyk.c);
		m = colToDbl(cmyk.m);
		y = colToDbl(cmyk.y);
		k = colToDbl(cmyk.k);
		writePSFmt("{} {} {} {} k\n", Round4(c), Round4(m), Round4(y), Round4(k));
		addProcessColor(c, m, y, k);
		break;
	case psLevel2:
	case psLevel3:
		if (state->getFillColorSpace()->getMode() != csPattern)
		{
			colorPtr = state->getFillColor();
			writePS("[");
			for (i = 0; i < state->getFillColorSpace()->getNComps(); ++i)
			{
				if (i > 0)
					writePS(" ");
				writePSFmt("{}", Round4(colToDbl(colorPtr->c[i])));
			}
			writePS("] sc\n");
		}
		break;
	case psLevel2Sep:
	case psLevel3Sep:
		if (state->getFillColorSpace()->getMode() == csSeparation)
		{
			sepCS      = (GfxSeparationColorSpace*)state->getFillColorSpace();
			color.c[0] = gfxColorComp1;
			sepCS->getCMYK(&color, &cmyk, state->getRenderingIntent());
			writePSFmt("{} {} {} {} {} ({}) ck\n", Round4(colToDbl(state->getFillColor()->c[0])), Round4(colToDbl(cmyk.c)), Round4(colToDbl(cmyk.m)), Round4(colToDbl(cmyk.y)), Round4(colToDbl(cmyk.k)), sepCS->getName());
			addCustomColor(state, sepCS);
		}
		else
		{
			state->getFillCMYK(&cmyk);
			c = colToDbl(cmyk.c);
			m = colToDbl(cmyk.m);
			y = colToDbl(cmyk.y);
			k = colToDbl(cmyk.k);
			writePSFmt("{} {} {} {} k\n", Round4(c), Round4(m), Round4(y), Round4(k));
			addProcessColor(c, m, y, k);
		}
		break;
	}
	t3Cacheable    = false;
	noStateChanges = false;
}

void PSOutputDev::updateStrokeColor(GfxState* state)
{
	GfxColor                 color;
	GfxColor*                colorPtr;
	GfxGray                  gray;
	GfxCMYK                  cmyk;
	GfxSeparationColorSpace* sepCS;
	double                   c, m, y, k;
	int                      i;

	switch (level)
	{
	case psLevel1:
	case psLevel2Gray:
	case psLevel3Gray:
		state->getStrokeGray(&gray);
		writePSFmt("{} G\n", Round4(colToDbl(gray)));
		break;
	case psLevel1Sep:
		state->getStrokeCMYK(&cmyk);
		c = colToDbl(cmyk.c);
		m = colToDbl(cmyk.m);
		y = colToDbl(cmyk.y);
		k = colToDbl(cmyk.k);
		writePSFmt("{} {} {} {} K\n", Round4(c), Round4(m), Round4(y), Round4(k));
		addProcessColor(c, m, y, k);
		break;
	case psLevel2:
	case psLevel3:
		if (state->getStrokeColorSpace()->getMode() != csPattern)
		{
			colorPtr = state->getStrokeColor();
			writePS("[");
			for (i = 0; i < state->getStrokeColorSpace()->getNComps(); ++i)
			{
				if (i > 0)
					writePS(" ");
				writePSFmt("{}", Round4(colToDbl(colorPtr->c[i])));
			}
			writePS("] SC\n");
		}
		break;
	case psLevel2Sep:
	case psLevel3Sep:
		if (state->getStrokeColorSpace()->getMode() == csSeparation)
		{
			sepCS      = (GfxSeparationColorSpace*)state->getStrokeColorSpace();
			color.c[0] = gfxColorComp1;
			sepCS->getCMYK(&color, &cmyk, state->getRenderingIntent());
			writePSFmt("{} {} {} {} {} ({}) CK\n", Round4(colToDbl(state->getStrokeColor()->c[0])), Round4(colToDbl(cmyk.c)), Round4(colToDbl(cmyk.m)), Round4(colToDbl(cmyk.y)), Round4(colToDbl(cmyk.k)), sepCS->getName());
			addCustomColor(state, sepCS);
		}
		else
		{
			state->getStrokeCMYK(&cmyk);
			c = colToDbl(cmyk.c);
			m = colToDbl(cmyk.m);
			y = colToDbl(cmyk.y);
			k = colToDbl(cmyk.k);
			writePSFmt("{} {} {} {} K\n", Round4(c), Round4(m), Round4(y), Round4(k));
			addProcessColor(c, m, y, k);
		}
		break;
	}
	t3Cacheable    = false;
	noStateChanges = false;
}

void PSOutputDev::addProcessColor(double c, double m, double y, double k)
{
	if (c > 0)
		processColors |= psProcessCyan;
	if (m > 0)
		processColors |= psProcessMagenta;
	if (y > 0)
		processColors |= psProcessYellow;
	if (k > 0)
		processColors |= psProcessBlack;
}

void PSOutputDev::addCustomColor(GfxState* state, GfxSeparationColorSpace* sepCS)
{
	PSOutCustomColor* cc;
	GfxColor          color;
	GfxCMYK           cmyk;

	for (cc = customColors; cc; cc = cc->next)
		if (cc->name == sepCS->getName()) return;
	color.c[0] = gfxColorComp1;
	sepCS->getCMYK(&color, &cmyk, state->getRenderingIntent());
	cc           = new PSOutCustomColor(colToDbl(cmyk.c), colToDbl(cmyk.m), colToDbl(cmyk.y), colToDbl(cmyk.k), sepCS->getName());
	cc->next     = customColors;
	customColors = cc;
}

void PSOutputDev::addCustomColors(GfxState* state, GfxDeviceNColorSpace* devnCS)
{
	PSOutCustomColor* cc;
	GfxColor          color;
	GfxCMYK           cmyk;

	for (int i = 0; i < devnCS->getNComps(); ++i)
		color.c[i] = 0;
	for (int i = 0; i < devnCS->getNComps(); ++i)
	{
		for (cc = customColors; cc; cc = cc->next)
			if (cc->name == devnCS->getColorantName(i))
				break;
		if (cc)
			continue;
		color.c[i] = gfxColorComp1;
		devnCS->getCMYK(&color, &cmyk, state->getRenderingIntent());
		color.c[i]   = 0;
		cc           = new PSOutCustomColor(colToDbl(cmyk.c), colToDbl(cmyk.m), colToDbl(cmyk.y), colToDbl(cmyk.k), devnCS->getColorantName(i));
		cc->next     = customColors;
		customColors = cc;
	}
}

void PSOutputDev::updateFillOverprint(GfxState* state)
{
	if (level == psLevel2 || level == psLevel2Sep || level == psLevel3 || level == psLevel3Sep)
	{
		writePSFmt("{} op\n", state->getFillOverprint() ? "true" : "false");
		noStateChanges = false;
	}
}

void PSOutputDev::updateStrokeOverprint(GfxState* state)
{
	if (level == psLevel2 || level == psLevel2Sep || level == psLevel3 || level == psLevel3Sep)
	{
		writePSFmt("{} OP\n", state->getStrokeOverprint() ? "true" : "false");
		noStateChanges = false;
	}
}

void PSOutputDev::updateOverprintMode(GfxState* state)
{
	if (level == psLevel3 || level == psLevel3Sep)
	{
		writePSFmt("{} opm\n", state->getOverprintMode() ? "true" : "false");
		noStateChanges = false;
	}
}

void PSOutputDev::updateTransfer(GfxState* state)
{
	Function** funcs;
	int        i;

	funcs = state->getTransfer();
	if (funcs[0] && funcs[1] && funcs[2] && funcs[3])
	{
		if (level == psLevel2 || level == psLevel2Sep || level == psLevel3 || level == psLevel3Sep)
		{
			for (i = 0; i < 4; ++i)
				cvtFunction(funcs[i]);
			writePS("setcolortransfer\n");
		}
		else
		{
			cvtFunction(funcs[3]);
			writePS("settransfer\n");
		}
	}
	else if (funcs[0])
	{
		cvtFunction(funcs[0]);
		writePS("settransfer\n");
	}
	else
	{
		writePS("{} settransfer\n");
	}
	noStateChanges = false;
}

void PSOutputDev::updateFont(GfxState* state)
{
	if (state->getFont())
	{
		if (state->getFont()->getTag().size() && state->getFont()->getTag() == "xpdf_default_font")
			writePSFmt("/xpdf_default_font {} Tf\n", Round6(fabs(state->getFontSize()) < 0.0001 ? 0.0001 : state->getFontSize()));
		else
			writePSFmt("/F{}_{} {} Tf\n", state->getFont()->getID()->num, state->getFont()->getID()->gen, Round6(fabs(state->getFontSize()) < 0.0001 ? 0.0001 : state->getFontSize()));
		noStateChanges = false;
	}
}

void PSOutputDev::updateTextMat(GfxState* state)
{
	double* mat;

	mat = state->getTextMat();
	if (fabs(mat[0] * mat[3] - mat[1] * mat[2]) < 1e-10)
	{
		// avoid a singular (or close-to-singular) matrix
		writePSFmt("[0.00001 0 0 0.00001 {} {}] Tm\n", Round6(mat[4]), Round6(mat[5]));
	}
	else
	{
		writePSFmt("[{} {} {} {} {} {}] Tm\n", Round6(mat[0]), Round6(mat[1]), Round6(mat[2]), Round6(mat[3]), Round6(mat[4]), Round6(mat[5]));
	}
	noStateChanges = false;
}

void PSOutputDev::updateCharSpace(GfxState* state)
{
	writePSFmt("{} Tc\n", Round6(state->getCharSpace()));
	noStateChanges = false;
}

void PSOutputDev::updateRender(GfxState* state)
{
	int rm;

	rm = state->getRender();
	writePSFmt("{} Tr\n", rm);
	rm &= 3;
	if (rm != 0 && rm != 3)
		t3Cacheable = false;
	noStateChanges = false;
}

void PSOutputDev::updateRise(GfxState* state)
{
	writePSFmt("{} Ts\n", Round6(state->getRise()));
	noStateChanges = false;
}

void PSOutputDev::updateWordSpace(GfxState* state)
{
	writePSFmt("{} Tw\n", Round6(state->getWordSpace()));
	noStateChanges = false;
}

void PSOutputDev::updateHorizScaling(GfxState* state)
{
	double h;

	h = state->getHorizScaling();
	if (fabs(h) < 0.01)
		h = 0.01;
	writePSFmt("{} Tz\n", Round6(h));
	noStateChanges = false;
}

void PSOutputDev::updateTextPos(GfxState* state)
{
	writePSFmt("{} {} Td\n", Round6(state->getLineX()), Round6(state->getLineY()));
	noStateChanges = false;
}

void PSOutputDev::updateTextShift(GfxState* state, double shift)
{
	if (state->getFont()->getWMode())
		writePSFmt("{} TJmV\n", Round6(shift));
	else
		writePSFmt("{} TJm\n", Round6(shift));
	noStateChanges = false;
}

void PSOutputDev::stroke(GfxState* state)
{
	doPath(state->getPath());
	if (inType3Char && t3FillColorOnly)
	{
		// if we're constructing a cacheable Type 3 glyph, we need to do
		// everything in the fill color
		writePS("Sf\n");
	}
	else
	{
		writePS("S\n");
	}
	noStateChanges = false;
}

void PSOutputDev::fill(GfxState* state)
{
	doPath(state->getPath());
	writePS("f\n");
	noStateChanges = false;
}

void PSOutputDev::eoFill(GfxState* state)
{
	doPath(state->getPath());
	writePS("f*\n");
	noStateChanges = false;
}

void PSOutputDev::tilingPatternFill(GfxState* state, Gfx* gfx, Object* strRef, int paintType, int tilingType, Dict* resDict, double* mat, double* bbox, int x0, int y0, int x1, int y1, double xStep, double yStep)
{
	if (level <= psLevel1Sep)
		tilingPatternFillL1(state, gfx, strRef, paintType, tilingType, resDict, mat, bbox, x0, y0, x1, y1, xStep, yStep);
	else
		tilingPatternFillL2(state, gfx, strRef, paintType, tilingType, resDict, mat, bbox, x0, y0, x1, y1, xStep, yStep);
}

void PSOutputDev::tilingPatternFillL1(GfxState* state, Gfx* gfx, Object* strRef, int paintType, int tilingType, Dict* resDict, double* mat, double* bbox, int x0, int y0, int x1, int y1, double xStep, double yStep)
{
	PDFRectangle box;
	Gfx*         gfx2;

	// define a Type 3 font
	writePS("8 dict begin\n");
	writePS("/FontType 3 def\n");
	writePS("/FontMatrix [1 0 0 1 0 0] def\n");
	writePSFmt("/FontBBox [{} {} {} {}] def\n", Round6(bbox[0]), Round6(bbox[1]), Round6(bbox[2]), Round6(bbox[3]));
	writePS("/Encoding 256 array def\n");
	writePS("  0 1 255 { Encoding exch /.notdef put } for\n");
	writePS("  Encoding 120 /x put\n");
	writePS("/BuildGlyph {\n");
	writePS("  exch /CharProcs get exch\n");
	writePS("  2 copy known not { pop /.notdef } if\n");
	writePS("  get exec\n");
	writePS("} bind def\n");
	writePS("/BuildChar {\n");
	writePS("  1 index /Encoding get exch get\n");
	writePS("  1 index /BuildGlyph get exec\n");
	writePS("} bind def\n");
	writePS("/CharProcs 1 dict def\n");
	writePS("CharProcs begin\n");
	box.x1 = bbox[0];
	box.y1 = bbox[1];
	box.x2 = bbox[2];
	box.y2 = bbox[3];
	gfx2   = new Gfx(doc, this, resDict, &box, nullptr);
	gfx2->takeContentStreamStack(gfx);
	writePS("/x {\n");
	if (paintType == 2)
	{
		writePSFmt("{} 0 {} {} {} {} setcachedevice\n", Round6(xStep), Round6(bbox[0]), Round6(bbox[1]), Round6(bbox[2]), Round6(bbox[3]));
		t3FillColorOnly = true;
	}
	else
	{
		if (x1 - 1 <= x0)
			writePS("1 0 setcharwidth\n");
		else
			writePSFmt("{} 0 setcharwidth\n", Round6(xStep));
		t3FillColorOnly = false;
	}
	inType3Char = true;
	++numTilingPatterns;
	gfx2->display(strRef);
	--numTilingPatterns;
	inType3Char = false;
	writePS("} def\n");
	delete gfx2;
	writePS("end\n");
	writePS("currentdict end\n");
	writePSFmt("/xpdfTile{} exch definefont pop\n", numTilingPatterns);

	// draw the tiles
	writePSFmt("/xpdfTile{} findfont setfont\n", numTilingPatterns);
	writePS("fCol\n");
	writePSFmt("gsave [{} {} {} {} {} {}] concat\n", Round6(mat[0]), Round6(mat[1]), Round6(mat[2]), Round6(mat[3]), Round6(mat[4]), Round6(mat[5]));
	writePSFmt("{} 1 {} {{ {} exch {} mul m {} 1 {} {{ pop (x) show }} for }} for\n", y0, y1 - 1, Round6(x0 * xStep), Round6(yStep), x0, x1 - 1);
	writePS("grestore\n");
	noStateChanges = false;
}

void PSOutputDev::tilingPatternFillL2(GfxState* state, Gfx* gfx, Object* strRef, int paintType, int tilingType, Dict* resDict, double* mat, double* bbox, int x0, int y0, int x1, int y1, double xStep, double yStep)
{
	PDFRectangle box;
	Gfx*         gfx2;

	// switch to pattern space
	writePSFmt("gsave [{} {} {} {} {} {}] concat\n", Round6(mat[0]), Round6(mat[1]), Round6(mat[2]), Round6(mat[3]), Round6(mat[4]), Round6(mat[5]));

	// define a pattern
	writePSFmt("/xpdfTile{}\n", numTilingPatterns);
	writePS("<<\n");
	writePS("  /PatternType 1\n");
	writePSFmt("  /PaintType {}\n", paintType);
	writePSFmt("  /TilingType {}\n", tilingType);
	writePSFmt("  /BBox [{} {} {} {}]\n", Round6(bbox[0]), Round6(bbox[1]), Round6(bbox[2]), Round6(bbox[3]));
	writePSFmt("  /XStep {}\n", Round6(xStep));
	writePSFmt("  /YStep {}\n", Round6(yStep));
	writePS("  /PaintProc {\n");
	writePS("    pop\n");
	box.x1 = bbox[0];
	box.y1 = bbox[1];
	box.x2 = bbox[2];
	box.y2 = bbox[3];
	gfx2   = new Gfx(doc, this, resDict, &box, nullptr);
	gfx2->takeContentStreamStack(gfx);
	t3FillColorOnly = paintType == 2;
	inType3Char     = true;
	++numTilingPatterns;
	gfx2->display(strRef);
	--numTilingPatterns;
	inType3Char = false;
	delete gfx2;
	writePS("  }\n");
	writePS(">> matrix makepattern def\n");

	// set the pattern
	if (paintType == 2)
	{
		writePS("fCol\n");
		writePS("currentcolor ");
	}
	writePSFmt("xpdfTile{} setpattern\n", numTilingPatterns);

	// fill with the pattern
	writePSFmt("{} {} {} {} rectfill\n", Round6(x0 * xStep + bbox[0]), Round6(y0 * yStep + bbox[1]), Round6((x1 - x0) * xStep + bbox[2]), Round6((y1 - y0) * yStep + bbox[3]));

	writePS("grestore\n");
	noStateChanges = false;
}

bool PSOutputDev::shadedFill(GfxState* state, GfxShading* shading)
{
	if (level != psLevel2 && level != psLevel2Sep && level != psLevel3 && level != psLevel3Sep)
		return false;

	switch (shading->getType())
	{
	case 1:
		functionShadedFill(state, (GfxFunctionShading*)shading);
		return true;
	case 2:
		axialShadedFill(state, (GfxAxialShading*)shading);
		return true;
	case 3:
		radialShadedFill(state, (GfxRadialShading*)shading);
		return true;
	default:
		return false;
	}
}

bool PSOutputDev::functionShadedFill(GfxState* state, GfxFunctionShading* shading)
{
	double  x0, y0, x1, y1;
	double* mat;
	int     i;

	if (level == psLevel2Sep || level == psLevel3Sep)
	{
		if (shading->getColorSpace()->getMode() != csDeviceCMYK)
			return false;
		processColors |= psProcessCMYK;
	}

	shading->getDomain(&x0, &y0, &x1, &y1);
	mat = shading->getMatrix();
	writePSFmt("/mat [{} {} {} {} {} {}] def\n", Round6(mat[0]), Round6(mat[1]), Round6(mat[2]), Round6(mat[3]), Round6(mat[4]), Round6(mat[5]));
	writePSFmt("/n {} def\n", shading->getColorSpace()->getNComps());
	if (shading->getNFuncs() == 1)
	{
		writePS("/func ");
		cvtFunction(shading->getFunc(0));
		writePS("def\n");
	}
	else
	{
		writePS("/func {\n");
		for (i = 0; i < shading->getNFuncs(); ++i)
		{
			if (i < shading->getNFuncs() - 1)
				writePS("2 copy\n");
			cvtFunction(shading->getFunc(i));
			writePS("exec\n");
			if (i < shading->getNFuncs() - 1)
				writePS("3 1 roll\n");
		}
		writePS("} def\n");
	}
	writePSFmt("{} {} {} {} 0 funcSH\n", Round6(x0), Round6(y0), Round6(x1), Round6(y1));

	noStateChanges = false;
	return true;
}

bool PSOutputDev::axialShadedFill(GfxState* state, GfxAxialShading* shading)
{
	double xMin, yMin, xMax, yMax;
	double x0, y0, x1, y1, dx, dy, mul;
	double tMin, tMax, t, t0, t1;
	int    i;

	if (level == psLevel2Sep || level == psLevel3Sep)
	{
		if (shading->getColorSpace()->getMode() != csDeviceCMYK)
			return false;
		processColors |= psProcessCMYK;
	}

	// get the clip region bbox
	state->getUserClipBBox(&xMin, &yMin, &xMax, &yMax);

	// compute min and max t values, based on the four corners of the
	// clip region bbox
	shading->getCoords(&x0, &y0, &x1, &y1);
	dx = x1 - x0;
	dy = y1 - y0;
	if (fabs(dx) < 0.01 && fabs(dy) < 0.01)
	{
		return true;
	}
	else
	{
		mul  = 1 / (dx * dx + dy * dy);
		tMin = tMax = ((xMin - x0) * dx + (yMin - y0) * dy) * mul;
		t           = ((xMin - x0) * dx + (yMax - y0) * dy) * mul;
		if (t < tMin)
			tMin = t;
		else if (t > tMax)
			tMax = t;
		t = ((xMax - x0) * dx + (yMin - y0) * dy) * mul;
		if (t < tMin)
			tMin = t;
		else if (t > tMax)
			tMax = t;
		t = ((xMax - x0) * dx + (yMax - y0) * dy) * mul;
		if (t < tMin)
			tMin = t;
		else if (t > tMax)
			tMax = t;
		if (tMin < 0 && !shading->getExtend0())
			tMin = 0;
		if (tMax > 1 && !shading->getExtend1())
			tMax = 1;
	}

	// get the function domain
	t0 = shading->getDomain0();
	t1 = shading->getDomain1();

	// generate the PS code
	writePSFmt("/t0 {} def\n", Round6(t0));
	writePSFmt("/t1 {} def\n", Round6(t1));
	writePSFmt("/dt {} def\n", Round6(t1 - t0));
	writePSFmt("/x0 {} def\n", Round6(x0));
	writePSFmt("/y0 {} def\n", Round6(y0));
	writePSFmt("/dx {} def\n", Round6(x1 - x0));
	writePSFmt("/x1 {} def\n", Round6(x1));
	writePSFmt("/y1 {} def\n", Round6(y1));
	writePSFmt("/dy {} def\n", Round6(y1 - y0));
	writePSFmt("/xMin {} def\n", Round6(xMin));
	writePSFmt("/yMin {} def\n", Round6(yMin));
	writePSFmt("/xMax {} def\n", Round6(xMax));
	writePSFmt("/yMax {} def\n", Round6(yMax));
	writePSFmt("/n {} def\n", shading->getColorSpace()->getNComps());
	if (shading->getNFuncs() == 1)
	{
		writePS("/func ");
		cvtFunction(shading->getFunc(0));
		writePS("def\n");
	}
	else
	{
		writePS("/func {\n");
		for (i = 0; i < shading->getNFuncs(); ++i)
		{
			if (i < shading->getNFuncs() - 1)
				writePS("dup\n");
			cvtFunction(shading->getFunc(i));
			writePS("exec\n");
			if (i < shading->getNFuncs() - 1)
				writePS("exch\n");
		}
		writePS("} def\n");
	}
	writePSFmt("{} {} 0 axialSH\n", Round6(tMin), Round6(tMax));

	noStateChanges = false;
	return true;
}

bool PSOutputDev::radialShadedFill(GfxState* state, GfxRadialShading* shading)
{
	double xMin, yMin, xMax, yMax;
	double x0, y0, r0, x1, y1, r1, t0, t1;
	double xa, ya, ra;
	double sMin, sMax, h, ta;
	double sLeft, sRight, sTop, sBottom, sZero, sDiag;
	bool   haveSLeft, haveSRight, haveSTop, haveSBottom, haveSZero;
	bool   haveSMin, haveSMax;
	double theta, alpha, a1, a2;
	bool   enclosed;
	int    i;

	if (level == psLevel2Sep || level == psLevel3Sep)
	{
		if (shading->getColorSpace()->getMode() != csDeviceCMYK)
			return false;
		processColors |= psProcessCMYK;
	}

	// get the shading info
	shading->getCoords(&x0, &y0, &r0, &x1, &y1, &r1);
	t0 = shading->getDomain0();
	t1 = shading->getDomain1();

	// Compute the point at which r(s) = 0; check for the enclosed
	// circles case; and compute the angles for the tangent lines.
	h = sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
	if (h == 0)
	{
		enclosed = true;
		theta    = 0; // make gcc happy
	}
	else if (r1 - r0 == 0)
	{
		enclosed = false;
		theta    = 0;
	}
	else if (fabs(r1 - r0) >= h)
	{
		enclosed = true;
		theta    = 0; // make gcc happy
	}
	else
	{
		enclosed = false;
		theta    = asin((r1 - r0) / h);
	}
	if (enclosed)
	{
		a1 = 0;
		a2 = 360;
	}
	else
	{
		alpha = atan2(y1 - y0, x1 - x0);
		a1    = (180 / M_PI) * (alpha + theta) + 90;
		a2    = (180 / M_PI) * (alpha - theta) - 90;
		while (a2 < a1)
			a2 += 360;
	}

	// compute the (possibly extended) s range
	state->getUserClipBBox(&xMin, &yMin, &xMax, &yMax);
	if (enclosed)
	{
		sMin = 0;
		sMax = 1;
	}
	else
	{
		// solve x(sLeft) + r(sLeft) = xMin
		if ((haveSLeft = fabs((x1 + r1) - (x0 + r0)) > 0.000001))
			sLeft = (xMin - (x0 + r0)) / ((x1 + r1) - (x0 + r0));
		else
			sLeft = 0; // make gcc happy
		// solve x(sRight) - r(sRight) = xMax
		if ((haveSRight = fabs((x1 - r1) - (x0 - r0)) > 0.000001))
			sRight = (xMax - (x0 - r0)) / ((x1 - r1) - (x0 - r0));
		else
			sRight = 0; // make gcc happy
		// solve y(sBottom) + r(sBottom) = yMin
		if ((haveSBottom = fabs((y1 + r1) - (y0 + r0)) > 0.000001))
			sBottom = (yMin - (y0 + r0)) / ((y1 + r1) - (y0 + r0));
		else
			sBottom = 0; // make gcc happy
		// solve y(sTop) - r(sTop) = yMax
		if ((haveSTop = fabs((y1 - r1) - (y0 - r0)) > 0.000001))
			sTop = (yMax - (y0 - r0)) / ((y1 - r1) - (y0 - r0));
		else
			sTop = 0; // make gcc happy
		// solve r(sZero) = 0
		if ((haveSZero = fabs(r1 - r0) > 0.000001))
			sZero = -r0 / (r1 - r0);
		else
			sZero = 0; // make gcc happy
		// solve r(sDiag) = sqrt((xMax-xMin)^2 + (yMax-yMin)^2)
		if (haveSZero)
			sDiag = (sqrt((xMax - xMin) * (xMax - xMin) + (yMax - yMin) * (yMax - yMin)) - r0) / (r1 - r0);
		else
			sDiag = 0; // make gcc happy
		// compute sMin
		if (shading->getExtend0())
		{
			sMin     = 0;
			haveSMin = false;
			if (x0 < x1 && haveSLeft && sLeft < 0)
			{
				sMin     = sLeft;
				haveSMin = true;
			}
			else if (x0 > x1 && haveSRight && sRight < 0)
			{
				sMin     = sRight;
				haveSMin = true;
			}
			if (y0 < y1 && haveSBottom && sBottom < 0)
			{
				if (!haveSMin || sBottom > sMin)
				{
					sMin     = sBottom;
					haveSMin = true;
				}
			}
			else if (y0 > y1 && haveSTop && sTop < 0)
			{
				if (!haveSMin || sTop > sMin)
				{
					sMin     = sTop;
					haveSMin = true;
				}
			}
			if (haveSZero && sZero < 0)
			{
				if (!haveSMin || sZero > sMin)
					sMin = sZero;
			}
		}
		else
		{
			sMin = 0;
		}
		// compute sMax
		if (shading->getExtend1())
		{
			sMax     = 1;
			haveSMax = false;
			if (x1 < x0 && haveSLeft && sLeft > 1)
			{
				sMax     = sLeft;
				haveSMax = true;
			}
			else if (x1 > x0 && haveSRight && sRight > 1)
			{
				sMax     = sRight;
				haveSMax = true;
			}
			if (y1 < y0 && haveSBottom && sBottom > 1)
			{
				if (!haveSMax || sBottom < sMax)
				{
					sMax     = sBottom;
					haveSMax = true;
				}
			}
			else if (y1 > y0 && haveSTop && sTop > 1)
			{
				if (!haveSMax || sTop < sMax)
				{
					sMax     = sTop;
					haveSMax = true;
				}
			}
			if (haveSZero && sDiag > 1)
			{
				if (!haveSMax || sDiag < sMax)
					sMax = sDiag;
			}
		}
		else
		{
			sMax = 1;
		}
	}

	// generate the PS code
	writePSFmt("/x0 {} def\n", Round6(x0));
	writePSFmt("/x1 {} def\n", Round6(x1));
	writePSFmt("/dx {} def\n", Round6(x1 - x0));
	writePSFmt("/y0 {} def\n", Round6(y0));
	writePSFmt("/y1 {} def\n", Round6(y1));
	writePSFmt("/dy {} def\n", Round6(y1 - y0));
	writePSFmt("/r0 {} def\n", Round6(r0));
	writePSFmt("/r1 {} def\n", Round6(r1));
	writePSFmt("/dr {} def\n", Round6(r1 - r0));
	writePSFmt("/t0 {} def\n", Round6(t0));
	writePSFmt("/t1 {} def\n", Round6(t1));
	writePSFmt("/dt {} def\n", Round6(t1 - t0));
	writePSFmt("/n {} def\n", shading->getColorSpace()->getNComps());
	writePSFmt("/encl {} def\n", enclosed ? "true" : "false");
	writePSFmt("/a1 {} def\n", Round6(a1));
	writePSFmt("/a2 {} def\n", Round6(a2));
	if (shading->getNFuncs() == 1)
	{
		writePS("/func ");
		cvtFunction(shading->getFunc(0));
		writePS("def\n");
	}
	else
	{
		writePS("/func {\n");
		for (i = 0; i < shading->getNFuncs(); ++i)
		{
			if (i < shading->getNFuncs() - 1)
				writePS("dup\n");
			cvtFunction(shading->getFunc(i));
			writePS("exec\n");
			if (i < shading->getNFuncs() - 1)
				writePS("exch\n");
		}
		writePS("} def\n");
	}
	writePSFmt("{} {} 0 radialSH\n", Round6(sMin), Round6(sMax));

	// extend the 'enclosed' case
	if (enclosed)
	{
		// extend the smaller circle
		if ((shading->getExtend0() && r0 <= r1) || (shading->getExtend1() && r1 < r0))
		{
			if (r0 <= r1)
			{
				ta = t0;
				ra = r0;
				xa = x0;
				ya = y0;
			}
			else
			{
				ta = t1;
				ra = r1;
				xa = x1;
				ya = y1;
			}
			if (level == psLevel2Sep || level == psLevel3Sep)
				writePSFmt("{} radialCol aload pop k\n", Round6(ta));
			else
				writePSFmt("{} radialCol sc\n", Round6(ta));
			writePSFmt("{} {} {} 0 360 arc h f*\n", Round6(xa), Round6(ya), Round6(ra));
		}

		// extend the larger circle
		if ((shading->getExtend0() && r0 > r1) || (shading->getExtend1() && r1 >= r0))
		{
			if (r0 > r1)
			{
				ta = t0;
				ra = r0;
				xa = x0;
				ya = y0;
			}
			else
			{
				ta = t1;
				ra = r1;
				xa = x1;
				ya = y1;
			}
			if (level == psLevel2Sep || level == psLevel3Sep)
				writePSFmt("{} radialCol aload pop k\n", Round6(ta));
			else
				writePSFmt("{} radialCol sc\n", Round6(ta));
			writePSFmt("{} {} {} 0 360 arc h\n", Round6(xa), Round6(ya), Round6(ra));
			writePSFmt("{} {} m {} {} l {} {} l {} {} l h f*\n", Round6(xMin), Round6(yMin), Round6(xMin), Round6(yMax), Round6(xMax), Round6(yMax), Round6(xMax), Round6(yMin));
		}
	}

	noStateChanges = false;
	return true;
}

void PSOutputDev::clip(GfxState* state)
{
	doPath(state->getPath());
	writePS("W\n");
	noStateChanges = false;
}

void PSOutputDev::eoClip(GfxState* state)
{
	doPath(state->getPath());
	writePS("W*\n");
	noStateChanges = false;
}

void PSOutputDev::clipToStrokePath(GfxState* state)
{
	doPath(state->getPath());
	writePS("Ws\n");
	noStateChanges = false;
}

void PSOutputDev::doPath(GfxPath* path)
{
	GfxSubpath* subpath;
	double      x0, y0, x1, y1, x2, y2, x3, y3, x4, y4;
	int         n, m, i, j;

	n = path->getNumSubpaths();

	if (n == 1 && path->getSubpath(0)->getNumPoints() == 5)
	{
		subpath = path->getSubpath(0);
		x0      = subpath->getX(0);
		y0      = subpath->getY(0);
		x4      = subpath->getX(4);
		y4      = subpath->getY(4);
		if (x4 == x0 && y4 == y0)
		{
			x1 = subpath->getX(1);
			y1 = subpath->getY(1);
			x2 = subpath->getX(2);
			y2 = subpath->getY(2);
			x3 = subpath->getX(3);
			y3 = subpath->getY(3);
			if (x0 == x1 && x2 == x3 && y0 == y3 && y1 == y2)
			{
				writePSFmt("{} {} {} {} re\n", Round6(x0 < x2 ? x0 : x2), Round6(y0 < y1 ? y0 : y1), Round6(fabs(x2 - x0)), Round6(fabs(y1 - y0)));
				return;
			}
			else if (x0 == x3 && x1 == x2 && y0 == y1 && y2 == y3)
			{
				writePSFmt("{} {} {} {} re\n", Round6(x0 < x1 ? x0 : x1), Round6(y0 < y2 ? y0 : y2), Round6(fabs(x1 - x0)), Round6(fabs(y2 - y0)));
				return;
			}
		}
	}

	for (i = 0; i < n; ++i)
	{
		subpath = path->getSubpath(i);
		m       = subpath->getNumPoints();
		writePSFmt("{} {} m\n", Round6(subpath->getX(0)), Round6(subpath->getY(0)));
		j = 1;
		while (j < m)
		{
			if (subpath->getCurve(j))
			{
				writePSFmt("{} {} {} {} {} {} c\n", Round6(subpath->getX(j)), Round6(subpath->getY(j)), Round6(subpath->getX(j + 1)), Round6(subpath->getY(j + 1)), Round6(subpath->getX(j + 2)), Round6(subpath->getY(j + 2)));
				j += 3;
			}
			else
			{
				writePSFmt("{} {} l\n", Round6(subpath->getX(j)), Round6(subpath->getY(j)));
				++j;
			}
		}
		if (subpath->isClosed())
			writePS("h\n");
	}
}

void PSOutputDev::drawString(GfxState* state, const std::string& s, bool fill, bool stroke, bool makePath)
{
	// check for invisible text -- this is used by Acrobat Capture
	if (!fill && !stroke && !makePath)
		return;

	// ignore empty strings
	if (s.size() == 0)
		return;

	// get the font
	GfxFont* font = state->getFont();
	if (!font)
		return;
	int wMode = font->getWMode();

	PSFontInfo* fi = nullptr;
	for (int i = 0; i < fontInfo->getLength(); ++i)
	{
		fi = (PSFontInfo*)fontInfo->get(i);
		if (fi->fontID.num == font->getID()->num && fi->fontID.gen == font->getID()->gen)
			break;
		fi = nullptr;
	}

	// check for a subtitute 16-bit font
	UnicodeMap* uMap      = nullptr;
	int*        codeToGID = nullptr;
	if (font->isCIDFont())
	{
		if (!(fi && fi->ff))
		{
			// font substitution failed, so don't output any text
			return;
		}
		if (fi->ff->encoding.size())
			uMap = globalParams->getUnicodeMap(fi->ff->encoding);

		// check for an 8-bit code-to-GID map
	}
	else
	{
		if (fi && fi->ff)
			codeToGID = fi->ff->codeToGID;
	}

	// compute the positioning (dx, dy) for each char in the string
	int         nChars = 0;
	const char* p      = s.c_str();
	int         len    = TO_INT(s.size());
	std::string s2;
	int         dxdySize = font->isCIDFont() ? 8 : TO_INT(s.size());
	double*     dxdy     = (double*)gmallocn(2 * dxdySize, sizeof(double));
	double      originX0 = 0, originY0 = 0;
	while (len > 0)
	{
		CharCode code;
		Unicode  u[8];
		int      uLen;
		double   dx, dy, originX, originY;
		int      n = font->getNextChar(p, len, &code, u, (int)(sizeof(u) / sizeof(Unicode)), &uLen, &dx, &dy, &originX, &originY);
		//~ this doesn't handle the case where the origin offset changes
		//~   within a string of characters -- which could be fixed by
		//~   modifying dx,dy as needed for each character
		if (p == s.c_str())
		{
			originX0 = originX;
			originY0 = originY;
		}
		dx *= state->getFontSize();
		dy *= state->getFontSize();
		if (wMode)
		{
			dy += state->getCharSpace();
			if (n == 1 && *p == ' ')
				dy += state->getWordSpace();
		}
		else
		{
			dx += state->getCharSpace();
			if (n == 1 && *p == ' ')
				dx += state->getWordSpace();
		}
		dx *= state->getHorizScaling();
		if (font->isCIDFont())
		{
			if (uMap)
			{
				if (nChars + uLen > dxdySize)
				{
					do
					{
						dxdySize *= 2;
					}
					while (nChars + uLen > dxdySize);
					dxdy = (double*)greallocn(dxdy, 2 * dxdySize, sizeof(double));
				}
				char buf[8];
				for (int i = 0; i < uLen; ++i)
				{
					int m = uMap->mapUnicode(u[i], buf, (int)sizeof(buf));
					for (int j = 0; j < m; ++j)
						s2 += buf[j];
					//~ this really needs to get the number of chars in the target encoding
					// - which may be more than the number of Unicode chars
					dxdy[2 * nChars]     = dx;
					dxdy[2 * nChars + 1] = dy;
					++nChars;
				}
			}
			else
			{
				if (nChars + 1 > dxdySize)
				{
					dxdySize *= 2;
					dxdy = (double*)greallocn(dxdy, 2 * dxdySize, sizeof(double));
				}
				s2 += (char)((code >> 8) & 0xff);
				s2 += (char)(code & 0xff);
				dxdy[2 * nChars]     = dx;
				dxdy[2 * nChars + 1] = dy;
				++nChars;
			}
		}
		else if (!codeToGID || codeToGID[code] >= 0)
		{
			s2 += (char)code;
			dxdy[2 * nChars]     = dx;
			dxdy[2 * nChars + 1] = dy;
			++nChars;
		}
		p += n;
		len -= n;
	}
	if (uMap)
		uMap->decRefCnt();
	originX0 *= state->getFontSize();
	originY0 *= state->getFontSize();
	double tOriginX0, tOriginY0;
	state->textTransformDelta(originX0, originY0, &tOriginX0, &tOriginY0);

	if (nChars > 0)
	{
		if (wMode)
			writePSFmt("{} {} rmoveto\n", Round6(-tOriginX0), Round6(-tOriginY0));

		writePSString(s2);
		writePS("\n[");
		for (int i = 0; i < 2 * nChars; ++i)
		{
			if (i > 0)
				writePS("\n");
			writePSFmt("{}", Round6(dxdy[i]));
		}
		writePS("] ");

		// the possible operations are:
		//   - fill
		//   - stroke
		//   - fill + stroke
		//   - makePath

		if (font->getType() == fontType3)
		{
			writePS("Tj3\n");
		}
		else if (fill && stroke)
		{
			writePS("TjFS\n");
		}
		else if (fill)
		{
			writePS("Tj\n");
		}
		else if (stroke)
		{
			writePS("TjS\n");
		}
		else if (makePath)
		{
			writePS("TjSave\n");
			haveSavedTextPath = true;
		}

		if (wMode)
			writePSFmt("{} {} rmoveto\n", Round6(tOriginX0), Round6(tOriginY0));
	}
	gfree(dxdy);

	noStateChanges = false;
}

void PSOutputDev::fillTextPath(GfxState* state)
{
	writePS("Tfill\n");
}

void PSOutputDev::strokeTextPath(GfxState* state)
{
	writePS("Tstroke\n");
}

void PSOutputDev::clipToTextPath(GfxState* state)
{
	writePS("Tclip\n");
}

void PSOutputDev::clipToTextStrokePath(GfxState* state)
{
	writePS("Tstrokeclip\n");
}

void PSOutputDev::clearTextPath(GfxState* state)
{
	if (haveSavedTextPath)
	{
		writePS("Tclear\n");
		haveSavedTextPath = false;
	}
}

void PSOutputDev::addTextPathToSavedClipPath(GfxState* state)
{
	if (haveSavedTextPath)
	{
		writePS("Tsave\n");
		haveSavedTextPath = false;
		haveSavedClipPath = true;
	}
}

void PSOutputDev::clipToSavedClipPath(GfxState* state)
{
	if (!haveSavedClipPath)
		return;
	writePS("Tclip2\n");
	haveSavedClipPath = false;
}

void PSOutputDev::endTextObject(GfxState* state)
{
}

void PSOutputDev::drawImageMask(GfxState* state, Object* ref, Stream* str, int width, int height, bool invert, bool inlineImg, bool interpolate)
{
	int len;

	len = height * ((width + 7) / 8);
	switch (level)
	{
	case psLevel1:
	case psLevel1Sep:
		doImageL1(ref, state, nullptr, invert, inlineImg, str, width, height, len);
		break;
	case psLevel2:
	case psLevel2Gray:
	case psLevel2Sep:
		doImageL2(ref, state, nullptr, invert, inlineImg, str, width, height, len, nullptr, nullptr, 0, 0, false);
		break;
	case psLevel3:
	case psLevel3Gray:
	case psLevel3Sep:
		doImageL3(ref, state, nullptr, invert, inlineImg, str, width, height, len, nullptr, nullptr, 0, 0, false);
		break;
	}
	noStateChanges = false;
}

void PSOutputDev::drawImage(GfxState* state, Object* ref, Stream* str, int width, int height, GfxImageColorMap* colorMap, int* maskColors, bool inlineImg, bool interpolate)
{
	int len;

	len = height * ((width * colorMap->getNumPixelComps() * colorMap->getBits() + 7) / 8);
	switch (level)
	{
	case psLevel1:
		doImageL1(ref, state, colorMap, false, inlineImg, str, width, height, len);
		break;
	case psLevel1Sep:
		//~ handle indexed, separation, ... color spaces
		doImageL1Sep(state, colorMap, false, inlineImg, str, width, height, len);
		break;
	case psLevel2:
	case psLevel2Gray:
	case psLevel2Sep:
		doImageL2(ref, state, colorMap, false, inlineImg, str, width, height, len, maskColors, nullptr, 0, 0, false);
		break;
	case psLevel3:
	case psLevel3Gray:
	case psLevel3Sep:
		doImageL3(ref, state, colorMap, false, inlineImg, str, width, height, len, maskColors, nullptr, 0, 0, false);
		break;
	}
	t3Cacheable    = false;
	noStateChanges = false;
}

void PSOutputDev::drawMaskedImage(GfxState* state, Object* ref, Stream* str, int width, int height, GfxImageColorMap* colorMap, Object* maskRef, Stream* maskStr, int maskWidth, int maskHeight, bool maskInvert, bool interpolate)
{
	int len;

	len = height * ((width * colorMap->getNumPixelComps() * colorMap->getBits() + 7) / 8);
	switch (level)
	{
	case psLevel1:
		doImageL1(ref, state, colorMap, false, false, str, width, height, len);
		break;
	case psLevel1Sep:
		//~ handle indexed, separation, ... color spaces
		doImageL1Sep(state, colorMap, false, false, str, width, height, len);
		break;
	case psLevel2:
	case psLevel2Gray:
	case psLevel2Sep:
		doImageL2(ref, state, colorMap, false, false, str, width, height, len, nullptr, maskStr, maskWidth, maskHeight, maskInvert);
		break;
	case psLevel3:
	case psLevel3Gray:
	case psLevel3Sep:
		doImageL3(ref, state, colorMap, false, false, str, width, height, len, nullptr, maskStr, maskWidth, maskHeight, maskInvert);
		break;
	}
	t3Cacheable    = false;
	noStateChanges = false;
}

void PSOutputDev::doImageL1(Object* ref, GfxState* state, GfxImageColorMap* colorMap, bool invert, bool inlineImg, Stream* str, int width, int height, int len)
{
	ImageStream* imgStr;
	uint8_t      pixBuf[gfxColorMaxComps];
	GfxGray      gray;
	int          col, x, y, c, i;

	if ((inType3Char || preload) && !colorMap)
	{
		if (inlineImg)
		{
			// create an array
			str = new FixedLengthEncoder(str, len);
			str = new ASCIIHexEncoder(str);
			str->reset();
			col = 0;
			writePS("[<");
			do
			{
				do
				{
					c = str->getChar();
				}
				while (c == '\n' || c == '\r');
				if (c == '>' || c == EOF)
					break;
				writePSChar((char)c);
				++col;
				// each line is: "<...data...><eol>"
				// so max data length = 255 - 4 = 251
				// but make it 240 just to be safe
				// chunks are 2 bytes each, so we need to stop on an even col number
				if (col == 240)
				{
					writePS(">\n<");
					col = 0;
				}
			}
			while (c != '>' && c != EOF);
			writePS(">]\n");
			writePS("0\n");
			str->close();
			delete str;
		}
		else
		{
			// set up to use the array already created by setupImages()
			writePSFmt("ImData_{}_{} 0\n", ref->getRefNum(), ref->getRefGen());
		}
	}

	// image/imagemask command
	if ((inType3Char || preload) && !colorMap)
		writePSFmt("{} {} {} [{} 0 0 {} 0 {}] pdfImM1a\n", width, height, invert ? "true" : "false", width, -height, height);
	else if (colorMap)
		writePSFmt("{} {} 8 [{} 0 0 {} 0 {}] pdfIm1\n", width, height, width, -height, height);
	else
		writePSFmt("{} {} {} [{} 0 0 {} 0 {}] pdfImM1\n", width, height, invert ? "true" : "false", width, -height, height);

	// image data
	if (!((inType3Char || preload) && !colorMap))
	{
		if (colorMap)
		{
			// set up to process the data stream
			imgStr = new ImageStream(str, width, colorMap->getNumPixelComps(), colorMap->getBits());
			imgStr->reset();

			// process the data stream
			i = 0;
			for (y = 0; y < height; ++y)
			{
				// write the line
				for (x = 0; x < width; ++x)
				{
					imgStr->getPixel(pixBuf);
					colorMap->getGray(pixBuf, &gray, state->getRenderingIntent());
					writePSFmt("{:02x}", colToByte(gray));
					if (++i == 32)
					{
						writePSChar('\n');
						i = 0;
					}
				}
			}
			if (i != 0)
				writePSChar('\n');
			str->close();
			delete imgStr;

			// imagemask
		}
		else
		{
			str->reset();
			i = 0;
			for (y = 0; y < height; ++y)
			{
				for (x = 0; x < width; x += 8)
				{
					writePSFmt("{:02x}", str->getChar() & 0xff);
					if (++i == 32)
					{
						writePSChar('\n');
						i = 0;
					}
				}
			}
			if (i != 0)
				writePSChar('\n');
			str->close();
		}
	}
}

void PSOutputDev::doImageL1Sep(GfxState* state, GfxImageColorMap* colorMap, bool invert, bool inlineImg, Stream* str, int width, int height, int len)
{
	ImageStream* imgStr;
	uint8_t*     lineBuf;
	uint8_t      pixBuf[gfxColorMaxComps];
	GfxCMYK      cmyk;
	int          x, y, i, comp;

	// width, height, matrix, bits per component
	writePSFmt("{} {} 8 [{} 0 0 {} 0 {}] pdfIm1Sep\n", width, height, width, -height, height);

	// allocate a line buffer
	lineBuf = (uint8_t*)gmallocn(width, 4);

	// set up to process the data stream
	imgStr = new ImageStream(str, width, colorMap->getNumPixelComps(), colorMap->getBits());
	imgStr->reset();

	// process the data stream
	i = 0;
	for (y = 0; y < height; ++y)
	{
		// read the line
		for (x = 0; x < width; ++x)
		{
			imgStr->getPixel(pixBuf);
			colorMap->getCMYK(pixBuf, &cmyk, state->getRenderingIntent());
			lineBuf[4 * x + 0] = colToByte(cmyk.c);
			lineBuf[4 * x + 1] = colToByte(cmyk.m);
			lineBuf[4 * x + 2] = colToByte(cmyk.y);
			lineBuf[4 * x + 3] = colToByte(cmyk.k);
			addProcessColor(colToDbl(cmyk.c), colToDbl(cmyk.m), colToDbl(cmyk.y), colToDbl(cmyk.k));
		}

		// write one line of each color component
		for (comp = 0; comp < 4; ++comp)
		{
			for (x = 0; x < width; ++x)
			{
				writePSFmt("{:02x}", lineBuf[4 * x + comp]);
				if (++i == 32)
				{
					writePSChar('\n');
					i = 0;
				}
			}
		}
	}

	if (i != 0)
		writePSChar('\n');

	str->close();
	delete imgStr;
	gfree(lineBuf);
}

void PSOutputDev::doImageL2(Object* ref, GfxState* state, GfxImageColorMap* colorMap, bool invert, bool inlineImg, Stream* str, int width, int height, int len, int* maskColors, Stream* maskStr, int maskWidth, int maskHeight, bool maskInvert)
{
	Stream*                  str2;
	int                      n, numComps;
	bool                     useLZW, useRLE, useASCII, useASCIIHex, useCompressed;
	GfxSeparationColorSpace* sepCS;
	GfxColor                 color;
	GfxCMYK                  cmyk;
	char                     buf[4096];
	int                      c, col, i;

	// color key masking
	if (maskColors && colorMap && !inlineImg)
	{
		// can't read the stream twice for inline images -- but masking
		// isn't allowed with inline images anyway
		convertColorKeyMaskToClipRects(colorMap, str, width, height, maskColors);

		// explicit masking
	}
	else if (maskStr)
	{
		convertExplicitMaskToClipRects(maskStr, maskWidth, maskHeight, maskInvert);
	}

	// color space
	if (colorMap && !(level == psLevel2Gray || level == psLevel3Gray))
	{
		dumpColorSpaceL2(state, colorMap->getColorSpace(), false, true, false);
		writePS(" setcolorspace\n");
	}

	useASCIIHex = globalParams->getPSASCIIHex();

	// set up the image data
	if (mode == psModeForm || inType3Char || preload)
	{
		if (inlineImg)
		{
			// create an array
			str2 = new FixedLengthEncoder(str, len);
			if (colorMap && (level == psLevel2Gray || level == psLevel3Gray))
				str2 = new GrayRecoder(str2, width, height, colorMap);
			if (globalParams->getPSLZW())
				str2 = new LZWEncoder(str2);
			else
				str2 = new RunLengthEncoder(str2);
			if (useASCIIHex)
				str2 = new ASCIIHexEncoder(str2);
			else
				str2 = new ASCII85Encoder(str2);
			str2->reset();
			col = 0;
			writePS((char*)(useASCIIHex ? "[<" : "[<~"));
			do
			{
				do
				{
					c = str2->getChar();
				}
				while (c == '\n' || c == '\r');
				if (c == (useASCIIHex ? '>' : '~') || c == EOF)
					break;
				if (c == 'z')
				{
					writePSChar((char)c);
					++col;
				}
				else
				{
					writePSChar((char)c);
					++col;
					for (i = 1; i <= (useASCIIHex ? 1 : 4); ++i)
					{
						do
						{
							c = str2->getChar();
						}
						while (c == '\n' || c == '\r');
						if (c == (useASCIIHex ? '>' : '~') || c == EOF)
							break;
						writePSChar((char)c);
						++col;
					}
				}
				// each line is: "<~...data...~><eol>"
				// so max data length = 255 - 6 = 249
				// chunks are 1 or 5 bytes each, so we have to stop at 245
				// but make it 240 just to be safe
				if (col > 240)
				{
					writePS((char*)(useASCIIHex ? ">\n<" : "~>\n<~"));
					col = 0;
				}
			}
			while (c != (useASCIIHex ? '>' : '~') && c != EOF);
			writePS((char*)(useASCIIHex ? ">\n" : "~>\n"));
			// add an extra entry because the LZWDecode/RunLengthDecode
			// filter may read past the end
			writePS("<>]\n");
			writePS("0\n");
			str2->close();
			delete str2;
		}
		else
		{
			// set up to use the array already created by setupImages()
			writePSFmt("ImData_{}_{} 0\n", ref->getRefNum(), ref->getRefGen());
		}
	}

	// image dictionary
	writePS("<<\n  /ImageType 1\n");

	// width, height, matrix, bits per component
	writePSFmt("  /Width {}\n", width);
	writePSFmt("  /Height {}\n", height);
	writePSFmt("  /ImageMatrix [{} 0 0 {} 0 {}]\n", width, -height, height);
	if (colorMap && (colorMap->getColorSpace()->getMode() == csDeviceN || level == psLevel2Gray || level == psLevel3Gray))
		writePS("  /BitsPerComponent 8\n");
	else
		writePSFmt("  /BitsPerComponent {}\n", colorMap ? colorMap->getBits() : 1);

	// decode
	if (colorMap)
	{
		writePS("  /Decode [");
		if ((level == psLevel2Sep || level == psLevel3Sep) && colorMap->getColorSpace()->getMode() == csSeparation)
		{
			// this matches up with the code in the pdfImSep operator
			n = (1 << colorMap->getBits()) - 1;
			writePSFmt("{} {}", Round4(colorMap->getDecodeLow(0) * n), Round4(colorMap->getDecodeHigh(0) * n));
		}
		else if (level == psLevel2Gray || level == psLevel3Gray)
		{
			writePS("0 1");
		}
		else if (colorMap->getColorSpace()->getMode() == csDeviceN)
		{
			numComps = ((GfxDeviceNColorSpace*)colorMap->getColorSpace())->getAlt()->getNComps();
			for (i = 0; i < numComps; ++i)
			{
				if (i > 0)
					writePS(" ");
				writePS("0 1");
			}
		}
		else
		{
			numComps = colorMap->getNumPixelComps();
			for (i = 0; i < numComps; ++i)
			{
				if (i > 0)
					writePS(" ");
				writePSFmt("{} {}", Round4(colorMap->getDecodeLow(i)), Round4(colorMap->getDecodeHigh(i)));
			}
		}
		writePS("]\n");
	}
	else
	{
		writePSFmt("  /Decode [{} {}]\n", invert ? 1 : 0, invert ? 0 : 1);
	}

	// data source
	if (mode == psModeForm || inType3Char || preload)
		writePS("  /DataSource { pdfImStr }\n");
	else
		writePS("  /DataSource currentfile\n");

	// filters
	std::string s;
	if ((mode == psModeForm || inType3Char || preload) && globalParams->getPSUncompressPreloadedImages())
	{
		useLZW = useRLE = false;
		useCompressed   = false;
		useASCII        = false;
	}
	else
	{
		s = str->getPSFilter((level < psLevel2) ? 1 : ((level < psLevel3) ? 2 : 3), "    ", !inlineImg);
		if ((colorMap && (colorMap->getColorSpace()->getMode() == csDeviceN || level == psLevel2Gray || level == psLevel3Gray)) || inlineImg || s.empty())
		{
			if (globalParams->getPSLZW())
			{
				useLZW = true;
				useRLE = false;
			}
			else
			{
				useRLE = true;
				useLZW = false;
			}
			useASCII      = !(mode == psModeForm || inType3Char || preload);
			useCompressed = false;
		}
		else if (globalParams->getPSLZW() && !str->hasStrongCompression())
		{
			useRLE        = false;
			useLZW        = true;
			useASCII      = !(mode == psModeForm || inType3Char || preload);
			useCompressed = false;
		}
		else
		{
			useLZW = useRLE = false;
			useASCII        = str->isBinary() && !(mode == psModeForm || inType3Char || preload);
			useCompressed   = true;
		}
	}
	if (useASCII)
		writePSFmt("    /ASCII{}Decode filter\n", useASCIIHex ? "Hex" : "85");
	if (useLZW)
		writePS("    /LZWDecode filter\n");
	else if (useRLE)
		writePS("    /RunLengthDecode filter\n");
	if (useCompressed)
		writePS(s.c_str());

	if (mode == psModeForm || inType3Char || preload)
	{
		// end of image dictionary
		writePSFmt(">>\n{}\n", colorMap ? "image" : "imagemask");

		// get rid of the array and index
		writePS("pop pop\n");
	}
	else
	{
		// cut off inline image streams at appropriate length
		if (inlineImg)
			str = new FixedLengthEncoder(str, len);
		else if (useCompressed)
			str = str->getUndecodedStream();

		// recode to grayscale
		if (colorMap && (level == psLevel2Gray || level == psLevel3Gray))
		{
			str = new GrayRecoder(str, width, height, colorMap);

			// recode DeviceN data
		}
		else if (colorMap && colorMap->getColorSpace()->getMode() == csDeviceN)
		{
			str = new DeviceNRecoder(str, width, height, colorMap);
		}

		// add LZWEncode/RunLengthEncode and ASCIIHex/85 encode filters
		if (useLZW)
			str = new LZWEncoder(str);
		else if (useRLE)
			str = new RunLengthEncoder(str);
		if (useASCII)
		{
			if (useASCIIHex)
				str = new ASCIIHexEncoder(str);
			else
				str = new ASCII85Encoder(str);
		}

		// end of image dictionary
		writePS(">>\n");
#if OPI_SUPPORT
		if (opi13Nest)
		{
			if (inlineImg)
			{
				// this can't happen -- OPI dictionaries are in XObjects
				error(errSyntaxError, -1, "OPI in inline image");
				n = 0;
			}
			else
			{
				// need to read the stream to count characters -- the length
				// is data-dependent (because of ASCII and LZW/RunLength
				// filters)
				str->reset();
				n = 0;
				do
				{
					i = str->discardChars(4096);
					n += i;
				}
				while (i == 4096);
				str->close();
			}
			// +6/7 for "pdfIm\n" / "pdfImM\n"
			// +8 for newline + trailer
			n += colorMap ? 14 : 15;
			writePSFmt("%%BeginData: {} Hex Bytes\n", n);
		}
#endif
		if ((level == psLevel2Sep || level == psLevel3Sep) && colorMap && colorMap->getColorSpace()->getMode() == csSeparation)
		{
			color.c[0] = gfxColorComp1;
			sepCS      = (GfxSeparationColorSpace*)colorMap->getColorSpace();
			sepCS->getCMYK(&color, &cmyk, state->getRenderingIntent());
			writePSFmt("{} {} {} {} ({}) pdfImSep\n", Round4(colToDbl(cmyk.c)), Round4(colToDbl(cmyk.m)), Round4(colToDbl(cmyk.y)), Round4(colToDbl(cmyk.k)), sepCS->getName());
		}
		else
		{
			writePSFmt("{}\n", colorMap ? "pdfIm" : "pdfImM");
		}

		// copy the stream data
		str->reset();
		while ((n = str->getBlock(buf, sizeof(buf))) > 0)
			writePSBlock(buf, n);
		str->close();

		// add newline and trailer to the end
		writePSChar('\n');
		writePS("%-EOD-\n");
#if OPI_SUPPORT
		if (opi13Nest)
			writePS("%%EndData\n");
#endif

		// delete encoders
		if (useLZW || useRLE || useASCII || inlineImg)
			delete str;
	}

	if ((maskColors && colorMap && !inlineImg) || maskStr)
		writePS("pdfImClipEnd\n");
}

// Convert color key masking to a clipping region consisting of a
// sequence of clip rectangles.
void PSOutputDev::convertColorKeyMaskToClipRects(GfxImageColorMap* colorMap, Stream* str, int width, int height, int* maskColors)
{
	ImageStream*      imgStr;
	uint8_t*          line;
	PSOutImgClipRect *rects0, *rects1, *rectsTmp, *rectsOut;
	int               rects0Len, rects1Len, rectsSize, rectsOutLen, rectsOutSize;
	bool              emitRect, addRect, extendRect;
	int               numComps, i, j, x0, x1, y;

	numComps = colorMap->getNumPixelComps();
	imgStr   = new ImageStream(str, width, numComps, colorMap->getBits());
	imgStr->reset();
	rects0Len = rects1Len = rectsOutLen = 0;
	rectsSize = rectsOutSize = 64;
	rects0                   = (PSOutImgClipRect*)gmallocn(rectsSize, sizeof(PSOutImgClipRect));
	rects1                   = (PSOutImgClipRect*)gmallocn(rectsSize, sizeof(PSOutImgClipRect));
	rectsOut                 = (PSOutImgClipRect*)gmallocn(rectsOutSize, sizeof(PSOutImgClipRect));
	for (y = 0; y < height; ++y)
	{
		if (!(line = imgStr->getLine()))
			break;
		i         = 0;
		rects1Len = 0;
		for (x0 = 0; x0 < width; ++x0)
		{
			for (j = 0; j < numComps; ++j)
				if (line[x0 * numComps + j] < maskColors[2 * j] || line[x0 * numComps + j] > maskColors[2 * j + 1])
					break;
			if (j < numComps)
				break;
		}
		for (x1 = x0; x1 < width; ++x1)
		{
			for (j = 0; j < numComps; ++j)
				if (line[x1 * numComps + j] < maskColors[2 * j] || line[x1 * numComps + j] > maskColors[2 * j + 1])
					break;
			if (j == numComps)
				break;
		}
		while (x0 < width || i < rects0Len)
		{
			emitRect = addRect = extendRect = false;
			if (x0 >= width)
				emitRect = true;
			else if (i >= rects0Len)
				addRect = true;
			else if (rects0[i].x0 < x0)
				emitRect = true;
			else if (x0 < rects0[i].x0)
				addRect = true;
			else if (rects0[i].x1 == x1)
				extendRect = true;
			else
				emitRect = addRect = true;
			if (emitRect)
			{
				if (rectsOutLen == rectsOutSize)
				{
					rectsOutSize *= 2;
					rectsOut = (PSOutImgClipRect*)greallocn(rectsOut, rectsOutSize, sizeof(PSOutImgClipRect));
				}
				rectsOut[rectsOutLen].x0 = rects0[i].x0;
				rectsOut[rectsOutLen].x1 = rects0[i].x1;
				rectsOut[rectsOutLen].y0 = height - y - 1;
				rectsOut[rectsOutLen].y1 = height - rects0[i].y0 - 1;
				++rectsOutLen;
				++i;
			}
			if (addRect || extendRect)
			{
				if (rects1Len == rectsSize)
				{
					rectsSize *= 2;
					rects0 = (PSOutImgClipRect*)greallocn(rects0, rectsSize, sizeof(PSOutImgClipRect));
					rects1 = (PSOutImgClipRect*)greallocn(rects1, rectsSize, sizeof(PSOutImgClipRect));
				}
				rects1[rects1Len].x0 = x0;
				rects1[rects1Len].x1 = x1;
				if (addRect)
					rects1[rects1Len].y0 = y;
				if (extendRect)
				{
					rects1[rects1Len].y0 = rects0[i].y0;
					++i;
				}
				++rects1Len;
				for (x0 = x1; x0 < width; ++x0)
				{
					for (j = 0; j < numComps; ++j)
						if (line[x0 * numComps + j] < maskColors[2 * j] || line[x0 * numComps + j] > maskColors[2 * j + 1])
							break;
					if (j < numComps)
						break;
				}
				for (x1 = x0; x1 < width; ++x1)
				{
					for (j = 0; j < numComps; ++j)
						if (line[x1 * numComps + j] < maskColors[2 * j] || line[x1 * numComps + j] > maskColors[2 * j + 1])
							break;
					if (j == numComps)
						break;
				}
			}
		}
		rectsTmp  = rects0;
		rects0    = rects1;
		rects1    = rectsTmp;
		i         = rects0Len;
		rects0Len = rects1Len;
		rects1Len = i;
	}
	for (i = 0; i < rects0Len; ++i)
	{
		if (rectsOutLen == rectsOutSize)
		{
			rectsOutSize *= 2;
			rectsOut = (PSOutImgClipRect*)greallocn(rectsOut, rectsOutSize, sizeof(PSOutImgClipRect));
		}
		rectsOut[rectsOutLen].x0 = rects0[i].x0;
		rectsOut[rectsOutLen].x1 = rects0[i].x1;
		rectsOut[rectsOutLen].y0 = height - y - 1;
		rectsOut[rectsOutLen].y1 = height - rects0[i].y0 - 1;
		++rectsOutLen;
	}
	writePSFmt("{} {}\n", width, height);
	for (i = 0; i < rectsOutLen; ++i)
		writePSFmt("{} {} {} {} pr\n", rectsOut[i].x0, rectsOut[i].y0, rectsOut[i].x1 - rectsOut[i].x0, rectsOut[i].y1 - rectsOut[i].y0);
	writePS("pop pop pdfImClip\n");
	gfree(rectsOut);
	gfree(rects0);
	gfree(rects1);
	delete imgStr;
	str->close();
}

// Convert an explicit mask image to a clipping region consisting of a
// sequence of clip rectangles.
void PSOutputDev::convertExplicitMaskToClipRects(Stream* maskStr, int maskWidth, int maskHeight, bool maskInvert)
{
	ImageStream*      imgStr;
	uint8_t*          line;
	PSOutImgClipRect *rects0, *rects1, *rectsTmp, *rectsOut;
	int               rects0Len, rects1Len, rectsSize, rectsOutLen, rectsOutSize;
	bool              emitRect, addRect, extendRect;
	int               i, x0, x1, y, maskXor;

	imgStr = new ImageStream(maskStr, maskWidth, 1, 1);
	imgStr->reset();
	rects0Len = rects1Len = rectsOutLen = 0;
	rectsSize = rectsOutSize = 64;
	rects0                   = (PSOutImgClipRect*)gmallocn(rectsSize, sizeof(PSOutImgClipRect));
	rects1                   = (PSOutImgClipRect*)gmallocn(rectsSize, sizeof(PSOutImgClipRect));
	rectsOut                 = (PSOutImgClipRect*)gmallocn(rectsOutSize, sizeof(PSOutImgClipRect));
	maskXor                  = maskInvert ? 1 : 0;
	for (y = 0; y < maskHeight; ++y)
	{
		if (!(line = imgStr->getLine()))
			break;
		i         = 0;
		rects1Len = 0;
		for (x0 = 0; x0 < maskWidth && (line[x0] ^ maskXor); ++x0)
			;
		for (x1 = x0; x1 < maskWidth && !(line[x1] ^ maskXor); ++x1)
			;
		while (x0 < maskWidth || i < rects0Len)
		{
			emitRect = addRect = extendRect = false;
			if (x0 >= maskWidth)
				emitRect = true;
			else if (i >= rects0Len)
				addRect = true;
			else if (rects0[i].x0 < x0)
				emitRect = true;
			else if (x0 < rects0[i].x0)
				addRect = true;
			else if (rects0[i].x1 == x1)
				extendRect = true;
			else
				emitRect = addRect = true;
			if (emitRect)
			{
				if (rectsOutLen == rectsOutSize)
				{
					rectsOutSize *= 2;
					rectsOut = (PSOutImgClipRect*)greallocn(rectsOut, rectsOutSize, sizeof(PSOutImgClipRect));
				}
				rectsOut[rectsOutLen].x0 = rects0[i].x0;
				rectsOut[rectsOutLen].x1 = rects0[i].x1;
				rectsOut[rectsOutLen].y0 = maskHeight - y - 1;
				rectsOut[rectsOutLen].y1 = maskHeight - rects0[i].y0 - 1;
				++rectsOutLen;
				++i;
			}
			if (addRect || extendRect)
			{
				if (rects1Len == rectsSize)
				{
					rectsSize *= 2;
					rects0 = (PSOutImgClipRect*)greallocn(rects0, rectsSize, sizeof(PSOutImgClipRect));
					rects1 = (PSOutImgClipRect*)greallocn(rects1, rectsSize, sizeof(PSOutImgClipRect));
				}
				rects1[rects1Len].x0 = x0;
				rects1[rects1Len].x1 = x1;
				if (addRect)
					rects1[rects1Len].y0 = y;
				if (extendRect)
				{
					rects1[rects1Len].y0 = rects0[i].y0;
					++i;
				}
				++rects1Len;
				for (x0 = x1; x0 < maskWidth && (line[x0] ^ maskXor); ++x0)
					;
				for (x1 = x0; x1 < maskWidth && !(line[x1] ^ maskXor); ++x1)
					;
			}
		}
		rectsTmp  = rects0;
		rects0    = rects1;
		rects1    = rectsTmp;
		i         = rects0Len;
		rects0Len = rects1Len;
		rects1Len = i;
	}
	for (i = 0; i < rects0Len; ++i)
	{
		if (rectsOutLen == rectsOutSize)
		{
			rectsOutSize *= 2;
			rectsOut = (PSOutImgClipRect*)greallocn(rectsOut, rectsOutSize, sizeof(PSOutImgClipRect));
		}
		rectsOut[rectsOutLen].x0 = rects0[i].x0;
		rectsOut[rectsOutLen].x1 = rects0[i].x1;
		rectsOut[rectsOutLen].y0 = maskHeight - y - 1;
		rectsOut[rectsOutLen].y1 = maskHeight - rects0[i].y0 - 1;
		++rectsOutLen;
	}
	writePSFmt("{} {}\n", maskWidth, maskHeight);
	for (i = 0; i < rectsOutLen; ++i)
		writePSFmt("{} {} {} {} pr\n", rectsOut[i].x0, rectsOut[i].y0, rectsOut[i].x1 - rectsOut[i].x0, rectsOut[i].y1 - rectsOut[i].y0);
	writePS("pop pop pdfImClip\n");
	gfree(rectsOut);
	gfree(rects0);
	gfree(rects1);
	delete imgStr;
	maskStr->close();
}

//~ this doesn't currently support OPI
void PSOutputDev::doImageL3(Object* ref, GfxState* state, GfxImageColorMap* colorMap, bool invert, bool inlineImg, Stream* str, int width, int height, int len, int* maskColors, Stream* maskStr, int maskWidth, int maskHeight, bool maskInvert)
{
	Stream*                  str2;
	std::string              s;
	int                      n, numComps;
	bool                     useLZW, useRLE, useASCII, useASCIIHex, useCompressed;
	bool                     maskUseLZW, maskUseRLE, maskUseASCII, maskUseCompressed;
	std::string              maskFilters;
	GfxSeparationColorSpace* sepCS;
	GfxColor                 color;
	GfxCMYK                  cmyk;
	char                     buf[4096];
	int                      c;
	int                      col, i;

	useASCIIHex = globalParams->getPSASCIIHex();
	useLZW = useRLE = useASCII = useCompressed = false; // make gcc happy
	maskUseLZW = maskUseRLE = maskUseASCII = false;     // make gcc happy
	maskUseCompressed                      = false;     // make gcc happy
	maskFilters                            = nullptr;   // make gcc happy

	// explicit masking
	// -- this also converts color key masking in grayscale mode
	if (maskStr || (maskColors && colorMap && level == psLevel3Gray))
	{
		// mask data source
		if (maskColors && colorMap && level == psLevel3Gray)
		{
			s = nullptr;
			if (mode == psModeForm || inType3Char || preload)
			{
				if (globalParams->getPSUncompressPreloadedImages())
				{
					maskUseLZW = maskUseRLE = false;
				}
				else if (globalParams->getPSLZW())
				{
					maskUseLZW = true;
					maskUseRLE = false;
				}
				else
				{
					maskUseRLE = true;
					maskUseLZW = false;
				}
				maskUseASCII      = false;
				maskUseCompressed = false;
			}
			else
			{
				if (globalParams->getPSLZW())
				{
					maskUseLZW = true;
					maskUseRLE = false;
				}
				else
				{
					maskUseRLE = true;
					maskUseLZW = false;
				}
				maskUseASCII = true;
			}
			maskUseCompressed = false;
			maskWidth         = width;
			maskHeight        = height;
			maskInvert        = false;
		}
		else if ((mode == psModeForm || inType3Char || preload) && globalParams->getPSUncompressPreloadedImages())
		{
			maskUseLZW = maskUseRLE = false;
			maskUseCompressed       = false;
			maskUseASCII            = false;
		}
		else
		{
			s = maskStr->getPSFilter(3, "  ", !inlineImg);
			if (s.empty())
			{
				if (globalParams->getPSLZW())
				{
					maskUseLZW = true;
					maskUseRLE = false;
				}
				else
				{
					maskUseRLE = true;
					maskUseLZW = false;
				}
				maskUseASCII      = !(mode == psModeForm || inType3Char || preload);
				maskUseCompressed = false;
			}
			else if (globalParams->getPSLZW() && !maskStr->hasStrongCompression())
			{
				maskUseRLE        = false;
				maskUseLZW        = true;
				maskUseASCII      = !(mode == psModeForm || inType3Char || preload);
				maskUseCompressed = false;
			}
			else
			{
				maskUseLZW = maskUseRLE = false;
				maskUseASCII            = maskStr->isBinary() && !(mode == psModeForm || inType3Char || preload);
				maskUseCompressed       = true;
			}
		}
		maskFilters.clear();
		if (maskUseASCII)
			maskFilters += fmt::format("    /ASCII{}Decode filter\n", useASCIIHex ? "Hex" : "85");
		if (maskUseLZW)
			maskFilters += "    /LZWDecode filter\n";
		else if (maskUseRLE)
			maskFilters += "    /RunLengthDecode filter\n";
		if (maskUseCompressed)
			maskFilters += s;

		if (mode == psModeForm || inType3Char || preload)
		{
			writePSFmt("MaskData_{}_{} pdfMaskInit\n", ref->getRefNum(), ref->getRefGen());
		}
		else
		{
			writePS("currentfile\n");
			writePS(maskFilters.c_str());
			writePS("pdfMask\n");

			// add the ColorKeyToMask filter
			if (maskColors && colorMap && level == psLevel3Gray)
				maskStr = new ColorKeyToMaskEncoder(str, width, height, colorMap, maskColors);

			// add LZWEncode/RunLengthEncode and ASCIIHex/85 encode filters
			if (maskUseCompressed)
				maskStr = maskStr->getUndecodedStream();
			if (maskUseLZW)
				maskStr = new LZWEncoder(maskStr);
			else if (maskUseRLE)
				maskStr = new RunLengthEncoder(maskStr);
			if (maskUseASCII)
			{
				if (useASCIIHex)
					maskStr = new ASCIIHexEncoder(maskStr);
				else
					maskStr = new ASCII85Encoder(maskStr);
			}

			// copy the stream data
			maskStr->reset();
			while ((n = maskStr->getBlock(buf, sizeof(buf))) > 0)
				writePSBlock(buf, n);
			maskStr->close();
			writePSChar('\n');
			writePS("%-EOD-\n");

			// delete encoders
			if (maskUseLZW || maskUseRLE || maskUseASCII)
				delete maskStr;
		}
	}

	// color space
	if (colorMap && level != psLevel3Gray)
	{
		dumpColorSpaceL2(state, colorMap->getColorSpace(), false, true, false);
		writePS(" setcolorspace\n");
	}

	// set up the image data
	if (mode == psModeForm || inType3Char || preload)
	{
		if (inlineImg)
		{
			// create an array
			str2 = new FixedLengthEncoder(str, len);
			if (colorMap && level == psLevel3Gray)
				str2 = new GrayRecoder(str2, width, height, colorMap);
			if (globalParams->getPSLZW())
				str2 = new LZWEncoder(str2);
			else
				str2 = new RunLengthEncoder(str2);
			if (useASCIIHex)
				str2 = new ASCIIHexEncoder(str2);
			else
				str2 = new ASCII85Encoder(str2);
			str2->reset();
			col = 0;
			writePS((char*)(useASCIIHex ? "[<" : "[<~"));
			do
			{
				do
				{
					c = str2->getChar();
				}
				while (c == '\n' || c == '\r');
				if (c == (useASCIIHex ? '>' : '~') || c == EOF)
					break;
				if (c == 'z')
				{
					writePSChar((char)c);
					++col;
				}
				else
				{
					writePSChar((char)c);
					++col;
					for (i = 1; i <= (useASCIIHex ? 1 : 4); ++i)
					{
						do
						{
							c = str2->getChar();
						}
						while (c == '\n' || c == '\r');
						if (c == (useASCIIHex ? '>' : '~') || c == EOF)
							break;
						writePSChar((char)c);
						++col;
					}
				}
				// each line is: "<~...data...~><eol>"
				// so max data length = 255 - 6 = 249
				// chunks are 1 or 5 bytes each, so we have to stop at 245
				// but make it 240 just to be safe
				if (col > 240)
				{
					writePS((char*)(useASCIIHex ? ">\n<" : "~>\n<~"));
					col = 0;
				}
			}
			while (c != (useASCIIHex ? '>' : '~') && c != EOF);
			writePS((char*)(useASCIIHex ? ">\n" : "~>\n"));
			// add an extra entry because the LZWDecode/RunLengthDecode
			// filter may read past the end
			writePS("<>]\n");
			writePS("0\n");
			str2->close();
			delete str2;
		}
		else
		{
			// set up to use the array already created by setupImages()
			writePSFmt("ImData_{}_{} 0\n", ref->getRefNum(), ref->getRefGen());
		}
	}

	// explicit masking
	if (maskStr || (maskColors && colorMap && level == psLevel3Gray))
	{
		writePS("<<\n  /ImageType 3\n");
		writePS("  /InterleaveType 3\n");
		writePS("  /DataDict\n");
	}

	// image (data) dictionary
	writePSFmt("<<\n  /ImageType {}\n", (maskColors && colorMap && level != psLevel3Gray) ? 4 : 1);

	// color key masking
	if (maskColors && colorMap && level != psLevel3Gray)
	{
		writePS("  /MaskColor [\n");
		numComps = colorMap->getNumPixelComps();
		for (i = 0; i < 2 * numComps; i += 2)
			writePSFmt("    {} {}\n", maskColors[i], maskColors[i + 1]);
		writePS("  ]\n");
	}

	// width, height, matrix, bits per component
	writePSFmt("  /Width {}\n", width);
	writePSFmt("  /Height {}\n", height);
	writePSFmt("  /ImageMatrix [{} 0 0 {} 0 {}]\n", width, -height, height);
	if (colorMap && level == psLevel3Gray)
		writePS("  /BitsPerComponent 8\n");
	else
		writePSFmt("  /BitsPerComponent {}\n", colorMap ? colorMap->getBits() : 1);

	// decode
	if (colorMap)
	{
		writePS("  /Decode [");
		if (level == psLevel3Sep && colorMap->getColorSpace()->getMode() == csSeparation)
		{
			// this matches up with the code in the pdfImSep operator
			n = (1 << colorMap->getBits()) - 1;
			writePSFmt("{} {}", Round4(colorMap->getDecodeLow(0) * n), Round4(colorMap->getDecodeHigh(0) * n));
		}
		else if (level == psLevel3Gray)
		{
			writePS("0 1");
		}
		else
		{
			numComps = colorMap->getNumPixelComps();
			for (i = 0; i < numComps; ++i)
			{
				if (i > 0)
					writePS(" ");
				writePSFmt("{} {}", Round4(colorMap->getDecodeLow(i)), Round4(colorMap->getDecodeHigh(i)));
			}
		}
		writePS("]\n");
	}
	else
	{
		writePSFmt("  /Decode [{} {}]\n", invert ? 1 : 0, invert ? 0 : 1);
	}

	// data source
	if (mode == psModeForm || inType3Char || preload)
		writePS("  /DataSource { pdfImStr }\n");
	else
		writePS("  /DataSource currentfile\n");

	// filters
	if ((mode == psModeForm || inType3Char || preload) && globalParams->getPSUncompressPreloadedImages())
	{
		useLZW = useRLE = false;
		useCompressed   = false;
		useASCII        = false;
	}
	else
	{
		s = str->getPSFilter(3, "    ", !inlineImg);
		if ((colorMap && level == psLevel3Gray) || inlineImg || s.empty())
		{
			if (globalParams->getPSLZW())
			{
				useLZW = true;
				useRLE = false;
			}
			else
			{
				useRLE = true;
				useLZW = false;
			}
			useASCII      = !(mode == psModeForm || inType3Char || preload);
			useCompressed = false;
		}
		else if (globalParams->getPSLZW() && !str->hasStrongCompression())
		{
			useRLE        = false;
			useLZW        = true;
			useASCII      = !(mode == psModeForm || inType3Char || preload);
			useCompressed = false;
		}
		else
		{
			useLZW = useRLE = false;
			useASCII        = str->isBinary() && !(mode == psModeForm || inType3Char || preload);
			useCompressed   = true;
		}
	}
	if (useASCII)
		writePSFmt("    /ASCII{}Decode filter\n", useASCIIHex ? "Hex" : "85");
	if (useLZW)
		writePS("    /LZWDecode filter\n");
	else if (useRLE)
		writePS("    /RunLengthDecode filter\n");
	if (useCompressed)
		writePS(s.c_str());

	// end of image (data) dictionary
	writePS(">>\n");

	// explicit masking
	if (maskStr || (maskColors && colorMap && level == psLevel3Gray))
	{
		writePS("  /MaskDict\n");
		writePS("<<\n");
		writePS("  /ImageType 1\n");
		writePSFmt("  /Width {}\n", maskWidth);
		writePSFmt("  /Height {}\n", maskHeight);
		writePSFmt("  /ImageMatrix [{} 0 0 {} 0 {}]\n", maskWidth, -maskHeight, maskHeight);
		writePS("  /BitsPerComponent 1\n");
		writePSFmt("  /Decode [{} {}]\n", maskInvert ? 1 : 0, maskInvert ? 0 : 1);

		// mask data source
		if (mode == psModeForm || inType3Char || preload)
		{
			writePS("  /DataSource {pdfMaskSrc}\n");
			writePS(maskFilters.c_str());
		}
		else
		{
			writePS("  /DataSource maskStream\n");
		}

		writePS(">>\n");
		writePS(">>\n");
	}

	if (mode == psModeForm || inType3Char || preload)
	{
		// image command
		writePSFmt("{}\n", colorMap ? "image" : "imagemask");
	}
	else

	    if (level == psLevel3Sep && colorMap && colorMap->getColorSpace()->getMode() == csSeparation)
	{
		color.c[0] = gfxColorComp1;
		sepCS      = (GfxSeparationColorSpace*)colorMap->getColorSpace();
		sepCS->getCMYK(&color, &cmyk, state->getRenderingIntent());
		writePSFmt("{} {} {} {} ({}) pdfImSep\n", Round4(colToDbl(cmyk.c)), Round4(colToDbl(cmyk.m)), Round4(colToDbl(cmyk.y)), Round4(colToDbl(cmyk.k)), sepCS->getName());
	}
	else
	{
		writePSFmt("{}\n", colorMap ? "pdfIm" : "pdfImM");
	}

	// get rid of the array and index
	if (mode == psModeForm || inType3Char || preload)
	{
		writePS("pop pop\n");

		// image data
	}
	else
	{
		// cut off inline image streams at appropriate length
		if (inlineImg)
			str = new FixedLengthEncoder(str, len);
		else if (useCompressed)
			str = str->getUndecodedStream();

		// recode to grayscale
		if (colorMap && level == psLevel3Gray)
			str = new GrayRecoder(str, width, height, colorMap);

		// add LZWEncode/RunLengthEncode and ASCIIHex/85 encode filters
		if (useLZW)
			str = new LZWEncoder(str);
		else if (useRLE)
			str = new RunLengthEncoder(str);
		if (useASCII)
		{
			if (useASCIIHex)
				str = new ASCIIHexEncoder(str);
			else
				str = new ASCII85Encoder(str);
		}

		// copy the stream data
		str->reset();
		while ((n = str->getBlock(buf, sizeof(buf))) > 0)
			writePSBlock(buf, n);
		str->close();

		// add newline and trailer to the end
		writePSChar('\n');
		writePS("%-EOD-\n");

		// delete encoders
		if (useLZW || useRLE || useASCII || inlineImg)
			delete str;
	}

	// close the mask stream
	if (maskStr || (maskColors && colorMap && level == psLevel3Gray))
	{
		if (!(mode == psModeForm || inType3Char || preload))
			writePS("pdfMaskEnd\n");
	}
}

void PSOutputDev::dumpColorSpaceL2(GfxState* state, GfxColorSpace* colorSpace, bool genXform, bool updateColors, bool map01)
{
	switch (colorSpace->getMode())
	{
	case csDeviceGray:
		dumpDeviceGrayColorSpace((GfxDeviceGrayColorSpace*)colorSpace, genXform, updateColors, map01);
		break;
	case csCalGray:
		dumpCalGrayColorSpace((GfxCalGrayColorSpace*)colorSpace, genXform, updateColors, map01);
		break;
	case csDeviceRGB:
		dumpDeviceRGBColorSpace((GfxDeviceRGBColorSpace*)colorSpace, genXform, updateColors, map01);
		break;
	case csCalRGB:
		dumpCalRGBColorSpace((GfxCalRGBColorSpace*)colorSpace, genXform, updateColors, map01);
		break;
	case csDeviceCMYK:
		dumpDeviceCMYKColorSpace((GfxDeviceCMYKColorSpace*)colorSpace, genXform, updateColors, map01);
		break;
	case csLab:
		dumpLabColorSpace((GfxLabColorSpace*)colorSpace, genXform, updateColors, map01);
		break;
	case csICCBased:
		dumpICCBasedColorSpace(state, (GfxICCBasedColorSpace*)colorSpace, genXform, updateColors, map01);
		break;
	case csIndexed:
		dumpIndexedColorSpace(state, (GfxIndexedColorSpace*)colorSpace, genXform, updateColors, map01);
		break;
	case csSeparation:
		dumpSeparationColorSpace(state, (GfxSeparationColorSpace*)colorSpace, genXform, updateColors, map01);
		break;
	case csDeviceN:
		if (level >= psLevel3)
			dumpDeviceNColorSpaceL3(state, (GfxDeviceNColorSpace*)colorSpace, genXform, updateColors, map01);
		else
			dumpDeviceNColorSpaceL2(state, (GfxDeviceNColorSpace*)colorSpace, genXform, updateColors, map01);
		break;
	case csPattern:
		//~ unimplemented
		break;
	}
}

void PSOutputDev::dumpDeviceGrayColorSpace(GfxDeviceGrayColorSpace* cs, bool genXform, bool updateColors, bool map01)
{
	writePS("/DeviceGray");
	if (genXform)
		writePS(" {}");
	if (updateColors)
		processColors |= psProcessBlack;
}

void PSOutputDev::dumpCalGrayColorSpace(GfxCalGrayColorSpace* cs, bool genXform, bool updateColors, bool map01)
{
	writePS("[/CIEBasedA <<\n");
	writePSFmt(" /DecodeA {{{} exp}} bind\n", Round4(cs->getGamma()));
	writePSFmt(" /MatrixA [{} {} {}]\n", Round4(cs->getWhiteX()), Round4(cs->getWhiteY()), Round4(cs->getWhiteZ()));
	writePSFmt(" /WhitePoint [{} {} {}]\n", Round4(cs->getWhiteX()), Round4(cs->getWhiteY()), Round4(cs->getWhiteZ()));
	writePSFmt(" /BlackPoint [{} {} {}]\n", Round4(cs->getBlackX()), Round4(cs->getBlackY()), Round4(cs->getBlackZ()));
	writePS(">>]");
	if (genXform)
		writePS(" {}");
	if (updateColors)
		processColors |= psProcessBlack;
}

void PSOutputDev::dumpDeviceRGBColorSpace(GfxDeviceRGBColorSpace* cs, bool genXform, bool updateColors, bool map01)
{
	writePS("/DeviceRGB");
	if (genXform)
		writePS(" {}");
	if (updateColors)
		processColors |= psProcessCMYK;
}

void PSOutputDev::dumpCalRGBColorSpace(GfxCalRGBColorSpace* cs, bool genXform, bool updateColors, bool map01)
{
	writePS("[/CIEBasedABC <<\n");
	writePSFmt(" /DecodeABC [{{{} exp}} bind {{{} exp}} bind {{{} exp}} bind]\n", Round4(cs->getGammaR()), Round4(cs->getGammaG()), Round4(cs->getGammaB()));
	writePSFmt(" /MatrixABC [{} {} {} {} {} {} {} {} {}]\n", Round4(cs->getMatrix()[0]), Round4(cs->getMatrix()[1]), Round4(cs->getMatrix()[2]), Round4(cs->getMatrix()[3]), Round4(cs->getMatrix()[4]), Round4(cs->getMatrix()[5]), Round4(cs->getMatrix()[6]), Round4(cs->getMatrix()[7]), Round4(cs->getMatrix()[8]));
	writePSFmt(" /WhitePoint [{} {} {}]\n", Round4(cs->getWhiteX()), Round4(cs->getWhiteY()), Round4(cs->getWhiteZ()));
	writePSFmt(" /BlackPoint [{} {} {}]\n", Round4(cs->getBlackX()), Round4(cs->getBlackY()), Round4(cs->getBlackZ()));
	writePS(">>]");
	if (genXform)
		writePS(" {}");
	if (updateColors)
		processColors |= psProcessCMYK;
}

void PSOutputDev::dumpDeviceCMYKColorSpace(GfxDeviceCMYKColorSpace* cs, bool genXform, bool updateColors, bool map01)
{
	writePS("/DeviceCMYK");
	if (genXform)
		writePS(" {}");
	if (updateColors)
		processColors |= psProcessCMYK;
}

void PSOutputDev::dumpLabColorSpace(GfxLabColorSpace* cs, bool genXform, bool updateColors, bool map01)
{
	writePS("[/CIEBasedABC <<\n");
	if (map01)
	{
		writePS(" /RangeABC [0 1 0 1 0 1]\n");
		writePSFmt(" /DecodeABC [{{100 mul 16 add 116 div}} bind {{{} mul {} add}} bind {{{} mul {} add}} bind]\n", Round4((cs->getAMax() - cs->getAMin()) / 500.0), Round4(cs->getAMin() / 500.0), Round4((cs->getBMax() - cs->getBMin()) / 200.0), Round4(cs->getBMin() / 200.0));
	}
	else
	{
		writePSFmt(" /RangeABC [0 100 {} {} {} {}]\n", Round4(cs->getAMin()), Round4(cs->getAMax()), Round4(cs->getBMin()), Round4(cs->getBMax()));
		writePS(" /DecodeABC [{16 add 116 div} bind {500 div} bind {200 div} bind]\n");
	}
	writePS(" /MatrixABC [1 1 1 1 0 0 0 0 -1]\n");
	writePS(" /DecodeLMN\n");
	writePS("   [{dup 6 29 div ge {dup dup mul mul}\n");
	writePSFmt("     {{4 29 div sub 108 841 div mul }} ifelse {} mul}} bind\n", Round4(cs->getWhiteX()));
	writePS("    {dup 6 29 div ge {dup dup mul mul}\n");
	writePSFmt("     {{4 29 div sub 108 841 div mul }} ifelse {} mul}} bind\n", Round4(cs->getWhiteY()));
	writePS("    {dup 6 29 div ge {dup dup mul mul}\n");
	writePSFmt("     {{4 29 div sub 108 841 div mul }} ifelse {} mul}} bind]\n", Round4(cs->getWhiteZ()));
	writePSFmt(" /WhitePoint [{} {} {}]\n", Round4(cs->getWhiteX()), Round4(cs->getWhiteY()), Round4(cs->getWhiteZ()));
	writePSFmt(" /BlackPoint [{} {} {}]\n", Round4(cs->getBlackX()), Round4(cs->getBlackY()), Round4(cs->getBlackZ()));
	writePS(">>]");
	if (genXform)
		writePS(" {}");
	if (updateColors)
		processColors |= psProcessCMYK;
}

void PSOutputDev::dumpICCBasedColorSpace(GfxState* state, GfxICCBasedColorSpace* cs, bool genXform, bool updateColors, bool map01)
{
	// there is no transform function to the alternate color space, so
	// we can use it directly
	dumpColorSpaceL2(state, cs->getAlt(), genXform, updateColors, false);
}

void PSOutputDev::dumpIndexedColorSpace(GfxState* state, GfxIndexedColorSpace* cs, bool genXform, bool updateColors, bool map01)
{
	GfxColorSpace*    baseCS;
	GfxLabColorSpace* labCS;
	uint8_t *         lookup, *p;
	double            x[gfxColorMaxComps], y[gfxColorMaxComps];
	double            low[gfxColorMaxComps], range[gfxColorMaxComps];
	GfxColor          color;
	GfxCMYK           cmyk;
	Function*         func;
	int               n, numComps, numAltComps;
	int               byte;
	int               i, j, k;

	baseCS = cs->getBase();
	writePS("[/Indexed ");
	dumpColorSpaceL2(state, baseCS, false, updateColors, true);
	n        = cs->getIndexHigh();
	numComps = baseCS->getNComps();
	lookup   = cs->getLookup();
	writePSFmt(" {} <\n", n);
	if (baseCS->getMode() == csDeviceN && level < psLevel3)
	{
		func = ((GfxDeviceNColorSpace*)baseCS)->getTintTransformFunc();
		baseCS->getDefaultRanges(low, range, cs->getIndexHigh());
		if (((GfxDeviceNColorSpace*)baseCS)->getAlt()->getMode() == csLab)
			labCS = (GfxLabColorSpace*)((GfxDeviceNColorSpace*)baseCS)->getAlt();
		else
			labCS = nullptr;
		numAltComps = ((GfxDeviceNColorSpace*)baseCS)->getAlt()->getNComps();
		p           = lookup;
		for (i = 0; i <= n; i += 8)
		{
			writePS("  ");
			for (j = i; j < i + 8 && j <= n; ++j)
			{
				for (k = 0; k < numComps; ++k)
					x[k] = low[k] + (*p++ / 255.0) * range[k];
				func->transform(x, y);
				if (labCS)
				{
					y[0] /= 100.0;
					y[1] = (y[1] - labCS->getAMin()) / (labCS->getAMax() - labCS->getAMin());
					y[2] = (y[2] - labCS->getBMin()) / (labCS->getBMax() - labCS->getBMin());
				}
				for (k = 0; k < numAltComps; ++k)
				{
					byte = (int)(y[k] * 255 + 0.5);
					if (byte < 0)
						byte = 0;
					else if (byte > 255)
						byte = 255;
					writePSFmt("{:02x}", byte);
				}
				if (updateColors)
				{
					color.c[0] = dblToCol(j);
					cs->getCMYK(&color, &cmyk, state->getRenderingIntent());
					addProcessColor(colToDbl(cmyk.c), colToDbl(cmyk.m), colToDbl(cmyk.y), colToDbl(cmyk.k));
				}
			}
			writePS("\n");
		}
	}
	else
	{
		for (i = 0; i <= n; i += 8)
		{
			writePS("  ");
			for (j = i; j < i + 8 && j <= n; ++j)
			{
				for (k = 0; k < numComps; ++k)
					writePSFmt("{:02x}", lookup[j * numComps + k]);
				if (updateColors)
				{
					color.c[0] = dblToCol(j);
					cs->getCMYK(&color, &cmyk, state->getRenderingIntent());
					addProcessColor(colToDbl(cmyk.c), colToDbl(cmyk.m), colToDbl(cmyk.y), colToDbl(cmyk.k));
				}
			}
			writePS("\n");
		}
	}
	writePS(">]");
	if (genXform)
		writePS(" {}");
}

void PSOutputDev::dumpSeparationColorSpace(GfxState* state, GfxSeparationColorSpace* cs, bool genXform, bool updateColors, bool map01)
{
	writePS("[/Separation ");
	writePSString(cs->getName());
	writePS(" ");
	dumpColorSpaceL2(state, cs->getAlt(), false, false, false);
	writePS("\n");
	cvtFunction(cs->getFunc());
	writePS("]");
	if (genXform) writePS(" {}");
	if (updateColors) addCustomColor(state, cs);
}

void PSOutputDev::dumpDeviceNColorSpaceL2(GfxState* state, GfxDeviceNColorSpace* cs, bool genXform, bool updateColors, bool map01)
{
	dumpColorSpaceL2(state, cs->getAlt(), false, updateColors, map01);
	if (genXform)
	{
		writePS(" ");
		cvtFunction(cs->getTintTransformFunc());
	}
}

void PSOutputDev::dumpDeviceNColorSpaceL3(GfxState* state, GfxDeviceNColorSpace* cs, bool genXform, bool updateColors, bool map01)
{
	writePS("[/DeviceN [\n");
	for (int i = 0; i < cs->getNComps(); ++i)
	{
		writePSString(cs->getColorantName(i));
		writePS("\n");
	}
	writePS("]\n");

	std::string tint;
	if ((tint = createDeviceNTintFunc(cs)).size())
	{
		writePS("/DeviceCMYK\n");
		writePS(tint.c_str());
	}
	else
	{
		dumpColorSpaceL2(state, cs->getAlt(), false, false, false);
		writePS("\n");
		cvtFunction(cs->getTintTransformFunc());
	}
	writePS("]");
	if (genXform)
		writePS(" {}");
	if (updateColors)
		addCustomColors(state, cs);
}

// If the DeviceN color space has a Colorants dictionary, and all of the colorants are one of: "None", "Cyan", "Magenta", "Yellow", "Black",
// or have an entry in the Colorants dict that maps to DeviceCMYK, then build a new tint function; else use the existing tint function.
std::string PSOutputDev::createDeviceNTintFunc(GfxDeviceNColorSpace* cs)
{
	Object    colorants, sepCSObj, funcObj, obj1;
	Function* func;
	double    sepIn;
	double    cmyk[gfxColorMaxComps][4];
	bool      first;

	Object* attrs = cs->getAttrs();
	if (!attrs->isDict()) return "";
	if (!attrs->dictLookup("Colorants", &colorants)->isDict())
	{
		colorants.free();
		return "";
	}
	for (int i = 0; i < cs->getNComps(); ++i)
	{
		const auto name = cs->getColorantName(i);
		if (name == "None")
		{
			cmyk[i][0] = cmyk[i][1] = cmyk[i][2] = cmyk[i][3] = 0;
		}
		else if (name == "Cyan")
		{
			cmyk[i][1] = cmyk[i][2] = cmyk[i][3] = 0;
			cmyk[i][0]                           = 1;
		}
		else if (name == "Magenta")
		{
			cmyk[i][0] = cmyk[i][2] = cmyk[i][3] = 0;
			cmyk[i][1]                           = 1;
		}
		else if (name == "Yellow")
		{
			cmyk[i][0] = cmyk[i][1] = cmyk[i][3] = 0;
			cmyk[i][2]                           = 1;
		}
		else if (name == "Black")
		{
			cmyk[i][0] = cmyk[i][1] = cmyk[i][2] = 0;
			cmyk[i][3]                           = 1;
		}
		else
		{
			colorants.dictLookup(name.c_str(), &sepCSObj);
			if (!sepCSObj.isArray() || sepCSObj.arrayGetLength() != 4)
			{
				sepCSObj.free();
				colorants.free();
				return "";
			}
			if (!sepCSObj.arrayGet(0, &obj1)->isName("Separation"))
			{
				obj1.free();
				sepCSObj.free();
				colorants.free();
				return "";
			}
			obj1.free();
			if (!sepCSObj.arrayGet(2, &obj1)->isName("DeviceCMYK"))
			{
				obj1.free();
				sepCSObj.free();
				colorants.free();
				return "";
			}
			obj1.free();
			sepCSObj.arrayGet(3, &funcObj);
			if (!(func = Function::parse(&funcObj, 1, 4)))
			{
				funcObj.free();
				sepCSObj.free();
				colorants.free();
				return "";
			}
			funcObj.free();
			sepIn = 1;
			func->transform(&sepIn, cmyk[i]);
			delete func;
			sepCSObj.free();
		}
	}
	colorants.free();

	std::string tint;
	tint += "{\n";
	for (int j = 0; j < 4; ++j)
	{
		// C, M, Y, K
		first = true;
		for (int i = 0; i < cs->getNComps(); ++i)
		{
			if (cmyk[i][j] != 0)
			{
				tint += fmt::format("{} index {:.4f} mul{}\n", j + cs->getNComps() - 1 - i, cmyk[i][j], first ? "" : " add");
				first = false;
			}
		}
		if (first)
			tint += "0\n";
	}
	tint += fmt::format("{} 4 roll\n", cs->getNComps() + 4);
	for (int i = 0; i < cs->getNComps(); ++i)
		tint += "pop\n";
	tint += "}\n";

	return tint;
}

#if OPI_SUPPORT
void PSOutputDev::opiBegin(GfxState* state, Dict* opiDict)
{
	Object dict;

	if (globalParams->getPSOPI())
	{
		opiDict->lookup("2.0", &dict);
		if (dict.isDict())
		{
			opiBegin20(state, dict.getDict());
			dict.free();
		}
		else
		{
			dict.free();
			opiDict->lookup("1.3", &dict);
			if (dict.isDict())
				opiBegin13(state, dict.getDict());
			dict.free();
		}
	}
}

void PSOutputDev::opiBegin20(GfxState* state, Dict* dict)
{
	Object obj1, obj2, obj3, obj4;
	double width, height, left, right, top, bottom;
	int    w, h;
	int    i;

	writePS("%%BeginOPI: 2.0\n");
	writePS("%%Distilled\n");

	dict->lookup("F", &obj1);
	if (getFileSpec(&obj1, &obj2))
	{
		writePSFmt("%%ImageFileName: {}\n", obj2.getString());
		obj2.free();
	}
	obj1.free();

	dict->lookup("MainImage", &obj1);
	if (obj1.isString())
		writePSFmt("%%MainImage: {}\n", obj1.getString());
	obj1.free();

	//~ ignoring 'Tags' entry
	//~ need to use writePSString() and deal with >255-char lines

	dict->lookup("Size", &obj1);
	if (obj1.isArray() && obj1.arrayGetLength() == 2)
	{
		obj1.arrayGet(0, &obj2);
		width = obj2.getNum();
		obj2.free();
		obj1.arrayGet(1, &obj2);
		height = obj2.getNum();
		obj2.free();
		writePSFmt("%%ImageDimensions: {} {}\n", Round6(width), Round6(height));
	}
	obj1.free();

	dict->lookup("CropRect", &obj1);
	if (obj1.isArray() && obj1.arrayGetLength() == 4)
	{
		obj1.arrayGet(0, &obj2);
		left = obj2.getNum();
		obj2.free();
		obj1.arrayGet(1, &obj2);
		top = obj2.getNum();
		obj2.free();
		obj1.arrayGet(2, &obj2);
		right = obj2.getNum();
		obj2.free();
		obj1.arrayGet(3, &obj2);
		bottom = obj2.getNum();
		obj2.free();
		writePSFmt("%%ImageCropRect: {} {} {} {}\n", Round6(left), Round6(top), Round6(right), Round6(bottom));
	}
	obj1.free();

	dict->lookup("Overprint", &obj1);
	if (obj1.isBool())
		writePSFmt("%%ImageOverprint: {}\n", obj1.getBool() ? "true" : "false");
	obj1.free();

	dict->lookup("Inks", &obj1);
	if (obj1.isName())
	{
		writePSFmt("%%ImageInks: {}\n", obj1.getName());
	}
	else if (obj1.isArray() && obj1.arrayGetLength() >= 1)
	{
		obj1.arrayGet(0, &obj2);
		if (obj2.isName())
		{
			writePSFmt("%%ImageInks: {} {}", obj2.getName(), (obj1.arrayGetLength() - 1) / 2);
			for (i = 1; i + 1 < obj1.arrayGetLength(); i += 2)
			{
				obj1.arrayGet(i, &obj3);
				obj1.arrayGet(i + 1, &obj4);
				if (obj3.isString() && obj4.isNum())
				{
					writePS(" ");
					writePSString(obj3.getString());
					writePSFmt(" {}", Round4(obj4.getNum()));
				}
				obj3.free();
				obj4.free();
			}
			writePS("\n");
		}
		obj2.free();
	}
	obj1.free();

	writePS("gsave\n");

	writePS("%%BeginIncludedImage\n");

	dict->lookup("IncludedImageDimensions", &obj1);
	if (obj1.isArray() && obj1.arrayGetLength() == 2)
	{
		obj1.arrayGet(0, &obj2);
		w = obj2.getInt();
		obj2.free();
		obj1.arrayGet(1, &obj2);
		h = obj2.getInt();
		obj2.free();
		writePSFmt("%%IncludedImageDimensions: {} {}\n", w, h);
	}
	obj1.free();

	dict->lookup("IncludedImageQuality", &obj1);
	if (obj1.isNum())
		writePSFmt("%%IncludedImageQuality: {}\n", Round4(obj1.getNum()));
	obj1.free();

	++opi20Nest;
}

void PSOutputDev::opiBegin13(GfxState* state, Dict* dict)
{
	Object obj1, obj2;
	int    left, right, top, bottom, samples, bits, width, height;
	double c, m, y, k;
	double llx, lly, ulx, uly, urx, ury, lrx, lry;
	double tllx, tlly, tulx, tuly, turx, tury, tlrx, tlry;
	double horiz, vert;
	int    i, j;

	writePS("save\n");
	writePS("/opiMatrix2 matrix currentmatrix def\n");
	writePS("opiMatrix setmatrix\n");

	dict->lookup("F", &obj1);
	if (getFileSpec(&obj1, &obj2))
	{
		writePSFmt("%ALDImageFileName: {}\n", obj2.getString());
		obj2.free();
	}
	obj1.free();

	dict->lookup("CropRect", &obj1);
	if (obj1.isArray() && obj1.arrayGetLength() == 4)
	{
		obj1.arrayGet(0, &obj2);
		left = obj2.getInt();
		obj2.free();
		obj1.arrayGet(1, &obj2);
		top = obj2.getInt();
		obj2.free();
		obj1.arrayGet(2, &obj2);
		right = obj2.getInt();
		obj2.free();
		obj1.arrayGet(3, &obj2);
		bottom = obj2.getInt();
		obj2.free();
		writePSFmt("%ALDImageCropRect: {} {} {} {}\n", left, top, right, bottom);
	}
	obj1.free();

	dict->lookup("Color", &obj1);
	if (obj1.isArray() && obj1.arrayGetLength() == 5)
	{
		obj1.arrayGet(0, &obj2);
		c = obj2.getNum();
		obj2.free();
		obj1.arrayGet(1, &obj2);
		m = obj2.getNum();
		obj2.free();
		obj1.arrayGet(2, &obj2);
		y = obj2.getNum();
		obj2.free();
		obj1.arrayGet(3, &obj2);
		k = obj2.getNum();
		obj2.free();
		obj1.arrayGet(4, &obj2);
		if (obj2.isString())
			writePSFmt("%ALDImageColor: {} {} {} {} ", Round4(c), Round4(m), Round4(y), Round4(k);
			writePSString(obj2.getString());
			writePS("\n");
		obj2.free();
	}
	obj1.free();

	dict->lookup("ColorType", &obj1);
	if (obj1.isName())
		writePSFmt("%ALDImageColorType: {}\n", obj1.getName());
	obj1.free();

	//~ ignores 'Comments' entry
	//~ need to handle multiple lines

	dict->lookup("CropFixed", &obj1);
	if (obj1.isArray())
	{
		obj1.arrayGet(0, &obj2);
		ulx = obj2.getNum();
		obj2.free();
		obj1.arrayGet(1, &obj2);
		uly = obj2.getNum();
		obj2.free();
		obj1.arrayGet(2, &obj2);
		lrx = obj2.getNum();
		obj2.free();
		obj1.arrayGet(3, &obj2);
		lry = obj2.getNum();
		obj2.free();
		writePSFmt("%ALDImageCropFixed: {} {} {} {}\n", Round4(ulx), Round4(uly), Round4(lrx), Round4(lry));
	}
	obj1.free();

	dict->lookup("GrayMap", &obj1);
	if (obj1.isArray())
	{
		writePS("%ALDImageGrayMap:");
		for (i = 0; i < obj1.arrayGetLength(); i += 16)
		{
			if (i > 0)
				writePS("\n%%+");
			for (j = 0; j < 16 && i + j < obj1.arrayGetLength(); ++j)
			{
				obj1.arrayGet(i + j, &obj2);
				writePSFmt(" {}", obj2.getInt());
				obj2.free();
			}
		}
		writePS("\n");
	}
	obj1.free();

	dict->lookup("ID", &obj1);
	if (obj1.isString())
		writePSFmt("%ALDImageID: {}\n", obj1.getString());
	obj1.free();

	dict->lookup("ImageType", &obj1);
	if (obj1.isArray() && obj1.arrayGetLength() == 2)
	{
		obj1.arrayGet(0, &obj2);
		samples = obj2.getInt();
		obj2.free();
		obj1.arrayGet(1, &obj2);
		bits = obj2.getInt();
		obj2.free();
		writePSFmt("%ALDImageType: {} {}\n", samples, bits);
	}
	obj1.free();

	dict->lookup("Overprint", &obj1);
	if (obj1.isBool())
		writePSFmt("%ALDImageOverprint: {}\n", obj1.getBool() ? "true" : "false");
	obj1.free();

	dict->lookup("Position", &obj1);
	if (obj1.isArray() && obj1.arrayGetLength() == 8)
	{
		obj1.arrayGet(0, &obj2);
		llx = obj2.getNum();
		obj2.free();
		obj1.arrayGet(1, &obj2);
		lly = obj2.getNum();
		obj2.free();
		obj1.arrayGet(2, &obj2);
		ulx = obj2.getNum();
		obj2.free();
		obj1.arrayGet(3, &obj2);
		uly = obj2.getNum();
		obj2.free();
		obj1.arrayGet(4, &obj2);
		urx = obj2.getNum();
		obj2.free();
		obj1.arrayGet(5, &obj2);
		ury = obj2.getNum();
		obj2.free();
		obj1.arrayGet(6, &obj2);
		lrx = obj2.getNum();
		obj2.free();
		obj1.arrayGet(7, &obj2);
		lry = obj2.getNum();
		obj2.free();
		opiTransform(state, llx, lly, &tllx, &tlly);
		opiTransform(state, ulx, uly, &tulx, &tuly);
		opiTransform(state, urx, ury, &turx, &tury);
		opiTransform(state, lrx, lry, &tlrx, &tlry);
		writePSFmt("%ALDImagePosition: {} {} {} {} {} {} {} {}\n", Round4(tllx), Round4(tlly), Round4(tulx), Round4(tuly), Round4(turx), Round4(tury), Round4(tlrx), Round4(tlry));
		obj2.free();
	}
	obj1.free();

	dict->lookup("Resolution", &obj1);
	if (obj1.isArray() && obj1.arrayGetLength() == 2)
	{
		obj1.arrayGet(0, &obj2);
		horiz = obj2.getNum();
		obj2.free();
		obj1.arrayGet(1, &obj2);
		vert = obj2.getNum();
		obj2.free();
		writePSFmt("%ALDImageResoution: {} {}\n", Round4(horiz), Round4(vert));
		obj2.free();
	}
	obj1.free();

	dict->lookup("Size", &obj1);
	if (obj1.isArray() && obj1.arrayGetLength() == 2)
	{
		obj1.arrayGet(0, &obj2);
		width = obj2.getInt();
		obj2.free();
		obj1.arrayGet(1, &obj2);
		height = obj2.getInt();
		obj2.free();
		writePSFmt("%ALDImageDimensions: {} {}\n", width, height);
	}
	obj1.free();

	//~ ignoring 'Tags' entry
	//~ need to use writePSString() and deal with >255-char lines

	dict->lookup("Tint", &obj1);
	if (obj1.isNum())
		writePSFmt("%ALDImageTint: {}\n", Round4(obj1.getNum()));
	obj1.free();

	dict->lookup("Transparency", &obj1);
	if (obj1.isBool())
		writePSFmt("%ALDImageTransparency: {}\n", obj1.getBool() ? "true" : "false");
	obj1.free();

	writePS("%%BeginObject: image\n");
	writePS("opiMatrix2 setmatrix\n");
	++opi13Nest;
}

// Convert PDF user space coordinates to PostScript default user space
// coordinates.  This has to account for both the PDF CTM and the
// PSOutputDev page-fitting transform.
void PSOutputDev::opiTransform(GfxState* state, double x0, double y0, double* x1, double* y1)
{
	double t;

	state->transform(x0, y0, x1, y1);
	*x1 += tx;
	*y1 += ty;
	if (rotate == 90)
	{
		t   = *x1;
		*x1 = -*y1;
		*y1 = t;
	}
	else if (rotate == 180)
	{
		*x1 = -*x1;
		*y1 = -*y1;
	}
	else if (rotate == 270)
	{
		t   = *x1;
		*x1 = *y1;
		*y1 = -t;
	}
	*x1 *= xScale;
	*y1 *= yScale;
}

void PSOutputDev::opiEnd(GfxState* state, Dict* opiDict)
{
	Object dict;

	if (globalParams->getPSOPI())
	{
		opiDict->lookup("2.0", &dict);
		if (dict.isDict())
		{
			writePS("%%EndIncludedImage\n");
			writePS("%%EndOPI\n");
			writePS("grestore\n");
			--opi20Nest;
			dict.free();
		}
		else
		{
			dict.free();
			opiDict->lookup("1.3", &dict);
			if (dict.isDict())
			{
				writePS("%%EndObject\n");
				writePS("restore\n");
				--opi13Nest;
			}
			dict.free();
		}
	}
}

bool PSOutputDev::getFileSpec(Object* fileSpec, Object* fileName)
{
	if (fileSpec->isString())
	{
		fileSpec->copy(fileName);
		return true;
	}
	if (fileSpec->isDict())
	{
		fileSpec->dictLookup("DOS", fileName);
		if (fileName->isString())
			return true;
		fileName->free();
		fileSpec->dictLookup("Mac", fileName);
		if (fileName->isString())
			return true;
		fileName->free();
		fileSpec->dictLookup("Unix", fileName);
		if (fileName->isString())
			return true;
		fileName->free();
		fileSpec->dictLookup("F", fileName);
		if (fileName->isString())
			return true;
		fileName->free();
	}
	return false;
}
#endif // OPI_SUPPORT

void PSOutputDev::type3D0(GfxState* state, double wx, double wy)
{
	writePSFmt("{} {} setcharwidth\n", Round6(wx), Round6(wy));
	writePS("q\n");
	t3NeedsRestore = true;
	noStateChanges = false;
}

void PSOutputDev::type3D1(GfxState* state, double wx, double wy, double llx, double lly, double urx, double ury)
{
	if (t3String.size())
	{
		error(errSyntaxError, -1, "Multiple 'd1' operators in Type 3 CharProc");
		return;
	}
	t3WX  = wx;
	t3WY  = wy;
	t3LLX = llx;
	t3LLY = lly;
	t3URX = urx;
	t3URY = ury;
	t3String.clear();
	writePS("q\n");
	t3FillColorOnly = true;
	t3Cacheable     = true;
	t3NeedsRestore  = true;
	noStateChanges  = false;
}

void PSOutputDev::drawForm(Ref id)
{
	writePSFmt("f_{}_{}\n", id.num, id.gen);
	noStateChanges = false;
}

void PSOutputDev::psXObject(Stream* psStream, Stream* level1Stream)
{
	Stream* str;
	char    buf[4096];
	int     n;

	if ((level == psLevel1 || level == psLevel1Sep) && level1Stream)
		str = level1Stream;
	else
		str = psStream;
	str->reset();
	while ((n = str->getBlock(buf, sizeof(buf))) > 0)
		writePSBlock(buf, n);
	str->close();
	noStateChanges = false;
}

//~ can nextFunc be reset to 0 -- maybe at the start of each page?
//~   or maybe at the start of each color space / pattern?
void PSOutputDev::cvtFunction(Function* func)
{
	SampledFunction*     func0;
	ExponentialFunction* func2;
	StitchingFunction*   func3;
	PostScriptFunction*  func4;
	int                  thisFunc, m, n, nSamples, i, j, k;

	switch (func->getType())
	{
	case -1: // identity
		writePS("{}\n");
		break;

	case 0: // sampled
		func0    = (SampledFunction*)func;
		thisFunc = nextFunc++;
		m        = func0->getInputSize();
		n        = func0->getOutputSize();
		nSamples = n;
		for (i = 0; i < m; ++i)
			nSamples *= func0->getSampleSize(i);
		writePSFmt("/xpdfSamples{} [\n", thisFunc);
		for (i = 0; i < nSamples; ++i)
			writePSFmt("{}\n", Round6(func0->getSamples()[i]));
		writePS("] def\n");
		writePSFmt("{{ {} array {} array {} 2 roll\n", 2 * m, m, m + 2);
		// [e01] [efrac] x0 x1 ... xm-1
		for (i = m - 1; i >= 0; --i)
		{
			// [e01] [efrac] x0 x1 ... xi
			writePSFmt("{} sub {} mul {} add\n", Round6(func0->getDomainMin(i)), Round6((func0->getEncodeMax(i) - func0->getEncodeMin(i)) / (func0->getDomainMax(i) - func0->getDomainMin(i))), Round6(func0->getEncodeMin(i)));
			// [e01] [efrac] x0 x1 ... xi-1 xi'
			writePSFmt("dup 0 lt {{ pop 0 }} {{ dup {} gt {{ pop {} }} if }} ifelse\n", func0->getSampleSize(i) - 1, func0->getSampleSize(i) - 1);
			// [e01] [efrac] x0 x1 ... xi-1 xi'
			writePS("dup floor cvi exch dup ceiling cvi exch 2 index sub\n");
			// [e01] [efrac] x0 x1 ... xi-1 floor(xi') ceiling(xi') xi'-floor(xi')
			writePSFmt("{} index {} 3 2 roll put\n", i + 3, i);
			// [e01] [efrac] x0 x1 ... xi-1 floor(xi') ceiling(xi')
			writePSFmt("{} index {} 3 2 roll put\n", i + 3, 2 * i + 1);
			// [e01] [efrac] x0 x1 ... xi-1 floor(xi')
			writePSFmt("{} index {} 3 2 roll put\n", i + 2, 2 * i);
			// [e01] [efrac] x0 x1 ... xi-1
		}
		// [e01] [efrac]
		for (i = 0; i < n; ++i)
		{
			// [e01] [efrac] y(0) ... y(i-1)
			for (j = 0; j < (1 << m); ++j)
			{
				// [e01] [efrac] y(0) ... y(i-1) s(0) s(1) ... s(j-1)
				writePSFmt("xpdfSamples{}\n", thisFunc);
				k = m - 1;
				writePSFmt("{} index {} get\n", i + j + 2, 2 * k + ((j >> k) & 1));
				for (k = m - 2; k >= 0; --k)
					writePSFmt("{} mul {} index {} get add\n", func0->getSampleSize(k), i + j + 3, 2 * k + ((j >> k) & 1));
				if (n > 1)
					writePSFmt("{} mul {} add ", n, i);
				writePS("get\n");
			}
			// [e01] [efrac] y(0) ... y(i-1) s(0) s(1) ... s(2^m-1)
			for (j = 0; j < m; ++j)
			{
				// [e01] [efrac] y(0) ... y(i-1) s(0) s(1) ... s(2^(m-j)-1)
				for (k = 0; k < (1 << (m - j)); k += 2)
				{
					// [e01] [efrac] y(0) ... y(i-1) <k/2 s' values> <2^(m-j)-k s values>
					writePSFmt("{} index {} get dup\n", i + k / 2 + (1 << (m - j)) - k, j);
					writePS("3 2 roll mul exch 1 exch sub 3 2 roll mul add\n");
					writePSFmt("{} 1 roll\n", k / 2 + (1 << (m - j)) - k - 1);
				}
				// [e01] [efrac] s'(0) s'(1) ... s(2^(m-j-1)-1)
			}
			// [e01] [efrac] y(0) ... y(i-1) s
			writePSFmt("{} mul {} add\n", Round6(func0->getDecodeMax(i) - func0->getDecodeMin(i)), Round6(func0->getDecodeMin(i)));
			writePSFmt("dup {} lt {{ pop {} }} {{ dup {} gt {{ pop {} }} if }} ifelse\n", Round6(func0->getRangeMin(i)), Round6(func0->getRangeMin(i)), Round6(func0->getRangeMax(i)), Round6(func0->getRangeMax(i)));
			// [e01] [efrac] y(0) ... y(i-1) y(i)
		}
		// [e01] [efrac] y(0) ... y(n-1)
		writePSFmt("{} {} roll pop pop }}\n", n + 2, n);
		break;

	case 2: // exponential
		func2 = (ExponentialFunction*)func;
		n     = func2->getOutputSize();
		writePSFmt("{{ dup {} lt {{ pop {} }} {{ dup {} gt {{ pop {} }} if }} ifelse\n", Round6(func2->getDomainMin(0)), Round6(func2->getDomainMin(0)), Round6(func2->getDomainMax(0)), Round6(func2->getDomainMax(0)));
		// x
		for (i = 0; i < n; ++i)
		{
			// x y(0) .. y(i-1)
			writePSFmt("{} index {} exp {} mul {} add\n", i, Round6(func2->getE()), Round6(func2->getC1()[i] - func2->getC0()[i]), Round6(func2->getC0()[i]));
			if (func2->getHasRange())
				writePSFmt("dup {} lt {{ pop {} }} {{ dup {} gt {{ pop {} }} if }} ifelse\n", Round6(func2->getRangeMin(i)), Round6(func2->getRangeMin(i)), Round6(func2->getRangeMax(i)), Round6(func2->getRangeMax(i)));
		}
		// x y(0) .. y(n-1)
		writePSFmt("{} {} roll pop }}\n", n + 1, n);
		break;

	case 3: // stitching
		func3    = (StitchingFunction*)func;
		thisFunc = nextFunc++;
		for (i = 0; i < func3->getNumFuncs(); ++i)
		{
			cvtFunction(func3->getFunc(i));
			writePSFmt("/xpdfFunc{}_{} exch def\n", thisFunc, i);
		}
		writePSFmt("{{ dup {} lt {{ pop {} }} {{ dup {} gt {{ pop {} }} if }} ifelse\n", Round6(func3->getDomainMin(0)), Round6(func3->getDomainMin(0)), Round6(func3->getDomainMax(0)), Round6(func3->getDomainMax(0)));
		for (i = 0; i < func3->getNumFuncs() - 1; ++i)
			writePSFmt("dup {} lt {{ {} sub {} mul {} add xpdfFunc{}_{} }} {{\n", Round6(func3->getBounds()[i + 1]), Round6(func3->getBounds()[i]), Round6(func3->getScale()[i]), Round6(func3->getEncode()[2 * i]), thisFunc, i);
		writePSFmt("{} sub {} mul {} add xpdfFunc{}_{}\n", Round6(func3->getBounds()[i]), Round6(func3->getScale()[i]), Round6(func3->getEncode()[2 * i]), thisFunc, i);
		for (i = 0; i < func3->getNumFuncs() - 1; ++i)
			writePS("} ifelse\n");
		writePS("}\n");
		break;

	case 4: // PostScript
		func4 = (PostScriptFunction*)func;
		writePS(func4->getCodeString().c_str());
		writePS("\n");
		break;
	}
}

void PSOutputDev::writePSChar(char c)
{
	if (t3String.size())
		t3String += c;
	else
		(*outputFunc)(outputStream, &c, 1);
}

void PSOutputDev::writePSBlock(const char* s, size_t len)
{
	if (t3String.size())
		t3String.append(s, len);
	else
		(*outputFunc)(outputStream, s, len);
}

void PSOutputDev::writePS(const char* s)
{
	if (t3String.size())
		t3String += s;
	else
		(*outputFunc)(outputStream, s, (int)strlen(s));
}

template <typename... T>
void PSOutputDev::writePSFmt(fmt::format_string<T...> fmt, T&&... args)
{
	const auto& vargs = fmt::make_format_args(args...);
	const auto  buf   = fmt::vformat(fmt, vargs);
	if (t3String.size())
		t3String.append(buf);
	else
		(*outputFunc)(outputStream, buf.c_str(), buf.size());
}

void PSOutputDev::writePSString(std::string_view sv)
{
	writePSChar('(');
	int      line = 1;
	int      n    = TO_INT(sv.size());
	uint8_t* p    = (uint8_t*)sv.data();
	for (; n; ++p, --n)
	{
		if (line >= 64)
		{
			writePSChar('\\');
			writePSChar('\n');
			line = 0;
		}
		if (*p == '(' || *p == ')' || *p == '\\')
		{
			writePSChar('\\');
			writePSChar((char)*p);
			line += 2;
		}
		else if (*p < 0x20 || *p >= 0x80)
		{
			char buf[8];
			snprintf(buf, sizeof(buf), "\\%03o", *p);
			writePS(buf);
			line += 4;
		}
		else
		{
			writePSChar((char)*p);
			++line;
		}
	}
	writePSChar(')');
}

void PSOutputDev::writePSName(const std::string& s)
{
	const char* p;
	char        c;

	p = s.c_str();
	while ((c = *p++))
		if (c <= (char)0x20 || c >= (char)0x7f || c == '(' || c == ')' || c == '<' || c == '>' || c == '[' || c == ']' || c == '{' || c == '}' || c == '/' || c == '%')
			writePSFmt("#{:02x}", c & 0xff);
		else
			writePSChar(c);
}

std::string PSOutputDev::filterPSName(const std::string& name)
{
	char        buf[8];
	std::string name2;

	// ghostscript chokes on names that begin with out-of-limits numbers,
	// e.g., 1e4foo is handled correctly (as a name), but 1e999foo generates a limitcheck error
	const char c = name.at(0);
	if (c >= '0' && c <= '9')
		name2 += 'f';

	for (int i = 0; i < name.size(); ++i)
	{
		const char c = name.at(i);
		if (c <= (char)0x20 || c >= (char)0x7f || c == '(' || c == ')' || c == '<' || c == '>' || c == '[' || c == ']' || c == '{' || c == '}' || c == '/' || c == '%')
		{
			snprintf(buf, sizeof(buf), "#%02x", c & 0xff);
			name2 += buf;
		}
		else
		{
			name2 += c;
		}
	}
	return name2;
}

// Write a DSC-compliant <textline>.
void PSOutputDev::writePSTextLine(const std::string& s)
{
	TextString* ts;
	Unicode*    u;

	// - DSC comments must be printable ASCII; control chars and backslashes have to be escaped (we do cheap Unicode-to-ASCII conversion by simply ignoring the high byte)
	// - lines are limited to 255 chars (we limit to 200 here to allow for the keyword, which was emitted by the caller)
	// - lines that start with a left paren are treated as <text> instead of <textline>, so we escape a leading paren
	ts = new TextString(s);
	u  = ts->getUnicode();
	for (int i = 0, j = 0; i < ts->getLength() && j < 200; ++i)
	{
		const int c = u[i] & 0xff;
		if (c == '\\')
		{
			writePS("\\\\");
			j += 2;
		}
		else if (c < 0x20 || c > 0x7e || (j == 0 && c == '('))
		{
			writePSFmt("\\{:03o}", c);
			j += 4;
		}
		else
		{
			writePSChar((char)c);
			++j;
		}
	}
	writePS("\n");
	delete ts;
}
