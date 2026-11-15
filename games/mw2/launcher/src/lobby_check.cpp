#include "lobby.h"
#include <iostream>
#include <stdexcept>
#include <winrt/base.h>

int wmain(int argc, wchar_t **argv) {
  winrt::init_apartment(winrt::apartment_type::multi_threaded);
  const std::wstring base =
      argc == 3 && std::wstring(argv[1]) == L"--base" ? argv[2] : L"";
  std::wstring code;
  mw2::LobbyRequest host;
  host.name = L"Test-Host";
  host.token = mw2::memberToken();
  auto send = [&](const mw2::LobbyRequest &request) {
    return mw2::lobbyRequest(request, base);
  };
  auto check = [](bool success, const char *message) {
    std::cout << (success ? "PASS " : "FAIL ") << message << std::endl;
    if (!success)
      throw std::runtime_error(message);
  };
  try {
    check(send({}).ok, "lobby service health");
    host.action = mw2::LobbyAction::Create;
    auto created = send(host);
    check(created.ok && created.lobby.host(), "host creates six-letter code");
    code = created.lobby.code;
    host.code = code;
    check(code.size() == 6, "six letters allocated");
    std::array<mw2::LobbyRequest, 3> guests;
    for (int i = 0; i < 3; i++) {
      auto &guest = guests[i];
      guest.action = mw2::LobbyAction::Join;
      guest.name = L"Test-Player-" + std::to_wstring(i + 2);
      guest.code = code;
      guest.token = mw2::memberToken();
      auto joined = send(guest);
      check(joined.ok && joined.lobby.ownSlot == i + 1,
            "guest appears in the correct slot");
    }
    auto fifth = guests[0];
    fifth.token = mw2::memberToken();
    check(send(fifth).status == 409, "fifth player refused");
    host.action = mw2::LobbyAction::Settings;
    host.mission = L"sniper-fi";
    host.difficulty = L"hardened";
    check(send(host).ok, "host changes mission and difficulty");
    guests[0].action = mw2::LobbyAction::Heartbeat;
    auto synced = send(guests[0]);
    check(synced.ok && synced.lobby.mission == L"sniper-fi" &&
              synced.lobby.difficulty == L"hardened",
          "guest receives shared match settings");
    check(synced.lobby.players[3].name == L"Test-Player-4",
          "all four names visible");
    guests[0].action = mw2::LobbyAction::Settings;
    check(send(guests[0]).status == 403, "guest cannot change host settings");
    guests[0].action = mw2::LobbyAction::Leave;
    check(send(guests[0]).ok, "guest leaves lobby");
    host.action = mw2::LobbyAction::Leave;
    check(send(host).ok, "host closes temporary test lobby");
    code.clear();
    std::cout
        << "Launcher lobby only; gameplay has not been started or tested.\n";
    return 0;
  } catch (const std::exception &error) {
    if (!code.empty()) {
      host.action = mw2::LobbyAction::Leave;
      send(host);
    }
    std::cerr << error.what() << '\n';
    return 1;
  }
}
