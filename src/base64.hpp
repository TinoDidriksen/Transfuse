/*
* Public domain
*/

#pragma once
#ifndef e5bd51be_BASE64_HPP__
#define e5bd51be_BASE64_HPP__

#include <string>
#include <string_view>
#include <cstdint>

// Non-standard base64-encoder meant for URL-safe outputs. Doesn't pad and uses -_ instead of +/
inline std::string base64_url(std::string_view input) {
	const static char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

	std::string rv;
	rv.reserve(((input.size()/3) + (input.size() % 3 > 0)) * 4);

	uint32_t tmp = 0;
	auto cursor = input.begin();
	for(size_t i = 0; i < input.size()/3; ++i) {
		tmp  = (*cursor++) << 16;
		tmp += (*cursor++) << 8;
		tmp += (*cursor++);
		rv.append(1, table[(tmp & 0x00FC0000) >> 18]);
		rv.append(1, table[(tmp & 0x0003F000) >> 12]);
		rv.append(1, table[(tmp & 0x00000FC0) >> 6 ]);
		rv.append(1, table[(tmp & 0x0000003F)      ]);
	}

	switch(input.size() % 3) {
	case 1:
		tmp  = (*cursor++) << 16;
		rv.append(1, table[(tmp & 0x00FC0000) >> 18]);
		rv.append(1, table[(tmp & 0x0003F000) >> 12]);
		break;
	case 2:
		tmp  = (*cursor++) << 16;
		tmp += (*cursor++) << 8;
		rv.append(1, table[(tmp & 0x00FC0000) >> 18]);
		rv.append(1, table[(tmp & 0x0003F000) >> 12]);
		rv.append(1, table[(tmp & 0x00000FC0) >> 6 ]);
		break;
	}

	return rv;
}

inline std::string base64_url(uint64_t input) {
	const char* r = reinterpret_cast<const char*>(&input);
	return base64_url(std::string_view(r, sizeof(input)));
}

#endif
