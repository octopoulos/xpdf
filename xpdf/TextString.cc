//========================================================================
//
// TextString.cc
//
// Copyright 2011-2013 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include <string.h>
#include "gmem.h"
#include "gmempp.h"
#include "PDFDocEncoding.h"
#include "UTF8.h"
#include "TextString.h"

//------------------------------------------------------------------------

TextString::TextString()
{
}

TextString::TextString(const std::string& s)
{
	append(s);
}

TextString::TextString(TextString* s)
{
	len = size = s->len;
	if (len)
	{
		u = (Unicode*)gmallocn(size, sizeof(Unicode));
		memcpy(u, s->u, len * sizeof(Unicode));
	}
}

TextString::~TextString()
{
	gfree(u);
}

TextString* TextString::append(Unicode c)
{
	expand(1);
	u[len] = c;
	++len;
	return this;
}

TextString* TextString::append(const std::string& s)
{
	return insert(len, s);
}

TextString* TextString::insert(int idx, Unicode c)
{
	if (idx >= 0 && idx <= len)
	{
		expand(1);
		if (idx < len)
			memmove(u + idx + 1, u + idx, (len - idx) * sizeof(Unicode));
		u[idx] = c;
		++len;
	}
	return this;
}

TextString* TextString::insert(int idx, Unicode* u2, int n)
{
	if (idx >= 0 && idx <= len)
	{
		expand(n);
		if (idx < len) memmove(u + idx + n, u + idx, (len - idx) * sizeof(Unicode));
		memcpy(u + idx, u2, n * sizeof(Unicode));
		len += n;
	}
	return this;
}

TextString* TextString::insert(int idx, const std::string& s)
{
	Unicode uBuf[100];

	if (idx >= 0 && idx <= len)
	{
		// look for a UTF-16BE BOM
		if ((s.at(0) & 0xff) == 0xfe && (s.at(1) & 0xff) == 0xff)
		{
			int i = 2;
			int n = 0;
			while (getUTF16BE(s, &i, uBuf + n))
			{
				++n;
				if (n == sizeof(uBuf) / sizeof(Unicode))
				{
					insert(idx, uBuf, n);
					idx += n;
					n = 0;
				}
			}
			if (n > 0)
				insert(idx, uBuf, n);

			// look for a UTF-16LE BOM
			// (technically, this isn't allowed by the PDF spec, but some PDF files use it)
		}
		else if ((s.at(0) & 0xff) == 0xff && (s.at(1) & 0xff) == 0xfe)
		{
			int i = 2;
			int n = 0;
			while (getUTF16LE(s, &i, uBuf + n))
			{
				++n;
				if (n == sizeof(uBuf) / sizeof(Unicode))
				{
					insert(idx, uBuf, n);
					idx += n;
					n = 0;
				}
			}
			if (n > 0)
				insert(idx, uBuf, n);

			// look for a UTF-8 BOM
		}
		else if ((s.at(0) & 0xff) == 0xef && (s.at(1) & 0xff) == 0xbb && (s.at(2) & 0xff) == 0xbf)
		{
			int i = 3;
			int n = 0;
			while (getUTF8(s, &i, uBuf + n))
			{
				++n;
				if (n == sizeof(uBuf) / sizeof(Unicode))
				{
					insert(idx, uBuf, n);
					idx += n;
					n = 0;
				}
			}
			if (n > 0)
				insert(idx, uBuf, n);

			// otherwise, use PDFDocEncoding
		}
		else
		{
			const int n = TO_INT(s.size());
			expand(n);
			if (idx < len)
				memmove(u + idx + n, u + idx, (len - idx) * sizeof(Unicode));
			for (int i = 0; i < n; ++i)
				u[idx + i] = pdfDocEncoding[s.at(i) & 0xff];
			len += n;
		}
	}
	return this;
}

void TextString::expand(int delta)
{
	const int newLen = len + delta;
	if (delta > INT_MAX - len)
	{
		// trigger an out-of-memory error
		size = -1;
	}
	else if (newLen <= size)
	{
		return;
	}
	else if (size > 0 && size <= INT_MAX / 2 && size * 2 >= newLen)
	{
		size *= 2;
	}
	else
	{
		size = newLen;
	}
	u = (Unicode*)greallocn(u, size, sizeof(Unicode));
}

std::string TextString::toPDFTextString()
{
	bool useUnicode = false;
	for (int i = 0; i < len; ++i)
	{
		if (u[i] >= 0x80)
		{
			useUnicode = true;
			break;
		}
	}

	std::string s;
	if (useUnicode)
	{
		s += (char)0xfe;
		s += (char)0xff;
		for (int i = 0; i < len; ++i)
		{
			s += (char)(u[i] >> 8);
			s += (char)u[i];
		}
	}
	else
	{
		for (int i = 0; i < len; ++i)
			s += (char)u[i];
	}
	return s;
}

std::string TextString::toUTF8()
{
	std::string s;
	for (int i = 0; i < len; ++i)
	{
		char buf[8];
		int  n = mapUTF8(u[i], buf, sizeof(buf));
		s.append(buf, n);
	}
	return s;
}
