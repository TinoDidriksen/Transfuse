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
#include "formats.hpp"
#include <libxml/tree.h>
#include <libxml/xmlsave.h>
#include <zip.h>
#include <fstream>
#include <iostream>
#include <random>
#include <memory>

namespace Transfuse {

fs::path extract(fs::path tmpdir, fs::path infile, std::string_view format, Stream stream, bool wipe) {
	if (stream == Streams::detect) {
		stream = Streams::apertium;
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
			tmpfile.exceptions(std::ios::badbit | std::ios::failbit);
			tmpfile << std::cin.rdbuf();
			tmpfile.close();
		}
		else {
			fs::copy_file(infile, tmpdir / "original");
		}

		fs::current_path(tmpdir);

		state = std::make_unique<State>(fs::current_path());
		state->name(infile.filename().string());

		if (format == "auto") {
			auto ext = infile.extension().string();
			if (!ext.empty()) {
				ext = ext.substr(1);
			}
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
				format = "text";
			}
			else {
				bool is_zip = []() {
					char buf[4]{};
					std::ifstream in("original", std::ios::binary);
					in.exceptions(std::ios::badbit | std::ios::failbit);
					in.read(buf, sizeof(buf));
					return (buf[0] == 'P' && buf[1] == 'K' && ((buf[2] == '\x03' && buf[3] == '\x04') || (buf[2] == '\x05' && buf[3] == '\x06') || (buf[2] == '\x07' && buf[3] == '\x08')));
				}();

				if (is_zip) {
					int e = 0;
					auto zip = zip_open("original", ZIP_RDONLY, &e);
					if (zip == nullptr) {
						throw std::runtime_error(concat("Could not open zip file: ", std::to_string(e)));
					}
					if (zip_name_locate(zip, "word/document.xml", 0) >= 0) {
						format = "docx";
					}
					else if (zip_name_locate(zip, "ppt/slides/slide1.xml", 0) >= 0) {
						format = "pptx";
					}
					else if (zip_name_locate(zip, "content.xml", 0) >= 0) {
						// ODP == ODT
						format = "odt";
					}
					zip_close(zip);
				}
				else {
					auto c = file_load("original");
					if (c.find("</head>") != std::string::npos || c.find("</body>") != std::string::npos || c.find("</html>") != std::string::npos) {
						format = "html";
					}
					else if (c.find("</b>") != std::string::npos || c.find("</a>") != std::string::npos || c.find("</i>") != std::string::npos || c.find("</span>") != std::string::npos || c.find("</p>") != std::string::npos || c.find("</u>") != std::string::npos || c.find("</strong>") != std::string::npos || c.find("</em>") != std::string::npos) {
						format = "html-fragment";
					}
					else {
						format = "text";
					}
				}
			}
		}
		if (format == "auto") {
			throw std::runtime_error("Could not auto-detect input file format");
		}

		state->format(format);
		state->stream(stream);

		if (format == "docx") {
			dom = extract_docx(*state);
		}
		else if (format == "pptx") {
			dom = extract_pptx(*state);
		}
		else if (format == "odt" || format == "odp") {
			dom = extract_odt(*state);
		}
		else if (format == "html") {
			dom = extract_html(*state);
		}
		else if (format == "html-fragment") {
			dom = extract_html_fragment(*state);
		}
		else if (format == "text") {
			dom = extract_text(*state);
		}
		else {
			throw std::runtime_error(concat("Unknown format: ", format));
		}
	}
	else {
		fs::current_path(tmpdir);

		auto xml = xmlReadFile("styled.xml", "UTF-8", XML_PARSE_RECOVER | XML_PARSE_NONET);
		if (xml == nullptr) {
			throw std::runtime_error(concat("Could not parse styled.xml: ", xmlLastError.message));
		}
		state = std::make_unique<State>(fs::current_path(), true);
		dom = std::make_unique<DOM>(*state, xml);
	}

	auto extracted = dom->extract_blocks();
	file_save("extracted", x2s(extracted));

	auto cntx = xmlSaveToFilename("content.xml", "UTF-8", 0);
	xmlSaveDoc(cntx, dom->xml.get());
	xmlSaveClose(cntx);

	return tmpdir;
}

}
