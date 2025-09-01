#include "verity/command.hpp"
#include "commands/add_keyframe.hpp"
#include "commands/move_selection.hpp"
#include <iostream>
#include <optional>
#include <string>
#include "verity/db.hpp"
#include "verity/replay.hpp"
#include "verity/autosave.hpp"

using namespace verity;

static std::optional<std::string> get_arg(int argc, char** argv, const std::string& flag) {
    for (int i = 1; i < argc - 1; ++i) {
        if (flag == argv[i]) return std::string(argv[i + 1]);
    }
    return std::nullopt;
}

int main(int argc, char** argv) {
    // Demo runner: exercise the command stack with a NullStorage or SQLite when --db is provided
    std::unique_ptr<IStorage> storage;
    std::optional<verity::AutosaveScheduler> autosaver;
    if (auto db = get_arg(argc, argv, "--db")) {
#if VERITY_DESKTOP_SQLITE
        storage = std::make_unique<verity::SqliteStorage>(*db);
        // If a project directory is provided via --proj, start autosave
        if (auto proj = get_arg(argc, argv, "--proj")) {
            autosaver.emplace(*proj, std::chrono::seconds(60));
            autosaver->start();
        }
#else
        std::cerr << "SQLite not enabled; rebuild with -DENABLE_SQLITE=ON\n";
        return 2;
#endif
    } else {
        storage = std::make_unique<NullStorage>();
    }

    CommandStack stack(*storage);

#if VERITY_DESKTOP_SQLITE
    if (get_arg(argc, argv, "--restore").has_value()) {
        if (auto* sql = dynamic_cast<verity::SqliteStorage*>(storage.get())) {
            auto revs = sql->readRevisions();
            // Replay into stack to reconstruct state and undo history
            restore_from_revisions(stack, *storage, revs);
            std::cout << "Restored from revisions: " << revs.size() << " entries\n";
        }
    }
#endif

    auto add = std::make_unique<AddKeyframeCommand>("track-demo", 1000, "{\"x\":1}", "auto");
    stack.execute(std::move(add));
    std::cout << "Executed AddKeyframe\n";

    std::vector<std::pair<std::string, int>> sel = {{"key-1", 1000}, {"key-2", 1500}};
    auto move = std::make_unique<MoveSelectionCommand>(sel, 50);
    stack.execute(std::move(move));
    std::cout << "Executed MoveSelection\n";

    if (stack.canUndo()) {
        stack.undo();
        std::cout << "Undo\n";
        stack.redo();
        std::cout << "Redo\n";
    }
    if (autosaver.has_value()) autosaver->stop();
    return 0;
}
