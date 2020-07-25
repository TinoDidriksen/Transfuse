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
#ifndef e5bd51be_STREAM_VISL_HPP__
#define e5bd51be_STREAM_VISL_HPP__

#include "filesystem.hpp"
#include "xml.hpp"
#include "shared.hpp"
#include "stream.hpp"

namespace Transfuse {

struct VISLStream final : StreamBase {
	// Output functions

	static void escape_body(xmlString& s, std::string_view xc) {
		for (size_t i = 0; i < xc.size(); ++i) {
			if (xc[i] == '\xee' && xc[i + 1] == '\x80' && xc[i + 2] >= '\x91' && xc[i + 2] <= '\x93') {
				if (xc[i + 2] == '\x91') {
					s += "\n<STYLE:";
				}
				else if (xc[i + 2] == '\x92') {
					s += ">\n";
				}
				else if (xc[i + 2] == '\x93') {
					s += "\n</STYLE>\n";
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
		s += "<STREAMCMD:TRANSFUSE:";
		s += tmpdir.string();
		s += ">\n\n";
	}

	void block_open(xmlString& s, xmlChar_view xc) final {
		s += "\n<s";
		s += xc;
		s += ">\n";
	}

	void block_body(xmlString& s, xmlChar_view xc) final {
		escape_body(s, xc);
	}

	void block_close(xmlString& s, xmlChar_view xc) final {
		s += "\n</s";
		s += xc;
		s += ">\n\n";
	}

	// Input functions

	fs::path get_tmpdir(std::string& line) final {
		auto b = line.find("<STREAMCMD:TRANSFUSE:");
		auto e = line.find(">", b);
		if (b != std::string::npos && e != std::string::npos) {
			return fs::path(line.begin() + PD(b) + 21, line.begin() + PD(e));
		}
		return {};
	}

	std::istream& get_block(std::istream& in, std::string& str, std::string& block_id) final {
		str.clear();
		block_id.clear();
		while (std::getline(in, buffer)) {
			str += buffer;
			if (buffer.find("\n</s") == 0) {
				break;
			}
		}
		return in;
	}

private:
	std::string buffer;
};

}

#endif
