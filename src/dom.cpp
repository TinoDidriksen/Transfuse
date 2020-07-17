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

#include "dom.hpp"
#include "shared.hpp"
#include "base64.hpp"
#include <unicode/utext.h>
#include <unicode/regex.h>
#include <xxhash.h>
#include <memory>
#include <stdexcept>
using namespace icu;

namespace Transfuse {

void append_attrs(xmlString& s, xmlNodePtr n, bool with_tf = false) {
	for (auto a = n->properties; a != nullptr; a = a->next) {
		if (with_tf == false && xmlStrncmp(a->name, XC("tf-"), 3) == 0) {
			continue;
		}
		s.push_back(' ');
		s.append(a->name);
		s.append(XC("=\""));
		append_xml(s, a->children->content, true);
		s.push_back('"');
	}
	// ToDo: ODT family needs namespaces separately serialized?
}

DOM::DOM(State& state, xmlDocPtr xml)
  : state(state)
  , xml(xml, &xmlFreeDoc)
  , rx_space_only(UnicodeString::fromUTF8(R"X(^([\s\p{Zs}]+)$)X"), 0, status)
  , rx_blank_only(UnicodeString::fromUTF8(R"X(^([\s\r\n\p{Z}]+)$)X"), 0, status)
  , rx_blank_head(UnicodeString::fromUTF8(R"X(^([\s\r\n\p{Z}]+))X"), 0, status)
  , rx_blank_tail(UnicodeString::fromUTF8(R"X(([\s\r\n\p{Z}]+)$)X"), 0, status)
  , rx_any_alnum(UnicodeString::fromUTF8(R"X([\w\p{L}\p{N}\p{M}])X"), 0, status)
{
	if (U_FAILURE(status)) {
		throw std::runtime_error(concat("Something ICU went wrong in DOM::DOM(): ", u_errorName(status)));
	}
}

DOM::~DOM() {
	utext_close(&tmp_ut);
}

void DOM::save_spaces(xmlNodePtr dom, size_t rn) {
	if (dom == nullptr) {
		return;
	}
	tmp_xss.resize(std::max(tmp_xss.size(), rn + 1));
	tmp_xs = &tmp_xss[rn];
	auto& tmp_lxs = tmp_xss[rn];

	for (auto child = dom->children; child != nullptr; child = child->next) {
		tmp_lxs[0] = child->name;
		if (tags_prot.count(to_lower(tmp_lxs[0]))) {
			continue;
		}
		if (child->type != XML_TEXT_NODE) {
			save_spaces(child, rn + 1);
		}
		else if (child->content && child->parent) {
			utext_openUTF8(tmp_ut, child->content);

			rx_blank_only.reset(&tmp_ut);
			if (rx_blank_only.matches(status)) {
				if (!child->prev) {
					xmlSetProp(child->parent, XC("tf-space-prefix"), child->content);
				}
				else if (!child->next) {
					xmlSetProp(child->parent, XC("tf-space-suffix"), child->content);
				}
				else if (child->prev->type == XML_ELEMENT_NODE || child->prev->properties) {
					xmlSetProp(child->prev, XC("tf-space-after"), child->content);
				}
				else if (child->next->type == XML_ELEMENT_NODE || child->next->properties) {
					xmlSetProp(child->next, XC("tf-space-before"), child->content);
				}
				// If the node was entirely whitespace, skip looking for leading/trailing
				continue;
			}
			if (U_FAILURE(status)) {
				throw std::runtime_error(concat("Could not match rx_blank_only: ", u_errorName(status)));
			}

			rx_blank_head.reset(&tmp_ut);
			if (rx_blank_head.find(status)) {
				tmp_lxs[0].assign(child->content + rx_blank_head.start(1, status), child->content + rx_blank_head.end(1, status));
				if (child->prev) {
					if (child->prev->type == XML_ELEMENT_NODE || child->prev->properties) {
						xmlSetProp(child->prev, XC("tf-space-after"), tmp_lxs[0].c_str());
					}
				}
				else {
					xmlSetProp(child->parent, XC("tf-space-prefix"), tmp_lxs[0].c_str());
				}
			}
			if (U_FAILURE(status)) {
				throw std::runtime_error(concat("Could not match rx_blank_head: ", u_errorName(status)));
			}

			rx_blank_tail.reset(&tmp_ut);
			if (rx_blank_tail.find(status)) {
				tmp_lxs[0].assign(child->content + rx_blank_tail.start(1, status), child->content + rx_blank_tail.end(1, status));
				if (child->next) {
					if (child->next->type == XML_ELEMENT_NODE || child->next->properties) {
						xmlSetProp(child->next, XC("tf-space-before"), tmp_lxs[0].c_str());
					}
				}
				else {
					xmlSetProp(child->parent, XC("tf-space-suffix"), tmp_lxs[0].c_str());
				}
			}
			if (U_FAILURE(status)) {
				throw std::runtime_error(concat("Could not match rx_blank_tail: ", u_errorName(status)));
			}
		}
	}
}

void DOM::append_ltrim(xmlString& s, xmlChar_view xc) {
	utext_openUTF8(tmp_ut, xc);
	rx_blank_head.reset(&tmp_ut);
	if (rx_blank_head.find()) {
		auto e = rx_blank_head.end(0, status);
		s.append(xc.begin() + PD(e), xc.end());
	}
	else {
		s.append(xc.begin(), xc.end());
	}
}

void DOM::assign_rtrim(xmlString& s, xmlChar_view xc) {
	s.clear();
	utext_openUTF8(tmp_ut, xc);
	rx_blank_tail.reset(&tmp_ut);
	if (rx_blank_tail.find()) {
		auto b = rx_blank_tail.start(0, status);
		s.append(xc.begin(), xc.begin() + PD(b));
	}
	else {
		s.append(xc.begin(), xc.end());
	}
}

void DOM::create_spaces(xmlNodePtr dom, size_t rn) {
	if (dom == nullptr) {
		return;
	}
	tmp_xss.resize(std::max(tmp_xss.size(), rn + 1));
	tmp_xs = &tmp_xss[rn];
	auto& tmp_lxs = tmp_xss[rn];

	for (auto child = dom->children; child != nullptr; child = child->next) {
		tmp_lxs[0] = child->name;
		if (tags_prot.count(to_lower(tmp_lxs[0]))) {
			continue;
		}
		if (child->type == XML_ELEMENT_NODE || child->properties) {
			create_spaces(child, rn + 1);

			xmlAttrPtr attr;
			if ((attr = xmlHasProp(child, XC("tf-space-after"))) != nullptr) {
				auto text = xmlNewText(attr->children->content);
				xmlAddNextSibling(child, text);
				xmlRemoveProp(attr);
			}
			if ((attr = xmlHasProp(child, XC("tf-space-prefix"))) != nullptr) {
				auto text = xmlNewText(attr->children->content);
				if (child->children) {
					xmlAddPrevSibling(child->children, text);
				}
				else {
					xmlAddChild(child, text);
				}
				xmlRemoveProp(attr);
			}
			if ((attr = xmlHasProp(child, XC("tf-space-before"))) != nullptr) {
				auto text = xmlNewText(attr->children->content);
				xmlAddPrevSibling(child, text);
				xmlRemoveProp(attr);
			}
			if ((attr = xmlHasProp(child, XC("tf-space-suffix"))) != nullptr) {
				auto text = xmlNewText(attr->children->content);
				xmlAddChild(child, text);
				xmlRemoveProp(attr);
			}
		}
	}
}

void DOM::restore_spaces(xmlNodePtr dom, size_t rn) {
	if (dom == nullptr) {
		return;
	}
	tmp_xss.resize(std::max(tmp_xss.size(), rn + 1));
	tmp_xs = &tmp_xss[rn];
	auto& tmp_lxs = tmp_xss[rn];

	for (auto child = dom->children; child != nullptr; child = child->next) {
		tmp_lxs[0] = child->name;
		if (tags_prot.count(to_lower(tmp_lxs[0]))) {
			continue;
		}
		if (child->type != XML_TEXT_NODE) {
			restore_spaces(child, rn + 1);
		}
		else if (child->content && child->parent) {
			xmlAttrPtr attr;
			if (child->prev && (attr = xmlHasProp(child->prev, XC("tf-space-after"))) != nullptr) {
				tmp_lxs[1] = attr->children->content;
				append_ltrim(tmp_lxs[1], child->content);
				xmlNodeSetContent(child, tmp_lxs[1].c_str());
				xmlRemoveProp(attr);
			}
			if (child == child->parent->children && (attr = xmlHasProp(child->parent, XC("tf-space-prefix"))) != nullptr) {
				tmp_lxs[1] = attr->children->content;
				append_ltrim(tmp_lxs[1], child->content);
				xmlNodeSetContent(child, tmp_lxs[1].c_str());
				xmlRemoveProp(attr);
			}
			if (child->next && (attr = xmlHasProp(child->next, XC("tf-space-before"))) != nullptr) {
				assign_rtrim(tmp_lxs[1], child->content);
				tmp_lxs[1].append(attr->children->content);
				xmlNodeSetContent(child, tmp_lxs[1].c_str());
				xmlRemoveProp(attr);
			}
			if (child == child->parent->last && (attr = xmlHasProp(child->parent, XC("tf-space-suffix"))) != nullptr) {
				assign_rtrim(tmp_lxs[1], child->content);
				tmp_lxs[1].append(attr->children->content);
				xmlNodeSetContent(child, tmp_lxs[1].c_str());
				xmlRemoveProp(attr);
			}
		}
	}
}

bool DOM::is_space(xmlChar_view xc) {
	bool rv = true;
	utext_openUTF8(tmp_ut, xc);
	rx_space_only.reset(&tmp_ut);
	rv = rx_space_only.matches(status);
	if (U_FAILURE(status)) {
		throw std::runtime_error(concat("Could not match rx_space_only: ", u_errorName(status)));
	}
	return rv;
}

bool DOM::is_only_child(xmlNodePtr cn) {
	bool onlychild = true;
	if (!(cn->parent->children == cn || (cn->parent->children->next == cn && cn->parent->children->type == XML_TEXT_NODE && is_space(cn->parent->children->content)))) {
		onlychild = false;
	}
	else if (!(cn->parent->last == cn || (cn->parent->last->prev == cn && cn->parent->last->type == XML_TEXT_NODE && is_space(cn->parent->last->content)))) {
		onlychild = false;
	}
	if (onlychild && tags_inline.count(to_lower((*tmp_xs)[4], cn->parent->name))) {
		return is_only_child(cn->parent);
	}
	return onlychild;
}

bool DOM::has_block_child(xmlNodePtr dom) {
	bool blockchild = false;
	for (auto cn = dom->children; cn != nullptr; cn = cn->next) {
		if (cn->type == XML_TEXT_NODE) {
		}
		else if (cn->type == XML_ELEMENT_NODE || cn->properties) {
			if (!(tags_inline.count(to_lower((*tmp_xs)[5], cn->name)) || tags_prot_inline.count((*tmp_xs)[5])) || has_block_child(cn)) {
				blockchild = true;
				break;
			}
		}
	}
	return blockchild;
}

void DOM::protect_to_styles(xmlString& styled) {
	// Merge protected regions if they only have whitespace between them
	auto rx_prots = std::make_unique<RegexMatcher>(R"X(</tf-protect>([\s\r\n\p{Z}]*)<tf-protect>)X", 0, status);

	utext_openUTF8(tmp_ut, styled);
	rx_prots->reset(&tmp_ut);

	xmlString ns;
	ns.reserve(styled.size());

	int32_t last = 0;
	while (rx_prots->find()) {
		auto b = rx_prots->start(status);
		ns.append(styled.begin() + last, styled.begin() + b);
		auto b1 = rx_prots->start(1, status);
		auto e1 = rx_prots->end(1, status);
		ns.append(styled.begin() + b1, styled.begin() + e1);
		last = rx_prots->end(status);
	}
	ns.append(styled.begin() + last, styled.end());

	styled.swap(ns);

	// Find all protected regions and convert them to styles on the surrounding tokens
	rx_prots = std::make_unique<RegexMatcher>(R"X(<tf-protect>(.*?)</tf-protect>)X", UREGEX_DOTALL, status);
	RegexMatcher rx_block_start(R"X(>[\s\p{Zs}]*$)X", 0, status);
	RegexMatcher rx_block_end(R"X(^[\s\p{Zs}]*<)X", 0, status);

	RegexMatcher rx_pfx_style(R"X(\ue013[\s\p{Zs}]*$)X", 0, status);
	RegexMatcher rx_pfx_token(R"X([^>\s\p{Z}\ue012]+[\s\p{Zs}]*$)X", 0, status);

	RegexMatcher rx_ifx_start(R"X((\ue011[^\ue012]+\ue012)[\s\p{Zs}]*$)X", 0, status);

	utext_openUTF8(tmp_ut, styled);
	rx_prots->reset(&tmp_ut);

	ns.resize(0);
	ns.reserve(styled.size());
	tmp_xss.resize(std::max(tmp_xss.size(), SZ(1)));
	auto& tmp_lxs = tmp_xss[0];

	UText tmp_pfx = UTEXT_INITIALIZER;
	UText tmp_sfx = UTEXT_INITIALIZER;
	for (size_t i=0; i<100; ++i) {
		last = 0;
		while (rx_prots->find(last, status)) {
			auto b = rx_prots->start(status);
			ns.append(styled.begin() + last, styled.begin() + b);

			auto b1 = rx_prots->start(1, status);
			auto e1 = rx_prots->end(1, status);
			tmp_lxs[0].assign(styled.begin() + b1, styled.begin() + e1);
			last = rx_prots->end(status);

			utext_openUTF8(tmp_pfx, ns);
			utext_openUTF8(tmp_sfx, xmlChar_view(styled).substr(SZ(last)));

			rx_block_start.reset(&tmp_pfx);
			if (rx_block_start.find()) {
				// If we are at the beginning of a block tag, just leave the protected inline as-is
				ns.append(tmp_lxs[0]);
				continue;
			}

			rx_block_end.reset(&tmp_sfx);
			if (rx_block_end.find()) {
				// If we are at the end of a block tag, just leave the protected inline as-is
				ns.append(tmp_lxs[0]);
				continue;
			}

			rx_ifx_start.reset(&tmp_pfx);
			if (rx_ifx_start.find()) {
				// We're inside at the start of an existing style, so wrap whole inside
				auto hash = state.style(XC("P"), tmp_lxs[0], XC(""));
				auto last_s = rx_ifx_start.end(1, status);
				tmp_lxs[1] = ns.substr(SZ(last_s));
				ns.resize(SZ(last_s));
				ns.append(XC(TFI_OPEN_B "P:"));
				ns.append(hash.begin(), hash.end());
				ns.append(XC(TFI_OPEN_E));
				ns.append(tmp_lxs[1]);
				auto first_c = styled.find(XC(TFI_CLOSE), SZ(last));
				ns.append(styled, SZ(last), first_c - SZ(last));
				ns.append(XC(TFI_CLOSE));
				last += SI32(first_c) - last;
				continue;
			}

			rx_pfx_style.reset(&tmp_pfx);
			if (rx_pfx_style.find()) {
				// Create a new style around the immediately preceding style
				auto hash = state.style(XC("P"), XC(""), tmp_lxs[0]);
				auto last_s = ns.rfind(XC(TFI_OPEN_B));
				tmp_lxs[1] = ns.substr(SZ(last_s));
				ns.resize(SZ(last_s));
				ns.append(XC(TFI_OPEN_B "P:"));
				ns.append(hash.begin(), hash.end());
				ns.append(XC(TFI_OPEN_E));
				ns.append(tmp_lxs[1]);
				ns.append(XC(TFI_CLOSE));
				continue;
			}

			rx_pfx_token.reset(&tmp_pfx);
			if (rx_pfx_token.find()) {
				// Create a new style around the immediately preceding token
				auto hash = state.style(XC("P"), XC(""), tmp_lxs[0]);
				auto last_s = rx_pfx_token.start(status);
				tmp_lxs[1] = ns.substr(SZ(last_s));
				ns.resize(SZ(last_s));
				ns.append(XC(TFI_OPEN_B "P:"));
				ns.append(hash.begin(), hash.end());
				ns.append(XC(TFI_OPEN_E));
				ns.append(tmp_lxs[1]);
				ns.append(XC(TFI_CLOSE));
				continue;
			}
		}

		if (last == 0) {
			break;
		}

		ns.append(styled.begin() + last, styled.end());
		styled.swap(ns);
		utext_openUTF8(tmp_ut, styled);
		rx_prots->reset(&tmp_ut);
		ns.resize(0);
		ns.reserve(styled.size());
	}

	// ToDo: Move space at start/end of style to outside that style
	utext_close(&tmp_pfx);
	utext_close(&tmp_sfx);
}

void DOM::save_styles(xmlString& s, xmlNodePtr dom, size_t rn, bool protect) {
	if (dom == nullptr || dom->children == nullptr) {
		return;
	}
	tmp_xss.resize(std::max(tmp_xss.size(), rn + 1));
	tmp_xs = &tmp_xss[rn];
	auto& tmp_lxs = tmp_xss[rn];

	for (auto child = dom->children; child != nullptr; child = child->next) {
		if (child->type == XML_TEXT_NODE || child->type == XML_CDATA_SECTION_NODE) {
			if (child->parent && child->parent->name && tags_raw.count(to_lower(tmp_lxs[1], child->parent->name))) {
				s.append(child->content);
			}
			else {
				append_xml(s, child->content);
			}
		}
		else if (child->type == XML_ELEMENT_NODE || child->properties) {
			tmp_lxs[0] = child->name;
			auto& lname = to_lower(tmp_lxs[0]);

			bool l_protect = false;
			if (tags_prot.count(lname) || protect) {
				l_protect = true;
			}

			/* Not actually the right place to do this - we can just restore the translate="no" parts after translation
			// Respect HTML and XML translate attribute
			if (auto trans = xmlHasProp(child, XC("translate"))) {
				// translate="no" protects, but any other value un-protects
				l_protect = (xmlStrcmp(trans->children->content, XC("no")) == 0);
			}
			//*/
			if (xmlHasProp(child, XC("tf-protect"))) {
				l_protect = true;
			}

			auto& otag = tmp_lxs[1];
			otag.assign(XC("<"));
			otag.append(child->name);
			append_attrs(otag, child, true);
			if (!child->children) {
				otag.append(XC("/>"));
				if (tags_prot_inline.count(lname) && !protect) {
					s.append(XC("<tf-protect>"));
					s.append(otag);
					s.append(XC("</tf-protect>"));
				}
				else {
					s.append(otag);
				}
				continue;
			}
			otag.push_back('>');

			auto& ctag = tmp_lxs[2];
			ctag.assign(XC("</"));
			ctag.append(child->name);
			ctag.push_back('>');

			if (tags_prot_inline.count(lname) && !protect) {
				s.append(XC("<tf-protect>"));
				s.append(otag);
				save_styles(s, child, rn + 1, true);
				s.append(ctag);
				s.append(XC("</tf-protect>"));
				continue;
			}

			if (!l_protect && tags_inline.count(lname) && !tags_prot.count(to_lower(tmp_lxs[3], child->children->name)) && !is_only_child(child) && !has_block_child(child)) {
				auto hash = state.style(lname, otag, ctag);
				s.append(XC(TFI_OPEN_B));
				s.append(lname);
				s.push_back(':');
				s.append(hash.begin(), hash.end());
				s.append(XC(TFI_OPEN_E));
				save_styles(s, child, rn + 1);
				s.append(XC(TFI_CLOSE));
				continue;
			}

			s.append(otag);
			save_styles(s, child, rn + 1, l_protect);
			s.append(ctag);
		}
	}
}

void DOM::extract_blocks(xmlString& s, xmlNodePtr dom, size_t rn, bool txt) {
	if (dom == nullptr || dom->children == nullptr) {
		return;
	}
	tmp_xss.resize(std::max(tmp_xss.size(), rn + 1));
	tmp_xs = &tmp_xss[rn];
	auto& tmp_lxs = tmp_xss[rn];

	// If there are no parent tags set, assume all tags are valid parents
	if (tags_parents_allow.empty()) {
		txt = true;
	}

	for (auto child = dom->children; child != nullptr; child = child->next) {
		tmp_lxs[0] = child->name ? child->name : XC("");
		auto& lname = to_lower(tmp_lxs[0]);

		if (tags_prot.count(lname)) {
			continue;
		}

		if (child->type == XML_ELEMENT_NODE || child->properties) {
			for (auto a : tag_attrs) {
				if (auto attr = xmlHasProp(child, a.data())) {
					tmp_lxs[1] = attr->children->content;
					utext_openUTF8(tmp_ut, tmp_lxs[1]);
					rx_any_alnum.reset(&tmp_ut);
					if (!rx_any_alnum.find()) {
						continue;
					}

					++blocks;
					tmp_lxs[2] = s2x(std::to_string(blocks)).data();
					auto hash = static_cast<uint32_t>(XXH32(tmp_lxs[1].data(), tmp_lxs[1].size(), 0));
					base64_url(tmp_s, hash);
					tmp_lxs[2] += '-';
					tmp_lxs[2].append(tmp_s.begin(), tmp_s.end());

					stream->block_open(s, tmp_lxs[2]);
					stream->block_body(s, tmp_lxs[1]);
					stream->block_close(s, tmp_lxs[2]);

					tmp_lxs[3] = XC(TFB_OPEN_B);
					tmp_lxs[3].append(tmp_lxs[2]);
					tmp_lxs[3].append(XC(TFB_OPEN_E));
					tmp_lxs[3].append(tmp_lxs[1]);
					tmp_lxs[3].append(XC(TFB_CLOSE_B));
					tmp_lxs[3].append(tmp_lxs[2]);
					tmp_lxs[3].append(XC(TFB_CLOSE_E));
					xmlNodeSetContent(attr->children, tmp_lxs[3].c_str());
				}
			}
		}

		if (tags_parents_allow.count(lname)) {
			extract_blocks(s, child, rn + 1, true);
		}
		else if (child->type == XML_ELEMENT_NODE || child->properties) {
			extract_blocks(s, child, rn + 1, txt);
		}
		else if (child->content && child->content[0]) {
			if (!txt) {
				continue;
			}
			if (xmlHasProp(child->parent, XC("tf-protect"))) {
				continue;
			}

			tmp_lxs[0] = child->parent->name ? child->parent->name : XC("");
			auto& pname = to_lower(tmp_lxs[0]);

			if (!tags_parents_direct.empty() && !tags_parents_direct.count(pname)) {
				continue;
			}

			tmp_lxs[1] = child->content;
			utext_openUTF8(tmp_ut, tmp_lxs[1]);
			rx_any_alnum.reset(&tmp_ut);
			if (!rx_any_alnum.find()) {
				continue;
			}

			++blocks;
			tmp_lxs[2] = s2x(std::to_string(blocks)).data();
			auto hash = static_cast<uint32_t>(XXH32(tmp_lxs[1].data(), tmp_lxs[1].size(), 0));
			base64_url(tmp_s, hash);
			tmp_lxs[2] += '-';
			tmp_lxs[2].append(tmp_s.begin(), tmp_s.end());

			stream->block_open(s, tmp_lxs[2]);
			stream->block_body(s, tmp_lxs[1]);
			stream->block_close(s, tmp_lxs[2]);

			tmp_lxs[3] = XC(TFB_OPEN_B);
			tmp_lxs[3].append(tmp_lxs[2]);
			tmp_lxs[3].append(XC(TFB_OPEN_E));
			tmp_lxs[3].append(tmp_lxs[1]);
			tmp_lxs[3].append(XC(TFB_CLOSE_B));
			tmp_lxs[3].append(tmp_lxs[2]);
			tmp_lxs[3].append(XC(TFB_CLOSE_E));
			xmlNodeSetContent(child, tmp_lxs[3].c_str());
		}
	}
}

void cleanup_styles(std::string& str) {
	UText tmp_ut = UTEXT_INITIALIZER;
	UErrorCode status = U_ZERO_ERROR;
	std::string tmp;
	tmp.reserve(str.size());

	{
		RegexMatcher rx_spc_prefix(R"X((\ue011[^\ue012]+\ue012)([\s\p{Zs}]+))X", 0, status);
		utext_openUTF8(tmp_ut, str);
		rx_spc_prefix.reset(&tmp_ut);
		int32_t l = 0;
		while (rx_spc_prefix.find()) {
			auto tb = rx_spc_prefix.start(1, status);
			auto te = rx_spc_prefix.end(1, status);
			auto sb = rx_spc_prefix.start(2, status);
			auto se = rx_spc_prefix.end(2, status);
			tmp.append(str.begin() + l, str.begin() + tb);
			tmp.append(str.begin() + sb, str.begin() + se);
			tmp.append(str.begin() + tb, str.begin() + te);
			l = se;
		}
		tmp.append(str.begin() + l, str.end());
		str.swap(tmp);
	}

	{
		tmp.resize(0);
		RegexMatcher rx_spc_suffix(R"X(([\s\p{Zs}]+)(\ue013))X", 0, status);
		utext_openUTF8(tmp_ut, str);
		rx_spc_suffix.reset(&tmp_ut);
		int32_t l = 0;
		while (rx_spc_suffix.find()) {
			auto tb = rx_spc_suffix.start(1, status);
			auto te = rx_spc_suffix.end(1, status);
			auto sb = rx_spc_suffix.start(2, status);
			auto se = rx_spc_suffix.end(2, status);
			tmp.append(str.begin() + l, str.begin() + tb);
			tmp.append(str.begin() + sb, str.begin() + se);
			tmp.append(str.begin() + tb, str.begin() + te);
			l = se;
		}
		tmp.append(str.begin() + l, str.end());
		str.swap(tmp);
	}

	{
		tmp.resize(0);
		RegexMatcher rx_merge(R"X((\ue011[^\ue012]+\ue012)([^\ue011-\ue013]+)\ue013([\s\p{Zs}]*)(\1))X", 0, status);
		utext_openUTF8(tmp_ut, str);
		rx_merge.reset(&tmp_ut);
		int32_t l = 0;
		while (rx_merge.find()) {
			auto tb = rx_merge.start(1, status);
			auto be = rx_merge.end(2, status);
			auto sb = rx_merge.start(3, status);
			auto se = rx_merge.end(3, status);
			auto de = rx_merge.end(4, status);
			tmp.append(str.begin() + l, str.begin() + tb);
			tmp.append(str.begin() + tb, str.begin() + be);
			tmp.append(str.begin() + sb, str.begin() + se);
			l = de;
		}
		tmp.append(str.begin() + l, str.end());
		str.swap(tmp);
	}

	utext_close(&tmp_ut);
}

}
