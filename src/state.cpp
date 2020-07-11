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
#include <sqlite3.h>
#include <array>
#include <stdexcept>

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
	num_stmts
};

struct State::impl {
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

State::State(fs::path tmpdir, bool ro)
  : tmpdir(tmpdir)
  , s(std::make_unique<impl>())
{
	if (sqlite3_initialize() != SQLITE_OK) {
		throw std::runtime_error("sqlite3_initialize() errored");
	}

	int flags = ro ? (SQLITE_OPEN_READONLY) : (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
	if (sqlite3_open_v2((tmpdir / "state.sqlite3").string().c_str(), &s->db, flags, nullptr) != SQLITE_OK) {
		throw std::runtime_error(concat("sqlite3_open_v2() error: ", sqlite3_errmsg(s->db)));
	}

	// All the write operations and writing prepared statements
	if (!ro) {
		if (sqlite3_exec(s->db, "CREATE TABLE IF NOT EXISTS info (key TEXT PRIMARY KEY NOT NULL, value TEXT NOT NULL)") != SQLITE_OK) {
			throw std::runtime_error(concat("sqlite3 error while creating info table: ", sqlite3_errmsg(s->db)));
		}

		if (sqlite3_exec(s->db, "CREATE TABLE IF NOT EXISTS blocks (tag TEXT NOT NULL, hash TEXT NOT NULL, body TEXT NOT NULL, PRIMARY KEY (tag, hash))") != SQLITE_OK) {
			throw std::runtime_error(concat("sqlite3 error while creating blocks table: ", sqlite3_errmsg(s->db)));
		}

		if (sqlite3_exec(s->db, "CREATE TABLE IF NOT EXISTS inlines (tag TEXT NOT NULL, hash TEXT NOT NULL, body TEXT NOT NULL, PRIMARY KEY (tag, hash))") != SQLITE_OK) {
			throw std::runtime_error(concat("sqlite3 error while creating inlines table: ", sqlite3_errmsg(s->db)));
		}

		if (sqlite3_prepare_v2(s->db, "INSERT OR REPLACE INTO info (key, value) VALUES (:key, :value)", -1, &s->stm(info_ins)(), nullptr) != SQLITE_OK) {
			throw std::runtime_error(concat("sqlite3 error preparing insert into info table: ", sqlite3_errmsg(s->db)));
		}
	}

	// All the reading prepared statements
	if (sqlite3_prepare_v2(s->db, "SELECT value FROM info WHERE key = :key", -1, &s->stm(info_sel)(), nullptr) != SQLITE_OK) {
		throw std::runtime_error(concat("sqlite3 error preparing select from info table: ", sqlite3_errmsg(s->db)));
	}
}

State::~State() {
}

void State::name(std::string_view val) {
	info("name", val);
	_name.assign(val.begin(), val.end()); // ToDo: C++17 change to =
}

std::string_view State::name() {
	if (_name.empty()) {
		_name = info("name");
	}
	return _name;
}

void State::format(std::string_view val) {
	info("format", val);
	_format.assign(val.begin(), val.end()); // ToDo: C++17 change to =
}

std::string_view State::format() {
	if (_format.empty()) {
		_format = info("format");
	}
	return _format;
}

void State::info(std::string_view key, std::string_view val) {
	s->stm(info_ins).reset();
	if (sqlite3_bind_text(s->stm(info_ins), 1, key.data(), -1, SQLITE_STATIC) != SQLITE_OK) {
		throw std::runtime_error(concat("sqlite3 error trying to bind text for key: ", sqlite3_errmsg(s->db)));
	}
	if (sqlite3_bind_text(s->stm(info_ins), 2, val.data(), -1, SQLITE_STATIC) != SQLITE_OK) {
		throw std::runtime_error(concat("sqlite3 error trying to bind text for value: ", sqlite3_errmsg(s->db)));
	}
	if (sqlite3_step(s->stm(info_ins)) != SQLITE_DONE) {
		throw std::runtime_error(concat("sqlite3 error inserting into info table: ", sqlite3_errmsg(s->db)));
	}
}

std::string State::info(std::string_view key) {
	std::string rv;

	s->stm(info_sel).reset();
	if (sqlite3_bind_text(s->stm(info_sel), 1, key.data(), -1, SQLITE_STATIC) != SQLITE_OK) {
		throw std::runtime_error(concat("sqlite3 error trying to bind text for key: ", sqlite3_errmsg(s->db)));
	}

	int r = 0;
	while ((r = sqlite3_step(s->stm(info_sel))) == SQLITE_ROW) {
		rv = reinterpret_cast<const char*>(sqlite3_column_text(s->stm(info_sel), 1));
	}

	return rv;
}

}
