#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace coop {
// Four-player lifecycle model. Transport and native avatar routing must adopt
// this model before the playable build can advertise four-player support.
class LobbyRoster {
public:
    enum class Phase { Lobby, Loading, Playing, Completing, Complete, Interrupted };
    struct Player {
        uint8_t id = 0;
        std::string key;
        bool available = false, ready = false, loaded = false, complete = false;
    };
    std::array<Player,4> players {};
    uint32_t epoch = 0;
    std::string mission;
    Phase phase = Phase::Lobby;
    explicit LobbyRoster(std::string host) { players[0] = {1, std::move(host)}; }
    size_t Count() const { size_t count = 0; for (const auto& p : players) if (p.id) ++count; return count; }
    uint8_t Join(const std::string& key) {
        if (key.empty()) return 0;
        for (const auto& p : players) if (p.id && p.key == key) return p.id;
        if (phase != Phase::Lobby) return 0;
        for (size_t i = 1; i < players.size(); ++i)
            if (!players[i].id) { players[i] = {uint8_t(i+1), key}; return players[i].id; }
        return 0;
    }
    Player* Find(uint8_t id) { return id && id <= 4 && players[id-1].id == id ? &players[id-1] : nullptr; }
    bool Leave(uint8_t id) {
        auto* p = Find(id);
        if (!p || id == 1) return false;
        *p = {};
        if (phase != Phase::Lobby && phase != Phase::Complete) phase = Phase::Interrupted;
        return true;
    }
    bool Select(const std::string& contract) {
        if (phase != Phase::Lobby || contract.empty() || contract.size() > 64) return false;
        mission = contract; ++epoch;
        for (auto& p : players) p.available = p.ready = p.loaded = p.complete = false;
        return true;
    }
    bool Ready(uint8_t id, uint32_t current, bool available, bool ready) {
        auto* p = Find(id);
        if (!p || current != epoch || !epoch || phase != Phase::Lobby) return false;
        p->available = available; p->ready = available && ready;
        return true;
    }
    bool BeginLoad() {
        if (phase != Phase::Lobby || mission.empty() || Count() < 2) return false;
        for (const auto& p : players) if (p.id && (!p.available || !p.ready)) return false;
        phase = Phase::Loading; return true;
    }
    bool Loaded(uint8_t id, uint32_t current) {
        auto* p = Find(id);
        if (!p || current != epoch || phase != Phase::Loading) return false;
        p->loaded = true;
        for (const auto& member : players) if (member.id && !member.loaded) return true;
        phase = Phase::Playing; return true;
    }
    bool Completed(uint8_t id, uint32_t current) {
        auto* p = Find(id);
        if (!p || current != epoch || (phase != Phase::Playing && phase != Phase::Completing)) return false;
        p->complete = true; phase = Phase::Completing;
        for (const auto& member : players) if (member.id && !member.complete) return true;
        phase = Phase::Complete; return true;
    }
    bool ReturnToLobby() {
        if (phase != Phase::Complete && phase != Phase::Interrupted) return false;
        phase = Phase::Lobby;
        for (auto& p : players) p.ready = p.loaded = p.complete = false;
        return true;
    }
    bool SceneLost(uint8_t id, uint32_t current) {
        if (!Find(id) || current != epoch ||
            (phase != Phase::Loading && phase != Phase::Playing && phase != Phase::Completing))
            return false;
        phase = Phase::Interrupted;
        return true;
    }
};
}
