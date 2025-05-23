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
#ifndef e5bd51be_STATE_HPP_
#define e5bd51be_STATE_HPP_

#include "filesystem.hpp"
#include "xml.hpp"
#include "shared.hpp"
#include <string>
#include <string_view>
#include <memory>

namespace Transfuse {

struct State {
	Settings* settings;

	State(Settings*, bool ro = false);
	~State();

	void begin();
	void commit();

	void name(std::string_view);
	std::string_view name();

	void format(std::string_view);
	std::string_view format();

	void stream(std::string_view);
	std::string_view stream();

	void info(std::string_view, std::string_view);
	std::string info(std::string_view);

	xmlChar_view style(xmlChar_view name, xmlChar_view otag, xmlChar_view ctag, std::string_view flags = "");
	std::string_view style(std::string_view name, std::string_view otag, std::string_view ctag, std::string_view flags = "") {
		return XV2SV(style(XCV(name), XCV(otag), XCV(ctag), flags));
	}
	std::tuple<std::string_view, std::string_view, std::string_view> style(std::string_view tag, std::string_view hash);

protected:
	struct impl;
	std::unique_ptr<impl> s;
};

}

#endif
