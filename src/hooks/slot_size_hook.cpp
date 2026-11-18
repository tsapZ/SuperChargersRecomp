// The hero data and vehicle slot sizes as settings, for mods with bigger characters (Core of Light #56, 2026-11-18:
// SkyP4tcher's superpatcher turns the files of the player's own Wii U SuperChargers into Bowser, Donkey Kong and their
// vehicles for this game; their data doesn't fit the slots the game reserves, and the game crashed loading them until
// the tool patched two constants in the exe, which every update replaced again).
//
// Two of the game's static initializers set the sizes once at startup: sub_836C38A0 writes 0x179800 to 0x8373D548 and
// 0x8373D54C (the hero data slot), sub_836C3938 writes 0x1C2000 to 0x8373D478 and 0x8373D47C (the vehicle slot), and
// sub_836C3B70 then copies both pairs into the game's memory budget table (its +112 and +96), which the allocations
// follow later. Found from the tool's exe patch: its two byte signatures land in exactly these initializers (checked
// against the release build 2026-11-18). A setting above the game's size is written over both words right after the
// initializer ran; 0 (the default) and anything not above the game's size leave the game as it is. A bigger slot only
// takes that much more of the game's heap (the tool's author played with 0x279800 and 0x2C2000, +1 MB each).

#include <cstdint>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/memory/utils.h>
#include <rex/ppc/context.h>

REXCVAR_DEFINE_INT32(hero_data_slot_size, 0, "Mods",
                     "Bytes for a hero's data slot, for mods with bigger characters (Bowser and Donkey Kong from "
                     "the Wii U game: 0x279800); 0 = the game's own 0x179800");
REXCVAR_DEFINE_INT32(vehicle_slot_size, 0, "Mods",
                     "Bytes for a vehicle's slot, for mods with bigger vehicles (the Clown Cruiser and the Barrel "
                     "Blaster: 0x2C2000); 0 = the game's own 0x1C2000");

namespace {

constexpr uint32_t kHeroSlot = 0x8373D548;
constexpr uint32_t kVehicleSlot = 0x8373D478;

// Both words of a size pair, when the setting asks for more than the game wrote.
void Enlarge(uint8_t* base, uint32_t at, int32_t wanted, const char* what) {
  const uint32_t game = rex::memory::load_and_swap<uint32_t>(base + at);
  if (wanted <= 0 || uint32_t(wanted) <= game) return;
  rex::memory::store_and_swap<uint32_t>(base + at, uint32_t(wanted));
  rex::memory::store_and_swap<uint32_t>(base + at + 4, uint32_t(wanted));
  REXLOG_INFO("Mods: {} slot {:#x} bytes (the game's {:#x})", what, uint32_t(wanted), game);
}

}  // namespace

extern "C" void __imp__sub_836C38A0(PPCContext& __restrict ctx, uint8_t* base);
extern "C" void __imp__sub_836C3938(PPCContext& __restrict ctx, uint8_t* base);

REX_HOOK_RAW(sub_836C38A0) {
  __imp__sub_836C38A0(ctx, base);
  Enlarge(base, kHeroSlot, REXCVAR_GET(hero_data_slot_size), "hero data");
}

REX_HOOK_RAW(sub_836C3938) {
  __imp__sub_836C3938(ctx, base);
  Enlarge(base, kVehicleSlot, REXCVAR_GET(vehicle_slot_size), "vehicle");
}
