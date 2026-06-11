#pragma once
#include <Windows.h>

class ClipboardMonitor
{
public:
    static ClipboardMonitor& Instance();

    bool Start(void (*callback)(const wchar_t* content, int contentType, long long timestamp, const wchar_t* hash));
    void Stop();
    bool IsRunning() const { return m_running; }

private:
    ClipboardMonitor() = default;
    ~ClipboardMonitor();
    ClipboardMonitor(const ClipboardMonitor&) = delete;
    ClipboardMonitor& operator=(const ClipboardMonitor&) = delete;

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    void NotifyClipboardChange();

    HWND m_hwnd = nullptr;
    void (*m_callback)(const wchar_t*, int, long long, const wchar_t*) = nullptr;
    bool m_running = false;
};
