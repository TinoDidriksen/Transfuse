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

#include "state.hpp"
#include "filesystem.hpp"
#include "stream.hpp"
#include "dom.hpp"
#include "formats.hpp"
#include <unicode/regex.h>
#include <unicode/utext.h>
#include <iostream>
#include <string>
#include <array>
#include <stdexcept>
using namespace icu;

namespace Transfuse {

std::pair<fs::path,std::string> inject(fs::path tmpdir, std::istream& in, Stream stream) {
	std::ios::sync_with_stdio(false);
	in.tie(nullptr);

	std::array<char, 4096> inbuf{};
	in.rdbuf()->pubsetbuf(inbuf.data(), inbuf.size());
	in.exceptions(std::ios::badbit);

	std::unique_ptr<StreamBase> sformat;

	std::string buffer;
	std::getline(in, buffer);

	if (stream == Stream::detect) {
		if (buffer.find("[transfuse:") != std::string::npos) {
			sformat.reset(new ApertiumStream);
		}
		else if (buffer.find("<STREAMCMD:TRANSFUSE:") != std::string::npos) {
			sformat.reset(new VISLStream);
		}
		else {
			throw std::runtime_error("Could not detect input stream format");
		}
	}
	else if (stream == Stream::apertium) {
		sformat.reset(new ApertiumStream);
	}
	else {
		sformat.reset(new VISLStream);
	}

	if (tmpdir.empty()) {
		tmpdir = sformat->get_tmpdir(buffer);
	}

	if (tmpdir.empty()) {
		throw std::runtime_error("Could not read state folder path from Transfuse stream header");
	}
	if (!fs::exists(tmpdir)) {
		throw std::runtime_error(concat("State folder did not exist: ", tmpdir.string()));
	}

	fs::current_path(tmpdir);

	if (!fs::exists("original") || !fs::exists("content.xml") || !fs::exists("state.sqlite3")) {
		throw std::runtime_error(concat("Given folder did not have expected state files: ", tmpdir.string()));
	}

	auto content = file_load("content.xml");
	std::string tmp_b;
	std::string tmp_e;

	// Read all blocks from the input stream and put them back in the document
	std::string tmp;
	std::string bid;
	while (sformat->get_block(in, buffer, bid)) {
		if (bid.empty()) {
			continue;
		}
		trim(buffer);
		tmp_b.clear();
		append_xml(tmp_b, buffer);
		buffer.swap(tmp_b);

		tmp_b = TFB_OPEN_B;
		tmp_b += bid;
		tmp_b += TFB_OPEN_E;

		tmp_e = TFB_CLOSE_B;
		tmp_e += bid;
		tmp_e += TFB_CLOSE_E;

		tmp.clear();
		size_t l = 0;
		auto b = content.find(tmp_b);
		auto e = content.find(tmp_e, b + tmp_b.size());
		while (b != std::string::npos && e != std::string::npos) {
			tmp.append(content.begin() + PD(l), content.begin() + PD(b));
			tmp += buffer;
			l = e + tmp_e.size();
			b = content.find(tmp_b, l);
			e = content.find(tmp_e, b + tmp_b.size());
		}
		if (l == 0) {
			std::cerr << "Block " << bid << " did not exist in this document." << std::endl;
		}
		tmp.append(content.begin() + PD(l), content.end());
		content.swap(tmp);
	}

	// ToDo: Remove and warn about remaining block IDs

	cleanup_styles(content);

	State state(tmpdir, true);

	UText tmp_ut = UTEXT_INITIALIZER;
	UErrorCode status = U_ZERO_ERROR;
	bool did = true;

	while (did) {
		did = false;
		tmp.resize(0);
		tmp.reserve(content.size());
		RegexMatcher rx_spc_prefix(R"X(\ue011([^\ue012]+?):([^\ue012:]+)\ue012([^\ue011-\ue013]*)\ue013)X", 0, status);
		utext_openUTF8(tmp_ut, content);

		rx_spc_prefix.reset(&tmp_ut);
		int32_t l = 0;
		while (rx_spc_prefix.find()) {
			auto mb = rx_spc_prefix.start(0, status);
			auto me = rx_spc_prefix.end(0, status);
			tmp.append(content.begin() + l, content.begin() + mb);
			l = me;
			did = true;

			auto tb = rx_spc_prefix.start(1, status);
			auto te = rx_spc_prefix.end(1, status);
			tmp_b.assign(content.begin() + tb, content.begin() + te);

			auto hb = rx_spc_prefix.start(2, status);
			auto he = rx_spc_prefix.end(2, status);
			tmp_e.assign(content.begin() + hb, content.begin() + he);

			auto body = state.style(tmp_b, tmp_e);
			if (body.first.empty() && body.second.empty()) {
				std::cerr << "Inline tag " << tmp_b << ":" << tmp_e << " did not exist in this document." << std::endl;
			}
			tmp += body.first;
			auto bb = rx_spc_prefix.start(3, status);
			auto be = rx_spc_prefix.end(3, status);
			tmp.append(content.begin() + bb, content.begin() + be);
			tmp += body.second;
		}
		tmp.append(content.begin() + l, content.end());
		content.swap(tmp);
	}
	utext_close(&tmp_ut);

	auto xml = xmlReadMemory(reinterpret_cast<const char*>(content.data()), SI(content.size()), "content.xml", "UTF-8", XML_PARSE_RECOVER | XML_PARSE_NONET);
	if (xml == nullptr) {
		throw std::runtime_error(concat("Could not parse styled XML: ", xmlLastError.message));
	}

	auto dom = std::make_unique<DOM>(state, xml);
	dom->restore_spaces();

	std::string fname;
	auto format = state.format();

	if (format == "docx") {
		fname = inject_docx(*dom);
	}
	else if (format == "pptx") {
	}
	else if (format == "odt" || format == "odp") {
		fname = inject_odt(*dom);
	}
	else if (format == "html") {
		fname = inject_html(*dom);
	}
	else if (format == "html-fragment") {
		fname = inject_html_fragment(*dom);
	}
	else if (format == "text") {
		fname = inject_text(*dom);
	}

	return {tmpdir, fname};
}

}
