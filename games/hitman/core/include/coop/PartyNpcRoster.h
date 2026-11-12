#pragma once

#include "PartyNpcAuthority.h"
#include <span>

namespace coop {
// A match is identity evidence. Native adapters must still validate the local
// actor instance and establish simulation ownership before applying movement.
class PartyNpcRoster {
public:
    static constexpr uint8_t Packet = 0xD0;
    explicit PartyNpcRoster(PartyRoom& room) : m_Room(room) {}
    void Update(std::span<const uint64_t> localIds);
    void Tick();
    bool Receive(const PartyGameEvent& event);
    const std::optional<std::vector<uint64_t>>& Shared() const;
private:
    static constexpr size_t IdsPerPage = 120;
    static constexpr size_t MaxIds = 500;
    static constexpr size_t MaxPages = (MaxIds + IdsPerPage - 1) / IdsPerPage;
    struct Snapshot {
        uint32_t revision = 0;
        uint8_t pageCount = 0;
        std::array<std::optional<std::vector<uint64_t>>, MaxPages> pages;
        std::optional<std::vector<uint64_t>> ids;
    };
    struct Outgoing {
        uint32_t revision = 0;
        std::optional<std::vector<uint64_t>> ids;
        bool sent = false;
    };
    PartyRoom& m_Room;
    PartyNpcScope m_Scope;
    std::array<Snapshot,4> m_Reports;
    Snapshot m_HostReport;
    Outgoing m_Local, m_Broadcast;
    std::optional<std::vector<uint64_t>> m_Shared;
    bool RefreshScope();
    void UpdateShared();
    void Send(Outgoing& outgoing, PartyChannel channel);
    static void Replace(Outgoing& outgoing, std::vector<uint64_t> ids);
};
}
