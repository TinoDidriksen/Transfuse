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

#include "base64.hpp"

namespace Transfuse {

// Non-standard base64-encoder meant for URL-safe outputs. Doesn't pad and uses -_ instead of +/
void base64_url(std::string& rv, bytes_view input) {
	constexpr static char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

	rv.resize(0);
	rv.reserve(((input.size()/3) + (input.size() % 3 > 0)) * 4);

	uint32_t tmp = 0;
	auto cursor = input.begin();
	for(size_t i = 0; i < input.size()/3; ++i) {
		tmp  = static_cast<uint32_t>((*cursor++) << 16);
		tmp += static_cast<uint32_t>((*cursor++) << 8);
		tmp += static_cast<uint32_t>((*cursor++));
		rv += table[(tmp & 0x00FC0000) >> 18];
		rv += table[(tmp & 0x0003F000) >> 12];
		rv += table[(tmp & 0x00000FC0) >> 6 ];
		rv += table[(tmp & 0x0000003F)      ];
	}

	switch(input.size() % 3) {
	case 1:
		tmp  = static_cast<uint32_t>((*cursor++) << 16);
		rv += table[(tmp & 0x00FC0000) >> 18];
		rv += table[(tmp & 0x0003F000) >> 12];
		break;
	case 2:
		tmp  = static_cast<uint32_t>((*cursor++) << 16);
		tmp += static_cast<uint32_t>((*cursor++) << 8);
		rv += table[(tmp & 0x00FC0000) >> 18];
		rv += table[(tmp & 0x0003F000) >> 12];
		rv += table[(tmp & 0x00000FC0) >> 6 ];
		break;
	}
}

}
