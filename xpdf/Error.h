//========================================================================
//
// Error.h
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#pragma once

#include <aconf.h>
#include <stdio.h>
#include "config.h"
#include "gfile.h"

enum ErrorCategory
{
	errSyntaxWarning, // PDF syntax error which can be worked around; output will probably be correct
	errSyntaxError,   // PDF syntax error which cannot be worked around; output will probably be incorrect
	errConfig,        // error in Xpdf config info (xpdfrc file, etc.)
	errCommandLine,   // error in user-supplied parameters, action not allowed, etc. (only used by command-line tools)
	errIO,            // error in file I/O
	errNotAllowed,    // action not allowed by PDF permission bits
	errUnimplemented, // unimplemented PDF feature - display will be incorrect
	errInternal       // internal error - malfunction within the Xpdf code
};

extern const char* errorCategoryNames[];

extern void setErrorCallback(void (*cbk)(void* data, ErrorCategory category, int pos, const char* msg), void* data);

extern void* getErrorCallbackData();

void errorText(ErrorCategory category, int64_t pos, std::string text);

template <typename... T>
extern void CDECL error(ErrorCategory category, int64_t pos, fmt::format_string<T...> fmt, T&&... args)
{
	const auto& vargs = fmt::make_format_args(args...);
	auto        text  = fmt::vformat(fmt, vargs);
	errorText(category, pos, std::move(text));
}
