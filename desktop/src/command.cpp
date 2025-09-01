#include "verity/command.hpp"
#include <stdexcept>

namespace verity {

CommandStack::CommandStack(IStorage& storage) : storage_(storage) {}

void CommandStack::beginBatch(std::string label) {
    if (batch_.has_value()) {
        throw std::runtime_error("Batch already in progress");
    }
    batch_ = CommandBatch{std::move(label), {}, {}};
    batch_failed_ = false;
    // Begin a single transaction for the whole batch
    storage_.begin();
}

void CommandStack::endBatch() {
    if (!batch_.has_value()) return;
    if (batch_failed_) {
        // Already rolled back on failure during execute(); drop the batch
        batch_.reset();
        batch_failed_ = false;
        return;
    }
    if (batch_->commands.empty()) {
        // Nothing happened; rollback the empty transaction
        storage_.rollback();
        batch_.reset();
        return;
    }
    // Collapse into a single composite command by capturing the batch commands
    struct Composite : public ICommand {
        std::string lbl;
        std::vector<std::unique_ptr<ICommand>> cmds;
        std::string label() const override { return lbl; }
        void doAction(IStorage& store) override {
            for (auto& c : cmds) c->doAction(store);
        }
        void undoAction(IStorage& store) override {
            for (auto it = cmds.rbegin(); it != cmds.rend(); ++it) {
                (*it)->undoAction(store);
            }
        }
    };
    auto composite = std::make_unique<Composite>();
    composite->lbl = batch_->label;
    composite->cmds = std::move(batch_->commands);
    // Commit the batch transaction
    storage_.commit();

    // Write a single coalesced revision for the batch
    if (!batch_->diffs.empty()) {
        std::string items;
        for (size_t i = 0; i < batch_->diffs.size(); ++i) {
            items += batch_->diffs[i];
            if (i + 1 < batch_->diffs.size()) items += ",";
        }
        std::string diff = std::string("{\"op\":\"batch\",\"label\":\"") + composite->lbl +
                           "\",\"items\":[" + items + "]}";
        storage_.addRevision(RevisionRecord{composite->lbl, diff});
    }

    batch_.reset();
    undo_.push_back(std::move(composite));
    redo_.clear();
}

void CommandStack::execute(std::unique_ptr<ICommand> cmd) {
    const bool batched = batch_.has_value();
    if (!batched) storage_.begin();
    try {
        cmd->doAction(storage_);
        if (!batched) storage_.commit();
    } catch (...) {
        if (batched) {
            storage_.rollback();
            batch_failed_ = true;
        } else {
            storage_.rollback();
        }
        throw;
    }

    if (auto diff = cmd->diffJson()) {
        if (batched) {
            batch_->diffs.push_back(*diff);
        } else {
            storage_.addRevision(RevisionRecord{cmd->label(), *diff});
        }
    }

    if (batched) {
        batch_->commands.push_back(std::move(cmd));
    } else {
        undo_.push_back(std::move(cmd));
        redo_.clear();
    }
}

void CommandStack::undo() {
    if (undo_.empty()) return;
    auto cmd = std::move(undo_.back());
    undo_.pop_back();
    storage_.begin();
    try {
        cmd->undoAction(storage_);
        storage_.commit();
    } catch (...) {
        storage_.rollback();
        throw;
    }
    redo_.push_back(std::move(cmd));
}

void CommandStack::redo() {
    if (redo_.empty()) return;
    auto cmd = std::move(redo_.back());
    redo_.pop_back();
    storage_.begin();
    try {
        cmd->doAction(storage_);
        storage_.commit();
    } catch (...) {
        storage_.rollback();
        throw;
    }
    undo_.push_back(std::move(cmd));
}

void CommandStack::pushRevision(const RevisionRecord& r) {
    storage_.addRevision(r);
}

} // namespace verity
