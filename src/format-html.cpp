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
#include <libxml/HTMLparser.h>
#include <libxml/tree.h>
#include <libxml/xmlsave.h>
#include <unicode/regex.h>
#include <memory>
using namespace icu;

namespace Transfuse {

std::unique_ptr<DOM> extract_html(State& state, std::unique_ptr<icu::UnicodeString> data) {
	if (!data) {
		auto raw_data = file_load("original");
		auto enc = detect_encoding(raw_data);
		data = std::make_unique<UnicodeString>(to_ustring(raw_data, enc));
	}

	// Find any charset="" charset='' charset= and replace with a placeholder that we will set to UTF-8 in injection
	UErrorCode status = U_ZERO_ERROR;
	RegexMatcher rx(R"X(charset\s*=(["']?)\s*([-\w\d]+)\s*(["']?))X", UREGEX_CASE_INSENSITIVE, status);
	if (U_FAILURE(status)) {
		throw std::runtime_error(concat("Could not create RegexMatcher: ", u_errorName(status)));
	}

	rx.reset(*data);
	if (rx.find()) {
		UnicodeString cset("charset=");
		auto b = rx.start(1, status);
		auto e = rx.end(1, status);
		cset.append(*data, b, e-b);
		cset += XML_ENC_UC;
		b = rx.start(3, status);
		e = rx.end(3, status);
		cset.append(*data, b, e - b);

		b = rx.start(0, status);
		e = rx.end(0, status);
		data->replace(b, e - b, cset);
	}
	if (U_FAILURE(status)) {
		throw std::runtime_error(concat("Could not replace charset in data: ", u_errorName(status)));
	}

	auto xml = htmlReadMemory(reinterpret_cast<const char*>(data->getTerminatedBuffer()), SI(SZ(data->length()) * sizeof(UChar)), "transfuse.html", "UTF-16", HTML_PARSE_RECOVER | HTML_PARSE_NOWARNING | HTML_PARSE_NOERROR | HTML_PARSE_NONET);
	if (xml == nullptr) {
		throw std::runtime_error(concat("Could not parse HTML: ", xmlLastError.message));
	}
	data.reset();

	auto dom = std::make_unique<DOM>(state, xml);
	dom->tags_prot = make_xmlChars("applet", "area", "base", "cite", "code", "frame", "frameset", "link", "meta", "nowiki", "object", "pre", "ref", "script", "style", "syntaxhighlight", "template");
	dom->tags_prot_inline = make_xmlChars("br", "ruby", "wbr");
	dom->tags_raw = make_xmlChars("script", "style");
	dom->tags_inline = make_xmlChars("a", "abbr", "acronym", "address", "b", "bdi", "bdo", "big", "del", "em", "font", "i", "ins", "kbd", "mark", "meter", "output", "q", "s", "samp", "small", "span", "strike", "strong", "sub", "sup", "time", "tt", "u", "var");
	dom->tag_attrs = make_xmlChars("alt", "caption", "label", "summary", "title", "placeholder");
	dom->save_spaces();

	auto styled = dom->save_styles(true);
	file_save("styled.xml", x2s(styled));
	dom->xml.reset(xmlReadMemory(reinterpret_cast<const char*>(styled.data()), SI(styled.size()), "styled.xml", "UTF-8", XML_PARSE_RECOVER | XML_PARSE_NONET));
	if (dom->xml == nullptr) {
		throw std::runtime_error(concat("Could not parse styled XML: ", xmlLastError.message));
	}

	return dom;
}

std::string inject_html(DOM& dom) {
	auto cntx = xmlSaveToFilename("injected.html", "UTF-8", XML_SAVE_AS_HTML);
	xmlSaveDoc(cntx, dom.xml.get());
	xmlSaveClose(cntx);

	std::ifstream in("original", std::ios::binary);
	in.exceptions(std::ios::badbit | std::ios::failbit);
	std::string line;
	std::getline(in, line);
	bool had_doctype = to_lower(line).find("<!doctype") != std::string::npos;
	in.close();

	auto content = file_load("injected.html");
	auto b = content.find(XML_ENC_U8);
	if (b != std::string::npos) {
		content.replace(b, 3, "UTF-8");
		// libxml2's serializer adds this <meta> tag to be helpful, but we already had one
		std::string meta{ R"X(<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">)X" }; // ToDo: C++17 string_view
		auto m = content.find(meta);
		if (m != std::string::npos) {
			content.erase(m, meta.size());
		}
	}
	if (had_doctype) {
		content.insert(0, "<!DOCTYPE html>\n");
	}
	file_save("injected.html", content);

	return "injected.html";
}

}
