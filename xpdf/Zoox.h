//========================================================================
//
// Zoox.h
//
//========================================================================

#pragma once

#include <aconf.h>

class ZxAttr;
class ZxDocTypeDecl;
class ZxElement;
class ZxXMLDecl;

//------------------------------------------------------------------------

typedef bool (*ZxWriteFunc)(void* stream, const char* data, size_t length);

//------------------------------------------------------------------------

class ZxNode
{
public:
	ZxNode();
	virtual ~ZxNode();

	virtual bool isDoc() { return false; }

	virtual bool isXMLDecl() { return false; }

	virtual bool isDocTypeDecl() { return false; }

	virtual bool isComment() { return false; }

	virtual bool isPI() { return false; }

	virtual bool isElement() { return false; }

	virtual bool isElement(const char* type) { return false; }

	virtual bool isCharData() { return false; }

	virtual ZxNode* getFirstChild() { return firstChild; }

	virtual ZxNode* getNextChild() { return next; }

	ZxNode* getParent() { return parent; }

	ZxNode*              deleteChild(ZxNode* child);
	void                 appendChild(ZxNode* child);
	void                 insertChildAfter(ZxNode* child, ZxNode* prev);
	ZxElement*           findFirstElement(const char* type);
	ZxElement*           findFirstChildElement(const char* type);
	std::vector<ZxNode*> findAllElements(const char* type);
	std::vector<ZxNode*> findAllChildElements(const char* type);
	virtual void         addChild(ZxNode* child);

	virtual bool write(ZxWriteFunc writeFunc, void* stream) = 0;

protected:
	void findAllElements(const char* type, std::vector<ZxNode*> results);

	ZxNode* next       = nullptr; //
	ZxNode* parent     = nullptr; //
	ZxNode* firstChild = nullptr; //
	ZxNode* lastChild  = nullptr; //
};

//------------------------------------------------------------------------

class ZxDoc : public ZxNode
{
public:
	ZxDoc();

	// Parse from memory.  Returns nullptr on error.
	static ZxDoc* loadMem(const char* data, size_t dataLen);

	// Parse from disk.  Returns nullptr on error.
	static ZxDoc* loadFile(const char* fileName);

	virtual ~ZxDoc();

	// Write to disk.  Returns false on error.
	bool writeFile(const char* fileName);

	virtual bool isDoc() { return true; }

	ZxXMLDecl* getXMLDecl() { return xmlDecl; }

	ZxDocTypeDecl* getDocTypeDecl() { return docTypeDecl; }

	ZxElement* getRoot() { return root; }

	virtual void addChild(ZxNode* node);

	virtual bool write(ZxWriteFunc writeFunc, void* stream);

private:
	bool        parse(const char* data, size_t dataLen);
	void        parseXMLDecl(ZxNode* par);
	void        parseDocTypeDecl(ZxNode* par);
	void        parseElement(ZxNode* par);
	ZxAttr*     parseAttr();
	void        parseContent(ZxElement* par);
	void        parseCharData(ZxElement* par);
	void        appendUTF8(std::string& s, uint32_t c);
	void        parseCDSect(ZxNode* par);
	void        parseMisc(ZxNode* par);
	void        parseComment(ZxNode* par);
	void        parsePI(ZxNode* par);
	std::string parseName();
	std::string parseQuotedString();
	void        parseBOM();
	void        parseSpace();
	bool        match(const char* s);

	ZxXMLDecl*     xmlDecl     = nullptr; // may be nullptr
	ZxDocTypeDecl* docTypeDecl = nullptr; // may be nullptr
	ZxElement*     root        = nullptr; // may be nullptr
	const char*    parsePtr    = nullptr; //
	const char*    parseEnd    = nullptr; //
};

//------------------------------------------------------------------------

class ZxXMLDecl : public ZxNode
{
public:
	ZxXMLDecl(const std::string& versionA, const std::string& encodingA, bool standaloneA);
	virtual ~ZxXMLDecl();

	virtual bool isXMLDecl() { return true; }

	std::string getVersion() { return version; }

	std::string getEncoding() { return encoding; }

	bool getStandalone() { return standalone; }

	virtual bool write(ZxWriteFunc writeFunc, void* stream);

private:
	std::string version    = "";    //
	std::string encoding   = "";    // may be ""
	bool        standalone = false; //
};

//------------------------------------------------------------------------

class ZxDocTypeDecl : public ZxNode
{
public:
	ZxDocTypeDecl(const std::string& nameA);
	virtual ~ZxDocTypeDecl();

	virtual bool isDocTypeDecl() { return true; }

	std::string_view getName() { return name; }

	virtual bool write(ZxWriteFunc writeFunc, void* stream);

private:
	std::string name = ""; //
};

//------------------------------------------------------------------------

class ZxComment : public ZxNode
{
public:
	ZxComment(const std::string& textA);
	virtual ~ZxComment();

	virtual bool isComment() { return true; }

	std::string getText() { return text; }

	virtual bool write(ZxWriteFunc writeFunc, void* stream);

private:
	std::string text = ""; //
};

//------------------------------------------------------------------------

class ZxPI : public ZxNode
{
public:
	ZxPI(const std::string& targetA, const std::string& textA);
	virtual ~ZxPI();

	virtual bool isPI() { return true; }

	std::string getTarget() { return target; }

	std::string getText() { return text; }

	virtual bool write(ZxWriteFunc writeFunc, void* stream);

private:
	std::string target = ""; //
	std::string text   = ""; //
};

//------------------------------------------------------------------------

class ZxElement : public ZxNode
{
public:
	ZxElement(const std::string& typeA);
	virtual ~ZxElement();

	virtual bool isElement() { return true; }

	virtual bool isElement(const char* typeA);

	std::string getType() { return type; }

	ZxAttr* findAttr(const char* attrName);

	ZxAttr* getFirstAttr() { return firstAttr; }

	void addAttr(ZxAttr* attr);

	virtual bool write(ZxWriteFunc writeFunc, void* stream);

private:
	void appendEscapedAttrValue(std::string& out, const std::string& s);

	std::string                type      = "";      //
	UMAP<std::string, ZxAttr*> attrs     = {};      // [ZxAttr]
	ZxAttr*                    firstAttr = nullptr; //
	ZxAttr*                    lastAttr  = nullptr; //
};

//------------------------------------------------------------------------

class ZxAttr
{
public:
	ZxAttr(const std::string& nameA, const std::string& valueA);
	~ZxAttr();

	std::string_view getName() { return name; }

	std::string getValue() { return value; }

	ZxAttr* getNextAttr() { return next; }

	ZxNode* getParent() { return parent; }

private:
	std::string name   = "";      //
	std::string value  = "";      //
	ZxElement*  parent = nullptr; //
	ZxAttr*     next   = nullptr; //

	friend class ZxElement;
};

//------------------------------------------------------------------------

class ZxCharData : public ZxNode
{
public:
	ZxCharData(const std::string& dataA, bool parsedA);
	virtual ~ZxCharData();

	virtual bool isCharData() { return true; }

	std::string getData() { return data; }

	bool isParsed() { return parsed; }

	virtual bool write(ZxWriteFunc writeFunc, void* stream);

private:
	std::string data   = "";    // in UTF-8 format
	bool        parsed = false; //
};
