#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h> // Windows SDK headers below require this first.
#include "lobby.h"
#include "version.h"
#include <algorithm>
#include <bcrypt.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <winhttp.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/base.h>

namespace mw2 {
using namespace winrt::Windows::Data::Json;
namespace {
struct Handle {
  HINTERNET value{};
  explicit Handle(HINTERNET v) : value(v) {
    if (!v)
      throw std::runtime_error("HTTP handle unavailable");
  }
  ~Handle() {
    if (value)
      WinHttpCloseHandle(value);
  }
  Handle(const Handle &) = delete;
  Handle &operator=(const Handle &) = delete;
};
void require(BOOL value) {
  if (!value)
    throw std::runtime_error("Network request failed");
}
void string(JsonObject &object, const wchar_t *key, const std::wstring &value) {
  object.SetNamedValue(key, JsonValue::CreateStringValue(value));
}
std::wstring get(const JsonObject &object, const wchar_t *key) {
  return std::wstring(object.GetNamedString(key, L""));
}
} // namespace
std::wstring memberToken() {
  unsigned char bytes[32];
  if (BCryptGenRandom(nullptr, bytes, sizeof(bytes),
                      BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
    throw std::runtime_error("Secure random unavailable");
  std::wostringstream value;
  for (auto byte : bytes)
    value << std::hex << std::setw(2) << std::setfill(L'0') << unsigned(byte);
  return value.str();
}
LobbyReply lobbyRequest(const LobbyRequest &request,
                        const std::wstring &testBase) {
  LobbyReply result;
  const auto start = std::chrono::steady_clock::now();
  bool initialized = false;
  try {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    initialized = true;
    std::wstring host = L"ops-hub.breezops.chatgpt.site";
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
    bool secure = true;
    if (!testBase.empty()) {
      constexpr wchar_t prefix[] = L"http://127.0.0.1:";
      if (testBase.rfind(prefix, 0) != 0)
        throw std::runtime_error("Only loopback test endpoints are allowed");
      const auto value = testBase.substr(std::size(prefix) - 1);
      if (value.empty() ||
          value.find_first_not_of(L"0123456789") != std::wstring::npos)
        throw std::runtime_error("Invalid test port");
      auto numeric = std::stoul(value);
      if (numeric == 0 || numeric > 65535)
        throw std::runtime_error("Invalid test port");
      port = static_cast<INTERNET_PORT>(numeric);
      host = L"127.0.0.1";
      secure = false;
    }
    std::wstring code = request.code;
    std::transform(code.begin(), code.end(), code.begin(), [](wchar_t c) {
      return c >= L'a' && c <= L'z' ? static_cast<wchar_t>(c - L'a' + L'A') : c;
    });
    if (request.action != LobbyAction::Health &&
        request.action != LobbyAction::Create &&
        (code.size() < 4 || code.size() > 8 ||
         code.find_first_not_of(L"ABCDEFGHJKLMNPQRSTUVWXYZ") !=
             std::wstring::npos)) {
      result.error = L"Enter a 4-8 letter lobby code (no I or O).";
      winrt::uninit_apartment();
      return result;
    }
    std::wstring path = L"/mw2/api/lobbies", method = L"POST";
    if (request.action == LobbyAction::Health) {
      path = L"/mw2/api/health";
      method = L"GET";
    } else if (request.action != LobbyAction::Create)
      path += L"/" + code;
    if (request.action == LobbyAction::Join)
      path += L"/join";
    if (request.action == LobbyAction::Heartbeat)
      path += L"/heartbeat";
    if (request.action == LobbyAction::Settings)
      method = L"PATCH";
    if (request.action == LobbyAction::Leave)
      method = L"DELETE";
    JsonObject body;
    if (request.action == LobbyAction::Create ||
        request.action == LobbyAction::Join) {
      string(body, L"name", request.name);
      string(body, L"member_token", request.token);
      body.SetNamedValue(L"protocol",
                         JsonValue::CreateNumberValue(LobbyProtocol));
    }
    if (request.action == LobbyAction::Create ||
        request.action == LobbyAction::Settings) {
      string(body, L"mission", request.mission);
      string(body, L"difficulty", request.difficulty);
    }
    if (request.action == LobbyAction::Heartbeat)
      body.SetNamedValue(L"service_rtt_ms",
                         JsonValue::CreateNumberValue(request.previousRtt));
    const auto encoded = (method == L"POST" || method == L"PATCH")
                             ? winrt::to_string(body.Stringify())
                             : std::string();
    Handle session(WinHttpOpen(L"OPS-HUB-MW2/" MW2_VERSION_LITERAL,
                               WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                               WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS,
                               0));
    require(WinHttpSetTimeouts(session.value, 3000, 5000, 5000, 5000));
    Handle connection(WinHttpConnect(session.value, host.c_str(), port, 0));
    Handle message(WinHttpOpenRequest(connection.value, method.c_str(),
                                      path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                      WINHTTP_DEFAULT_ACCEPT_TYPES,
                                      secure ? WINHTTP_FLAG_SECURE : 0));
    DWORD redirects = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    require(WinHttpSetOption(message.value, WINHTTP_OPTION_REDIRECT_POLICY,
                             &redirects, sizeof(redirects)));
    DWORD disabled = WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_AUTHENTICATION;
    require(WinHttpSetOption(message.value, WINHTTP_OPTION_DISABLE_FEATURE,
                             &disabled, sizeof(disabled)));
    std::wstring headers =
        L"Accept: application/json\r\nContent-Type: application/json\r\n";
    if (!request.token.empty() && request.action != LobbyAction::Create &&
        request.action != LobbyAction::Join)
      headers += L"Authorization: Bearer " + request.token + L"\r\n";
    require(WinHttpSendRequest(
        message.value, headers.c_str(), static_cast<DWORD>(headers.size()),
        encoded.empty() ? WINHTTP_NO_REQUEST_DATA
                        : const_cast<char *>(encoded.data()),
        static_cast<DWORD>(encoded.size()), static_cast<DWORD>(encoded.size()),
        0));
    require(WinHttpReceiveResponse(message.value, nullptr));
    DWORD status = 0, length = sizeof(status);
    require(WinHttpQueryHeaders(
        message.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status, &length,
        WINHTTP_NO_HEADER_INDEX));
    result.status = static_cast<int>(status);
    std::string response;
    while (true) {
      char buffer[4096];
      DWORD got = 0;
      require(WinHttpReadData(message.value, buffer, sizeof(buffer), &got));
      if (!got)
        break;
      if (response.size() + got > 65536)
        throw std::runtime_error("Oversized service response");
      response.append(buffer, got);
    }
    const auto data = JsonObject::Parse(winrt::to_hstring(response));
    if (status < 200 || status >= 300) {
      result.error = get(data, L"error");
      if (result.error.empty())
        result.error = L"The lobby service rejected the request.";
    } else if (request.action == LobbyAction::Health) {
      result.ready = data.GetNamedBoolean(L"ready", false);
      result.ok = result.ready;
      if (!result.ready)
        result.error = L"Lobby service is not ready.";
    } else if (request.action == LobbyAction::Leave)
      result.ok = true;
    else {
      if (data.GetNamedNumber(L"protocol", 0) != LobbyProtocol)
        throw std::runtime_error("Different lobby protocol");
      result.lobby.code = get(data, L"code");
      result.lobby.mission = get(data, L"mission");
      result.lobby.difficulty = get(data, L"difficulty");
      result.lobby.ownSlot =
          static_cast<int>(data.GetNamedNumber(L"you_slot", -1));
      const auto players = data.GetNamedArray(L"players");
      if (players.Size() != 4 || !result.lobby.member())
        throw std::runtime_error("Invalid lobby roster");
      for (unsigned i = 0; i < 4; i++)
        if (players.GetAt(i).ValueType() != JsonValueType::Null) {
          auto p = players.GetObjectAt(i);
          auto &target = result.lobby.players[i];
          target.occupied = true;
          target.online = p.GetNamedBoolean(L"online", false);
          target.name = get(p, L"name");
          const auto ping = p.GetNamedValue(L"service_rtt_ms");
          if (ping.ValueType() == JsonValueType::Number)
            target.serviceRtt = static_cast<int>(ping.GetNumber());
        }
      result.ok = true;
      result.ready = true;
    }
  } catch (...) {
    result.ok = false;
    result.error =
        L"Cannot reach the lobby service. Check your connection and try again.";
  }
  if (initialized)
    winrt::uninit_apartment();
  result.rtt =
      static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start)
                           .count());
  return result;
}
} // namespace mw2
