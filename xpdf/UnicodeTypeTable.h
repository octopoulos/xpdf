//========================================================================
//
// UnicodeTypeTable.h
//
// Copyright 2003-2013 Glyph & Cog, LLC
//
//========================================================================

#pragma once

extern bool unicodeTypeL(Unicode c);

extern bool unicodeTypeR(Unicode c);

extern bool unicodeTypeCombiningMark(Unicode c);

extern bool unicodeTypeDigit(Unicode c);

extern bool unicodeTypeNumSep(Unicode c);

extern bool unicodeTypeNum(Unicode c);

extern bool unicodeTypeAlphaNum(Unicode c);

extern bool unicodeTypeWord(Unicode c);

extern Unicode unicodeToLower(Unicode c);

extern bool unicodeBracketInfo(Unicode c, bool* open, Unicode* opposite);
