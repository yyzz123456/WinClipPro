#include "HotkeyManager.h"

HotkeyManager::HotkeyManager() {}
HotkeyManager::~HotkeyManager() {}

bool HotkeyManager::registerHotkey(HWND hwnd, UINT modifiers, UINT vk, int id, Callback callback) {
    m_id = id;
    m_callback = callback;
    return RegisterHotKey(hwnd, id, modifiers, vk);
}

void HotkeyManager::unregister(HWND hwnd, int id) {
    UnregisterHotKey(hwnd, id);
}

void HotkeyManager::handleHotkey(int id) {
    if (id == m_id && m_callback)
        m_callback();
}
