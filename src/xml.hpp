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
#ifndef e5bd51be_XML_HPP__
#define e5bd51be_XML_HPP__

#include "string_view.hpp"
#include <libxml/xmlstring.h>
#include <string>
#include <set>
#include <algorithm>
#include <cctype>

namespace Transfuse {

using xmlChar_view = ::std::basic_string_view<xmlChar>;
using xmlChars = std::set<xmlChar_view>;
using xmlString = std::basic_string<xmlChar>;

inline const xmlChar* XC(const char* c) {
	return reinterpret_cast<const xmlChar*>(c);
}

inline xmlChar_view operator "" _xcv(const char* str, std::size_t len) {
	return xmlChar_view(reinterpret_cast<const xmlChar*>(str), len);
}

namespace details {
	inline void _make_xmlChars(xmlChars&) {
	}

	template<typename... Args>
	inline void _make_xmlChars(xmlChars& rv, std::string_view value, Args... args) {
		xmlChar_view v(reinterpret_cast<const xmlChar*>(value.data()), value.size());
		rv.insert(v);
		_make_xmlChars(rv, args...);
	}
}

template<typename... Args>
inline xmlChars make_xmlChars(std::string_view value, Args... args) {
	xmlChar_view v(reinterpret_cast<const xmlChar*>(value.data()), value.size());
	xmlChars rv{ v };
	details::_make_xmlChars(rv, args...);
	return rv;
}

inline xmlString& to_lower(xmlString& str) {
	std::transform(str.begin(), str.end(), str.begin(), [](xmlChar c) { return static_cast<xmlChar>(tolower(c)); });
	return str;
}

inline xmlString& to_lower(xmlString& str, const xmlChar* xc) {
	str.assign(xc ? xc : XC(""));
	to_lower(str);
	return str;
}

inline std::string_view x2s(xmlChar_view xv) {
	return std::string_view(reinterpret_cast<const char*>(xv.data()), xv.size());
}

inline xmlChar_view s2x(std::string_view sv) {
	return xmlChar_view(reinterpret_cast<const xmlChar*>(sv.data()), sv.size());
}

}

#endif
