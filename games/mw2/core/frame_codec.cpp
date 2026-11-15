#include "frame_codec.h"
#include <cstring>

namespace specops4 {
namespace {
constexpr std::uint8_t writer[] = {
    0x41,0xb8,0x01,0x00,0x00,0x00, // mov r8d, 1: MSG_WriteBits width
    0x8b,0xd6,                     // mov edx, esi: player number
    0x48,0x8b,0xcf,                // mov rcx, rdi: frame message
    0xe8,0x4f,0x29,0xfd,0xff       // call MSG_WriteBits
};
constexpr std::uint8_t reader[] = {
    0x8d,0x50,0xfe,                // lea edx, [rax-2]; section type in eax is 3
    0x49,0x8d,0x4f,0x08,           // lea rcx, [r15+8]: frame message
    0xe8,0x3d,0x1d,0xe7,0xff,      // call MSG_ReadBits
    0x83,0xf8,0x01,                // cmp eax, 1: highest valid player number
    0x77,0x68                      // ja invalid_player
};
struct Site { std::uint32_t rva; const std::uint8_t* expected; std::size_t size; };
constexpr Site sites[] = {{0x22f1d1, writer, sizeof(writer)}, {0x2305c7, reader, sizeof(reader)}};
}

CodecResult patch_frame_codec(std::uint8_t* image, std::size_t size) {
    if (!image) return {false, 0, "null image"};
    for (const auto& site : sites) {
        if (site.rva > size || site.size > size - site.rva)
            return {false, site.rva, "site outside image"};
        if (std::memcmp(image + site.rva, site.expected, site.size))
            return {false, site.rva, "instruction signature mismatch; no changes made"};
    }
    image[0x22f1d3] = 2;    // WriteBits(player, 2)
    image[0x2305c9] = 0xff; // ReadBits(msg, sectionType - 1), i.e. 2 bits
    image[0x2305d5] = 3;    // allow player IDs 0..3
    return {true, 0, "frame codec widened; other four-player engine work still required"};
}
}
