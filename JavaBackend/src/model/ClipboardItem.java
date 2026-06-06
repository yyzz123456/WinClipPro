package model;

public class ClipboardItem {
    private int id;
    private String content;
    private String contentType;
    private long timestamp;
    private int isPinned;

    public ClipboardItem() {}

    public ClipboardItem(String content, String contentType, long timestamp) {
        this.content = content;
        this.contentType = contentType;
        this.timestamp = timestamp;
        this.isPinned = 0;
    }

    public int getId() { return id; }
    public void setId(int id) { this.id = id; }

    public String getContent() { return content; }
    public void setContent(String content) { this.content = content; }

    public String getContentType() { return contentType; }
    public void setContentType(String contentType) { this.contentType = contentType; }

    public long getTimestamp() { return timestamp; }
    public void setTimestamp(long timestamp) { this.timestamp = timestamp; }

    public int getIsPinned() { return isPinned; }
    public void setIsPinned(int isPinned) { this.isPinned = isPinned; }

    public boolean isPinned() { return isPinned == 1; }
}
