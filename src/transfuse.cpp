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

#include "options.hpp"
#include "base64.hpp"
#include <xxhash.h>
#include <iostream>

int main(int argc, char* argv[]) {
	using namespace Options;
	auto opts = make_options(
		O('h', "help", "shows this help"),
		O('?', "", "shows this help"),
		spacer(),
		O('f', "format", ARG_REQ, "input file format: txt, html, odt, ods, odp, docx, xlsx, pptx; defaults to auto"),
		O('t', "to",   ARG_REQ, "output stream format: apertium, visl; defaults to apertium"),
		spacer(),
		O(0, "url64", ARG_REQ, "base64-url encodes the passed value"),
		O(0, "hash64", ARG_REQ, "xxhash-base64-url encodes the passed value"),
		final()
	);
	argc = opts.parse(argc, argv);

	if (opts['h'] || opts['?']) {
		std::cout << opts.explain();
		return 0;
	}

	if (auto o = opts["url64"]) {
		std::cout << base64_url(o->value) << std::endl;
		return 0;
	}

	if (auto o = opts["hash64"]) {
		auto xxh = XXH64(o->value.data(), o->value.size(), 0);
		std::cout << base64_url(xxh) << std::endl;
		return 0;
	}

	while (auto o = opts.get()) {
		switch (o->opt) {
		case 'f':
			break;
		}
	}
}
