#pragma once

#include "verity/command.hpp"
#include <string>

namespace verity {

class AddKeyframeCommand : public ICommand {
public:
    AddKeyframeCommand(std::string track_id, int t_ms, std::string value_json, std::string interp);
    AddKeyframeCommand(std::string track_id, int t_ms, std::string value_json, std::string interp, std::string fixed_id);
    std::string label() const override { return "AddKeyframe"; }
    void doAction(IStorage& store) override;
    void undoAction(IStorage& store) override;
    std::optional<std::string> diffJson() const override;

private:
    std::string track_id_;
    int t_ms_;
    std::string value_json_;
    std::string interp_;
    std::string key_id_;
};

} // namespace verity
