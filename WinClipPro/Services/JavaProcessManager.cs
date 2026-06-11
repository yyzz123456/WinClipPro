using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;

namespace WinClipPro.Services;

public class JavaProcessManager : IDisposable
{
    private Process? _process;
    private readonly string _baseDir;
    private readonly string _jarPath;
    private readonly string _classpathDir;

    public JavaProcessManager()
    {
        _baseDir = Path.GetFullPath(Path.Combine(
            AppDomain.CurrentDomain.BaseDirectory, "..", "..", "..", ".."));
        _jarPath = Path.Combine(_baseDir, "JavaBackend", "out", "production", "JavaBackend");
        _classpathDir = _jarPath;
    }

    public async Task<bool> StartAsync()
    {
        // Check if Java backend is already running
        using var checkClient = new TcpClientService();
        var result = await checkClient.QueryAsync(0, 1);
        if (result != null)
        {
            Debug.WriteLine("Java backend already running");
            return true;
        }

        try
        {
            var libPath = Path.Combine(_baseDir, "lib", "*");
            var classpath = $"{libPath};{_classpathDir}";

            var psi = new ProcessStartInfo
            {
                FileName = "java",
                Arguments = $"-cp \"{classpath}\" Main",
                WorkingDirectory = _baseDir,
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            };

            _process = Process.Start(psi);
            if (_process == null) return false;

            // Wait for server to be ready
            for (int i = 0; i < 20; i++)
            {
                await Task.Delay(250);
                using var client = new TcpClientService();
                var check = await client.QueryAsync(0, 1);
                if (check != null) return true;
            }

            return false;
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"Failed to start Java: {ex.Message}");
            return false;
        }
    }

    public void Stop()
    {
        if (_process is { HasExited: false })
        {
            _process.Kill(entireProcessTree: true);
            _process.Dispose();
            _process = null;
        }
    }

    public void Dispose()
    {
        Stop();
    }
}
