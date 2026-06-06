#pragma once
#include <windows.h>
#include <functional>

class HotkeyManager {
public:
    using Callback = std::function<void()>;

    HotkeyManager();
    ~HotkeyManager();

    bool registerHotkey(HWND hwnd, UINT modifiers, UINT vk, int id, Callback callback);
    void unregister(HWND hwnd, int id);
    void handleHotkey(int id);

private:
    Callback m_callback;
    int m_id = 0;
};
