#include "verity/autosave.hpp"
#include <fstream>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace verity {

void AutosaveScheduler::snapshotOnce() {
    fs::path root(project_dir_);
    fs::path db = root / "project.db";
    fs::path snaps = root / "snapshots";
    std::error_code ec;
    fs::create_directories(snaps, ec);

    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
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

