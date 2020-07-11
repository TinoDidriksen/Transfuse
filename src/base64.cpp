/*
* Public domain
*/

#include "base64.hpp"

// Non-standard base64-encoder meant for URL-safe outputs. Doesn't pad and uses -_ instead of +/
std::string base64_url(std::string_view input) {
	constexpr static char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

	std::string rv;
	rv.reserve(((input.size()/3) + (input.size() % 3 > 0)) * 4);

	uint32_t tmp = 0;
	auto cursor = input.begin();
	for(size_t i = 0; i < input.size()/3; ++i) {
		tmp  = (*cursor++) << 16;
		tmp += (*cursor++) << 8;
		tmp += (*cursor++);
		rv += table[(tmp & 0x00FC0000) >> 18];
		rv += table[(tmp & 0x0003F000) >> 12];
		rv += table[(tmp & 0x00000FC0) >> 6 ];
		rv += table[(tmp & 0x0000003F)      ];
	}

	switch(input.size() % 3) {
	case 1:
		tmp  = (*cursor++) << 16;
		rv += table[(tmp & 0x00FC0000) >> 18];
		rv += table[(tmp & 0x0003F000) >> 12];
		break;
	case 2:
		tmp  = (*cursor++) << 16;
		tmp += (*cursor++) << 8;
		rv += table[(tmp & 0x00FC0000) >> 18];
		rv += table[(tmp & 0x0003F000) >> 12];
		rv += table[(tmp & 0x00000FC0) >> 6 ];
		break;
	}

	return rv;
}

std::string base64_url(uint32_t input) {
	const char* r = reinterpret_cast<const char*>(&input);
	return base64_url(std::string_view(r, sizeof(input)));
}

std::string base64_url(uint64_t input) {
	const char* r = reinterpret_cast<const char*>(&input);
	return base64_url(std::string_view(r, sizeof(input)));
}
