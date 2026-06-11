using System.Diagnostics;
using System.IO;

namespace WinClipPro.Services;

public class JavaProcessManager : IDisposable
{
    private Process? _process;

    public async Task<bool> StartAsync()
    {
        using var checkClient = new TcpClientService();
        var result = await checkClient.QueryAsync(0, 1);
        if (result != null)
        {
            Debug.WriteLine("Java backend already running");
            return true;
        }

        try
        {
            // Find the Java backend directory relative to the EXE
            var exeDir = AppDomain.CurrentDomain.BaseDirectory;
            var javaDir = FindJavaDir(exeDir);
            if (javaDir == null) return false;

            var libPath = Path.Combine(javaDir, "lib", "*");
            var classpathDir = Path.Combine(javaDir, "JavaBackend", "out", "production", "JavaBackend");
            var classpath = $"{libPath};{classpathDir}";

            var psi = new ProcessStartInfo
            {
                FileName = "java",
                Arguments = $"-cp \"{classpath}\" Main",
                WorkingDirectory = javaDir,
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            };

            _process = Process.Start(psi);
            if (_process == null) return false;

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

    private static string? FindJavaDir(string startDir)
    {
        // Walk up from EXE dir looking for JavaBackend/out/production/JavaBackend
        var dir = startDir;
        for (int i = 0; i < 6; i++)
        {
            var javaBackendDir = Path.Combine(dir, "JavaBackend", "out", "production", "JavaBackend");
            if (Directory.Exists(javaBackendDir)) return dir;
            var parent = Directory.GetParent(dir);
            if (parent == null) break;
            dir = parent.FullName;
        }
        return null;
    }

    public void Stop()
    {
        if (_process is { HasExited: false })
        {
            try
            {
                _process.Kill(entireProcessTree: true);
            }
            catch { }
            _process.Dispose();
            _process = null;
        }
    }

    public void Dispose() => Stop();
}
