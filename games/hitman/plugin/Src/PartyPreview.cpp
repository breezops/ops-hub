#include "HitmanCoop.h"
#include "CoopText.h"
#include "CrashGuards.h"
#ifdef COOP_NPC_AGREED_CATALOG
#include "NpcHoldArbiter.h"
#endif
#include <sstream>

namespace {
#ifdef COOP_PARTY_INITIAL_LOAD
constexpr bool kAutoInitialLoad=true;
#else
constexpr bool kAutoInitialLoad=false;
#endif
#ifdef COOP_PARTY_GUEST_START
constexpr bool kGuestLaunch=true;
#else
constexpr bool kGuestLaunch=false;
#endif
const char* Phase(coop::LobbyRoster::Phase phase) {
    using P=coop::LobbyRoster::Phase;
    switch(phase) {
    case P::Lobby:return "Lobby"; case P::Loading:return "Checking loaded missions";
    case P::Restarting:return "Waiting for mission restart";
    case P::Playing:return "Movement preview running"; case P::Completing:return "Completing";
    case P::Complete:return "Complete"; case P::Interrupted:return "Interrupted";
    }
    return "Unknown";
}
const char* State(coop::PartyRoomStatus::State state) {
    using S=coop::PartyRoomStatus::State;
    switch(state) {
    case S::Hosting:return "Hosting"; case S::Connecting:return "Connecting"; case S::Connected:return "Connected";
    case S::Leaving:return "Leaving"; case S::Closed:return "Closed"; case S::Rejected:return "Rejected"; case S::Failed:return "Failed";
    }
    return "Unknown";
}
bool Live(const coop::PartyRoomStatus& s) {
    return s.state==coop::PartyRoomStatus::State::Hosting || s.state==coop::PartyRoomStatus::State::Connected ||
        s.state==coop::PartyRoomStatus::State::Connecting || s.state==coop::PartyRoomStatus::State::Leaving;
}
}

std::string HitmanCoop::HandlePartyPreview(const coop::LauncherRequest& request, double now) {
    CheckLauncherPluginLayout({});
    bool ok=true;
    std::string message;
    if (request.action=="preview_host" || request.action=="preview_join") {
#ifdef COOP_NPC_CATALOG
        coop::PartyNpcOptions npcOptions;
#endif
        if (!m_Unsupported.empty()) { ok=false; message=UnsupportedText(); }
        else if (!m_World.PartyPreviewError().empty()) { ok=false; message=m_World.PartyPreviewError(); }
        else if ((m_Session && m_Session->Active()) || (m_PartyPreview && Live(m_PartyPreview->Status()))) {
            ok=false; message="Leave the current session first.";
        } else if (!m_World.CanOpenPartyPreview()) {
            ok=false; message="Reload Freeform Training to clear the previous session, then try again.";
#if defined(COOP_NPC_CATALOG) && !defined(COOP_NPC_AGREED_CATALOG)
        } else if (message=m_World.ResolveNpcCatalog(npcOptions.fixedCatalog); !message.empty()) {
            ok=false;
#endif
        } else {
            m_Session.reset(); // Inactive legacy sockets must release the host port.
            std::string error;
            const bool host=request.action=="preview_host";
            coop::Endpoint endpoint;
            if (!host && !coop::ResolveEndpoint(request.value,coop::kDefaultPort,endpoint,error)) {
                ok=false; message="Could not resolve host: "+error;
            } else {
                auto socket=coop::UdpSocket::Open(host ? coop::kDefaultPort : 0,error);
                if (!socket) { ok=false; message="Could not open UDP: "+error; }
                else {
                    coop::PartyRoomConfig config;
                    config.build=c_ModVersion; config.game=m_GameVersion; config.name=m_NameBuffer;
#if defined(COOP_NPC_CATALOG) && !defined(COOP_NPC_AGREED_CATALOG)
                    std::ostringstream key;
                    key<<COOP_NPC_CATALOG_ROOM_BUILD<<std::hex;
                    for (const auto id:npcOptions.fixedCatalog) key<<'/'<<id;
                    config.build=key.str();
#endif
                    auto room=host ? coop::PartyRoom::Host(std::move(socket),config) :
                        coop::PartyRoom::Join(std::move(socket),endpoint,config,now);
                    if (!room) { ok=false; message="Could not create the preview room."; }
                    else {
                        // CanOpenPartyPreview already guarantees that no live
                        // legacy stand-in remains. Keep restart history while
                        // clearing its stale current actor/knowledge pointers.
                        CrashGuards::SetKnowledge(nullptr);
                        CrashGuards::SetActor(nullptr);
                        m_Session.reset(); m_LastJoin.reset(); m_Reconnecting=false; m_Relay.reset(); m_FrameStatus={};
#ifdef COOP_NPC_AGREED_CATALOG
                        npcOptions.capacity=NpcHoldArbiter::Capacity;
                        m_World.SetNpcCatalogHost(host);
                        m_PartyPreview=std::make_unique<coop::PartyPreviewRuntime>(std::move(room),m_World,m_World,&m_World,
                            npcOptions,true,true,static_cast<coop::IPartyFreeformLaunch*>(this),true,true);
#elif defined(COOP_NPC_CATALOG)
                        npcOptions.capacity=npcOptions.fixedCatalog.size();
                        npcOptions.streamPoses=true;
                        npcOptions.confirmInitialGuestBaselines=true;
                        m_World.SetNpcCatalogHost(host);
                        m_PartyPreview=std::make_unique<coop::PartyPreviewRuntime>(std::move(room),m_World,m_World,&m_World,npcOptions);
#elif defined(COOP_PARTY_GUEST_START)
                        m_PartyPreview=std::make_unique<coop::PartyPreviewRuntime>(std::move(room),m_World,m_World,nullptr,
                            coop::PartyNpcOptions{},true,true,static_cast<coop::IPartyFreeformLaunch*>(this),
#ifdef COOP_PARTY_NPC_DISCOVERY
                            true
#else
                            false
#endif
                        );
#elif defined(COOP_PARTY_NATIVE_RESTART)
                        m_PartyPreview=std::make_unique<coop::PartyPreviewRuntime>(std::move(room),m_World,m_World,nullptr,coop::PartyNpcOptions{},true,kAutoInitialLoad);
#else
                        m_PartyPreview=std::make_unique<coop::PartyPreviewRuntime>(std::move(room),m_World,m_World);
#endif
                        message=host ? "Preview hosted on UDP 47047. Friends can join by your address." : "Joining the preview host.";
                    }
                }
            }
        }
    } else if (request.action!="preview_status") {
        if (!m_PartyPreview) { ok=false; message="Host or join the movement preview first."; }
        else {
            if (request.action=="preview_ready") ok=m_PartyPreview->Ready();
            else if (request.action=="preview_start") ok=m_PartyPreview->Start();
            else if (request.action=="preview_lobby") ok=m_PartyPreview->Lobby();
            else if (request.action=="preview_reconnect") ok=m_PartyPreview->Reconnect(now);
            else if (request.action=="preview_leave") m_PartyPreview->Leave(now);
            else ok=false;
            message=m_PartyPreview->Message();
        }
    }
    if (request.action!="preview_status") CoopLog::Info("party",request.action+": "+message);
    if (!m_World.PartyPreviewError().empty()) message=m_World.PartyPreviewError();
    const auto quote=[](const std::string& value) { return "\""+TestChannel::Escape(value)+"\""; };
    const auto status=m_PartyPreview ? m_PartyPreview->Status() : coop::PartyRoomStatus{};
    const bool active=m_PartyPreview && Live(status);
    const bool native=m_PartyPreview ? m_PartyPreview->NativeReady() :
        m_World.PartyPreviewError().empty() && m_World.IsInScene() && coop::IsFreeformTrainingScene(m_World.CurrentScene().scene);
    bool allReady=status.members.size()>=2;
    for (const auto& member:status.members) allReady=allReady && member.available && member.ready;
    const bool legacy=m_Session && m_Session->Active();
    std::ostringstream out;
    out<<"{\"ok\":"<<(ok?"true":"false")<<",\"preview\":true,\"bridge_version\":1,\"protocol\":2"
       <<",\"version\":"<<quote(c_ModVersion)<<",\"game_version\":"<<quote(m_GameVersion)
       <<",\"connection\":"<<quote(m_PartyPreview?State(status.state):"Not connected")
       <<",\"phase\":"<<quote(m_PartyPreview?Phase(status.phase):"Lobby")
       <<",\"role\":"<<quote(m_PartyPreview?(status.host?"Host":"Guest"):"None")
       <<",\"room_message\":"<<quote(status.message)<<",\"message\":"<<quote(message)
       <<",\"presentation_error\":"<<quote(m_World.PartyPreviewError())
       <<",\"auto_initial_load\":"<<(kAutoInitialLoad?"true":"false")
       <<",\"guest_launch\":"<<(kGuestLaunch?"true":"false")
       <<",\"lifecycle_message\":"<<quote(m_PartyPreview?m_PartyPreview->Message():"")
       <<",\"native_ready\":"<<(native?"true":"false")<<",\"capacity\":4,\"epoch\":"<<status.epoch
       <<",\"can_connect\":"<<(!legacy && !active && m_World.PartyPreviewError().empty() && m_World.CanOpenPartyPreview()?"true":"false")
       <<",\"can_leave\":"<<(active?"true":"false")
       <<",\"can_ready\":"<<(!kAutoInitialLoad && active && native && status.epoch && status.phase==coop::LobbyRoster::Phase::Lobby?"true":"false")
       <<",\"local_ready\":"<<(m_PartyPreview && m_PartyPreview->LocalReady()?"true":"false")
       <<",\"can_start\":"<<(!kAutoInitialLoad && active && native && status.host && allReady && status.phase==coop::LobbyRoster::Phase::Lobby?"true":"false")
       <<",\"can_stop\":"<<(active && status.host && status.phase!=coop::LobbyRoster::Phase::Lobby?"true":"false")
       <<",\"can_reconnect\":"<<(m_PartyPreview && !status.host && status.state==coop::PartyRoomStatus::State::Failed?"true":"false")
       <<",\"accepted\":"<<(m_PartyPreview?m_PartyPreview->Accepted():0)
       <<",\"rejected\":"<<(m_PartyPreview?m_PartyPreview->Rejected():0)<<",\"players\":[";
    bool first=true;
    for (const auto& member:status.members) {
        if (!first) out<<","; first=false;
        out<<"{\"id\":"<<unsigned(member.id)<<",\"name\":"<<quote(member.name)
           <<",\"local\":"<<(member.id==status.self?"true":"false")
           <<",\"ready\":"<<(member.ready?"true":"false")<<",\"loaded\":"<<(member.loaded?"true":"false")<<"}";
    }
    out<<"],\"shared_npcs\":false,\"shared_items\":false,\"shared_extraction\":false}";
    return out.str();
}
