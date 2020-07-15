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
#ifndef e5bd51be_STREAM_HPP__
#define e5bd51be_STREAM_HPP__

#include "filesystem.hpp"
#include "xml.hpp"
#include "shared.hpp"

namespace Transfuse {

enum class Stream {
	apertium,
	visl,
};

struct StreamBase {
	virtual void stream_header(xmlString&, fs::path) = 0;
	virtual void block_open(xmlString&, xmlChar_view) = 0;
	virtual void block_body(xmlString&, xmlChar_view) = 0;
	virtual void block_close(xmlString&, xmlChar_view) = 0;
	virtual ~StreamBase() = default;
};

struct ApertiumStream : StreamBase {
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
					s.append(XC("[\\[t:")); // ToDo: Remove \\ when lt-proc can handle [[]]
				}
				else if (xc[i + 2] == '\x92') {
					s.append(XC("\\]]")); // ToDo: Remove \\ when lt-proc can handle [[]]
				}
				else if (xc[i + 2] == '\x93') {
					s.append(XC("[\\[\\/\\]]")); // ToDo: Remove \\ when lt-proc can handle [[]]
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
		s.append(XC("[transfuse:"));
		escape_meta(s, tmpdir.string());
		s.append(XC("]\n"));
		s.push_back('\0');
	}

	void block_open(xmlString& s, xmlChar_view xc) final {
		s.append(XC("\n[tf-block:"));
		escape_meta(s, xc);
		s.append(XC("]\n\n"));
	}

	void block_body(xmlString& s, xmlChar_view xc) final {
		escape_body(s, xc);
	}

	void block_close(xmlString& s, xmlChar_view) final {
		s.append(XC(".[]\n"));
		s.push_back('\0');
	}
};

struct VISLStream : StreamBase {
	static void escape_body(xmlString& s, std::string_view xc) {
		for (size_t i = 0; i < xc.size(); ++i) {
			if (xc[i] == '\xee' && xc[i + 1] == '\x80' && xc[i + 2] >= '\x91' && xc[i + 2] <= '\x93') {
				if (xc[i + 2] == '\x91') {
					s.append(XC("\n<STYLE:"));
				}
				else if (xc[i + 2] == '\x92') {
					s.append(XC(">\n"));
				}
				else if (xc[i + 2] == '\x93') {
					s.append(XC("\n</STYLE>\n"));
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
		s.append(XC("<STREAMCMD:TRANSFUSE:"));
		s.append(XC(tmpdir.string().c_str()));
		s.append(XC(">\n\n"));
	}

	void block_open(xmlString& s, xmlChar_view xc) final {
		s.append(XC("\n<s"));
		s.append(xc.data());
		s.append(XC(">\n"));
	}

	void block_body(xmlString& s, xmlChar_view xc) final {
		escape_body(s, xc);
	}

	void block_close(xmlString& s, xmlChar_view xc) final {
		s.append(XC("\n</s"));
		s.append(xc.data());
		s.append(XC(">\n\n"));
	}
};

}

#endif
