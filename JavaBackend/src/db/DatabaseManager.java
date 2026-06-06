package db;

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.Statement;

public class DatabaseManager {
    private static final String DB_PATH = System.getenv("LOCALAPPDATA")
            + "\\Clipper\\clipper.db";
    private Connection connection;

    public void init() {
        try {
            Class.forName("org.sqlite.JDBC");
            String dbDir = System.getenv("LOCALAPPDATA") + "\\Clipper";
            java.io.File dir = new java.io.File(dbDir);
            if (!dir.exists()) dir.mkdirs();

            connection = DriverManager.getConnection("jdbc:sqlite:" + DB_PATH);
            createTables();
            System.out.println("[DB] SQLite connected: " + DB_PATH);
        } catch (Exception e) {
            throw new RuntimeException("Database init failed", e);
        }
    }

    private void createTables() throws Exception {
        String sql = """
            CREATE TABLE IF NOT EXISTS clipboard_items (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                content TEXT NOT NULL,
                content_type TEXT NOT NULL,
                timestamp INTEGER NOT NULL,
                is_pinned INTEGER DEFAULT 0
            );
            CREATE INDEX IF NOT EXISTS idx_timestamp ON clipboard_items(timestamp);
            CREATE INDEX IF NOT EXISTS idx_pinned ON clipboard_items(is_pinned);
            """;
        try (Statement stmt = connection.createStatement()) {
            for (String s : sql.split(";")) {
                s = s.trim();
                if (!s.isEmpty()) stmt.execute(s);
            }
        }
    }

    public Connection getConnection() {
        return connection;
    }

    public void close() {
        try {
            if (connection != null && !connection.isClosed()) {
                connection.close();
                System.out.println("[DB] Connection closed.");
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
