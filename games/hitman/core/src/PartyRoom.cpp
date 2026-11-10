#include "coop/PartyRoom.h"
#include <Windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <stdexcept>

namespace coop {
namespace partycrypto {
std::array<uint8_t,32> Hmac(const std::array<uint8_t,32>& key,const std::vector<uint8_t>& bytes) {
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,BCRYPT_ALG_HANDLE_HMAC_FLAG)!=0)
        throw std::runtime_error("HMAC provider unavailable");
    DWORD length=0,result=0;
    if (BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&length),sizeof(length),&result,0)!=0) {
        BCryptCloseAlgorithmProvider(algorithm,0); throw std::runtime_error("HMAC object unavailable");
    }
    std::vector<uint8_t> object(length); BCRYPT_HASH_HANDLE hash=nullptr;
    auto status=BCryptCreateHash(algorithm,&hash,object.data(),length,const_cast<PUCHAR>(key.data()),DWORD(key.size()),0);
    std::array<uint8_t,32> digest{};
    if (status==0) status=BCryptHashData(hash,const_cast<PUCHAR>(bytes.data()),DWORD(bytes.size()),0);
    if (status==0) status=BCryptFinishHash(hash,digest.data(),DWORD(digest.size()),0);
    if (hash) BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm,0);
    if (status!=0) throw std::runtime_error("HMAC calculation failed");
    return digest;
}
}
namespace {
using Secret=std::array<uint8_t,16>;
constexpr uint32_t JoinMagic=0x314a3448; // H4J1
constexpr uint32_t TransportMagic=0x31503448;
enum class JoinKind:uint8_t { Hello=1,Challenge,Proof,Welcome,Reject,Leave,Left };
enum class Control:uint8_t { Joined=1,Roster,Ready,Loaded,Completed,SceneLost };
void Random(void* bytes,size_t size) {
    if (BCryptGenRandom(nullptr,static_cast<PUCHAR>(bytes),ULONG(size),BCRYPT_USE_SYSTEM_PREFERRED_RNG)!=0)
        throw std::runtime_error("random source unavailable");
}
uint64_t Token() { uint64_t value=0; while(!value) Random(&value,sizeof(value)); return value; }
Secret NewSecret() { Secret value{}; Random(value.data(),value.size()); return value; }
bool Empty(const Secret& value) { return std::all_of(value.begin(),value.end(),[](uint8_t byte){return byte==0;}); }
bool Equal(const Secret& a,const Secret& b) { uint8_t diff=0; for(size_t i=0;i<a.size();++i) diff|=a[i]^b[i]; return diff==0; }
bool NameValid(const std::string& name) {
    return !name.empty() && name.size()<=48 && std::none_of(name.begin(),name.end(),[](unsigned char c){return c<32 || c==127;});
}
struct Datagram { Endpoint from; std::vector<uint8_t> bytes; };
class Hub:public std::enable_shared_from_this<Hub> {
public:
    explicit Hub(std::unique_ptr<IDatagramSocket> socket):socket(std::move(socket)) { if(!this->socket) throw std::invalid_argument("socket required"); }
    std::unique_ptr<IDatagramSocket> View();
    std::unique_ptr<IDatagramSocket> socket;
    std::deque<Datagram> joins,frames;
    void Poll() {
        uint8_t bytes[kMaxPacketSize+partywire::HeaderSize]; Endpoint from;
        for(size_t i=0;i<512;++i) {
            int count=socket->RecvFrom(from,bytes,sizeof(bytes)); if(count<0) break;
            ByteReader reader(bytes,size_t(count)); uint32_t magic=0; if(!reader.U32(magic)) continue;
            auto* queue=magic==JoinMagic ? &joins : magic==TransportMagic ? &frames : nullptr;
            if(queue && queue->size()<256 && (magic!=JoinMagic || (count>=80 && count<=384)))
                queue->push_back({from,{bytes,bytes+count}});
        }
    }
    void Send(Endpoint to,const ByteWriter& writer) {
        auto bytes=writer.Data(); while(bytes.size()<80) bytes.push_back(0);
        socket->SendTo(to,bytes.data(),bytes.size());
    }
};
class HubView:public IDatagramSocket {
    std::shared_ptr<Hub> hub;
public:
    explicit HubView(std::shared_ptr<Hub> hub):hub(std::move(hub)){}
    bool SendTo(const Endpoint& to,const uint8_t* bytes,size_t size) override { return hub->socket->SendTo(to,bytes,size); }
    int RecvFrom(Endpoint& from,uint8_t* bytes,size_t capacity) override {
        if(hub->frames.empty()) return -1;
        auto frame=std::move(hub->frames.front()); hub->frames.pop_front();
        if(frame.bytes.size()>capacity) return -1;
        from=frame.from; std::copy(frame.bytes.begin(),frame.bytes.end(),bytes); return int(frame.bytes.size());
    }
};
std::unique_ptr<IDatagramSocket> Hub::View(){return std::make_unique<HubView>(shared_from_this());}
ByteWriter Begin(JoinKind kind,uint64_t nonce) { ByteWriter writer; writer.U32(JoinMagic); writer.U8(uint8_t(kind)); writer.U64(nonce); return writer; }
bool Padding(ByteReader& reader) { uint8_t byte=0; while(reader.Remaining()) if(!reader.U8(byte) || byte) return false; return reader.Ok(); }
struct Hello {
    uint64_t nonce=0; uint32_t protocol=0,bucket=0;
    std::string build,game,name;
    Secret resume{},cookie{};
};
void HelloFields(ByteWriter& writer,const Hello& hello) {
    writer.U32(hello.protocol); writer.Str(hello.build); writer.Str(hello.game); writer.Str(hello.name);
    writer.Raw(hello.resume.data(),hello.resume.size());
}
bool ReadHello(ByteReader& reader,Hello& hello,bool proof) {
    if(!hello.nonce || !reader.U32(hello.protocol) || !reader.Str(hello.build,64) || !reader.Str(hello.game,32) ||
        !reader.Str(hello.name,48) || !NameValid(hello.name) || !reader.Raw(hello.resume.data(),hello.resume.size())) return false;
    if(proof && (!reader.U32(hello.bucket) || !reader.Raw(hello.cookie.data(),hello.cookie.size()))) return false;
    return Padding(reader);
}
Secret Cookie(const std::array<uint8_t,32>& secret,Endpoint endpoint,uint64_t session,const Hello& hello,uint32_t bucket) {
    ByteWriter writer; writer.U32(endpoint.ip); writer.U16(endpoint.port); writer.U32(endpoint.relayPeer);
    writer.U64(session); writer.U64(hello.nonce); writer.U32(bucket); HelloFields(writer,hello);
    auto digest=partycrypto::Hmac(secret,writer.Data()); Secret cookie{}; std::copy_n(digest.begin(),cookie.size(),cookie.begin()); return cookie;
}
uint8_t Bits(const LobbyRoster::Player& p) { return (p.available?1:0)|(p.ready?2:0)|(p.loaded?4:0)|(p.complete?8:0); }
}

struct PartyRoom::Impl {
    PartyRoomConfig config;
    PartyRoomStatus status;
    std::shared_ptr<Hub> hub;
    std::unique_ptr<PartyHost> host;
    std::unique_ptr<PartyClient> client;
    LobbyRoster lobby {"host"};
    struct Member {
        Endpoint endpoint; Secret resume{};
        uint64_t nonce=0,key=0; double deadline=0;
        std::string name; bool connected=false,pending=false;
    };
    std::array<Member,4> members;
    std::array<uint8_t,32> secret{};
    Hello hello;
    Endpoint remote;
    uint64_t challengeSession=0,link=0;
    double now=0,started=0,lastSend=-100,leaveStarted=0;
    uint32_t revision=0;
    bool proving=false;
    std::deque<PartyGameEvent> events;

    Impl(std::unique_ptr<IDatagramSocket> socket,PartyRoomConfig config,bool isHost):config(std::move(config)),hub(std::make_shared<Hub>(std::move(socket))) {
        if(!NameValid(this->config.name) || this->config.build.empty() || this->config.build.size()>64 ||
            this->config.game.empty() || this->config.game.size()>32) throw std::invalid_argument("invalid room configuration");
        status.host=isHost;
        if(isHost) {
            Random(secret.data(),secret.size()); status.session=Token(); status.self=1; status.state=PartyRoomStatus::State::Hosting;
            host=std::make_unique<PartyHost>(hub->View(),status.session,false);
            members[0].key=status.session; members[0].name=this->config.name; members[0].connected=true;
            Publish();
        }
    }
    bool Live(uint8_t id,uint64_t key) const {
        return id>=1 && id<=4 && members[id-1].connected && members[id-1].key==key;
    }
    void Start(double time) {
        now=started=time; lastSend=-100; proving=false; challengeSession=0; client.reset();
        hub->frames.clear(); hub->joins.clear(); events.clear(); status.state=PartyRoomStatus::State::Connecting;
        status.epoch=0; status.phase=LobbyRoster::Phase::Lobby; status.mission.clear(); status.message.clear();
        status.members.clear(); status.revision=0; status.self=0; status.session=0;
        hello.nonce=Token(); hello.protocol=config.protocol; hello.build=config.build; hello.game=config.game; hello.name=config.name;
    }
    void Reject(Endpoint endpoint,uint64_t nonce,const char* why) {
        auto writer=Begin(JoinKind::Reject,nonce); writer.Str(why); hub->Send(endpoint,writer);
    }
    void Welcome(Endpoint endpoint,uint8_t id) {
        const auto& member=members[id-1];
        auto writer=Begin(JoinKind::Welcome,member.nonce);
        writer.U64(status.session);
        writer.U8(id); writer.U64(member.key); writer.Raw(member.resume.data(),member.resume.size());
        hub->Send(endpoint,writer);
    }
    void Publish() {
        status.revision=++revision; status.epoch=lobby.epoch; status.phase=lobby.phase; status.mission=lobby.mission;
        status.members.clear(); ByteWriter writer; writer.U8(uint8_t(Control::Roster)); writer.U64(status.session);
        writer.U32(revision); writer.U32(lobby.epoch); writer.U8(uint8_t(lobby.phase)); writer.Str(lobby.mission);
        uint8_t count=0; for(const auto& member:members) if(member.connected) ++count; writer.U8(count);
        for(size_t i=0;i<4;++i) if(members[i].connected) {
            const auto& p=lobby.players[i]; const auto& member=members[i];
            writer.U8(uint8_t(i+1)); writer.U64(member.key); writer.Str(member.name); writer.U8(Bits(p));
            status.members.push_back({uint8_t(i+1),member.key,member.name,p.available,p.ready,p.loaded,p.complete});
        }
        host->Broadcast(PartyChannel::Lifecycle,writer.Data(),true);
    }
    void Drop(uint8_t id) {
        if(id<2 || id>4) return;
        auto& member=members[id-1]; bool changed=member.connected;
        member.connected=member.pending=false; member.deadline=now+60;
        host->Remove(id); lobby.Leave(id);
        if(changed) Publish();
    }
    void HandleHostJoin(const Datagram& packet) {
        ByteReader reader(packet.bytes); uint32_t magic=0; uint8_t raw=0; uint64_t nonce=0;
        if(!reader.U32(magic) || !reader.U8(raw) || !reader.U64(nonce)) return;
        const auto kind=JoinKind(raw);
        if(kind==JoinKind::Leave) {
            uint64_t session=0,key=0; uint8_t id=0;
            if(!reader.U64(session) || !reader.U64(key) || !reader.U8(id) || !Padding(reader) || session!=status.session || id<2 || id>4) return;
            const auto& member=members[id-1];
            if(member.endpoint!=packet.from || member.key!=key || member.nonce!=nonce) return;
            Drop(id); auto writer=Begin(JoinKind::Left,nonce); writer.U64(session); writer.U64(key); hub->Send(packet.from,writer); return;
        }
        if(kind!=JoinKind::Hello && kind!=JoinKind::Proof) return;
        Hello request; request.nonce=nonce;
        if(!ReadHello(reader,request,kind==JoinKind::Proof)) return;
        if(request.protocol!=config.protocol || request.build!=config.build || request.game!=config.game) {
            Reject(packet.from,nonce,"Game or mod build mismatch"); return;
        }
        const uint32_t bucket=uint32_t(now/5.0);
        if(kind==JoinKind::Hello) {
            auto cookie=Cookie(secret,packet.from,status.session,request,bucket);
            auto writer=Begin(JoinKind::Challenge,nonce); writer.U64(status.session); writer.U32(bucket); writer.Raw(cookie.data(),cookie.size());
            hub->Send(packet.from,writer); return;
        }
        if((request.bucket!=bucket && (bucket==0 || request.bucket!=bucket-1)) ||
            !Equal(request.cookie,Cookie(secret,packet.from,status.session,request,request.bucket))) return;
        // Duplicate proofs only resend the same assignment; never reset its Connection.
        for(size_t i=1;i<4;++i) if((members[i].pending || members[i].connected) && members[i].endpoint==packet.from && members[i].nonce==nonce) {
            Welcome(packet.from,uint8_t(i+1)); return;
        }
        uint8_t id=0;
        if(!Empty(request.resume)) {
            for(size_t i=1;i<4;++i) if(Equal(request.resume,members[i].resume) &&
                (members[i].connected || members[i].pending || members[i].deadline>=now)) id=uint8_t(i+1);
            if(!id) { Reject(packet.from,nonce,"Previous slot expired; start a fresh join"); return; }
        } else {
            if(lobby.phase!=LobbyRoster::Phase::Lobby) { Reject(packet.from,nonce,"Mission is already starting or running"); return; }
            for(size_t i=1;i<4;++i) if(!members[i].connected && !members[i].pending) { id=uint8_t(i+1); break; }
        }
        if(!id) { Reject(packet.from,nonce,"Party is full"); return; }
        // A fresh join must not evict an existing endpoint or identity.
        for(size_t i=1;i<4;++i) if(i!=id-1 && (members[i].connected || members[i].pending) && members[i].endpoint==packet.from) {
            Reject(packet.from,nonce,"Endpoint is already connected"); return;
        }
        if(members[id-1].connected || members[id-1].pending) Drop(id);
        auto resume=Empty(request.resume)?NewSecret():request.resume;
        const auto key=Token();
        if(!host->AdmitAt(id,packet.from,key,now)) { Reject(packet.from,nonce,"Slot unavailable"); return; }
        auto& member=members[id-1]; member={packet.from,resume,nonce,key,now+5,request.name,false,true};
        Welcome(packet.from,id);
    }
    bool ReadRoster(const PartyDelivery& frame) {
        if(frame.source!=1 || frame.incarnation!=status.session) return false;
        ByteReader reader(frame.payload); uint8_t control=0,phase=0,count=0;
        PartyRoomStatus next=status; uint64_t session=0;
        if(!reader.U8(control) || control!=uint8_t(Control::Roster) || !reader.U64(session) || session!=status.session ||
            !reader.U32(next.revision) || next.revision<=status.revision || !reader.U32(next.epoch) || !reader.U8(phase) || phase>5 ||
            !reader.Str(next.mission,64) || !reader.U8(count) || count<1 || count>4) return false;
        next.phase=LobbyRoster::Phase(phase); next.members.clear(); uint8_t used=0;
        bool self=false;
        for(uint8_t i=0;i<count;++i) {
            PartyMember member; uint8_t flags=0;
            if(!reader.U8(member.id) || member.id<1 || member.id>4 || (used&(1<<member.id)) || !reader.U64(member.incarnation) ||
                !member.incarnation || !reader.Str(member.name,48) || !NameValid(member.name) || !reader.U8(flags) || flags>15) return false;
            used|=uint8_t(1<<member.id); member.available=flags&1; member.ready=flags&2; member.loaded=flags&4; member.complete=flags&8;
            if(member.id==status.self && member.incarnation==link) self=true;
            next.members.push_back(member);
        }
        if(!reader.AtEnd() || !(used&(1<<1)) || !self ||
            (!next.mission.empty() && next.mission!=kPartyFreeformContract) ||
            !std::any_of(next.members.begin(),next.members.end(),[&](const auto& member){return member.id==1 && member.incarnation==status.session;})) return false;
        next.state=PartyRoomStatus::State::Connected; status=std::move(next); return true;
    }
    void HandleClientJoin(const Datagram& packet) {
        if(packet.from!=remote) return;
        ByteReader reader(packet.bytes); uint32_t magic=0; uint8_t raw=0; uint64_t nonce=0;
        if(!reader.U32(magic) || !reader.U8(raw) || !reader.U64(nonce) || nonce!=hello.nonce) return;
        const auto kind=JoinKind(raw);
        if(kind==JoinKind::Left && status.state==PartyRoomStatus::State::Leaving) {
            uint64_t session=0,key=0; if(reader.U64(session) && reader.U64(key) && Padding(reader) && session==status.session && key==link) {
                client.reset(); status.state=PartyRoomStatus::State::Closed; status.members.clear(); hub->socket.reset();
            }
            return;
        }
        if(status.state!=PartyRoomStatus::State::Connecting) return;
        if(kind==JoinKind::Reject) {
            std::string why; if(reader.Str(why,128) && Padding(reader)) { status.state=PartyRoomStatus::State::Rejected; status.message=why; }
        } else if(kind==JoinKind::Challenge) {
            uint64_t session=0; uint32_t bucket=0; Secret cookie{};
            if(reader.U64(session) && session && reader.U32(bucket) && reader.Raw(cookie.data(),cookie.size()) && Padding(reader)) {
                challengeSession=session; hello.bucket=bucket; hello.cookie=cookie; proving=true; lastSend=-100;
            }
        } else if(kind==JoinKind::Welcome && proving) {
            uint64_t session=0,key=0; uint8_t id=0; Secret resume{};
            if(!reader.U64(session) || session!=challengeSession || !reader.U8(id) || id<2 || id>4 ||
                !reader.U64(key) || !key || !reader.Raw(resume.data(),resume.size()) || Empty(resume) || !Padding(reader)) return;
            if(!client || key!=link) {
                status.session=session; status.self=id; status.revision=0; link=key; hello.resume=resume;
                client=std::make_unique<PartyClient>(hub->View(),remote,session,id,key);
                ByteWriter writer; writer.U8(uint8_t(Control::Joined)); client->Send(PartyChannel::Lifecycle,writer.Data(),true);
            }
        }
    }
    void HostControl(const PartyDelivery& frame) {
        if(frame.source<2 || frame.source>4) return;
        auto& member=members[frame.source-1];
        if(member.key!=frame.incarnation) return;
        ByteReader reader(frame.payload); uint8_t raw=0; if(!reader.U8(raw)) return;
        if(Control(raw)==Control::Joined) {
            if(reader.AtEnd() && member.pending) {
                member.pending=false; member.connected=true;
                lobby.players[frame.source-1]={frame.source,std::to_string(member.key)}; Publish();
            }
            return;
        }
        uint32_t epoch=0; if(!member.connected || !reader.U32(epoch)) return;
        bool changed=false;
        if(Control(raw)==Control::Ready) {
            bool available=false,ready=false;
            if(reader.Bool(available) && reader.Bool(ready) && reader.AtEnd()) changed=lobby.Ready(frame.source,epoch,available,ready);
        } else if(reader.AtEnd() && Control(raw)==Control::Loaded) changed=lobby.Loaded(frame.source,epoch);
        else if(reader.AtEnd() && Control(raw)==Control::Completed) changed=lobby.Completed(frame.source,epoch);
        else if(reader.AtEnd() && Control(raw)==Control::SceneLost) changed=lobby.SceneLost(frame.source,epoch);
        if(changed) Publish();
    }
    bool LiveRoom() const { return status.state==PartyRoomStatus::State::Hosting || status.state==PartyRoomStatus::State::Connected; }
    bool ActiveGame() const { return LiveRoom() && (status.phase==LobbyRoster::Phase::Playing || status.phase==LobbyRoster::Phase::Completing); }
    bool Current(const PartyGameEvent& event) const {
        if(!ActiveGame() || event.epoch!=status.epoch) return false;
        return std::any_of(status.members.begin(),status.members.end(),[&](const auto& member){return member.id==event.source && member.incarnation==event.incarnation;});
    }
    void GameFrame(const PartyDelivery& frame) {
        ByteReader reader(frame.payload); PartyGameEvent event;
        if(!reader.U32(event.epoch)) return;
        event.source=frame.source; event.incarnation=frame.incarnation; event.channel=frame.channel; event.reliable=frame.reliable;
        event.payload.assign(reader.Cursor(),reader.Cursor()+reader.Remaining());
        if(!Current(event)) return;
        if(host && (frame.channel==PartyChannel::Avatar || frame.channel==PartyChannel::Pose)) host->Relay(frame);
        if(events.size()>=1024) { status.state=PartyRoomStatus::State::Failed; status.message="Game event queue exhausted"; return; }
        events.push_back(std::move(event));
    }
    void Tick(double time) {
        if(status.state==PartyRoomStatus::State::Closed || status.state==PartyRoomStatus::State::Failed || status.state==PartyRoomStatus::State::Rejected) return;
        now=time; hub->Poll();
        size_t budget=32;
        while(!hub->joins.empty() && budget--) {
            auto packet=std::move(hub->joins.front()); hub->joins.pop_front();
            if(host) HandleHostJoin(packet); else HandleClientJoin(packet);
        }
        if(status.state==PartyRoomStatus::State::Closed) return;
        if(host) {
            host->Tick(now); PartyIdentity removed;
            while(host->PopRemoved(removed)) {
                auto& member=members[removed.id-1]; if(member.key==removed.incarnation && (member.pending || member.connected)) Drop(removed.id);
            }
            PartyDelivery frame;
            while(host->Pop(frame)) {
                if(frame.channel==PartyChannel::Lifecycle) HostControl(frame);
                else if(Live(frame.source,frame.incarnation)) GameFrame(frame);
            }
            for(uint8_t id=2;id<=4;++id) if(members[id-1].pending && now>members[id-1].deadline) Drop(id);
            if(host->Failed()) { status.state=PartyRoomStatus::State::Failed; status.message="Transport failed"; }
            return;
        }
        if(client) {
            client->Tick(now); PartyDelivery frame;
            while(client->Pop(frame)) {
                if(status.state!=PartyRoomStatus::State::Leaving) {
                    if(frame.channel==PartyChannel::Lifecycle) ReadRoster(frame); else GameFrame(frame);
                }
            }
            if(client->Failed() && status.state==PartyRoomStatus::State::Connected) {
                status.state=PartyRoomStatus::State::Failed; status.message="Lost connection to host";
            }
        }
        if(status.state==PartyRoomStatus::State::Connecting) {
            if(now-started>10) { status.state=PartyRoomStatus::State::Failed; status.message="Join timed out"; return; }
            if(now-lastSend>=0.4 && !client) {
                auto writer=Begin(proving?JoinKind::Proof:JoinKind::Hello,hello.nonce); HelloFields(writer,hello);
                if(proving) { writer.U32(hello.bucket); writer.Raw(hello.cookie.data(),hello.cookie.size()); }
                hub->Send(remote,writer); lastSend=now;
            }
        } else if(status.state==PartyRoomStatus::State::Leaving) {
            if(now-leaveStarted>5) { client.reset(); status.state=PartyRoomStatus::State::Closed; status.members.clear(); hub->socket.reset(); return; }
            if(now-lastSend>=0.3) {
                auto writer=Begin(JoinKind::Leave,hello.nonce); writer.U64(status.session); writer.U64(link); writer.U8(status.self);
                hub->Send(remote,writer); lastSend=now;
            }
        }
    }
};

PartyRoom::PartyRoom(std::unique_ptr<Impl> impl):m_Impl(std::move(impl)){}
PartyRoom::~PartyRoom()=default;
std::unique_ptr<PartyRoom> PartyRoom::Host(std::unique_ptr<IDatagramSocket> socket,PartyRoomConfig config) {
    return std::unique_ptr<PartyRoom>(new PartyRoom(std::make_unique<Impl>(std::move(socket),std::move(config),true)));
}
std::unique_ptr<PartyRoom> PartyRoom::Join(std::unique_ptr<IDatagramSocket> socket,Endpoint host,PartyRoomConfig config,double now) {
    auto impl=std::make_unique<Impl>(std::move(socket),std::move(config),false); impl->remote=host; impl->Start(now);
    return std::unique_ptr<PartyRoom>(new PartyRoom(std::move(impl)));
}
void PartyRoom::Tick(double now){m_Impl->Tick(now);}
const PartyRoomStatus& PartyRoom::Status()const{return m_Impl->status;}
bool PartyRoom::Reconnect(double now) {
    auto& self=*m_Impl; if(self.host || (self.status.state!=PartyRoomStatus::State::Failed && self.status.state!=PartyRoomStatus::State::Connected)) return false;
    self.Start(now); return true;
}
bool PartyRoom::Reconnect(std::unique_ptr<IDatagramSocket> socket,double now) {
    auto& self=*m_Impl;
    if(!socket || self.host || (self.status.state!=PartyRoomStatus::State::Failed && self.status.state!=PartyRoomStatus::State::Connected)) return false;
    self.Start(now); self.hub->socket=std::move(socket); return true;
}
void PartyRoom::Leave(double now) {
    auto& self=*m_Impl;
    if(self.host) { for(uint8_t id=2;id<=4;++id) self.Drop(id); self.status.state=PartyRoomStatus::State::Closed; self.hub->socket.reset(); }
    else if(self.status.self) { self.status.state=PartyRoomStatus::State::Leaving; self.leaveStarted=now; self.lastSend=-100; }
    else self.status.state=PartyRoomStatus::State::Closed;
}
bool PartyRoom::SelectMission(const std::string& contract) {
    auto& self=*m_Impl;
    if(!self.LiveRoom() || !self.host || contract!=kPartyFreeformContract || !self.lobby.Select(contract)) return false;
    self.Publish(); return true;
}
bool PartyRoom::SetReady(bool available,bool ready) {
    auto& self=*m_Impl;
    if(!self.LiveRoom() || self.status.phase!=LobbyRoster::Phase::Lobby || !self.status.epoch) return false;
    if(self.host) {
        if(!self.lobby.Ready(1,self.status.epoch,available,ready)) return false;
        self.Publish(); return true;
    }
    if(!self.client || self.status.state!=PartyRoomStatus::State::Connected) return false;
    ByteWriter writer; writer.U8(uint8_t(Control::Ready)); writer.U32(self.status.epoch); writer.Bool(available); writer.Bool(ready);
    return self.client->Send(PartyChannel::Lifecycle,writer.Data(),true);
}
bool PartyRoom::BeginLoad() {
    auto& self=*m_Impl;
    if(!self.LiveRoom() || !self.host || std::any_of(self.members.begin(),self.members.end(),[](const auto& member){return member.pending;}) || !self.lobby.BeginLoad()) return false;
    self.Publish(); return true;
}
bool PartyRoom::ReportLoaded() {
    auto& self=*m_Impl; if(!self.LiveRoom() || self.status.phase!=LobbyRoster::Phase::Loading) return false;
    if(self.host) { if(!self.lobby.Loaded(1,self.status.epoch)) return false; self.Publish(); return true; }
    if(!self.client || self.status.state!=PartyRoomStatus::State::Connected) return false;
    ByteWriter writer; writer.U8(uint8_t(Control::Loaded)); writer.U32(self.status.epoch);
    return self.client->Send(PartyChannel::Lifecycle,writer.Data(),true);
}
bool PartyRoom::ReportNativeCompletion() {
    auto& self=*m_Impl; if(!self.ActiveGame()) return false;
    if(self.host) { if(!self.lobby.Completed(1,self.status.epoch)) return false; self.Publish(); return true; }
    if(!self.client || self.status.state!=PartyRoomStatus::State::Connected) return false;
    ByteWriter writer; writer.U8(uint8_t(Control::Completed)); writer.U32(self.status.epoch);
    return self.client->Send(PartyChannel::Lifecycle,writer.Data(),true);
}
bool PartyRoom::ReturnToLobby() {
    auto& self=*m_Impl; if(!self.LiveRoom() || !self.host || !self.lobby.ReturnToLobby()) return false; self.Publish(); return true;
}
bool PartyRoom::ReportSceneLost() {
    auto& self=*m_Impl;
    if(!self.LiveRoom() || (self.status.phase!=LobbyRoster::Phase::Loading &&
        self.status.phase!=LobbyRoster::Phase::Playing && self.status.phase!=LobbyRoster::Phase::Completing)) return false;
    if(self.host) { if(!self.lobby.SceneLost(1,self.status.epoch)) return false; self.Publish(); return true; }
    if(!self.client || self.status.state!=PartyRoomStatus::State::Connected) return false;
    ByteWriter writer; writer.U8(uint8_t(Control::SceneLost)); writer.U32(self.status.epoch);
    return self.client->Send(PartyChannel::Lifecycle,writer.Data(),true);
}
bool PartyRoom::SendGame(PartyChannel channel,const std::vector<uint8_t>& payload,bool reliable) {
    auto& self=*m_Impl;
    if(!self.ActiveGame() || payload.size()>partywire::MaxPayload-4 || channel==PartyChannel::Lifecycle) return false;
    ByteWriter writer; writer.U32(self.status.epoch); if(!payload.empty()) writer.Raw(payload.data(),payload.size());
    return self.host ? self.host->Broadcast(channel,writer.Data(),reliable) : self.client && self.client->Send(channel,writer.Data(),reliable);
}
bool PartyRoom::ApproveInteraction(const PartyGameEvent& event) {
    auto& self=*m_Impl;
    if(!self.host || event.channel!=PartyChannel::Interaction || !self.Current(event) || event.payload.size()>partywire::MaxPayload-4) return false;
    ByteWriter writer; writer.U32(event.epoch); if(!event.payload.empty()) writer.Raw(event.payload.data(),event.payload.size());
    return self.host->Relay({event.source,event.incarnation,event.channel,writer.Data(),event.reliable});
}
bool PartyRoom::PopGame(PartyGameEvent& event) {
    auto& self=*m_Impl;
    while(!self.events.empty()) {
        event=std::move(self.events.front()); self.events.pop_front(); if(self.Current(event)) return true;
    }
    return false;
}
}
