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
#ifndef e5bd51be_SHARED_HPP__
#define e5bd51be_SHARED_HPP__

#include "string_view.hpp"
#include "filesystem.hpp"
#include <unicode/unistr.h>
#include <string>
#include <fstream>
#include <algorithm>
#include <cctype>

namespace Transfuse {

#if defined(BIG_ENDIAN)
	const std::string_view utf16_bom{ "\xfe\xff" };
	const auto utf16_native = "UTF-16BE";
#else
	const std::string_view utf16_bom{ "\xff\xfe" };
	const auto utf16_native = "UTF-16LE";
#endif
using ustring_view = std::basic_string_view<UChar>;

namespace details {
	inline void _concat(std::string&) {
	}

	// ToDo: C++17 renders this function obsolete
	template<typename... Args>
	inline void _concat(std::string& msg, std::string_view t, Args... args) {
		msg.append(t.begin(), t.end());
		_concat(msg, args...);
	}

	template<typename T, typename... Args>
	inline void _concat(std::string& msg, const T& t, Args... args) {
		msg.append(t);
		_concat(msg, args...);
	}
}

template<typename T, typename... Args>
inline std::string concat(const T& value, Args... args) {
	std::string msg(value);
	details::_concat(msg, args...);
	return msg;
}

inline void to_lower(std::string& str) {
	std::transform(str.begin(), str.end(), str.begin(), [](char c) { return static_cast<char>(tolower(c)); });
}

inline std::string file_load(fs::path fn) {
	std::ifstream file(fn.string(), std::ios::binary);
	file.seekg(0, std::istream::end);
	std::size_t size(static_cast<size_t>(file.tellg()));

	file.seekg(0, std::istream::beg);

	std::string rv(size, 0);
	file.read(&rv[0], size);

	return rv;
}

inline void file_save(fs::path fn, std::string_view data) {
	std::ofstream file(fn.string(), std::ios::binary);
	file.write(data.data(), data.size());
}

inline void file_save(fs::path fn, ustring_view data) {
	std::ofstream file(fn.string(), std::ios::binary);
	file.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(ustring_view::value_type));
}

inline void file_save(fs::path fn, const icu::UnicodeString& data, bool bom = true) {
	std::ofstream file(fn.string(), std::ios::binary);
	if (bom) {
		file.write(utf16_bom.data(), utf16_bom.size());
	}
	file.write(reinterpret_cast<const char*>(data.getBuffer()), data.length() * sizeof(UChar));
}

std::string detect_encoding(std::string_view);

icu::UnicodeString to_ustring(std::string_view, std::string_view);
std::string to_utf8(std::string_view, std::string_view);

}

#endif
