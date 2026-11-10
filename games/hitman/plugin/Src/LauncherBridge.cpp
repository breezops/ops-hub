#include "LauncherBridge.h"
#include "Log.h"
#include <sddl.h>
#include <vector>

LauncherBridge::~LauncherBridge() { if (m_Pipe != INVALID_HANDLE_VALUE) CloseHandle(m_Pipe); }

bool LauncherBridge::Open() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    std::vector<unsigned char> user(size);
    const bool got = GetTokenInformation(token, TokenUser, user.data(), size, &size) != FALSE;
    CloseHandle(token);
    if (!got) return false;
    LPWSTR sid = nullptr;
    if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid, &sid)) return false;
    const std::wstring sddl = L"D:P(A;;GA;;;" + std::wstring(sid) + L")";
    LocalFree(sid);
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &descriptor, nullptr)) return false;
    SECURITY_ATTRIBUTES security { sizeof(SECURITY_ATTRIBUTES), descriptor, FALSE };
    const auto name = L"\\\\.\\pipe\\HitmanCoop.Launcher." + std::to_wstring(GetCurrentProcessId());
    m_Pipe = CreateNamedPipeW(name.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_NOWAIT | PIPE_REJECT_REMOTE_CLIENTS,
        1, 16384, 1024, 0, &security);
    LocalFree(descriptor);
    if (m_Pipe != INVALID_HANDLE_VALUE) CoopLog::Info("launcher", "local launcher bridge ready");
    return m_Pipe != INVALID_HANDLE_VALUE;
}

void LauncherBridge::Disconnect() {
    DisconnectNamedPipe(m_Pipe);
    m_ConnectedAt = -1;
    m_Replied = false;
}

void LauncherBridge::Poll(double now, const std::function<std::string(const coop::LauncherRequest&)>& handle) {
    if (now < m_NextPoll) return;
    m_NextPoll = now + 0.03;
    if (!m_Tried) { m_Tried = true; if (!Open()) CoopLog::Warn("launcher", "local launcher bridge unavailable"); }
    if (m_Pipe == INVALID_HANDLE_VALUE) return;
    if (m_ConnectedAt < 0) {
        if (!ConnectNamedPipe(m_Pipe, nullptr)) {
            const DWORD error = GetLastError();
            if (error != ERROR_PIPE_CONNECTED) {
                if (error == ERROR_NO_DATA || error == ERROR_BROKEN_PIPE) Disconnect();
                return;
            }
        }
        m_ConnectedAt = now;
    }
    char bytes[1025];
    DWORD count = 0;
    if (!ReadFile(m_Pipe, bytes, 1024, &count, nullptr)) {
        const DWORD error = GetLastError();
        if (error != ERROR_NO_DATA || now - m_ConnectedAt > 2.0) Disconnect();
        return;
    }
    if (m_Replied) { Disconnect(); return; } // acknowledgement consumed
    coop::LauncherRequest request;
    const std::string reply = coop::LauncherRequest::Parse(std::string(bytes, count), request)
        ? handle(request) : "{\"ok\":false,\"message\":\"Invalid launcher command\"}";
    if (reply.size() > 16000) { Disconnect(); return; }
    DWORD written = 0;
    if (!WriteFile(m_Pipe, reply.data(), static_cast<DWORD>(reply.size()), &written, nullptr) || written != reply.size()) {
        Disconnect(); return;
    }
    m_Replied = true;
    // Disconnect only after the client has read the response, without FlushFileBuffers
    // (which would block the game). The client sends a final one-byte acknowledgement.
    m_ConnectedAt = now;
    // Ack is processed on the next poll; stale clients are dropped after two seconds.
    m_NextPoll = now + 0.03;
}
