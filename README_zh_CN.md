# WinClip Pro

适用于 Windows 的轻量级剪贴板管理器，采用 Fluent Design 风格。按 **Alt+,** 呼出剪贴板历史。

![](https://img.shields.io/badge/平台-Windows%2010%2B-blue)
![](https://img.shields.io/badge/.NET-10.0-purple)
![](https://img.shields.io/badge/Java-17-orange)
![](https://img.shields.io/badge/C%2B%2B-17-blue)

[English](README.md) | **中文简体**

## 架构

```
C# WPF 前端             C++ 钩子 DLL           Java 后端
┌─────────────────┐     ┌──────────────┐     ┌─────────────────┐
│ Fluent Design   │     │ 剪贴板监听   │     │ TCP 服务器      │
│ 亚克力背景      │◄───►│              │     │ (localhost:9099)│
│ 系统托盘图标    │     │ 热键管理     │     │ SQLite 存储     │
│ TcpClient       │     │              │     │ JSON 协议       │
└────────┬─────────┘     └──────────────┘     └────────┬────────┘
         │                                              │
         └──────────────── TCP/JSON ────────────────────┘
```

- **WPF (.NET 10)** — 界面层，基于 ModernWpfUI 实现 Fluent Design
- **C++ DLL** — 系统级剪贴板钩子和全局热键注册
- **Java 17** — 数据后端：TCP 服务器 + SQLite (JDBC)

## 功能

- **全局热键** `Alt+,` 显示/隐藏剪贴板窗口
- **亚克力背景** 配合 Windows 11 圆角风格
- **系统托盘** 静默运行于后台
- **自动粘贴** 点击条目即可复制并粘贴到当前应用
- **窗口固定** 让剪贴板窗口始终可见
- **实时搜索** 快速过滤历史记录
- **条目固定** 标记重要内容防止被清理
- **自动裁剪** 可配置最大条目数量
- **深色文字** 浅色亚克力背景，亮暗模式均可读
- **单实例** 重复启动只会唤出已有窗口

## 环境要求

| 组件 | 用途 |
|------|------|
| .NET 10 SDK | WPF 编译 |
| Visual Studio 2026 | C++ DLL 编译 |
| Java 17+ (JDK) | 后端运行 |
| IntelliJ IDEA | Java 编译 |

## 构建与运行

### 1. Java 后端

用 IntelliJ IDEA 打开 `JavaBackend/` 目录，构建项目（使用原生 IntelliJ 构建系统，非 Gradle）。编译输出到 `out/production/JavaBackend/`。

`JavaBackend/lib/` 中需要的 jar 包：
- `sqlite-jdbc-3.42.0.0.jar`
- `gson-2.10.1.jar`

### 2. C++ 钩子 DLL

在 Visual Studio 中打开 `WinClipHook/WinClipHook.vcxproj`，编译为 **x64 Debug**（或 Release）。输出：`x64/Debug/WinClipHook.dll`。

### 3. WPF 前端

```bash
cd WinClipPro
dotnet build          # 开发模式
dotnet publish -c Debug -o publish   # 单文件 EXE
```

发布的 EXE 会自动向上查找 Java 后端目录并在启动时启动 Java 服务。

## 设置

点击标题栏齿轮图标打开设置：

- **快捷键** — 查看当前绑定
- **最大条目数** — 限制剪贴板历史数量（默认 500）
- **保留天数** — 超过 N 天后自动清理
- **开机自启** — 注册表自动启动

设置保存到 `%LOCALAPPDATA%\WinClipPro\settings.json`。

## 项目结构

```
WinClipPro/           WPF 前端 (.NET 10)
  ├── Models/         数据模型
  ├── Services/       TCP 客户端、剪贴板 P/Invoke、设置、Java 启动器
  └── Resources/      SVG 托盘图标
WinClipHook/          C++ 钩子 DLL (VS 2026)
  ├── ClipboardMonitor.*   剪贴板监听 (WM_CLIPBOARDUPDATE)
  ├── HotkeyManager.*      全局热键注册
  └── Exports.*            P/Invoke 导出函数
JavaBackend/          Java 数据服务 (IntelliJ)
  └── src/
      ├── server/     TCP 服务器 (端口 9099)
      ├── protocol/   JSON 请求/响应
      ├── db/         SQLite 数据访问
      └── model/      剪贴板条目模型
```

## TCP 协议

所有通信通过 `127.0.0.1:9099`，换行符分隔的 JSON。

| 命令 | 数据 | 说明 |
|------|------|------|
| `save` | `content`, `contentType`, `timestamp` | 保存新条目 |
| `query` | `lastId`, `limit` | 分页查询历史 |
| `delete` | `id` | 删除条目 |
| `pin` | `id`, `isPinned` | 切换固定状态 |
| `search` | `keyword`, `limit` | 全文搜索 |

## 许可证

课程项目，自由使用和修改。
