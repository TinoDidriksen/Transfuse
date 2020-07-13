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
#include <unicode/regex.h>
#include <memory>
using namespace icu;

namespace Transfuse {

std::unique_ptr<DOM> extract_html(fs::path tmpdir, State& state) {
	auto raw_data = file_load(tmpdir / "original");
	auto enc = detect_encoding(raw_data);

	auto data = std::make_unique<UnicodeString>(to_ustring(raw_data, enc));

	// Find any charset="" charset='' charset= and replace with charset UTF-16
	UnicodeString rx_enc(R"X(charset\s*=(["']?)\s*([-\w\d]+)\s*(["']?))X");

	UErrorCode status = U_ZERO_ERROR;
	RegexMatcher rx(rx_enc, UREGEX_CASE_INSENSITIVE, status);
	if (U_FAILURE(status)) {
		throw std::runtime_error(concat("Could not create RegexMatcher: ", u_errorName(status)));
	}

	rx.reset(*data);
	if (rx.find()) {
		UnicodeString cset("charset=");
		auto b = rx.start(1, status);
		auto e = rx.end(1, status);
		cset.append(*data, b, e-b);
		cset.append(XML_ENC_UC);
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

	auto xml = htmlReadMemory(reinterpret_cast<const char*>(data->getTerminatedBuffer()), static_cast<int>(data->length() * sizeof(UChar)), "transfuse.html", utf16_native, HTML_PARSE_RECOVER | HTML_PARSE_NOWARNING | HTML_PARSE_NOERROR | HTML_PARSE_NONET);
	if (xml == nullptr) {
		throw std::runtime_error(concat("Could not parse HTML: ", xmlLastError.message));
	}
	data.reset();

	auto dom = std::make_unique<DOM>(state, xml);
	dom->tags_prot = make_xmlChars("applet", "area", "base", "cite", "code", "frame", "frameset", "link", "meta", "nowiki", "object", "pre", "ref", "samp", "script", "style", "syntaxhighlight", "template", "var");
	dom->tags_prot_inline = make_xmlChars("br");
	dom->tags_raw = make_xmlChars("script", "style");
	dom->tags_inline = make_xmlChars("a", "abbr", "acronym", "address", "b", "bdo", "big", "del", "em", "font", "i", "ins", "q", "s", "small", "span", "strike", "strong", "sub", "sup", "tt", "u");
	dom->tag_attrs = make_xmlChars("alt", "caption", "title");
	dom->save_spaces();

	auto styled = dom->to_styles(true);
	file_save(tmpdir / "styled.xml", x2s(styled));
	dom->xml = xmlReadMemory(reinterpret_cast<const char*>(styled.data()), static_cast<int>(styled.size()), "styled.xml", "UTF-8", XML_PARSE_RECOVER | XML_PARSE_NONET);
	if (xml == nullptr) {
		throw std::runtime_error(concat("Could not parse styled XML: ", xmlLastError.message));
	}

	return dom;
}

}
