using System.IO;
using System.Text.Json;

namespace WinClipPro.Services;

public class AppSettings
{
    public int RetentionDays { get; set; } = 7;
    public bool AutoStart { get; set; }

    private static string FilePath =>
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "WinClipPro", "settings.json");

    public static AppSettings Load()
    {
        try
        {
            if (File.Exists(FilePath))
            {
                var json = File.ReadAllText(FilePath);
                return JsonSerializer.Deserialize<AppSettings>(json) ?? new AppSettings();
            }
        }
        catch { }
        return new AppSettings();
    }

    public static void Save(int retentionDays, bool autoStart)
    {
        try
        {
            var dir = Path.GetDirectoryName(FilePath);
            if (dir != null) Directory.CreateDirectory(dir);
            var settings = new AppSettings { RetentionDays = retentionDays, AutoStart = autoStart };
            var json = JsonSerializer.Serialize(settings);
            File.WriteAllText(FilePath, json);

            // Auto-start registry
            SetAutoStart(autoStart);
        }
        catch { }
    }

    private static void SetAutoStart(bool enable)
    {
        try
        {
            var rk = Microsoft.Win32.Registry.CurrentUser.OpenSubKey(
                @"SOFTWARE\Microsoft\Windows\CurrentVersion\Run", true);
            if (rk == null) return;

            if (enable)
                rk.SetValue("WinClipPro", Environment.ProcessPath ?? "");
            else
                rk.DeleteValue("WinClipPro", false);
        }
        catch { }
    }
}
