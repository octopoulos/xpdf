//========================================================================
//
// UTF8.cc
//
// Copyright 2001-2017 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include "UTF8.h"

int mapUTF8(Unicode u, char* buf, int bufSize)
{
	if (u <= 0x0000007f)
	{
		if (bufSize < 1) return 0;
		buf[0] = (char)u;
		return 1;
	}
	else if (u <= 0x000007ff)
	{
		if (bufSize < 2) return 0;
		buf[0] = (char)(0xc0 + (u >> 6));
		buf[1] = (char)(0x80 + (u & 0x3f));
		return 2;
	}
	else if (u <= 0x0000ffff)
	{
		if (bufSize < 3) return 0;
		buf[0] = (char)(0xe0 + (u >> 12));
		buf[1] = (char)(0x80 + ((u >> 6) & 0x3f));
		buf[2] = (char)(0x80 + (u & 0x3f));
		return 3;
	}
	else if (u <= 0x0010ffff)
	{
		if (bufSize < 4) return 0;
		buf[0] = (char)(0xf0 + (u >> 18));
		buf[1] = (char)(0x80 + ((u >> 12) & 0x3f));
		buf[2] = (char)(0x80 + ((u >> 6) & 0x3f));
		buf[3] = (char)(0x80 + (u & 0x3f));
		return 4;
	}
	else
	{
		return 0;
	}
}

int mapUCS2(Unicode u, char* buf, int bufSize)
{
	if (u <= 0xffff)
	{
		if (bufSize < 2) return 0;
		buf[0] = (char)((u >> 8) & 0xff);
		buf[1] = (char)(u & 0xff);
		return 2;
	}
	else
	{
		return 0;
	}
}

bool getUTF8(const std::string& s, int* i, Unicode* u)
{
	uint8_t c0, c1, c2, c3, c4, c5;

	if (*i >= s.size()) return false;
	c0 = (uint8_t)s.at((*i)++);
	if (c0 < 0x80)
	{
		*u = (Unicode)c0;
	}
	else if (c0 < 0xe0)
	{
		if (*i < s.size() && ((c1 = (uint8_t)s.at(*i)) & 0xc0) == 0x80)
		{
			*i += 1;
			*u = (Unicode)(((c0 & 0x1f) << 6) | (c1 & 0x3f));
		}
		else
		{
			*u = (Unicode)c0;
		}
	}
	else if (c0 < 0xf0)
	{
		if (*i < s.size() - 1 && ((c1 = (uint8_t)s.at(*i)) & 0xc0) == 0x80 && ((c2 = (uint8_t)s.at(*i + 1)) & 0xc0) == 0x80)
		{
			*i += 2;
			*u = (Unicode)(((c0 & 0x0f) << 12) | ((c1 & 0x3f) << 6) | (c2 & 0x3f));
		}
		else
		{
			*u = (Unicode)c0;
		}
	}
	else if (c0 < 0xf8)
	{
		if (*i < s.size() - 2 && ((c1 = (uint8_t)s.at(*i)) & 0xc0) == 0x80 && ((c2 = (uint8_t)s.at(*i + 1)) & 0xc0) == 0x80 && ((c3 = (uint8_t)s.at(*i + 2)) & 0xc0) == 0x80)
		{
			*i += 3;
			*u = (Unicode)(((c0 & 0x07) << 18) | ((c1 & 0x3f) << 12) | ((c2 & 0x3f) << 6) | (c3 & 0x3f));
		}
		else
		{
			*u = (Unicode)c0;
		}
	}
	else if (c0 < 0xfc)
	{
		if (*i < s.size() - 3 && ((c1 = (uint8_t)s.at(*i)) & 0xc0) == 0x80 && ((c2 = (uint8_t)s.at(*i + 1)) & 0xc0) == 0x80 && ((c3 = (uint8_t)s.at(*i + 2)) & 0xc0) == 0x80 && ((c4 = (uint8_t)s.at(*i + 3)) & 0xc0) == 0x80)
		{
			*i += 4;
			*u = (Unicode)(((c0 & 0x03) << 24) | ((c1 & 0x3f) << 18) | ((c2 & 0x3f) << 12) | ((c3 & 0x3f) << 6) | (c4 & 0x3f));
		}
		else
		{
			*u = (Unicode)c0;
		}
	}
	else if (c0 < 0xfe)
	{
		if (*i < s.size() - 4 && ((c1 = (uint8_t)s.at(*i)) & 0xc0) == 0x80 && ((c2 = (uint8_t)s.at(*i + 1)) & 0xc0) == 0x80 && ((c3 = (uint8_t)s.at(*i + 2)) & 0xc0) == 0x80 && ((c4 = (uint8_t)s.at(*i + 3)) & 0xc0) == 0x80 && ((c5 = (uint8_t)s.at(*i + 4)) & 0xc0) == 0x80)
		{
			*i += 5;
			*u = (Unicode)(((c0 & 0x01) << 30) | ((c1 & 0x3f) << 24) | ((c2 & 0x3f) << 18) | ((c3 & 0x3f) << 12) | ((c4 & 0x3f) << 6) | (c5 & 0x3f));
		}
		else
		{
			*u = (Unicode)c0;
		}
	}
	else
	{
		*u = (Unicode)c0;
	}
	return true;
}

bool getUTF16BE(const std::string& s, int* i, Unicode* u)
{
	if (*i >= s.size() - 1) return false;
	const int w0 = ((s.at(*i) & 0xff) << 8) | (s.at(*i + 1) & 0xff);
	*i += 2;
	if (w0 < 0xd800 || w0 >= 0xe000)
	{
		*u = (Unicode)w0;
	}
	else if (*i < s.size() - 1)
	{
		const int w1 = ((s.at(*i) & 0xff) << 8) | (s.at(*i + 1) & 0xff);
		*i += 2;
		*u = 0x10000 + ((w0 - 0xd800) << 10) + (w1 - 0xdc00);
	}
	else
	{
		*u = (Unicode)w0;
	}
	return true;
}

bool getUTF16LE(const std::string& s, int* i, Unicode* u)
{
	if (*i >= s.size() - 1) return false;
	const int w0 = (s.at(*i) & 0xff) | ((s.at(*i + 1) & 0xff) << 8);
	*i += 2;
	if (w0 < 0xd800 || w0 >= 0xe000)
	{
		*u = (Unicode)w0;
	}
	else if (*i < s.size() - 1)
	{
		const int w1 = (s.at(*i) & 0xff) | ((s.at(*i + 1) & 0xff) << 8);
		*i += 2;
		*u = 0x10000 + ((w0 - 0xd800) << 10) + (w1 - 0xdc00);
	}
	else
	{
		*u = (Unicode)w0;
	}
	return true;
}
