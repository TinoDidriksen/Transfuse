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

#include "state.hpp"
#include "filesystem.hpp"
#include "string_view.hpp"
#include "shared.hpp"
#include "base64.hpp"
#include "dom.hpp"
#include <libxml/tree.h>
#include <libxml/xmlsave.h>
#include <fstream>
#include <iostream>
#include <random>
#include <memory>

namespace Transfuse {

std::unique_ptr<DOM> extract_html(State& state);

fs::path extract(fs::path tmpdir, fs::path infile, std::string_view format, Stream stream, bool wipe) {
	if (stream == Stream::detect) {
		stream = Stream::apertium;
	}

	// Did not get --dir, so try to make a working dir in a temporary location
	if (tmpdir.empty()) {
		std::string name{ "transfuse-" };
		std::random_device rd;
		auto rnd = UI64(rd()) | (UI64(rd()) << UI64(32));
		name += base64_url(rnd);

		// fs::t_d_p() does check the envvars on some OSs, but not all.
		std::vector<fs::path> paths{ fs::temp_directory_path() };

		const char* envs[] = { "TMPDIR", "TEMPDIR", "TMP", "TEMP" };
		for (auto env : envs) {
			if (auto p = getenv(env)) {
				paths.push_back(p);
			}
		}
		paths.push_back("/tmp");

		// Create the working dir
		for (auto dir : paths) {
			dir /= name;
			try {
				fs::remove_all(dir);
			}
			catch (...) {
			}
			try {
				fs::create_directories(dir);
				tmpdir = dir;
				break;
			}
			catch (...) {
			}
		}
	}
	if (tmpdir.empty()) {
		throw std::runtime_error("Could not create state folder in any of OS temporary folder, $TMPDIR, $TEMPDIR, $TMP, $TEMP, or /tmp");
	}
	if (wipe) {
		try {
			fs::remove_all(tmpdir);
		}
		catch (...) {
		}
	}
	fs::create_directories(tmpdir);
	if (!fs::exists(tmpdir)) {
		throw std::runtime_error(concat("State folder did not exist and could not be created: ", tmpdir.string()));
	}

	std::unique_ptr<State> state;
	std::unique_ptr<DOM> dom;

	// If the folder already contains an extraction, assume the user just wants to output the existing extraction again, potentially in another stream format
	if (!fs::exists(tmpdir / "extracted")) {
		// If input is coming from stdin, put it into a file that we can manipulate
		if (infile == "-") {
			std::ofstream tmpfile(tmpdir / "original", std::ios::binary);
			tmpfile << std::cin.rdbuf();
			tmpfile.close();
		}
		else {
			fs::copy_file(infile, tmpdir / "original");
		}

		fs::current_path(tmpdir);

		state = std::make_unique<State>(tmpdir);
		state->name(infile.filename().string());

		if (format == "auto") {
			auto ext = infile.extension().string().substr(1);
			to_lower(ext);

			if (ext == "docx") {
				format = "docx";
			}
			else if (ext == "pptx") {
				format = "pptx";
			}
			else if (ext == "odt") {
				format = "odt";
			}
			else if (ext == "odp") {
				format = "odp";
			}
			else if (ext == "html" || ext == "htm") {
				format = "html";
			}
			else if (ext == "text" || ext == "txt") {
				format = "txt";
			}
			else {
				// ToDo: Try to open as zip and list contents, then try to find HTML tags, then give up and declare it txt
				format = "html";
			}
		}

		state->format(format);

		if (format == "html") {
			dom = extract_html(*state);
		}
	}
	else {
		fs::current_path(tmpdir);

		auto xml = xmlReadFile("styled.xml", "UTF-8", XML_PARSE_RECOVER | XML_PARSE_NONET);
		if (xml == nullptr) {
			throw std::runtime_error(concat("Could not parse styled.xml: ", xmlLastError.message));
		}
		state = std::make_unique<State>(tmpdir, true);
		dom = std::make_unique<DOM>(*state, xml);
	}

	if (stream == Stream::apertium) {
		dom->stream.reset(new ApertiumStream);
	}
	else {
		dom->stream.reset(new VISLStream);
	}
	auto extracted = dom->extract_blocks();
	file_save("extracted", x2s(extracted));

	auto cntx = xmlSaveToFilename("content.xml", "UTF-8", 0);
	xmlSaveDoc(cntx, dom->xml.get());
	xmlSaveClose(cntx);

	/*
	auto cntx = xmlSaveToFilename("content.html", "UTF-8", XML_SAVE_NO_DECL | XML_SAVE_AS_HTML);
	xmlSaveDoc(cntx, dom->xml);
	xmlSaveClose(cntx);
	//*/

	return tmpdir;
}

}
