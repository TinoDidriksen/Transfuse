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
#ifndef e5bd51be_DOM_HPP_
#define e5bd51be_DOM_HPP_

#include "state.hpp"
#include "xml.hpp"
#include "stream.hpp"
#include <unicode/utext.h>
#include <unicode/regex.h>
#include <libxml/tree.h>
#include <string_view>
#include <array>
#include <deque>
#include <map>
#include <regex>

namespace Transfuse {

void cleanup_styles(State& state, std::string& str);
inline void cleanup_styles(State& state, xmlString& str) {
	return cleanup_styles(state, reinterpret_cast<std::string&>(str));
}

inline void append_xml(xmlString& str, xmlChar_view xc, bool nls = false) {
	for (auto c : xc) {
		if (c == '&') {
			str += "&amp;";
		}
		else if (c == '"') {
			str += "&quot;";
		}
		else if (c == '\'') {
			str += "&apos;";
		}
		else if (c == '<') {
			str += "&lt;";
		}
		else if (c == '>') {
			str += "&gt;";
		}
		else if (c == '\t' && nls) {
			str += "&#9;";
		}
		else if (c == '\n' && nls) {
			str += "&#10;";
		}
		else if (c == '\r' && nls) {
			str += "&#13;";
		}
		else {
			str += c;
		}
	}
}

inline void append_xml(std::string& str, std::string_view xc, bool nls = false) {
	return append_xml(reinterpret_cast<xmlString&>(str), s2x(xc), nls);
}

template<typename S, typename SV>
inline void assign_xml(S& str, SV sv, bool nls = false) {
	str.clear();
	append_xml(str, sv, nls);
}

inline void rx_replaceAll(const char* pattern, const char* repl, std::string& udata, std::string& tmp) {
	tmp.clear();
	std::regex rx(pattern);
	std::regex_replace(std::back_inserter(tmp), udata.begin(), udata.end(), rx, repl);
	std::swap(udata, tmp);
}

inline void rx_replaceAll(const char* pattern, const char* repl, icu::UnicodeString& udata, icu::UnicodeString& tmp) {
	UErrorCode status = U_ZERO_ERROR;
	icu::RegexMatcher regex(pattern, 0, status);
	if (U_FAILURE(status)) {
		throw status;
	}

	regex.reset(udata);
	tmp = regex.replaceAll(repl, status);
	if (U_FAILURE(status)) {
		throw status;
	}
	std::swap(udata, tmp);
}

inline void rx_replaceAll_expand_21(const char* pattern, icu::UnicodeString& udata, icu::UnicodeString& tmp) {
	UErrorCode status = U_ZERO_ERROR;
	icu::RegexMatcher regex(pattern, 0, status);
	if (U_FAILURE(status)) {
		throw status;
	}

	regex.reset(udata);
	tmp.remove();
	int32_t last = 0;
	while (regex.find()) {
		auto pb = regex.start(1, status);
		while (pb > 0 && udata.charAt(pb - 1) != '>') {
			--pb;
		}
		tmp.append(udata, last, pb - last);

		auto sb = regex.start(2, status);
		auto se = regex.end(2, status);
		tmp.append(udata, sb, se - sb);

		tmp.append(udata, pb, sb - pb);

		last = regex.end(status);
		if (U_FAILURE(status)) {
			throw status;
		}
	}
	tmp.append(udata, last, udata.length() - last);
	std::swap(udata, tmp);
}

struct DOM {
	State& state;
	std::unique_ptr<xmlDoc,decltype(&xmlFreeDoc)> xml;

	using tmp_xs_t = std::array<xmlString, 6>;
	std::deque<tmp_xs_t> tmp_xss;
	tmp_xs_t* tmp_xs = nullptr;
	std::string tmp_s;
	size_t blocks = 0;
	size_t unique = 0;
	std::unique_ptr<StreamBase> stream;

	UText tmp_ut = UTEXT_INITIALIZER;
	UErrorCode status = U_ZERO_ERROR;
	icu::RegexMatcher rx_space_only;
	icu::RegexMatcher rx_blank_only;
	icu::RegexMatcher rx_blank_head;
	icu::RegexMatcher rx_blank_tail;
	std::unique_ptr<icu::RegexMatcher> rx_any_content;

	std::map<std::string_view, xmlChars> tags;

	DOM(State&, xmlDocPtr);
	~DOM();

	void cmdline_tags();

	void save_spaces(xmlNodePtr, size_t);
	void save_spaces() {
		save_spaces(reinterpret_cast<xmlNodePtr>(xml.get()), 0);
	}

	void append_ltrim(xmlString&, xmlChar_view);
	void assign_ltrim(xmlString&, xmlChar_view);
	void assign_rtrim(xmlString&, xmlChar_view);

	void create_spaces(xmlNodePtr, size_t);
	void restore_spaces(xmlNodePtr, size_t);
	void restore_spaces() {
		restore_spaces(reinterpret_cast<xmlNodePtr>(xml.get()), 0);
		create_spaces(reinterpret_cast<xmlNodePtr>(xml.get()), 0);
	}

	bool is_space(xmlChar_view);
	bool is_only_child(xmlNodePtr);
	bool has_block_child(xmlNodePtr);

	void save_styles(xmlString&, xmlNodePtr, size_t, bool protect = false);
	xmlString save_styles(bool prefix = false) {
		xmlString rv;
		if (prefix) {
			rv += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
		}
		state.begin();
		save_styles(rv, reinterpret_cast<xmlNodePtr>(xml.get()), 0);
		stream->protect_to_styles(rv, state);
		state.commit();
		cleanup_styles(state, rv);
		return rv;
	}

	void extract_blocks(xmlString&, xmlNodePtr, size_t, bool txt = false, bool header = false);
	xmlString extract_blocks() {
		xmlString rv;
		stream->stream_header(rv, state.settings->tmpdir);
		blocks = 0;
		extract_blocks(rv, reinterpret_cast<xmlNodePtr>(xml.get()), 0);
		return rv;
	}
};

}

#endif
