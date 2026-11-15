#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace mw2 {
struct MissionRaster {
  int width=0,height=0;
  std::vector<std::uint8_t> bgra;
  bool available() const { return width>0&&height>0&&!bgra.empty(); }
};
// Supports the current Steam build's raw 24-bit, 32-byte-header IWi v8 images.
// Validates all lengths before decoding; unsupported variants fail gracefully.
MissionRaster decodeMissionIwi(const std::vector<std::uint8_t>& bytes);
std::vector<MissionRaster> readMissionImages(const std::filesystem::path& game,
                                           const std::vector<std::string>& assets);
}
