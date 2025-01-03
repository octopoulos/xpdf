//========================================================================
//
// GString.h
//
// Simple variable-length string type.
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>

std::string LowerCase(const std::string& str);

template <typename T>
std::string Round2(T number);
template <typename T>
std::string Round4(T number);
template <typename T>
std::string Round6(T number);
template <typename T>
std::string Round8(T number);

std::string UpperCase(const std::string& str);
