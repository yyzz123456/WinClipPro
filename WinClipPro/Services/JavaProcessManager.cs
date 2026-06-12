using System.Diagnostics;
using System.IO;

namespace WinClipPro.Services;

public class JavaProcessManager : IDisposable
{
    private Process? _process;

    public async Task<bool> StartAsync()
    {
        var logPath = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "WinClipPro", "startup.log");
        void Log(string msg)
        {
            try { Directory.CreateDirectory(Path.GetDirectoryName(logPath)!); File.AppendAllText(logPath, $"{DateTime.Now:HH:mm:ss} {msg}\n"); } catch { }
        }

        Log("StartAsync called");
        using var checkClient = new TcpClientService();
        var check = await checkClient.SendAsync("query", new { lastId = 0, limit = 1 });
        if (check != null) { Log("Java already running"); return true; }
        Log("Java not running, will start");

        try
        {
            var exeDir = AppDomain.CurrentDomain.BaseDirectory;
            Log($"BaseDirectory: {exeDir}");
            var javaDir = FindJavaDir(exeDir);
            if (javaDir == null) { Log("Java dir not found"); return false; }

            Log($"Java dir: {javaDir}");

            // Prefer bundled JRE, fall back to system PATH
            var bundledJava = Path.Combine(javaDir, "JavaBackend", "jre", "bin", "java.exe");
            var javaExe = File.Exists(bundledJava) ? bundledJava : "java";
            Log($"Using Java: {javaExe}");

            var libPath = Path.Combine(javaDir, "JavaBackend", "lib", "*");
            var classpathDir = Path.Combine(javaDir, "JavaBackend", "out", "production", "JavaBackend");
            var classpath = $"{libPath};{classpathDir}";
            Log($"Classpath: {classpath}");

            var psi = new ProcessStartInfo
            {
                FileName = javaExe,
                Arguments = $"-cp \"{classpath}\" Main",
                WorkingDirectory = javaDir,
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            };

            _process = Process.Start(psi);
            if (_process == null) { Log("Process.Start returned null"); return false; }
            Log($"Java process started PID: {_process.Id}");

            for (int i = 0; i < 20; i++)
            {
                await Task.Delay(250);
                using var client = new TcpClientService();
                var c = await client.SendAsync("query", new { lastId = 0, limit = 1 });
                if (c != null) { Log("TCP check OK"); return true; }
            }

            Log("TCP check timeout");
            return false;
        }
        catch (Exception ex)
        {
            Log($"Exception: {ex}");
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
            Debug.WriteLine($"FindJavaDir checking: {javaBackendDir}");
            if (Directory.Exists(javaBackendDir))
            {
                Debug.WriteLine($"Found Java at: {dir}");
                return dir;
            }
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
