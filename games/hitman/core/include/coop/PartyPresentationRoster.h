#pragma once
#include "PartyRoom.h"

namespace coop {
// A native presentation scope, separate from admission. One scene can retain
// native stand-ins for one room/local identity; swapping rooms requires Reset
// during scene cleanup so old actors cannot become the new local player.
class PartyPresentationRoster {
public:
    bool Update(const PartyRoomStatus& status) {
        if (!status.session || !status.self || status.self > 4 ||
            status.members.empty() || status.members.size() > 4 ||
            status.host != (status.self == 1) ||
            (status.state == PartyRoomStatus::State::Hosting) != status.host ||
            (status.state != PartyRoomStatus::State::Hosting && status.state != PartyRoomStatus::State::Connected))
            return false;
        if (m_Status.session && (status.session != m_Status.session || status.self != m_Status.self ||
            status.revision < m_Status.revision || status.epoch < m_Status.epoch))
            return false;
        bool seen[5] {};
        for (const auto& member : status.members) {
            if (!member.id || member.id > 4 || !member.incarnation || seen[member.id])
                return false;
            seen[member.id] = true;
        }
        if (!seen[1] || !seen[status.self])
            return false;
        if (status.phase == LobbyRoster::Phase::Playing) {
            if (!status.epoch || status.mission != kPartyFreeformContract)
                return false;
            for (const auto& member : status.members)
                if (!member.loaded)
                    return false;
        }
        m_Status = status;
        return true;
    }
    const PartyMember* Remote(uint8_t id, uint64_t incarnation, uint32_t epoch) const {
        if (m_Status.phase != LobbyRoster::Phase::Playing || epoch != m_Status.epoch || id == m_Status.self)
            return nullptr;
        for (const auto& member : m_Status.members)
            if (member.id == id && member.incarnation == incarnation)
                return &member;
        return nullptr;
    }
    const PartyRoomStatus& Status() const { return m_Status; }
    void Reset() { m_Status = {}; }
private:
    PartyRoomStatus m_Status;
};
}
