#pragma once
#include "PartyAvatarPlayback.h"

namespace coop {
// Explicit synthetic test participant. It has no game or NPC simulation and
// mirrors only bounded avatar/pose/outfit/skeleton packets from one real source.
class PartyReplayClient {
public:
    PartyReplayClient(std::unique_ptr<PartyRoom> room, Vec3 offset, uint8_t source=1);
    void Tick(double now);
    void Leave(double now);
    const PartyRoomStatus& Status() const { return m_Room->Status(); }
    uint64_t Forwarded() const { return m_Forwarded; }
private:
    std::unique_ptr<PartyRoom> m_Room;
    Vec3 m_Offset;
    uint8_t m_Source;
    double m_NextReady=0, m_NextLoaded=0;
    uint64_t m_Forwarded=0;
    bool Forward(const PartyGameEvent&);
};
}
