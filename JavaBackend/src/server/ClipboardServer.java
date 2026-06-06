package server;

import db.ClipboardRepository;
import db.DatabaseManager;
import model.ClipboardItem;
import protocol.Request;
import protocol.Response;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.util.List;

public class ClipboardServer {
    private static final int PORT = 9099;
    private final ClipboardRepository repository;
    private volatile boolean running = true;

    public ClipboardServer(ClipboardRepository repository) {
        this.repository = repository;
    }

    public void start() {
        try (ServerSocket serverSocket = new ServerSocket(PORT, 50,
                java.net.InetAddress.getByName("127.0.0.1"))) {
            System.out.println("[Server] Listening on 127.0.0.1:" + PORT);

            while (running) {
                Socket client = serverSocket.accept();
                new Thread(() -> handleClient(client)).start();
            }
        } catch (Exception e) {
            System.err.println("[Server] Error: " + e.getMessage());
        }
        System.out.println("[Server] Stopped.");
    }

    private void handleClient(Socket client) {
        try (BufferedReader reader = new BufferedReader(
                new InputStreamReader(client.getInputStream(), StandardCharsets.UTF_8));
             OutputStreamWriter writer = new OutputStreamWriter(client.getOutputStream(), StandardCharsets.UTF_8)) {

            String line = reader.readLine();
            if (line == null || line.isEmpty()) {
                client.close();
                return;
            }

            Request req = Request.fromJson(line);
            String result = dispatch(req);
            writer.write(result);
            writer.write('\n');
            writer.flush();
        } catch (Exception e) {
            System.err.println("[Server] Client error: " + e.getMessage());
        } finally {
            try { client.close(); } catch (Exception ignored) {}
        }
    }

    private String dispatch(Request req) {
        try {
            return switch (req.getType()) {
                case "save" -> {
                    ClipboardItem item = new ClipboardItem(
                            req.getString("content"),
                            req.getString("contentType"),
                            req.getLong("timestamp"));
                    int id = repository.save(item);
                    yield id > 0
                            ? Response.ok("id", id)
                            : Response.error("save failed");
                }
                case "query" -> {
                    List<ClipboardItem> items = repository.query(
                            req.getInt("lastId"), req.getInt("limit"));
                    yield Response.okWithItems(items);
                }
                case "delete" -> {
                    boolean ok = repository.delete(req.getInt("id"));
                    yield ok ? Response.ok() : Response.error("delete failed");
                }
                case "pin" -> {
                    boolean ok = repository.pin(req.getInt("id"), req.getInt("isPinned"));
                    yield ok ? Response.ok() : Response.error("pin failed");
                }
                case "search" -> {
                    List<ClipboardItem> items = repository.search(
                            req.getString("keyword"), req.getInt("limit"));
                    yield Response.okWithItems(items);
                }
                default -> Response.error("unknown type: " + req.getType());
            };
        } catch (Exception e) {
            return Response.error(e.getMessage());
        }
    }

    public void stop() {
        running = false;
        try {
            new Socket("127.0.0.1", PORT).close();
        } catch (Exception ignored) {}
    }
}
