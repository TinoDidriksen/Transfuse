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
#include "formats.hpp"
using namespace icu;

namespace Transfuse {

std::unique_ptr<DOM> extract_html_fragment(State& state) {
	auto raw_data = file_load("original");
	auto enc = detect_encoding(raw_data);

	auto data = std::make_unique<UnicodeString>(to_ustring(raw_data, enc));

	data->insert(0, "<!DOCTYPE html>\n<html><head><meta charset=\"UTF-16\"></head><body>");
	data->append("</body></html>");

	return extract_html(state, std::move(data));
}

std::string inject_html_fragment(DOM& dom) {
	auto fragment = file_load(inject_html(dom));

	auto e = fragment.find("</body>");
	fragment.erase(e);

	auto b = fragment.find("<body>");
	fragment.erase(0, b + 6);

	file_save("injected.fragment", fragment);

	hook_inject(dom.state.settings, "injected.fragment");

	return "injected.fragment";
}

}
