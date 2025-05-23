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
#include "shared.hpp"
#include "base64.hpp"
#include <xxhash.h>
#include <sqlite3.h>
#include <array>
#include <map>
#include <stdexcept>

// SQLite use is completely contained in this file and hidden from the rest of the codebase
// Only begin() and commit() hint at there being a database for storage, but they would also be useful for other storage backends

namespace Transfuse {

inline auto sqlite3_exec(sqlite3* db, const char* sql) {
	return ::sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
}

struct sqlite3_stmt_h {
	sqlite3_stmt*& operator()() {
		return s;
	}

	operator sqlite3_stmt*() {
		return s;
	}

	void reset() {
		if (s) {
			sqlite3_reset(s);
		}
	}

	void clear() {
		if (s) {
			sqlite3_finalize(s);
		}
		s = nullptr;
	}

	~sqlite3_stmt_h() {
		clear();
	}

protected:
	sqlite3_stmt* s = nullptr;
};

enum Stmt {
	info_sel,
	info_ins,
	style_ins,
	style_sel,
	num_stmts
};

struct State::impl {
	std::string name;
	std::string format;
	std::string stream;
	std::string tmp_s;

	std::map<std::string, std::map<std::string, std::tuple<std::string, std::string, std::string>>> styles;

	sqlite3* db = nullptr;
	std::array<sqlite3_stmt_h, num_stmts> stmts;

	auto& stm(Stmt s) {
		return stmts[s];
	}

	~impl() {
		for (auto& stm : stmts) {
			stm.clear();
		}
		sqlite3_close(db);
	}
};

State::State(Settings* settings, bool ro)
  : settings(settings)
  , s(std::make_unique<impl>())
{
	if (sqlite3_initialize() != SQLITE_OK) {
		throw std::runtime_error("sqlite3_initialize() errored");
	}

	int flags = ro ? (SQLITE_OPEN_READONLY) : (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
	if (sqlite3_open_v2((fs::current_path() / "state.sqlite3").string().c_str(), &s->db, flags, nullptr) != SQLITE_OK) {
		throw std::runtime_error(concat("sqlite3_open_v2() error: ", sqlite3_errmsg(s->db)));
	}

	// All the write operations and writing prepared statements
	if (!ro) {
		if (sqlite3_exec(s->db, "CREATE TABLE IF NOT EXISTS info (key TEXT PRIMARY KEY NOT NULL, value TEXT NOT NULL)") != SQLITE_OK) {
			throw std::runtime_error(concat("sqlite3 error while creating info table: ", sqlite3_errmsg(s->db)));
		}

		if (sqlite3_exec(s->db, "CREATE TABLE IF NOT EXISTS styles (tag TEXT NOT NULL, hash TEXT NOT NULL, otag TEXT NOT NULL, ctag TEXT NOT NULL, flags TEXT DEFAULT '', PRIMARY KEY (tag, hash))") != SQLITE_OK) {
			throw std::runtime_error(concat("sqlite3 error while creating inlines table: ", sqlite3_errmsg(s->db)));
		}

		if (sqlite3_prepare_v2(s->db, "INSERT OR REPLACE INTO info (key, value) VALUES (:key, :value)", -1, &s->stm(info_ins)(), nullptr) != SQLITE_OK) {
			throw std::runtime_error(concat("sqlite3 error preparing insert into info table: ", sqlite3_errmsg(s->db)));
		}

		if (sqlite3_prepare_v2(s->db, "INSERT OR REPLACE INTO styles (tag, hash, otag, ctag, flags) VALUES (:tag, :hash, :otag, :ctag, :flags)", -1, &s->stm(style_ins)(), nullptr) != SQLITE_OK) {
			throw std::runtime_error(concat("sqlite3 error preparing insert into styles table: ", sqlite3_errmsg(s->db)));
		}
	}

	// All the reading prepared statements
	if (sqlite3_prepare_v2(s->db, "SELECT value FROM info WHERE key = :key", -1, &s->stm(info_sel)(), nullptr) != SQLITE_OK) {
		throw std::runtime_error(concat("sqlite3 error preparing select from info table: ", sqlite3_errmsg(s->db)));
	}

	if (sqlite3_prepare_v2(s->db, "SELECT tag, hash, otag, ctag, flags FROM styles", -1, &s->stm(style_sel)(), nullptr) != SQLITE_OK) {
		throw std::runtime_error(concat("sqlite3 error preparing select from styles table: ", sqlite3_errmsg(s->db)));
	}
}

State::~State() {
}

void State::begin() {
	if (sqlite3_exec(s->db, "BEGIN") != SQLITE_OK) {
		throw std::runtime_error(concat("sqlite3 error while beginning transaction: ", sqlite3_errmsg(s->db)));
	}
}

void State::commit() {
	if (sqlite3_exec(s->db, "COMMIT") != SQLITE_OK) {
		throw std::runtime_error(concat("sqlite3 error while committing transaction: ", sqlite3_errmsg(s->db)));
	}
}

void State::name(std::string_view val) {
	info("name", val);
	s->name = val;
}

std::string_view State::name() {
	if (s->name.empty()) {
		s->name = info("name");
	}
	return s->name;
}

void State::format(std::string_view val) {
	info("format", val);
	s->format = val;
}

std::string_view State::format() {
	if (s->format.empty()) {
		s->format = info("format");
	}
	return s->format;
}

void State::stream(std::string_view val) {
	info("stream", val);
	s->stream = val;
}

std::string_view State::stream() {
	if (s->stream.empty()) {
		s->stream = info("stream");
	}
	return s->stream;
}

void State::info(std::string_view key, std::string_view val) {
	s->stm(info_ins).reset();
	if (sqlite3_bind_text(s->stm(info_ins), 1, key.data(), SI(key.size()), SQLITE_STATIC) != SQLITE_OK) {
		throw std::runtime_error(concat("sqlite3 error trying to bind text for key: ", sqlite3_errmsg(s->db)));
	}
	if (sqlite3_bind_text(s->stm(info_ins), 2, val.data(), SI(val.size()), SQLITE_STATIC) != SQLITE_OK) {
		throw std::runtime_error(concat("sqlite3 error trying to bind text for value: ", sqlite3_errmsg(s->db)));
	}
	if (sqlite3_step(s->stm(info_ins)) != SQLITE_DONE) {
		throw std::runtime_error(concat("sqlite3 error inserting into info table: ", sqlite3_errmsg(s->db)));
	}
}

std::string State::info(std::string_view key) {
	std::string rv;

	s->stm(info_sel).reset();
	if (sqlite3_bind_text(s->stm(info_sel), 1, key.data(), SI(key.size()), SQLITE_STATIC) != SQLITE_OK) {
		throw std::runtime_error(concat("sqlite3 error trying to bind text for key: ", sqlite3_errmsg(s->db)));
	}

	while (sqlite3_step(s->stm(info_sel)) == SQLITE_ROW) {
		rv = reinterpret_cast<const char*>(sqlite3_column_text(s->stm(info_sel), 0));
	}

	return rv;
}

xmlChar_view State::style(xmlChar_view _name, xmlChar_view _otag, xmlChar_view _ctag, std::string_view flags) {
	auto name = x2s(_name);
	auto otag = x2s(_otag);
	auto ctag = x2s(_ctag);

	// Make sure that empty opening or closing tag still causes a difference
	s->tmp_s.assign(otag.begin(), otag.end());
	s->tmp_s += TFI_HASH_SEP;
	s->tmp_s += ctag;
	auto h32 = XXH32(s->tmp_s.data(), s->tmp_s.size(), 0);
	base64_url(s->tmp_s, h32);

	s->stm(style_ins).reset();
	if (sqlite3_bind_text(s->stm(style_ins), 1, name.data(), SI(name.size()), SQLITE_STATIC) != SQLITE_OK) {
		throw std::runtime_error(concat("sqlite3 error trying to bind text for tag: ", sqlite3_errmsg(s->db)));
	}
	if (sqlite3_bind_text(s->stm(style_ins), 2, s->tmp_s.data(), SI(s->tmp_s.size()), SQLITE_STATIC) != SQLITE_OK) {
		throw std::runtime_error(concat("sqlite3 error trying to bind text for hash: ", sqlite3_errmsg(s->db)));
	}
	if (sqlite3_bind_text(s->stm(style_ins), 3, otag.data(), SI(otag.size()), SQLITE_STATIC) != SQLITE_OK) {
		throw std::runtime_error(concat("sqlite3 error trying to bind text for otag: ", sqlite3_errmsg(s->db)));
	}
	if (sqlite3_bind_text(s->stm(style_ins), 4, ctag.data(), SI(ctag.size()), SQLITE_STATIC) != SQLITE_OK) {
		throw std::runtime_error(concat("sqlite3 error trying to bind text for ctag: ", sqlite3_errmsg(s->db)));
	}
	if (sqlite3_bind_text(s->stm(style_ins), 5, flags.data(), SI(flags.size()), SQLITE_STATIC) != SQLITE_OK) {
		throw std::runtime_error(concat("sqlite3 error trying to bind text for flags: ", sqlite3_errmsg(s->db)));
	}
	if (sqlite3_step(s->stm(style_ins)) != SQLITE_DONE) {
		throw std::runtime_error(concat("sqlite3 error inserting into styles table: ", sqlite3_errmsg(s->db)));
	}

	return s2x(s->tmp_s);
}

std::tuple<std::string_view, std::string_view, std::string_view> State::style(std::string_view tag, std::string_view hash) {
	if (s->styles.empty()) {
		std::string t;
		std::string h;
		std::string o;
		std::string c;
		std::string f;
		s->stm(style_sel).reset();
		while (sqlite3_step(s->stm(style_sel)) == SQLITE_ROW) {
			t = reinterpret_cast<const char*>(sqlite3_column_text(s->stm(style_sel), 0));
			h = reinterpret_cast<const char*>(sqlite3_column_text(s->stm(style_sel), 1));
			o = reinterpret_cast<const char*>(sqlite3_column_text(s->stm(style_sel), 2));
			c = reinterpret_cast<const char*>(sqlite3_column_text(s->stm(style_sel), 3));
			f = reinterpret_cast<const char*>(sqlite3_column_text(s->stm(style_sel), 4));
			s->styles[t][h] = std::make_tuple(o, c, f);
		}
	}

	s->tmp_s.assign(tag.begin(), tag.end());
	auto t = s->styles.find(s->tmp_s);
	if (t == s->styles.end()) {
		return {};
	}

	s->tmp_s.assign(hash.begin(), hash.end());
	auto oc = t->second.find(s->tmp_s);
	if (oc == t->second.end()) {
		return {};
	}
	return oc->second;
}

}
