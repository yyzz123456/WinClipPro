using System.Windows;
using System.Windows.Input;
using System.Windows.Interop;
using WinClipPro.Services;

namespace WinClipPro;

public partial class SettingsWindow : Window
{
    public string HotkeyText { get; private set; } = "Alt + ,";
    public int RetentionDays { get; private set; } = 7;
    public bool AutoStart { get; private set; }
    public int MaxItems { get; private set; } = 500;

    private bool _isClosing;

    public SettingsWindow()
    {
        InitializeComponent();
    }

    private void OnLoaded(object sender, RoutedEventArgs e)
    {
        var hwnd = new WindowInteropHelper(this).Handle;
        AcrylicHelper.ApplyAcrylic(hwnd);

        var s = AppSettings.Load();
        RetentionBox.SelectedIndex = s.RetentionDays switch
        {
            14 => 1, 30 => 2, 90 => 3, -1 => 4, _ => 0
        };
        AutoStartSwitch.IsOn = s.AutoStart;
        MaxItemsBox.SelectedIndex = s.MaxItems switch
        {
            100 => 0, 200 => 1, 1000 => 3, 5000 => 4, -1 => 5, _ => 2 // default 500
        };
    }

    private void Done()
    {
        _isClosing = true;
        RetentionDays = RetentionBox.SelectedIndex switch
        {
            0 => 7, 1 => 14, 2 => 30, 3 => 90, 4 => -1, _ => 7
        };
        AutoStart = AutoStartSwitch.IsOn;
        MaxItems = MaxItemsBox.SelectedIndex switch
        {
            0 => 100, 1 => 200, 2 => 500, 3 => 1000, 4 => 5000, 5 => -1, _ => 500
        };
        AppSettings.Save(RetentionDays, AutoStart, MaxItems);
        Close();
    }

    private void OnDone(object sender, RoutedEventArgs e) => Done();

    private void OnChangeHotkey(object sender, RoutedEventArgs e)
    {
        System.Windows.MessageBox.Show("Hotkey customization coming soon.", "WinClip Pro");
    }

    private void OnDeactivated(object sender, EventArgs e)
    {
        if (!_isClosing) Done();
    }
}
