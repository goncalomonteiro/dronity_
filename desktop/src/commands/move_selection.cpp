#include "commands/move_selection.hpp"
#include "verity/db.hpp"

namespace verity {

MoveSelectionCommand::MoveSelectionCommand(std::vector<std::pair<std::string, int>> selection, int delta_ms)
    : selection_(std::move(selection)), delta_ms_(delta_ms) {}

void MoveSelectionCommand::doAction(IStorage& store) {
    (void)store;
#if VERITY_DESKTOP_SQLITE
    if (auto* sql = dynamic_cast<verity::SqliteStorage*>(&store)) {
        for (const auto& kv : selection_) {
            sql->updateKeyframeTime(kv.first, kv.second + delta_ms_);
        }
        return;
    }
#endif
}

void MoveSelectionCommand::undoAction(IStorage& store) {
    (void)store;
#if VERITY_DESKTOP_SQLITE
    if (auto* sql = dynamic_cast<verity::SqliteStorage*>(&store)) {
        for (const auto& kv : selection_) {
            sql->updateKeyframeTime(kv.first, kv.second);
        }
        return;
    }
#endif
}

std::optional<std::string> MoveSelectionCommand::diffJson() const {
    std::string s = "{\"op\":\"move\",\"delta\":" + std::to_string(delta_ms_) + ",\"items\":[";
    for (size_t i = 0; i < selection_.size(); ++i) {
        const auto& it = selection_[i];
        s += "{\\\"id\\\":\\\"" + it.first + "\\\",\\\"orig_t_ms\\\":" + std::to_string(it.second) + "}";
        if (i + 1 < selection_.size()) s += ",";
    }
    s += "]}";
    return s;
}

} // namespace verity
