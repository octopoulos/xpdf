//========================================================================
//
// FoFiEncodings.h
//
// Copyright 1999-2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>

//------------------------------------------------------------------------
// Type 1 and 1C font data
//------------------------------------------------------------------------

extern const char* fofiType1StandardEncoding[256];
extern const char* fofiType1ExpertEncoding[256];

//------------------------------------------------------------------------
// Type 1C font data
//------------------------------------------------------------------------

extern const char* fofiType1CStdStrings[391];
extern uint16_t    fofiType1CISOAdobeCharset[229];
extern uint16_t    fofiType1CExpertCharset[166];
extern uint16_t    fofiType1CExpertSubsetCharset[87];
