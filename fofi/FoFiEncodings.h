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

extern const VEC_STR fofiType1StandardEncoding;
extern const VEC_STR fofiType1ExpertEncoding;

//------------------------------------------------------------------------
// Type 1C font data
//------------------------------------------------------------------------

extern const VEC_STR fofiType1CStdStrings;
extern uint16_t      fofiType1CISOAdobeCharset[229];
extern uint16_t      fofiType1CExpertCharset[166];
extern uint16_t      fofiType1CExpertSubsetCharset[87];
