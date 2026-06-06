#pragma once
#include <windows.h>
#include <functional>
#include <string>

class ClipboardMonitor {
public:
    using Callback = std::function<void(const std::string& content,
                                        const std::string& contentType)>;

    ClipboardMonitor();
    ~ClipboardMonitor();

    bool start(HWND hwnd, Callback callback);
    void stop();
    void handleClipboardUpdate();

private:
    std::string getClipboardText();
    std::string getClipboardImagePath();
    std::string getClipboardFileList();

    HWND m_hwnd = nullptr;
    Callback m_callback;
    std::string m_lastContent;
    std::string m_lastContentType;
};
