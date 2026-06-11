namespace WinClipPro.Models;

public class ClipboardItem
{
    public int Id { get; set; }
    public string Content { get; set; } = "";
    public string ContentType { get; set; } = "text";
    public long Timestamp { get; set; }
    public int IsPinned { get; set; }

    public DateTime DateTime => DateTimeOffset.FromUnixTimeSeconds(Timestamp).LocalDateTime;
    public string Summary => Content.Length > 80 ? Content[..80] + "..." : Content;
    public bool Pinned => IsPinned != 0;
}
