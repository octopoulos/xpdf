//========================================================================
//
// XFAScanner.cc
//
// Copyright 2020 Glyph & Cog, LLC
//
//========================================================================

#include <aconf.h>
#include "GHash.h"
#include "Object.h"
#include "Error.h"
#include "Zoox.h"
#include "XFAScanner.h"

//------------------------------------------------------------------------

// fields have two names:
//
// name:
//   - nodes with bind=global set the index to 0 ("foo[0]") regardless of the number of nodes with the same name
//   - nodes with bind=none are dropped from the name
//   - <area> nodes are dropped from the name
//   - used for field value lookup in <xfa:datasets>
//
// fullName:
//   - all named nodes are treated the same, regardless of bind=global or bind=none
//   - <area> nodes are included in the name, but don't reset the numbering
//     (i.e., <area> nodes are "transparent" with respect to node numbering)
//   - used for field value lookup in <form>
//   - used for matching with AcroForm names
//
// Both names use indexes on all nodes, even if there's only one node with the name
// -- this isn't correct for XFA naming, but it matches the AcroForm behavior.

//------------------------------------------------------------------------

XFAFieldLayoutInfo::XFAFieldLayoutInfo(XFAFieldLayoutHAlign hAlignA, XFAFieldLayoutVAlign vAlignA)
{
	hAlign = hAlignA;
	vAlign = vAlignA;
}

//------------------------------------------------------------------------

XFAFieldPictureInfo::XFAFieldPictureInfo(XFAFieldPictureSubtype subtypeA, const std::string& formatA)
{
	subtype = subtypeA;
	format  = formatA;
}

XFAFieldPictureInfo::~XFAFieldPictureInfo()
{
}

//------------------------------------------------------------------------

XFAFieldBarcodeInfo::XFAFieldBarcodeInfo(const std::string& barcodeTypeA, double wideNarrowRatioA, double moduleWidthA, double moduleHeightA, int dataLengthA, int errorCorrectionLevelA, const std::string& textLocationA)
{
	barcodeType          = barcodeTypeA;
	wideNarrowRatio      = wideNarrowRatioA;
	moduleWidth          = moduleWidthA;
	moduleHeight         = moduleHeightA;
	dataLength           = dataLengthA;
	errorCorrectionLevel = errorCorrectionLevelA;
	textLocation         = textLocationA;
}

XFAFieldBarcodeInfo::~XFAFieldBarcodeInfo()
{
}

//------------------------------------------------------------------------

XFAField::XFAField(const std::string& nameA, const std::string& fullNameA, const std::string& valueA, XFAFieldLayoutInfo* layoutInfoA, XFAFieldPictureInfo* pictureInfoA, XFAFieldBarcodeInfo* barcodeInfoA)
    : name(nameA)
    , fullName(fullNameA)
    , value(valueA)
    , layoutInfo(layoutInfoA)
    , pictureInfo(pictureInfoA)
    , barcodeInfo(barcodeInfoA)
{
}

XFAField::~XFAField()
{
	delete layoutInfo;
	delete pictureInfo;
	delete barcodeInfo;
}

//------------------------------------------------------------------------

XFAScanner* XFAScanner::load(Object* xfaObj)
{
	const auto xfaData = readXFAStreams(xfaObj);
	if (xfaData.empty()) return nullptr;
	ZxDoc* xml = ZxDoc::loadMem(xfaData.c_str(), xfaData.size());
	if (!xml)
	{
		error(errSyntaxError, -1, "Invalid XML in XFA form");
		return nullptr;
	}

	XFAScanner* scanner = new XFAScanner();

	if (xml->getRoot())
	{
		auto       formValues = scanner->scanFormValues(xml->getRoot());
		ZxElement* dataElem   = nullptr;
		ZxElement* datasets   = xml->getRoot()->findFirstChildElement("xfa:datasets");
		if (datasets)
			dataElem = datasets->findFirstChildElement("xfa:data");
		ZxElement* tmpl = xml->getRoot()->findFirstChildElement("template");
		if (tmpl)
			scanner->scanNode(tmpl, nullptr, nullptr, nullptr, nullptr, nullptr, dataElem, formValues);
	}

	delete xml;

	return scanner;
}

XFAScanner::XFAScanner()
{
}

XFAScanner::~XFAScanner()
{
}

XFAField* XFAScanner::findField(const std::string& acroFormFieldName)
{
	if (const auto& it = fields.find(acroFormFieldName); it != fields.end())
		return &it->second;
	return nullptr;
}

std::string XFAScanner::readXFAStreams(Object* xfaObj)
{
	std::string data;
	char        buf[4096];
	int         n;
	if (xfaObj->isStream())
	{
		xfaObj->streamReset();
		while ((n = xfaObj->getStream()->getBlock(buf, sizeof(buf))) > 0)
			data.append(buf, n);
	}
	else if (xfaObj->isArray())
	{
		for (int i = 1; i < xfaObj->arrayGetLength(); i += 2)
		{
			Object obj;
			if (!xfaObj->arrayGet(i, &obj)->isStream())
			{
				error(errSyntaxError, -1, "XFA array element is wrong type");
				obj.free();
				return "";
			}
			obj.streamReset();
			while ((n = obj.getStream()->getBlock(buf, sizeof(buf))) > 0)
				data.append(buf, n);
			obj.free();
		}
	}
	else
	{
		error(errSyntaxError, -1, "XFA object is wrong type");
		return "";
	}
	return data;
}

UMAP_STR_STR XFAScanner::scanFormValues(ZxElement* xmlRoot)
{
	UMAP_STR_STR formValues;
	ZxElement*   formElem = xmlRoot->findFirstChildElement("form");
	if (formElem)
		scanFormNode(formElem, nullptr, formValues);
	return formValues;
}

void XFAScanner::scanFormNode(ZxElement* elem, const std::string& fullName, UMAP_STR_STR& formValues)
{
	GHash* fullNameIdx = new GHash();
	for (ZxNode* node = elem->getFirstChild(); node; node = node->getNextChild())
	{
		if (node->isElement("value"))
		{
			if (fullName.size())
			{
				ZxNode* child1Node = ((ZxElement*)node)->getFirstChild();
				if (child1Node && child1Node->isElement())
				{
					ZxNode* child2Node = ((ZxElement*)child1Node)->getFirstChild();
					if (child2Node && child2Node->isCharData())
						formValues.emplace(fullName, ((ZxCharData*)child2Node)->getData());
				}
			}
		}
		else if (node->isElement())
		{
			ZxAttr* nameAttr = ((ZxElement*)node)->findAttr("name");
			if (nameAttr && (node->isElement("subform") || node->isElement("field")))
			{
				const auto  nodeName = nameAttr->getValue();
				std::string childFullName;
				if (fullName.size())
					childFullName = fmt::format("{}.{}", fullName, nodeName);
				else
					childFullName = nodeName;
				int idx = fullNameIdx->lookupInt(nodeName);
				childFullName += fmt::format("[{}]", idx);
				fullNameIdx->replace(nodeName, idx + 1);
				scanFormNode((ZxElement*)node, childFullName, formValues);
			}
			else if (node->isElement("subform"))
			{
				scanFormNode((ZxElement*)node, fullName, formValues);
			}
		}
	}
	delete fullNameIdx;
}

void XFAScanner::scanNode(ZxElement* elem, const std::string& parentName, const std::string& parentFullName, GHash* nameIdx, GHash* fullNameIdx, const std::string& exclGroupName, ZxElement* dataElem, UMAP_STR_STR& formValues)
{
	const auto nodeName = getNodeName(elem);
	GHash*     childNameIdx;
	if (!nameIdx || nodeName.size())
		childNameIdx = new GHash();
	else
		childNameIdx = nameIdx;
	const auto nodeFullName = getNodeFullName(elem);
	GHash*     childFullNameIdx;
	if (!fullNameIdx || (nodeFullName.size() && !elem->isElement("area")))
		childFullNameIdx = new GHash();
	else
		childFullNameIdx = fullNameIdx;

	std::string childName;
	if (nodeName.size())
	{
		if (parentName.size())
			childName = fmt::format("{}.{}", parentName, nodeName);
		else
			childName = nodeName;
		int idx = nameIdx->lookupInt(nodeName);
		nameIdx->replace(nodeName, idx + 1);
		if (nodeIsBindGlobal(elem))
			childName += fmt::format("[0]");
		else
			childName += fmt::format("[{}]", idx);
	}
	else
	{
		childName = parentName;
	}
	std::string childFullName;
	if (nodeFullName.size())
	{
		if (parentFullName.size())
			childFullName = fmt::format("{}.{}", parentFullName, nodeFullName);
		else
			childFullName = nodeFullName;
		int idx = fullNameIdx->lookupInt(nodeFullName);
		fullNameIdx->replace(nodeFullName, idx + 1);
		childFullName += fmt::format("[{}]", idx);
	}
	else
	{
		childFullName = parentFullName;
	}

	if (elem->isElement("field"))
	{
		if (childName.size() && childFullName.size())
			scanField(elem, childName, childFullName, exclGroupName, dataElem, formValues);
	}
	else
	{
		std::string childExclGroupName;
		if (elem->isElement("exclGroup"))
			childExclGroupName = childName;
		for (ZxNode* child = elem->getFirstChild(); child; child = child->getNextChild())
			if (child->isElement())
				scanNode((ZxElement*)child, childName, childFullName, childNameIdx, childFullNameIdx, childExclGroupName, dataElem, formValues);
	}

	if (childNameIdx != nameIdx)
		delete childNameIdx;
	if (childFullNameIdx != fullNameIdx)
		delete childFullNameIdx;
}

void XFAScanner::scanField(ZxElement* elem, const std::string& name, const std::string& fullName, const std::string& exclGroupName, ZxElement* dataElem, UMAP_STR_STR& formValues)
{
	std::string          value       = getFieldValue(elem, name, fullName, exclGroupName, dataElem, formValues);
	XFAFieldLayoutInfo*  layoutInfo  = getFieldLayoutInfo(elem);
	XFAFieldPictureInfo* pictureInfo = getFieldPictureInfo(elem);
	XFAFieldBarcodeInfo* barcodeInfo = getFieldBarcodeInfo(elem);
	fields.emplace(fullName, XFAField(name, fullName, value, layoutInfo, pictureInfo, barcodeInfo));
}

std::string XFAScanner::getFieldValue(ZxElement* elem, const std::string& name, const std::string& fullName, const std::string& exclGroupName, ZxElement* dataElem, UMAP_STR_STR& formValues)
{
	//--- check the <xfa:datasets> packet
	auto val = getDatasetsValue(name.c_str(), dataElem);
	if (val.empty() && exclGroupName.size())
		val = getDatasetsValue(exclGroupName.c_str(), dataElem);

	//--- check the <form> element
	if (val.empty())
		val = FindDefault(formValues, fullName, "");

	//--- check the <value> element within the field
	if (val.empty())
	{
		ZxElement* valueElem = elem->findFirstChildElement("value");
		if (valueElem)
		{
			ZxNode* child1Node = valueElem->getFirstChild();
			if (child1Node && child1Node->isElement())
			{
				ZxNode* child2Node = ((ZxElement*)child1Node)->getFirstChild();
				if (child2Node && child2Node->isCharData())
					val = ((ZxCharData*)child2Node)->getData();
			}
		}
	}

	//--- get the checkbutton item value
	std::string checkbuttonItem;
	ZxElement*  uiElem = elem->findFirstChildElement("ui");
	if (uiElem)
	{
		ZxNode* uiChild = uiElem->getFirstChild();
		if (uiChild && uiChild->isElement("checkButton"))
		{
			ZxElement* itemsElem = elem->findFirstChildElement("items");
			if (itemsElem)
			{
				ZxNode* node1 = itemsElem->getFirstChild();
				if (node1 && node1->isElement())
				{
					ZxNode* node2 = ((ZxElement*)node1)->getFirstChild();
					if (node2 && node2->isCharData())
						checkbuttonItem = ((ZxCharData*)node2)->getData();
				}
			}
		}
	}
	// convert XFA checkbutton value to AcroForm-style On/Off value
	if (checkbuttonItem.size() && val.size())
		if (val != checkbuttonItem)
			val = "Off";
		else
			val = "On";

	return val;
}

std::string XFAScanner::getDatasetsValue(const char* partName, ZxElement* elem)
{
	if (!elem) return "";

	// partName = xxxx[nn].yyyy----
	const char* p = strchr(partName, '[');
	if (!p) return "";
	int partLen = (int)(p - partName);
	int idx     = atoi(p + 1);
	p           = strchr(p + 1, '.');
	if (p) ++p;

	int curIdx = 0;
	for (ZxNode* node = elem->getFirstChild(); node; node = node->getNextChild())
	{
		if (!node->isElement())
			continue;
		const auto nodeName = ((ZxElement*)node)->getType();
		if (nodeName.size() != partLen || strncmp(nodeName.c_str(), partName, partLen))
			continue;
		if (curIdx != idx)
		{
			++curIdx;
			continue;
		}
		if (p)
		{
			const auto val = getDatasetsValue(p, (ZxElement*)node);
			if (val.size()) return val;
			break;
		}
		else
		{
			ZxNode* child = ((ZxElement*)node)->getFirstChild();
			if (!child || !child->isCharData()) return "";
			return ((ZxCharData*)child)->getData();
		}
	}

	// search for an 'ancestor match'
	if (p) return getDatasetsValue(p, elem);

	return "";
}

XFAFieldLayoutInfo* XFAScanner::getFieldLayoutInfo(ZxElement* elem)
{
	ZxElement* paraElem = elem->findFirstChildElement("para");
	if (!paraElem) return nullptr;
	XFAFieldLayoutHAlign hAlign     = xfaFieldLayoutHAlignLeft;
	ZxAttr*              hAlignAttr = paraElem->findAttr("hAlign");
	if (hAlignAttr)
	{
		if (hAlignAttr->getValue() == "left")
			hAlign = xfaFieldLayoutHAlignLeft;
		else if (hAlignAttr->getValue() == "center")
			hAlign = xfaFieldLayoutHAlignCenter;
		else if (hAlignAttr->getValue() == "right")
			hAlign = xfaFieldLayoutHAlignRight;
	}
	XFAFieldLayoutVAlign vAlign     = xfaFieldLayoutVAlignTop;
	ZxAttr*              vAlignAttr = paraElem->findAttr("vAlign");
	if (vAlignAttr)
	{
		if (vAlignAttr->getValue() == "top")
			vAlign = xfaFieldLayoutVAlignTop;
		else if (vAlignAttr->getValue() == "middle")
			vAlign = xfaFieldLayoutVAlignMiddle;
		else if (vAlignAttr->getValue() == "bottom")
			vAlign = xfaFieldLayoutVAlignBottom;
	}
	return new XFAFieldLayoutInfo(hAlign, vAlign);
}

XFAFieldPictureInfo* XFAScanner::getFieldPictureInfo(ZxElement* elem)
{
	ZxElement* uiElem = elem->findFirstChildElement("ui");
	if (!uiElem) return nullptr;
	XFAFieldPictureSubtype subtype;
	if (uiElem->findFirstChildElement("dateTimeEdit"))
		subtype = xfaFieldPictureDateTime;
	else if (uiElem->findFirstChildElement("numericEdit"))
		subtype = xfaFieldPictureNumeric;
	else if (uiElem->findFirstChildElement("textEdit"))
		subtype = xfaFieldPictureText;
	else
		return nullptr;

	ZxElement *formatElem, *pictureElem;
	ZxNode*    pictureChildNode;
	if (!(formatElem = elem->findFirstChildElement("format")) || !(pictureElem = formatElem->findFirstChildElement("picture")) || !(pictureChildNode = pictureElem->getFirstChild()) || !pictureChildNode->isCharData())
		return nullptr;
	const auto format = ((ZxCharData*)pictureChildNode)->getData();

	return new XFAFieldPictureInfo(subtype, format);
}

XFAFieldBarcodeInfo* XFAScanner::getFieldBarcodeInfo(ZxElement* elem)
{
	ZxElement *uiElem, *barcodeElem;
	if (!(uiElem = elem->findFirstChildElement("ui")) || !(barcodeElem = uiElem->findFirstChildElement("barcode")))
		return nullptr;

	ZxAttr* attr;
	if (!(attr = barcodeElem->findAttr("type"))) return nullptr;
	const auto barcodeType = attr->getValue();

	double wideNarrowRatio = 3;
	if ((attr = barcodeElem->findAttr("wideNarrowRatio")))
	{
		const char* s     = attr->getValue().c_str();
		const char* colon = strchr(s, ':');
		if (colon)
		{
			const auto numStr = std::string(s, (int)(colon - s));
			double     num    = atof(numStr.c_str());
			double     den    = atof(colon + 1);
			if (den == 0)
				wideNarrowRatio = num;
			else
				wideNarrowRatio = num / den;
		}
		else
		{
			wideNarrowRatio = atof(s);
		}
	}

	double moduleWidth = (0.25 / 25.4) * 72.0; // 0.25mm
	if ((attr = barcodeElem->findAttr("moduleWidth")))
		moduleWidth = getMeasurement(attr->getValue());

	double moduleHeight = (5.0 / 25.4) * 72.0; // 5mm
	if ((attr = barcodeElem->findAttr("moduleHeight")))
		moduleHeight = getMeasurement(attr->getValue());

	int dataLength = 0;
	if ((attr = barcodeElem->findAttr("dataLength")))
		dataLength = atoi(attr->getValue().c_str());

	int errorCorrectionLevel = 0;
	if ((attr = barcodeElem->findAttr("errorCorrectionLevel")))
		errorCorrectionLevel = atoi(attr->getValue().c_str());

	std::string textLocation;
	if ((attr = barcodeElem->findAttr("textLocation")))
		textLocation = attr->getValue();
	else
		textLocation = "below";

	return new XFAFieldBarcodeInfo(barcodeType, wideNarrowRatio, moduleWidth, moduleHeight, dataLength, errorCorrectionLevel, textLocation);
}

double XFAScanner::getMeasurement(const std::string& s)
{
	int  i   = 0;
	bool neg = false;
	if (i < s.size() && s.at(i) == '+')
	{
		++i;
	}
	else if (i < s.size() && s.at(i) == '-')
	{
		neg = true;
		++i;
	}
	double val = 0;
	while (i < s.size() && s.at(i) >= '0' && s.at(i) <= '9')
	{
		val = val * 10 + s.at(i) - '0';
		++i;
	}
	if (i < s.size() && s.at(i) == '.')
	{
		++i;
		double mul = 0.1;
		while (i < s.size() && s.at(i) >= '0' && s.at(i) <= '9')
		{
			val += mul * (s.at(i) - '0');
			mul *= 0.1;
			++i;
		}
	}
	if (neg)
		val = -val;
	if (i + 1 < s.size())
	{
		if (s.at(i) == 'i' && s.at(i + 1) == 'n')
		{
			val *= 72;
		}
		else if (s.at(i) == 'p' && s.at(i + 1) == 't')
		{
			// no change
		}
		else if (s.at(i) == 'c' && s.at(i + 1) == 'm')
		{
			val *= 72 / 2.54;
		}
		else if (s.at(i) == 'm' && s.at(i + 1) == 'm')
		{
			val *= 72 / 25.4;
		}
		else
		{
			// default to inches
			val *= 72;
		}
	}
	else
	{
		// default to inches
		val *= 72;
	}
	return val;
}

std::string XFAScanner::getNodeName(ZxElement* elem)
{
	if (elem->isElement("template") || elem->isElement("area") || elem->isElement("draw")) return "";
	if (!elem->isElement("field") && nodeIsBindNone(elem)) return "";
	ZxAttr* nameAttr = elem->findAttr("name");
	if (!nameAttr) return "";
	return nameAttr->getValue();
}

std::string XFAScanner::getNodeFullName(ZxElement* elem)
{
	if (elem->isElement("template") || elem->isElement("draw")) return "";
	ZxAttr* nameAttr = elem->findAttr("name");
	if (!nameAttr) return "";
	return nameAttr->getValue();
}

bool XFAScanner::nodeIsBindGlobal(ZxElement* elem)
{
	ZxElement* bindElem = elem->findFirstChildElement("bind");
	if (!bindElem) return false;
	ZxAttr* attr = bindElem->findAttr("match");
	return attr && attr->getValue() == "global";
}

bool XFAScanner::nodeIsBindNone(ZxElement* elem)
{
	ZxElement* bindElem = elem->findFirstChildElement("bind");
	if (!bindElem) return false;
	ZxAttr* attr = bindElem->findAttr("match");
	return attr && attr->getValue() == "none";
}
