//========================================================================
//
// Zoox.cc
//
//========================================================================

#include <aconf.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "gmem.h"
#include "gmempp.h"
#include "Zoox.h"

//~ all of this code assumes the encoding is UTF-8 or ASCII or something similar (ISO-8859-*)

//------------------------------------------------------------------------

static char nameStartChar[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 00
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 10
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 20
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, // 30
	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 40
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, // 50
	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 60
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, // 70
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 80
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 90
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // a0
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // b0
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // c0
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // d0
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // e0
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1  // f0
};

static char nameChar[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 00
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 10
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, // 20
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, // 30
	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 40
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, // 50
	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 60
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, // 70
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 80
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 90
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // a0
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // b0
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // c0
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // d0
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // e0
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1  // f0
};

//------------------------------------------------------------------------

ZxNode::ZxNode()
{
	next       = nullptr;
	parent     = nullptr;
	firstChild = lastChild = nullptr;
}

ZxNode::~ZxNode()
{
	ZxNode* child;

	while (firstChild)
	{
		child      = firstChild;
		firstChild = firstChild->next;
		delete child;
	}
}

ZxNode* ZxNode::deleteChild(ZxNode* child)
{
	ZxNode *p1, *p2;

	for (p1 = nullptr, p2 = firstChild; p2 && p2 != child; p1 = p2, p2 = p2->next)
		;
	if (!p2) return nullptr;
	if (p1)
		p1->next = child->next;
	else
		firstChild = child->next;
	child->parent = nullptr;
	child->next   = nullptr;
	return child;
}

void ZxNode::appendChild(ZxNode* child)
{
	ZxNode* p1;

	if (child->parent || child->next)
		return;
	if (firstChild)
	{
		for (p1 = firstChild; p1->next; p1 = p1->next)
			;
		p1->next = child;
	}
	else
	{
		firstChild = child;
	}
	child->parent = this;
}

void ZxNode::insertChildAfter(ZxNode* child, ZxNode* prev)
{
	if (child->parent || child->next || (prev && prev->parent != this))
		return;
	if (prev)
	{
		child->next = prev->next;
		prev->next  = child;
	}
	else
	{
		child->next = firstChild;
		firstChild  = child;
	}
	child->parent = this;
}

ZxElement* ZxNode::findFirstElement(const char* type)
{
	ZxNode*    child;
	ZxElement* result;

	if (isElement(type))
		return (ZxElement*)this;
	for (child = firstChild; child; child = child->next)
		if ((result = child->findFirstElement(type)))
			return result;
	return nullptr;
}

ZxElement* ZxNode::findFirstChildElement(const char* type)
{
	ZxNode* child;
	for (child = firstChild; child; child = child->next)
		if (child->isElement(type))
			return (ZxElement*)child;
	return nullptr;
}

std::vector<ZxNode*> ZxNode::findAllElements(const char* type)
{
	std::vector<ZxNode*> results;
	findAllElements(type, results);
	return results;
}

void ZxNode::findAllElements(const char* type, std::vector<ZxNode*> results)
{
	ZxNode* child;

	if (isElement(type))
		results.push_back(this);
	for (child = firstChild; child; child = child->next)
		child->findAllElements(type, results);
}

std::vector<ZxNode*> ZxNode::findAllChildElements(const char* type)
{
	std::vector<ZxNode*> results;
	ZxNode*              child;

	for (child = firstChild; child; child = child->next)
		if (child->isElement(type))
			results.push_back(child);
	return results;
}

void ZxNode::addChild(ZxNode* child)
{
	if (lastChild)
	{
		lastChild->next = child;
		lastChild       = child;
	}
	else
	{
		firstChild = lastChild = child;
	}
	child->parent = this;
	child->next   = nullptr;
}

//------------------------------------------------------------------------

ZxDoc::ZxDoc()
{
	xmlDecl     = nullptr;
	docTypeDecl = nullptr;
	root        = nullptr;
}

ZxDoc* ZxDoc::loadMem(const char* data, size_t dataLen)
{
	ZxDoc* doc;

	doc = new ZxDoc();
	if (!doc->parse(data, dataLen))
	{
		delete doc;
		return nullptr;
	}
	return doc;
}

ZxDoc* ZxDoc::loadFile(const char* fileName)
{
	ZxDoc*   doc;
	FILE*    f;
	char*    data;
	uint32_t dataLen;

	if (!(f = fopen(fileName, "rb"))) return nullptr;
	fseek(f, 0, SEEK_END);
	dataLen = (uint32_t)ftell(f);
	if (!dataLen)
	{
		fclose(f);
		return nullptr;
	}
	fseek(f, 0, SEEK_SET);
	data = (char*)gmalloc(dataLen);
	if (fread(data, 1, dataLen, f) != dataLen)
	{
		fclose(f);
		gfree(data);
		return nullptr;
	}
	fclose(f);
	doc = loadMem(data, dataLen);
	gfree(data);
	return doc;
}

ZxDoc::~ZxDoc()
{
}

static bool writeToFileFunc(void* stream, const char* data, size_t length)
{
	return (fwrite(data, 1, length, (FILE*)stream) == length);
}

bool ZxDoc::writeFile(const char* fileName)
{
	FILE* f;

	if (!(f = fopen(fileName, "wb")))
		return false;
	write(&writeToFileFunc, f);
	fclose(f);
	return true;
}

void ZxDoc::addChild(ZxNode* node)
{
	if (node->isXMLDecl() && !xmlDecl)
		xmlDecl = (ZxXMLDecl*)node;
	else if (node->isDocTypeDecl() && !docTypeDecl)
		docTypeDecl = (ZxDocTypeDecl*)node;
	else if (node->isElement() && !root)
		root = (ZxElement*)node;
	ZxNode::addChild(node);
}

bool ZxDoc::parse(const char* data, size_t dataLen)
{
	parsePtr = data;
	parseEnd = data + dataLen;

	parseSpace();
	parseBOM();
	parseSpace();
	parseXMLDecl(this);
	parseMisc(this);
	parseDocTypeDecl(this);
	parseMisc(this);
	if (match("<"))
		parseElement(this);
	parseMisc(this);
	return root != nullptr;
}

void ZxDoc::parseXMLDecl(ZxNode* par)
{
	if (!match("<?xml")) return;
	parsePtr += 5;
	parseSpace();

	// version
	std::string version;
	if (match("version"))
	{
		parsePtr += 7;
		parseSpace();
		if (match("="))
		{
			++parsePtr;
			parseSpace();
			version = parseQuotedString();
		}
	}
	if (version.empty()) version = "1.0";
	parseSpace();

	// encoding
	std::string encoding;
	if (match("encoding"))
	{
		parsePtr += 8;
		parseSpace();
		if (match("="))
		{
			++parsePtr;
			parseSpace();
			encoding = parseQuotedString();
		}
	}
	parseSpace();

	// standalone
	bool standalone = false;
	if (match("standalone"))
	{
		parsePtr += 10;
		parseSpace();
		if (match("="))
		{
			++parsePtr;
			parseSpace();
			const auto s = parseQuotedString();
			standalone   = (s == "yes");
		}
	}
	parseSpace();

	if (match("?>"))
		parsePtr += 2;

	par->addChild(new ZxXMLDecl(version, encoding, standalone));
}

//~ this just skips everything after the name
void ZxDoc::parseDocTypeDecl(ZxNode* par)
{
	if (!match("<!DOCTYPE")) return;
	parsePtr += 9;
	parseSpace();

	const auto name = parseName();
	parseSpace();

	int  state = 0;
	char quote = '\0';
	while (parsePtr < parseEnd && state < 4)
	{
		const char c = *parsePtr++;
		switch (state)
		{
		case 0: // not in square brackets; not in quotes
			if (c == '>')
				state = 4;
			else if (c == '"' || c == '\'')
				state = 1;
			else if (c == '[')
				state = 2;
			break;
		case 1: // not in square brackets; in quotes
			if (c == quote)
				state = 0;
			break;
		case 2: // in square brackets; not in quotes
			if (c == ']')
				state = 0;
			else if (c == '"' || c == '\'')
				state = 3;
			break;
		case 3: // in square brackets; in quotes
			if (c == quote)
				state = 2;
			break;
		}
	}

	par->addChild(new ZxDocTypeDecl(name));
}

// assumes match("<")
void ZxDoc::parseElement(ZxNode* par)
{
	++parsePtr;
	const auto type = parseName();
	ZxElement* elem = new ZxElement(type);
	parseSpace();

	ZxAttr* attr;
	while ((attr = parseAttr()))
	{
		elem->addAttr(attr);
		parseSpace();
	}
	if (match("/>"))
	{
		parsePtr += 2;
	}
	else if (match(">"))
	{
		++parsePtr;
		parseContent(elem);
	}
	par->addChild(elem);
}

ZxAttr* ZxDoc::parseAttr()
{
	const char* start;
	char        quote, c;
	uint32_t    x;
	int         n;

	const auto name = parseName();
	parseSpace();
	if (!match("=")) return nullptr;
	++parsePtr;
	parseSpace();
	if (!(parsePtr < parseEnd && (*parsePtr == '"' || *parsePtr == '\'')))
		return nullptr;
	quote = *parsePtr++;
	std::string value;
	while (parsePtr < parseEnd && *parsePtr != quote)
	{
		if (*parsePtr == '&')
		{
			++parsePtr;
			if (parsePtr < parseEnd && *parsePtr == '#')
			{
				++parsePtr;
				if (parsePtr < parseEnd && *parsePtr == 'x')
				{
					++parsePtr;
					x = 0;
					while (parsePtr < parseEnd)
					{
						c = *parsePtr;
						if (c >= '0' && c <= '9')
							x = (x << 4) + (c - '0');
						else if (c >= 'a' && c <= 'f')
							x = (x << 4) + (10 + c - 'a');
						else if (c >= 'A' && c <= 'F')
							x = (x << 4) + (10 + c - 'A');
						else
							break;
						++parsePtr;
					}
					if (parsePtr < parseEnd && *parsePtr == ';')
						++parsePtr;
					appendUTF8(value, x);
				}
				else
				{
					x = 0;
					while (parsePtr < parseEnd)
					{
						c = *parsePtr;
						if (c >= '0' && c <= '9')
							x = x * 10 + (c - '0');
						else
							break;
						++parsePtr;
					}
					if (parsePtr < parseEnd && *parsePtr == ';')
						++parsePtr;
					appendUTF8(value, x);
				}
			}
			else
			{
				start = parsePtr;
				for (++parsePtr; parsePtr < parseEnd && *parsePtr != ';' && *parsePtr != quote && *parsePtr != '&'; ++parsePtr)
					;
				n = (int)(parsePtr - start);
				if (parsePtr < parseEnd && *parsePtr == ';')
					++parsePtr;
				if (n == 2 && !strncmp(start, "lt", 2))
					value += '<';
				else if (n == 2 && !strncmp(start, "gt", 2))
					value += '>';
				else if (n == 3 && !strncmp(start, "amp", 3))
					value += '&';
				else if (n == 4 && !strncmp(start, "apos", 4))
					value += '\'';
				else if (n == 4 && !strncmp(start, "quot", 4))
					value += '"';
				else
					value.append(start - 1, (int)(parsePtr - start) + 1);
			}
		}
		else
		{
			start = parsePtr;
			for (++parsePtr; parsePtr < parseEnd && *parsePtr != quote && *parsePtr != '&'; ++parsePtr)
				;
			value.append(start, (int)(parsePtr - start));
		}
	}
	if (parsePtr < parseEnd && *parsePtr == quote)
		++parsePtr;
	return new ZxAttr(name, value);
}

// this consumes the end tag
void ZxDoc::parseContent(ZxElement* par)
{
	std::string endType = "</";
	endType += par->getType();

	while (parsePtr < parseEnd)
	{
		if (match(endType.c_str()))
		{
			parsePtr += endType.size();
			parseSpace();
			if (match(">"))
				++parsePtr;
			break;
		}
		else if (match("<?"))
		{
			parsePI(par);
		}
		else if (match("<![CDATA["))
		{
			parseCDSect(par);
		}
		else if (match("<!--"))
		{
			parseComment(par);
		}
		else if (match("<"))
		{
			parseElement(par);
		}
		else
		{
			parseCharData(par);
		}
	}
}

void ZxDoc::parseCharData(ZxElement* par)
{
	std::string data;
	while (parsePtr < parseEnd && *parsePtr != '<')
	{
		if (*parsePtr == '&')
		{
			++parsePtr;
			if (parsePtr < parseEnd && *parsePtr == '#')
			{
				++parsePtr;
				if (parsePtr < parseEnd && *parsePtr == 'x')
				{
					++parsePtr;
					uint32_t x = 0;
					while (parsePtr < parseEnd)
					{
						uint32_t c = *parsePtr;
						if (c >= '0' && c <= '9')
							x = (x << 4) + (c - '0');
						else if (c >= 'a' && c <= 'f')
							x = (x << 4) + (10 + c - 'a');
						else if (c >= 'A' && c <= 'F')
							x = (x << 4) + (10 + c - 'A');
						else
							break;
						++parsePtr;
					}
					if (parsePtr < parseEnd && *parsePtr == ';')
						++parsePtr;
					appendUTF8(data, x);
				}
				else
				{
					uint32_t x = 0;
					while (parsePtr < parseEnd)
					{
						const char c = *parsePtr;
						if (c >= '0' && c <= '9')
							x = x * 10 + (c - '0');
						else
							break;
						++parsePtr;
					}
					if (parsePtr < parseEnd && *parsePtr == ';')
						++parsePtr;
					appendUTF8(data, x);
				}
			}
			else
			{
				const char* start = parsePtr;
				for (++parsePtr; parsePtr < parseEnd && *parsePtr != ';' && *parsePtr != '<' && *parsePtr != '&'; ++parsePtr)
					;
				int n = (int)(parsePtr - start);
				if (parsePtr < parseEnd && *parsePtr == ';')
					++parsePtr;
				if (n == 2 && !strncmp(start, "lt", 2))
					data += '<';
				else if (n == 2 && !strncmp(start, "gt", 2))
					data += '>';
				else if (n == 3 && !strncmp(start, "amp", 3))
					data += '&';
				else if (n == 4 && !strncmp(start, "apos", 4))
					data += '\'';
				else if (n == 4 && !strncmp(start, "quot", 4))
					data += '"';
				else
					data.append(start - 1, (int)(parsePtr - start) + 1);
			}
		}
		else
		{
			const char* start = parsePtr;
			for (++parsePtr; parsePtr < parseEnd && *parsePtr != '<' && *parsePtr != '&'; ++parsePtr)
				;
			data.append(start, (int)(parsePtr - start));
		}
	}
	par->addChild(new ZxCharData(data, true));
}

void ZxDoc::appendUTF8(std::string& s, uint32_t c)
{
	if (c <= 0x7f)
	{
		s += (char)c;
	}
	else if (c <= 0x7ff)
	{
		s += (char)(0xc0 + (c >> 6));
		s += (char)(0x80 + (c & 0x3f));
	}
	else if (c <= 0xffff)
	{
		s += (char)(0xe0 + (c >> 12));
		s += (char)(0x80 + ((c >> 6) & 0x3f));
		s += (char)(0x80 + (c & 0x3f));
	}
	else if (c <= 0x1fffff)
	{
		s += (char)(0xf0 + (c >> 18));
		s += (char)(0x80 + ((c >> 12) & 0x3f));
		s += (char)(0x80 + ((c >> 6) & 0x3f));
		s += (char)(0x80 + (c & 0x3f));
	}
	else if (c <= 0x3ffffff)
	{
		s += (char)(0xf8 + (c >> 24));
		s += (char)(0x80 + ((c >> 18) & 0x3f));
		s += (char)(0x80 + ((c >> 12) & 0x3f));
		s += (char)(0x80 + ((c >> 6) & 0x3f));
		s += (char)(0x80 + (c & 0x3f));
	}
	else if (c <= 0x7fffffff)
	{
		s += (char)(0xfc + (c >> 30));
		s += (char)(0x80 + ((c >> 24) & 0x3f));
		s += (char)(0x80 + ((c >> 18) & 0x3f));
		s += (char)(0x80 + ((c >> 12) & 0x3f));
		s += (char)(0x80 + ((c >> 6) & 0x3f));
		s += (char)(0x80 + (c & 0x3f));
	}
}

// assumes match("<![CDATA[")
void ZxDoc::parseCDSect(ZxNode* par)
{
	const char* start;

	parsePtr += 9;
	start = parsePtr;
	while (parsePtr < parseEnd - 3)
	{
		if (!strncmp(parsePtr, "]]>", 3))
		{
			par->addChild(new ZxCharData(std::string(start, (int)(parsePtr - start)), false));
			parsePtr += 3;
			return;
		}
		++parsePtr;
	}
	parsePtr = parseEnd;
	par->addChild(new ZxCharData(std::string(start, (int)(parsePtr - start)), false));
}

void ZxDoc::parseMisc(ZxNode* par)
{
	while (1)
		if (match("<!--"))
			parseComment(par);
		else if (match("<?"))
			parsePI(par);
		else if (parsePtr < parseEnd && (*parsePtr == '\x20' || *parsePtr == '\x09' || *parsePtr == '\x0d' || *parsePtr == '\x0a'))
			++parsePtr;
		else
			break;
}

// assumes match("<!--")
void ZxDoc::parseComment(ZxNode* par)
{
	const char* start;

	parsePtr += 4;
	start = parsePtr;
	while (parsePtr <= parseEnd - 3)
	{
		if (!strncmp(parsePtr, "-->", 3))
		{
			par->addChild(new ZxComment(std::string(start, (int)(parsePtr - start))));
			parsePtr += 3;
			return;
		}
		++parsePtr;
	}
	parsePtr = parseEnd;
}

// assumes match("<?")
void ZxDoc::parsePI(ZxNode* par)
{
	parsePtr += 2;
	const auto target = parseName();
	parseSpace();
	const char* start = parsePtr;
	while (parsePtr <= parseEnd - 2)
	{
		if (!strncmp(parsePtr, "?>", 2))
		{
			par->addChild(new ZxPI(target, std::string(start, (int)(parsePtr - start))));
			parsePtr += 2;
			return;
		}
		++parsePtr;
	}
	parsePtr = parseEnd;
	par->addChild(new ZxPI(target, std::string(start, (int)(parsePtr - start))));
}

//~ this accepts all chars >= 0x80
//~ this doesn't check for properly-formed UTF-8
std::string ZxDoc::parseName()
{
	std::string name;
	if (parsePtr < parseEnd && nameStartChar[*parsePtr & 0xff])
	{
		name += (*parsePtr++);
		while (parsePtr < parseEnd && nameChar[*parsePtr & 0xff])
			name += (*parsePtr++);
	}
	return name;
}

std::string ZxDoc::parseQuotedString()
{
	std::string s;
	if (parsePtr < parseEnd && (*parsePtr == '"' || *parsePtr == '\''))
	{
		const char  quote = *parsePtr++;
		const char* start = parsePtr;
		while (parsePtr < parseEnd && *parsePtr != quote)
			++parsePtr;

		s.assign(start, (int)(parsePtr - start));
		if (parsePtr < parseEnd && *parsePtr == quote)
			++parsePtr;
	}
	else
	{
	}
	return s;
}

void ZxDoc::parseBOM()
{
	if (match("\xef\xbb\xbf"))
		parsePtr += 3;
}

void ZxDoc::parseSpace()
{
	while (parsePtr < parseEnd && (*parsePtr == '\x20' || *parsePtr == '\x09' || *parsePtr == '\x0d' || *parsePtr == '\x0a'))
		++parsePtr;
}

bool ZxDoc::match(const char* s)
{
	const int n = (int)strlen(s);
	return parseEnd - parsePtr >= n && !strncmp(parsePtr, s, n);
}

bool ZxDoc::write(ZxWriteFunc writeFunc, void* stream)
{
	ZxNode* child;
	for (child = getFirstChild(); child; child = child->getNextChild())
	{
		if (!child->write(writeFunc, stream)) return false;
		if (!(*writeFunc)(stream, "\n", 1)) return false;
	}
	return true;
}

//------------------------------------------------------------------------

ZxXMLDecl::ZxXMLDecl(const std::string& versionA, const std::string& encodingA, bool standaloneA)
{
	version    = versionA;
	encoding   = encodingA;
	standalone = standaloneA;
}

ZxXMLDecl::~ZxXMLDecl()
{
}

bool ZxXMLDecl::write(ZxWriteFunc writeFunc, void* stream)
{
	std::string s = "<?xml version=\"";
	s += version;
	s += "\"";
	if (encoding.size())
	{
		s += " encoding=\"";
		s += encoding;
		s += "\"";
	}
	if (standalone)
		s += " standlone=\"yes\"";
	s += "?>";

	return (*writeFunc)(stream, s.c_str(), s.size());
}

//------------------------------------------------------------------------

ZxDocTypeDecl::ZxDocTypeDecl(const std::string& nameA)
{
	name = nameA;
}

ZxDocTypeDecl::~ZxDocTypeDecl()
{
}

bool ZxDocTypeDecl::write(ZxWriteFunc writeFunc, void* stream)
{
	std::string s = "<!DOCTYPE ";
	s += name;
	s += ">";
	return (*writeFunc)(stream, s.c_str(), s.size());
}

//------------------------------------------------------------------------

ZxComment::ZxComment(const std::string& textA)
{
	text = textA;
}

ZxComment::~ZxComment()
{
}

bool ZxComment::write(ZxWriteFunc writeFunc, void* stream)
{
	std::string s = "<!--";
	s += text;
	s += "-->";
	return (*writeFunc)(stream, s.c_str(), s.size());
}

//------------------------------------------------------------------------

ZxPI::ZxPI(const std::string& targetA, const std::string& textA)
{
	target = targetA;
	text   = textA;
}

ZxPI::~ZxPI()
{
}

bool ZxPI::write(ZxWriteFunc writeFunc, void* stream)
{
	std::string s = "<?";
	s += target;
	s += " ";
	s += text;
	s += "?>";
	return (*writeFunc)(stream, s.c_str(), s.size());
}

//------------------------------------------------------------------------

ZxElement::ZxElement(const std::string& typeA)
{
	type      = typeA;
	firstAttr = lastAttr = nullptr;
}

ZxElement::~ZxElement()
{
}

bool ZxElement::isElement(const char* typeA)
{
	return type == typeA;
}

ZxAttr* ZxElement::findAttr(const char* attrName)
{
	if (const auto& it = attrs.find(attrName); it != attrs.end())
		return it->second;
	return nullptr;
}

void ZxElement::addAttr(ZxAttr* attr)
{
	attrs.emplace(attr->getName(), attr);
	if (lastAttr)
	{
		lastAttr->next = attr;
		lastAttr       = attr;
	}
	else
	{
		firstAttr = lastAttr = attr;
	}
	attr->parent = this;
	attr->next   = nullptr;
}

bool ZxElement::write(ZxWriteFunc writeFunc, void* stream)
{
	ZxAttr* attr;
	ZxNode* child;

	std::string s = "<";
	s += type;
	for (attr = firstAttr; attr; attr = attr->getNextAttr())
	{
		s += " ";
		s += attr->name;
		s += "=\"";
		appendEscapedAttrValue(s, attr->value);
		s += "\"";
	}
	if ((child = getFirstChild()))
		s += ">";
	else
		s += "/>";

	const bool ok = (*writeFunc)(stream, s.c_str(), s.size());
	if (!ok) return false;

	if (child)
	{
		for (; child; child = child->getNextChild())
			if (!child->write(writeFunc, stream))
				return false;
		std::string s;
		s += "</";
		s += type;
		s += ">";

		const bool ok = (*writeFunc)(stream, s.c_str(), s.size());
		if (!ok) return false;
	}
	return true;
}

void ZxElement::appendEscapedAttrValue(std::string& out, const std::string& s)
{
	for (int i = 0; i < s.size(); ++i)
	{
		const char c = s.at(i);
		if (c == '<')
			out += "&lt;";
		else if (c == '>')
			out += "&gt;";
		else if (c == '&')
			out += "&amp;";
		else if (c == '"')
			out += "&quot;";
		else
			out += c;
	}
}

//------------------------------------------------------------------------

ZxAttr::ZxAttr(const std::string& nameA, const std::string& valueA)
{
	name   = nameA;
	value  = valueA;
	parent = nullptr;
	next   = nullptr;
}

ZxAttr::~ZxAttr()
{
}

//------------------------------------------------------------------------

ZxCharData::ZxCharData(const std::string& dataA, bool parsedA)
{
	data   = dataA;
	parsed = parsedA;
}

ZxCharData::~ZxCharData()
{
}

bool ZxCharData::write(ZxWriteFunc writeFunc, void* stream)
{
	std::string s;
	if (parsed)
	{
		for (int i = 0; i < data.size(); ++i)
		{
			const char c = data.at(i);
			if (c == '<')
				s += "&lt;";
			else if (c == '>')
				s += "&gt;";
			else if (c == '&')
				s += "&amp;";
			else
				s += c;
		}
	}
	else
	{
		s += "<![CDATA[";
		s += data;
		s += "]]>";
	}

	return (*writeFunc)(stream, s.c_str(), s.size());
}
