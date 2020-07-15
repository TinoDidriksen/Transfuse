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
#include <string>
#include <fstream>

namespace Transfuse {

enum class Stream {
	detect,
	apertium,
	visl,
};

struct StreamBase {
	virtual ~StreamBase() = default;

	// Output functions
	virtual void stream_header(xmlString&, fs::path) = 0;
	virtual void block_open(xmlString&, xmlChar_view) = 0;
	virtual void block_body(xmlString&, xmlChar_view) = 0;
	virtual void block_close(xmlString&, xmlChar_view) = 0;

	// Input functions
	virtual fs::path get_tmpdir(std::string&) = 0;
	virtual std::istream& get_block(std::istream&, std::string&, std::string&) = 0;
};

}

#include "stream-apertium.hpp"
#include "stream-visl.hpp"

#endif
