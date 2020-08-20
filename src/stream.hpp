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
#include "string_view.hpp"
#include "xml.hpp"
#include "shared.hpp"
#include "state.hpp"
#include <unicode/utext.h>
#include <vector>
#include <string>
#include <fstream>

namespace Transfuse {

namespace Streams {
	const std::string_view detect{ "detect" };
	const std::string_view apertium{ "apertium" };
	const std::string_view visl{ "visl" };
}
using Stream = std::string_view;

struct StreamBase {
	virtual ~StreamBase() = default;

	// Output functions
	virtual void protect_to_styles(xmlString&, State&) = 0;
	virtual void stream_header(xmlString&, fs::path) = 0;
	virtual void block_open(xmlString&, xmlChar_view) = 0;
	virtual void block_body(xmlString&, xmlChar_view) = 0;
	virtual void block_close(xmlString&, xmlChar_view) = 0;

	// Input functions
	virtual fs::path get_tmpdir(std::string&) = 0;
	virtual std::istream& get_block(std::istream&, std::string&, std::string&) = 0;
};

struct ApertiumStream final : StreamBase {
	// Output functions
	void protect_to_styles(xmlString&, State&) final;
	void stream_header(xmlString&, fs::path) final;
	void block_open(xmlString&, xmlChar_view) final;
	void block_body(xmlString&, xmlChar_view) final;
	void block_close(xmlString&, xmlChar_view) final;

	// Input functions
	fs::path get_tmpdir(std::string&) final;
	std::istream& get_block(std::istream&, std::string&, std::string&) final;

private:
	std::vector<std::string> wbs;
	std::string wb;
	std::string unesc;
};

struct VISLStream final : StreamBase {
	// Output functions
	void protect_to_styles(xmlString&, State&) final;
	void stream_header(xmlString&, fs::path) final;
	void block_open(xmlString&, xmlChar_view) final;
	void block_body(xmlString&, xmlChar_view) final;
	void block_close(xmlString&, xmlChar_view) final;

	// Input functions
	fs::path get_tmpdir(std::string&) final;
	std::istream& get_block(std::istream&, std::string&, std::string&) final;

private:
	std::string buffer;
};

inline void utext_openUTF8(UText& ut, xmlChar_view xc) {
	UErrorCode status = U_ZERO_ERROR;
	utext_openUTF8(&ut, reinterpret_cast<const char*>(xc.data()), SI64(xc.size()), &status);
	if (U_FAILURE(status)) {
		throw std::runtime_error(concat("Could not open UText: ", u_errorName(status)));
	}
}

inline void utext_openUTF8(UText& ut, std::string_view xc) {
	UErrorCode status = U_ZERO_ERROR;
	utext_openUTF8(&ut, xc.data(), SI64(xc.size()), &status);
	if (U_FAILURE(status)) {
		throw std::runtime_error(concat("Could not open UText: ", u_errorName(status)));
	}
}

}

#endif
