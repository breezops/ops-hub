#pragma once

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_set>
#include "Math.h"

namespace coop {

// Native naked appearance and availability of the original outfit are separate bits.
inline bool BodyHasClothes(bool hasClothOutfit, bool nude) {
    return hasClothOutfit && !nude;
}
inline bool CanStreamVictim(bool down, size_t boneCount) {
    return down || boneCount >= 2;
}

// A body may already have been stripped before its first inventory scan.
// Taking its original outfit is irreversible for the lifetime of that scene.
struct BodyClothingState {
    bool known = false;
    bool taken = false;
    bool Observe(bool hasClothes) {
        const bool announce = !hasClothes && !taken;
        known = true;
        taken = taken || !hasClothes;
        return announce;
    }
    void AcceptPeer(bool stripped) { known = true; taken = taken || stripped; }
};

// A reliable event is still stale after a restart unless its mission epoch matches.
struct TeamExitRequest {
    uint32_t epoch = 0;
    std::string scene;
    std::string repositoryId;
    Vec3 at;
    std::string Encode() const {
        std::ostringstream out;
        out << epoch << ' ' << std::quoted(scene) << ' ' << std::quoted(repositoryId)
            << ' ' << std::setprecision(9) << at.x << ' ' << at.y << ' ' << at.z;
        return out.str();
    }
    static bool Decode(const std::string& text, TeamExitRequest& result) {
        TeamExitRequest value;
        std::istringstream in(text);
        if (!(in >> value.epoch >> std::quoted(value.scene) >> std::quoted(value.repositoryId)
              >> value.at.x >> value.at.y >> value.at.z)) return false;
        in >> std::ws;
        if (!in.eof() || !value.epoch || value.scene.empty() || value.scene.size() > 256 ||
            value.repositoryId.size() != 36 || !std::isfinite(value.at.x) ||
            !std::isfinite(value.at.y) || !std::isfinite(value.at.z)) return false;
        for (size_t i = 0; i < value.repositoryId.size(); ++i) {
            const char c = value.repositoryId[i];
            if (i == 8 || i == 13 || i == 18 || i == 23) { if (c != '-') return false; }
            else if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
        }
        result = value;
        return true;
    }
    bool Matches(uint32_t currentEpoch, const std::string& currentScene) const {
        return epoch == currentEpoch && scene == currentScene;
    }
    bool Near(const Vec3& local, float radius = 5.f) const {
        return std::isfinite(local.x) && std::isfinite(local.y) && std::isfinite(local.z) &&
            Distance(local, at) <= radius;
    }
};
}
