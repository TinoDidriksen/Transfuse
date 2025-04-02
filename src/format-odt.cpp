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
#include <libxml/xmlsave.h>
#include <unicode/ustring.h>
#include <unicode/regex.h>
#include <zip.h>
#include <unordered_map>
using namespace icu;

namespace Transfuse {

struct ustring_hash {
	using hash_type = std::hash<ustring_view>;
	using is_transparent = void;
	size_t operator()(const UChar* str) const { return hash_type{}(str); }
	size_t operator()(ustring_view str) const { return hash_type{}(str); }
	size_t operator()(UnicodeString const& str) const { return hash_type{}(ustring_view(str.getBuffer(), str.length())); }
};

std::unique_ptr<DOM> extract_odt(State& state) {
	int e = 0;
	auto zip = zip_open("original", ZIP_RDONLY, &e);
	if (zip == nullptr) {
		throw std::runtime_error(concat("Could not open ODT/ODP file: ", std::to_string(e)));
	}

	zip_stat_t stat{};
	if (zip_stat(zip, "content.xml", 0, &stat) != 0) {
		throw std::runtime_error("ODT/ODP did not have content.xml");
	}
	if (stat.size == 0) {
		throw std::runtime_error("ODT/ODP content.xml was empty");
	}

	auto zf = zip_fopen_index(zip, stat.index, 0);
	if (zf == nullptr) {
		throw std::runtime_error("Could not open ODT/ODP content.xml");
	}

	std::string data(stat.size, 0);
	zip_fread(zf, &data[0], stat.size);
	zip_fclose(zf);

	zip_close(zip);

	// ToDo: Turn <text:tab> and <text:tab [^>]*> into \t?

	UnicodeString tmp;
	auto udata = UnicodeString::fromUTF8(data);
	udata.findAndReplace(" encoding=\"UTF-8\"", " encoding=\"UTF-16\"");

	// Wipe chaff that's not relevant when translated, or simply superfluous
	rx_replaceAll(R"X( fo:language="[^"]+")X", "", udata, tmp);
	rx_replaceAll(R"X( style:language-complex="[^"]+")X", "", udata, tmp);
	rx_replaceAll(R"X( style:language-asian="[^"]+")X", "", udata, tmp);
	rx_replaceAll(R"X( fo:country="[^"]+")X", "", udata, tmp);
	rx_replaceAll(R"X( style:country-complex="[^"]+")X", "", udata, tmp);
	rx_replaceAll(R"X( style:country-asian="[^"]+")X", "", udata, tmp);

	// Revision tracking information
	rx_replaceAll(R"X( officeooo:paragraph-rsid="[^"]+")X", "", udata, tmp);
	rx_replaceAll(R"X( officeooo:rsid="[^"]+")X", "", udata, tmp);

	udata.findAndReplace("<style:text-properties/>", "");

	UnicodeString normed = udata;
	UnicodeString rpl;
	std::unordered_map<UnicodeString, UnicodeString, ustring_hash> styles;
	UErrorCode status = U_ZERO_ERROR;
	RegexMatcher rx_styles(R"X((<style:style style:name=")([^"]+)(".+?</style:style>))X", 0, status);

	rx_styles.reset(udata);
	while (rx_styles.find()) {
		tmp.remove();

		auto b1 = rx_styles.start(1, status);
		auto e1 = rx_styles.end(1, status);
		tmp.append(udata, b1, e1 - b1);

		auto b3 = rx_styles.start(3, status);
		auto e3 = rx_styles.end(3, status);
		tmp.append(udata, b3, e3 - b3);

		// If the style, minus the unique name, is identical to an already seen style, replace it with the existing one
		auto it = styles.find(tmp);
		if (it != styles.end()) {
			auto b0 = rx_styles.start(0, status);
			auto e0 = rx_styles.end(0, status);
			tmp.setTo(udata, b0, e0 - b0);
			normed.findAndReplace(tmp, "");

			auto b2 = rx_styles.start(2, status);
			auto e2 = rx_styles.end(2, status);
			tmp.setTo(" text:style-name=\"");
			tmp.append(udata, b2, e2 - b2);
			tmp.append("\"");
			rpl.setTo(" text:style-name=\"");
			rpl.append(it->second);
			rpl.append("\"");
			normed.findAndReplace(tmp, rpl);
		}
		else {
			auto b2 = rx_styles.start(2, status);
			auto e2 = rx_styles.end(2, status);
			styles[tmp].setTo(udata, b2, e2 - b2);
		}
	}

	udata.swap(normed);

	auto xml = xmlReadMemory(reinterpret_cast<const char*>(udata.getTerminatedBuffer()), SI(SZ(udata.length()) * sizeof(UChar)), "content.xml", utf16_native, XML_PARSE_RECOVER | XML_PARSE_NONET);
	if (xml == nullptr) {
		throw std::runtime_error(concat("Could not parse content.xml: ", xmlLastError.message));
	}
	data.clear();
	data.shrink_to_fit();

	auto dom = std::make_unique<DOM>(state, xml);
	dom->tags_parents_allow = make_xmlChars("text:h", "text:p");
	dom->tags_prot_inline = make_xmlChars("text:line-break", "text:s");
	dom->tags_inline = make_xmlChars("text:a", "text:span");
	dom->save_spaces();

	auto styled = dom->save_styles(true);
	file_save("styled.xml", x2s(styled));
	dom->xml.reset(xmlReadMemory(reinterpret_cast<const char*>(styled.data()), SI(styled.size()), "styled.xml", "UTF-8", XML_PARSE_RECOVER | XML_PARSE_NONET));
	if (dom->xml == nullptr) {
		throw std::runtime_error(concat("Could not parse styled XML: ", xmlLastError.message));
	}

	return dom;
}

std::string inject_odt(DOM& dom) {
	auto cntx = xmlSaveToFilename("injected.xml", "UTF-8", 0);
	xmlSaveDoc(cntx, dom.xml.get());
	xmlSaveClose(cntx);

	hook_inject(dom.state.settings, "injected.xml");

	fs::copy("original", "injected.odt");

	int e = 0;
	auto zip = zip_open("injected.odt", 0, &e);
	if (zip == nullptr) {
		throw std::runtime_error(concat("Could not open ODT/ODP file: ", std::to_string(e)));
	}

	auto src = zip_source_file(zip, "injected.xml", 0, 0);
	if (src == nullptr) {
		throw std::runtime_error("Could not open injected.xml");
	}

	if (zip_file_add(zip, "content.xml", src, ZIP_FL_OVERWRITE) < 0) {
		throw std::runtime_error("Could not replace content.xml");
	}

	zip_close(zip);

	return "injected.odt";
}

}
