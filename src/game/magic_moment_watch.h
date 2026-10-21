#pragma once

// Opens the portal menu when the game asks a player for a Skylander ("Player 1: place your
// Skylander or a vehicle on the Portal of Power"), and closes it again if the game stops waiting before a
// figure was chosen. Reads the game's CMagicMoment state from guest memory; see
// magic_moment_watch.cpp.

namespace rex::memory {
class Memory;
}  // namespace rex::memory

namespace superchargersrecomp {

// Starts the watcher thread. Call once, after the runtime is set up.
void StartMagicMomentWatch(rex::memory::Memory* memory);

// Stops and joins it; call before the runtime (and its guest memory) goes away.
void StopMagicMomentWatch();

// True while the game runs its figure sequence (asking for a Skylander, bringing one in): any
// CMagicMoment state but Inactive, as of the last poll.
bool MagicMomentActive();

}  // namespace superchargersrecomp
