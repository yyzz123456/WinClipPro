using System.IO;
using System.Net.Sockets;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using WinClipPro.Models;

namespace WinClipPro.Services;

public class TcpClientService : IDisposable
{
    private readonly string _host;
    private readonly int _port;
    private readonly JsonSerializerOptions _jsonOptions;

    public TcpClientService(string host = "127.0.0.1", int port = 9099)
    {
        _host = host;
        _port = port;
        _jsonOptions = new JsonSerializerOptions
        {
            PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
            DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
        };
    }

    public async Task<JsonDocument?> SendAsync(string type, object? data = null)
    {
        try
        {
            using var client = new TcpClient();
            await client.ConnectAsync(_host, _port).WaitAsync(TimeSpan.FromSeconds(3));

            using var stream = client.GetStream();
            var request = new Dictionary<string, object?> { ["type"] = type };
            if (data != null) request["data"] = data;

            var json = JsonSerializer.Serialize(request, _jsonOptions) + "\n";
            var bytes = Encoding.UTF8.GetBytes(json);
            await stream.WriteAsync(bytes);

            using var reader = new StreamReader(stream, Encoding.UTF8);
            var responseLine = await reader.ReadLineAsync().WaitAsync(TimeSpan.FromSeconds(3));
            if (responseLine == null) return null;

            return JsonDocument.Parse(responseLine);
        }
        catch
        {
            return null;
        }
    }

    public async Task<int?> SaveAsync(string content, string contentType = "text")
    {
        var result = await SendAsync("save", new
        {
            content,
            contentType,
            timestamp = DateTimeOffset.UtcNow.ToUnixTimeSeconds()
        });
        if (result == null) return null;
        var root = result.RootElement;
        if (root.TryGetProperty("status", out var status) && status.GetString() == "ok")
        {
            if (root.TryGetProperty("data", out var data) &&
                data.TryGetProperty("id", out var id))
                return id.GetInt32();
        }
        return null;
    }

    public async Task<List<ClipboardItem>> QueryAsync(int lastId = 0, int limit = 50)
    {
        var result = await SendAsync("query", new { lastId, limit });
        return ParseItemList(result);
    }

    public async Task<List<ClipboardItem>> SearchAsync(string keyword, int limit = 20)
    {
        var result = await SendAsync("search", new { keyword, limit });
        return ParseItemList(result);
    }

    public async Task<bool> DeleteAsync(int id)
    {
        var result = await SendAsync("delete", new { id });
        return result?.RootElement.TryGetProperty("status", out var s) == true &&
               s.GetString() == "ok";
    }

    public async Task<bool> PinAsync(int id, bool pinned)
    {
        var result = await SendAsync("pin", new { id, isPinned = pinned ? 1 : 0 });
        return result?.RootElement.TryGetProperty("status", out var s) == true &&
               s.GetString() == "ok";
    }

    private List<ClipboardItem> ParseItemList(JsonDocument? doc)
    {
        var items = new List<ClipboardItem>();
        if (doc == null) return items;

        var root = doc.RootElement;
        if (!root.TryGetProperty("status", out var status) || status.GetString() != "ok")
            return items;
        if (!root.TryGetProperty("data", out var data)) return items;
        if (!data.TryGetProperty("items", out var arr)) return items;

        foreach (var el in arr.EnumerateArray())
        {
            items.Add(new ClipboardItem
            {
                Id = el.TryGetProperty("id", out var id) ? id.GetInt32() : 0,
                Content = el.TryGetProperty("content", out var c) ? c.GetString() ?? "" : "",
                ContentType = el.TryGetProperty("contentType", out var ct) ? ct.GetString() ?? "text" : "text",
                Timestamp = el.TryGetProperty("timestamp", out var ts) ? ts.GetInt64() : 0,
                IsPinned = el.TryGetProperty("isPinned", out var ip) ? ip.GetInt32() : 0
            });
        }
        return items;
    }

    public void Dispose() { }
}
