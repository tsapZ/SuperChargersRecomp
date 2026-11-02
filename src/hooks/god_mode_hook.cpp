// No health loss, as a cheat (Spyro's Adventure's, Giants' and Trap Team's setting name).
//
// SuperChargers has a programmer's god mode of its own: the debug variable "GodMode" in its
// PROGRAMMER_VARS group, a byte at 0x8376C5FB (0, off), registered by name in the game's debug
// variable list. The damage check sub_829E7300 reads it first. With it set, the check zeroes the
// damage of a victim with the players' mark (bit 0x40000 of the word at +92 set, bit 0x2 of the
// byte at +216 clear) and gives its own reason text, "Damage reduced when in God-Mode"; enemies
// don't have the mark and lose health as before. The god mode still lets a hit through whose
// description forces the damage (its byte +100: lava, burning; in Imaginators' lava arena that took
// the HUD to 100 of 450); the cheat doesn't.
//
// Three midasm hooks (config/default.toml), doing nothing while cheat_no_health_loss is off:
// - 0x829E733C: the compare right after the GodMode byte is read; reports it as set.
// - 0x829E7360: the compare of the hit's force byte, reached only for a spared victim; reports 0,
//   and that 0 is what the next block writes as the damage.
// - 0x829E7368: the start of that block; one log line the first time a hit is kept off.
//
// Found 2026-11-02 from the reason text in the image (the developer kit's gen_xref refs); the same
// check is in Swap Force, SuperChargers and Imaginators. Checked in a kit sweep: nat2, sky1, nat3,
// 150 s each of random play: all 60 kept-off hits were on the player's Skylander, the two enemies
// hit took theirs, the HUD stayed at 450 of 450. Nothing is written to the game's memory.

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>

REXCVAR_DEFINE_BOOL(cheat_no_health_loss, false, "Extras",
                    "Skylanders don't lose health when they get hit (achievements don't unlock while it's on)");

// cmplwi cr6,r10,0 right after lbz r10 read the GodMode byte.
void GodModeFlagHook(PPCRegister& r10) {
  if (REXCVAR_GET(cheat_no_health_loss)) r10.u64 = 1;
}

// cmplwi cr6,r9,0 after lbz r9 read the hit's "force" byte (lava, burning): the god mode lets such
// hits through, the cheat doesn't. Only a victim the god mode spares gets this far, and r9 is the 0
// the next block writes as the damage.
void GodModeForcedHook(PPCRegister& r9) {
  if (REXCVAR_GET(cheat_no_health_loss)) r9.u64 = 0;
}

// The first instruction of the block that zeroes a spared victim's damage.
void GodModeKeptOffHook() {
  static bool logged = false;
  if (!logged) {
    logged = true;
    REXLOG_INFO("Extras: no health loss kept a hit's damage off (the game's own god mode)");
  }
}
