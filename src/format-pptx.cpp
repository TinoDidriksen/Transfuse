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
#include "formats.hpp"
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/xmlsave.h>
#include <unicode/ustring.h>
#include <unicode/regex.h>
#include <zip.h>
#include <deque>
using namespace icu;

namespace Transfuse {

// Merges sibling a:t elements, except that a:t are never direct siblings - they're contained in a:r elements
// Very similar to docx_merge_wt(), but PPTX uses b="1", i="1", and child <a:hlinkClick> instead
void pptx_merge_at(State& state, xmlDocPtr xml) {
	auto ctx = xmlXPathNewContext(xml);
	if (ctx == nullptr) {
		throw std::runtime_error("Could not create XPath context");
	}

	if (xmlXPathRegisterNs(ctx, XC("a"), XC("http://schemas.openxmlformats.org/drawingml/2006/main")) != 0) {
		throw std::runtime_error("Could not register namespace a");
	}

	// Find all paragraphs
	auto rs = xmlXPathNodeEval(reinterpret_cast<xmlNodePtr>(xml), XC("//a:p"), ctx);
	if (rs == nullptr) {
		xmlXPathFreeContext(ctx);
		throw std::runtime_error("Could not execute XPath search for a:p elements");
	}

	if (xmlXPathNodeSetIsEmpty(rs->nodesetval)) {
		xmlXPathFreeObject(rs);
		throw std::runtime_error("XPath found zero a:p elements");
	}

	state.begin();

	xmlString tag;
	xmlString tmp;
	xmlString content;
	auto buf = xmlBufferCreate();

	// For each paragraph, merge all text nodes but remember if they were bold, italic, or hyperlinks
	// This creates <tf-text> elements, which will be removed after injection
	auto ns = rs->nodesetval;
	for (int i = 0; i < ns->nodeNr; ++i) {
		// First merge all sibling <a:r><a:t>...</a:t></a:r>
		auto ts = xmlXPathNodeEval(ns->nodeTab[i], XC(".//a:t"), ctx);
		if (ts == nullptr) {
			xmlXPathFreeContext(ctx);
			throw std::runtime_error("Could not execute XPath search");
		}

		if (xmlXPathNodeSetIsEmpty(ts->nodesetval) || ts->nodesetval->nodeNr == 1) {
			xmlXPathFreeObject(ts);
			continue;
		}

		for (int j = 0; j < ts->nodesetval->nodeNr; ++j) {
			auto node = ts->nodesetval->nodeTab[j];
			content = node->children->content ? node->children->content : XC("");
			xmlNodeSetContent(node, XC(TF_SENTINEL));

			auto bp = node->parent;
			xmlBufferEmpty(buf);
			auto sz = xmlNodeDump(buf, bp->doc, bp, 0, 0);
			tag.assign(buf->content, buf->content + sz);

			xmlChar_view type{ XC("text") };
			auto apos = tag.find(XC("a:hlinkClick"));
			auto bpos = tag.find(XC(" b=\"1\""));
			auto ipos = tag.find(XC(" i=\"1\""));
			if (apos != xmlString::npos && bpos != xmlString::npos && ipos != xmlString::npos) {
				type = XC("a+b+i");
			}
			else if (bpos != xmlString::npos && ipos != xmlString::npos) {
				type = XC("b+i");
			}
			else if (apos != xmlString::npos && bpos != xmlString::npos) {
				type = XC("a+b");
			}
			else if (apos != xmlString::npos && ipos != xmlString::npos) {
				type = XC("a+i");
			}
			else if (apos != xmlString::npos) {
				type = XC("a");
			}
			else if (bpos != xmlString::npos) {
				type = XC("b");
			}
			else if (ipos != xmlString::npos) {
				type = XC("i");
			}

			auto s = tag.find(XC(TF_SENTINEL));
			tmp.assign(tag.begin() + PD(s) + 3, tag.end());
			tag.erase(s);
			auto hash = state.style(type, tag, tmp);

			tmp = XC(TFI_OPEN_B);
			tmp += type;
			tmp += ':';
			tmp += hash;
			tmp += TFI_OPEN_E;
			tmp += content;
			tmp += TFI_CLOSE;

			if (bp->prev && xmlStrcmp(bp->prev->name, XC("tf-text")) == 0) {
				content = bp->prev->children->content;
				content += tmp;
				xmlNodeSetContent(bp->prev->children, content.c_str());
			}
			else {
				auto nn = xmlNewNode(nullptr, XC("tf-text"));
				nn = xmlAddPrevSibling(bp, nn);
				xmlNodeSetContent(nn, tmp.c_str());
			}
			xmlUnlinkNode(bp);
			xmlFreeNode(bp);
			ts->nodesetval->nodeTab[j] = nullptr;
		}
		xmlXPathFreeObject(ts);
	}

	xmlBufferFree(buf);
	state.commit();
}

std::unique_ptr<DOM> extract_pptx(State& state) {
	int e = 0;
	auto zip = zip_open("original", ZIP_RDONLY, &e);
	if (zip == nullptr) {
		throw std::runtime_error(concat("Could not open pptx file: ", std::to_string(e)));
	}

	std::string data{"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<tf-slides>"};
	std::string slide;
	for (int i = 1; ; ++i) {
		char buffer[64]{};
		sprintf(buffer, "ppt/slides/slide%d.xml", i);

		zip_stat_t stat{};
		if (zip_stat(zip, buffer, 0, &stat) != 0) {
			// No more slides
			break;
		}
		if (stat.size == 0) {
			throw std::runtime_error(concat("Empty pptx slide ", buffer));
		}

		auto zf = zip_fopen_index(zip, stat.index, 0);
		if (zf == nullptr) {
			throw std::runtime_error(concat("Could not open pptx ", buffer));
		}

		slide.resize(stat.size, 0);
		zip_fread(zf, &slide[0], stat.size);
		zip_fclose(zf);

		auto xs = slide.find("?>\r\n");
		if (xs != std::string::npos) {
			data.append(slide, xs + 4, std::string::npos);
		}
		else {
			xs = slide.find("?>\n");
			data.append(slide, xs + 3, std::string::npos);
		}
	}
	data += "</tf-slides>";

	zip_close(zip);

	auto udata = UnicodeString::fromUTF8(data);

	udata.findAndReplace(" encoding=\"UTF-8\"", " encoding=\"UTF-16\"");

	// Wipe chaff that's not relevant when translated, or simply superfluous
	UnicodeString tmp;
	UErrorCode status = U_ZERO_ERROR;

	RegexMatcher rx_lang(R"X( lang="[^"]*")X", 0, status);
	rx_lang.reset(udata);
	tmp = rx_lang.replaceAll("", status);
	std::swap(udata, tmp);

	udata.findAndReplace("<a:rPr/>", "");

	RegexMatcher rx_wt(R"X(</a:t>([^<>]+?)<a:t(?=[ >])[^>]*>)X", 0, status);
	rx_wt.reset(udata);
	tmp = rx_wt.replaceAll("", status);
	std::swap(udata, tmp);

	auto xml = xmlReadMemory(reinterpret_cast<const char*>(udata.getTerminatedBuffer()), SI(SZ(udata.length()) * sizeof(UChar)), "slides.xml", utf16_native, XML_PARSE_RECOVER | XML_PARSE_NONET);
	if (xml == nullptr) {
		throw std::runtime_error(concat("Could not parse slides.xml: ", xmlLastError.message));
	}
	udata.remove();
	tmp.remove();

	pptx_merge_at(state, xml);

	auto dom = std::make_unique<DOM>(state, xml);
	dom->tags_parents_allow = make_xmlChars("tf-text", "a:t");
	dom->save_spaces();

	auto buf = xmlBufferCreate();
	auto obuf = xmlOutputBufferCreateBuffer(buf, nullptr);
	xmlSaveFileTo(obuf, xml, "UTF-8");
	data.assign(buf->content, buf->content + buf->use);
	xmlBufferFree(buf);
	cleanup_styles(data);

	auto b = data.rfind("</tf-text><tf-text>");
	while (b != std::string::npos) {
		data.erase(b, 19);
		b = data.rfind("</tf-text><tf-text>");
	}

	dom->xml.reset(xmlReadMemory(reinterpret_cast<const char*>(data.data()), SI(data.size()), "styled.xml", "UTF-8", XML_PARSE_RECOVER | XML_PARSE_NONET));
	if (dom->xml == nullptr) {
		throw std::runtime_error(concat("Could not parse styled XML: ", xmlLastError.message));
	}
	file_save("styled.xml", data);

	return dom;
}

std::string inject_pptx(DOM& dom) {
	auto buf = xmlBufferCreate();
	auto obuf = xmlOutputBufferCreateBuffer(buf, nullptr);
	xmlSaveFileTo(obuf, dom.xml.get(), "UTF-8");
	std::string data(buf->content, buf->content + buf->use);
	xmlBufferFree(buf);

	auto udata = UnicodeString::fromUTF8(data);
	UnicodeString tmp;
	UErrorCode status = U_ZERO_ERROR;

	// pptx can't have any text outside a:t
	// Move text from after </a:t></a:r> inside it
	RegexMatcher rx_after_r(R"X((</a:t></a:r>)([^<>]+))X", 0, status);
	rx_after_r.reset(udata);
	tmp = rx_after_r.replaceAll("$2$1", status);
	std::swap(udata, tmp);

	// Move text from before <a:r><a:t> inside it
	RegexMatcher rx_before_r(R"X(([^<>]+)(<a:r(?=[ >][^>]*>).*?<a:t(?=[ >])[^>]*>))X", 0, status);
	rx_before_r.reset(udata);
	tmp = rx_before_r.replaceAll("$2$1", status);
	std::swap(udata, tmp);

	// Remove empty text elements
	RegexMatcher rx_snip_empty(R"X(<a:r><a:t/></a:r>)X", 0, status);
	rx_snip_empty.reset(udata);
	tmp = rx_snip_empty.replaceAll("", status);
	std::swap(udata, tmp);

	// Remove the <tf-text> helper elements that we added
	RegexMatcher rx_snip_tf(R"X(</?tf-text>)X", 0, status);
	rx_snip_tf.reset(udata);
	tmp = rx_snip_tf.replaceAll("", status);
	std::swap(udata, tmp);

	data.clear();
	udata.toUTF8String(data);
	file_save("injected.xml", data);

	hook_inject(dom.state.settings, "injected.xml");

	fs::copy("original", "injected.pptx");

	int er = 0;
	auto zip = zip_open("injected.pptx", 0, &er);
	if (zip == nullptr) {
		throw std::runtime_error(concat("Could not open pptx file: ", std::to_string(er)));
	}

	std::deque<std::string> slides;
	size_t b = data.find("<p:sld ");
	size_t e = data.find("</p:sld>", b);
	int i = 0;
	while (b != std::string::npos) {
		++i;
		char buffer[64]{};
		sprintf(buffer, "ppt/slides/slide%d.xml", i);

		slides.resize(slides.size() + 1);
		auto& slide = slides.back();
		slide = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
		slide.append(data, b, (e - b) + 8);

		auto src = zip_source_buffer(zip, slide.c_str(), slide.size(), 0);
		if (src == nullptr) {
			throw std::runtime_error(concat("Could not create buffer for ", buffer));
		}

		if (zip_file_add(zip, buffer, src, ZIP_FL_OVERWRITE) < 0) {
			throw std::runtime_error(concat("Could not replace ", buffer));
		}

		b = data.find("<p:sld ", e);
		e = data.find("</p:sld>", b);
	}

	zip_close(zip);

	return "injected.pptx";
}

}
