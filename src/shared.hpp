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
#ifndef e5bd51be_SHARED_HPP_
#define e5bd51be_SHARED_HPP_

#include "filesystem.hpp"
#include <unicode/unistr.h>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <iostream>

namespace Transfuse {

// If these are changed, be sure to search the codebase for equivalent \u escapes used in various regexes
#define TFI_HASH_SEP "\xee\x80\x90" /* \uE010 */
#define TFI_OPEN_B   "\xee\x80\x91" /* \uE011 */
#define TFI_OPEN_E   "\xee\x80\x92" /* \uE012 */
#define TFI_CLOSE    "\xee\x80\x93" /* \uE013 */
constexpr auto TFI_CLOSE_UC = static_cast<UChar>(u'\uE013');
#define XML_ENC_U8   "\xee\x80\x94" /* \uE014 */
constexpr auto XML_ENC_UC = static_cast<UChar>(u'\uE014');
#define TFB_OPEN_B   "\xee\x80\x95" /* \uE015 */
#define TFB_OPEN_E   "\xee\x80\x96" /* \uE016 */
#define TFB_CLOSE_B  "\xee\x80\x97" /* \uE017 */
#define TFB_CLOSE_E  "\xee\x80\x98" /* \uE018 */
#define TF_SENTINEL  "\xee\x80\x99" /* \uE019 */
#define TFP_OPEN     "\xee\x80\xa0" /* \uE020 */
#define TFP_CLOSE    "\xee\x80\xa1" /* \uE021 */
#define TFU_OPEN     "\xee\x80\xa2" /* \uE022 */
#define TFU_CLOSE    "\xee\x80\xa3" /* \uE023 */

#define TFP_STREAM_B "\xee\x80\xac" /* \uE02C */
#define TFP_STREAM_E "\xee\x80\xad" /* \uE02D */

#define TF_CURVED_PARAGRAPH "\xe2\x9d\xa1" /* \u2761 ❡ CURVED STEM PARAGRAPH SIGN ORNAMENT */

#if defined(ARCH_BIG_ENDIAN)
	const std::string_view utf16_bom{ "\xfe\xff" };
	const auto utf16_native = "UTF-16BE";
#else
	const std::string_view utf16_bom{ "\xff\xfe" };
	const auto utf16_native = "UTF-16LE";
#endif
using ustring_view = std::basic_string_view<UChar>;

// Integer casts to let -Wconversion know that we did actually think about what we're doing

template<typename T>
constexpr inline int SI(T t) {
	return static_cast<int>(t);
}

template<typename T>
constexpr inline int32_t SI32(T t) {
	return static_cast<int32_t>(t);
}

template<typename T>
constexpr inline int64_t SI64(T t) {
	return static_cast<int64_t>(t);
}

template<typename T>
constexpr inline uint64_t UI64(T t) {
	return static_cast<uint64_t>(t);
}

template<typename T>
constexpr inline size_t SZ(T t) {
	return static_cast<size_t>(t);
}

template<typename T>
constexpr inline ptrdiff_t PD(T t) {
	return static_cast<ptrdiff_t>(t);
}

template<typename T>
constexpr inline std::streamsize SS(T t) {
	return static_cast<std::streamsize>(t);
}

inline uint32_t to_little_endian(uint32_t in) {
#if defined(ARCH_BIG_ENDIAN)
	auto bytes = reinterpret_cast<uint8_t*>(&in);
	in = (bytes[3] << 24) | (bytes[2] << 16) | (bytes[1] << 8) | bytes[0];
#endif
	return in;
}

inline uint64_t to_little_endian(uint64_t in) {
#if defined(ARCH_BIG_ENDIAN)
	auto bytes = reinterpret_cast<uint8_t*>(&in);
	in = (UI64(bytes[7]) << 56ull) | (UI64(bytes[6]) << 48ull) | (UI64(bytes[5]) << 40ull) | (UI64(bytes[4]) << 32ull) | (UI64(bytes[3]) << 24ull) | (UI64(bytes[2]) << 16ull) | (UI64(bytes[1]) << 8ull) | UI64(bytes[0]);
#endif
	return in;
}

namespace details {
	inline void _concat(std::string&) {
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

inline std::string& to_lower(std::string& str) {
	std::transform(str.begin(), str.end(), str.begin(), [](char c) { return static_cast<char>(tolower(c)); });
	return str;
}

inline void replace_all(std::string from, std::string to, std::string& str, std::string& tmp) {
	tmp.clear();
	size_t l = 0;
	auto b = str.find(from);
	while (b != std::string::npos) {
		tmp.append(str.begin() + PD(l), str.begin() + PD(b));
		if (!to.empty()) {
			tmp += to;
		}
		l = b + from.size();
		b = str.find(from, l);
	}
	tmp.append(str.begin() + PD(l), str.end());
	str.swap(tmp);
}

template<typename Char>
inline bool is_space(Char c) {
	return (c == ' ' || c == '\t' || c == '\r' || c == '\n');
}

inline void remove_prefix(std::string& str, size_t n) {
	str.erase(0, n);
}
inline void remove_prefix(std::string_view& str, size_t n) {
	str.remove_prefix(n);
}
inline void remove_suffix(std::string& str, size_t n = 1) {
	str.erase(str.size() - n);
}
inline void remove_suffix(std::string_view& str, size_t n = 1) {
	str.remove_suffix(n);
}

inline void reduce_ws(std::string& str) {
	bool had_space = false;
	while (!str.empty() && is_space(str.back())) {
		if (str.back() == ' ') {
			had_space = true;
		}
		remove_suffix(str);
	}
	if (had_space) {
		str += ' ';
	}

	had_space = false;
	size_t h = 0;
	for (; h < str.size() && is_space(str[h]); ++h) {
		if (str[h] == ' ') {
			had_space = true;
		}
	}
	if (had_space) {
		remove_prefix(str, h - 1);
		str[0] = ' ';
	}
	else {
		remove_prefix(str, h);
	}
}

template<typename Str>
inline void trim(Str& str) {
	while (!str.empty() && is_space(str.back())) {
		remove_suffix(str);
	}
	size_t h = 0;
	for (; h < str.size() && is_space(str[h]); ++h) {
	}
	remove_prefix(str, h);
}

template<typename Str>
inline void trim_wb(Str& wb) {
	while (!wb.empty() && (wb.back() == ';' || is_space(wb.back()))) {
		remove_suffix(wb);
	}
	size_t h = 0;
	for (; h < wb.size() && (wb[h] == ';' || is_space(wb[h])); ++h) {
	}
	remove_prefix(wb, h);
}

inline std::string file_load(fs::path fn) {
	std::ifstream file(fn.string(), std::ios::binary);
	file.exceptions(std::ios::badbit | std::ios::failbit);
	file.seekg(0, std::istream::end);
	auto size = file.tellg();

	file.seekg(0, std::istream::beg);

	std::string rv(SZ(size), 0);
	file.read(&rv[0], size);

	return rv;
}

inline void file_save(fs::path fn, std::string_view data) {
	std::ofstream file(fn.string(), std::ios::binary);
	file.exceptions(std::ios::badbit | std::ios::failbit);
	file.write(data.data(), SS(data.size()));
}

inline void file_save(fs::path fn, ustring_view data) {
	std::ofstream file(fn.string(), std::ios::binary);
	file.exceptions(std::ios::badbit | std::ios::failbit);
	file.write(reinterpret_cast<const char*>(data.data()), SS(data.size() * sizeof(ustring_view::value_type)));
}

inline void file_save(fs::path fn, const icu::UnicodeString& data, bool bom = true) {
	std::ofstream file(fn.string(), std::ios::binary);
	file.exceptions(std::ios::badbit | std::ios::failbit);
	if (bom) {
		if (data[0] != 0xFEFF) {
			file.write(utf16_bom.data(), utf16_bom.size());
		}
		file.write(reinterpret_cast<const char*>(data.getBuffer()), SS(SZ(data.length()) * sizeof(UChar)));
	}
	else {
		if (data[0] == 0xFEFF) {
			file.write(reinterpret_cast<const char*>(data.getBuffer()) + sizeof(UChar), SS((SZ(data.length()) - 1) * sizeof(UChar)));
		}
		else {
			file.write(reinterpret_cast<const char*>(data.getBuffer()), SS(SZ(data.length()) * sizeof(UChar)));
		}
	}
}

std::string detect_encoding(std::string_view data);

icu::UnicodeString to_ustring(std::string_view data, std::string_view encoding);

namespace Streams {
	const std::string_view detect{ "detect" };
	const std::string_view apertium{ "apertium" };
	const std::string_view visl{ "visl" };
	const std::string_view cg{ "cg" };
}
using Stream = std::string_view;

namespace Strs {
	constexpr std::string_view tags_prot{ "tags-prot" }; // Protected tags
	constexpr std::string_view tags_prot_inline{ "tags-prot-inline" }; // Protected inline tags
	constexpr std::string_view tags_raw{ "tags-raw" }; // Tags with raw CDATA contents that should not be XML-mangled
	constexpr std::string_view tags_inline{ "tags-inline" }; // Inline tags
	constexpr std::string_view tags_semantic{ "tags-semantic" }; // Inline tags that should be emitted even if they're a full block
	constexpr std::string_view tags_unique{ "tags-unique" }; // Tags that must not be merged, even if they are inline and adjacent
	constexpr std::string_view tags_parents_allow{ "tags-parents-allow" }; // If set, only extract children of these tags
	constexpr std::string_view tags_parents_direct{ "tags-parents-direct" }; // Used for TTX <df>?
	constexpr std::string_view tag_attrs{ "tag-attrs" }; // Tags that should append ❡ (U+2761)
	constexpr std::string_view tags_headers{ "tags-headers" }; // Attributes that should also be extracted
	constexpr std::string_view attrs_headers{ "attrs-headers" }; // Attributes that should append ❡ (U+2761)
}
inline constexpr auto maybe_tags = { Strs::tags_prot, Strs::tags_prot_inline, Strs::tags_raw, Strs::tags_inline, Strs::tags_semantic, Strs::tags_unique, Strs::tags_parents_allow, Strs::tags_parents_direct, Strs::tag_attrs, Strs::tags_headers, Strs::attrs_headers };

struct Settings {
	std::string_view mode{ "clean" };
	std::string_view format{ "auto" };
	Stream stream{ Streams::detect };

	fs::path tmpdir;
	fs::path infile;
	std::istream* in = nullptr;
	std::unique_ptr<std::istream> _in;
	std::ostream* out = nullptr;
	std::unique_ptr<std::ostream> _out;

	bool opt_verbose = false;
	bool opt_debug = false;
	bool opt_keep = false;
	bool opt_no_keep = false;
	bool opt_mark_headers = false;
	bool opt_apertium_n = false;
	bool opt_inject_raw = false;
	bool opt_no_extend = false;

	std::string_view hook_inject;

	std::map<std::string_view, std::set<std::string_view>> tags;
};

void hook_inject(Settings* settings, std::string_view fn);

}

#endif
