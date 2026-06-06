package db;

import model.ClipboardItem;
import java.sql.*;
import java.util.ArrayList;
import java.util.List;

public class ClipboardRepository {
    private final DatabaseManager dbManager;

    public ClipboardRepository(DatabaseManager dbManager) {
        this.dbManager = dbManager;
    }

    public int save(ClipboardItem item) {
        String sql = "INSERT INTO clipboard_items (content, content_type, timestamp, is_pinned) VALUES (?, ?, ?, ?)";
        try (PreparedStatement ps = dbManager.getConnection().prepareStatement(sql,
                Statement.RETURN_GENERATED_KEYS)) {
            ps.setString(1, item.getContent());
            ps.setString(2, item.getContentType());
            ps.setLong(3, item.getTimestamp());
            ps.setInt(4, item.getIsPinned());
            ps.executeUpdate();
            try (ResultSet rs = ps.getGeneratedKeys()) {
                if (rs.next()) return rs.getInt(1);
            }
        } catch (SQLException e) {
            System.err.println("[Repo] save error: " + e.getMessage());
        }
        return -1;
    }

    public List<ClipboardItem> query(int lastId, int limit) {
        List<ClipboardItem> list = new ArrayList<>();
        String sql;
        if (lastId <= 0) {
            sql = "SELECT * FROM clipboard_items ORDER BY id DESC LIMIT ?";
        } else {
            sql = "SELECT * FROM clipboard_items WHERE id < ? ORDER BY id DESC LIMIT ?";
        }
        try (PreparedStatement ps = dbManager.getConnection().prepareStatement(sql)) {
            if (lastId <= 0) {
                ps.setInt(1, limit);
            } else {
                ps.setInt(1, lastId);
                ps.setInt(2, limit);
            }
            try (ResultSet rs = ps.executeQuery()) {
                while (rs.next()) {
                    list.add(mapRow(rs));
                }
            }
        } catch (SQLException e) {
            System.err.println("[Repo] query error: " + e.getMessage());
        }
        return list;
    }

    public boolean delete(int id) {
        String sql = "DELETE FROM clipboard_items WHERE id = ?";
        try (PreparedStatement ps = dbManager.getConnection().prepareStatement(sql)) {
            ps.setInt(1, id);
            return ps.executeUpdate() > 0;
        } catch (SQLException e) {
            System.err.println("[Repo] delete error: " + e.getMessage());
        }
        return false;
    }

    public boolean pin(int id, int isPinned) {
        String sql = "UPDATE clipboard_items SET is_pinned = ? WHERE id = ?";
        try (PreparedStatement ps = dbManager.getConnection().prepareStatement(sql)) {
            ps.setInt(1, isPinned);
            ps.setInt(2, id);
            return ps.executeUpdate() > 0;
        } catch (SQLException e) {
            System.err.println("[Repo] pin error: " + e.getMessage());
        }
        return false;
    }

    public List<ClipboardItem> search(String keyword, int limit) {
        List<ClipboardItem> list = new ArrayList<>();
        String sql = "SELECT * FROM clipboard_items WHERE content LIKE ? ORDER BY id DESC LIMIT ?";
        try (PreparedStatement ps = dbManager.getConnection().prepareStatement(sql)) {
            ps.setString(1, "%" + keyword + "%");
            ps.setInt(2, limit);
            try (ResultSet rs = ps.executeQuery()) {
                while (rs.next()) {
                    list.add(mapRow(rs));
                }
            }
        } catch (SQLException e) {
            System.err.println("[Repo] search error: " + e.getMessage());
        }
        return list;
    }

    public void cleanup(int maxDays, int maxCount) {
        long cutoff = System.currentTimeMillis() / 1000 - maxDays * 86400L;
        try {
            // Delete old unpinned items
            String sql1 = "DELETE FROM clipboard_items WHERE timestamp < ? AND is_pinned = 0";
            try (PreparedStatement ps = dbManager.getConnection().prepareStatement(sql1)) {
                ps.setLong(1, cutoff);
                int deleted = ps.executeUpdate();
                if (deleted > 0) System.out.println("[Repo] Cleaned " + deleted + " expired items.");
            }

            // Enforce max count
            String sql2 = "SELECT COUNT(*) FROM clipboard_items";
            try (Statement stmt = dbManager.getConnection().createStatement();
                 ResultSet rs = stmt.executeQuery(sql2)) {
                if (rs.next()) {
                    int count = rs.getInt(1);
                    if (count > maxCount) {
                        String sql3 = "DELETE FROM clipboard_items WHERE id IN "
                                + "(SELECT id FROM clipboard_items WHERE is_pinned = 0 "
                                + "ORDER BY timestamp ASC LIMIT ?)";
                        try (PreparedStatement ps = dbManager.getConnection().prepareStatement(sql3)) {
                            ps.setInt(1, count - maxCount);
                            int removed = ps.executeUpdate();
                            if (removed > 0) System.out.println("[Repo] Removed " + removed + " over-limit items.");
                        }
                    }
                }
            }
        } catch (SQLException e) {
            System.err.println("[Repo] cleanup error: " + e.getMessage());
        }
    }

    private ClipboardItem mapRow(ResultSet rs) throws SQLException {
        ClipboardItem item = new ClipboardItem();
        item.setId(rs.getInt("id"));
        item.setContent(rs.getString("content"));
        item.setContentType(rs.getString("content_type"));
        item.setTimestamp(rs.getLong("timestamp"));
        item.setIsPinned(rs.getInt("is_pinned"));
        return item;
    }
}
