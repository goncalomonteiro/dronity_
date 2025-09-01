#include "verity/replay.hpp"
#include "commands/add_keyframe.hpp"
#include "commands/move_selection.hpp"
#include <cctype>
#include <stdexcept>

namespace verity {

static std::string get_string(const std::string& json, const std::string& key) {
    const std::string needle = std::string("\"") + key + "\"";
    auto k = json.find(needle);
    if (k == std::string::npos) return {};
    auto colon = json.find(':', k + needle.size());
    if (colon == std::string::npos) return {};
    auto q = json.find('"', colon + 1);
    if (q == std::string::npos) return {};
    std::string out;
    for (size_t i = q + 1; i < json.size(); ++i) {
        char c = json[i];
        if (c == '\\') {
            if (i + 1 < json.size()) {
                char n = json[++i];
                switch (n) {
                case '\\': out.push_back('\\'); break;
                case '"': out.push_back('"'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                default: out.push_back(n); break;
                }
            }
            continue;
        }
        if (c == '"') break;
        out.push_back(c);
    }
    return out;
}

static int get_int(const std::string& json, const std::string& key) {
    const std::string needle = std::string("\"") + key + "\"";
    auto k = json.find(needle);
    if (k == std::string::npos) return 0;
    auto colon = json.find(':', k + needle.size());
    if (colon == std::string::npos) return 0;
    size_t i = colon + 1;
    while (i < json.size() && std::isspace(static_cast<unsigned char>(json[i]))) ++i;
    int sign = 1;
    if (i < json.size() && (json[i] == '-' || json[i] == '+')) {
        if (json[i] == '-') sign = -1;
        ++i;
    }
    int val = 0;
    while (i < json.size() && std::isdigit(static_cast<unsigned char>(json[i]))) {
        val = val * 10 + (json[i] - '0');
        ++i;
    }
    return sign * val;
}

static std::string get_op(const std::string& json) { return get_string(json, "op"); }

static void replay_one(CommandStack& stack, IStorage& store, const std::string& json);

static void replay_batch(CommandStack& stack, IStorage& store, const std::string& json) {
    // naive scan for items array
    const std::string tag = "\"items\":";
    auto p = json.find(tag);
    if (p == std::string::npos) return;
    auto lb = json.find('[', p + tag.size());
    if (lb == std::string::npos) return;
    auto rb = json.rfind(']');
    if (rb == std::string::npos || rb <= lb) return;
    std::string items = json.substr(lb + 1, rb - lb - 1);
    // split on top-level commas between objects (assumes no nested objects with unbalanced braces)
    size_t depth = 0;
    size_t start = 0;
    for (size_t i = 0; i <= items.size(); ++i) {
        if (i == items.size() || (items[i] == ',' && depth == 0)) {
            std::string obj = items.substr(start, i - start);
            if (!obj.empty()) replay_one(stack, store, obj);
            start = i + 1;
        } else if (items[i] == '{') {
            ++depth;
        } else if (items[i] == '}') {
            if (depth > 0) --depth;
        }
    }
}

static void replay_one(CommandStack& stack, IStorage& store, const std::string& json) {
    const auto op = get_op(json);
    if (op == "add_key") {
        const auto track = get_string(json, "track_id");
        const auto id = get_string(json, "id");
        const auto interp = get_string(json, "interp");
        const auto value_json = get_string(json, "value_json");
        int t_ms = get_int(json, "t_ms");
        auto cmd = std::make_unique<AddKeyframeCommand>(track, t_ms, value_json, interp, id);
        stack.execute(std::move(cmd));
    } else if (op == "move") {
        int delta = get_int(json, "delta");
        // items with {"id":"...","orig_t_ms":N}
        std::vector<std::pair<std::string, int>> sel;
        const std::string tag = "\"items\":";
        auto p = json.find(tag);
        if (p != std::string::npos) {
            auto lb = json.find('[', p + tag.size());
            auto rb = json.rfind(']');
            if (lb != std::string::npos && rb != std::string::npos && rb > lb) {
                std::string items = json.substr(lb + 1, rb - lb - 1);
                size_t start = 0;
                for (size_t i = 0; i <= items.size(); ++i) {
                    if (i == items.size() || items[i] == ',') {
                        std::string obj = items.substr(start, i - start);
                        if (!obj.empty()) {
                            std::string id = get_string(obj, "id");
                            int orig = get_int(obj, "orig_t_ms");
                            sel.emplace_back(id, orig);
                        }
                        start = i + 1;
                    }
                }
            }
        }
        auto cmd = std::make_unique<MoveSelectionCommand>(std::move(sel), delta);
        stack.execute(std::move(cmd));
    } else if (op == "batch") {
        // Reconstruct a batch by executing items within begin/endBatch
        stack.beginBatch(get_string(json, "label"));
        try {
            replay_batch(stack, store, json);
            stack.endBatch();
        } catch (...) {
            // Ensure we end the batch to cleanup
            try { stack.endBatch(); } catch (...) {}
            throw;
        }
    } else {
        // Unknown op; ignore
    }
}

void restore_from_revisions(CommandStack& stack, IStorage& store, const std::vector<RevisionRecord>& records) {
    // Rebuild state by replaying all revisions in order
    for (const auto& r : records) {
        replay_one(stack, store, r.diff_json);
    }
}

} // namespace verity

