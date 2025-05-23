/*
* Copyright (C) 2020 Tino Didriksen <mail@tinodidriksen.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "shared.hpp"
#include "filesystem.hpp"
#include "state.hpp"
#include "dom.hpp"
#include "formats.hpp"
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/xmlsave.h>
#include <unicode/regex.h>
#include <unicode/ustring.h>
#include <memory>
using namespace icu;

namespace Transfuse {

// Find all post[@generatedBy="human"] and their ab[@type="content"] that aren't generate by non-human
void tei_find_text(State& state, xmlDocPtr xml) {
	auto ctx = xmlXPathNewContext(xml);
	if (ctx == nullptr) {
		throw std::runtime_error("Could not create XPath context");
	}

	// The default namespace is not the same as a null namespace
	if (xmlXPathRegisterNs(ctx, XC("x"), XC("http://www.tei-c.org/ns/1.0")) != 0) {
		throw std::runtime_error("Could not register default namespace");
	}

	state.begin();

	xmlString tag;
	xmlString tmp;
	xmlString content;
	auto buf = xmlBufferCreate();
	xmlXPathObjectPtr rs = nullptr;
	xmlNodeSetPtr ns = nullptr;

	/*
	// Find all potential persName
	rs = xmlXPathNodeEval(reinterpret_cast<xmlNodePtr>(xml), XC("//x:post[@generatedBy='human']/x:ab/x:persName"), ctx);
	if (rs == nullptr) {
		xmlXPathFreeContext(ctx);
		throw std::runtime_error("Could not execute XPath search for persName elements");
	}

	// For each persName descendent of generatedBy="human|system" that has role="author", mark as protected
	if (!xmlXPathNodeSetIsEmpty(rs->nodesetval)) {
		ns = rs->nodesetval;
		for (int i = 0; i < ns->nodeNr; ++i) {
			auto node = ns->nodeTab[i];
			auto ab = node->parent;
			auto gb = xmlGetAttribute(ab, XCV("generatedBy"));
			if (!gb.empty() && gb != XCV("human") && gb != XCV("system")) {
				continue;
			}

			gb = xmlGetAttribute(node, XCV("role"));
			if (gb.find(XCV("author")) == xmlChar_view::npos) {
				continue;
			}

			auto nn = xmlNewNode(nullptr, XC("tf-protect"));
			if (node->prev) {
				auto p = node->prev;
				xmlUnlinkNode(node);
				xmlAddChild(nn, node);
				xmlAddNextSibling(p, nn);
			}
			else if (node->next) {
				auto p = node->next;
				xmlUnlinkNode(node);
				xmlAddChild(nn, node);
				xmlAddPrevSibling(p, nn);
			}
			else {
				xmlUnlinkNode(node);
				xmlAddChild(nn, node);
				xmlAddChild(ab, nn);
			}
		}
		xmlXPathFreeObject(rs);
	}


	// Find all potential seg
	rs = xmlXPathNodeEval(reinterpret_cast<xmlNodePtr>(xml), XC("//x:post[@generatedBy='human']/x:ab/x:seg"), ctx);
	if (rs == nullptr) {
		xmlXPathFreeContext(ctx);
		throw std::runtime_error("Could not execute XPath search for persName elements");
	}

	// For each seg descendent of generatedBy="human" that has generatedBy!="human|system", mark as protected
	if (!xmlXPathNodeSetIsEmpty(rs->nodesetval)) {
		ns = rs->nodesetval;
		for (int i = 0; i < ns->nodeNr; ++i) {
			auto node = ns->nodeTab[i];
			auto ab = node->parent;
			auto gb = xmlGetAttribute(node, XCV("generatedBy"));
			if (gb.empty() || gb == XCV("human") || gb == XCV("system")) {
				continue;
			}

			auto nn = xmlNewNode(nullptr, XC("tf-protect"));
			if (node->prev) {
				auto p = node->prev;
				xmlUnlinkNode(node);
				xmlAddChild(nn, node);
				xmlAddNextSibling(p, nn);
			}
			else if (node->next) {
				auto p = node->next;
				xmlUnlinkNode(node);
				xmlAddChild(nn, node);
				xmlAddPrevSibling(p, nn);
			}
			else {
				xmlUnlinkNode(node);
				xmlAddChild(nn, node);
				xmlAddChild(ab, nn);
			}
		}
		xmlXPathFreeObject(rs);
	}
	//*/

	// Find all potential figDesc
	rs = xmlXPathNodeEval(reinterpret_cast<xmlNodePtr>(xml), XC("//x:post[@generatedBy='human']/x:ab/x:figure/x:figDesc"), ctx);
	if (rs == nullptr) {
		xmlXPathFreeContext(ctx);
		throw std::runtime_error("Could not execute XPath search for figDesc elements");
	}

	// For each figDesc descendent of generatedBy="human|system", turn it into an inline element.
	if (!xmlXPathNodeSetIsEmpty(rs->nodesetval)) {
		ns = rs->nodesetval;
		for (int i = 0; i < ns->nodeNr; ++i) {
			auto node = ns->nodeTab[i];
			auto fig = node->parent;
			auto ab = fig->parent;
			auto gb = xmlGetAttribute(ab, XCV("generatedBy"));
			if (!gb.empty() && gb != XCV("human") && gb != XCV("system")) {
				continue;
			}

			// ToDo: Snip figDesc @generatedBy
			gb = xmlGetAttribute(node, XCV("generatedBy"));
			if (!gb.empty() && gb != XCV("human") && gb != XCV("system")) {
				continue;
			}

			gb = xmlGetAttribute(node, XCV("source"));
			if (!gb.empty() && gb != XCV("human") && gb != XCV("system")) {
				continue;
			}

			content = node->children->content ? node->children->content : XC("");
			xmlNodeSetContent(node, XC(TF_SENTINEL));

			auto bp = node->parent;
			xmlBufferEmpty(buf);
			xmlNodeDump(buf, bp->doc, bp, 0, 0);
			tag.assign(buf->content, buf->content + buf->use);

			xmlChar_view type{ XC("figure") };

			auto s = tag.find(XC(TF_SENTINEL));
			tmp.assign(tag.begin() + PD(s) + 3, tag.end());
			tag.erase(s);
			auto hash = state.style(type, tag, tmp);

			tmp = XC(TFI_OPEN_B);
			tmp += type;
			tmp += ':';
			tmp += hash;
			tmp += TFI_OPEN_E;
			append_xml(tmp, content);
			tmp += TFI_CLOSE;

			if (bp->prev && xmlStrcmp(bp->prev->name, XC("tf-text")) == 0) {
				assign_xml(content, bp->prev->children->content);
				content += tmp;
				xmlNodeSetContent(bp->prev, content.c_str());
			}
			else if (bp->prev && bp->prev->type == XML_TEXT_NODE) {
				assign_xml(content, bp->prev->content);
				content += tmp;
				xmlNodeSetContent(bp->prev, content.c_str());
			}
			else if (bp->next && xmlStrcmp(bp->next->name, XC("tf-text")) == 0) {
				append_xml(tmp, bp->next->children->content);
				xmlNodeSetContent(bp->next, tmp.c_str());
			}
			else if (bp->next && bp->next->type == XML_TEXT_NODE) {
				append_xml(tmp, bp->next->content);
				xmlNodeSetContent(bp->next, tmp.c_str());
			}
			else {
				auto nn = xmlNewText(tmp.c_str());
				if (bp->prev) {
					xmlAddPrevSibling(bp, nn);
				}
				else {
					xmlAddNextSibling(bp, nn);
				}
			}
			xmlUnlinkNode(bp);
			xmlFreeNode(bp);
		}
		xmlXPathFreeObject(rs);
	}


	// Find all potential ab
	rs = xmlXPathNodeEval(reinterpret_cast<xmlNodePtr>(xml), XC("//x:post[@generatedBy='human']/x:ab"), ctx);
	if (rs == nullptr) {
		xmlXPathFreeContext(ctx);
		throw std::runtime_error("Could not execute XPath search for ab elements");
	}

	if (xmlXPathNodeSetIsEmpty(rs->nodesetval)) {
		xmlXPathFreeObject(rs);
		throw std::runtime_error("XPath found zero ab elements");
	}

	// For each ab, find human or system texts
	// This creates <tf-text> elements, which will be removed after injection
	ns = rs->nodesetval;
	for (int i = 0; i < ns->nodeNr; ++i) {
		auto node = ns->nodeTab[i];
		auto gb = xmlGetAttribute(node, XCV("generatedBy"));
		if (!gb.empty() && gb != XCV("human") && gb != XCV("system")) {
			continue;
		}
		if (!node->children) {
			continue;
		}

		auto nn = xmlNewNode(nullptr, XC("tf-text"));
		while (node->children) {
			auto c = node->children;
			xmlUnlinkNode(c);
			xmlAddChild(nn, c);
		}
		xmlAddChild(node, nn);
	}
	xmlXPathFreeObject(rs);

	xmlBufferFree(buf);
	state.commit();
}

std::unique_ptr<DOM> extract_tei(State& state) {
	auto raw_data = file_load("original");
	auto enc = detect_encoding(raw_data);
	UnicodeString data = to_ustring(raw_data, enc);
	UnicodeString tmp;

	// Put spaces around <lb/> to avoid merging, and record that we did so
	rx_replaceAll(R"X(([^\s\p{Z}<>;&])<lb/>([^\s\p{Z}<>;&]))X", "$1 <lb tf-added-before=\"1\" tf-added-after=\"1\"/> $2", data, tmp);
	rx_replaceAll(R"X(([^\s\p{Z}<>;&])<lb/>)X", "$1 <lb tf-added-before=\"1\"/>", data, tmp);
	rx_replaceAll(R"X(<lb/>([^\s\p{Z}<>;&]))X", "<lb tf-added-after=\"1\"/> $1", data, tmp);

	auto xml = xmlReadMemory(reinterpret_cast<const char*>(data.getTerminatedBuffer()), SI(SZ(data.length()) * sizeof(UChar)), "content.xml", utf16_native, XML_PARSE_RECOVER | XML_PARSE_NONET);
	if (xml == nullptr) {
		throw std::runtime_error(concat("Could not parse TEI XML: ", xmlLastError.message));
	}

	tei_find_text(state, xml);

	auto dom = std::make_unique<DOM>(state, xml);
	dom->tags[Strs::tags_parents_allow] = make_xmlChars("tf-text");
	dom->tags[Strs::tags_prot] = make_xmlChars("figure", "tf-protect");
	dom->tags[Strs::tags_prot_inline] = make_xmlChars("lb", "space");
	dom->tags[Strs::tags_inline] = make_xmlChars("seg");
	dom->tags[Strs::tags_semantic] = make_xmlChars("date", "persname", "placename", "time");
	dom->tags[Strs::tags_unique] = make_xmlChars("lb", "seg");
	dom->cmdline_tags();
	dom->save_spaces();

	auto styled = dom->save_styles(true);
	file_save("styled.xml", x2s(styled));
	dom->xml.reset(xmlReadMemory(reinterpret_cast<const char*>(styled.data()), SI(styled.size()), "styled.xml", "UTF-8", XML_PARSE_RECOVER | XML_PARSE_NONET));
	if (dom->xml == nullptr) {
		throw std::runtime_error(concat("Could not parse styled XML: ", xmlLastError.message));
	}

	if (state.settings->opt_verbose) {
		std::cerr << "TEI ready for extraction" << std::endl;
	}

	return dom;
}

std::string inject_tei(DOM& dom) {
	auto buf = xmlBufferCreate();
	auto obuf = xmlOutputBufferCreateBuffer(buf, nullptr);
	xmlSaveFileTo(obuf, dom.xml.get(), "UTF-8");
	std::string data(buf->content, buf->content + buf->use);
	xmlBufferFree(buf);

	auto udata = UnicodeString::fromUTF8(data);
	UnicodeString tmp;

	// Remove the <tf-text> and <tf-protect> helper elements that we added
	rx_replaceAll(R"X(</?tf-(text|protect)>)X", "", udata, tmp);

	rx_replaceAll(R"X( <lb tf-added-(before|after)=\"1\" tf-added-(before|after)=\"1\"/> )X", "<lb/>", udata, tmp);
	rx_replaceAll(R"X( <lb tf-added-before=\"1\"/>)X", "<lb/>", udata, tmp);
	rx_replaceAll(R"X(<lb tf-added-after=\"1\"/> )X", "<lb/>", udata, tmp);
	rx_replaceAll(R"X( tf-added-(before|after)=\"1\")X", "", udata, tmp);

	data.clear();
	udata.toUTF8String(data);
	file_save("injected.xml", data);

	hook_inject(dom.state.settings, "injected.xml");

	return "injected.xml";
}

}
