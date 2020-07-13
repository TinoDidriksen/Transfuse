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
#ifndef e5bd51be_DOM_HPP__
#define e5bd51be_DOM_HPP__

#include "state.hpp"
#include "string_view.hpp"
#include "xml.hpp"
#include "stream.hpp"
#include <unicode/utext.h>
#include <unicode/regex.h>
#include <libxml/tree.h>
#include <array>
#include <deque>

namespace Transfuse {

struct DOM {
	State& state;
	xmlDocPtr xml = nullptr;

	using tmp_xs_t = std::array<xmlString, 6>;
	std::deque<tmp_xs_t> tmp_xss;
	tmp_xs_t* tmp_xs = nullptr;
	size_t blocks = 0;
	std::unique_ptr<StreamBase> stream;

	UText tmp_ut = UTEXT_INITIALIZER;
	UErrorCode status = U_ZERO_ERROR;
	icu::RegexMatcher rx_space_only;
	icu::RegexMatcher rx_blank_only;
	icu::RegexMatcher rx_blank_head;
	icu::RegexMatcher rx_blank_tail;
	icu::RegexMatcher rx_any_alnum;

	xmlChars tags_prot; // Protected tags
	xmlChars tags_prot_inline; // Protected inline tags
	xmlChars tags_raw; // Tags with raw CDATA contents that should not be XML-mangled
	xmlChars tags_inline; // Inline tags
	xmlChars tags_parents_allow; // If set, only extract children of these tags
	xmlChars tags_parents_direct; // Used for TTX <df>?
	xmlChars tag_attrs; // Attributes that should also be extracted

	DOM(State&, xmlDocPtr);
	~DOM();

	void save_spaces(xmlNodePtr, size_t);
	void save_spaces() {
		save_spaces(reinterpret_cast<xmlNodePtr>(xml), 0);
	}

	bool is_space(xmlChar_view);
	bool is_only_child(xmlNodePtr);
	bool has_block_child(xmlNodePtr);

	void protect_to_styles(xmlString&);
	void to_styles(xmlString&, xmlNodePtr, size_t, bool protect = false);
	xmlString to_styles(bool prefix = false) {
		xmlString rv;
		if (prefix) {
			rv.append(XC("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"));
		}
		state.begin();
		to_styles(rv, reinterpret_cast<xmlNodePtr>(xml), 0);
		protect_to_styles(rv);
		state.commit();
		return rv;
	}

	void extract_blocks(xmlString&, xmlNodePtr, size_t, bool txt = false);
	xmlString extract_blocks() {
		xmlString rv;
		stream->stream_header(rv, state.tmpdir);
		blocks = 0;
		extract_blocks(rv, reinterpret_cast<xmlNodePtr>(xml), 0);
		return rv;
	}
};

}

#endif
