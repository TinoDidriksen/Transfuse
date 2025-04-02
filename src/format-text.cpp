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

std::unique_ptr<DOM> extract_text(State& state, bool by_line) {
	auto raw_data = file_load("original");
	auto enc = detect_encoding(raw_data);

	auto data = std::make_unique<UnicodeString>(to_ustring(raw_data, enc));
	data->findAndReplace("&", "&amp;");
	data->findAndReplace("<", "&lt;");
	data->findAndReplace(">", "&gt;");
	data->findAndReplace("\"", "&quot;");
	data->findAndReplace("'", "&apos;");

	UErrorCode status = U_ZERO_ERROR;
	RegexMatcher rx_multiline(UnicodeString::fromUTF8(R"X(\n[\s\p{Zs}]*(\n[\s\p{Zs}]*)+)X"), 0, status);
	rx_multiline.reset(*data);
	*data = rx_multiline.replaceAll(UnicodeString::fromUTF8("</p><p>"), status);

	if (by_line) {
		data->findAndReplace("\n", "</p><p>");
	}
	else {
		data->findAndReplace("\n", "<br>\n");
	}
	data->findAndReplace("</p><p>", "</p>\n<p>");

	data->insert(0, "<!DOCTYPE html>\n<html><head><meta charset=\"UTF-16\"></head><body><p>");
	data->append("</p></body></html>");

	return extract_html(state, std::move(data));
}

std::string inject_text(DOM& dom, bool by_line) {
	auto txt = file_load(inject_html(dom));

	auto e = txt.find("</p></body>");
	txt.erase(e);

	auto b = txt.find("<body><p>");
	txt.erase(0, b + 9);

	std::string tmp;
	replace_all("<p>", "", txt, tmp);
	replace_all("<br>", "", txt, tmp);
	if (by_line) {
		replace_all("</p>", "", txt, tmp);
	}
	else {
		replace_all("</p>", "\n", txt, tmp);
	}
	replace_all("&lt;", "<", txt, tmp);
	replace_all("&gt;", ">", txt, tmp);
	replace_all("&quot;", "\"", txt, tmp);
	replace_all("&apos;", "'", txt, tmp);
	replace_all("&amp;", "&", txt, tmp);

	file_save("injected.txt", txt);

	hook_inject(dom.state.settings, "injected.txt");

	return "injected.txt";
}

}
