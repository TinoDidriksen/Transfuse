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

#include "filesystem.hpp"
#include "xml.hpp"
#include "shared.hpp"
#include "stream.hpp"
#include <unicode/utext.h>
#include <unicode/regex.h>
#include <memory>
using namespace icu;

namespace Transfuse {

// Output functions

static void escape_body(xmlString& s, std::string_view xc) {
	for (size_t i = 0; i < xc.size(); ++i) {
		if (xc[i] == '\xee' && xc[i + 1] == '\x80' && xc[i + 2] >= '\x91' && xc[i + 2] <= '\x93') {
			if (xc[i + 2] == '\x91') {
				s += "<STYLE:";
			}
			else if (xc[i + 2] == '\x92') {
				s += ">";
			}
			else if (xc[i + 2] == '\x93') {
				s += "</STYLE>";
			}
			i += 2;
			continue;
		}
		s += static_cast<xmlChar>(xc[i]);
	}
}
static void escape_body(xmlString& s, xmlChar_view xc) {
	return escape_body(s, std::string_view(reinterpret_cast<const char*>(xc.data()), xc.size()));
}

// Turns protected tags to inline tags on the surrounding tokens
void VISLStream::protect_to_styles(xmlString& styled, State& state) {
	if (state.settings->opt_verbose) {
		std::cerr << "Protected to inline (VISL)" << std::endl;
	}

	UText tmp_ut = UTEXT_INITIALIZER;
	UErrorCode status = U_ZERO_ERROR;

	// Merge protected regions if they only have whitespace between them
	RegexMatcher rx_pmerge(R"X(\uE021([\s\r\n\p{Z}]*)\uE020)X", 0, status);

	utext_openUTF8(tmp_ut, styled);
	rx_pmerge.reset(&tmp_ut);

	xmlString ns;
	ns.reserve(styled.size());

	int32_t last = 0;
	while (rx_pmerge.find()) {
		auto b = rx_pmerge.start(status);
		ns.append(styled.begin() + last, styled.begin() + b);
		auto b1 = rx_pmerge.start(1, status);
		auto e1 = rx_pmerge.end(1, status);
		ns.append(styled.begin() + b1, styled.begin() + e1);
		last = rx_pmerge.end(status);
	}
	ns.append(styled.begin() + last, styled.end());

	styled.swap(ns);

	// Find all protected regions and convert them to styles on the surrounding tokens
	RegexMatcher rx_prots(R"X(\uE020(.*?)\uE021)X", UREGEX_DOTALL, status);
	RegexMatcher rx_block_start(R"X(>[\s\p{Zs}]*$)X", 0, status);
	RegexMatcher rx_block_end(R"X(^[\s\p{Zs}]*<)X", 0, status);

	RegexMatcher rx_tag_start(R"X(<([-:_\p{L}\p{N}\p{M}]+))X", 0, status);

	RegexMatcher rx_pfx_style(R"X(\ue013[\s\p{Zs}]*$)X", 0, status);
	RegexMatcher rx_pfx_token(R"X([^<>\s\p{Z}\ue011-\ue013]+[\s\p{Zs}]*$)X", 0, status);
	RegexMatcher rx_sfx_token(R"X(^[\s\p{Zs}]*[^<>\s\p{Z}\ue011-\ue013]+)X", 0, status);

	RegexMatcher rx_ifx_start(R"X((\ue011[^\ue012]+\ue012)[\s\p{Zs}]*$)X", 0, status);

	utext_openUTF8(tmp_ut, styled);
	rx_prots.reset(&tmp_ut);

	ns.resize(0);
	ns.reserve(styled.size());
	xmlString tmp_lxs[2];

	UText tmp_pfx = UTEXT_INITIALIZER;
	UText tmp_sfx = UTEXT_INITIALIZER;
	UText tmp_p = UTEXT_INITIALIZER;
	int64_t ni = 0;
	for (size_t i = 0; i < 100; ++i) {
		last = 0;
		while (rx_prots.find(last, status)) {
			auto b = rx_prots.start(status);
			ns.append(styled.begin() + last, styled.begin() + b);

			auto b1 = rx_prots.start(1, status);
			auto e1 = rx_prots.end(1, status);
			tmp_lxs[0].assign(styled.begin() + b1, styled.begin() + e1);
			utext_openUTF8(tmp_p, tmp_lxs[0]);
			last = rx_prots.end(status);

			auto sfx = xmlChar_view(styled).substr(SZ(last));
			utext_openUTF8(tmp_pfx, ns);
			utext_openUTF8(tmp_sfx, sfx);

			utext_setNativeIndex(&tmp_pfx, std::max(SI32(ns.size()) - 100, 0));
			ni = utext_getNativeIndex(&tmp_pfx);
			rx_block_start.reset(&tmp_pfx);
			if (rx_block_start.find(ni, status)) {
				// If we are at the beginning of a block tag, just leave the protected inline as-is
				ns += tmp_lxs[0];
				continue;
			}

			rx_block_end.reset(&tmp_sfx);
			if (rx_block_end.find()) {
				// If we are at the end of a block tag, just leave the protected inline as-is
				ns += tmp_lxs[0];
				continue;
			}

			auto hash = state.style(XC("P"), tmp_lxs[0], XC(""), "P");
			ns += TFI_OPEN_B "P:";
			ns += hash;
			ns += TFI_OPEN_E;

			bool had_tags = false;
			rx_tag_start.reset(&tmp_p);
			while (rx_tag_start.find()) {
				auto tb = rx_tag_start.start(1, status);
				auto te = rx_tag_start.end(1, status);
				ns += TFP_STREAM_B;
				ns.append(tmp_lxs[0].begin() + tb, tmp_lxs[0].begin() + te);
				ns += TFP_STREAM_E;
				had_tags = true;
			}
			if (!had_tags) {
				ns += TFP_STREAM_B;
				ns += "xml-special";
				ns += TFP_STREAM_E;
			}

			ns += TFI_CLOSE;

			/*
			utext_setNativeIndex(&tmp_pfx, std::max(SI32(ns.size()) - 100, 0));
			ni = utext_getNativeIndex(&tmp_pfx);
			rx_ifx_start.reset(&tmp_pfx);
			if (rx_ifx_start.find(ni, status)) {
				// We're inside at the start of an existing style, so wrap whole inside
				auto hash = state.style(XC("P"), tmp_lxs[0], XC(""));
				auto last_s = rx_ifx_start.end(1, status);
				tmp_lxs[1] = ns.substr(SZ(last_s));
				ns.resize(SZ(last_s));
				ns += TFI_OPEN_B "P:";
				ns += hash;
				ns += TFI_OPEN_E;
				ns += tmp_lxs[1];
				auto first_c = styled.find(XC(TFI_CLOSE), SZ(last));
				ns.append(styled, SZ(last), first_c - SZ(last));
				ns += TFI_CLOSE;
				last += SI32(first_c) - last;
				continue;
			}

			utext_setNativeIndex(&tmp_pfx, std::max(SI32(ns.size()) - 100, 0));
			ni = utext_getNativeIndex(&tmp_pfx);
			rx_pfx_style.reset(&tmp_pfx);
			if (rx_pfx_style.find(ni, status)) {
				// Create a new style around the immediately preceding style
				auto hash = state.style(XC("P"), XC(""), tmp_lxs[0]);
				auto last_s = ns.rfind(XC(TFI_OPEN_B));
				tmp_lxs[1] = ns.substr(SZ(last_s));
				ns.resize(SZ(last_s));
				ns += TFI_OPEN_B "P:";
				ns += hash;
				ns += TFI_OPEN_E;
				ns += tmp_lxs[1];
				ns += TFI_CLOSE;
				continue;
			}

			utext_setNativeIndex(&tmp_pfx, std::max(SI32(ns.size()) - 100, 0));
			ni = utext_getNativeIndex(&tmp_pfx);
			rx_pfx_token.reset(&tmp_pfx);
			if (rx_pfx_token.find(ni, status)) {
				// Create a new style around the immediately preceding token
				auto hash = state.style(XC("P"), XC(""), tmp_lxs[0]);
				auto last_s = rx_pfx_token.start(status);

				if (last_s <= std::max(SI32(ns.size()) - 100, 0)) {
					// If we are in the middle of a very long token (e.g. URL), encompass the whole token
					UChar32 cp = 0;
					utext_setNativeIndex(&tmp_pfx, last_s);
					while ((cp = UTEXT_PREVIOUS32(&tmp_pfx)) != U_SENTINEL && (cp != '>' && cp != TFI_CLOSE_UC && !u_isWhitespace(cp))) {
						// UTEXT_PREVIOUS32 already changed tmp_pfx
					}
					UTEXT_NEXT32(&tmp_pfx);
					last_s = SI(utext_getNativeIndex(&tmp_pfx));
				}
				else {
					// Move to next entity if there is one, to avoid cutting entities in half
					auto amp = ns.find('&', last_s);
					if (amp != xmlString::npos) {
						last_s = SI(amp);
					}
				}

				tmp_lxs[1] = ns.substr(SZ(last_s));
				ns.resize(SZ(last_s));
				ns += TFI_OPEN_B "P:";
				ns += hash;
				ns += TFI_OPEN_E;
				ns += tmp_lxs[1];
				ns += TFI_CLOSE;
				continue;
			}

			rx_sfx_token.reset(&tmp_sfx);
			if (rx_sfx_token.find()) {
				// Create a new style around the immediately succeeding token
				auto hash = state.style(XC("P"), tmp_lxs[0], XC(""));
				auto e = rx_sfx_token.end(status);
				tmp_lxs[1] = sfx.substr(0, e);
				ns += TFI_OPEN_B "P:";
				ns += hash;
				ns += TFI_OPEN_E;
				ns += tmp_lxs[1];
				ns += TFI_CLOSE;
				last += e;
				continue;
			}

			if (state.settings->opt_verbose) {
				std::cerr << "Could not attach protected tag: " << XV2SV(tmp_lxs[0]) << std::endl;
			}
			//*/
		}

		if (last == 0) {
			break;
		}

		ns.append(styled.begin() + last, styled.end());
		styled.swap(ns);
		utext_openUTF8(tmp_ut, styled);
		rx_prots.reset(&tmp_ut);
		ns.resize(0);
		ns.reserve(styled.size());
	}

	utext_close(&tmp_pfx);
	utext_close(&tmp_sfx);
}

void VISLStream::stream_header(xmlString& s, fs::path tmpdir) {
	s += "<STREAMCMD:TRANSFUSE:";
	s += tmpdir.string();
	s += ">\n\n";
}

void VISLStream::block_open(xmlString& s, xmlChar_view xc) {
	s += "\n<s id=\"";
	s += xc;
	s += "\">\n";
}

void VISLStream::block_body(xmlString& s, xmlChar_view xc) {
	escape_body(s, xc);
}

void VISLStream::block_term_header(xmlString& s) {
	(void)s;
}

void VISLStream::block_close(xmlString& s, xmlChar_view) {
	s += "\n</s>\n\n";
}

// Input functions

fs::path VISLStream::get_tmpdir(std::string& line) {
	auto b = line.find("<STREAMCMD:TRANSFUSE:");
	auto e = line.find(">", b);
	if (b != std::string::npos && e != std::string::npos) {
		return fs::path(line.begin() + PD(b) + 21, line.begin() + PD(e));
	}
	return {};
}

std::istream& VISLStream::get_block(std::istream& in, std::string& str, std::string& block_id) {
	str.clear();
	block_id.clear();
	while (std::getline(in, buffer)) {
		auto bb = buffer.find("<s id=\"");
		auto eb = buffer.find("\">");
		if (bb == 0 && eb != std::string::npos) {
			block_id.assign(buffer.begin() + 7, buffer.begin() + PD(eb));
			continue;
		}
		else if (buffer.compare("</s>") == 0) {
			break;
		}

		std::string_view buf{ buffer };
		while (!buf.empty()) {
			auto bs = buf.find("<STYLE:");
			auto es = buf.find("</STYLE>");
			if (es != std::string_view::npos && (bs == std::string_view::npos || es < bs)) {
				str.append(buf.begin(), buf.begin() + es);
				buf.remove_prefix(es + 8);
				str += TFI_CLOSE;
				continue;
			}
			if (bs != std::string_view::npos && (es == std::string_view::npos || bs < es)) {
				str.append(buf.begin(), buf.begin() + bs);
				str += TFI_OPEN_B;
				buf.remove_prefix(bs + 7);
				auto c = buf.find('>');
				str.append(buf.begin(), buf.begin() + c);
				buf.remove_prefix(c + 1);
				str += ";";
				str += TFI_OPEN_E;
				continue;
			}
			break;
		}
		str += buf;
	}
	return in;
}

std::istream& CGStream::get_block(std::istream& in, std::string& str, std::string& block_id) {
	str.clear();
	block_id.clear();
	while (std::getline(in, buffer)) {
		auto bb = buffer.find("<s id=\"");
		auto eb = buffer.find("\">");
		auto bs = buffer.find("<STYLE:");
		if (bb == 0 && eb != std::string::npos) {
			block_id.assign(buffer.begin() + 7, buffer.begin() + PD(eb));
			str += TF_SENTINEL;
			continue;
		}
		else if (block_id.empty()) {
			continue;
		}
		else if (bs == 0) {
			trim(buffer);
			str += TFI_OPEN_B;
			str.append(buffer.begin() + 7, buffer.end() - 1);
			str += ";";
			str += TFI_OPEN_E;
			str += TF_SENTINEL;
			continue;
		}
		else if (buffer.compare("</STYLE>") == 0) {
			str += TFI_CLOSE;
			str += TF_SENTINEL;
			continue;
		}
		else if (buffer.compare("</s>") == 0) {
			break;
		}
		str += buffer;
		str += TF_SENTINEL;
	}
	return in;
}

}
