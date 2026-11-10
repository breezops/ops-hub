#include "HitmanCoop.h"
#include "GamePause.h"
#include "CoopText.h"
#include <sstream>

std::string HitmanCoop::HandleLauncher(const coop::LauncherRequest& request, double now) {
    m_LauncherLastContact = now;
    if (request.action.rfind("preview_", 0) == 0)
        return HandlePartyPreview(request, now);
    const auto status = m_Session ? m_Session->Status() : coop::SessionStatus{};
    const bool connected = status.state == coop::SessionState::Connected;
    const bool freeform = m_World.IsInScene() && coop::IsFreeformTrainingScene(m_World.CurrentScene().scene);
    const bool replay = freeform && status.synced && status.role == coop::SessionRole::Host && GamePause::CanRestart();
    std::string message;
    bool ok = true;
    if (request.action != "status") {
        if (!m_Unsupported.empty() && request.action != "leave") { ok = false; message = UnsupportedText(); }
        else if (request.action == "host" || request.action == "join") {
            if (connected || status.state == coop::SessionState::Listening || status.state == coop::SessionState::Connecting) {
                ok = false; message = "Leave the current session before hosting or joining another.";
            } else {
                PushCommand({ request.action == "host" ? Command::Kind::Host : Command::Kind::Join,
                    request.value, static_cast<uint16_t>(m_Port), m_NameBuffer, "" });
                message = request.action == "host" ? "Host request queued." : "Join request queued.";
            }
        } else if (request.action == "leave") {
            PushCommand({ Command::Kind::Leave }); message = "Leaving the session.";
        } else if (request.action == "ready" || request.action == "countdown") {
            if (!status.synced) { ok = false; message = "Both players must load the same mission first."; }
            else {
                PushCommand({ request.action == "ready" ? Command::Kind::ReadyCheck : Command::Kind::Countdown });
                message = request.action == "ready" ? (now - m_PartnerAskedReadyAt < 60.0 ? "Sending your ready reply." : "Asking your friend if they are ready.") : "Countdown requested.";
            }
        } else if (request.action == "replay") {
            if (!replay) { ok = false; message = "Replay is available to the host when both players are in Freeform Training."; }
            else { m_LauncherReplayRequested = true; message = "Restarting Freeform Training for both players."; }
        }
        CoopLog::Info("launcher", request.action + ": " + message);
    }
    const auto quote = [](const std::string& text) { return "\"" + TestChannel::Escape(text) + "\""; };
    std::ostringstream out;
    out << "{\"ok\":" << (ok ? "true" : "false") << ",\"bridge_version\":1"
        << ",\"version\":" << quote(c_ModVersion) << ",\"protocol\":" << coop::kProtocolVersion
        << ",\"game_version\":" << quote(m_GameVersion)
        << ",\"connection\":" << quote(coop::SessionStateName(status.state))
        << ",\"session_message\":" << quote(status.message)
        << ",\"role\":" << quote(status.role == coop::SessionRole::Host ? "Host" : status.role == coop::SessionRole::Client ? "Guest" : "None")
        << ",\"partner\":" << quote(status.peerName)
        << ",\"can_leave\":" << ((status.state != coop::SessionState::Idle || m_Reconnecting) ? "true" : "false")
        << ",\"connected\":" << (connected ? "true" : "false")
        << ",\"synced\":" << (status.synced ? "true" : "false")
        << ",\"in_mission\":" << (m_World.IsInScene() ? "true" : "false")
        << ",\"freeform\":" << (freeform ? "true" : "false")
        << ",\"can_replay\":" << (replay ? "true" : "false")
        << ",\"ready_state\":" << quote(coop::LauncherReadyState(status.synced, now, m_ReadyAskedAt, m_PartnerAskedReadyAt,
            m_LauncherPartnerReadyAt, m_LauncherOwnReadyAt))
        << ",\"ready_prompt\":" << (status.synced && now - m_PartnerAskedReadyAt < 60.0 ? "true" : "false")
        << ",\"player_capacity\":2,\"target_player_capacity\":4,\"players\":[{\"id\":"
        << (status.role == coop::SessionRole::Client ? 2 : 1) << ",\"name\":" << quote(m_NameBuffer)
        << ",\"local\":true}";
    if (connected) out << ",{\"id\":" << (status.role == coop::SessionRole::Client ? 1 : 2)
        << ",\"name\":" << quote(status.peerName) << ",\"local\":false}";
    out << "]"
        << ",\"automatic_mission_launch\":false,\"shared_extraction_verified\":false"
        << ",\"message\":" << quote(message) << "}";
    return out.str();
}
