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
#include <libxml/xmlsave.h>
#include <fstream>
#include <iostream>
#include <random>
#include <memory>

namespace Transfuse {

std::unique_ptr<DOM> extract_html(fs::path tmpdir, State& state);

fs::path extract(fs::path tmpdir, fs::path infile, std::string_view format, Stream stream, bool wipe) {
	// Did not get --dir, so try to make a working dir in a temporary location
	if (tmpdir.empty()) {
		std::string name{ "transfuse-" };
		std::random_device rd;
		uint64_t rnd = static_cast<uint64_t>(rd()) | (static_cast<uint64_t>(rd()) << 32llu);
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
			fs::create_directories(tmpdir);
		}
		catch (...) {
		}
	}

	// If the folder already contains an original, assume the user just wants to output the existing extraction again, potentially in another stream format
	if (!fs::exists(tmpdir / "original")) {
		// If input is coming from stdin, put it into a file that we can manipulate
		if (infile == "-") {
			std::ofstream tmpfile((tmpdir / "original").string(), std::ios::binary);
			tmpfile << std::cin.rdbuf();
			tmpfile.close();
		}
		else {
			fs::copy_file(infile, tmpdir / "original");
		}

		State state(tmpdir);
		state.name(infile.filename().string());

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

		state.format(format);

		std::unique_ptr<DOM> dom;
		if (format == "html") {
			dom = extract_html(tmpdir, state);
		}

		if (stream == Stream::apertium) {
			dom->stream.reset(new ApertiumStream);
		}
		else {
			dom->stream.reset(new VISLStream);
		}
		auto extracted = dom->extract_blocks();
		file_save(tmpdir / "extracted", x2s(extracted));

		auto cntx = xmlSaveToFilename((tmpdir / "content.xml").string().c_str(), "UTF-8", 0);
		xmlSaveDoc(cntx, dom->xml);
		xmlSaveFlush(cntx);

		/*
		auto cntx = xmlSaveToFilename((tmpdir / "content.html").string().c_str(), "UTF-8", XML_SAVE_NO_DECL | XML_SAVE_AS_HTML);
		xmlSaveDoc(cntx, dom->xml);
		xmlSaveFlush(cntx);
		//*/
	}

	return tmpdir;
}

}
