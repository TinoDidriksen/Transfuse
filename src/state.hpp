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
#ifndef e5bd51be_STATE_HPP__
#define e5bd51be_STATE_HPP__

#include "string_view.hpp"
#include "filesystem.hpp"
#include "xml.hpp"
#include <string>
#include <memory>

namespace Transfuse {

struct State {
	fs::path tmpdir;
	bool opt_verbose = false;
	bool opt_debug = false;

	State(fs::path, bool ro = false);
	~State();

	void begin();
	void commit();

	void name(std::string_view);
	std::string_view name();

	void format(std::string_view);
	std::string_view format();

	void info(std::string_view, std::string_view);
	std::string info(std::string_view);

	xmlChar_view style(xmlChar_view, xmlChar_view, xmlChar_view);
	std::pair<std::string_view, std::string_view> style(std::string_view, std::string_view);

protected:
	struct impl;
	std::unique_ptr<impl> s;
};

}

#endif
