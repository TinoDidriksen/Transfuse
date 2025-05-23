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

#include "shared.hpp"
#include <unicode/ucsdet.h>
#include <unicode/ucnv.h>
#include <unicode/utf8.h>
#include <libxml/tree.h>
#include <stdexcept>
using namespace icu;

namespace Transfuse {

constexpr std::string_view UTF8_BOM("\xef\xbb\xbf");
constexpr std::string_view UTF32LE_BOM("\xff\xfe\x00\x00", 4);
constexpr std::string_view UTF32BE_BOM("\x00\x00\xfe\xff", 4);
constexpr std::string_view UTF16LE_BOM("\xff\xfe");
constexpr std::string_view UTF16BE_BOM("\xfe\xff");

inline bool is_utf8(std::string_view data) {
	UChar32 c = 0;
	auto raw = reinterpret_cast<const uint8_t*>(data.data());
	int32_t i = 0;
	auto sz = SI32(data.size());
	while (c >= 0 && i<sz) {
		U8_NEXT(raw, i, sz, c);
		if (c < 0) {
			return false;
		}
	}
	return true;
}

std::string detect_encoding(std::string_view data) {
	std::string rv;

	if (data.substr(0, 3) == UTF8_BOM) {
		rv = "UTF-8";
	}
	else if (data.substr(0, 4) == UTF32LE_BOM) {
		rv = "UTF-32LE";
	}
	else if (data.substr(0, 4) == UTF32BE_BOM) {
		rv = "UTF-32BE";
	}
	else if (data.substr(0, 2) == UTF16LE_BOM) {
		rv = "UTF-16LE";
	}
	else if (data.substr(0, 2) == UTF16BE_BOM) {
		rv = "UTF-16BE";
	}
	else if (is_utf8(data)) {
		rv = "UTF-8";
	}
	else {
		UErrorCode status = U_ZERO_ERROR;
		auto det = ucsdet_open(&status);
		if (U_FAILURE(status)) {
			throw std::runtime_error(concat("Could not create charset detector: ", u_errorName(status)));
		}

		ucsdet_setText(det, data.data(), SI32(data.size()), &status);
		if (U_FAILURE(status)) {
			throw std::runtime_error(concat("Could not fill charset detector: ", u_errorName(status)));
		}

		auto cset = ucsdet_detect(det, &status);
		if (U_FAILURE(status)) {
			throw std::runtime_error(concat("Could not detect charset: ", u_errorName(status)));
		}

		rv = ucsdet_getName(cset, &status);
		if (U_FAILURE(status)) {
			throw std::runtime_error(concat("Could not name charset: ", u_errorName(status)));
		}

		ucsdet_close(det);
	}

	return rv;
}

UnicodeString to_ustring(std::string_view data, std::string_view enc) {
	UErrorCode status = U_ZERO_ERROR;
	auto conv = ucnv_open(enc.data(), &status);
	if (U_FAILURE(status)) {
		throw std::runtime_error(concat("Could not create charset converter: ", u_errorName(status)));
	}

	status = U_ZERO_ERROR;
	UnicodeString rv(data.data(), SI32(data.size()), conv, status);
	if (U_FAILURE(status)) {
		throw std::runtime_error(concat("Could not convert to UnicodeString: ", u_errorName(status)));
	}

	return rv;
}

void hook_inject(Settings* settings, std::string_view fn) {
	if (!settings->hook_inject.empty()) {
		std::string cmd{ settings->hook_inject };
		cmd += ' ';
		cmd += '"';
		cmd += fs::current_path().string();
		cmd += '/';
		cmd += fn;
		cmd += '"';
		system(cmd.c_str());
	}
}

}
