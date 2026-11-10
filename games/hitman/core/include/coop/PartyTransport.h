#pragma once
#include "Connection.h"
#include <memory>

namespace coop {
// Experimental host-and-three-guests transport. Admission/handshake, native
// avatars and NPC authority are deliberately separate from this routing layer.
enum class PartyChannel : uint8_t { Avatar=1, Pose=2, Interaction=3, HostWorld=4, Lifecycle=5 };
struct PartyIdentity { uint8_t id=0; uint64_t incarnation=0; };
struct PartyDelivery {
    uint8_t source = 0;
    uint64_t incarnation = 0;
    PartyChannel channel = PartyChannel::Avatar;
    std::vector<uint8_t> payload;
    bool reliable = false;
};
namespace partywire {
constexpr size_t HeaderSize = 20;
constexpr size_t MaxPayload = kMaxMessageSize - 11;
std::vector<uint8_t> Wrap(uint64_t session, uint64_t link, const std::vector<uint8_t>& packet);
bool Unwrap(const uint8_t* bytes, size_t size, uint64_t session, uint64_t link);
std::vector<uint8_t> Encode(const PartyDelivery& frame);
bool Decode(const std::vector<uint8_t>& bytes, PartyDelivery& frame);
}
class PartyHost {
public:
    PartyHost(std::unique_ptr<IDatagramSocket> socket, uint64_t session, bool autoRelay = true);
    // Called only AFTER future admission validates a guest. No anonymous UDP
    // packet can allocate a slot. Incarnations must be unique, nonzero values.
    uint8_t Admit(Endpoint endpoint, uint64_t incarnation, double now);
    uint8_t AdmitAt(uint8_t id, Endpoint endpoint, uint64_t incarnation, double now);
    bool Relay(const PartyDelivery& frame);
    bool Remove(uint8_t id);
    bool Broadcast(PartyChannel channel, const std::vector<uint8_t>& payload, bool reliable);
    void Tick(double now);
    bool Pop(PartyDelivery& frame);
    size_t Guests() const;
    uint64_t Rejected() const { return m_Rejected; }
    bool Failed() const { return m_Failed; }
    bool PopRemoved(PartyIdentity& peer);
private:
    struct Guest {
        uint8_t id;
        uint64_t incarnation;
        double admitted;
        Connection connection;
        // ACK=0 is the connection's pre-receive sentinel. Begin at sequence 1
        // so a host sending before the guest's first packet cannot falsely ACK it.
        Guest(uint8_t id, uint64_t key, Endpoint endpoint, double now) : id(id), incarnation(key), admitted(now), connection(endpoint,1) {}
    };
    std::unique_ptr<IDatagramSocket> m_Socket;
    uint64_t m_Session, m_Rejected = 0;
    bool m_Failed = false, m_AutoRelay = true;
    std::deque<PartyIdentity> m_Removed;
    std::array<std::unique_ptr<Guest>,3> m_Guests;
    std::deque<PartyDelivery> m_Inbox;
    bool Forward(const PartyDelivery& frame, uint8_t exclude);
};
class PartyClient {
public:
    PartyClient(std::unique_ptr<IDatagramSocket> socket, Endpoint host, uint64_t session, uint8_t id, uint64_t incarnation);
    bool Send(PartyChannel channel, const std::vector<uint8_t>& payload, bool reliable);
    void Tick(double now);
    bool Pop(PartyDelivery& frame);
    bool Failed() const { return m_Failed; }
private:
    bool m_Failed = false;
    double m_FirstTick = -1.0;
    std::unique_ptr<IDatagramSocket> m_Socket;
    Endpoint m_Host;
    uint64_t m_Session, m_Incarnation;
    uint8_t m_Id;
    Connection m_Connection;
    std::deque<PartyDelivery> m_Inbox;
};
}
