#ifndef DB_HPP
#define DB_HPP

#include <sqlite3.h>

#include "../utils.hpp"

#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>

#include "../globals.hpp"

namespace {

bool run_sql(sqlite3*& db, const char* sql) {
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::cerr << "[db] error: sql error: " << errmsg << "\n";
        sqlite3_free(errmsg);
        return false;
    }
    return true;
}

}

void db_thread_func(void) {
    g_exit_db_thread.store(false);
    std::cout << "[db] info: starting sqllite3 database thread.\n";

    const std::string DB_PATH = "data/database.db";

    sqlite3* db = nullptr;
    int rc = sqlite3_open(DB_PATH.c_str(), &db);
    if (rc != SQLITE_OK) {
        std::cerr << "[db] error: cannot open database: " << sqlite3_errmsg(db) << "\n";
        sqlite3_close(db);
        return;
    }
    std::cout << "[db] info: database opened.\n";

    if (!run_sql(db, R"SQL(
            CREATE TABLE IF NOT EXISTS people (
                person_id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT UNIQUE
            );
            )SQL")) {
        return;
    }

    if (!run_sql(db, R"SQL(
            CREATE TABLE IF NOT EXISTS embeddings (
                embedding_id INTEGER PRIMARY KEY AUTOINCREMENT,
                person_id INTEGER NOT NULL,
                vec BLOB NOT NULL,
                img_src TEXT,
                FOREIGN KEY (person_id) REFERENCES people(person_id) ON DELETE CASCADE
            );
            )SQL")) {
        return;
    }

    while (!g_exit_db_thread.load()) {
        SQLQuery query;
        { std::unique_lock<std::mutex> lock(g_sql_queue_mutex);
            g_sql_queue_cv.wait(lock, [] { return !g_sql_queue.empty() || g_exit_db_thread.load(); });

            if (g_exit_db_thread.load() && g_sql_queue.empty()) break;
            query = std::move(g_sql_queue.top());
            g_sql_queue.pop();
        }
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, query.sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            if (query.on_bind) {
                query.on_bind(stmt);
            }

            int step_rc = sqlite3_step(stmt);
            if (query.on_row) {
                while (step_rc == SQLITE_ROW) {
                    query.on_row(stmt);
                    step_rc = sqlite3_step(stmt);
                }
            } else {
                while (step_rc == SQLITE_ROW || step_rc == SQLITE_DONE) {
                    if (step_rc == SQLITE_DONE) break;
                    step_rc = sqlite3_step(stmt);
                }
            }
            if (query.on_done) query.on_done();
        } else {
            std::cerr << "[db] error: failed to prepare sql: " << query.sql
                      << " | error: " << sqlite3_errmsg(db) << "\n";
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);

    std::cout << "[db] info: exiting sqllite3 database thread.\n";
}

#endif