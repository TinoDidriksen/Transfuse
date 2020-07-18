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
#include <zip.h>

namespace Transfuse {

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

	auto xml = xmlReadMemory(data.c_str(), SI(data.size()), "content.xml", "UTF-8", XML_PARSE_RECOVER | XML_PARSE_NONET);
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
