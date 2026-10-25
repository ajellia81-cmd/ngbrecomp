#pragma once
#include <cstdint>

namespace gpu {
// Two frame slots share one allocation. A draw must fit in one slot, not
// merely inside the entire allocation, or Upload() can reset its uniforms.
inline constexpr uint64_t kUploadSlotBytes = 32u << 20;
inline constexpr uint64_t kUploadRingBytes = 2 * kUploadSlotBytes;
inline constexpr bool UploadRingFits(uint64_t position, uint64_t bytes) {
    return bytes <= kUploadSlotBytes && position <= kUploadSlotBytes - bytes;
}
}
