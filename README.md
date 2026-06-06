以下是为您设计的剪贴板管理器完整技术方案，基于 C++ 构建原生界面与系统钩子，Java 负责 SQLite 数据层，风格对标 PowerToys 的 Fluent Design 美学。

---

## 一、整体架构

```
┌─────────────────────────────────────────────┐
│               C++ 前端进程                    │
│  ┌───────────────┐  ┌──────────────────────┐ │
│  │ WinUI 3 界面  │  │ 系统钩子与输入监听    │ │
│  │ (Fluent UI)  │  │ (Clipboard + Hotkey) │ │
│  └──────┬────────┘  └──────────┬───────────┘ │
│         │                      │             │
│         └──────────┬───────────┘             │
│                    │ IPC (JSON / TCP)        │
└────────────────────┼─────────────────────────┘
                     │
┌────────────────────┼─────────────────────────┐
│               Java 后端服务                   │
│  ┌─────────────────┴───────────────────────┐ │
│  │          SQLite 数据库交互层             │ │
│  │  ┌──────┐  ┌──────┐  ┌──────────────┐  │ │
│  │  │ 存储  │  │ 查询  │  │ 搜索/删除等  │  │ │
│  │  └──────┘  └──────┘  └──────────────┘  │ │
│  └────────────────────────────────────────┘ │
└──────────────────────────────────────────────┘
```

- **C++ 端**：负责所有用户可见功能和系统级交互（UI、剪贴板监控、全局热键、文本/图片获取）。
- **Java 端**：纯数据服务，通过本地 TCP 或命名管道接收指令，操作 SQLite，返回结果。
- **通信**：C++ 发起连接，JSON 格式的请求/响应。

---

## 二、技术选型

| 层次 | 技术 | 说明 |
|------|------|------|
| UI 框架 | WinUI 3 + C++/WinRT | 与 PowerToys 相同的技术栈，原生 Fluent Design，支持亚克力、阴影等效果 |
| 剪贴板监听 | Win32 API `AddClipboardFormatListener` + `WM_CLIPBOARDUPDATE` | 轻量、稳定，无需轮询 |
| 全局热键 | `RegisterHotKey` 模拟 Win+V | 可自定义组合键，避免冲突 |
| 进程间通信 | 本地 TCP Socket（127.0.0.1）或命名管道 | 跨语言通用，调试方便，JSON 协议 |
| Java 运行时 | 内嵌 JRE 或依赖系统 Java | 建议使用 jlink 定制最小 JRE，随应用分发 |
| 数据库 | SQLite via JDBC (`org.xerial:sqlite-jdbc`) | 零配置、单文件，适合本地历史管理 |
| 序列化 | JSON（nlohmann/json 用于 C++，Jackson/Gson 用于 Java） | 可读性强，易于扩展 |
| 构建 | CMake（C++）+ Gradle（Java） | 两部分独立编译，最后通过脚本组装 |

---

## 三、模块详细设计

### 1. C++ 前端模块

#### 1.1 主窗口（WinUI 3）
- **窗口类型**：`DesktopWindow` 或 `OverlappedWindow`，带亚克力背景。
- **布局**：
  - 顶部搜索框（实时过滤历史）。
  - 中间列表（`ListView`），每项显示缩略文本、时间戳、固定状态、内容类型图标。
  - 底部状态栏显示条目数、快捷键提示。
- **右键菜单**：复制、固定、删除、编辑（文本型）。
- **外观特性**：与 PowerToys 一致的间距、字体（Segoe UI Variable）、圆角、主题感知（浅色/深色）。

#### 1.2 剪贴板监控
```cpp
// 注册窗口为剪贴板监听器
AddClipboardFormatListener(m_hWnd);
```
在 `WndProc` 中处理 `WM_CLIPBOARDUPDATE`：
- 检查剪贴板当前包含的格式（文本、位图、文件列表等）。
- 获取内容（文本直接取；图片保存为 PNG 到临时目录，仅存路径；文件列表存完整路径）。
- 去重（与最近一次内容比较，避免重复保存）。
- 调用 Java 服务接口 `saveClipboard`。

#### 1.3 热键处理
- 启动时注册 `Win+V`（或其他组合）：
```cpp
RegisterHotKey(m_hWnd, HOTKEY_ID, MOD_WIN, 'V');
```
- 按下时：显示/隐藏主窗口，并自动刷新历史列表。

#### 1.4 IPC 客户端
- 使用 WinHTTP 或 Boost.Asio（如果引入）建立 TCP 连接到 `localhost:9099`。
- 封装请求方法：`callService(json_request) -> json_response`。
- 支持请求类型：`save`, `query`, `delete`, `pin`, `search`。

### 2. Java 后端服务

#### 2.1 启动与生命周期
- 以标准 Java 应用运行，监听指定端口。
- C++ 进程启动时会检测服务是否存在，若不存在则启动 `java -jar clipper-service.jar`（通过 `CreateProcess`）。
- 服务在空闲时自动退出？建议常驻，用 `socket` 心跳保持。

#### 2.2 数据库管理
- 使用 HikariCP 连接池（可选，单连接也够）。
- 所有 SQL 操作封装在 `ClipboardRepository` 类中。
- 启动时自动建表。

#### 2.3 IPC 服务端
- 基于 `java.net.ServerSocket` 简单实现，每连接一个线程处理请求。
- 协议格式：
```json
// 请求
{
  "type": "save",
  "data": {
    "content": "Hello",
    "contentType": "text",
    "timestamp": 1717595800
  }
}

// 响应
{
  "status": "ok",
  "id": 42
}
```
- 支持请求：
  - `save` – 保存新条目
  - `query` – 分页/全部历史 `{ "lastId": 0, "limit": 50 }`
  - `delete` – 删除条目
  - `pin` – 切换固定状态
  - `search` – 关键字模糊搜索

#### 2.4 数据清理
- 定时删除超过一定天数（如 30 天）且未固定的条目。
- 限制总条目数（如 1000 条），超出后按时间删除最旧的未固定项。

---

## 四、数据库设计（SQLite）

```sql
CREATE TABLE IF NOT EXISTS clipboard_items (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    content TEXT NOT NULL,          -- 文本内容或图片路径/文件路径（JSON数组）
    content_type TEXT NOT NULL,     -- 'text', 'image', 'files'
    timestamp INTEGER NOT NULL,     -- Unix 秒
    is_pinned INTEGER DEFAULT 0,    -- 1 表示固定
    hash TEXT                       -- 内容的 SHA256，用于快速去重（可选）
);

CREATE INDEX idx_timestamp ON clipboard_items(timestamp);
CREATE INDEX idx_pinned ON clipboard_items(is_pinned);
```

---

## 五、运行流程

1. 用户登录后，C++ 应用启动（可设置为开机自启）。
2. C++ 启动 Java 服务子进程，并等待其就绪（轮询端口）。
3. 连接建立后，C++ 请求最新历史，填充 UI 缓存。
4. 每当系统剪贴板变化，C++ 获取内容，去重后调用 Java `save`。
5. 用户按下 `Win+V` 激活界面，界面从 Java 拉取最新列表并展示。
6. 用户操作（复制旧项）：C++ 将对应内容写入剪贴板（`SetClipboardData`），并可选通知 Java 更新该项的使用统计（可扩展）。

---

## 六、构建与部署

### C++ 部分
- 使用 Visual Studio 2022 解决方案，CMake 项目。
- 目标平台：x64，Windows 11 SDK。
- 依赖：`Microsoft.WindowsAppSDK`（含 WinUI3），nlohmann/json。
- 打包为 MSIX 或独立 exe（通过 Windows Application Packaging Project）。

### Java 部分
- Gradle 项目，依赖：`org.xerial:sqlite-jdbc:3.42.0.0`，`com.google.code.gson:gson:2.10.1`。
- 使用 `jlink` 创建包含精简 JRE 的运行时镜像，与 C++ 二进制放在同一目录。
- C++ 启动时指定 JRE 路径，确保未安装 Java 的用户也能运行。

### 安装程序
- 使用 WiX Toolset 或 Inno Setup 制作安装包，同时部署 C++ 前端、Java 运行时、数据库文件位置（如 `%LOCALAPPDATA%\Clipper\clipper.db`）。

---

## 七、安全与性能

- 通信仅监听 `127.0.0.1`，防止远程访问。
- 敏感内容（如密码管理器复制的内容）存储时不做特殊处理，但可考虑简单加密（如 AES），并提供“不记录来自特定应用”的选项（通过检测前台窗口进程名）。
- 图片存储使用路径而非 BLOB，避免数据库膨胀；图片文件放在 `%LOCALAPPDATA%\Clipper\images\`，由 Java 服务管理清理。
- 内存缓存：C++ 端维护最近 100 条记录的缓存，减少 IPC 调用。

---

## 八、扩展性

- 若未来需跨设备同步，可在 Java 端增加网络同步模块，C++ 无需改动。
- UI 风格可进一步模仿 PowerToys 的设置界面，通过 WinUI 的 `NavigationView` 实现多页（历史、设置、关于）。
- 可增加“剪贴板分组”“标签”等高级功能，数据库结构易于扩展。

---

此方案严格遵循您的要求：C++ 用于 UI 与输入收集，Java 仅限数据库交互，两者通过标准化 IPC 解耦，最终交付一个媲美 PowerToys 风格的高效原生剪贴板管理器。