//========================================================================
//
// GString.cc
//
// Simple variable-length string type.
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include "GString.h"

//------------------------------------------------------------------------

std::string LowerCase(const std::string& str)
{
	std::string lowerStr = str;
	std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), [](unsigned char c) { return std::tolower(c); });
	return lowerStr;
}

/**
 * Remove zeroes at the end
 */
inline static std::string_view TrimDecimals(std::string_view snumber)
{
	if (const auto pos = snumber.find('.'); pos != std::string::npos)
	{
		int i = (int)snumber.size() - 1;
		while (i > pos && snumber.at(i) == '0') --i;
		if (i == pos) --i;
		return snumber.substr(0, i + 1);
	}
	return snumber;
}

// Explicit instantiation for double and float
template std::string Round2<double>(double number);
template std::string Round2<float>(float number);
template std::string Round4<double>(double number);
template std::string Round4<float>(float number);
template std::string Round6<double>(double number);
template std::string Round6<float>(float number);
template std::string Round8<double>(double number);
template std::string Round8<float>(float number);

template <typename T>
std::string Round2(T number)
{
	return std::string { TrimDecimals(fmt::format("{:.2f}", number)) };
}

template <typename T>
std::string Round4(T number)
{
	return std::string { TrimDecimals(fmt::format("{:.4f}", number)) };
}

template <typename T>
std::string Round6(T number)
{
	return std::string { TrimDecimals(fmt::format("{:.6f}", number)) };
}

template <typename T>
std::string Round8(T number)
{
	return std::string { TrimDecimals(fmt::format("{:.8f}", number)) };
}

std::string UpperCase(const std::string& str)
{
	std::string upperStr = str;
	std::transform(upperStr.begin(), upperStr.end(), upperStr.begin(), [](unsigned char c) { return std::toupper(c); });
	return upperStr;
}
