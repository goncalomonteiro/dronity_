#include "commands/add_keyframe.hpp"
#include "commands/move_selection.hpp"
#include "verity/command.hpp"
#include "verity/db.hpp"
#include <cassert>
#include <filesystem>
#include <sqlite3.h>
#include <string>

using namespace verity;

static void exec(sqlite3* db, const char* sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err ? err : "");
        sqlite3_free(err);
        assert(false && "sqlite exec failed");
    }
}

static void prepare_db(const std::string& path) {
    sqlite3* db = nullptr;
    assert(sqlite3_open(path.c_str(), &db) == SQLITE_OK);
    exec(db, "PRAGMA foreign_keys=ON;");
    exec(db, "PRAGMA journal_mode=WAL;");
    exec(db, "CREATE TABLE projects(id TEXT PRIMARY KEY, name TEXT, version INTEGER, created_at INTEGER, updated_at INTEGER);");
    exec(db, "CREATE TABLE tracks(id TEXT PRIMARY KEY, scene_id TEXT, name TEXT, kind TEXT, created_at INTEGER, updated_at INTEGER);");
    exec(db, "CREATE TABLE keyframes(id TEXT PRIMARY KEY, track_id TEXT NOT NULL, t_ms INTEGER NOT NULL, value_json TEXT NOT NULL, interp TEXT NOT NULL, created_at INTEGER, updated_at INTEGER);");
    exec(db, "CREATE TABLE revisions(id INTEGER PRIMARY KEY AUTOINCREMENT, project_id TEXT, user TEXT, label TEXT, diff_json TEXT, created_at INTEGER);");
    exec(db, "INSERT INTO projects(id,name,version,created_at,updated_at) VALUES('proj','Test',1,0,0);");
    exec(db, "INSERT INTO tracks(id,scene_id,name,kind,created_at,updated_at) VALUES('track1','scene','T','curve',0,0);");
    sqlite3_close(db);
}

static int count(sqlite3* db, const char* table) {
    std::string q = std::string("SELECT COUNT(*) FROM ") + table;
    sqlite3_stmt* st = nullptr;
    assert(sqlite3_prepare_v2(db, q.c_str(), -1, &st, nullptr) == SQLITE_OK);
    assert(sqlite3_step(st) == SQLITE_ROW);
    int c = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    return c;
}

static int get_t(sqlite3* db, const std::string& id) {
    sqlite3_stmt* st = nullptr;
    assert(sqlite3_prepare_v2(db, "SELECT t_ms FROM keyframes WHERE id=?", -1, &st, nullptr) == SQLITE_OK);
    sqlite3_bind_text(st, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    assert(sqlite3_step(st) == SQLITE_ROW);
    int t = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    return t;
}

static std::string get_first_key(sqlite3* db) {
    sqlite3_stmt* st = nullptr;
    assert(sqlite3_prepare_v2(db, "SELECT id FROM keyframes LIMIT 1", -1, &st, nullptr) == SQLITE_OK);
    assert(sqlite3_step(st) == SQLITE_ROW);
    std::string id(reinterpret_cast<const char*>(sqlite3_column_text(st, 0)));
    sqlite3_finalize(st);
    return id;
}

int main() {
    namespace fs = std::filesystem;
    fs::create_directories("test_tmp");
    std::string dbpath = "test_tmp/test.db";
    prepare_db(dbpath);

    SqliteStorage storage(dbpath);
    CommandStack stack(storage);

    // Add keyframe
    auto add = std::make_unique<AddKeyframeCommand>("track1", 1000, "{\"x\":1}", "auto", "key1");
    stack.execute(std::move(add));

    sqlite3* db = nullptr;
    assert(sqlite3_open(dbpath.c_str(), &db) == SQLITE_OK);
    assert(count(db, "keyframes") == 1);
    std::string key_id = get_first_key(db);
    assert(get_t(db, key_id) == 1000);

    // Move selection by +50
    std::vector<std::pair<std::string, int>> sel = {{key_id, 1000}};
    auto move = std::make_unique<MoveSelectionCommand>(sel, 50);
    stack.execute(std::move(move));
    assert(get_t(db, key_id) == 1050);

    // Undo move
    stack.undo();
    assert(get_t(db, key_id) == 1000);

    // Redo move
    stack.redo();
    assert(get_t(db, key_id) == 1050);

    // Undo move again (back to 1000), then undo add (remove row)
    stack.undo();
    assert(get_t(db, key_id) == 1000);
    stack.undo();
    assert(count(db, "keyframes") == 0);

    // Revisions recorded (one for add, one for move)
    assert(count(db, "revisions") >= 2);

    sqlite3_close(db);
    return 0;
}
