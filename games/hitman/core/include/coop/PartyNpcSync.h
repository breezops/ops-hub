#pragma once
#include "PartyNpcAuthority.h"
#include "Interpolation.h"
#include <set>

namespace coop {
// Network/native boundary for the experimental NPC core. Not instantiated by
// the released preview runtime. An IPartyNpcWorld adapter must enforce native
// freeze/apply/resume before this can be enabled in HITMAN.
class PartyNpcSync {
public:
    PartyNpcSync(PartyRoom& room, IPartyNpcWorld& world, PartyNpcOptions options = {});
    ~PartyNpcSync();
    bool Start();
    bool Register(uint64_t npc, const PartyNpcState&, double now);
    bool ObservePlayer(PartyIdentity, Vec3, double now);
    bool Propose(uint64_t npc, PartyIdentity next, double now);
    bool Receive(const PartyGameEvent&, double now);
    void Tick(double now, bool allowOwnershipChanges=true);
    void Stop();
    bool Active() const { return m_Active; }
    bool Failed() const { return m_Failed; }
    bool StopConfirmed() const { return !m_Replica || m_Replica->StopConfirmed(); }
    const PartyNpcAuthority* Authority() const { return m_Host.get(); }
private:
    PartyRoom& m_Room;
    IPartyNpcWorld& m_World;
    PartyNpcOptions m_Options;
    PartyNpcScope m_Scope;
    PartyIdentity m_Self;
    std::unique_ptr<PartyNpcAuthority> m_Host;
    std::unique_ptr<PartyNpcReplica> m_Replica;
    std::set<uint64_t> m_Known;
    std::map<uint8_t,PartyNpcLocation> m_Locations;
    struct PresentationFrame {
        Transform transform;
        Pose pose;
        friend PresentationFrame Lerp(const PresentationFrame& a,const PresentationFrame& b,float t) {
            return {coop::Lerp(a.transform,b.transform,t),coop::Lerp(a.pose,b.pose,t)};
        }
    };
    struct PoseStream {
        TimedBuffer<PresentationFrame> buffer;
        uint64_t generation=0;
        uint8_t owner=0;
        uint32_t timeMs=0;
        double received=0;
        bool applied=false;
    };
    std::map<uint64_t,PoseStream> m_Poses;
    std::vector<uint64_t> m_PoseSources;
    double m_NextPose=0;
    double m_NextReport=0, m_NextPolicy=0, m_NextRefresh=0;
    std::array<std::array<uint64_t,kMaxAutomaticGuardsPerGuest>,4> m_AutomaticGuards {};
    bool m_Active=false, m_Failed=false;
    bool Current() const;
    bool ApplyHost(const PartyNpcMessage&);
    bool ReceivePose(const PartyGameEvent&,double now);
    void SendPoses(double now);
    void TickPoses(double now);
    void ConsiderAutomaticGuards(double now);
    void Flush(double now);
    void Fail();
};
}
