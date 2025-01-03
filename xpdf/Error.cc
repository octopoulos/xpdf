//========================================================================
//
// Error.cc
//
// Copyright 1996-2013 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include "gmempp.h"
#include "GlobalParams.h"
#include "Error.h"

const char* errorCategoryNames[] = {
	"Syntax Warning",
	"Syntax Error",
	"Config Error",
	"Command Line Error",
	"I/O Error",
	"Permission Error",
	"Unimplemented Feature",
	"Internal Error"
};

static void  (*errorCbk)(void* data, ErrorCategory category, int pos, const char* msg) = nullptr;
static void* errorCbkData                                                              = nullptr;

void setErrorCallback(void (*cbk)(void* data, ErrorCategory category, int pos, const char* msg), void* data)
{
	errorCbk     = cbk;
	errorCbkData = data;
}

void* getErrorCallbackData()
{
	return errorCbkData;
}

/**
 * Make printable text for console output
 */
static std::string Printify(std::string_view text, size_t count = std::string_view::npos)
{
	std::string clean;
	for (size_t i = 0, size = std::min(text.size(), count); i < size; ++i)
	{
		if (static_cast<uint8_t>(text[i] - 32) >= 96)
			clean += fmt::format("\\x{:02x}", static_cast<uint8_t>(text[i]));
		else
			clean += text[i];
	}

	return clean;
}

void errorText(ErrorCategory category, int64_t pos, std::string text)
{
	// NB: this can be called before the globalParams object is created
	if (!errorCbk && globalParams && globalParams->getErrQuiet())
		return;

	// remove non-printable characters, just in case they might cause
	// problems for the terminal program
	const auto sanitized = Printify(text);
	if (errorCbk)
	{
		(*errorCbk)(errorCbkData, category, (int)pos, sanitized.c_str());
	}
	else
	{
		fflush(stdout);
		if (pos >= 0)
			fprintf(stderr, "%s (%d): %s\n", errorCategoryNames[category], (int)pos, sanitized.c_str());
		else
			fprintf(stderr, "%s: %s\n", errorCategoryNames[category], sanitized.c_str());
		fflush(stderr);
	}
}
