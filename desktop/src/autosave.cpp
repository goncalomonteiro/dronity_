#include "verity/autosave.hpp"
#include <fstream>
#include <iomanip>
#include <sstream>
#if VERITY_DESKTOP_SQLITE
#include <sqlite3.h>
#endif

namespace fs = std::filesystem;

namespace verity {

void AutosaveScheduler::snapshotOnce() {
    fs::path root(project_dir_);
    fs::path db = root / "project.db";
    fs::path snaps = root / "snapshots";
    std::error_code ec;
    fs::create_directories(snaps, ec);

#if VERITY_DESKTOP_SQLITE
    // Ensure WAL data is checkpointed before copying the file, best-effort.
    sqlite3* sdb = nullptr;
    if (sqlite3_open(db.string().c_str(), &sdb) == SQLITE_OK) {
        sqlite3_exec(sdb, "PRAGMA wal_checkpoint(FULL);", nullptr, nullptr, nullptr);
    }
    if (sdb) sqlite3_close(sdb);
#endif

    std::stringstream ss;
    ss << "slot1.db"; // single slot for simplicity; could rotate by timestamp
    fs::copy_file(db, snaps / ss.str(), fs::copy_options::overwrite_existing, ec);
}

void AutosaveScheduler::run() {
    while (running_.load()) {
        snapshotOnce();
        for (int i = 0; i < interval_.count() && running_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

} // namespace verity
