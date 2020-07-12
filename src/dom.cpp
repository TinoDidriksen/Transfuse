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
#include <unicode/utext.h>
#include <unicode/regex.h>
#include <memory>
#include <stdexcept>
using namespace icu;

namespace Transfuse {

inline void utext_openUTF8(UText& ut, xmlChar_view xc) {
	UErrorCode status = U_ZERO_ERROR;
	utext_openUTF8(&ut, reinterpret_cast<const char*>(xc.data()), xc.size(), &status);
	if (U_FAILURE(status)) {
		throw std::runtime_error(concat("Could not open UText: ", u_errorName(status)));
	}
}

inline void append_escaped(xmlString& str, xmlChar_view xc, bool nls = false) {
	for (auto c : xc) {
		if (c == '&') {
			str.append(XC("&amp;"));
		}
		else if (c == '"') {
			str.append(XC("&quot;"));
		}
		else if (c == '\'') {
			str.append(XC("&apos;"));
		}
		else if (c == '<') {
			str.append(XC("&lt;"));
		}
		else if (c == '>') {
			str.append(XC("&gt;"));
		}
		else if (c == '\n' && nls) {
			str.append(XC("&#10;"));
		}
		else if (c == '\r' && nls) {
			str.append(XC("&#13;"));
		}
		else {
			str.push_back(c);
		}
	}
}

void append_attrs(xmlString& s, xmlNodePtr n, bool with_tf = false) {
	for (auto a = n->properties; a != nullptr; a = a->next) {
		if (with_tf == false && xmlStrncmp(a->name, XC("tf-"), 3) == 0) {
			continue;
		}
		s.push_back(' ');
		s.append(a->name);
		s.append(XC("=\""));
		append_escaped(s, a->children->content, true);
		s.push_back('"');
	}
}

DOM::DOM(State& state, xmlDocPtr xml)
  : state(state)
  , xml(xml)
  , rx_space_only(UnicodeString::fromUTF8(R"X(^([\s\p{Zs}]+)$)X"), 0, status)
  , rx_blank_only(UnicodeString::fromUTF8(R"X(^([\s\r\n\p{Z}]+)$)X"), 0, status)
  , rx_blank_head(UnicodeString::fromUTF8(R"X(^([\s\r\n\p{Z}]+))X"), 0, status)
  , rx_blank_tail(UnicodeString::fromUTF8(R"X(([\s\r\n\p{Z}]+)$)X"), 0, status)
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

	for (auto child = dom->children; child != nullptr; child = child->next) {
		(*tmp_xs)[0] = child->name;
		if (tags_prot.count(to_lower((*tmp_xs)[0]))) {
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
					xmlSetProp(child->parent, XC("tf-space-inner-before"), child->content);
				}
				else if (!child->next) {
					xmlSetProp(child->parent, XC("tf-space-inner-after"), child->content);
				}
				else if (child->prev->properties) {
					xmlSetProp(child->prev, XC("tf-space-after"), child->content);
				}
				else if (child->next->properties) {
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
				(*tmp_xs)[0].assign(child->content + rx_blank_head.start(1, status), child->content + rx_blank_head.end(1, status));
				if (child->prev) {
					if (child->prev->properties) {
						xmlSetProp(child->prev, XC("tf-space-after"), (*tmp_xs)[0].c_str());
					}
				}
				else {
					xmlSetProp(child->parent, XC("tf-space-inner-before"), (*tmp_xs)[0].c_str());
				}
			}
			if (U_FAILURE(status)) {
				throw std::runtime_error(concat("Could not match rx_blank_head: ", u_errorName(status)));
			}

			rx_blank_tail.reset(&tmp_ut);
			if (rx_blank_tail.find(status)) {
				(*tmp_xs)[0].assign(child->content + rx_blank_tail.start(1, status), child->content + rx_blank_tail.end(1, status));
				if (child->next) {
					if (child->next->properties) {
						xmlSetProp(child->prev, XC("tf-space-before"), (*tmp_xs)[0].c_str());
					}
				}
				else {
					xmlSetProp(child->parent, XC("tf-space-inner-after"), (*tmp_xs)[0].c_str());
				}
			}
			if (U_FAILURE(status)) {
				throw std::runtime_error(concat("Could not match rx_blank_tail: ", u_errorName(status)));
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
		else if (cn->type == XML_ELEMENT_NODE) {
			if (!tags_inline.count(to_lower((*tmp_xs)[5], cn->name)) || has_block_child(cn)) {
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

	utext_openUTF8(tmp_ut, styled);
	rx_prots->reset(&tmp_ut);

	ns.resize(0);
	ns.reserve(styled.size());
	tmp_xss.resize(std::max(tmp_xss.size(), static_cast<size_t>(1)));
	tmp_xs = &tmp_xss[0];

	UText tmp = UTEXT_INITIALIZER;
	last = 0;
	while (rx_prots->find()) {
		auto b = rx_prots->start(status);
		ns.append(styled.begin() + last, styled.begin() + b);

		auto b1 = rx_prots->start(1, status);
		auto e1 = rx_prots->end(1, status);
		(*tmp_xs)[0].assign(styled.begin() + b1, styled.begin() + e1);

		last = rx_prots->end(status);
	}
	ns.append(styled.begin() + last, styled.end());

	styled.swap(ns);

	// ToDo: Finish this function
}

void DOM::to_styles(xmlString& s, xmlNodePtr dom, size_t rn, bool protect) {
	if (dom == nullptr || dom->children == nullptr) {
		return;
	}
	tmp_xss.resize(std::max(tmp_xss.size(), rn + 1));
	tmp_xs = &tmp_xss[rn];

	for (auto child = dom->children; child != nullptr; child = child->next) {
		if (child->type == XML_TEXT_NODE || child->type == XML_CDATA_SECTION_NODE) {
			if (child->parent && child->parent->name && tags_raw.count(to_lower((*tmp_xs)[1], child->parent->name))) {
				s.append(child->content);
			}
			else {
				append_escaped(s, child->content);
			}
		}
		else if (child->type == XML_ELEMENT_NODE) {
			(*tmp_xs)[0] = child->name;
			auto& lname = to_lower((*tmp_xs)[0]);

			bool l_protect = false;
			if (tags_prot.count(lname) || protect) {
				l_protect = true;
			}

			// Respect HTML and XML translate attribute
			if (auto trans = xmlHasProp(child, XC("translate"))) {
				// translate="no" protects, but any other value un-protects
				l_protect = (xmlStrcmp(trans->children->content, XC("no")) == 0);
			}

			auto& otag = (*tmp_xs)[1];
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

			auto& ctag = (*tmp_xs)[2];
			ctag.assign(XC("</"));
			ctag.append(child->name);
			ctag.push_back('>');

			if (tags_prot_inline.count(lname) && !protect) {
				s.append(XC("<tf-protect>"));
				s.append(otag);
				to_styles(s, child, rn + 1, true);
				s.append(ctag);
				s.append(XC("</tf-protect>"));
				continue;
			}

			if (!l_protect && tags_inline.count(lname) && !tags_prot.count(to_lower((*tmp_xs)[3], child->children->name)) && !is_only_child(child) && !has_block_child(child)) {
				auto hash = state.store_style(lname, otag);
				s.append(XC("{tf-inline:"));
				s.append(lname);
				s.push_back(':');
				s.append(hash.begin(), hash.end());
				s.push_back('}');
				to_styles(s, child, rn + 1);
				s.append(XC("{/tf-inline}"));
				continue;
			}

			s.append(otag);
			to_styles(s, child, rn + 1, l_protect);
			s.append(ctag);
		}
	}
}

}
