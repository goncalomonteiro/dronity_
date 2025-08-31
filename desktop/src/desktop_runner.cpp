#include "verity/command.hpp"
#include "commands/add_keyframe.hpp"
#include "commands/move_selection.hpp"
#include <iostream>

using namespace verity;

int main() {
    // Demo runner: exercise the command stack with a NullStorage
    NullStorage store;
    CommandStack stack(store);

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
    return 0;
}
