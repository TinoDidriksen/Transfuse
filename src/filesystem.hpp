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
#ifndef e5bd51be_FILESYSTEM_HPP_
#define e5bd51be_FILESYSTEM_HPP_

#include "string_view.hpp"

#if __cplusplus >= 201703L
	#include <filesystem>
#else
	#include <experimental/filesystem>
	namespace std {
		namespace filesystem {
			using namespace ::std::experimental::filesystem;
		}
	}
#endif

inline std::filesystem::path path(std::string_view sv) {
	std::filesystem::path rv(sv.begin(), sv.end());
	return rv;
}

namespace fs = ::std::filesystem;

#endif
