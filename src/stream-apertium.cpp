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

#include "filesystem.hpp"
#include "xml.hpp"
#include "shared.hpp"
#include "stream.hpp"
#include <unicode/utext.h>
#include <unicode/regex.h>
#include <vector>
#include <string>
#include <memory>
using namespace icu;

namespace Transfuse {

// Output functions

static void escape_meta(xmlString& s, std::string_view xc) {
	for (auto c : xc) {
		// ToDo: Should only need to escape []: https://github.com/apertium/apertium/issues/79
		if (c == '^' || c == '$' || c == '[' || c == ']' || c == '{' || c == '}' || c == '/' || c == '\\') {
			s += '\\';
		}
		s += static_cast<xmlChar>(c);
	}
}
static void escape_meta(xmlString& s, xmlChar_view xc) {
	return escape_meta(s, std::string_view(reinterpret_cast<const char*>(xc.data()), xc.size()));
}

static void escape_body(xmlString& s, std::string_view xc) {
	for (size_t i = 0; i < xc.size(); ++i) {
		if (xc[i] == '^' || xc[i] == '$' || xc[i] == '[' || xc[i] == ']' || xc[i] == '{' || xc[i] == '}' || xc[i] == '/' || xc[i] == '\\' || xc[i] == '@' || xc[i] == '<' || xc[i] == '>') {
			s += '\\';
		}
		else if (xc[i] == '\xee' && xc[i + 1] == '\x80' && xc[i + 2] == '\x91') {
			s += "[[";
			i += 3;
			auto b = xc.find(TFI_OPEN_E, i);
			auto wbs = xc.substr(i, b - i);
			i = b + 2;
			b = 0;
			size_t e = 0;
			while (b < wbs.size()) {
				e = wbs.find(';', b);
				s += "t:";
				if (e == std::string_view::npos) {
					s.append(wbs.begin() + b, wbs.end());
				}
				else {
					s.append(wbs.begin() + b, wbs.begin() + e);
					s += ';';
				}
				b = std::max(e, e + 1);
			}
			s += "]]";
			continue;
		}
		else if (xc[i] == '\xee' && xc[i + 1] == '\x80' && xc[i + 2] == '\x93') {
			s += "[[/]]";
			i += 2;
			continue;
		}
		else if (xc[i] == '\xee' && xc[i + 1] == '\x80' && xc[i + 2] >= '\xa0' && xc[i + 2] <= '\xa1') {
			if (xc[i + 2] == '\xa0') {
				s += "[tf:";
			}
			else if (xc[i + 2] == '\xa1') {
				s += "]";
			}
			i += 2;
			continue;
		}
		s += static_cast<xmlChar>(xc[i]);
	}
}
static void escape_body(xmlString& s, xmlChar_view xc) {
	return escape_body(s, std::string_view(reinterpret_cast<const char*>(xc.data()), xc.size()));
}

// Stores the protected content as a style, but leaves the markers for later superblank treatment
void ApertiumStream::protect_to_styles(xmlString& styled, State& state) {
	UText tmp_ut = UTEXT_INITIALIZER;
	UErrorCode status = U_ZERO_ERROR;

	// Merge protected regions if they only have whitespace between them
	auto rx_prots = std::make_unique<RegexMatcher>(R"X(\uE021([\s\r\n\p{Z}]*)\uE020)X", 0, status);

	utext_openUTF8(tmp_ut, styled);
	rx_prots->reset(&tmp_ut);

	xmlString ns;
	ns.reserve(styled.size());

	int32_t last = 0;
	while (rx_prots->find()) {
		auto b = rx_prots->start(status);
		ns.append(styled.begin() + last, styled.begin() + b);
		auto b1 = rx_prots->start(1, status);
		auto e1 = rx_prots->end(1, status);
		ns.append(styled.begin() + b1, styled.begin() + e1);
		last = rx_prots->end(status);
	}
	ns.append(styled.begin() + last, styled.end());

	styled.swap(ns);

	// Find all protected regions and store their contents
	rx_prots = std::make_unique<RegexMatcher>(R"X(\uE020(.*?)\uE021)X", UREGEX_DOTALL, status);
	RegexMatcher rx_block_start(R"X(>[\s\p{Zs}]*$)X", 0, status);
	RegexMatcher rx_block_end(R"X(^[\s\p{Zs}]*<)X", 0, status);

	utext_openUTF8(tmp_ut, styled);
	rx_prots->reset(&tmp_ut);

	ns.resize(0);
	ns.reserve(styled.size());
	xmlString tmp;

	UText tmp_pfx = UTEXT_INITIALIZER;
	UText tmp_sfx = UTEXT_INITIALIZER;
	last = 0;
	while (rx_prots->find(last, status)) {
		auto b = rx_prots->start(status);
		ns.append(styled.begin() + last, styled.begin() + b);

		auto b1 = rx_prots->start(1, status);
		auto e1 = rx_prots->end(1, status);
		tmp.assign(styled.begin() + b1, styled.begin() + e1);
		last = rx_prots->end(status);

		utext_openUTF8(tmp_pfx, ns);
		utext_openUTF8(tmp_sfx, xmlChar_view(styled).substr(SZ(last)));

		rx_block_start.reset(&tmp_pfx);
		if (rx_block_start.find()) {
			// If we are at the beginning of a block tag, just leave the protected inline as-is
			ns += tmp;
			continue;
		}

		rx_block_end.reset(&tmp_sfx);
		if (rx_block_end.find()) {
			// If we are at the end of a block tag, just leave the protected inline as-is
			ns += tmp;
			continue;
		}

		auto hash = state.style(XC("P"), tmp, XC(""));
		ns += TFP_OPEN;
		ns += "P:";
		ns += hash;
		ns += TFP_CLOSE;
	}

	ns.append(styled.begin() + last, styled.end());
	styled.swap(ns);
}

void ApertiumStream::stream_header(xmlString& s, fs::path tmpdir) {
	s += "[transfuse:";
	escape_meta(s, tmpdir.string());
	s += "]\n";
	s.push_back('\0');
}

void ApertiumStream::block_open(xmlString& s, xmlChar_view xc) {
	s += "\n[tf-block:";
	escape_meta(s, xc);
	s += "]\n\n";
}

void ApertiumStream::block_body(xmlString& s, xmlChar_view xc) {
	escape_body(s, xc);
}

void ApertiumStream::block_close(xmlString& s, xmlChar_view) {
	s += ".[]\n";
	s.push_back('\0');
}

// Input functions

fs::path ApertiumStream::get_tmpdir(std::string& line) {
	std::string tmp;
	for (size_t i = 0; i < line.size(); ++i) {
		if (line[i] == '\\') {
			++i;
		}
		tmp += line[i];
	}
	auto b = tmp.find("[transfuse:");
	auto e = tmp.find("]", b);
	if (b != std::string::npos && e != std::string::npos) {
		return fs::path(tmp.begin() + PD(b) + 11, tmp.begin() + PD(e));
	}
	return {};
}

std::istream& ApertiumStream::get_block(std::istream& in, std::string& str, std::string& block_id) {
	str.clear();
	block_id.clear();

	if (!in || in.peek() == std::ios::traits_type::eof()) {
		return in;
	}

	wbs.clear();
	unesc.clear();

	bool in_blank = false;
	bool in_wblank = false;

	char c = 0;
	int p = 0;
	while (in.get(c)) {
		if (c == '\\' && (p = in.peek()) != 0 && p != std::ios::traits_type::eof()) {
			auto n = static_cast<char>(in.get());
			if (in_blank) {
				unesc += n;
			}
			else {
				str += n;
			}
			continue;
		}

		if (c == '\0') {
			break;
		}

		if (c == '[') {
			if (in_blank) {
				in_wblank = true;
			}
			in_blank = true;
		}
		else if (in_wblank && c == ']') {
			// Do nothing
		}
		else if (in_blank && c == ']') {
			// Do nothing
		}

		if (in_blank) {
			unesc += c;
		}
		else {
			str += c;
		}

		if (in_wblank && c == ']') {
			in_wblank = false;
		}
		else if (in_blank && c == ']') {
			in_blank = false;
			if (unesc[0] == '[' && unesc[1] == '[' && unesc[2] == '/' && unesc[3] == ']' && unesc[4] == ']') {
				if (!wbs.empty()) {
					str += TFI_CLOSE;
				}
			}
			else if (unesc[0] == '[' && unesc[1] == '[') {
				wbs.clear();
				wb.assign(unesc.begin() + 2, unesc.end() - 2);
				size_t b = 0;
				while (b < wb.size()) {
					size_t e = wb.find(';', b);
					unesc.assign(wb, b, e - b);
					trim_wb(unesc);
					// Deduplicate, and discard non-markup data
					if (unesc[0] == 't' && unesc[1] == ':' && std::find(wbs.begin(), wbs.end(), unesc) == wbs.end()) {
						unesc.erase(0, 2);
						wbs.push_back(unesc);
					}
					b = std::max(e, e + 1);
				}
				if (!wbs.empty()) {
					str += TFI_OPEN_B;
					for (auto& t : wbs) {
						str += t;
						str += ";";
					}
					str += TFI_OPEN_E;
				}
			}
			else {
				auto bb = unesc.find("[tf-block:");
				auto eb = unesc.find("]", bb);
				auto bp = unesc.find("[tf:");
				auto ep = unesc.find("]", bp);
				if (bb != std::string::npos && eb != std::string::npos) {
					block_id.assign(unesc.begin() + PD(bb) + 10, unesc.begin() + PD(eb));
				}
				else if (bp != std::string::npos && ep != std::string::npos) {
					str += TFP_OPEN;
					str.append(unesc.begin() + 4, unesc.end() - 1);
					str += TFP_CLOSE;
				}
				else if (unesc.compare("[]") == 0) {
					if (str.back() == '.') {
						str.pop_back();
					}
				}
				else {
					str.append(unesc.begin() + 1, unesc.end() - 1);
				}
			}
			unesc.clear();
		}
	}

	return in;
}

}
