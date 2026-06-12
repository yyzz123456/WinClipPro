using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.IO.Pipes;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Interop;
using System.Windows.Media;

using WinClipPro.Models;
using WinClipPro.Services;
using Clipboard = System.Windows.Clipboard;
using MenuItem = System.Windows.Controls.MenuItem;
using Application = System.Windows.Application;

namespace WinClipPro;

public partial class MainWindow : Window
{
    private readonly TcpClientService _tcp;
    private readonly ObservableCollection<ClipboardItem> _items = new();
    private System.Windows.Forms.NotifyIcon? _trayIcon;
    private System.Timers.Timer? _debounceTimer;
    private NativeClipboardService.ClipboardUpdateDelegate? _clipboardCallback;
    private bool _isClosing;
    private bool _isAnimating;
    private IntPtr _hwnd;
    private double _targetTop;
    private double _hiddenTop;
    private bool _isSelecting;
    private bool _isPasting;
    private bool _selfCopy;

    public MainWindow()
    {
        InitializeComponent();
        _tcp = new TcpClientService();

        CreateTrayIcon();

        _ = ListenForShowSignalAsync();

        SourceInitialized += OnSourceInitialized;

        _debounceTimer = new System.Timers.Timer(200) { AutoReset = false };
        _debounceTimer.Elapsed += (_, _) => RunOnUi(async () => await DoSearchAsync());

        Closing += (_, _) =>
        {
            _isClosing = true;
            UnregisterHotKey(_hwnd, 1);
            NativeClipboardService.StopClipboardMonitor();
            _trayIcon?.Dispose();
        };
    }

    private void OnSourceInitialized(object? sender, EventArgs e)
    {
        _hwnd = new WindowInteropHelper(this).Handle;
        AcrylicHelper.ApplyAcrylic(_hwnd);
        var src = HwndSource.FromHwnd(_hwnd);
        if (src != null)
            src.CompositionTarget!.BackgroundColor = Colors.Transparent;

        StartNativeClipboardMonitor();

        var hook = new HwndSourceHook(WndProcHook);
        src?.AddHook(hook);
        RegisterHotKey(_hwnd, 1, 0x0001, 0xBC); // MOD_ALT, VK_OEM_COMMA
    }

    [DllImport("user32.dll")]
    private static extern bool RegisterHotKey(IntPtr hWnd, int id, uint fsModifiers, uint vk);

    [DllImport("user32.dll")]
    private static extern bool UnregisterHotKey(IntPtr hWnd, int id);

    [DllImport("user32.dll")]
    private static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    private const int SW_HIDE = 0;
    private const int SW_SHOW = 5;


    private IntPtr WndProcHook(IntPtr hwnd, int msg, IntPtr wParam, IntPtr lParam, ref bool handled)
    {
        const int WM_HOTKEY = 0x0312;
        if (msg == WM_HOTKEY && wParam.ToInt32() == 1)
        {
            RunOnUi(async () => await ShowWithAnimation());
            handled = true;
        }
        return IntPtr.Zero;
    }

    private void StartNativeClipboardMonitor()
    {
        _clipboardCallback = (content, _, timestamp, hash) =>
        {
            if (_selfCopy) return;

            RunOnUi(async () =>
            {
                try
                {
                    var id = await _tcp.SaveAsync(content);
                    if (id != null)
                    {
                        StatusText.Text = "Clipboard saved";
                        if (IsVisible) await LoadItemsAsync();
                        await PruneIfNeeded();
                    }
                }
                catch { }
            });
        };

        GC.KeepAlive(_clipboardCallback);
        NativeClipboardService.StartClipboardMonitor(_clipboardCallback);
    }

    private double CalculateTargetTop()
    {
        var workArea = SystemParameters.WorkArea;
        return workArea.Bottom - Height - 10;
    }

    private double CalculateTargetLeft()
    {
        var workArea = SystemParameters.WorkArea;
        return workArea.Right - Width - 10;
    }

    public async Task ShowWithAnimation()
    {
        if (_isAnimating) return;
        _isAnimating = true;

        _targetTop = CalculateTargetTop();
        _hiddenTop = _targetTop + Height + 20;

        Left = CalculateTargetLeft();
        Top = _hiddenTop;
        Opacity = 0;
        Show();
        Activate();

        for (int i = 1; i <= 8; i++)
        {
            double t = i / 8.0;
            Top = _hiddenTop + (_targetTop - _hiddenTop) * EaseOutCubic(t);
            Opacity = t;
            await Task.Delay(12);
        }
        Top = _targetTop;
        Opacity = 1;

        _isAnimating = false;
        await LoadItemsAsync();
    }

    public async Task HideWithAnimation()
    {
        if (_isAnimating || !IsVisible) return;
        _isAnimating = true;

        for (int i = 1; i <= 6; i++)
        {
            double t = i / 6.0;
            Opacity = 1 - t;
            Top = _targetTop + (Height + 20) * EaseInCubic(t);
            await Task.Delay(10);
        }
        Opacity = 0;
        Hide();

        _isAnimating = false;
    }

    private static double EaseOutCubic(double t) => 1 - Math.Pow(1 - t, 3);
    private static double EaseInCubic(double t) => Math.Pow(t, 3);

    private async Task LoadItemsAsync()
    {
        try
        {
            var items = await _tcp.QueryAsync(0, 100);
            _items.Clear();
            foreach (var item in items) _items.Add(item);
            ClipboardList.ItemsSource = _items;
            ItemCount.Text = $"{_items.Count} items";
        }
        catch
        {
            StatusText.Text = "Backend not connected";
        }
    }

    private void OnSearchTextChanged(object sender, TextChangedEventArgs e)
    {
        UpdatePlaceholderVisibility();
        _debounceTimer?.Stop();
        _debounceTimer?.Start();
    }

    private void OnSearchGotFocus(object sender, RoutedEventArgs e) => UpdatePlaceholderVisibility();
    private void OnSearchLostFocus(object sender, RoutedEventArgs e) => UpdatePlaceholderVisibility();

    private void UpdatePlaceholderVisibility()
    {
        PlaceholderText.Visibility = string.IsNullOrEmpty(SearchBox.Text) && !SearchBox.IsFocused
            ? Visibility.Visible : Visibility.Hidden;
    }

    private async Task DoSearchAsync()
    {
        var keyword = SearchBox.Text.Trim();
        if (string.IsNullOrEmpty(keyword))
        {
            await LoadItemsAsync();
            return;
        }

        try
        {
            var items = await _tcp.SearchAsync(keyword, 50);
            _items.Clear();
            foreach (var item in items) _items.Add(item);
            ItemCount.Text = $"{_items.Count} results";
        }
        catch { }
    }

    private async void OnItemSelected(object sender, SelectionChangedEventArgs e)
    {
        if (_isSelecting || e.AddedItems.Count == 0) return;
        if (ClipboardList.SelectedItem is not ClipboardItem item) return;

        _isSelecting = true;
        try
        {
            _selfCopy = true;
            Clipboard.SetText(item.Content);
            StatusText.Text = "Copied!";
            if (!_isPasting)
            {
                _isPasting = true;
                ShowWindow(_hwnd, SW_HIDE);
                System.Windows.Forms.SendKeys.SendWait("^v");
                ShowWindow(_hwnd, SW_SHOW);
                _isPasting = false;
            }
        }
        catch { }
        finally
        {
            ClipboardList.SelectedIndex = -1;
            _isSelecting = false;
            _selfCopy = false;
        }
    }

    private async void OnCopyItem(object sender, RoutedEventArgs e)
    {
        var item = (sender as FrameworkElement)?.DataContext as ClipboardItem;
        if (item != null && !_isPasting)
        {
            _isPasting = true;
            _selfCopy = true;
            Clipboard.SetText(item.Content);
            StatusText.Text = "Copied!";
            ShowWindow(_hwnd, SW_HIDE);
            System.Windows.Forms.SendKeys.SendWait("^v");
            ShowWindow(_hwnd, SW_SHOW);
            _isPasting = false;
            _selfCopy = false;
        }
    }

    private async void OnTogglePin(object sender, RoutedEventArgs e)
    {
        var item = (sender as FrameworkElement)?.DataContext as ClipboardItem;
        if (item != null)
        {
            bool newState = !item.Pinned;
            await _tcp.PinAsync(item.Id, newState);
            item.IsPinned = newState ? 1 : 0;
            ClipboardList.Items.Refresh();
        }
    }

    private async void OnDeleteItem(object sender, RoutedEventArgs e)
    {
        var item = (sender as FrameworkElement)?.DataContext as ClipboardItem;
        if (item != null)
        {
            await _tcp.DeleteAsync(item.Id);
            _items.Remove(item);
            ItemCount.Text = $"{_items.Count} items";
        }
    }

    private void OnTitleBarDrag(object sender, MouseButtonEventArgs e)
    {
        if (e.LeftButton == MouseButtonState.Pressed)
            DragMove();
    }

    private async void OnHideWindow(object sender, RoutedEventArgs e) => await HideWithAnimation();

    private void OnOpenSettings(object sender, RoutedEventArgs e)
    {
        var settings = new SettingsWindow { Owner = this };
        settings.ShowDialog();
    }

    private void OnToggleTopmost(object sender, RoutedEventArgs e)
    {
        Topmost = !Topmost;
        if (Topmost)
        {
            PinBtn.Background = new SolidColorBrush(System.Windows.Media.Color.FromArgb(0x40, 0, 0, 0));
            PinBtn.Foreground = new SolidColorBrush(System.Windows.Media.Color.FromArgb(0xCC, 0, 0, 0));
        }
        else
        {
            PinBtn.Background = System.Windows.Media.Brushes.Transparent;
            PinBtn.Foreground = new SolidColorBrush(System.Windows.Media.Color.FromArgb(0x80, 0, 0, 0));
        }
    }

    private async void OnDeactivated(object sender, EventArgs e)
    {
        if (!_isClosing && !_isSelecting && !Topmost)
            await HideWithAnimation();
    }

    public void ShowAndFocus() => _ = ShowWithAnimation();

    private void RunOnUi(Func<Task> action) => Dispatcher.BeginInvoke(() => _ = action());

    private async Task PruneIfNeeded()
    {
        try
        {
            var settings = AppSettings.Load();
            if (settings.MaxItems <= 0) return;

            var items = await _tcp.QueryAsync(0, settings.MaxItems + 100);
            var excess = items.Skip(settings.MaxItems).ToList();
            foreach (var item in excess)
                await _tcp.DeleteAsync(item.Id);
        }
        catch { }
    }

    private void CreateTrayIcon()
    {
        _trayIcon = new System.Windows.Forms.NotifyIcon
        {
            Text = "WinClip Pro",
            Visible = true,
            Icon = LoadTrayIcon()
        };

        var menu = new System.Windows.Forms.ContextMenuStrip();
        var showItem = menu.Items.Add("Show / Hide");
        showItem.Click += async (_, _) =>
        {
            if (IsVisible) await HideWithAnimation();
            else await ShowWithAnimation();
        };

        menu.Items.Add(new System.Windows.Forms.ToolStripSeparator());

        var exitItem = menu.Items.Add("Exit");
        exitItem.Click += (_, _) =>
        {
            _isClosing = true;
            _trayIcon?.Dispose();
            Application.Current.Shutdown();
        };

        _trayIcon.ContextMenuStrip = menu;

        _trayIcon.MouseClick += async (_, e) =>
        {
            if (e.Button == System.Windows.Forms.MouseButtons.Left)
            {
                if (IsVisible) await HideWithAnimation();
                else await ShowWithAnimation();
            }
        };
    }

    private static System.Drawing.Icon LoadTrayIcon()
    {
        try
        {
            var iconPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "Resources", "app.ico");
            if (File.Exists(iconPath))
                return new System.Drawing.Icon(iconPath);
        }
        catch { }
        return System.Drawing.SystemIcons.Application;
    }

    private async Task ListenForShowSignalAsync()
    {
        while (!_isClosing)
        {
            try
            {
                using var server = new NamedPipeServerStream("WinClipPro_ShowWindow",
                    PipeDirection.In, 1, PipeTransmissionMode.Byte,
                    PipeOptions.Asynchronous | PipeOptions.CurrentUserOnly);
                await server.WaitForConnectionAsync();
                RunOnUi(async () => await ShowWithAnimation());
            }
            catch { }
        }
    }
}
