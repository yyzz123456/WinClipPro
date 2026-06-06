package protocol;

import com.google.gson.JsonArray;
import com.google.gson.JsonObject;
import model.ClipboardItem;
import java.util.List;

public class Response {

    public static String ok() {
        return "{\"status\":\"ok\"}";
    }

    public static String ok(JsonObject data) {
        JsonObject obj = new JsonObject();
        obj.addProperty("status", "ok");
        obj.add("data", data);
        return obj.toString();
    }

    public static String ok(String key, int value) {
        JsonObject obj = new JsonObject();
        obj.addProperty("status", "ok");
        JsonObject data = new JsonObject();
        data.addProperty(key, value);
        obj.add("data", data);
        return obj.toString();
    }

    public static String okWithItems(List<ClipboardItem> items) {
        JsonObject obj = new JsonObject();
        obj.addProperty("status", "ok");
        JsonObject data = new JsonObject();
        JsonArray arr = new JsonArray();
        for (ClipboardItem item : items) {
            JsonObject i = new JsonObject();
            i.addProperty("id", item.getId());
            i.addProperty("content", item.getContent());
            i.addProperty("contentType", item.getContentType());
            i.addProperty("timestamp", item.getTimestamp());
            i.addProperty("isPinned", item.getIsPinned());
            arr.add(i);
        }
        data.add("items", arr);
        obj.add("data", data);
        return obj.toString();
    }

    public static String error(String message) {
        JsonObject obj = new JsonObject();
        obj.addProperty("status", "error");
        obj.addProperty("message", message);
        return obj.toString();
    }
}
