using System.IO.Pipes;
using System.Windows;
using WinClipPro.Services;
using Application = System.Windows.Application;

namespace WinClipPro;

public partial class App : Application
{
    private static readonly string MutexName = "WinClipPro_SingleInstance";
    private Mutex? _mutex;
    private JavaProcessManager? _javaManager;

    protected override async void OnStartup(StartupEventArgs e)
    {
        _mutex = new Mutex(true, MutexName, out bool createdNew);
        if (!createdNew)
        {
            // Signal existing instance to show window
            try
            {
                using var client = new NamedPipeClientStream(".", "WinClipPro_ShowWindow", PipeDirection.Out);
                client.Connect(1000);
                client.WriteByte(1);
            }
            catch { }
            Shutdown();
            return;
        }

        _javaManager = new JavaProcessManager();
        await _javaManager.StartAsync();

        base.OnStartup(e);
    }

    protected override void OnExit(ExitEventArgs e)
    {
        _javaManager?.Dispose();
        NativeClipboardService.StopClipboardMonitor();
        _mutex?.Dispose();
        base.OnExit(e);
    }
}
