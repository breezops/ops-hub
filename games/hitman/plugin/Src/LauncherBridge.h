#pragma once
#include <Windows.h>
#include <functional>
#include <string>
#include "coop/LauncherProtocol.h"

// Nonblocking, current-Windows-user-only message pipe. Runs on the game thread;
// a stalled launcher cannot make the game wait for IO or a worker thread.
class LauncherBridge {
public:
    ~LauncherBridge();
    void Poll(double now, const std::function<std::string(const coop::LauncherRequest&)>& handle);
private:
    HANDLE m_Pipe = INVALID_HANDLE_VALUE;
    double m_NextPoll = 0, m_ConnectedAt = -1;
    bool m_Tried = false, m_Replied = false;
    bool Open();
    void Disconnect();
};
