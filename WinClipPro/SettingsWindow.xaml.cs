using System.Windows;
using System.Windows.Input;
using System.Windows.Interop;

namespace WinClipPro;

public partial class SettingsWindow : Window
{
    public string HotkeyText { get; private set; } = "Alt + ,";
    public int RetentionDays { get; private set; } = 7;
    public bool AutoStart { get; private set; }

    public SettingsWindow()
    {
        InitializeComponent();
    }

    private void OnLoaded(object sender, RoutedEventArgs e)
    {
        var hwnd = new WindowInteropHelper(this).Handle;
        AcrylicHelper.ApplyAcrylic(hwnd);
    }

    private void OnDone(object sender, RoutedEventArgs e)
    {
        RetentionDays = RetentionBox.SelectedIndex switch
        {
            0 => 7, 1 => 14, 2 => 30, 3 => 90, 4 => -1, _ => 7
        };
        AutoStart = AutoStartSwitch.IsOn;
        Close();
    }

    private void OnChangeHotkey(object sender, RoutedEventArgs e)
    {
        System.Windows.MessageBox.Show("Hotkey customization coming soon.", "WinClip Pro");
    }

    private void OnDeactivated(object sender, EventArgs e) => Close();
}
