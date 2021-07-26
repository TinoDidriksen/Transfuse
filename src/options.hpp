/*
* Copyright (C) 2020 Tino Didriksen <mail@tinodidriksen.com>
*
* Unlike other files in this project, this file is freely usable under any of these licenses:
* Public Domain, Creative Commons CC0, MIT, Apache-2.0, LGPLv3+, GPLv3+
*/

#pragma once
#ifndef e5bd51be_OPTIONS_HPP_
#define e5bd51be_OPTIONS_HPP_

#include "string_view.hpp"
#include <utility>
#include <array>
#include <algorithm>
#include <map>
#include <string>
#include <cstring>
#include <cstdint>

namespace Options {

enum ArgType : uint8_t {
	ARG_NO, // Does not take any arguments
	ARG_OPT, // Optionally takes an argument
	ARG_REQ, // Requires an argument
};

struct Option {
	char opt = 0;
	std::string_view longopt;
	ArgType arg = ARG_NO;
	std::string_view desc;
	bool occurs = false;
	std::string_view value;

	Option(char opt, std::string_view longopt = {}, ArgType arg = ARG_NO, std::string_view desc = {})
	  : opt(opt)
	  , longopt(longopt)
	  , arg(arg)
	  , desc(desc)
	{}

	Option(char opt, std::string_view longopt = {}, std::string_view desc = {})
	  : opt(opt)
	  , longopt(longopt)
	  , desc(desc)
	{}

	Option(std::string_view longopt = {}, ArgType arg = ARG_NO, std::string_view desc = {})
	  : longopt(longopt)
	  , arg(arg)
	  , desc(desc)
	{}
};

using O = Option;

inline Option spacer() {
	return { 0, {}, ARG_OPT };
}
inline Option final() {
	return { 0, {}, ARG_REQ };
}

inline int _parse(int argc, char* argv[], Option* opts, size_t cnt) {
	int nonopts = 1;
	bool dashdash = false;

	for (int i = 1; i < argc; ++i) {
		auto arg = argv[i];

		if (dashdash || *arg != '-' || arg[1] == 0) {
			// The value was neither an option nor an argument, so keep it for later
			// This includes stand-alone - which many programs take as a special filename
			argv[nonopts++] = arg;
			continue;
		}

		auto c = arg[1];
		arg += 2;
		if (c == '-') {
			// Handle a --long-option
			if (*arg == 0) {
				// Stop parsing args after --
				dashdash = true;
			}
			else {
				Option* opt = nullptr;
				for (size_t j = 0; j < cnt; ++j) {
					if (!opts[j].longopt.empty() && opts[j].longopt.compare(arg) == 0) {
						opt = &opts[j];
						break;
					}
				}
				if (opt == nullptr) {
					return -i;
				}
				opt->occurs = true;

				if (opt->arg != ARG_NO) {
					if (i + 1 < argc && !(argv[i + 1][0] == '-' && argv[i + 1][1] != 0)) {
						opt->value = argv[++i];
					}
					else if (opt->arg == ARG_REQ) {
						return -i;
					}
				}
			}
		}
		else {
			// Handle short options, potentially bundled
			do {
				Option* opt = nullptr;
				for (size_t j = 0; j < cnt; ++j) {
					if (c == opts[j].opt) {
						opt = &opts[j];
						break;
					}
				}
				if (opt == nullptr) {
					return -i;
				}
				opt->occurs = true;

				if (opt->arg != ARG_NO) {
					// Short options may have the value inline, so eat remainder if an arg is possible
					if (*arg != 0) {
						opt->value = arg;
						break;
					}
					else if (i + 1 < argc && !(argv[i + 1][0] == '-' && argv[i + 1][1] != 0)) {
						opt->value = argv[++i];
						break;
					}
					else if (opt->arg == ARG_REQ) {
						return -i;
					}
				}

				c = *arg++;
			} while (c != 0);
		}
	}

	return nonopts;
}

template<size_t N>
struct Options {
	std::array<Option, N> opts;
	size_t cur = N + 1;
	std::map<std::string_view, Option*> map;

	const Option* operator[](std::string_view c) const {
		auto f = map.find(c);
		if (f != map.end() && f->second->occurs) {
			return f->second;
		}
		return nullptr;
	}

	const Option* operator[](char c) const {
		return operator[](std::string_view(&c, 1));
	}

	int parse(int argc, char* argv[]) {
		for (size_t i = 0; i < N; ++i) {
			auto& opt = opts[i];
			if (opt.opt) {
				map[std::string_view(&opt.opt, 1)] = &opt;
			}
			if (!opt.longopt.empty()) {
				map[opt.longopt] = &opt;
			}
		}
		return _parse(argc, argv, opts.data(), N);
	}

	const Option* get() {
		if (cur == N) {
			++cur;
			return nullptr;
		}
		if (cur == N + 1) {
			cur = 0;
		}
		for (; cur < N; ++cur) {
			if (opts[cur].occurs) {
				auto opt = &opts[cur];
				++cur;
				return opt;
			}
		}
		return nullptr;
	}

	void set(std::string_view which) {
		map[which]->occurs = true;
	}

	void set(std::string_view which, std::string_view what) {
		map[which]->occurs = true;
		map[which]->value = what;
	}

	void unset(std::string_view which) {
		map[which]->occurs = false;
	}

	std::string explain() {
		std::string rv;

		size_t sz = 0;
		size_t longest = 0;
		for (size_t i = 0; i < N; i++) {
			sz += 1;
			if (!opts[i].desc.empty()) {
				sz += 6 + opts[i].desc.size(); // At least space, dash, letter, space, description, newline
				if (!opts[i].longopt.empty()) {
					size_t len = opts[i].longopt.size();
					sz += 4 + len; // At least comma, space, 2 dashes, long name
					longest = std::max(longest, len);
				}
				sz += 3; // Wild guess at average padding amount
			}
			else if (!opts[i].opt && opts[i].longopt.empty() && opts[i].arg == ARG_REQ) {
				// Null option with required arg is final()
				break;
			}
		}
		// Preallocate the guessed size
		rv.reserve(sz);

		for (size_t i = 0; i < N; i++) {
			if (opts[i].desc.empty()) {
				if (opts[i].opt || !opts[i].longopt.empty()) {
					// Only options with descriptions are rendered
				}
				else if (opts[i].arg == ARG_REQ) {
					// Null option with required arg is final()
					break;
				}
				else {
					// Null option with optional arg is spacer()
					rv += '\n';
				}
				continue;
			}

			rv += ' ';
			size_t ldiff = longest;
			if (opts[i].opt && !opts[i].longopt.empty()) {
				rv += '-';
				rv += opts[i].opt;
				rv += ", --";
				rv += opts[i].longopt;
				ldiff -= opts[i].longopt.size();
			}
			else if (opts[i].opt) {
				rv += '-';
				rv += opts[i].opt;
				rv += "    ";
			}
			else if (!opts[i].longopt.empty()) {
				rv += "    --";
				rv += opts[i].longopt;
				ldiff -= opts[i].longopt.size();
			}

			while (ldiff--) {
				rv += ' ';
			}
			rv += "  ";
			rv += opts[i].desc;
			rv += '\n';
		}

		return rv;
	}
};

template<class D = void, class... Types>
constexpr auto make_options(Types&&... t) -> Options<sizeof...(Types)> {
	return { { std::forward<Types>(t)... } };
}

}
#endif
