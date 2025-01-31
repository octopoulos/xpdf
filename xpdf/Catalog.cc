//========================================================================
//
// Catalog.cc
//
// Copyright 1996-2013 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include "gmem.h"
#include "gmempp.h"
#include "gfile.h"
#include "GList.h"
#include "Object.h"
#include "CharTypes.h"
#include "PDFDoc.h"
#include "XRef.h"
#include "Dict.h"
#include "Page.h"
#include "Error.h"
#include "Link.h"
#include "AcroForm.h"
#include "TextString.h"
#include "Catalog.h"

//------------------------------------------------------------------------
// PageTreeNode
//------------------------------------------------------------------------

class PageTreeNode
{
public:
	PageTreeNode(Ref refA, int countA, PageTreeNode* parentA);
	~PageTreeNode();

	Ref           ref    = {};
	int           count  = 0;
	PageTreeNode* parent = nullptr;
	GList*        kids   = nullptr; // [PageTreeNode]
	PageAttrs*    attrs  = nullptr;
};

PageTreeNode::PageTreeNode(Ref refA, int countA, PageTreeNode* parentA)
{
	ref    = refA;
	count  = countA;
	parent = parentA;
	kids   = nullptr;
	attrs  = nullptr;
}

PageTreeNode::~PageTreeNode()
{
	delete attrs;
	if (kids) deleteGList(kids, PageTreeNode);
}

//------------------------------------------------------------------------
// EmbeddedFile
//------------------------------------------------------------------------

class EmbeddedFile
{
public:
	EmbeddedFile(TextString* nameA, Object* streamRefA);
	~EmbeddedFile();

	TextString* name      = nullptr;
	Object      streamRef = {};
};

EmbeddedFile::EmbeddedFile(TextString* nameA, Object* streamRefA)
{
	name = nameA;
	streamRefA->copy(&streamRef);
}

EmbeddedFile::~EmbeddedFile()
{
	delete name;
	streamRef.free();
}

//------------------------------------------------------------------------
// PageLabelNode
//------------------------------------------------------------------------

class PageLabelNode
{
public:
	PageLabelNode(int firstPageA, Dict* dict);
	~PageLabelNode();

	int         firstPage = 0;       // first page number covered by this node
	int         lastPage  = 0;       // last page number covered by this node
	TextString* prefix    = nullptr; // label prefix (may be empty)
	int         start     = 0;       // value of the numeric portion of this label for the first page in the range
	char        style     = 0;       // page label style
};

PageLabelNode::PageLabelNode(int firstPageA, Dict* dict)
{
	Object prefixObj, styleObj, startObj;

	// convert page index to page number
	firstPage = firstPageA + 1;

	// lastPage will be filled in later
	lastPage = -1;

	if (dict->lookup("P", &prefixObj)->isString())
		prefix = new TextString(prefixObj.getString());
	else
		prefix = new TextString();
	prefixObj.free();

	style = '\0';
	if (dict->lookup("S", &styleObj)->isName())
	{
		if (strlen(styleObj.getName()) == 1)
			style = styleObj.getName()[0];
	}
	styleObj.free();

	start = 1;
	if (dict->lookup("St", &startObj)->isInt())
		start = startObj.getInt();
	startObj.free();
}

PageLabelNode::~PageLabelNode()
{
	delete prefix;
}

//------------------------------------------------------------------------
// Catalog
//------------------------------------------------------------------------

Catalog::Catalog(PDFDoc* docA)
{
	Object catDict;
	Object obj, obj2;

	doc  = docA;
	xref = doc->getXRef();
#if MULTITHREADED
	gInitMutex(&pageMutex);
#endif

	xref->getCatalog(&catDict);
	if (!catDict.isDict())
	{
		error(errSyntaxError, -1, "Catalog object is wrong type ({})", catDict.getTypeName());
		goto err1;
	}

	// read page tree
	if (!readPageTree(&catDict))
		goto err1;

	// read named destination dictionary
	catDict.dictLookup("Dests", &dests);

	// read root of named destination tree
	if (catDict.dictLookup("Names", &obj)->isDict())
		obj.dictLookup("Dests", &nameTree);
	else
		nameTree.initNull();
	obj.free();

	// read base URI
	if (catDict.dictLookup("URI", &obj)->isDict())
	{
		if (obj.dictLookup("Base", &obj2)->isString())
			baseURI = obj2.getString();
		obj2.free();
	}
	obj.free();
	if (baseURI.empty())
	{
		if (doc->getFileName().size())
		{
			baseURI = makePathAbsolute(grabPath(doc->getFileName().c_str()));
			if (baseURI.at(0) == '/')
				baseURI.insert(0, "file://localhost");
			else
				baseURI.insert(0, "file://localhost/");
		}
		else
		{
			baseURI = "file://localhost/";
		}
	}

	// get the metadata stream
	catDict.dictLookup("Metadata", &metadata);

	// get the structure tree root
	catDict.dictLookup("StructTreeRoot", &structTreeRoot);

	// get the outline dictionary
	catDict.dictLookup("Outlines", &outline);

	// get the AcroForm dictionary
	catDict.dictLookup("AcroForm", &acroForm);

	// get the NeedsRendering flag
	// NB: AcroForm::load() uses this value
	needsRendering = catDict.dictLookup("NeedsRendering", &obj)->isBool() && obj.getBool();
	obj.free();

	// create the Form
	// (if acroForm is a null object, this will still create an AcroForm
	// if there are unattached Widget-type annots)
	form = AcroForm::load(doc, this, &acroForm);

	// get the OCProperties dictionary
	catDict.dictLookup("OCProperties", &ocProperties);

	// get the list of embedded files
	readEmbeddedFileList(catDict.getDict());

	// get the ViewerPreferences object
	catDict.dictLookupNF("ViewerPreferences", &viewerPrefs);

	if (catDict.dictLookup("PageLabels", &obj)->isDict())
		readPageLabelTree(&obj);
	obj.free();

	catDict.free();
	return;

err1:
	catDict.free();
	dests.initNull();
	nameTree.initNull();
	ok = false;
}

Catalog::~Catalog()
{
	if (pageTree)
		delete pageTree;
	if (pages)
	{
		for (int i = 0; i < numPages; ++i)
			if (pages[i]) delete pages[i];
		gfree(pages);
		gfree(pageRefs);
	}
#if MULTITHREADED
	gDestroyMutex(&pageMutex);
#endif
	dests.free();
	nameTree.free();
	metadata.free();
	structTreeRoot.free();
	outline.free();
	acroForm.free();
	if (form) delete form;
	ocProperties.free();
	if (embeddedFiles)
		deleteGList(embeddedFiles, EmbeddedFile);
	if (pageLabels)
		deleteGList(pageLabels, PageLabelNode);
	viewerPrefs.free();
}

Page* Catalog::getPage(int i)
{
#if MULTITHREADED
	gLockMutex(&pageMutex);
#endif
	if (!pages[i - 1])
		loadPage(i);
	Page* page = pages[i - 1];
#if MULTITHREADED
	gUnlockMutex(&pageMutex);
#endif
	return page;
}

Ref* Catalog::getPageRef(int i)
{
#if MULTITHREADED
	gLockMutex(&pageMutex);
#endif
	if (!pages[i - 1])
		loadPage(i);
	Ref* pageRef = &pageRefs[i - 1];
#if MULTITHREADED
	gUnlockMutex(&pageMutex);
#endif
	return pageRef;
}

void Catalog::doneWithPage(int i)
{
#if MULTITHREADED
	gLockMutex(&pageMutex);
#endif
	if (pages[i - 1])
	{
		delete pages[i - 1];
		pages[i - 1] = nullptr;
	}
#if MULTITHREADED
	gUnlockMutex(&pageMutex);
#endif
}

std::string Catalog::readMetadata()
{
	Object obj;
	char   buf[4096];

	if (!metadata.isStream())
		return "";
	Dict* dict = metadata.streamGetDict();
	if (!dict->lookup("Subtype", &obj)->isName("XML"))
		error(errSyntaxWarning, -1, "Unknown Metadata type: '{}'", obj.isName() ? obj.getName() : "???");
	obj.free();

	std::string s;
	metadata.streamReset();
	int n;
	while ((n = metadata.streamGetBlock(buf, sizeof(buf))) > 0)
		s.append(buf, n);
	metadata.streamClose();
	return s;
}

int Catalog::findPage(int num, int gen)
{
#if MULTITHREADED
	gLockMutex(&pageMutex);
#endif
	for (int i = 0; i < numPages; ++i)
	{
		if (!pages[i])
			loadPage(i + 1);
		if (pageRefs[i].num == num && pageRefs[i].gen == gen)
		{
#if MULTITHREADED
			gUnlockMutex(&pageMutex);
#endif
			return i + 1;
		}
	}
#if MULTITHREADED
	gUnlockMutex(&pageMutex);
#endif
	return 0;
}

LinkDest* Catalog::findDest(const std::string& name)
{
	LinkDest* dest;
	Object    obj1, obj2;

	// try named destination dictionary then name tree
	bool found = false;
	if (dests.isDict())
	{
		if (!dests.dictLookup(name.c_str(), &obj1)->isNull())
			found = true;
		else
			obj1.free();
	}
	if (!found && nameTree.isDict())
	{
		if (!findDestInTree(&nameTree, name, &obj1)->isNull())
			found = true;
		else
			obj1.free();
	}
	if (!found)
		return nullptr;

	// construct LinkDest
	dest = nullptr;
	if (obj1.isArray())
	{
		dest = new LinkDest(obj1.getArray());
	}
	else if (obj1.isDict())
	{
		if (obj1.dictLookup("D", &obj2)->isArray())
			dest = new LinkDest(obj2.getArray());
		else
			error(errSyntaxWarning, -1, "Bad named destination value");
		obj2.free();
	}
	else
	{
		error(errSyntaxWarning, -1, "Bad named destination value");
	}
	obj1.free();
	if (dest && !dest->isOk())
	{
		delete dest;
		dest = nullptr;
	}

	return dest;
}

Object* Catalog::findDestInTree(Object* tree, const std::string& name, Object* obj)
{
	Object names, name1;
	Object kids, kid, limits, low, high;
	bool   done, found;

	// leaf node
	if (tree->dictLookup("Names", &names)->isArray())
	{
		done = found = false;
		for (int i = 0; !done && i < names.arrayGetLength(); i += 2)
		{
			if (names.arrayGet(i, &name1)->isString())
			{
				const auto& name1Str = name1.getString();
				if (name == name1Str)
				{
					names.arrayGet(i + 1, obj);
					found = true;
					done  = true;
				}
				else if (name < name1Str)
				{
					done = true;
				}
			}
			name1.free();
		}
		names.free();
		if (!found)
			obj->initNull();
		return obj;
	}
	names.free();

	// root or intermediate node
	done = false;
	if (tree->dictLookup("Kids", &kids)->isArray())
	{
		for (int i = 0; !done && i < kids.arrayGetLength(); ++i)
		{
			if (kids.arrayGet(i, &kid)->isDict())
			{
				if (kid.dictLookup("Limits", &limits)->isArray())
				{
					if (limits.arrayGet(0, &low)->isString() && name >= low.getString())
					{
						if (limits.arrayGet(1, &high)->isString() && name <= high.getString())
						{
							findDestInTree(&kid, name, obj);
							done = true;
						}
						high.free();
					}
					low.free();
				}
				limits.free();
			}
			kid.free();
		}
	}
	kids.free();

	// name was outside of ranges of all kids
	if (!done)
		obj->initNull();

	return obj;
}

bool Catalog::readPageTree(Object* catDict)
{
	Object topPagesRef, topPagesObj, countObj;

	if (!catDict->dictLookupNF("Pages", &topPagesRef)->isRef())
	{
		error(errSyntaxError, -1, "Top-level pages reference is wrong type ({})", topPagesRef.getTypeName());
		topPagesRef.free();
		return false;
	}
	if (!topPagesRef.fetch(xref, &topPagesObj)->isDict())
	{
		error(errSyntaxError, -1, "Top-level pages object is wrong type ({})", topPagesObj.getTypeName());
		topPagesObj.free();
		topPagesRef.free();
		return false;
	}
	if (topPagesObj.dictLookup("Count", &countObj)->isInt())
	{
		numPages = countObj.getInt();
		if (numPages == 0 || numPages > 50000)
		{
			// 1. Acrobat apparently scans the page tree if it sees a zero
			//    count.
			// 2. Absurdly large page counts result in very slow loading,
			//    because other code tries to fetch pages 1 through n.
			// In both cases: ignore the given page count and scan the tree
			// instead.
			char* touchedObjs = (char*)gmalloc(xref->getNumObjects());
			memset(touchedObjs, 0, xref->getNumObjects());
			numPages = countPageTree(&topPagesRef, touchedObjs);
			gfree(touchedObjs);
		}
	}
	else
	{
		// assume we got a Page node instead of a Pages node
		numPages = 1;
	}
	countObj.free();
	if (numPages < 0)
	{
		error(errSyntaxError, -1, "Invalid page count");
		topPagesObj.free();
		topPagesRef.free();
		numPages = 0;
		return false;
	}
	pageTree = new PageTreeNode(topPagesRef.getRef(), numPages, nullptr);
	topPagesObj.free();
	topPagesRef.free();
	pages    = (Page**)greallocn(pages, numPages, sizeof(Page*));
	pageRefs = (Ref*)greallocn(pageRefs, numPages, sizeof(Ref));
	for (int i = 0; i < numPages; ++i)
	{
		pages[i]        = nullptr;
		pageRefs[i].num = -1;
		pageRefs[i].gen = -1;
	}
	return true;
}

int Catalog::countPageTree(Object* pagesNodeRef, char* touchedObjs)
{
	// check for invalid reference
	if (pagesNodeRef->isRef() && (pagesNodeRef->getRefNum() < 0 || pagesNodeRef->getRefNum() >= xref->getNumObjects()))
		return 0;

	// check for a page tree loop; fetch the node object
	Object pagesNode;
	if (pagesNodeRef->isRef())
	{
		if (touchedObjs[pagesNodeRef->getRefNum()])
		{
			error(errSyntaxError, -1, "Loop in Pages tree");
			return 0;
		}
		touchedObjs[pagesNodeRef->getRefNum()] = 1;
		xref->fetch(pagesNodeRef->getRefNum(), pagesNodeRef->getRefGen(), &pagesNode);
	}
	else
	{
		pagesNodeRef->copy(&pagesNode);
	}

	// count the subtree
	int n = 0;
	if (pagesNode.isDict())
	{
		Object kids;
		if (pagesNode.dictLookup("Kids", &kids)->isArray())
		{
			for (int i = 0; i < kids.arrayGetLength(); ++i)
			{
				Object kid;
				kids.arrayGetNF(i, &kid);
				int n2 = countPageTree(&kid, touchedObjs);
				if (n2 < INT_MAX - n)
				{
					n += n2;
				}
				else
				{
					error(errSyntaxError, -1, "Page tree contains too many pages");
					n = INT_MAX;
				}
				kid.free();
			}
		}
		else
		{
			n = 1;
		}
		kids.free();
	}

	pagesNode.free();

	return n;
}

void Catalog::loadPage(int pg)
{
	loadPage2(pg, pg - 1, pageTree);
}

void Catalog::loadPage2(int pg, int relPg, PageTreeNode* node)
{
	Object        pageRefObj, pageObj, kidsObj, kidRefObj, kidObj, countObj;
	PageTreeNode *kidNode, *p;
	PageAttrs*    attrs;

	if (relPg >= node->count)
	{
		error(errSyntaxError, -1, "Internal error in page tree");
		pages[pg - 1] = new Page(doc, pg);
		return;
	}

	// if this node has not been filled in yet, it's either a leaf node
	// or an unread internal node
	if (!node->kids)
	{
		// check for a loop in the page tree
		for (p = node->parent; p; p = p->parent)
		{
			if (node->ref.num == p->ref.num && node->ref.gen == p->ref.gen)
			{
				error(errSyntaxError, -1, "Loop in Pages tree");
				pages[pg - 1] = new Page(doc, pg);
				return;
			}
		}

		// fetch the Page/Pages object
		pageRefObj.initRef(node->ref.num, node->ref.gen);
		if (!pageRefObj.fetch(xref, &pageObj)->isDict())
		{
			error(errSyntaxError, -1, "Page tree object is wrong type ({})", pageObj.getTypeName());
			pageObj.free();
			pageRefObj.free();
			pages[pg - 1] = new Page(doc, pg);
			return;
		}

		// merge the PageAttrs
		attrs = new PageAttrs(node->parent ? node->parent->attrs : (PageAttrs*)nullptr, pageObj.getDict(), xref);

		// if "Kids" exists, it's an internal node
		if (pageObj.dictLookup("Kids", &kidsObj)->isArray())
		{
			// save the PageAttrs
			node->attrs = attrs;

			// read the kids
			node->kids = new GList();
			for (int i = 0; i < kidsObj.arrayGetLength(); ++i)
			{
				if (kidsObj.arrayGetNF(i, &kidRefObj)->isRef())
				{
					if (kidRefObj.fetch(xref, &kidObj)->isDict())
					{
						int count;
						if (kidObj.dictLookup("Count", &countObj)->isInt())
							count = countObj.getInt();
						else
							count = 1;
						countObj.free();
						node->kids->append(new PageTreeNode(kidRefObj.getRef(), count, node));
					}
					else
					{
						error(errSyntaxError, -1, "Page tree object is wrong type ({})", kidObj.getTypeName());
					}
					kidObj.free();
				}
				else
				{
					error(errSyntaxError, -1, "Page tree reference is wrong type ({})", kidRefObj.getTypeName());
				}
				kidRefObj.free();
			}
		}
		else
		{
			// create the Page object
			pageRefs[pg - 1] = node->ref;
			pages[pg - 1]    = new Page(doc, pg, pageObj.getDict(), attrs);
			if (!pages[pg - 1]->isOk())
			{
				delete pages[pg - 1];
				pages[pg - 1] = new Page(doc, pg);
			}
		}

		kidsObj.free();
		pageObj.free();
		pageRefObj.free();
	}

	// recursively descend the tree
	if (node->kids)
	{
		int i;
		for (i = 0; i < node->kids->getLength(); ++i)
		{
			kidNode = (PageTreeNode*)node->kids->get(i);
			if (relPg < kidNode->count)
			{
				loadPage2(pg, relPg, kidNode);
				break;
			}
			relPg -= kidNode->count;
		}

		// this will only happen if the page tree is invalid
		// (i.e., parent count > sum of children counts)
		if (i == node->kids->getLength())
		{
			error(errSyntaxError, -1, "Invalid page count in page tree");
			pages[pg - 1] = new Page(doc, pg);
		}
	}
}

Object* Catalog::getDestOutputProfile(Object* destOutProf)
{
	Object catDict, intents, intent, subtype;

	if (!xref->getCatalog(&catDict)->isDict())
		goto err1;
	if (!catDict.dictLookup("OutputIntents", &intents)->isArray())
		goto err2;
	for (int i = 0; i < intents.arrayGetLength(); ++i)
	{
		intents.arrayGet(i, &intent);
		if (!intent.isDict())
		{
			intent.free();
			continue;
		}
		if (!intent.dictLookup("S", &subtype)->isName("GTS_PDFX"))
		{
			subtype.free();
			intent.free();
			continue;
		}
		subtype.free();
		if (!intent.dictLookup("DestOutputProfile", destOutProf)->isStream())
		{
			destOutProf->free();
			intent.free();
			goto err2;
		}
		intent.free();
		intents.free();
		catDict.free();
		return destOutProf;
	}

err2:
	intents.free();
err1:
	catDict.free();
	return nullptr;
}

void Catalog::readEmbeddedFileList(Dict* catDict)
{
	Object obj1, obj2;
	char*  touchedObjs;

	touchedObjs = (char*)gmalloc(xref->getNumObjects());
	memset(touchedObjs, 0, xref->getNumObjects());

	// read the embedded file name tree
	if (catDict->lookup("Names", &obj1)->isDict())
	{
		obj1.dictLookupNF("EmbeddedFiles", &obj2);
		readEmbeddedFileTree(&obj2, touchedObjs);
		obj2.free();
	}
	obj1.free();

	// look for file attachment annotations
	readFileAttachmentAnnots(catDict->lookupNF("Pages", &obj1), touchedObjs);
	obj1.free();

	gfree(touchedObjs);
}

void Catalog::readEmbeddedFileTree(Object* nodeRef, char* touchedObjs)
{
	Object node, kidsObj, kidObj;
	Object namesObj, nameObj, fileSpecObj;

	// check for an object loop
	if (nodeRef->isRef())
	{
		if (nodeRef->getRefNum() < 0 || nodeRef->getRefNum() >= xref->getNumObjects() || touchedObjs[nodeRef->getRefNum()])
			return;
		touchedObjs[nodeRef->getRefNum()] = 1;
		xref->fetch(nodeRef->getRefNum(), nodeRef->getRefGen(), &node);
	}
	else
	{
		nodeRef->copy(&node);
	}

	if (!node.isDict())
	{
		node.free();
		return;
	}

	if (node.dictLookup("Kids", &kidsObj)->isArray())
	{
		for (int i = 0; i < kidsObj.arrayGetLength(); ++i)
		{
			kidsObj.arrayGetNF(i, &kidObj);
			readEmbeddedFileTree(&kidObj, touchedObjs);
			kidObj.free();
		}
	}
	else
	{
		if (node.dictLookup("Names", &namesObj)->isArray())
		{
			for (int i = 0; i + 1 < namesObj.arrayGetLength(); ++i)
			{
				namesObj.arrayGet(i, &nameObj);
				namesObj.arrayGet(i + 1, &fileSpecObj);
				readEmbeddedFile(&fileSpecObj, &nameObj);
				nameObj.free();
				fileSpecObj.free();
			}
		}
		namesObj.free();
	}
	kidsObj.free();

	node.free();
}

void Catalog::readFileAttachmentAnnots(Object* pageNodeRef, char* touchedObjs)
{
	Object pageNode, kids, kid, annots, annot, subtype, fileSpec, contents;

	// check for an invalid object reference (e.g., in a damaged PDF file)
	if (pageNodeRef->isRef() && (pageNodeRef->getRefNum() < 0 || pageNodeRef->getRefNum() >= xref->getNumObjects()))
		return;

	// check for a page tree loop
	if (pageNodeRef->isRef())
	{
		if (touchedObjs[pageNodeRef->getRefNum()]) return;
		touchedObjs[pageNodeRef->getRefNum()] = 1;
		xref->fetch(pageNodeRef->getRefNum(), pageNodeRef->getRefGen(), &pageNode);
	}
	else
	{
		pageNodeRef->copy(&pageNode);
	}

	if (pageNode.isDict())
	{
		if (pageNode.dictLookup("Kids", &kids)->isArray())
		{
			for (int i = 0; i < kids.arrayGetLength(); ++i)
			{
				readFileAttachmentAnnots(kids.arrayGetNF(i, &kid), touchedObjs);
				kid.free();
			}
		}
		else
		{
			if (pageNode.dictLookup("Annots", &annots)->isArray())
			{
				for (int i = 0; i < annots.arrayGetLength(); ++i)
				{
					if (annots.arrayGet(i, &annot)->isDict())
					{
						if (annot.dictLookup("Subtype", &subtype)->isName("FileAttachment"))
						{
							if (annot.dictLookup("FS", &fileSpec))
							{
								readEmbeddedFile(&fileSpec, annot.dictLookup("Contents", &contents));
								contents.free();
							}
							fileSpec.free();
						}
						subtype.free();
					}
					annot.free();
				}
			}
			annots.free();
		}
		kids.free();
	}

	pageNode.free();
}

void Catalog::readEmbeddedFile(Object* fileSpec, Object* name1)
{
	Object      name2, efObj, streamObj;
	TextString* name;

	if (fileSpec->isDict())
	{
		if (fileSpec->dictLookup("UF", &name2)->isString())
		{
			name = new TextString(name2.getString());
		}
		else
		{
			name2.free();
			if (fileSpec->dictLookup("F", &name2)->isString())
				name = new TextString(name2.getString());
			else if (name1 && name1->isString())
				name = new TextString(name1->getString());
			else
				name = new TextString("?");
		}
		name2.free();
		if (fileSpec->dictLookup("EF", &efObj)->isDict())
		{
			if (efObj.dictLookupNF("F", &streamObj)->isRef())
			{
				if (!embeddedFiles)
					embeddedFiles = new GList();
				embeddedFiles->append(new EmbeddedFile(name, &streamObj));
			}
			else
			{
				delete name;
			}
			streamObj.free();
		}
		else
		{
			delete name;
		}
		efObj.free();
	}
}

int Catalog::getNumEmbeddedFiles()
{
	return embeddedFiles ? embeddedFiles->getLength() : 0;
}

Unicode* Catalog::getEmbeddedFileName(int idx)
{
	return ((EmbeddedFile*)embeddedFiles->get(idx))->name->getUnicode();
}

int Catalog::getEmbeddedFileNameLength(int idx)
{
	return ((EmbeddedFile*)embeddedFiles->get(idx))->name->getLength();
}

Object* Catalog::getEmbeddedFileStreamRef(int idx)
{
	return &((EmbeddedFile*)embeddedFiles->get(idx))->streamRef;
}

Object* Catalog::getEmbeddedFileStreamObj(int idx, Object* strObj)
{
	((EmbeddedFile*)embeddedFiles->get(idx))->streamRef.fetch(xref, strObj);
	if (!strObj->isStream())
	{
		strObj->free();
		return nullptr;
	}
	return strObj;
}

void Catalog::readPageLabelTree(Object* root)
{
	PageLabelNode *label0, *label1;
	char*          touchedObjs;

	touchedObjs = (char*)gmalloc(xref->getNumObjects());
	memset(touchedObjs, 0, xref->getNumObjects());
	pageLabels = new GList();
	readPageLabelTree2(root, touchedObjs);
	gfree(touchedObjs);

	if (pageLabels->getLength() == 0)
	{
		deleteGList(pageLabels, PageLabelNode);
		pageLabels = nullptr;
		return;
	}

	// set lastPage in each node
	label0 = (PageLabelNode*)pageLabels->get(0);
	for (int i = 1; i < pageLabels->getLength(); ++i)
	{
		label1           = (PageLabelNode*)pageLabels->get(i);
		label0->lastPage = label1->firstPage - 1;
		label0           = label1;
	}
	label0->lastPage = numPages;
}

void Catalog::readPageLabelTree2(Object* nodeRef, char* touchedObjs)
{
	Object node, nums, num, labelObj, kids, kid;

	// check for an object loop
	if (nodeRef->isRef())
	{
		if (nodeRef->getRefNum() < 0 || nodeRef->getRefNum() >= xref->getNumObjects() || touchedObjs[nodeRef->getRefNum()])
			return;
		touchedObjs[nodeRef->getRefNum()] = 1;
		xref->fetch(nodeRef->getRefNum(), nodeRef->getRefGen(), &node);
	}
	else
	{
		nodeRef->copy(&node);
	}

	if (!node.isDict())
	{
		node.free();
		return;
	}

	if (node.dictLookup("Nums", &nums)->isArray())
	{
		for (int i = 0; i < nums.arrayGetLength() - 1; i += 2)
		{
			if (nums.arrayGet(i, &num)->isInt())
			{
				if (nums.arrayGet(i + 1, &labelObj)->isDict())
					pageLabels->append(new PageLabelNode(num.getInt(), labelObj.getDict()));
				labelObj.free();
			}
			num.free();
		}
	}
	nums.free();

	if (node.dictLookup("Kids", &kids)->isArray())
	{
		for (int i = 0; i < kids.arrayGetLength(); ++i)
		{
			kids.arrayGetNF(i, &kid);
			readPageLabelTree2(&kid, touchedObjs);
			kid.free();
		}
	}
	kids.free();

	node.free();
}

TextString* Catalog::getPageLabel(int pageNum)
{
	PageLabelNode* label;
	if (!pageLabels || !(label = findPageLabel(pageNum)))
		return nullptr;

	auto ts = new TextString(label->prefix);

	const int pageRangeNum = label->start + (pageNum - label->firstPage);

	std::string suffix;
	if (label->style == 'D')
		suffix = fmt::format("{}", pageRangeNum);
	else if (label->style == 'R')
		suffix = makeRomanNumeral(pageRangeNum, true);
	else if (label->style == 'r')
		suffix = makeRomanNumeral(pageRangeNum, false);
	else if (label->style == 'A')
		suffix = makeLetterLabel(pageRangeNum, true);
	else if (label->style == 'a')
		suffix = makeLetterLabel(pageRangeNum, false);
	if (suffix.size())
		ts->append(suffix);

	return ts;
}

PageLabelNode* Catalog::findPageLabel(int pageNum)
{
	//~ this could use a binary search
	for (int i = 0; i < pageLabels->getLength(); ++i)
	{
		const auto label = (PageLabelNode*)pageLabels->get(i);
		if (pageNum >= label->firstPage && pageNum <= label->lastPage)
			return label;
	}
	return nullptr;
}

std::string Catalog::makeRomanNumeral(int num, bool uppercase)
{
	std::string s;
	while (num >= 1000)
	{
		s += uppercase ? 'M' : 'm';
		num -= 1000;
	}
	if (num >= 900)
	{
		s += uppercase ? "CM" : "cm";
		num -= 900;
	}
	else if (num >= 500)
	{
		s += uppercase ? 'D' : 'd';
		num -= 500;
	}
	else if (num >= 400)
	{
		s += uppercase ? "CD" : "cd";
		num -= 400;
	}
	while (num >= 100)
	{
		s += uppercase ? 'C' : 'c';
		num -= 100;
	}
	if (num >= 90)
	{
		s += uppercase ? "XC" : "xc";
		num -= 90;
	}
	else if (num >= 50)
	{
		s += uppercase ? 'L' : 'l';
		num -= 50;
	}
	else if (num >= 40)
	{
		s += uppercase ? "XL" : "xl";
		num -= 40;
	}
	while (num >= 10)
	{
		s += uppercase ? 'X' : 'x';
		num -= 10;
	}
	if (num >= 9)
	{
		s += uppercase ? "IX" : "ix";
		num -= 9;
	}
	else if (num >= 5)
	{
		s += uppercase ? 'V' : 'v';
		num -= 5;
	}
	else if (num >= 4)
	{
		s += uppercase ? "IV" : "iv";
		num -= 4;
	}
	while (num >= 1)
	{
		s += uppercase ? 'I' : 'i';
		num -= 1;
	}
	return s;
}

std::string Catalog::makeLetterLabel(int num, bool uppercase)
{
	const int m = (num - 1) / 26 + 1;
	const int n = (num - 1) % 26;

	std::string s;
	for (int i = 0; i < m; ++i)
		s += (char)((uppercase ? 'A' : 'a') + n);
	return s;
}

int Catalog::getPageNumFromPageLabel(TextString* pageLabel)
{
	PageLabelNode* label;
	int            pageNum, prefixLength, i, n;

	if (!pageLabels)
		return -1;
	for (i = 0; i < pageLabels->getLength(); ++i)
	{
		label        = (PageLabelNode*)pageLabels->get(i);
		prefixLength = label->prefix->getLength();
		if (pageLabel->getLength() < prefixLength || memcmp(pageLabel->getUnicode(), label->prefix->getUnicode(), prefixLength * sizeof(Unicode)))
			continue;
		if (label->style == '\0' && pageLabel->getLength() == prefixLength)
			return label->firstPage;
		if (!convertPageLabelToInt(pageLabel, prefixLength, label->style, &n))
			continue;
		if (n < label->start)
			continue;
		pageNum = label->firstPage + n - label->start;
		if (pageNum <= label->lastPage)
			return pageNum;
	}
	return -1;
}

// Attempts to convert pageLabel[prefixLength .. end] to an integer,
// following the specified page label style.  If successful, sets *n
// and returns true; else returns false.
bool Catalog::convertPageLabelToInt(TextString* pageLabel, int prefixLength, char style, int* n)
{
	const int len = pageLabel->getLength();
	if (len <= prefixLength)
		return false;
	const Unicode* u = pageLabel->getUnicode();
	if (style == 'D')
	{
		*n = 0;
		for (int i = prefixLength; i < len; ++i)
		{
			if (u[i] < (Unicode)'0' || u[i] > (Unicode)'9')
				return false;
			*n = *n * 10 + (u[i] - (Unicode)'0');
		}
		return true;
	}
	else if (style == 'R' || style == 'r')
	{
		const Unicode delta = style - 'R';
		*n                  = 0;
		int i               = prefixLength;
		while (i < len && u[i] == (Unicode)'M' + delta)
		{
			*n += 1000;
			++i;
		}
		if (i + 1 < len && u[i] == (Unicode)'C' + delta && u[i + 1] == (Unicode)'M' + delta)
		{
			*n += 900;
			i += 2;
		}
		else if (i < len && u[i] == (Unicode)'D' + delta)
		{
			*n += 500;
			++i;
		}
		else if (i + 1 < len && u[i] == (Unicode)'C' + delta && u[i + 1] == (Unicode)'D' + delta)
		{
			*n += 400;
			i += 2;
		}
		while (i < len && u[i] == (Unicode)'C' + delta)
		{
			*n += 100;
			++i;
		}
		if (i + 1 < len && u[i] == (Unicode)'X' + delta && u[i + 1] == (Unicode)'C' + delta)
		{
			*n += 90;
			i += 2;
		}
		else if (i < len && u[i] == (Unicode)'L' + delta)
		{
			*n += 50;
			++i;
		}
		else if (i + 1 < len && u[i] == (Unicode)'X' + delta && u[i + 1] == (Unicode)'L' + delta)
		{
			*n += 40;
			i += 2;
		}
		while (i < len && u[i] == (Unicode)'X' + delta)
		{
			*n += 10;
			++i;
		}
		if (i + 1 < len && u[i] == (Unicode)'I' + delta && u[i + 1] == (Unicode)'X' + delta)
		{
			*n += 9;
			i += 2;
		}
		else if (i < len && u[i] == (Unicode)'V' + delta)
		{
			*n += 5;
			++i;
		}
		else if (i + 1 < len && u[i] == (Unicode)'I' + delta && u[i + 1] == (Unicode)'V' + delta)
		{
			*n += 4;
			i += 2;
		}
		while (i < len && u[i] == (Unicode)'I' + delta)
		{
			*n += 1;
			++i;
		}
		return i == len;
	}
	else if (style == 'A' || style == 'a')
	{
		if (u[prefixLength] < (Unicode)style || u[prefixLength] > (Unicode)style + 25)
			return false;
		for (int i = prefixLength + 1; i < len; ++i)
			if (u[i] != u[prefixLength])
				return false;
		*n = (len - prefixLength - 1) * 26 + (u[prefixLength] - (Unicode)style) + 1;
		return true;
	}
	return false;
}

bool Catalog::usesJavaScript()
{
	Object catDict;
	if (!xref->getCatalog(&catDict)->isDict())
	{
		catDict.free();
		return false;
	}

	bool usesJS = false;

	// check for Catalog.Names.JavaScript
	Object namesObj;
	if (catDict.dictLookup("Names", &namesObj)->isDict())
	{
		Object jsNamesObj;
		namesObj.dictLookup("JavaScript", &jsNamesObj);
		if (jsNamesObj.isDict())
			usesJS = true;
		jsNamesObj.free();
	}
	namesObj.free();

	// look for JavaScript actionas in Page.AA
	if (!usesJS)
	{
		char* touchedObjs = (char*)gmalloc(xref->getNumObjects());
		memset(touchedObjs, 0, xref->getNumObjects());
		Object pagesObj;
		usesJS = scanPageTreeForJavaScript(catDict.dictLookupNF("Pages", &pagesObj), touchedObjs);
		pagesObj.free();
		gfree(touchedObjs);
	}

	catDict.free();

	return usesJS;
}

bool Catalog::scanPageTreeForJavaScript(Object* pageNodeRef, char* touchedObjs)
{
	// check for an invalid object reference (e.g., in a damaged PDF file)
	if (pageNodeRef->isRef() && (pageNodeRef->getRefNum() < 0 || pageNodeRef->getRefNum() >= xref->getNumObjects()))
		return false;

	// check for a page tree loop
	Object pageNode;
	if (pageNodeRef->isRef())
	{
		if (touchedObjs[pageNodeRef->getRefNum()])
			return false;
		touchedObjs[pageNodeRef->getRefNum()] = 1;
		xref->fetch(pageNodeRef->getRefNum(), pageNodeRef->getRefGen(), &pageNode);
	}
	else
	{
		pageNodeRef->copy(&pageNode);
	}

	// scan the page tree node
	bool usesJS = false;
	if (pageNode.isDict())
	{
		Object kids;
		if (pageNode.dictLookup("Kids", &kids)->isArray())
		{
			for (int i = 0; i < kids.arrayGetLength() && !usesJS; ++i)
			{
				Object kid;
				if (scanPageTreeForJavaScript(kids.arrayGetNF(i, &kid), touchedObjs))
					usesJS = true;
				kid.free();
			}
		}
		else
		{
			// scan Page.AA
			Object pageAA;
			if (pageNode.dictLookup("AA", &pageAA)->isDict())
			{
				if (scanAAForJavaScript(&pageAA))
					usesJS = true;
			}
			pageAA.free();

			// scanPage.Annots
			if (!usesJS)
			{
				Object annots;
				if (pageNode.dictLookup("Annots", &annots)->isArray())
				{
					for (int i = 0; i < annots.arrayGetLength() && !usesJS; ++i)
					{
						Object annot;
						if (annots.arrayGet(i, &annot)->isDict())
						{
							Object annotAA;
							if (annot.dictLookup("AA", &annotAA)->isDict())
							{
								if (scanAAForJavaScript(&annotAA))
									usesJS = true;
							}
							annotAA.free();
						}
						annot.free();
					}
				}
				annots.free();
			}
		}
		kids.free();
	}

	pageNode.free();

	return usesJS;
}

bool Catalog::scanAAForJavaScript(Object* aaObj)
{
	bool usesJS = false;
	for (int i = 0; i < aaObj->dictGetLength() && !usesJS; ++i)
	{
		Object action;
		if (aaObj->dictGetVal(i, &action)->isDict())
		{
			Object js;
			if (!action.dictLookupNF("JS", &js)->isNull())
				usesJS = true;
			js.free();
		}
		action.free();
	}
	return usesJS;
}
