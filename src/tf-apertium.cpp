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
#include "filesystem.hpp"
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
	using namespace Options;

	auto opts = make_options(
		O('h', "help", "shows this help"),
		O('?', "", "shows this help"),
		spacer(),
		O('m', "morph", "split markup across tokens coming out of an lt-proc compatible morphological analysis; default"),
		O('g', "generate", "merge markup from tokens immediately prior to generation"),
		final()
	);
	argc = opts.parse(argc, argv);

	std::string exe = fs::path(argv[0]).stem().string();
	if (opts['h'] || opts['?']) {
		std::cout << exe << " [options]\n";
		std::cout << "\n";
		std::cout << "Options:\n";
		std::cout << opts.explain();
		return 0;
	}

	std::cin.sync_with_stdio(false);
	std::cout.sync_with_stdio(false);

	std::vector<std::string> tags;

	std::string buffer(1024, 0);
	while (std::cin.read(&buffer[0], buffer.size())) {
		buffer.resize(std::cin.gcount());

		auto tz = buffer.find("[\\[t:");
		if (tz == std::string::npos) {
			std::cout << buffer;
			continue;
		}

		std::cout.write(buffer.data(), tz);
		buffer.erase(0, tz);

		for (size_t i = 0; i < buffer.size(); ++i) {
			if (buffer[i] == '\\') {
				++i;
				continue;
			}
			if (buffer[i] == '^') {
				if (!tags.empty()) {
					std::cout << "[[";
					for (auto& t : tags) {
						std::cout << t;
						std::cout << ';';
					}
					std::cout << "]]";
				}
			}
			if (buffer[i] == '[' && buffer[i +1 ] == '\\' && buffer[i + 2] == '[' && buffer[i + 3] == 't' && buffer[i + 4] == ':') {
				buffer = buffer;
			}
		}

		buffer.resize(1024);
	}
}
