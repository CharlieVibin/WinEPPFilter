# WinEPPFilter - Windows Enhance Pointer Precision to Raw Input Kernel Driver

**Target:** Windows 10 / 11 x64 (KMDF 1.15)  
**Configuration:** 800 DPI @ 2000 Hz Sub-pixel Fixed-Point Kernel Pipeline

---

## 🌐 Opening & Editing via VS Code for the Web (https://vscode.dev)

You can open and edit this entire project directly in your browser:

1. **Unzip** this project folder to your local drive (e.g. `C:\Projects\WinEPPFilter`).
2. Navigate to **[https://vscode.dev](https://vscode.dev)** in Google Chrome, Microsoft Edge, or any modern browser.
3. Click **Open Folder...** (or press `Ctrl+K Ctrl+O`) and select your unzipped `WinEPPFilter` directory.
4. Grant the browser permission to view and edit files.
5. All C/C++ syntax highlighting, structure navigation, and project files are instantly available in the cloud IDE!

### 💡 Alternatively, via GitHub + VS Code Web:
1. Push this folder to a GitHub repository (e.g., `github.com/your-username/WinEPPFilter`).
2. Go to **`https://vscode.dev/github/your-username/WinEPPFilter`** or press `.` (period) on any GitHub repo page to launch the web editor instantly.

---

## ⚙️ Building & Installing the Driver (Native Windows Environment)

> ⚠️ **Note on Kernel Compilation:** While you can edit, manage, and review all source files inside `vscode.dev`, compiling Windows Kernel-Mode Drivers (`.sys`) requires the **Microsoft Visual Studio 2022 + Windows Driver Kit (WDK)** or **MSBuild / WDK command line tools** on a Windows host machine.

### Prerequisites on Windows Host:
1. Visual Studio 2022 (Community, Professional, or Enterprise) with *"Desktop development with C++"*.
2. Windows 11 SDK & **Windows Driver Kit (WDK 10.0.22621+)*.

### One-Command Build (Developer Command Prompt / MSBuild):
```cmd
msbuild WinEPPFilter.sln /p:Configuration=Release /p:Platform=x64 /p:TargetVersion="Windows10"
```

### Testing & Installation:
```cmd
# Run as Administrator:
.\scripts\install.bat
```

### Running 2000 Hz Diagnostic Benchmark Suite:
```cmd
.\x64\Release\DiagnosticApp\DiagnosticApp.exe
```

---

## 📂 Project Structure

- `WinEPPFilter/`:
  - `Driver.h` & `Driver.c`: KMDF UpperFilter entry points and `MouseClassServiceCallback` interception.
  - `EppEngine.h` & `EppEngine.c`: Fixed-point Q16.16 Windows EPP curve evaluator with sub-pixel remainder state.
  - `Public.h`: Shared IOCTL definitions & configuration structures.
  - `WinEPPFilter.inf`: Plug-and-Play UpperFilter installation manifest.
  - `WinEPPFilter.vcxproj`: WDK project file.
- `DiagnosticApp/`:
  - `DiagnosticApp.cpp`: High-resolution QPC performance and telemetry benchmark harness.
  - `DiagnosticApp.vcxproj`: C++20 console application project file.
- `scripts/`:
  - `install.bat`: Automated test-signing certificate generation, signing, and `pnputil` registration.
  - `uninstall.bat`: Clean uninstaller restoring default mouse filter chain.
- `.vscode/`:
  - `settings.json` & `tasks.json`: VS Code task configurations.
