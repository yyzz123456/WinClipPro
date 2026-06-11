#pragma once
#include <Windows.h>

#ifdef WINCLIPHOOK_EXPORTS
#define WINCLIPHOOK_API __declspec(dllexport)
#else
#define WINCLIPHOOK_API __declspec(dllimport)
#endif

// Callback types
typedef void (*ClipboardUpdateCallback)(const wchar_t* content, int contentType, long long timestamp, const wchar_t* hash);
typedef void(*HotkeyCallback)(int hotkeyId);

extern "C" {
    // Clipboard monitoring
    WINCLIPHOOK_API bool StartClipboardMonitor(ClipboardUpdateCallback callback);
    WINCLIPHOOK_API void StopClipboardMonitor();

    // Hotkey management
    WINCLIPHOOK_API void SetHotkeyCallback(HotkeyCallback callback);
    WINCLIPHOOK_API bool RegisterClipboardHotKey(int id, unsigned int modifiers, unsigned int vk);
    WINCLIPHOOK_API void UnregisterClipboardHotKey(int id);
    WINCLIPHOOK_API void UnregisterAllHotKeys(HWND hwnd);

    // Content utilities
    WINCLIPHOOK_API wchar_t* GetClipboardText();
    WINCLIPHOOK_API void FreeClipboardText(wchar_t* text);
}
