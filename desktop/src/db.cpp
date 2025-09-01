#include "verity/db.hpp"
#if VERITY_DESKTOP_SQLITE
#include <stdexcept>
#include <string>

namespace verity {

static void exec_or_throw(sqlite3* db, const char* sql) {
    char* err = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "unknown sqlite error";
        sqlite3_free(err);
        throw std::runtime_error(msg);
    }
}

SqliteStorage::SqliteStorage(const std::string& db_path) : db_path_(db_path) {
    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK) {
        throw std::runtime_error("failed to open sqlite database");
    }
    exec_or_throw(db_, "PRAGMA foreign_keys=ON;");
    exec_or_throw(db_, "PRAGMA journal_mode=WAL;");
    exec_or_throw(db_, "PRAGMA synchronous=NORMAL;");
}

SqliteStorage::~SqliteStorage() {
    if (db_) sqlite3_close(db_);
}

void SqliteStorage::begin() { exec_or_throw(db_, "BEGIN"); }
void SqliteStorage::commit() { exec_or_throw(db_, "COMMIT"); }
void SqliteStorage::rollback() { exec_or_throw(db_, "ROLLBACK"); }

void SqliteStorage::addRevision(const RevisionRecord& r) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO revisions(project_id, user, label, diff_json, created_at)"
                      " VALUES((SELECT id FROM projects LIMIT 1), ?, ?, ?, CAST(strftime('%s','now') AS INTEGER))";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("prepare failed for insert revision");
    }
    sqlite3_bind_text(stmt, 1, "local", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, r.label.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, r.diff_json.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("insert revision failed");
    }
    sqlite3_finalize(stmt);
}

std::vector<RevisionRecord> SqliteStorage::readRevisions() const {
    std::vector<RevisionRecord> out;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT label, diff_json FROM revisions ORDER BY id ASC";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("prepare failed for select revisions");
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* lbl = sqlite3_column_text(stmt, 0);
        const unsigned char* diff = sqlite3_column_text(stmt, 1);
        RevisionRecord r;
        r.label = lbl ? reinterpret_cast<const char*>(lbl) : "";
        r.diff_json = diff ? reinterpret_cast<const char*>(diff) : "";
        out.emplace_back(std::move(r));
    }
    sqlite3_finalize(stmt);
    return out;
}

void SqliteStorage::insertKeyframe(const std::string& key_id,
                                   const std::string& track_id,
                                   int t_ms,
                                   const std::string& value_json,
                                   const std::string& interp) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO keyframes(id, track_id, t_ms, value_json, interp, created_at, updated_at)"
                      " VALUES(?,?,?,?,?, CAST(strftime('%s','now') AS INTEGER), CAST(strftime('%s','now') AS INTEGER))";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("prepare failed for insert keyframe");
    }
    sqlite3_bind_text(stmt, 1, key_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, track_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, t_ms);
    sqlite3_bind_text(stmt, 4, value_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, interp.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("insert keyframe failed");
    }
    sqlite3_finalize(stmt);
}

void SqliteStorage::deleteKeyframe(const std::string& key_id) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "DELETE FROM keyframes WHERE id = ?";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("prepare failed for delete keyframe");
    }
    sqlite3_bind_text(stmt, 1, key_id.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("delete keyframe failed");
    }
    sqlite3_finalize(stmt);
}

void SqliteStorage::updateKeyframeTime(const std::string& key_id, int t_ms) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "UPDATE keyframes SET t_ms = ?, updated_at = CAST(strftime('%s','now') AS INTEGER) WHERE id = ?";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("prepare failed for update keyframe time");
    }
    sqlite3_bind_int(stmt, 1, t_ms);
    sqlite3_bind_text(stmt, 2, key_id.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("update keyframe failed");
    }
    sqlite3_finalize(stmt);
}

} // namespace verity
#endif
