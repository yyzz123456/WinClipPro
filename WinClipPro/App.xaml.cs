using System.IO.Pipes;
using System.Windows;
using WinClipPro.Services;

namespace WinClipPro;

public partial class App : Application
{
    private static readonly string MutexName = "WinClipPro_SingleInstance";
    private JavaProcessManager? _javaManager;

    protected override async void OnStartup(StartupEventArgs e)
    {
        using var mutex = new Mutex(true, MutexName, out bool createdNew);
        if (!createdNew)
        {
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

        // Start Java backend
        _javaManager = new JavaProcessManager();
        await _javaManager.StartAsync();

        base.OnStartup(e);
    }

    protected override void OnExit(ExitEventArgs e)
    {
        _javaManager?.Dispose();
        NativeClipboardService.StopClipboardMonitor();
        base.OnExit(e);
    }
}
