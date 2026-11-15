#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <fstream>
#include <iterator>
#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstring>
#include "frame_codec.h"
#pragma comment(lib, "bcrypt.lib")

static bool expect(bool ok, const char* name) {
    std::cout << (ok ? "PASS " : "FAIL ") << name << '\n';
    return ok;
}

int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "Usage: codec_check.exe <original iw4sp.exe>\n"; return 2; }
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) { std::cerr << "Cannot read executable\n"; return 2; }
    std::vector<std::uint8_t> file((std::istreambuf_iterator<char>(input)), {});
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return 2;
    unsigned char hash[32];
    const auto status = BCryptHash(alg, nullptr, 0, file.data(), static_cast<ULONG>(file.size()), hash, sizeof(hash));
    BCryptCloseAlgorithmProvider(alg, 0);
    if (status < 0) return 2;
    std::ostringstream digest;
    for (auto byte : hash) digest << std::hex << std::setw(2) << std::setfill('0') << unsigned(byte);
    if (!expect(digest.str() == "2f8c9cfda1b8c0fb308fd9719577154e8222fc7654f5e8f1bf54579bca69d805", "exact supported executable SHA-256")) return 1;
    // Hash validation above identifies one exact trusted layout before parsing it.
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(file.data());
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(file.data() + dos->e_lfanew);
    std::vector<std::uint8_t> image(nt->OptionalHeader.SizeOfImage, 0);
    std::memcpy(image.data(), file.data(), nt->OptionalHeader.SizeOfHeaders);
    const auto* sections = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        const auto& s = sections[i];
        std::memcpy(image.data() + s.VirtualAddress, file.data() + s.PointerToRawData, s.SizeOfRawData);
    }
    const auto original = image;
    bool ok = true;
    // A late-site mismatch must leave the earlier writer entirely untouched.
    image[0x2305c7] ^= 1;
    auto before = image;
    auto result = specops4::patch_frame_codec(image.data(), image.size());
    ok &= expect(!result.ok && result.failed_rva == 0x2305c7 && image == before, "late signature mismatch is atomic");
    image = original;
    result = specops4::patch_frame_codec(image.data(), 0x2305c7);
    ok &= expect(!result.ok && image == original, "truncated image is refused without writes");
    ok &= expect(!specops4::patch_frame_codec(nullptr, 0).ok, "null image is refused");
    result = specops4::patch_frame_codec(image.data(), image.size());
    ok &= expect(result.ok, "both real executable instruction signatures match");
    std::vector<std::size_t> changed;
    for (std::size_t i = 0; i < image.size(); ++i) if (image[i] != original[i]) changed.push_back(i);
    ok &= expect(changed == std::vector<std::size_t>{0x22f1d3, 0x2305c9, 0x2305d5}, "only the three intended immediate bytes change");
    before = image;
    ok &= expect(!specops4::patch_frame_codec(image.data(), image.size()).ok && image == before, "repeat patch is refused without writes");
    std::cout << "Private memory only. No game files written, process launched, or runtime compatibility established.\n";
    return ok ? 0 : 1;
}
