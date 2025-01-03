//========================================================================
//
// Link.cc
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include <stddef.h>
#include <string.h>
#include "gmem.h"
#include "gmempp.h"
#include "Error.h"
#include "Object.h"
#include "Array.h"
#include "Dict.h"
#include "Link.h"

//------------------------------------------------------------------------
// LinkAction
//------------------------------------------------------------------------

LinkAction* LinkAction::parseDest(Object* obj)
{
	LinkAction* action = new LinkGoTo(obj);
	if (!action->isOk())
	{
		delete action;
		return nullptr;
	}
	return action;
}

LinkAction* LinkAction::parseAction(Object* obj, const std::string& baseURI)
{
	LinkAction* action;
	Object      obj2, obj3, obj4, obj5;

	if (!obj->isDict())
	{
		error(errSyntaxWarning, -1, "Bad annotation action");
		return nullptr;
	}

	obj->dictLookup("S", &obj2);

	// GoTo action
	if (obj2.isName("GoTo"))
	{
		obj->dictLookup("D", &obj3);
		action = new LinkGoTo(&obj3);
		obj3.free();

		// GoToR action
	}
	else if (obj2.isName("GoToR"))
	{
		obj->dictLookup("F", &obj3);
		obj->dictLookup("D", &obj4);
		action = new LinkGoToR(&obj3, &obj4);
		obj3.free();
		obj4.free();

		// Launch action
	}
	else if (obj2.isName("Launch"))
	{
		action = new LinkLaunch(obj);

		// URI action
	}
	else if (obj2.isName("URI"))
	{
		obj->dictLookup("URI", &obj3);
		action = new LinkURI(&obj3, baseURI);
		obj3.free();

		// Named action
	}
	else if (obj2.isName("Named"))
	{
		obj->dictLookup("N", &obj3);
		action = new LinkNamed(&obj3);
		obj3.free();

		// Movie action
	}
	else if (obj2.isName("Movie"))
	{
		obj->dictLookupNF("Annot", &obj3);
		obj->dictLookup("T", &obj4);
		action = new LinkMovie(&obj3, &obj4);
		obj3.free();
		obj4.free();

		// JavaScript action
	}
	else if (obj2.isName("JavaScript"))
	{
		obj->dictLookup("JS", &obj3);
		action = new LinkJavaScript(&obj3);
		obj3.free();

		// SubmitForm action
	}
	else if (obj2.isName("SubmitForm"))
	{
		obj->dictLookup("F", &obj3);
		obj->dictLookup("Fields", &obj4);
		obj->dictLookup("Flags", &obj5);
		action = new LinkSubmitForm(&obj3, &obj4, &obj5);
		obj3.free();
		obj4.free();
		obj5.free();

		// Hide action
	}
	else if (obj2.isName("Hide"))
	{
		obj->dictLookupNF("T", &obj3);
		obj->dictLookup("H", &obj4);
		action = new LinkHide(&obj3, &obj4);
		obj3.free();
		obj4.free();

		// unknown action
	}
	else if (obj2.isName())
	{
		action = new LinkUnknown(obj2.getName());

		// action is missing or wrong type
	}
	else
	{
		error(errSyntaxWarning, -1, "Bad annotation action");
		action = nullptr;
	}

	obj2.free();

	if (action && !action->isOk())
	{
		delete action;
		return nullptr;
	}
	return action;
}

std::string LinkAction::getFileSpecName(Object* fileSpecObj)
{
	std::string name;
	Object      obj1;

	// string
	if (fileSpecObj->isString())
	{
		name = fileSpecObj->getString();

		// dictionary
	}
	else if (fileSpecObj->isDict())
	{
#ifdef _WIN32
		if (!fileSpecObj->dictLookup("DOS", &obj1)->isString())
		{
#else
		if (!fileSpecObj->dictLookup("Unix", &obj1)->isString())
		{
#endif
			obj1.free();
			fileSpecObj->dictLookup("F", &obj1);
		}
		if (obj1.isString())
			name = obj1.getString();
		else
			error(errSyntaxWarning, -1, "Illegal file spec in link");
		obj1.free();

		// error
	}
	else
	{
		error(errSyntaxWarning, -1, "Illegal file spec in link");
	}

	// system-dependent path manipulation
	if (name.size())
	{
#ifdef _WIN32
		// "//...."             --> "\...."
		// "/x/...."            --> "x:\...."
		// "/server/share/...." --> "\\server\share\...."
		// convert escaped slashes to slashes and unescaped slashes to backslashes
		int i = 0;
		if (name.at(0) == '/')
		{
			if (name.size() >= 2 && name.at(1) == '/')
			{
				name.erase(0, 1);
				i = 0;
			}
			else if (name.size() >= 2 && ((name.at(1) >= 'a' && name.at(1) <= 'z') || (name.at(1) >= 'A' && name.at(1) <= 'Z')) && (name.size() == 2 || name.at(2) == '/'))
			{
				name[0] = name.at(1);
				name[1] = ':';
				i       = 2;
			}
			else
			{
				int j;
				for (j = 2; j < name.size(); ++j)
					if (name.at(j - 1) != '\\' && name.at(j) == '/')
						break;
				if (j < name.size())
				{
					name[0] = '\\';
					name.insert(0, "\\");
					i = 2;
				}
			}
		}
		for (; i < name.size(); ++i)
			if (name.at(i) == '/')
				name[i] = '\\';
			else if (name.at(i) == '\\' && i + 1 < name.size() && name.at(i + 1) == '/')
				name.erase(i, 1);
#else
		// no manipulation needed for Unix
#endif
	}

	return name;
}

//------------------------------------------------------------------------
// LinkDest
//------------------------------------------------------------------------

LinkDest::LinkDest(Array* a)
{
	Object obj1, obj2;

	// get page
	if (a->getLength() < 2)
	{
		error(errSyntaxWarning, -1, "Annotation destination array is too short");
		return;
	}
	a->getNF(0, &obj1);
	if (obj1.isInt())
	{
		pageNum   = obj1.getInt() + 1;
		pageIsRef = false;
	}
	else if (obj1.isRef())
	{
		pageRef.num = obj1.getRefNum();
		pageRef.gen = obj1.getRefGen();
		pageIsRef   = true;
	}
	else
	{
		error(errSyntaxWarning, -1, "Bad annotation destination");
		goto err2;
	}
	obj1.free();

	// get destination type
	a->get(1, &obj1);

	// XYZ link
	if (obj1.isName("XYZ"))
	{
		kind = destXYZ;
		if (a->getLength() < 3)
		{
			changeLeft = false;
		}
		else
		{
			a->get(2, &obj2);
			if (obj2.isNull())
			{
				changeLeft = false;
			}
			else if (obj2.isNum())
			{
				changeLeft = true;
				left       = obj2.getNum();
			}
			else
			{
				error(errSyntaxWarning, -1, "Bad annotation destination position");
				goto err1;
			}
			obj2.free();
		}
		if (a->getLength() < 4)
		{
			changeTop = false;
		}
		else
		{
			a->get(3, &obj2);
			if (obj2.isNull())
			{
				changeTop = false;
			}
			else if (obj2.isNum())
			{
				changeTop = true;
				top       = obj2.getNum();
			}
			else
			{
				error(errSyntaxWarning, -1, "Bad annotation destination position");
				goto err1;
			}
			obj2.free();
		}
		if (a->getLength() < 5)
		{
			changeZoom = false;
		}
		else
		{
			a->get(4, &obj2);
			if (obj2.isNull())
			{
				changeZoom = false;
			}
			else if (obj2.isNum())
			{
				changeZoom = true;
				zoom       = obj2.getNum();
			}
			else
			{
				error(errSyntaxWarning, -1, "Bad annotation destination position");
				goto err1;
			}
			obj2.free();
		}

		// Fit link
	}
	else if (obj1.isName("Fit"))
	{
		if (a->getLength() < 2)
		{
			error(errSyntaxWarning, -1, "Annotation destination array is too short");
			goto err2;
		}
		kind = destFit;

		// FitH link
	}
	else if (obj1.isName("FitH"))
	{
		if (a->getLength() < 3)
		{
			error(errSyntaxWarning, -1, "Annotation destination array is too short");
			goto err2;
		}
		kind = destFitH;
		if (a->get(2, &obj2)->isNum())
		{
			top       = obj2.getNum();
			changeTop = true;
		}
		else if (obj2.isNull())
		{
			changeTop = false;
		}
		else
		{
			error(errSyntaxWarning, -1, "Bad annotation destination position");
			kind = destFit;
		}
		obj2.free();

		// FitV link
	}
	else if (obj1.isName("FitV"))
	{
		if (a->getLength() < 3)
		{
			error(errSyntaxWarning, -1, "Annotation destination array is too short");
			goto err2;
		}
		kind = destFitV;
		if (a->get(2, &obj2)->isNum())
		{
			left       = obj2.getNum();
			changeLeft = true;
		}
		else if (obj2.isNull())
		{
			changeLeft = false;
		}
		else
		{
			error(errSyntaxWarning, -1, "Bad annotation destination position");
			kind = destFit;
		}
		obj2.free();

		// FitR link
	}
	else if (obj1.isName("FitR"))
	{
		if (a->getLength() < 6)
		{
			error(errSyntaxWarning, -1, "Annotation destination array is too short");
			goto err2;
		}
		kind = destFitR;
		if (a->get(2, &obj2)->isNum())
		{
			left = obj2.getNum();
		}
		else
		{
			error(errSyntaxWarning, -1, "Bad annotation destination position");
			kind = destFit;
		}
		obj2.free();
		if (!a->get(3, &obj2)->isNum())
		{
			error(errSyntaxWarning, -1, "Bad annotation destination position");
			kind = destFit;
		}
		bottom = obj2.getNum();
		obj2.free();
		if (!a->get(4, &obj2)->isNum())
		{
			error(errSyntaxWarning, -1, "Bad annotation destination position");
			kind = destFit;
		}
		right = obj2.getNum();
		obj2.free();
		if (!a->get(5, &obj2)->isNum())
		{
			error(errSyntaxWarning, -1, "Bad annotation destination position");
			kind = destFit;
		}
		top = obj2.getNum();
		obj2.free();

		// FitB link
	}
	else if (obj1.isName("FitB"))
	{
		if (a->getLength() < 2)
		{
			error(errSyntaxWarning, -1, "Annotation destination array is too short");
			goto err2;
		}
		kind = destFitB;

		// FitBH link
	}
	else if (obj1.isName("FitBH"))
	{
		if (a->getLength() < 3)
		{
			error(errSyntaxWarning, -1, "Annotation destination array is too short");
			goto err2;
		}
		kind = destFitBH;
		if (a->get(2, &obj2)->isNum())
		{
			top       = obj2.getNum();
			changeTop = true;
		}
		else if (obj2.isNull())
		{
			changeTop = false;
		}
		else
		{
			error(errSyntaxWarning, -1, "Bad annotation destination position");
			kind = destFit;
		}
		obj2.free();

		// FitBV link
	}
	else if (obj1.isName("FitBV"))
	{
		if (a->getLength() < 3)
		{
			error(errSyntaxWarning, -1, "Annotation destination array is too short");
			goto err2;
		}
		kind = destFitBV;
		if (a->get(2, &obj2)->isNum())
		{
			left       = obj2.getNum();
			changeLeft = true;
		}
		else if (obj2.isNull())
		{
			changeLeft = false;
		}
		else
		{
			error(errSyntaxWarning, -1, "Bad annotation destination position");
			kind = destFit;
		}
		obj2.free();

		// unknown link kind
	}
	else
	{
		error(errSyntaxWarning, -1, "Unknown annotation destination type");
		goto err2;
	}

	obj1.free();
	ok = true;
	return;

err1:
	obj2.free();
err2:
	obj1.free();
}

LinkDest::LinkDest(LinkDest* dest)
{
	kind      = dest->kind;
	pageIsRef = dest->pageIsRef;
	if (pageIsRef)
		pageRef = dest->pageRef;
	else
		pageNum = dest->pageNum;
	left       = dest->left;
	bottom     = dest->bottom;
	right      = dest->right;
	top        = dest->top;
	zoom       = dest->zoom;
	changeLeft = dest->changeLeft;
	changeTop  = dest->changeTop;
	changeZoom = dest->changeZoom;
	ok         = true;
}

//------------------------------------------------------------------------
// LinkGoTo
//------------------------------------------------------------------------

LinkGoTo::LinkGoTo(Object* destObj)
{
	dest = nullptr;
	namedDest.clear();

	// named destination
	if (destObj->isName())
	{
		ASSIGN_CSTRING(namedDest, destObj->getName());
	}
	else if (destObj->isString())
	{
		namedDest = destObj->getString();

		// destination dictionary
	}
	else if (destObj->isArray())
	{
		dest = new LinkDest(destObj->getArray());
		if (!dest->isOk())
		{
			delete dest;
			dest = nullptr;
		}

		// error
	}
	else
	{
		error(errSyntaxWarning, -1, "Illegal annotation destination");
	}
}

LinkGoTo::~LinkGoTo()
{
	if (dest) delete dest;
}

//------------------------------------------------------------------------
// LinkGoToR
//------------------------------------------------------------------------

LinkGoToR::LinkGoToR(Object* fileSpecObj, Object* destObj)
{
	dest = nullptr;
	namedDest.clear();

	// get file name
	fileName = getFileSpecName(fileSpecObj);

	// named destination
	if (destObj->isName())
	{
		ASSIGN_CSTRING(namedDest, destObj->getName());
	}
	else if (destObj->isString())
	{
		namedDest = destObj->getString();

		// destination dictionary
	}
	else if (destObj->isArray())
	{
		dest = new LinkDest(destObj->getArray());
		if (!dest->isOk())
		{
			delete dest;
			dest = nullptr;
		}

		// error
	}
	else
	{
		error(errSyntaxWarning, -1, "Illegal annotation destination");
	}
}

LinkGoToR::~LinkGoToR()
{
	if (dest) delete dest;
}

//------------------------------------------------------------------------
// LinkLaunch
//------------------------------------------------------------------------

LinkLaunch::LinkLaunch(Object* actionObj)
{
	params.clear();

	if (actionObj->isDict())
	{
		Object obj1;
		if (!actionObj->dictLookup("F", &obj1)->isNull())
		{
			fileName = getFileSpecName(&obj1);
		}
		else
		{
			obj1.free();
			Object obj2;
#ifdef _WIN32
			if (actionObj->dictLookup("Win", &obj1)->isDict())
			{
				obj1.dictLookup("F", &obj2);
				fileName = getFileSpecName(&obj2);
				obj2.free();
				if (obj1.dictLookup("P", &obj2)->isString())
					params = obj2.getString();
				obj2.free();
			}
			else
			{
				error(errSyntaxWarning, -1, "Bad launch-type link action");
			}
#else
			//~ This hasn't been defined by Adobe yet, so assume it looks
			//~ just like the Win dictionary until they say otherwise.
			if (actionObj->dictLookup("Unix", &obj1)->isDict())
			{
				obj1.dictLookup("F", &obj2);
				fileName = getFileSpecName(&obj2);
				obj2.free();
				if (obj1.dictLookup("P", &obj2)->isString())
					params = obj2.getString();
				obj2.free();
			}
			else
			{
				error(errSyntaxWarning, -1, "Bad launch-type link action");
			}
#endif
		}
		obj1.free();
	}
}

LinkLaunch::~LinkLaunch()
{
}

//------------------------------------------------------------------------
// LinkURI
//------------------------------------------------------------------------

LinkURI::LinkURI(Object* uriObj, const std::string& baseURI)
{
	uri.clear();
	if (uriObj->isString())
	{
		const auto uri2 = uriObj->getString();
		const int  n    = (int)strcspn(uri2.c_str(), "/:");
		if (n < uri2.size() && uri2.at(n) == ':')
		{
			// "http:..." etc.
			uri = uri2;
		}
		else if (uri.substr(0, 4) == "www.")
		{
			// "www.[...]" without the leading "http://"
			uri = "http://";
			uri += uri2;
		}
		else
		{
			// relative URI
			if (baseURI.size())
			{
				uri          = baseURI;
				const char c = uri.back();
				if (c != '/' && c != '?')
					uri += '/';
				if (uri2.at(0) == '/')
					uri.append(uri2.c_str() + 1, uri2.size() - 1);
				else
					uri += uri2;
			}
			else
			{
				uri = uri2;
			}
		}
	}
	else
	{
		error(errSyntaxWarning, -1, "Illegal URI-type link");
	}
}

LinkURI::~LinkURI()
{
}

//------------------------------------------------------------------------
// LinkNamed
//------------------------------------------------------------------------

LinkNamed::LinkNamed(Object* nameObj)
{
	name.clear();
	if (nameObj->isName())
		ASSIGN_CSTRING(name, nameObj->getName());
}

LinkNamed::~LinkNamed()
{
}

//------------------------------------------------------------------------
// LinkMovie
//------------------------------------------------------------------------

LinkMovie::LinkMovie(Object* annotObj, Object* titleObj)
{
	annotRef.num = -1;
	title.clear();
	if (annotObj->isRef())
		annotRef = annotObj->getRef();
	else if (titleObj->isString())
		title = titleObj->getString();
	else
		error(errSyntaxError, -1, "Movie action is missing both the Annot and T keys");
}

LinkMovie::~LinkMovie()
{
}

//------------------------------------------------------------------------
// LinkJavaScript
//------------------------------------------------------------------------

LinkJavaScript::LinkJavaScript(Object* jsObj)
{
	char buf[4096];

	if (jsObj->isString())
	{
		js = jsObj->getString();
	}
	else if (jsObj->isStream())
	{
		js.clear();
		jsObj->streamReset();
		int n;
		while ((n = jsObj->getStream()->getBlock(buf, sizeof(buf))) > 0)
			js.append(buf, n);
		jsObj->streamClose();
	}
	else
	{
		error(errSyntaxError, -1, "JavaScript action JS key is wrong type");
		js.clear();
	}
}

LinkJavaScript::~LinkJavaScript()
{
}

//------------------------------------------------------------------------
// LinkSubmitForm
//------------------------------------------------------------------------

LinkSubmitForm::LinkSubmitForm(Object* urlObj, Object* fieldsObj, Object* flagsObj)
{
	if (urlObj->isString())
	{
		url = urlObj->getString();
	}
	else
	{
		error(errSyntaxError, -1, "SubmitForm action URL is wrong type");
		url.clear();
	}

	if (fieldsObj->isArray())
	{
		fieldsObj->copy(&fields);
	}
	else
	{
		if (!fieldsObj->isNull())
			error(errSyntaxError, -1, "SubmitForm action Fields value is wrong type");
		fields.initNull();
	}

	if (flagsObj->isInt())
	{
		flags = flagsObj->getInt();
	}
	else
	{
		if (!flagsObj->isNull())
			error(errSyntaxError, -1, "SubmitForm action Flags value is wrong type");
		flags = 0;
	}
}

LinkSubmitForm::~LinkSubmitForm()
{
	fields.free();
}

//------------------------------------------------------------------------
// LinkHide
//------------------------------------------------------------------------

LinkHide::LinkHide(Object* fieldsObj, Object* hideFlagObj)
{
	if (fieldsObj->isRef() || fieldsObj->isString() || fieldsObj->isArray())
	{
		fieldsObj->copy(&fields);
	}
	else
	{
		error(errSyntaxError, -1, "Hide action T value is wrong type");
		fields.initNull();
	}

	if (hideFlagObj->isBool())
	{
		hideFlag = hideFlagObj->getBool();
	}
	else
	{
		error(errSyntaxError, -1, "Hide action H value is wrong type");
		hideFlag = false;
	}
}

LinkHide::~LinkHide()
{
	fields.free();
}

//------------------------------------------------------------------------
// LinkUnknown
//------------------------------------------------------------------------

LinkUnknown::LinkUnknown(std::string_view actionA)
{
	action = actionA;
}

LinkUnknown::~LinkUnknown()
{
}

//------------------------------------------------------------------------
// Link
//------------------------------------------------------------------------

Link::Link(Dict* dict, const std::string& baseURI)
{
	Object obj1, obj2;
	double t;

	action = nullptr;
	ok     = false;

	// get rectangle
	if (!dict->lookup("Rect", &obj1)->isArray())
	{
		error(errSyntaxError, -1, "Annotation rectangle is wrong type");
		goto err2;
	}
	if (!obj1.arrayGet(0, &obj2)->isNum())
	{
		error(errSyntaxError, -1, "Bad annotation rectangle");
		goto err1;
	}
	x1 = obj2.getNum();
	obj2.free();
	if (!obj1.arrayGet(1, &obj2)->isNum())
	{
		error(errSyntaxError, -1, "Bad annotation rectangle");
		goto err1;
	}
	y1 = obj2.getNum();
	obj2.free();
	if (!obj1.arrayGet(2, &obj2)->isNum())
	{
		error(errSyntaxError, -1, "Bad annotation rectangle");
		goto err1;
	}
	x2 = obj2.getNum();
	obj2.free();
	if (!obj1.arrayGet(3, &obj2)->isNum())
	{
		error(errSyntaxError, -1, "Bad annotation rectangle");
		goto err1;
	}
	y2 = obj2.getNum();
	obj2.free();
	obj1.free();
	if (x1 > x2)
	{
		t  = x1;
		x1 = x2;
		x2 = t;
	}
	if (y1 > y2)
	{
		t  = y1;
		y1 = y2;
		y2 = t;
	}

	// look for destination
	if (!dict->lookup("Dest", &obj1)->isNull())
	{
		action = LinkAction::parseDest(&obj1);

		// look for action
	}
	else
	{
		obj1.free();
		if (dict->lookup("A", &obj1)->isDict())
			action = LinkAction::parseAction(&obj1, baseURI);
	}
	obj1.free();

	// check for bad action
	if (action)
		ok = true;

	return;

err1:
	obj2.free();
err2:
	obj1.free();
}

Link::~Link()
{
	if (action) delete action;
}

//------------------------------------------------------------------------
// Links
//------------------------------------------------------------------------

Links::Links(Object* annots, const std::string& baseURI)
{
	Link*  link;
	Object obj1, obj2, obj3;
	int    size;

	links    = nullptr;
	size     = 0;
	numLinks = 0;

	if (annots->isArray())
	{
		for (int i = 0; i < annots->arrayGetLength(); ++i)
		{
			if (annots->arrayGet(i, &obj1)->isDict())
			{
				obj1.dictLookup("Subtype", &obj2);
				obj1.dictLookup("FT", &obj3);
				if (obj2.isName("Link") || (obj2.isName("Widget") && (obj3.isName("Btn") || obj3.isNull())))
				{
					link = new Link(obj1.getDict(), baseURI);
					if (link->isOk())
					{
						if (numLinks >= size)
						{
							size += 16;
							links = (Link**)greallocn(links, size, sizeof(Link*));
						}
						links[numLinks++] = link;
					}
					else
					{
						delete link;
					}
				}
				obj3.free();
				obj2.free();
			}
			obj1.free();
		}
	}
}

Links::~Links()
{
	for (int i = 0; i < numLinks; ++i)
		delete links[i];
	gfree(links);
}

LinkAction* Links::find(double x, double y)
{
	for (int i = numLinks - 1; i >= 0; --i)
		if (links[i]->inRect(x, y))
			return links[i]->getAction();
	return nullptr;
}

bool Links::onLink(double x, double y)
{
	for (int i = 0; i < numLinks; ++i)
		if (links[i]->inRect(x, y))
			return true;
	return false;
}
