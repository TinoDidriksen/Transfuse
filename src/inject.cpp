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

std::pair<fs::path,std::string> inject(Settings& settings) {
	fs::path& tmpdir = settings.tmpdir;
	std::istream& in = *settings.in;
	Stream& stream = settings.stream;

	std::ios::sync_with_stdio(false);
	in.tie(nullptr);

	std::array<char, 4096> inbuf{};
	in.rdbuf()->pubsetbuf(inbuf.data(), inbuf.size());
	in.exceptions(std::ios::badbit);

	std::unique_ptr<StreamBase> sformat;

	std::string buffer;
	std::getline(in, buffer);

	if (stream == Streams::detect) {
		if (buffer.find("[transfuse:") != std::string::npos) {
			sformat.reset(new ApertiumStream(&settings));
		}
		else if (buffer.find("<STREAMCMD:TRANSFUSE:") != std::string::npos) {
			sformat.reset(new VISLStream(&settings));
		}
		else {
			throw std::runtime_error("Could not detect input stream format");
		}
	}
	else if (stream == Streams::apertium) {
		sformat.reset(new ApertiumStream(&settings));
	}
	else if (stream == Streams::cg) {
		sformat.reset(new CGStream(&settings));
	}
	else {
		sformat.reset(new VISLStream(&settings));
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
		if (settings.opt_inject_raw) {
			tmp_b += buffer;
		}
		else if (stream != Streams::cg) {
			reduce_ws(buffer);
			assign_xml(tmp_b, buffer);
		}
		else {
			assign_xml(tmp_b, buffer, true);
		}
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

	// Remove remaining block open markers
	auto b = content.find(TFB_OPEN_B);
	while (b != std::string::npos) {
		auto e = content.find(TFB_OPEN_E, b);
		content.erase(content.begin() + PD(b), content.begin() + PD(e) + 3);
		b = content.find(TFB_OPEN_B);
	}

	// Remove remaining block close markers
	b = content.find(TFB_CLOSE_B);
	while (b != std::string::npos) {
		auto e = content.find(TFB_CLOSE_E, b);
		content.erase(content.begin() + PD(b), content.begin() + PD(e) + 3);
		b = content.find(TFB_CLOSE_B);
	}

	cleanup_styles(content);

	State state(&settings, true);

	UText tmp_ut = UTEXT_INITIALIZER;
	UErrorCode status = U_ZERO_ERROR;
	std::string tag_close;

	RegexMatcher rx_inlines(R"X(\ue011([^\ue012]+)\ue012([^\ue011-\ue013]*)\ue013)X", 0, status);
	RegexMatcher rx_prots(R"X(\ue020([^\ue021]+?):([^\ue021:]+)\ue021)X", 0, status);

	bool did = true;
	while (did) {
		did = false;

		// Turn inline tags back into original forms
		tmp.resize(0);
		tmp.reserve(content.size());
		utext_openUTF8(tmp_ut, content);

		rx_inlines.reset(&tmp_ut);
		int32_t last = 0;
		while (rx_inlines.find()) {
			auto mb = rx_inlines.start(0, status);
			auto me = rx_inlines.end(0, status);
			tmp.append(content.begin() + last, content.begin() + mb);
			last = me;
			did = true;

			auto tb = rx_inlines.start(1, status);
			auto te = rx_inlines.end(1, status);
			tmp_b.assign(content.begin() + tb, content.begin() + te);

			tag_close.clear();
			b = 0;
			while (b < tmp_b.size()) {
				auto e = tmp_b.find(';', b);
				tmp_e.assign(tmp_b, b, e - b);
				trim_wb(tmp_e);
				auto c = tmp_e.find(':');
				auto tag = std::string_view(&tmp_e[0], c);
				auto hash = std::string_view(&tmp_e[c + 1]);

				auto body = state.style(tag, hash);
				if (body.first.empty() && body.second.empty()) {
					std::cerr << "Inline tag " << tmp_b << ":" << tmp_e << " did not exist in this document." << std::endl;
				}
				tmp += body.first;
				tag_close.insert(tag_close.begin(), body.second.begin(), body.second.end());
				b = std::max(e, e + 1);
			}

			auto bb = rx_inlines.start(2, status);
			auto be = rx_inlines.end(2, status);
			tmp.append(content.begin() + bb, content.begin() + be);
			tmp += tag_close;
		}
		tmp.append(content.begin() + last, content.end());
		content.swap(tmp);

		// Turn protected-inlines back into original form
		tmp.resize(0);
		tmp.reserve(content.size());
		utext_openUTF8(tmp_ut, content);

		rx_prots.reset(&tmp_ut);
		last = 0;
		while (rx_prots.find()) {
			auto mb = rx_prots.start(0, status);
			auto me = rx_prots.end(0, status);
			tmp.append(content.begin() + last, content.begin() + mb);
			last = me;
			did = true;

			auto tb = rx_prots.start(1, status);
			auto te = rx_prots.end(1, status);
			tmp_b.assign(content.begin() + tb, content.begin() + te);

			auto hb = rx_prots.start(2, status);
			auto he = rx_prots.end(2, status);
			tmp_e.assign(content.begin() + hb, content.begin() + he);

			auto body = state.style(tmp_b, tmp_e);
			if (body.first.empty() && body.second.empty()) {
				std::cerr << "Protected inline tag " << tmp_b << ":" << tmp_e << " did not exist in this document." << std::endl;
			}
			tmp += body.first;
			tmp += body.second;
		}
		tmp.append(content.begin() + last, content.end());
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
		fname = inject_pptx(*dom);
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
	else if (format == "tei") {
		fname = inject_tei(*dom);
	}
	else if (format == "line") {
		fname = inject_text(*dom, true);
	}

	return {tmpdir, fname};
}

}
