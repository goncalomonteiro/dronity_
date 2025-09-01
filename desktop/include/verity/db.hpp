#pragma once

#include "verity/command.hpp"
#if VERITY_DESKTOP_SQLITE
#include <sqlite3.h>
#endif
#include <string>

namespace verity {

class SqliteStorage : public IStorage {
public:
#if VERITY_DESKTOP_SQLITE
    explicit SqliteStorage(const std::string& db_path);
    ~SqliteStorage() override;
    void begin() override;
    void commit() override;
    void rollback() override;
    void addRevision(const RevisionRecord& r) override;
    std::vector<RevisionRecord> readRevisions() const;
    // Command helpers
    void insertKeyframe(const std::string& key_id,
                        const std::string& track_id,
                        int t_ms,
                        const std::string& value_json,
                        const std::string& interp);
    void deleteKeyframe(const std::string& key_id);
    void updateKeyframeTime(const std::string& key_id, int t_ms);
    // Utilities
    const std::string& dbPath() const { return db_path_; }
private:
    std::string db_path_;
    sqlite3* db_ {nullptr};
#else
    explicit SqliteStorage(const std::string&) {}
    void begin() override {}
    void commit() override {}
    void rollback() override {}
    void addRevision(const RevisionRecord&) override {}
    std::vector<RevisionRecord> readRevisions() const { return {}; }
#endif
};

} // namespace verity
