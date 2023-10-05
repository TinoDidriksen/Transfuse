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
#ifndef e5bd51be_BASE64_HPP_
#define e5bd51be_BASE64_HPP_

#include "shared.hpp"
#include <string>
#include <string_view>
#include <cstdint>

namespace Transfuse {

using bytes_view = std::basic_string_view<uint8_t>;

// Non-standard base64-encoder meant for URL-safe outputs. Doesn't pad and uses -_ instead of +/
void base64_url(std::string&, bytes_view input);

inline std::string base64_url(std::string_view input) {
	std::string rv;
	auto r = reinterpret_cast<const uint8_t*>(input.data());
	base64_url(rv, bytes_view(r, input.size()));
	return rv;
}

inline void base64_url(std::string& str, uint32_t input) {
	input = to_little_endian(input);
	auto r = reinterpret_cast<const uint8_t*>(&input);
	return base64_url(str, bytes_view(r, sizeof(input)));
}

inline std::string base64_url(uint32_t input) {
	std::string rv;
	base64_url(rv, input);
	return rv;
}

inline void base64_url(std::string& str, uint64_t input) {
	input = to_little_endian(input);
	auto r = reinterpret_cast<const uint8_t*>(&input);
	return base64_url(str, bytes_view(r, sizeof(input)));
}

inline std::string base64_url(uint64_t input) {
	std::string rv;
	base64_url(rv, input);
	return rv;
}

}

#endif
