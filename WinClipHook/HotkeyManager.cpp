#include "HotkeyManager.h"
#include <algorithm>

HotkeyManager& HotkeyManager::Instance()
{
    static HotkeyManager instance;
    return instance;
}

void HotkeyManager::SetCallback(void (*callback)(int))
{
    m_callback = callback;
}

bool HotkeyManager::Register(int id, unsigned int modifiers, unsigned int vk)
{
    // Use HWND_BROADCAST with a unique ID; the WPF side will receive WM_HOTKEY
    // Or we can create a dedicated message-only window
    // For simplicity, register with nullptr (caller handles WM_HOTKEY in their message loop)
    if (!RegisterHotKey(nullptr, id, modifiers, vk))
        return false;

    m_hotkeys.push_back({ id, modifiers, vk });
    return true;
}

void HotkeyManager::Unregister(int id)
{
    UnregisterHotKey(nullptr, id);
    auto it = std::find_if(m_hotkeys.begin(), m_hotkeys.end(),
        [id](const HotkeyEntry& e) { return e.id == id; });
    if (it != m_hotkeys.end()) m_hotkeys.erase(it);
}

void HotkeyManager::UnregisterAll(HWND hwnd)
{
    for (const auto& hotkey : m_hotkeys)
        UnregisterHotKey(hwnd, hotkey.id);
    m_hotkeys.clear();
}
