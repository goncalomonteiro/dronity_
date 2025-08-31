#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace verity {

struct RevisionRecord {
    std::string label;
    std::string diff_json; // serialized effect for persistence
};

// Abstract storage transaction API (SQLite-backed implementation optional)
class IStorage {
public:
    virtual ~IStorage() = default;
    virtual void begin() = 0;
    virtual void commit() = 0;
    virtual void rollback() = 0;
    virtual void addRevision(const RevisionRecord& r) = 0; // persist revision
};

class NullStorage : public IStorage {
public:
    void begin() override {}
    void commit() override {}
    void rollback() override {}
    void addRevision(const RevisionRecord&) override {}
};

// Command interface
class ICommand {
public:
    virtual ~ICommand() = default;
    virtual std::string label() const = 0;
    virtual void doAction(IStorage& store) = 0;
    virtual void undoAction(IStorage& store) = 0;
    // Optional serialized diff for persistence
    virtual std::optional<std::string> diffJson() const { return std::nullopt; }
};

// Batch groups multiple commands as one unit for undo/redo labels
struct CommandBatch {
    std::string label;
    std::vector<std::unique_ptr<ICommand>> commands;
};

class CommandStack {
public:
    explicit CommandStack(IStorage& storage);

    void beginBatch(std::string label);
    void endBatch();
    bool inBatch() const { return batch_.has_value(); }

    void execute(std::unique_ptr<ICommand> cmd);
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    void undo();
    void redo();

    // Optional: restore undo stack from serialized revisions
    void pushRevision(const RevisionRecord& r);

private:
    IStorage& storage_;
    std::optional<CommandBatch> batch_;
    std::vector<std::unique_ptr<ICommand>> undo_;
    std::vector<std::unique_ptr<ICommand>> redo_;
};

} // namespace verity

