#include "Exports.h"
#include "ClipboardMonitor.h"
#include "HotkeyManager.h"

bool StartClipboardMonitor(ClipboardUpdateCallback callback)
{
    return ClipboardMonitor::Instance().Start(callback);
}

void StopClipboardMonitor()
{
    ClipboardMonitor::Instance().Stop();
}

void SetHotkeyCallback(HotkeyCallback callback)
{
    HotkeyManager::Instance().SetCallback(callback);
}

bool RegisterClipboardHotKey(int id, unsigned int modifiers, unsigned int vk)
{
    return HotkeyManager::Instance().Register(id, modifiers, vk);
}

void UnregisterClipboardHotKey(int id)
{
    HotkeyManager::Instance().Unregister(id);
}

void UnregisterAllHotKeys(HWND hwnd)
{
    HotkeyManager::Instance().UnregisterAll(hwnd);
}

wchar_t* GetClipboardText()
{
    if (!OpenClipboard(nullptr)) return nullptr;

    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (!hData)
    {
        CloseClipboard();
        return nullptr;
    }

    wchar_t* src = static_cast<wchar_t*>(GlobalLock(hData));
    if (!src)
    {
        CloseClipboard();
        return nullptr;
    }

    size_t len = wcslen(src);
    wchar_t* result = static_cast<wchar_t*>(CoTaskMemAlloc((len + 1) * sizeof(wchar_t)));
    if (result) wcscpy_s(result, len + 1, src);

    GlobalUnlock(hData);
    CloseClipboard();
    return result;
}

void FreeClipboardText(wchar_t* text)
{
    if (text) CoTaskMemFree(text);
}
