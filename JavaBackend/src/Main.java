import db.ClipboardRepository;
import db.DatabaseManager;
import server.ClipboardServer;

public class Main {
    public static void main(String[] args) {
        System.out.println("=== Clipboard Service ===");

        DatabaseManager dbManager = new DatabaseManager();
        dbManager.init();

        ClipboardRepository repository = new ClipboardRepository(dbManager);
        ClipboardServer server = new ClipboardServer(repository);

        Thread cleanupThread = new Thread(() -> {
            while (true) {
                try {
                    Thread.sleep(10 * 60 * 1000); // every 10 minutes
                    repository.cleanup(30, 1000);
                } catch (InterruptedException e) {
                    break;
                }
            }
        }, "cleanup-thread");
        cleanupThread.setDaemon(true);
        cleanupThread.start();

        Runtime.getRuntime().addShutdownHook(new Thread(() -> {
            System.out.println("[Main] Shutting down...");
            server.stop();
            dbManager.close();
        }));

        server.start();
    }
}
