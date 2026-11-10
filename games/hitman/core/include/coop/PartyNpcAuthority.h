#pragma once
#include "PartyRoom.h"
#include <map>
#include <deque>
#include <optional>
#include <set>

namespace coop {
enum class PartyNpcLife : uint8_t { Alive, Unconscious, Dead };
struct PartyNpcState {
    Transform xf;
    PartyNpcLife life = PartyNpcLife::Alive;
    bool stripped = false;
    bool engaged = false;
    bool interacting = false;
    bool operator==(const PartyNpcState&) const;
};
struct PartyNpcScope {
    uint64_t session = 0;
    uint32_t epoch = 0;
    std::array<uint64_t, 4> incarnations {};
    static bool FromRoom(const PartyRoomStatus&, PartyNpcScope&);
    bool Contains(PartyIdentity) const;
    bool operator==(const PartyNpcScope&) const = default;
};
// Command authority is supplied by the transport envelope, not by these fields.
enum class PartyNpcKind : uint8_t { Commit=1, Quiesce, Released, Prepare, Prepared, Report, Canonical, Freeze, Declined };
struct PartyNpcMessage {
    PartyNpcKind kind = PartyNpcKind::Freeze;
    uint64_t session = 0, npc = 0, generation = 0, revision = 0, sequence = 0;
    uint32_t epoch = 0;
    PartyIdentity owner;
    PartyNpcState state;
};
struct PartyNpcDelivery {
    uint8_t target = 0; // zero: host broadcasts; otherwise one scoped peer
    PartyNpcMessage message;
};
struct PartyNpcLocation { PartyIdentity peer; Vec3 position; double observed = 0; };
enum class PartyNpcPolicy : uint8_t { HostOnly, NearestPlayer };
struct PartyNpcOptions {
    // Initial native integration keeps simulation on the host. Lease transfer
    // remains available explicitly for development and a later native adapter.
    PartyNpcPolicy policy = PartyNpcPolicy::HostOnly;
    size_t capacity = 1024, queueLimit = 4096;
    double retry = 0.25, transferTimeout = 3., minimumHold = 2., locationMaxAge = 1., baselineInterval=1.;
    float hysteresis = 5.f, delegationRadius = 50.f;
    // Private builds can bind a fixed catalog of stable entity IDs. All peers
    // in such a build must use the same catalog and a distinct room build ID.
    std::set<uint64_t> fixedCatalog;
    bool ValidCatalog() const {
        return fixedCatalog.empty() || (policy==PartyNpcPolicy::HostOnly &&
            fixedCatalog.size()<=capacity && !fixedCatalog.contains(0));
    }
    bool AcceptsNpc(uint64_t npc) const {
        return npc && (fixedCatalog.empty() || fixedCatalog.contains(npc));
    }
};

std::vector<uint8_t> EncodePartyNpc(const PartyNpcMessage&);
bool DecodePartyNpc(const std::vector<uint8_t>&, PartyNpcMessage&);
bool ValidPartyNpcState(const PartyNpcState&);

class PartyNpcAuthority {
public:
    enum class Phase { Active, Releasing, Preparing, Suspended };
    struct Record {
        PartyIdentity owner;
        uint64_t generation=1, revision=1, reportSequence=0, issuedGeneration=1;
        PartyNpcState state;
        Phase phase=Phase::Active;
        PartyIdentity next;
        uint64_t nextGeneration=0;
        double changed=0, deadline=0, retryAt=0, snapshotAt=0;
        bool confirmed=false;
    };
    explicit PartyNpcAuthority(PartyNpcScope, PartyNpcOptions = {});
    bool Register(uint64_t npc, const PartyNpcState&, double now);
    // Host policy only: guests cannot directly select the next owner.
    bool Propose(uint64_t npc, PartyIdentity next, double now);
    bool ConsiderNearest(uint64_t npc, const std::vector<PartyNpcLocation>&, double now);
    bool Receive(PartyIdentity source, const PartyNpcMessage&, double now);
    void Tick(double now);
    void Stop();
    bool Pop(PartyNpcDelivery&);
    const Record* Find(uint64_t npc) const;
    bool Failed() const { return m_Failed; }
    const PartyNpcScope& Scope() const { return m_Scope; }
private:
    PartyNpcScope m_Scope;
    PartyNpcOptions m_Options;
    std::map<uint64_t, Record> m_Npcs;
    std::deque<PartyNpcDelivery> m_Out;
    bool m_Failed=false, m_Stopped=false;
    bool Emit(uint8_t target, uint64_t npc, PartyNpcKind, const Record&);
    bool Adopt(Record&, const PartyNpcState&);
    void Suspend(uint64_t npc, Record&);
};

// Native implementations must return true from Freeze only after the NPC's
// simulation is actually stopped, and true from Apply only after applying the
// canonical baseline. The old void SetNpcPuppet API is not sufficient proof.
class IPartyNpcWorld {
public:
    enum class CatalogPreparation { Pending, Ready, Refused };
    virtual ~IPartyNpcWorld() = default;
    // Before the room acknowledges Loading: hold all local NPC simulation,
    // including actors entering the pool later, until individual leases resume
    // it. True requires a real worker fence, not just queued puppet requests.
    // Default refuses native startup for adapters that have not implemented it.
    virtual bool PrepareNpcScene() { return false; }
    // Ready requires a held and drained local instance of every listed actor.
    // The native adapter checks unique identity, birth, and its own hold.
    virtual CatalogPreparation PrepareNpcCatalog(const std::set<uint64_t>&) { return CatalogPreparation::Refused; }
    // Called even if loading ended before the first commit. Revoke pending
    // native writes/resumes and retain teardown evidence for this catalog.
    virtual void StopNpcCatalog() {}
    // Notification from the pre-clear hook, NOT a completed teardown. Preserve
    // enough native identity/worker state to verify this token after clearing.
    // The token is process-unique across rooms; zero is never issued.
    virtual void BeginNpcSceneRetirement(uint64_t) {}
    // Only true after the native scene and its outstanding jobs are retired.
    // IsInScene=false, queued cancellation or entry into ClearScene is not proof.
    // Must recognize the exact token, including after another scene has loaded.
    virtual bool ConfirmNpcSceneRetired(uint64_t) { return false; }
    virtual bool FreezeNpc(uint64_t) = 0;
    virtual bool ApplyNpcBaseline(uint64_t, const PartyNpcState&) = 0;
    virtual bool ResumeNpc(uint64_t) = 0;
    virtual bool CaptureNpc(uint64_t, PartyNpcState&) = 0;
};
class PartyNpcReplica {
public:
    PartyNpcReplica(PartyNpcScope scope, PartyIdentity self, IPartyNpcWorld& world, size_t limit=1024);
    ~PartyNpcReplica();
    // Caller passes the actual validated transport envelope origin and channel.
    bool Receive(PartyIdentity source, PartyChannel channel, const PartyNpcMessage&);
    bool Report(uint64_t npc, bool refresh=false);
    void RetryPending(); // Actor/template may become available after its catalog entry.
    bool Pop(PartyNpcMessage&);
    bool Running(uint64_t npc) const;
    bool Stopped() const { return m_Stopped; }
    bool Stop();
    bool StopConfirmed() const { return m_StopConfirmed; }
private:
    struct CommitProgress {
        PartyNpcMessage message;
        enum class Step { Freeze, Apply, Resume } step=Step::Freeze;
    };
    struct Local {
        PartyIdentity owner;
        uint64_t generation=0, revision=0, sequence=0, prepared=0, fence=0, attempt=0;
        PartyNpcState state;
        bool running=false, frozen=true, suspended=false;
        PartyNpcMessage cachedRelease;
        std::optional<PartyNpcMessage> pending;
        std::optional<CommitProgress> install;
        std::optional<PartyNpcState> reported;
    };
    PartyNpcScope m_Scope;
    PartyIdentity m_Self;
    IPartyNpcWorld& m_World;
    size_t m_Limit;
    bool m_Stopped=false, m_StopConfirmed=false;
    std::map<uint64_t,Local> m_Npcs;
    std::deque<PartyNpcMessage> m_Out;
    bool Emit(PartyNpcMessage);
};
}
