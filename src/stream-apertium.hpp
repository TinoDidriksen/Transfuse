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

#pragma once
#ifndef e5bd51be_STREAM_APERTIUM_HPP__
#define e5bd51be_STREAM_APERTIUM_HPP__

#include "filesystem.hpp"
#include "xml.hpp"
#include "shared.hpp"
#include "stream.hpp"
#include <vector>
#include <string>

namespace Transfuse {

struct ApertiumStream final : StreamBase {
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
			if (xc[i] == '^' || xc[i] == '$' || xc[i] == '[' || xc[i] == ']' || xc[i] == '{' || xc[i] == '}' || xc[i] == '/' || xc[i] == '\\') {
				s += '\\';
			}
			else if (xc[i] == '\xee' && xc[i + 1] == '\x80' && xc[i + 2] >= '\x91' && xc[i + 2] <= '\x93') {
				if (xc[i + 2] == '\x91') {
					s += "[[t:";
				}
				else if (xc[i + 2] == '\x92') {
					s += "]]";
				}
				else if (xc[i + 2] == '\x93') {
					s += "[[/]]";
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

	void stream_header(xmlString& s, fs::path tmpdir) final {
		s += "[transfuse:";
		escape_meta(s, tmpdir.string());
		s += "]\n";
		s.push_back('\0');
	}

	void block_open(xmlString& s, xmlChar_view xc) final {
		s += "\n[tf-block:";
		escape_meta(s, xc);
		s += "]\n\n";
	}

	void block_body(xmlString& s, xmlChar_view xc) final {
		escape_body(s, xc);
	}

	void block_close(xmlString& s, xmlChar_view) final {
		s += "[]\n";
		s.push_back('\0');
	}

	// Input functions

	static void trim_wb(std::string& wb) {
		while (!wb.empty() && (wb.back() == ';' || wb.back() == ' ')) {
			wb.pop_back();
		}
		size_t h = 0;
		for (; h < wb.size() && (wb[h] == ';' || wb[h] == ' '); ++h) {
		}
		wb.erase(0, h);
	}

	fs::path get_tmpdir(std::string& line) final {
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

	std::istream& get_block(std::istream& in, std::string& str, std::string& block_id) final {
		str.clear();
		block_id.clear();

		wbs.clear();
		blank.clear();
		unesc.clear();

		bool in_blank = false;
		bool in_wblank = false;

		char c = 0;
		while (in.get(c)) {
			if (c == '\\' && in.peek()) {
				auto n = static_cast<char>(in.get());
				if (in_blank) {
					blank += c;
					blank += n;
					unesc += n;
				}
				else {
					str += c;
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
				blank += c;
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
					for (size_t i = 0; i < wbs.size(); ++i) {
						str += TFI_CLOSE;
					}
				}
				else if (unesc[0] == '[' && unesc[1] == '[') {
					wbs.clear();
					blank.assign(unesc.begin() + 2, unesc.end() - 2);
					size_t b = 0;
					while (b < blank.size()) {
						size_t e = blank.find(';', b);
						unesc.assign(blank, b, e - b);
						trim_wb(unesc);
						// Deduplicate, and discard non-markup data
						if (unesc[0] == 't' && unesc[1] == ':' && std::find(wbs.begin(), wbs.end(), unesc) == wbs.end()) {
							unesc.erase(0, 2);
							wbs.push_back(unesc);
						}
						b = std::max(e, e + 1);
					}
					for (auto& wb : wbs) {
						str += TFI_OPEN_B;
						str += wb;
						str += TFI_OPEN_E;
					}
				}
				else {
					auto b = unesc.find("[tf-block:");
					auto e = unesc.find("]", b);
					if (b != std::string::npos && e != std::string::npos) {
						block_id.assign(unesc.begin() + PD(b) + 10, unesc.begin() + PD(e));
					}
					else {
						str.append(unesc.begin() + 1, unesc.end() - 1);
					}
				}
				blank.clear();
				unesc.clear();
			}
		}

		return in;
	}

private:
	std::vector<std::string> wbs;
	std::string blank;
	std::string unesc;
};

}

#endif
