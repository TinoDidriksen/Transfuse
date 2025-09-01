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

	auto dom = std::make_unique<DOM>(state, xml);
	dom->tags[Strs::tags_parents_allow] = make_xmlChars("ab", "floatingtext", "p");
	dom->tags[Strs::tags_prot] = make_xmlChars("binaryobject", "figdesc", "teiheader");
	dom->tags[Strs::tags_prot_inline] = make_xmlChars("gap", "lb", "space");
	dom->tags[Strs::tags_inline] = make_xmlChars("ref", "seg");
	dom->tags[Strs::tags_semantic] = make_xmlChars("date", "persname", "placename", "time");
	dom->tags[Strs::tags_unique] = make_xmlChars("gap", "lb", "ref", "seg");
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
