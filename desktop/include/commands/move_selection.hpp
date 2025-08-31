#pragma once

#include "verity/command.hpp"
#include <string>
#include <utility>
#include <vector>

namespace verity {

class MoveSelectionCommand : public ICommand {
public:
    // pairs of key_id and original t_ms
    MoveSelectionCommand(std::vector<std::pair<std::string, int>> selection, int delta_ms);
    std::string label() const override { return "MoveSelection"; }
    void doAction(IStorage& store) override;
    void undoAction(IStorage& store) override;
    std::optional<std::string> diffJson() const override;

private:
    std::vector<std::pair<std::string, int>> selection_; // stores original t_ms for undo
    int delta_ms_;
};

} // namespace verity

