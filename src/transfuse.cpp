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

#include "config.hpp"
#include "options.hpp"
#include "base64.hpp"
#include "filesystem.hpp"
#include "shared.hpp"
#include "stream.hpp"
#include <unicode/uclean.h>
#include <xxhash.h>
#include <iostream>
#include <fstream>
#include <random>
#include <memory>
#include <stdexcept>
#include <cstdlib>
using namespace icu;

namespace Transfuse {

void extract(Settings&);
std::pair<fs::path, std::string> inject(Settings&);

std::istream* read_or_stdin(const char* arg, std::unique_ptr<std::istream>& in) {
	if (arg[0] == '-' && arg[1] == 0) {
		return &std::cin;
	}
	in.reset(new std::ifstream(arg, std::ios::binary));
	if (!in->good()) {
		std::string msg{"Could not read file "};
		msg += arg;
		throw std::runtime_error(msg);
	}
	in->exceptions(std::ios::badbit | std::ios::failbit);
	return in.get();
}

std::istream* read_or_stdin(fs::path arg, std::unique_ptr<std::istream>& in) {
	return read_or_stdin(arg.string().c_str(), in);
}

std::ostream* write_or_stdout(const char* arg, std::unique_ptr<std::ostream>& out) {
	if (arg[0] == '-' && arg[1] == 0) {
		return &std::cout;
	}
	out.reset(new std::ofstream(arg, std::ios::binary));
	if (!out->good()) {
		std::string msg{"Could not write file "};
		msg += arg;
		throw std::runtime_error(msg);
	}
	out->exceptions(std::ios::badbit | std::ios::failbit);
	return out.get();
}

}

int main(int argc, char* argv[]) {
	using namespace Transfuse;
	using namespace Options;

	auto opts = make_options(
		O('h', "help", "shows this help"),
		O('?',     "", "shows this help"),
		spacer(),
		O('f',  "format", ARG_REQ, "input file format: text, html, html-fragment, line, odt, odp, docx, pptx; defaults to auto"),
		O('s',  "stream", ARG_REQ, "stream format: apertium, visl; defaults to apertium"),
		O('m',    "mode", ARG_REQ, "operating mode: extract, inject, clean; default depends on executable used"),
		O('d',     "dir", ARG_REQ, "folder to store state in (implies -k); defaults to creating temporary"),
		O('k',    "keep",  ARG_NO, "don't delete temporary folder after injection"),
		O('K', "no-keep",  ARG_NO, "recreate state folder before extraction and delete it after injection"),
		O('i',   "input", ARG_REQ, "input file, if not passed as arg; default and - is stdin"),
		O('o',  "output", ARG_REQ, "output file, if not passed as arg; default and - is stdout"),
		O('H', "mark-headers", ARG_NO, "output U+2761 after headers, such as HTML tags h1-h6 and attribute 'title'"),
		O('v', "verbose",  ARG_NO, "more information about steps and progress"),
		O(0,     "debug",  ARG_NO, "write debug files in state folder"),
		O('V', "version",  ARG_NO, "output version information"),
		O(0,   "apertium-n", ARG_NO, "apertium -n mode to prevent appending .[] to blocks"),
		O(0,   "inject-raw", ARG_NO, "inserts as verbatim as possible, as XML fragments"),
		O(0,   "no-extend",  ARG_NO, "don't extend inline tags to surrounding alphanumerics"),
		spacer(),
		text("Hook programs are called with a filename as first argument. After the hook exits, Transfuse reads the same filename and uses the contents as-is."),
		spacer(),
		text("Hooks:"),
		O(0,   "hook-inject", ARG_REQ, "program to modify injected data before re-packaging"),
		spacer(),
		text("Tags and attribute names that Transfuse uses for navigation and extraction. All are comma-separated lists. If + is listed then the list is appended to the default, otherwise it will override."),
		spacer(),
		text("Options:"),
		O(0,   "tags-prot", ARG_REQ, "protected tags; will be skipped and not recursed into; e.g. HTML script, svg"),
		O(0,   "tags-prot-inline", ARG_REQ, "inline protected tags; will be attached to a token as a P marker; e.g. HTML br"),
		O(0,   "tags-raw", ARG_REQ, "CDATA tags that shouldn't be XML-encoded in the result; e.g. HTML script, style"),
		O(0,   "tags-inline", ARG_REQ, "inline tags; formatting that shouldn't cause sentence breaks; e.g. HTML a, i, b"),
		O(0,   "tags-semantic", ARG_REQ, "inline tags that should be emitted even if they're a full block"),
		O(0,   "tags-unique", ARG_REQ, "tags that must not be merged even if adjacent; e.g. TEI seg"),
		O(0,   "tags-parents-allow", ARG_REQ, "if set, only extract children of these tags; e.g. ODT text:h, text:p"),
		//O(0,   "tags-parents-direct", ARG_REQ, "not sure, maybe remnant from TTX"),
		O(0,   "tag-attrs", ARG_REQ, "attributes that should be extracted as separate segments; e.g. HTML alt, label"),
		O(0,   "tags-headers", ARG_REQ, "tags that should append ❡ (U+2761) in the extract; e.g. HTML h1, h2"),
		O(0,   "attr-headers", ARG_REQ, "attributes that should append ❡ (U+2761) in the extract; e.g. HTML title"),
		// Options after final() are still usable, but not shown in --help
		final(),
		O(0,  "url64", ARG_REQ, "base64-url encodes the passed value"),
		O(0, "hash32", ARG_REQ, "xxhash32 + base64-url encodes the passed value"),
		O(0, "hash64", ARG_REQ, "xxhash64 + base64-url encodes the passed value"),
		final()
	);
	argc = opts.parse(argc, argv);

	std::string exe = fs::path(argv[0]).stem().string();
	if (opts['h'] || opts['?']) {
		std::cout << exe << " [options] [input-file] [output-file]\n";
		std::cout << "\n";
		std::cout << "Options:\n";
		std::cout << opts.explain();
		return 0;
	}

	if (opts['V']) {
		std::cout << "Transfuse v" << TF_VERSION << std::endl;
		return 0;
	}

	if (auto o = opts["url64"]) {
		std::cout << base64_url(o->value) << std::endl;
		return 0;
	}
	if (auto o = opts["hash32"]) {
		auto xxh = static_cast<uint32_t>(XXH32(o->value.data(), o->value.size(), 0));
		std::cout << base64_url(xxh) << std::endl;
		return 0;
	}
	if (auto o = opts["hash64"]) {
		auto xxh = static_cast<uint64_t>(XXH64(o->value.data(), o->value.size(), 0));
		std::cout << base64_url(xxh) << std::endl;
		return 0;
	}

	Settings settings;
	if (exe == "tf-extract") {
		settings.mode = "extract";
	}
	else if (exe == "tf-inject") {
		settings.mode = "inject";
	}
	else if (exe == "tf-clean") {
		settings.mode = "clean";
	}

	// Handle cmdline arguments
	while (auto o = opts.get()) {
		switch (o->opt) {
		case 'f':
			settings.format = o->value;
			break;
		case 's':
			if (o->value == Streams::apertium) {
				settings.stream = Streams::apertium;
			}
			else if (o->value == Streams::visl) {
				settings.stream = Streams::visl;
			}
			else if (o->value == Streams::cg) {
				settings.stream = Streams::cg;
			}
			break;
		case 'm':
			settings.mode = o->value;
			break;
		case 'd':
			settings.tmpdir = path(o->value);
			opts.set("keep");
			settings.opt_keep = true;
			break;
		case 'k':
			settings.opt_keep = true;
			break;
		case 'K':
			opts.unset("keep");
			settings.opt_keep = false;
			settings.opt_no_keep = true;
			break;
		case 'i':
			settings.infile = path(o->value);
			break;
		case 'o':
			settings.out = write_or_stdout(o->value.data(), settings._out);
			break;
		case 'v':
			settings.opt_verbose = true;
			break;
		}
		if (o->longopt == "apertium-n") {
			settings.opt_apertium_n = true;
		}
		else if (o->longopt == "inject-raw") {
			settings.opt_inject_raw = true;
		}
		else if (o->longopt == "hook-inject") {
			settings.hook_inject = o->value;
		}
		else if (o->longopt == "no-extend") {
			settings.opt_no_extend = true;
		}
		else if (o->longopt == "debug") {
			settings.opt_debug = true;
		}
	}

	for (auto mt : maybe_tags) {
		if (auto o = opts[mt]) {
			auto val = o->value;
			size_t b = 0;
			while ((b = val.find(',')) != std::string_view::npos) {
				settings.tags[mt].insert(val.substr(0, b));
				val.remove_prefix(b+1);
			}
			settings.tags[mt].insert(val);
		}
	}

	// Funnel remaining unparsed arguments into input and/or output files
	if (argc > 2) {
		if (settings.infile.empty() && !settings.out) {
			settings.infile = argv[1];
			settings.out = write_or_stdout(argv[2], settings._out);
		}
		else if (settings.infile.empty()) {
			settings.infile = argv[1];
		}
		else if (!settings.out) {
			settings.out = write_or_stdout(argv[1], settings._out);
		}
	}
	else if (argc > 1) {
		if (settings.infile.empty()) {
			settings.infile = argv[1];
		}
		else if (!settings.out) {
			settings.out = write_or_stdout(argv[2], settings._out);
		}
	}
	if (settings.infile.empty()) {
		settings.infile = "-";
	}
	if (!settings.out) {
		settings.out = &std::cout;
	}

	UErrorCode status = U_ZERO_ERROR;
	u_init(&status);
	if (U_FAILURE(status) && status != U_FILE_ACCESS_ERROR) {
		throw std::runtime_error(concat("Could not initialize ICU: ", u_errorName(status)));
	}

	auto curdir = fs::current_path();

	if (settings.mode == "clean") {
		if (settings.opt_verbose) {
			std::cerr << "Mode: clean" << std::endl;
		}
		// Extracts and immediately injects again - useful for cleaning documents for other CAT tools, such as OmegaT
		extract(settings);
		settings.in = read_or_stdin("extracted", settings._in);
		auto rv = inject(settings);
		std::ifstream data(rv.second, std::ios::binary);
		data.exceptions(std::ios::badbit | std::ios::failbit);
		(*settings.out) << data.rdbuf();
		settings.out->flush();
		settings.tmpdir = rv.first;
	}
	else if (settings.mode == "extract") {
		if (settings.opt_verbose) {
			std::cerr << "Mode: extract" << std::endl;
		}
		extract(settings);
		std::ifstream data("extracted", std::ios::binary);
		data.exceptions(std::ios::badbit | std::ios::failbit);
		(*settings.out) << data.rdbuf();
		settings.out->flush();
	}
	else if (settings.mode == "inject") {
		if (settings.opt_verbose) {
			std::cerr << "Mode: inject" << std::endl;
		}
		settings.in = read_or_stdin(settings.infile, settings._in);
		auto rv = inject(settings);
		std::ifstream data(rv.second, std::ios::binary);
		data.exceptions(std::ios::badbit | std::ios::failbit);
		(*settings.out) << data.rdbuf();
		settings.out->flush();
		settings.tmpdir = rv.first;
	}

	// If neither --dir nor --keep, wipe the temporary folder
	if (!settings.opt_keep && (settings.mode == "clean" || settings.mode == "inject")) {
		if (settings.opt_verbose) {
			std::cerr << "Removing folder " << settings.tmpdir << std::endl;
		}
		fs::current_path(curdir);
		fs::remove_all(settings.tmpdir);
	}
}
