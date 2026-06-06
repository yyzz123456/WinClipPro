package protocol;

import com.google.gson.JsonObject;
import com.google.gson.JsonParser;

public class Request {
    private final String type;
    private final JsonObject data;

    public Request(String type, JsonObject data) {
        this.type = type;
        this.data = data;
    }

    public static Request fromJson(String json) {
        JsonObject obj = JsonParser.parseString(json).getAsJsonObject();
        String type = obj.get("type").getAsString();
        JsonObject data = obj.getAsJsonObject("data");
        return new Request(type, data != null ? data : new JsonObject());
    }

    public String getType() { return type; }
    public JsonObject getData() { return data; }

    public String getString(String key) {
        return data.has(key) ? data.get(key).getAsString() : null;
    }

    public int getInt(String key) {
        return data.has(key) ? data.get(key).getAsInt() : 0;
    }

    public long getLong(String key) {
        return data.has(key) ? data.get(key).getAsLong() : 0;
    }
}
