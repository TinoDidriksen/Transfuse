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
#ifndef e5bd51be_FORMATS_HPP_
#define e5bd51be_FORMATS_HPP_

#include "dom.hpp"

namespace Transfuse {

std::unique_ptr<DOM> extract_docx(State& state);
std::unique_ptr<DOM> extract_html(State& state, std::unique_ptr<icu::UnicodeString> data = {});
std::unique_ptr<DOM> extract_html_fragment(State& state);
std::unique_ptr<DOM> extract_odt(State& state);
std::unique_ptr<DOM> extract_pptx(State& state);
std::unique_ptr<DOM> extract_text(State& state);

std::string inject_docx(DOM&);
std::string inject_html(DOM&);
std::string inject_html_fragment(DOM&);
std::string inject_odt(DOM&);
std::string inject_pptx(DOM&);
std::string inject_text(DOM&);

}

#endif
