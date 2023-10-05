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
using namespace icu;

namespace Transfuse {

// Merges sibling w:t elements, except that w:t are never direct siblings - they're contained in w:r elements
// Very similar to pptx_merge_at(), but DOCX uses <w:b/>, <w:i/>, and a parent <w:hyperlink> instead
void docx_merge_wt(State& state, xmlDocPtr xml) {
	auto ctx = xmlXPathNewContext(xml);
	if (ctx == nullptr) {
		throw std::runtime_error("Could not create XPath context");
	}

	if (xmlXPathRegisterNs(ctx, XC("w"), XC("http://schemas.openxmlformats.org/wordprocessingml/2006/main")) != 0) {
		throw std::runtime_error("Could not register namespace w");
	}

	// Find all paragraphs
	auto rs = xmlXPathNodeEval(reinterpret_cast<xmlNodePtr>(xml), XC("//w:p"), ctx);
	if (rs == nullptr) {
		xmlXPathFreeContext(ctx);
		throw std::runtime_error("Could not execute XPath search for w:p elements");
	}

	if (xmlXPathNodeSetIsEmpty(rs->nodesetval)) {
		xmlXPathFreeObject(rs);
		throw std::runtime_error("XPath found zero w:p elements");
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
		// First merge all sibling <w:r><w:t>...</w:t></w:r>
		auto ts = xmlXPathNodeEval(ns->nodeTab[i], XC(".//w:t"), ctx);
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
			if (tag.find(XC("<w:b/>")) != xmlString::npos && tag.find(XC("<w:i/>")) != xmlString::npos) {
				type = XC("b+i");
			}
			else if (tag.find(XC("<w:b/>")) != xmlString::npos) {
				type = XC("b");
			}
			else if (tag.find(XC("<w:i/>")) != xmlString::npos) {
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
			append_xml(tmp, content);
			tmp += TFI_CLOSE;

			if (bp->prev && xmlStrcmp(bp->prev->name, XC("tf-text")) == 0) {
				assign_xml(content, bp->prev->children->content);
				content += tmp;
				xmlNodeSetContent(bp->prev, content.c_str());
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

		// Merge <w:hyperlink>...</w:hyperlink> into child <tf-text>
		auto hs = xmlXPathNodeEval(ns->nodeTab[i], XC(".//w:hyperlink"), ctx);
		if (hs == nullptr) {
			xmlXPathFreeContext(ctx);
			throw std::runtime_error("Could not execute XPath search");
		}

		if (xmlXPathNodeSetIsEmpty(hs->nodesetval)) {
			xmlXPathFreeObject(hs);
			continue;
		}

		for (int j = 0; j < hs->nodesetval->nodeNr; ++j) {
			auto node = hs->nodesetval->nodeTab[j];
			auto text = node->children;
			// Don't merge if this hyperlink has other data, such as TOCs do
			if (text->next) {
				continue;
			}
			xmlUnlinkNode(text);
			xmlAddPrevSibling(node, text);

			xmlNodeSetContent(node, XC(TF_SENTINEL));

			xmlBufferEmpty(buf);
			auto sz = xmlNodeDump(buf, node->doc, node, 0, 0);
			tag.assign(buf->content, buf->content + sz);

			auto s = tag.find(XC(TF_SENTINEL));
			tmp.assign(tag.begin() + PD(s) + 3, tag.end());
			tag.erase(s);
			auto hash = state.style(XC("a"), tag, tmp);

			content = XC(TFI_OPEN_B);
			content += "a:";
			content += hash;
			content += TFI_OPEN_E;
			content += text->children->content ? text->children->content : XC("");
			content += TFI_CLOSE;

			xmlNodeSetContent(text->children, content.c_str());
			xmlUnlinkNode(node);
			xmlFreeNode(node);
		}
	}

	xmlBufferFree(buf);
	state.commit();
}

std::unique_ptr<DOM> extract_docx(State& state) {
	int e = 0;
	auto zip = zip_open("original", ZIP_RDONLY, &e);
	if (zip == nullptr) {
		throw std::runtime_error(concat("Could not open DOCX file: ", std::to_string(e)));
	}

	zip_stat_t stat{};

	// DOCX allows changing the name of the main document, so handle that if word/document.xml doesn't exist
	std::string docname{"word/document.xml"};
	if (zip_stat(zip, docname.c_str(), 0, &stat) != 0) {
		if (zip_stat(zip, "[Content_Types].xml", 0, &stat) != 0) {
			throw std::runtime_error("DOCX did not have [Content_Types].xml");
		}
		if (stat.size == 0) {
			throw std::runtime_error("DOCX [Content_Types].xml was empty");
		}

		auto zf = zip_fopen_index(zip, stat.index, 0);
		if (zf == nullptr) {
			throw std::runtime_error("Could not open DOCX [Content_Types].xml");
		}

		std::string ctypes(stat.size, 0);
		zip_fread(zf, &ctypes[0], stat.size);
		zip_fclose(zf);

		auto off = ctypes.find(".xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"");
		if (off != std::string::npos) {
			auto nb = ctypes.rfind('"', off) + 1;
			auto ne = ctypes.find('"', off);
			docname.assign(ctypes.begin() + nb, ctypes.begin() + ne);
		}
		else if ((off = ctypes.find(" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\" PartName=\"")) != std::string::npos) {
			auto nb = ctypes.find("PartName=\"", off) + 10;
			auto ne = ctypes.find('"', nb);
			docname.assign(ctypes.begin() + nb, ctypes.begin() + ne);
		}
		if (docname[0] == '/') {
			docname.erase(docname.begin());
		}
	}

	state.info("docx-document-main", docname);

	if (zip_stat(zip, docname.c_str(), 0, &stat) != 0) {
		throw std::runtime_error(concat("DOCX did not have main document ", docname));
	}
	if (stat.size == 0) {
		throw std::runtime_error(concat("DOCX main document ", docname, " was empty"));
	}

	auto zf = zip_fopen_index(zip, stat.index, 0);
	if (zf == nullptr) {
		throw std::runtime_error(concat("Could not open DOCX main document ", docname));
	}

	std::string data(stat.size, 0);
	zip_fread(zf, &data[0], stat.size);
	zip_fclose(zf);

	zip_close(zip);

	auto udata = UnicodeString::fromUTF8(data);

	udata.findAndReplace(" encoding=\"UTF-8\"", " encoding=\"UTF-16\"");

	// Wipe chaff that's not relevant when translated, or simply superfluous
	udata.findAndReplace(" xml:space=\"preserve\"", "");
	udata.findAndReplace(" w:eastAsiaTheme=\"minorHAnsi\"", "");
	udata.findAndReplace(" w:type=\"textWrapping\"", "");

	UnicodeString tmp;

	// Revision tracking information
	rx_replaceAll(R"X( w:rsidP="[^"]+")X", "", udata, tmp);
	rx_replaceAll(R"X( w:rsidRDefault="[^"]+")X", "", udata, tmp);
	rx_replaceAll(R"X( w:rsidR="[^"]+")X", "", udata, tmp);
	rx_replaceAll(R"X( w:rsidRPr="[^"]+")X", "", udata, tmp);
	rx_replaceAll(R"X( w:rsidDel="[^"]+")X", "", udata, tmp);

	// Other full-tag chaff, intentionally done after attributes because removing those may leave these tags empty
	rx_replaceAll(R"X(<w:lang(?=[ >])[^/>]+/>)X", "", udata, tmp);
	rx_replaceAll(R"X(<w:proofErr(?=[ >])[^/>]+/>)X", "", udata, tmp);

	udata.findAndReplace("<w:noProof/>", "");
	udata.findAndReplace("<w:lastRenderedPageBreak/>", "");
	udata.findAndReplace("<w:color w:val=\"auto\"/>", "");
	udata.findAndReplace("<w:rFonts/>", "");
	udata.findAndReplace("<w:rFonts></w:rFonts>", "");
	udata.findAndReplace("<w:rPr></w:rPr>", "");
	udata.findAndReplace("<w:softHyphen/>", "");
	udata.findAndReplace("<w:br/>", "<w:t>\n</w:t>");
	udata.findAndReplace("<w:cr/>", "<w:t>\n</w:t>");
	udata.findAndReplace("<w:noBreakHyphen/>", "<w:t>-</w:t>");

	rx_replaceAll(R"X(</w:t>([^<>]*?)<w:t(?=[ >])[^>]*>)X", "", udata, tmp);

	// Move <w:tab> to its very own <w:r> so it doesn't interfere with <w:t> merging or style hashing
	UErrorCode status = U_ZERO_ERROR;
	RegexMatcher rx_wr(R"X(<w:r(?=[ >])[^>]*>.*?</w:r>)X", 0, status);

	tmp.remove();
	rx_wr.reset(udata);
	int32_t last = 0;
	while (rx_wr.find(last, status)) {
		auto mb = rx_wr.start(0, status);
		auto me = rx_wr.end(0, status);

		tmp.append(udata, last, mb - last);
		int32_t tab = 0;
		if ((tab = udata.indexOf("<w:tab/><w:t>", mb, me - mb)) != -1) {
			tmp.append(udata, mb, tab - mb);
			tmp.append("<w:tab/></w:r>");
			tmp.append(udata, mb, tab - mb);
			tmp.append(udata, tab + 8, me - tab - 8);
		}
		else {
			tmp.append(udata, mb, me - mb);
		}
		last = me;
	}
	tmp.append(udata, last, udata.length() - last);
	std::swap(tmp, udata);

	auto xml = xmlReadMemory(reinterpret_cast<const char*>(udata.getTerminatedBuffer()), SI(SZ(udata.length()) * sizeof(UChar)), "document.xml", utf16_native, XML_PARSE_RECOVER | XML_PARSE_NONET);
	if (xml == nullptr) {
		throw std::runtime_error(concat("Could not parse document.xml: ", xmlLastError.message));
	}
	udata.remove();
	tmp.remove();

	docx_merge_wt(state, xml);

	auto dom = std::make_unique<DOM>(state, xml);
	dom->tags_parents_allow = make_xmlChars("tf-text", "w:t");
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

std::string inject_docx(DOM& dom) {
	auto buf = xmlBufferCreate();
	auto obuf = xmlOutputBufferCreateBuffer(buf, nullptr);
	xmlSaveFileTo(obuf, dom.xml.get(), "UTF-8");
	std::string data(buf->content, buf->content + buf->use);
	xmlBufferFree(buf);

	auto udata = UnicodeString::fromUTF8(data);
	UnicodeString tmp;

	// DOCX can't have any text outside w:t
	// Wrap tags around text after </w:t></w:r>, in a way that does not inherit formatting
	rx_replaceAll(R"X((</w:t></w:r>)([^<>]+))X", "$1<w:r><w:t>$2</w:t></w:r>", udata, tmp);

	// Ditto for text after </w:t></w:r></w:hyperlink>
	rx_replaceAll(R"X((</w:t></w:r></w:hyperlink>)([^<>]+))X", "$1<w:r><w:t>$2</w:t></w:r>", udata, tmp);

	// Move text from before <w:r><w:t> inside it
	rx_replaceAll_expand_21(R"X(([^>])(<w:r(?=[ >])[^>]*>.*?<w:t(?=[ >])[^>]*>))X", udata, tmp);

	// Move text from before <w:hyperlink><w:r><w:t> inside it
	rx_replaceAll_expand_21(R"X(([^>])(<w:hyperlink(?=[ >])[^>]*>.*?<w:r(?=[ >])[^>]*>.*?<w:t(?=[ >])[^>]*>))X", udata, tmp);

	// Remove empty text elements
	rx_replaceAll(R"X(<w:r><w:t/></w:r>)X", "", udata, tmp);

	// Remove the <tf-text> helper elements that we added
	rx_replaceAll(R"X(</?tf-text>)X", "", udata, tmp);

	// DOCX by default does ignores all leading/trailing whitespace, so tell it not do.
	// ToDo: xml:space=preserve needs adjusting to only be added where it makes sense, such as not before punctuation
	rx_replaceAll(R"X(<w:t([ >]))X", "<w:t xml:space=\"preserve\"$1", udata, tmp);

	data.clear();
	udata.toUTF8String(data);
	file_save("injected.xml", data);

	fs::copy("original", "injected.docx");

	int e = 0;
	auto zip = zip_open("injected.docx", 0, &e);
	if (zip == nullptr) {
		throw std::runtime_error(concat("Could not open DOCX file: ", std::to_string(e)));
	}

	auto src = zip_source_file(zip, "injected.xml", 0, 0);
	if (src == nullptr) {
		throw std::runtime_error("Could not open injected.xml");
	}

	auto docname = dom.state.info("docx-document-main");
	if (zip_file_add(zip, docname.c_str(), src, ZIP_FL_OVERWRITE) < 0) {
		throw std::runtime_error(concat("Could not replace main document ", docname));
	}

	zip_close(zip);

	return "injected.docx";
}

}
