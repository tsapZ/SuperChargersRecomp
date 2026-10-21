#pragma once

// Careful reads of guest memory from host threads: every address is checked to be committed,
// readable guest memory first, so a stale or wrong pointer yields nullopt instead of a crash.

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>

#include <rex/system/xmemory.h>

namespace superchargersrecomp {

inline bool GuestReadable(rex::memory::Memory* memory, uint32_t address, uint32_t size) {
  rex::memory::BaseHeap* heap = memory->LookupHeap(address);
  rex::memory::HeapAllocationInfo info{};
  return heap && heap->QueryRegionInfo(address, &info) &&
         (info.state & rex::memory::kMemoryAllocationCommit) &&
         (info.protect & rex::memory::kMemoryProtectRead) &&
         uint64_t(address) + size <= uint64_t(info.base_address) + info.region_size;
}

// A big-endian u32.
inline std::optional<uint32_t> ReadGuest32(rex::memory::Memory* memory, uint32_t address) {
  if (!GuestReadable(memory, address, 4)) return std::nullopt;
  uint32_t value;
  std::memcpy(&value, memory->TranslateVirtual<const uint8_t*>(address), 4);
  return rex::byte_swap(value);
}

// A NUL-terminated string of at most `max_length` characters.
inline std::optional<std::string> ReadGuestString(rex::memory::Memory* memory, uint32_t address,
                                                  uint32_t max_length = 256) {
  rex::memory::BaseHeap* heap = memory->LookupHeap(address);
  rex::memory::HeapAllocationInfo info{};
  if (!heap || !heap->QueryRegionInfo(address, &info) ||
      !(info.state & rex::memory::kMemoryAllocationCommit) ||
      !(info.protect & rex::memory::kMemoryProtectRead)) {
    return std::nullopt;
  }
  const uint64_t region_end = uint64_t(info.base_address) + info.region_size;
  if (address >= region_end) return std::nullopt;
  const uint64_t available = std::min<uint64_t>(max_length, region_end - address);
  const char* text = memory->TranslateVirtual<const char*>(address);
  const size_t length = strnlen(text, size_t(available));
  if (length == available) return std::nullopt;  // no terminator within reach
  return std::string(text, length);
}

}  // namespace superchargersrecomp
