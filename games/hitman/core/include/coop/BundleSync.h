#pragma once

#include "CoopFixes.h"

namespace coop {

// Mission events have no built-in epoch: scope every bag operation explicitly.
struct BundleMessage {
    uint32_t epoch = 0;
    std::string scene;
    uint64_t id = 0;
    std::string outfit;
    int charset = 0, variation = 0;
    bool modifiers = false;
    Transform xf;

    std::string Encode(bool spawn) const {
        std::ostringstream out;
        out << epoch << ' ' << std::quoted(scene) << ' ' << id;
        if (spawn) out << ' ' << std::quoted(outfit) << ' ' << charset << ' ' << variation << ' ' << modifiers
            << ' ' << std::setprecision(9) << xf.pos.x << ' ' << xf.pos.y << ' ' << xf.pos.z
            << ' ' << xf.rot.x << ' ' << xf.rot.y << ' ' << xf.rot.z << ' ' << xf.rot.w;
        return out.str();
    }
    static bool Decode(const std::string& text, bool spawn, BundleMessage& result) {
        BundleMessage v;
        std::istringstream in(text);
        if (!(in >> v.epoch >> std::quoted(v.scene) >> v.id) || !v.epoch || !v.id ||
            v.scene.empty() || v.scene.size() > 256 || (v.id >> 56) < 1 || (v.id >> 56) > 2) return false;
        if (spawn) {
            int mods = -1;
            if (!(in >> std::quoted(v.outfit) >> v.charset >> v.variation >> mods >> v.xf.pos.x >> v.xf.pos.y >> v.xf.pos.z
                  >> v.xf.rot.x >> v.xf.rot.y >> v.xf.rot.z >> v.xf.rot.w) || v.charset < 0 || v.charset > 255 ||
                v.variation < 0 || v.variation > 255 || mods < 0 || mods > 1) return false;
            // Reuse the strict GUID/finite-position validator.
            TeamExitRequest check { v.epoch, v.scene, v.outfit, v.xf.pos }, validated;
            if (!TeamExitRequest::Decode(check.Encode(), validated)) return false;
            for (float n : {v.xf.pos.x, v.xf.pos.y, v.xf.pos.z}) if (std::fabs(n) > 100000.f) return false;
            const float norm = Dot(v.xf.rot, v.xf.rot);
            if (!std::isfinite(norm) || norm < 0.25f || norm > 4.f) return false;
            v.xf.rot = Normalize(v.xf.rot);
            v.modifiers = mods != 0;
        }
        in >> std::ws;
        if (!in.eof()) return false;
        result = v;
        return true;
    }
    bool Matches(uint32_t currentEpoch, const std::string& currentScene) const {
        return epoch == currentEpoch && scene == currentScene;
    }
};

// A consumed bag never comes back from a duplicate or delayed spawn in this scene.
struct BundleLedger {
    std::unordered_set<uint64_t> live, removed;
    bool CanSpawn(uint64_t id) const { return id && !live.contains(id) && !removed.contains(id); }
    void Spawned(uint64_t id) { live.insert(id); }
    void Removed(uint64_t id) { live.erase(id); removed.insert(id); }
};

// Feed forward a moving drag target's velocity instead of making position error
// grow before the body can catch up. Keep the native impulse controller bounded.
inline Vec3 DragTargetVelocity(Vec3 previous, Vec3 next, double dt) {
    if (dt < 0.001 || dt > 0.2) return {};
    Vec3 velocity = (next - previous) * float(1.0 / dt);
    velocity.z = 0.f;
    const float speed = Length(velocity);
    return speed > 5.f ? Vec3{} : velocity; // discontinuity, not motion to chase
}
inline Vec3 DragWantedVelocity(Vec3 error, Vec3 targetVelocity, float maxSpeed) {
    Vec3 wanted { targetVelocity.x + error.x * 4.f, targetVelocity.y + error.y * 4.f, 0.f };
    const float speed = Length(wanted);
    if (speed > maxSpeed) wanted = wanted * (maxSpeed / speed);
    return wanted;
}
}
