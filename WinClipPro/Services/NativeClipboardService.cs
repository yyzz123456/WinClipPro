using System.Runtime.InteropServices;

namespace WinClipPro.Services;

public static class NativeClipboardService
{
    public delegate void ClipboardUpdateDelegate(
        [MarshalAs(UnmanagedType.LPWStr)] string content,
        int contentType,
        long timestamp,
        [MarshalAs(UnmanagedType.LPWStr)] string hash);

    public delegate void HotkeyDelegate(int hotkeyId);

    [DllImport("WinClipHook.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern bool StartClipboardMonitor(ClipboardUpdateDelegate callback);

    [DllImport("WinClipHook.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern void StopClipboardMonitor();

    [DllImport("WinClipHook.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern void SetHotkeyCallback(HotkeyDelegate callback);

    [DllImport("WinClipHook.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern bool RegisterClipboardHotKey(int id, uint modifiers, uint vk);

    [DllImport("WinClipHook.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern void UnregisterClipboardHotKey(int id);

    [DllImport("WinClipHook.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern void UnregisterAllHotKeys(IntPtr hwnd);

    // Modifier constants for RegisterHotKey
    public const uint MOD_ALT = 0x0001;
    public const uint MOD_CONTROL = 0x0002;
    public const uint MOD_SHIFT = 0x0004;
    public const uint MOD_WIN = 0x0008;
}
