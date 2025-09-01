#include "commands/add_keyframe.hpp"
#include "verity/db.hpp"
#include <random>

namespace verity {

static std::string make_uuid_like() {
    // simple pseudo-uuid for demo purposes
    static std::mt19937_64 rng{12345};
    uint64_t a = rng();
    uint64_t b = rng();
    char buf[37];
    snprintf(buf, sizeof(buf), "%08x-%04x-%04x-%04x-%012llx",
             (unsigned)(a & 0xffffffff), (unsigned)((a >> 32) & 0xffff), (unsigned)((a >> 48) & 0xffff),
             (unsigned)(b & 0xffff), (unsigned long long)((b >> 16) & 0xffffffffffffull));
    return std::string(buf);
}

AddKeyframeCommand::AddKeyframeCommand(std::string track_id, int t_ms, std::string value_json, std::string interp)
    : track_id_(std::move(track_id)), t_ms_(t_ms), value_json_(std::move(value_json)), interp_(std::move(interp)) {}

AddKeyframeCommand::AddKeyframeCommand(std::string track_id, int t_ms, std::string value_json, std::string interp, std::string fixed_id)
    : track_id_(std::move(track_id)), t_ms_(t_ms), value_json_(std::move(value_json)), interp_(std::move(interp)), key_id_(std::move(fixed_id)) {}

void AddKeyframeCommand::doAction(IStorage& store) {
    if (key_id_.empty()) key_id_ = make_uuid_like();
#if VERITY_DESKTOP_SQLITE
    if (auto* sql = dynamic_cast<verity::SqliteStorage*>(&store)) {
        sql->insertKeyframe(key_id_, track_id_, t_ms_, value_json_, interp_);
        return;
    }
#endif
    // NullStorage path: do nothing (in-memory only)
}

void AddKeyframeCommand::undoAction(IStorage& store) {
    (void)store;
#if VERITY_DESKTOP_SQLITE
    if (auto* sql = dynamic_cast<verity::SqliteStorage*>(&store)) {
        sql->deleteKeyframe(key_id_);
        return;
    }
#endif
}

std::optional<std::string> AddKeyframeCommand::diffJson() const {
    auto escape = [](const std::string& s) {
        std::string out;
        out.reserve(s.size() + 8);
        for (char c : s) {
            switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
            }
        }
        return out;
    };
    return std::string("{\"op\":\"add_key\",\"track_id\":\"") + track_id_ + "\",\"t_ms\":" +
           std::to_string(t_ms_) + ",\"id\":\"" + key_id_ + "\",\"interp\":\"" + interp_ +
           "\",\"value_json\":\"" + escape(value_json_) + "\"}";
}

} // namespace verity
