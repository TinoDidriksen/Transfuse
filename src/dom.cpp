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

template<typename N>
inline xmlString& append_name_ns(xmlString& s, N n) {
	auto ns = getNS(n);
	if (ns && ns->prefix) {
		s += ns->prefix;
		s += ':';
	}
	if (n->name) {
		s += n->name;
	}
	return s;
}

inline xmlString& assign_name_ns(xmlString& s, xmlNodePtr n) {
	s.clear();
	return append_name_ns(s, n);
}

void append_attrs(xmlString& s, xmlNodePtr n, bool with_tf = false) {
	for (auto a = n->nsDef; a != nullptr; a = a->next) {
		s += " xmlns";
		if (a->prefix) {
			s += ":";
			s += a->prefix;
		}
		s += "=\"";
		append_xml(s, a->href);
		s += '"';
	}
	for (auto a = n->properties; a != nullptr; a = a->next) {
		if (with_tf == false && xmlStrncmp(a->name, XC("tf-"), 3) == 0) {
			continue;
		}
		s += ' ';
		append_name_ns(s, a);
		s += "=\"";
		if (a->children) {
			append_xml(s, a->children->content, true);
		}
		s += '"';
	}
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
	if (state.stream() == Streams::apertium) {
		stream.reset(new ApertiumStream(state.settings));
	}
	else {
		stream.reset(new VISLStream(state.settings));
	}

	if (U_FAILURE(status)) {
		throw std::runtime_error(concat("Something ICU went wrong in DOM::DOM(): ", u_errorName(status)));
	}
}

DOM::~DOM() {
	utext_close(&tmp_ut);
}

void DOM::cmdline_tags() {
	for (auto mt : maybe_tags) {
		if (!state.settings->tags.contains(mt)) {
			continue;
		}
		auto& ctags = state.settings->tags[mt];
		if (!ctags.contains("+")) {
			tags[mt].clear();
		}
		for (auto t : ctags) {
			tags[mt].insert(XCV(t));
		}
	}
}

// Stores whether a node had space around and/or inside it
void DOM::save_spaces(xmlNodePtr dom, size_t rn) {
	if (dom == nullptr) {
		return;
	}
	// Reasonably dirty way to let each recursion depth have its own buffers, while not allocating new ones all the time
	tmp_xss.resize(std::max(tmp_xss.size(), rn + 1));
	tmp_xs = &tmp_xss[rn];
	auto& tmp_lxs = tmp_xss[rn];

	for (auto child = dom->children; child != nullptr; child = child->next) {
		assign_name_ns(tmp_lxs[0], child);
		if (tags[Strs::tags_prot].count(to_lower(tmp_lxs[0]))) {
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

			// If this node has leading whitespace, record that either in the previous sibling or parent
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

			// If this node has trailing whitespace, record that either in the next sibling or parent
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
		s += xc;
	}
}

void DOM::assign_ltrim(xmlString& s, xmlChar_view xc) {
	s.clear();
	return append_ltrim(s, xc);
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
		s += xc;
	}
}

// restore_spaces() can only modify existing nodes, so this function will create new nodes for any remaining saved whitespace
void DOM::create_spaces(xmlNodePtr dom, size_t rn) {
	if (dom == nullptr) {
		return;
	}
	// Reasonably dirty way to let each recursion depth have its own buffers, while not allocating new ones all the time
	tmp_xss.resize(std::max(tmp_xss.size(), rn + 1));
	tmp_xs = &tmp_xss[rn];
	auto& tmp_lxs = tmp_xss[rn];

	bool apertium = (state.stream() == Streams::apertium);

	for (auto child = dom->children; child != nullptr; child = child->next) {
		assign_name_ns(tmp_lxs[0], child);
		if (tags[Strs::tags_prot].count(to_lower(tmp_lxs[0]))) {
			continue;
		}
		if (child->type == XML_ELEMENT_NODE || child->properties) {
			create_spaces(child, rn + 1);

			xmlAttrPtr attr;
			if ((attr = xmlHasProp(child, XC("tf-space-after"))) != nullptr) {
				if (!apertium) {
					auto text = xmlNewText(attr->children->content);
					xmlAddNextSibling(child, text);
				}
				xmlRemoveProp(attr);
			}
			if ((attr = xmlHasProp(child, XC("tf-space-prefix"))) != nullptr) {
				if (!apertium) {
					auto text = xmlNewText(attr->children->content);
					if (child->children) {
						xmlAddPrevSibling(child->children, text);
					}
					else {
						xmlAddChild(child, text);
					}
				}
				xmlRemoveProp(attr);
			}
			if ((attr = xmlHasProp(child, XC("tf-space-before"))) != nullptr) {
				if (!apertium) {
					auto text = xmlNewText(attr->children->content);
					xmlAddPrevSibling(child, text);
				}
				xmlRemoveProp(attr);
			}
			if ((attr = xmlHasProp(child, XC("tf-space-suffix"))) != nullptr) {
				if (!apertium) {
					auto text = xmlNewText(attr->children->content);
					xmlAddChild(child, text);
				}
				xmlRemoveProp(attr);
			}
		}
	}
}

// Inserts whitespace from save_space() back into the document
void DOM::restore_spaces(xmlNodePtr dom, size_t rn) {
	if (dom == nullptr) {
		return;
	}
	// Reasonably dirty way to let each recursion depth have its own buffers, while not allocating new ones all the time
	tmp_xss.resize(std::max(tmp_xss.size(), rn + 1));
	tmp_xs = &tmp_xss[rn];
	auto& tmp_lxs = tmp_xss[rn];

	bool apertium = (state.stream() == Streams::apertium);

	for (auto child = dom->children; child != nullptr; child = child->next) {
		assign_name_ns(tmp_lxs[0], child);
		if (tags[Strs::tags_prot].count(to_lower(tmp_lxs[0]))) {
			continue;
		}
		if (child->type != XML_TEXT_NODE) {
			restore_spaces(child, rn + 1);
		}
		else if (child->content && child->parent) {
			xmlAttrPtr attr;
			if (child->prev && (attr = xmlHasProp(child->prev, XC("tf-space-after"))) != nullptr) {
				if (!apertium) {
					tmp_lxs[1] = attr->children->content;
					append_ltrim(tmp_lxs[1], child->content);
					xmlNodeSetContent(child, tmp_lxs[1].c_str());
				}
				xmlRemoveProp(attr);
			}
			if ((attr = xmlHasProp(child->parent, XC("tf-space-prefix"))) != nullptr) {
				if (child == child->parent->children && !apertium) {
					tmp_lxs[1] = attr->children->content;
					append_ltrim(tmp_lxs[1], child->content);
					xmlNodeSetContent(child, tmp_lxs[1].c_str());
				}
				xmlRemoveProp(attr);
			}
			if (child->next && (attr = xmlHasProp(child->next, XC("tf-space-before"))) != nullptr) {
				if (!apertium) {
					assign_rtrim(tmp_lxs[1], child->content);
					tmp_lxs[1] += attr->children->content;
					xmlNodeSetContent(child, tmp_lxs[1].c_str());
				}
				xmlRemoveProp(attr);
			}
			if ((attr = xmlHasProp(child->parent, XC("tf-space-suffix"))) != nullptr) {
				if (child == child->parent->last && !apertium) {
					assign_rtrim(tmp_lxs[1], child->content);
					tmp_lxs[1] += attr->children->content;
					xmlNodeSetContent(child, tmp_lxs[1].c_str());
				}
				xmlRemoveProp(attr);
			}
			if ((attr = xmlHasProp(child->parent, XC("tf-added-before"))) != nullptr) {
				if (child->parent->prev) {
					assign_rtrim(tmp_lxs[1], child->parent->prev->content);
					xmlNodeSetContent(child->parent->prev, tmp_lxs[1].c_str());
				}
				xmlRemoveProp(attr);
			}
			if ((attr = xmlHasProp(child->parent, XC("tf-added-after"))) != nullptr) {
				if (child->parent->next) {
					assign_ltrim(tmp_lxs[1], child->parent->next->content);
					xmlNodeSetContent(child->parent->next, tmp_lxs[1].c_str());
				}
				xmlRemoveProp(attr);
			}

			if (xmlStrstr(child->content, XC(TF_SENTINEL))) {
				tmp_lxs[1].clear();
				auto c = xmlChar_view(child->content);
				size_t b = 0, e = 0;
				while ((e = c.find(XCV(TF_SENTINEL), b)) != xmlChar_view::npos) {
					tmp_lxs[1].append(c.begin() + b, c.begin() + e);
					tmp_lxs[1] += "\n";
					b = e + 3;
				}
				tmp_lxs[1].append(c.begin() + b, c.end());
				xmlNodeSetContent(child, tmp_lxs[1].c_str());
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
	if (onlychild && tags[Strs::tags_inline].count(to_lower(assign_name_ns((*tmp_xs)[4], cn->parent)))) {
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
			if (!(tags[Strs::tags_inline].count(to_lower(assign_name_ns((*tmp_xs)[5], cn))) || tags[Strs::tags_prot_inline].count((*tmp_xs)[5])) || has_block_child(cn)) {
				blockchild = true;
				break;
			}
		}
	}
	return blockchild;
}

// Serializes the XML document while turning inline tags into something the stream can deal with
void DOM::save_styles(xmlString& s, xmlNodePtr dom, size_t rn, bool protect) {
	if (dom == nullptr || dom->children == nullptr) {
		return;
	}
	// Reasonably dirty way to let each recursion depth have its own buffers, while not allocating new ones all the time
	tmp_xss.resize(std::max(tmp_xss.size(), rn + 1));
	tmp_xs = &tmp_xss[rn];
	auto& tmp_lxs = tmp_xss[rn];

	for (auto child = dom->children; child != nullptr; child = child->next) {
		if (child->type == XML_TEXT_NODE || child->type == XML_CDATA_SECTION_NODE) {
			if (child->parent && child->parent->name && tags[Strs::tags_raw].count(to_lower(assign_name_ns(tmp_lxs[1], child->parent)))) {
				s += child->content;
			}
			else {
				append_xml(s, child->content);
			}
		}
		else if (child->type == XML_ELEMENT_NODE || child->properties) {
			assign_name_ns(tmp_lxs[0], child);
			auto& lname = to_lower(tmp_lxs[0]);

			bool l_protect = false;
			if (tags[Strs::tags_prot].count(lname) || protect) {
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
			otag = XC("<");
			append_name_ns(otag, child);
			append_attrs(otag, child, true);
			if (!child->children) {
				otag += "/>";
				if (tags[Strs::tags_prot_inline].count(lname) && !protect) {
					s += XC(TFP_OPEN);
					s += otag;
					s += XC(TFP_CLOSE);
				}
				else {
					s += otag;
				}
				continue;
			}
			otag += '>';

			auto& ctag = tmp_lxs[2];
			ctag = XC("</");
			append_name_ns(ctag, child);
			ctag += '>';

			if (tags[Strs::tags_prot_inline].count(lname) && !protect) {
				s += XC(TFP_OPEN);
				s += otag;
				save_styles(s, child, rn + 1, true);
				s += ctag;
				s += XC(TFP_CLOSE);
				continue;
			}

			if (!l_protect && tags[Strs::tags_inline].count(lname) && !tags[Strs::tags_prot].count(to_lower(assign_name_ns(tmp_lxs[3], child->children))) && !is_only_child(child) && !has_block_child(child)) {
				tmp_lxs[0] = child->name;
				auto& sname = to_lower(tmp_lxs[0]);
				auto hash = state.style(sname, otag, ctag);
				s += TFI_OPEN_B;
				s += sname;
				s += ':';
				s += hash;
				s += TFI_OPEN_E;
				save_styles(s, child, rn + 1);
				s += TFI_CLOSE;
				continue;
			}

			s += otag;
			save_styles(s, child, rn + 1, l_protect);
			s += ctag;
		}
		else if (child->type == XML_COMMENT_NODE) {
			s += XC(TFP_OPEN);
			s += XC("<!--");
			s += child->content;
			s += XC("-->");
			s += XC(TFP_CLOSE);
		}
		else if (child->type == XML_PI_NODE) {
			s += XC(TFP_OPEN);
			s += XC("<?");
			s += child->name;
			s += XC(" ");
			s += child->content;
			// This ? is bizarre. child->content already contains the final ?, but it's somehow lost in the final output, even though intermediate files have ??.
			s += XC("?>");
			s += XC(TFP_CLOSE);
		}
	}
}

// Extracts blocks and textual attributes for the stream, and leaves unique markers we can later search/replace
void DOM::extract_blocks(xmlString& s, xmlNodePtr dom, size_t rn, bool txt, bool header) {
	if (dom == nullptr || dom->children == nullptr) {
		return;
	}
	// Reasonably dirty way to let each recursion depth have its own buffers, while not allocating new ones all the time
	tmp_xss.resize(std::max(tmp_xss.size(), rn + 1));
	tmp_xs = &tmp_xss[rn];
	auto& tmp_lxs = tmp_xss[rn];

	// If there are no parent tags set, assume all tags are valid parents
	if (tags[Strs::tags_parents_allow].empty()) {
		txt = true;
	}

	for (auto child = dom->children; child != nullptr; child = child->next) {
		if (child->type == XML_COMMENT_NODE || child->type == XML_PI_NODE) {
			continue;
		}

		assign_name_ns(tmp_lxs[0], child);
		auto& lname = to_lower(tmp_lxs[0]);

		if (tags[Strs::tags_prot].count(lname) || tags[Strs::tags_prot_inline].count(lname)) {
			continue;
		}

		if (child->type == XML_ELEMENT_NODE || child->properties) {
			// Extract textual attributes, if any
			for (auto a : tags[Strs::tag_attrs]) {
				if (auto attr = xmlHasProp(child, a.data())) {
					tmp_lxs[1] = attr->children->content;
					utext_openUTF8(tmp_ut, tmp_lxs[1]);
					rx_any_alnum.reset(&tmp_ut);
					if (!rx_any_alnum.find()) {
						// If the value contains no alphanumeric data, skip it
						continue;
					}

					++blocks;
					tmp_lxs[2] = s2x(std::to_string(blocks)).data();
					auto hash = static_cast<uint32_t>(XXH32(tmp_lxs[1].data(), tmp_lxs[1].size(), 0));
					base64_url(tmp_s, hash);
					tmp_lxs[2] += '-';
					tmp_lxs[2] += tmp_s;

					stream->block_open(s, tmp_lxs[2]);
					stream->block_body(s, tmp_lxs[1]);
					if (tags[Strs::attrs_headers].count(a)) {
						stream->block_term_header(s);
					}
					stream->block_close(s, tmp_lxs[2]);

					tmp_lxs[3] = XC(TFB_OPEN_B);
					tmp_lxs[3] += tmp_lxs[2];
					tmp_lxs[3] += TFB_OPEN_E;
					tmp_lxs[3] += tmp_lxs[1];
					tmp_lxs[3] += TFB_CLOSE_B;
					tmp_lxs[3] += tmp_lxs[2];
					tmp_lxs[3] += TFB_CLOSE_E;
					xmlSetProp(child, attr->name, tmp_lxs[3].c_str());
				}
			}
		}

		if (tags[Strs::tags_parents_allow].count(lname)) {
			extract_blocks(s, child, rn + 1, true, header || tags[Strs::tags_headers].count(lname));
		}
		else if (child->type == XML_ELEMENT_NODE || child->properties) {
			extract_blocks(s, child, rn + 1, txt, header || tags[Strs::tags_headers].count(lname));
		}
		else if (child->content && child->content[0]) {
			if (!txt) {
				continue;
			}
			if (xmlHasProp(child->parent, XC("tf-protect"))) {
				continue;
			}

			assign_name_ns(tmp_lxs[0], child->parent);
			auto& pname = to_lower(tmp_lxs[0]);

			if (!tags[Strs::tags_parents_direct].empty() && !tags[Strs::tags_parents_direct].count(pname)) {
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
			tmp_lxs[2] += tmp_s;

			stream->block_open(s, tmp_lxs[2]);
			stream->block_body(s, tmp_lxs[1]);
			if (header || tags[Strs::tags_headers].count(pname)) {
				stream->block_term_header(s);
			}
			stream->block_close(s, tmp_lxs[2]);

			tmp_lxs[3] = XC(TFB_OPEN_B);
			tmp_lxs[3] += tmp_lxs[2];
			tmp_lxs[3] += TFB_OPEN_E;
			tmp_lxs[3] += tmp_lxs[1];
			tmp_lxs[3] += TFB_CLOSE_B;
			tmp_lxs[3] += tmp_lxs[2];
			tmp_lxs[3] += TFB_CLOSE_E;
			xmlNodeSetContent(child, tmp_lxs[3].c_str());
		}
	}
}

// Adjust and merge inline information where applicable
void cleanup_styles(std::string& str) {
	UText tmp_ut = UTEXT_INITIALIZER;
	UErrorCode status = U_ZERO_ERROR;
	std::string tmp;
	tmp.reserve(str.size());

	RegexMatcher rx_merge(R"X((\ue011[^\ue012]+\ue012)([^\ue011-\ue013]+)\ue013([\s\p{Zs}]*)(\1))X", 0, status);
	RegexMatcher rx_nested(R"X(\ue011([^\ue012]+)\ue012\ue011([^\ue012]+)\ue012([^\ue011-\ue013]+)\ue013\ue013)X", 0, status);
	RegexMatcher rx_alpha_prefix(R"X(([\p{L}\p{N}\p{M}]*?[\p{L}\p{M}])(\ue011[^\ue012]+\ue012)(\p{L}+))X", 0, status);
	RegexMatcher rx_alpha_suffix(R"X((\p{L}[\p{L}\p{M}]*)(\ue013)(\p{L}[\p{L}\p{N}\p{M}]*))X", 0, status);
	RegexMatcher rx_spc_prefix(R"X((\ue011[^\ue012]+\ue012)([\s\p{Zs}]+))X", 0, status);
	RegexMatcher rx_spc_suffix(R"X(([\s\p{Zs}]+)(\ue013))X", 0, status);

	bool did = true;
	while (did) {
		int32_t l = 0;
		did = false;

		// Merge identical inline tags if they have nothing or only space between them (first time)
		auto merge_spans = [&]() {
			tmp.resize(0);
			utext_openUTF8(tmp_ut, str);
			rx_merge.reset(&tmp_ut);
			l = 0;
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
				did = true;
			}
			if (did) {
				tmp.append(str.begin() + l, str.end());
				str.swap(tmp);
			}
		};
		merge_spans();

		// Merge perfectly nested inline tags
		tmp.resize(0);
		utext_openUTF8(tmp_ut, str);
		rx_nested.reset(&tmp_ut);
		l = 0;
		while (rx_nested.find()) {
			auto mb = rx_nested.start(status);
			auto me = rx_nested.end(status);
			auto fb = rx_nested.start(1, status);
			auto fe = rx_nested.end(1, status);
			auto sb = rx_nested.start(2, status);
			auto se = rx_nested.end(2, status);
			auto bp = rx_nested.start(3, status);
			auto be = rx_nested.end(3, status);
			tmp.append(str.begin() + l, str.begin() + mb);

			auto ft = std::string_view(&str[SZ(fb)], SZ(fe - fb));
			auto st = std::string_view(&str[SZ(sb)], SZ(se - sb));
			trim_wb(ft);
			trim_wb(st);

			tmp += TFI_OPEN_B;
			tmp.append(ft.begin(), ft.end());
			tmp += ';';
			tmp.append(st.begin(), st.end());
			tmp += TFI_OPEN_E;
			tmp.append(str.begin() + bp, str.begin() + be);
			tmp += TFI_CLOSE;
			l = me;
			did = true;
		}
		if (did) {
			tmp.append(str.begin() + l, str.end());
			str.swap(tmp);
		}

		// If the inline tag starts with a letter and has only alphanumerics before it (ending with alpha), move that prefix inside
		tmp.resize(0);
		utext_openUTF8(tmp_ut, str);
		rx_alpha_prefix.reset(&tmp_ut);
		l = 0;
		while (rx_alpha_prefix.find()) {
			auto pb = rx_alpha_prefix.start(1, status);
			auto pe = rx_alpha_prefix.end(1, status);
			auto tb = rx_alpha_prefix.start(2, status);
			auto te = rx_alpha_prefix.end(2, status);
			auto sb = rx_alpha_prefix.start(3, status);
			auto se = rx_alpha_prefix.end(3, status);
			tmp.append(str.begin() + l, str.begin() + pb);
			tmp.append(str.begin() + tb, str.begin() + te);
			tmp.append(str.begin() + pb, str.begin() + pe);
			tmp.append(str.begin() + sb, str.begin() + se);
			l = se;
			did = true;
		}
		if (did) {
			tmp.append(str.begin() + l, str.end());
			str.swap(tmp);
		}

		// If the inline tag ends with a letter and has only alphanumerics after it (starting with alpha), move that suffix inside
		tmp.resize(0);
		utext_openUTF8(tmp_ut, str);
		rx_alpha_suffix.reset(&tmp_ut);
		l = 0;
		while (rx_alpha_suffix.find()) {
			auto pb = rx_alpha_suffix.start(1, status);
			auto pe = rx_alpha_suffix.end(1, status);
			auto tb = rx_alpha_suffix.start(2, status);
			auto te = rx_alpha_suffix.end(2, status);
			auto sb = rx_alpha_suffix.start(3, status);
			auto se = rx_alpha_suffix.end(3, status);
			tmp.append(str.begin() + l, str.begin() + pb);
			tmp.append(str.begin() + pb, str.begin() + pe);
			tmp.append(str.begin() + sb, str.begin() + se);
			tmp.append(str.begin() + tb, str.begin() + te);
			l = se;
			did = true;
		}
		if (did) {
			tmp.append(str.begin() + l, str.end());
			str.swap(tmp);
		}

		// Move leading space from inside the tag to before it
		tmp.resize(0);
		utext_openUTF8(tmp_ut, str);
		rx_spc_prefix.reset(&tmp_ut);
		l = 0;
		while (rx_spc_prefix.find()) {
			auto tb = rx_spc_prefix.start(1, status);
			auto te = rx_spc_prefix.end(1, status);
			auto sb = rx_spc_prefix.start(2, status);
			auto se = rx_spc_prefix.end(2, status);
			tmp.append(str.begin() + l, str.begin() + tb);
			tmp.append(str.begin() + sb, str.begin() + se);
			tmp.append(str.begin() + tb, str.begin() + te);
			l = se;
			did = true;
		}
		if (did) {
			tmp.append(str.begin() + l, str.end());
			str.swap(tmp);
		}

		// Move trailing space from inside the tag to after it
		tmp.resize(0);
		utext_openUTF8(tmp_ut, str);
		rx_spc_suffix.reset(&tmp_ut);
		l = 0;
		while (rx_spc_suffix.find()) {
			auto tb = rx_spc_suffix.start(1, status);
			auto te = rx_spc_suffix.end(1, status);
			auto sb = rx_spc_suffix.start(2, status);
			auto se = rx_spc_suffix.end(2, status);
			tmp.append(str.begin() + l, str.begin() + tb);
			tmp.append(str.begin() + sb, str.begin() + se);
			tmp.append(str.begin() + tb, str.begin() + te);
			l = se;
			did = true;
		}
		if (did) {
			tmp.append(str.begin() + l, str.end());
			str.swap(tmp);
		}

		// Merge identical inline tags if they have nothing or only space between them (second time)
		merge_spans();
	}

	utext_close(&tmp_ut);
}

}
