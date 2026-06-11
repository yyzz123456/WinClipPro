#pragma once
#include <Windows.h>
#include <vector>

class HotkeyManager
{
public:
    static HotkeyManager& Instance();

    void SetCallback(void (*callback)(int hotkeyId));
    bool Register(int id, unsigned int modifiers, unsigned int vk);
    void Unregister(int id);
    void UnregisterAll(HWND hwnd);

private:
    HotkeyManager() = default;
    HotkeyManager(const HotkeyManager&) = delete;
    HotkeyManager& operator=(const HotkeyManager&) = delete;

    struct HotkeyEntry
    {
        int id;
        unsigned int modifiers;
        unsigned int vk;
    };

    void (*m_callback)(int) = nullptr;
    std::vector<HotkeyEntry> m_hotkeys;
};
