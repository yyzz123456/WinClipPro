# WinClip Pro

A lightweight clipboard manager for Windows with Fluent Design aesthetics. Press **Alt+,** to access your clipboard history.

**English** | [中文简体](README_zh_CN.md)

![](https://img.shields.io/badge/platform-Windows%2010%2B-blue)
![](https://img.shields.io/badge/.NET-10.0-purple)
![](https://img.shields.io/badge/Java-17-orange)
![](https://img.shields.io/badge/C%2B%2B-17-blue)

> [!WARNING]
> This tool is only a course assignment completed with the help of AI. It cannot guarantee stable operation under all circumstances. Please be sure to take note before use.

## Architecture

```
C# WPF Frontend          C++ Hook DLL           Java Backend
┌─────────────────┐     ┌──────────────┐     ┌─────────────────┐
│ Fluent Design UI │     │ Clipboard    │     │ TCP Server      │
│ Acrylic backdrop │◄───►│ Monitor      │     │ (localhost:9099)│
│ System tray icon │     │ Hotkey       │     │ SQLite storage  │
│ TcpClient        │     │ Manager      │     │ JSON protocol   │
└────────┬─────────┘     └──────────────┘     └────────┬────────┘
         │                                              │
         └──────────────── TCP/JSON ────────────────────┘
```

- **WPF (.NET 10)** — UI layer with ModernWpfUI for Fluent Design
- **C++ DLL** — System-level clipboard hooks and global hotkey registration
- **Java 17** — Data backend: TCP server + SQLite via JDBC

## Features

- **Global hotkey** `Alt+,` to show/hide the clipboard window
- **Acrylic backdrop** with rounded corners matching Windows 11 aesthetics
- **System tray icon** — runs silently in the background
- **Auto-paste** — click any item to copy and paste into your current app
- **Pin window** — keep the clipboard window always visible
- **Search** — filter history in real time
- **Pin items** — mark important clips to keep them
- **Auto-prune** — configurable max item limit
- **Dark text** on light acrylic — readable in both light and dark modes
- **Single instance** — launching again just shows the existing window

## Prerequisites

| Component | Required |
|-----------|----------|
| .NET 10 SDK | WPF build |
| Visual Studio 2026 | C++ DLL build |
| Java 17+ (JDK) | Backend runtime |
| IntelliJ IDEA | Java compilation |

## Build & Run

### 1. Java Backend

Open `JavaBackend/` in IntelliJ IDEA, build the project (native IntelliJ build system, not Gradle). Output goes to `out/production/JavaBackend/`.

Jars required in `JavaBackend/lib/`:
- `sqlite-jdbc-3.42.0.0.jar`
- `gson-2.10.1.jar`

### 2. C++ Hook DLL

Open `WinClipHook/WinClipHook.vcxproj` in Visual Studio, build for **x64 Debug** (or Release). Output: `x64/Debug/WinClipHook.dll`.

### 3. WPF Frontend

```bash
cd WinClipPro
dotnet build          # development
dotnet publish -c Debug -o publish   # single-file EXE
```

The published EXE auto-discovers the Java backend directory and starts it on launch.

## Settings

Click the gear icon in the title bar to configure:

- **Hotkey** — view current binding
- **Max items** — limit clipboard history (default: 500)
- **Keep history for** — auto-cleanup after N days
- **Start with Windows** — registry auto-start

Settings are persisted to `%LOCALAPPDATA%\WinClipPro\settings.json`.

## Project Structure

```
WinClipPro/           WPF frontend (.NET 10)
  ├── Models/         Data models
  ├── Services/       TCP client, clipboard P/Invoke, settings, Java launcher
  └── Resources/      SVG tray icon
WinClipHook/          C++ hook DLL (VS 2026)
  ├── ClipboardMonitor.*   WM_CLIPBOARDUPDATE listener
  ├── HotkeyManager.*      Global hotkey registration
  └── Exports.*            P/Invoke exports
JavaBackend/          Java data service (IntelliJ)
  └── src/
      ├── server/     TCP server (port 9099)
      ├── protocol/   JSON request/response
      ├── db/         SQLite repository
      └── model/      ClipboardItem
```

## TCP Protocol

All communication over `127.0.0.1:9099`, line-delimited JSON.

| Command | Data | Description |
|---------|------|-------------|
| `save` | `content`, `contentType`, `timestamp` | Store a new clip |
| `query` | `lastId`, `limit` | Fetch history (paginated) |
| `delete` | `id` | Remove a clip |
| `pin` | `id`, `isPinned` | Toggle pin status |
| `search` | `keyword`, `limit` | Full-text search |

## License

Course project. Free to use and modify.
