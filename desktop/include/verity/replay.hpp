#pragma once

#include "verity/command.hpp"
#include <vector>

namespace verity {

// Rehydrate undo stack from stored revisions (best-effort for built-in commands)
// Appends reconstructed commands to the CommandStack's undo history.
void restore_from_revisions(CommandStack& stack, IStorage& store, const std::vector<RevisionRecord>& records);

} // namespace verity

