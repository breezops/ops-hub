#pragma once
#include <cstddef>
#include <cstdint>

namespace specops4 {
// Component only: callers must relocate player storage and finish session/mission
// support before using this codec in a running game. Vanilla peers are incompatible.
struct CodecResult {
    bool ok;
    std::uint32_t failed_rva;
    const char* message;
};

// Takes an RVA-indexed, writable private image. Does not change protections, load
// the game, or locate another process. All sites validate before the first write.
CodecResult patch_frame_codec(std::uint8_t* image, std::size_t size);
}
