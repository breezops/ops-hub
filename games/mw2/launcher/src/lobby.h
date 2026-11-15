#pragma once
#include <array>
#include <string>

namespace mw2 {
struct Player {
  bool occupied = false;
  bool online = false;
  std::wstring name;
  int serviceRtt = -1;
};
struct Lobby {
  std::wstring code, mission = L"overkill-concept", difficulty = L"veteran";
  std::array<Player, 4> players;
  int ownSlot = -1;
  bool member() const { return ownSlot >= 0 && ownSlot < 4 && !code.empty(); }
  bool host() const { return member() && ownSlot == 0; }
};
struct LobbyReply {
  bool ok = false;
  bool ready = false;
  int status = 0, rtt = 0;
  Lobby lobby;
  std::wstring error;
};
enum class LobbyAction { Health, Create, Join, Heartbeat, Settings, Leave };
struct LobbyRequest {
  LobbyAction action = LobbyAction::Health;
  std::wstring code, name, token, mission = L"overkill-concept",
                                  difficulty = L"veteran";
  int previousRtt = 0;
};
std::wstring memberToken();
// Callers keep Windows Runtime initialized for the application lifetime.
// Production host is fixed. The test override accepts HTTP loopback only.
LobbyReply lobbyRequest(const LobbyRequest &request,
                        const std::wstring &testBase = L"");
} // namespace mw2
