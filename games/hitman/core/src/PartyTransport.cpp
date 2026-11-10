#include "coop/PartyTransport.h"
#include <algorithm>
#include <stdexcept>

namespace coop {
namespace partywire {
std::vector<uint8_t> Wrap(uint64_t session, uint64_t link, const std::vector<uint8_t>& packet) {
    ByteWriter writer;
    writer.U32(0x31503448); // H4P1; separate from playable protocol 19
    writer.U64(session); writer.U64(link); writer.Raw(packet.data(),packet.size());
    return writer.Data();
}
bool Unwrap(const uint8_t* bytes, size_t size, uint64_t session, uint64_t link) {
    if (size <= HeaderSize || size > kMaxPacketSize + HeaderSize) return false;
    ByteReader reader(bytes,size);
    uint32_t magic=0; uint64_t receivedSession=0, receivedLink=0;
    return reader.U32(magic) && reader.U64(receivedSession) && reader.U64(receivedLink) &&
        magic == 0x31503448 && receivedSession == session && receivedLink == link;
}
std::vector<uint8_t> Encode(const PartyDelivery& frame) {
    ByteWriter writer;
    writer.U8(frame.source); writer.U64(frame.incarnation); writer.U8(uint8_t(frame.channel)); writer.Bool(frame.reliable);
    if (!frame.payload.empty()) writer.Raw(frame.payload.data(),frame.payload.size()); return writer.Data();
}
bool Decode(const std::vector<uint8_t>& bytes, PartyDelivery& frame) {
    if (bytes.size() < 11 || bytes.size() > kMaxMessageSize) return false;
    ByteReader reader(bytes); uint8_t channel=0, reliable=0;
    if (!reader.U8(frame.source) || !reader.U64(frame.incarnation) || !reader.U8(channel) || !reader.U8(reliable) ||
        frame.source < 1 || frame.source > 4 || !frame.incarnation || channel < 1 || channel > 5 || reliable > 1) return false;
    frame.channel=PartyChannel(channel); frame.reliable=reliable!=0;
    frame.payload.assign(bytes.begin()+11,bytes.end()); return true;
}
}
namespace {
bool Queue(Connection& connection, const PartyDelivery& frame) {
    if (frame.payload.size() > partywire::MaxPayload) return false;
    auto encoded=partywire::Encode(frame);
    return frame.reliable ? connection.SendReliable(MsgType::Chat,std::move(encoded)) :
        connection.SendUnreliable(MsgType::Chat,std::move(encoded));
}
void Flush(IDatagramSocket& socket, Connection& connection, uint64_t session, uint64_t link, double now) {
    std::vector<std::vector<uint8_t>> packets;
    connection.Flush(now,packets);
    for (const auto& packet : packets) {
        auto wrapped=partywire::Wrap(session,link,packet);
        socket.SendTo(connection.Remote(),wrapped.data(),wrapped.size());
    }
}
bool PopFrame(std::deque<PartyDelivery>& inbox, PartyDelivery& frame) {
    if (inbox.empty()) return false;
    frame=std::move(inbox.front()); inbox.pop_front(); return true;
}
}
PartyHost::PartyHost(std::unique_ptr<IDatagramSocket> socket, uint64_t session, bool autoRelay) : m_Socket(std::move(socket)), m_Session(session), m_AutoRelay(autoRelay) {
    if (!m_Socket || !session) throw std::invalid_argument("party host requires socket and session identity");
}
uint8_t PartyHost::Admit(Endpoint endpoint, uint64_t key, double now) {
    if (m_Failed || !key || (!endpoint.IsRelay() && (!endpoint.ip || !endpoint.port))) return 0;
    for (const auto& guest : m_Guests)
        if (guest && (guest->connection.Remote()==endpoint || guest->incarnation==key))
            return guest->connection.Remote()==endpoint && guest->incarnation==key ? guest->id : 0;
    for (size_t i=0;i<m_Guests.size();++i) if (!m_Guests[i]) return AdmitAt(uint8_t(i+2),endpoint,key,now);
    return 0;
}
uint8_t PartyHost::AdmitAt(uint8_t id, Endpoint endpoint, uint64_t key, double now) {
    if (m_Failed || id<2 || id>4 || !key || (!endpoint.IsRelay() && (!endpoint.ip || !endpoint.port)) || m_Guests[id-2]) return 0;
    for (const auto& peer:m_Guests) if (peer && (peer->connection.Remote()==endpoint || peer->incarnation==key)) return 0;
    m_Guests[id-2]=std::make_unique<Guest>(id,key,endpoint,now); return id;
}
bool PartyHost::Relay(const PartyDelivery& frame) {
    if (m_Failed || frame.source<2 || frame.source>4 || frame.channel==PartyChannel::HostWorld) return false;
    const auto& peer=m_Guests[frame.source-2];
    return peer && peer->incarnation==frame.incarnation && Forward(frame,frame.source);
}
size_t PartyHost::Guests() const {
    return std::count_if(m_Guests.begin(),m_Guests.end(),[](const auto& guest){return bool(guest);});
}
bool PartyHost::Remove(uint8_t id) {
    if (id<2 || id>4 || !m_Guests[id-2]) return false;
    const auto key=m_Guests[id-2]->incarnation;
    if (m_Removed.size() >= 64) m_Failed=true;
    else m_Removed.push_back({id,key});
    m_Guests[id-2].reset();
    std::erase_if(m_Inbox,[=](const auto& frame){return frame.source==id && frame.incarnation==key;});
    return true;
}
bool PartyHost::Forward(const PartyDelivery& frame, uint8_t exclude) {
    bool ok=true;
    for (auto& guest:m_Guests) if (guest && guest->id!=exclude) {
        if (!Queue(guest->connection,frame)) { const auto id=guest->id; Remove(id); ok=false; }
    }
    return ok;
}
bool PartyHost::Broadcast(PartyChannel channel, const std::vector<uint8_t>& payload, bool reliable) {
    if (m_Failed || uint8_t(channel)<1 || uint8_t(channel)>5 || payload.size()>partywire::MaxPayload) return false;
    return Forward({1,m_Session,channel,payload,reliable},0);
}
void PartyHost::Tick(double now) {
    if (m_Failed) return;
    uint8_t bytes[kMaxPacketSize+partywire::HeaderSize]; Endpoint from;
    for (size_t budget=0;budget<256;++budget) {
        const int count=m_Socket->RecvFrom(from,bytes,sizeof(bytes)); if (count<0) break;
        auto found=std::find_if(m_Guests.begin(),m_Guests.end(),[&](const auto& guest){return guest && guest->connection.Remote()==from;});
        if (found==m_Guests.end() || !partywire::Unwrap(bytes,size_t(count),m_Session,(*found)->incarnation)) { ++m_Rejected; continue; }
        auto& guest=**found;
        // Reject old link/session generations BEFORE Connection sees any sequence or ACK.
        guest.connection.Receive(bytes+partywire::HeaderSize,size_t(count)-partywire::HeaderSize,now);
        Message message;
        while (guest.connection.PopMessage(message)) {
            PartyDelivery frame;
            if (message.type!=MsgType::Chat || !partywire::Decode(message.payload,frame) ||
                frame.source!=guest.id || frame.incarnation!=guest.incarnation || frame.channel==PartyChannel::HostWorld) {
                ++m_Rejected; continue;
            }
            if (m_Inbox.size()>=4096) { m_Failed=true; return; } // explicit failure, never silently lose reliable state
            // Endpoint binding determines sender identity. Never trust a guest's claim about another player.
            frame.source=guest.id; frame.incarnation=guest.incarnation;
            if (m_AutoRelay) Forward(frame,guest.id);
            m_Inbox.push_back(std::move(frame));
        }
    }
    for (auto& guest:m_Guests) if (guest) {
        const double last=guest->connection.HasReceived() ? guest->connection.LastReceiveTime() : guest->admitted;
        if (now-last>15.0 || guest->connection.Overflowed()) { const auto id=guest->id; Remove(id); continue; }
        Flush(*m_Socket,guest->connection,m_Session,guest->incarnation,now);
    }
}
bool PartyHost::Pop(PartyDelivery& frame) { return PopFrame(m_Inbox,frame); }
bool PartyHost::PopRemoved(PartyIdentity& peer) {
    if (m_Removed.empty()) return false;
    peer=m_Removed.front(); m_Removed.pop_front(); return true;
}
PartyClient::PartyClient(std::unique_ptr<IDatagramSocket> socket, Endpoint host, uint64_t session, uint8_t id, uint64_t key) :
    m_Socket(std::move(socket)),m_Host(host),m_Session(session),m_Incarnation(key),m_Id(id),m_Connection(host,1) {
    if (!m_Socket || !session || !key || id<2 || id>4) throw std::invalid_argument("invalid admitted party client");
}
bool PartyClient::Send(PartyChannel channel, const std::vector<uint8_t>& payload, bool reliable) {
    if (m_Failed || uint8_t(channel)<1 || uint8_t(channel)>5 || channel==PartyChannel::HostWorld) return false;
    return Queue(m_Connection,{m_Id,m_Incarnation,channel,payload,reliable});
}
void PartyClient::Tick(double now) {
    if (m_Failed) return;
    if (m_FirstTick<0) m_FirstTick=now;
    const double last=m_Connection.HasReceived() ? m_Connection.LastReceiveTime() : m_FirstTick;
    if (now-last>15.0 || m_Connection.Overflowed()) { m_Failed=true; return; }
    uint8_t bytes[kMaxPacketSize+partywire::HeaderSize]; Endpoint from;
    for(size_t budget=0;budget<256;++budget) {
        const int count=m_Socket->RecvFrom(from,bytes,sizeof(bytes)); if(count<0) break;
        if (from!=m_Host || !partywire::Unwrap(bytes,size_t(count),m_Session,m_Incarnation)) continue;
        m_Connection.Receive(bytes+partywire::HeaderSize,size_t(count)-partywire::HeaderSize,now);
        Message message;
        while(m_Connection.PopMessage(message)) {
            PartyDelivery frame;
            if(message.type==MsgType::Chat && partywire::Decode(message.payload,frame) && frame.source!=m_Id &&
                (frame.channel!=PartyChannel::HostWorld || frame.source==1)) {
                if(m_Inbox.size()>=4096) { m_Failed=true; return; }
                m_Inbox.push_back(std::move(frame));
            }
        }
    }
    Flush(*m_Socket,m_Connection,m_Session,m_Incarnation,now);
}
bool PartyClient::Pop(PartyDelivery& frame) { return PopFrame(m_Inbox,frame); }
}
