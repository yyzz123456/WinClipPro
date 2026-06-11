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

namespace WinClipPro;

public partial class MainWindow : Window
{
    private readonly TcpClientService _tcp;
    private readonly ObservableCollection<ClipboardItem> _items = new();
    private H.NotifyIcon.TaskbarIcon? _trayIcon;
    private System.Timers.Timer? _debounceTimer;
    private NativeClipboardService.ClipboardUpdateDelegate? _clipboardCallback;
    private bool _isClosing;
    private bool _isAnimating;
    private IntPtr _hwnd;
    private double _targetTop;
    private double _hiddenTop;

    public MainWindow()
    {
        InitializeComponent();
        _tcp = new TcpClientService();

        try { _trayIcon = CreateTrayIcon(); } catch { }

        _ = ListenForShowSignalAsync();

        SourceInitialized += OnSourceInitialized;

        _debounceTimer = new System.Timers.Timer(200) { AutoReset = false };
        _debounceTimer.Elapsed += (_, _) => Dispatcher.Invoke(async () => await DoSearchAsync());
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

    private IntPtr WndProcHook(IntPtr hwnd, int msg, IntPtr wParam, IntPtr lParam, ref bool handled)
    {
        const int WM_HOTKEY = 0x0312;
        if (msg == WM_HOTKEY && wParam.ToInt32() == 1)
        {
            Dispatcher.Invoke(async () => await ShowWithAnimation());
            handled = true;
        }
        return IntPtr.Zero;
    }

    private void StartNativeClipboardMonitor()
    {
        _clipboardCallback = (content, _, timestamp, hash) =>
        {
            Dispatcher.Invoke(async () =>
            {
                try
                {
                    var id = await _tcp.SaveAsync(content);
                    if (id != null)
                    {
                        StatusText.Text = "Clipboard saved";
                        if (IsVisible) await LoadItemsAsync();
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
        SearchBox.Focus();

        // Slide up + fade in
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

        // Fade out + slide down
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
        if (ClipboardList.SelectedItem is ClipboardItem item)
        {
            try
            {
                Clipboard.SetText(item.Content);
                StatusText.Text = "Copied!";
                await Task.Delay(1500);
                StatusText.Text = "Ready";
            }
            catch { }
            ClipboardList.SelectedIndex = -1;
        }
    }

    private async void OnCopyItem(object sender, RoutedEventArgs e)
    {
        if (((MenuItem)sender).DataContext is ClipboardItem item)
        {
            Clipboard.SetText(item.Content);
            StatusText.Text = "Copied!";
            await Task.Delay(1500);
            StatusText.Text = "Ready";
        }
    }

    private async void OnTogglePin(object sender, RoutedEventArgs e)
    {
        if (((MenuItem)sender).DataContext is ClipboardItem item)
        {
            bool newState = !item.Pinned;
            await _tcp.PinAsync(item.Id, newState);
            item.IsPinned = newState ? 1 : 0;
            ClipboardList.Items.Refresh();
        }
    }

    private async void OnDeleteItem(object sender, RoutedEventArgs e)
    {
        if (((MenuItem)sender).DataContext is ClipboardItem item)
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

    private async void OnHideWindow(object sender, RoutedEventArgs e)
    {
        await HideWithAnimation();
    }

    private void OnToggleTopmost(object sender, RoutedEventArgs e) => Topmost = !Topmost;

    private async void OnDeactivated(object sender, EventArgs e)
    {
        if (!_isClosing)
            await HideWithAnimation();
    }

    protected override void OnClosing(CancelEventArgs e)
    {
        _isClosing = true;
        UnregisterHotKey(_hwnd, 1);
        NativeClipboardService.StopClipboardMonitor();
        _trayIcon?.Dispose();
        base.OnClosing(e);
    }

    public void ShowAndFocus()
    {
        _ = ShowWithAnimation();
    }

    private H.NotifyIcon.TaskbarIcon CreateTrayIcon()
    {
        var icon = new H.NotifyIcon.TaskbarIcon
        {
            ToolTipText = "WinClip Pro",
            Visibility = Visibility.Visible
        };

        using var ms = new MemoryStream();
        DrawTrayIcon(ms);
        ms.Position = 0;
        var bmp = new System.Drawing.Bitmap(ms);
        icon.Icon = System.Drawing.Icon.FromHandle(bmp.GetHicon());

        var contextMenu = new ContextMenu();
        var showItem = new MenuItem { Header = "Show / Hide" };
        showItem.Click += async (_, _) =>
        {
            if (IsVisible) await HideWithAnimation();
            else await ShowWithAnimation();
        };
        contextMenu.Items.Add(showItem);

        var exitItem = new MenuItem { Header = "Exit" };
        exitItem.Click += (_, _) =>
        {
            _isClosing = true;
            _trayIcon?.Dispose();
            Application.Current.Shutdown();
        };
        contextMenu.Items.Add(exitItem);

        icon.ContextMenu = contextMenu;
        icon.TrayLeftMouseDown += async (_, _) =>
        {
            if (IsVisible) await HideWithAnimation();
            else await ShowWithAnimation();
        };

        return icon;
    }

    private static void DrawTrayIcon(MemoryStream ms)
    {
        const int size = 32;
        using var bmp = new System.Drawing.Bitmap(size, size);
        using var g = System.Drawing.Graphics.FromImage(bmp);
        g.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;
        g.Clear(System.Drawing.Color.Transparent);

        using var pen = new System.Drawing.Pen(System.Drawing.Color.White, 2);
        g.DrawRectangle(pen, 6, 4, 20, 24);
        g.DrawLine(pen, 10, 14, 22, 14);
        g.DrawLine(pen, 10, 20, 18, 20);
        bmp.Save(ms, System.Drawing.Imaging.ImageFormat.Png);
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
                Dispatcher.Invoke(async () => await ShowWithAnimation());
            }
            catch { }
        }
    }
}
