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

#include "shared.hpp"
#include "filesystem.hpp"
#include "state.hpp"
#include "dom.hpp"
#include "formats.hpp"
#include <libxml/HTMLparser.h>
#include <libxml/tree.h>
#include <libxml/xmlsave.h>
#include <unicode/regex.h>
#include <unicode/ustring.h>
#include <memory>
using namespace icu;

namespace Transfuse {

std::unique_ptr<DOM> extract_html(State& state, std::unique_ptr<icu::UnicodeString> data) {
	if (!data) {
		auto raw_data = file_load("original");
		auto enc = detect_encoding(raw_data);
		data = std::make_unique<UnicodeString>(to_ustring(raw_data, enc));

		// If there is no closing tag, this can't be a fully formed valid HTML document
		if (data->indexOf("</html>") == -1 && data->indexOf("</HTML>") == -1) {
			// Perform expensive lower-casing and check again, just in case someone uses </Html> or similar
			auto tmp = *data;
			tmp.toLower();
			if (tmp.indexOf("</html>") == -1) {
				state.format("html-fragment");
				return extract_html_fragment(state);
			}
		}
	}

	// Find any charset="" charset='' charset= and replace with a placeholder that we will set to UTF-8 in injection
	UErrorCode status = U_ZERO_ERROR;
	RegexMatcher rx(R"X(charset\s*=(["']?)\s*([-\w\d]+)\s*(["']?))X", UREGEX_CASE_INSENSITIVE, status);
	if (U_FAILURE(status)) {
		throw std::runtime_error(concat("Could not create RegexMatcher: ", u_errorName(status)));
	}

	rx.reset(*data);
	if (rx.find()) {
		UnicodeString cset("charset=");
		auto b = rx.start(1, status);
		auto e = rx.end(1, status);
		cset.append(*data, b, e-b);
		cset += XML_ENC_UC;
		b = rx.start(3, status);
		e = rx.end(3, status);
		cset.append(*data, b, e - b);

		b = rx.start(0, status);
		e = rx.end(0, status);
		data->replace(b, e - b, cset);
	}
	if (U_FAILURE(status)) {
		throw std::runtime_error(concat("Could not replace charset in data: ", u_errorName(status)));
	}

	{
		// Protect <script> and <style> because they may contain unescaped & and other meta-characters that annoy the XML parser
		RegexMatcher _rx_script(R"X(<script[^<>]*>(.*?)</script[^<>]*>)X", UREGEX_DOTALL | UREGEX_CASE_INSENSITIVE, status);
		RegexMatcher _rx_style(R"X(<style[^<>]*>(.*?)</style[^<>]*>)X", UREGEX_DOTALL | UREGEX_CASE_INSENSITIVE, status);
		RegexMatcher* rx_ss[]{ &_rx_script, &_rx_style };
		std::string tmp_str;
		std::string tmp_p;
		for (auto& rxs : rx_ss) {
			rxs->reset(*data);
			int32_t last = 0;
			while (rxs->find(last, status)) {
				auto b = rxs->start(1, status);
				auto e = rxs->end(1, status);
				if (b == e) {
					last = b + 1;
					continue;
				}
				tmp_str.resize(SZ((e - b) * 4));
				int32_t olen = 0;
				int32_t slen = 0;
				u_strToUTF8WithSub(&tmp_str[0], SI32(tmp_str.size()), &olen, &data->getTerminatedBuffer()[b], e - b, u'\uFFFD', &slen, &status);

				auto hash = state.style("U", tmp_str, "");
				tmp_p.clear();
				tmp_p += TFU_OPEN;
				tmp_p += hash;
				tmp_p += TFU_CLOSE;
				data->replaceBetween(b, e, UnicodeString::fromUTF8(tmp_p));
				rxs->reset(*data);
				last = b + 1;
			}
		}

		// Wipe <wbr>, &shy;, and all other forms soft-hyphens can take
		UnicodeString tmp;
		RegexMatcher rx_shy(R"X((<wbr\s*/?>)|(\u00ad)|(&shy;)|(&#173;)|(&#x(0*)ad;))X", UREGEX_CASE_INSENSITIVE, status);
		rx_shy.reset(*data);
		tmp = rx_shy.replaceAll("", status);
		std::swap(tmp, *data);

		// Add spaces around <sub> and <sup> where needed, and record that we've done so
		RegexMatcher rx_subp_open(R"X(([^>\s])(<su[bp])( |>))X", UREGEX_CASE_INSENSITIVE, status);
		rx_subp_open.reset(*data);
		tmp = rx_subp_open.replaceAll("$1 $2 tf-added-before=\"1\"$3", status);
		std::swap(tmp, *data);

		RegexMatcher rx_subp_close(R"X(<(su[bp])( |>)(.*?)(</\1>)([^<\s]))X", UREGEX_CASE_INSENSITIVE, status);
		rx_subp_close.reset(*data);
		tmp = rx_subp_close.replaceAll("<$1 tf-added-after=\"1\"$2$3$4 $5", status);
		std::swap(tmp, *data);
	}

	auto xml = htmlReadMemory(reinterpret_cast<const char*>(data->getTerminatedBuffer()), SI(SZ(data->length()) * sizeof(UChar)), "transfuse.html", utf16_native, HTML_PARSE_RECOVER | HTML_PARSE_NOWARNING | HTML_PARSE_NOERROR | HTML_PARSE_NONET);
	if (xml == nullptr) {
		throw std::runtime_error(concat("Could not parse HTML: ", xmlLastError.message));
	}
	data.reset();

	auto dom = std::make_unique<DOM>(state, xml);
	dom->tags[Strs::tags_prot] = make_xmlChars("applet", "area", "base", "cite", "code", "frame", "frameset", "link", "meta", "nowiki", "object", "pre", "ref", "script", "style", "svg", "syntaxhighlight", "template");
	dom->tags[Strs::tags_prot_inline] = make_xmlChars("apertium-notrans", "br", "ruby");
	dom->tags[Strs::tags_raw] = make_xmlChars("script", "style", "svg");
	dom->tags[Strs::tags_inline] = make_xmlChars("a", "abbr", "acronym", "address", "b", "bdi", "bdo", "big", "del", "em", "font", "i", "ins", "kbd", "mark", "meter", "output", "q", "s", "samp", "small", "span", "strike", "strong", "sub", "sup", "time", "tt", "u", "var");
	dom->tags[Strs::tag_attrs] = make_xmlChars("alt", "caption", "label", "summary", "title", "placeholder");
	if (state.settings->opt_mark_headers) {
		dom->tags[Strs::tags_headers] = make_xmlChars("h1", "h2", "h3", "h4", "h5", "h6");
		dom->tags[Strs::attrs_headers] = make_xmlChars("title");
	}
	dom->cmdline_tags();
	dom->save_spaces();

	auto styled = dom->save_styles(true);
	file_save("styled.xml", x2s(styled));
	dom->xml.reset(xmlReadMemory(reinterpret_cast<const char*>(styled.data()), SI(styled.size()), "styled.xml", "UTF-8", XML_PARSE_RECOVER | XML_PARSE_NONET));
	if (dom->xml == nullptr) {
		throw std::runtime_error(concat("Could not parse styled XML: ", xmlLastError.message));
	}

	return dom;
}

std::string inject_html(DOM& dom) {
	auto cntx = xmlSaveToFilename("injected.html", "UTF-8", XML_SAVE_AS_HTML);
	xmlSaveDoc(cntx, dom.xml.get());
	xmlSaveClose(cntx);

	std::ifstream in("original", std::ios::binary);
	in.exceptions(std::ios::badbit | std::ios::failbit);
	std::string line;
	std::getline(in, line);
	bool had_doctype = to_lower(line).find("<!doctype") != std::string::npos;
	in.close();

	auto content = file_load("injected.html");
	auto b = content.find(XML_ENC_U8);
	if (b != std::string::npos) {
		content.replace(b, 3, "UTF-8");
		// libxml2's serializer adds this <meta> tag to be helpful, but we already had one
		std::string_view meta{ R"X(<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">)X" };
		auto m = content.find(meta);
		if (m != std::string::npos) {
			content.erase(m, meta.size());
		}
	}
	if (had_doctype) {
		content.insert(0, "<!DOCTYPE html>\n");
	}

	b = content.find(TFU_OPEN);
	while (b != std::string::npos) {
		auto e = content.find(TFU_CLOSE, b);
		std::string_view hash{&content[b + 3], e - b - 3};
		auto [topen, tclose, _] = dom.state.style("U", hash);
		content.erase(content.begin() + PD(b), content.begin() + PD(e) + 3);
		content.insert(content.begin() + PD(b), tclose.begin(), tclose.end());
		content.insert(content.begin() + PD(b), topen.begin(), topen.end());
		b = content.find(TFU_OPEN);
	}

	file_save("injected.html", content);

	hook_inject(dom.state.settings, "injected.xml");

	return "injected.html";
}

}
