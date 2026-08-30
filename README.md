# WinEPPFilter - Windows Enhance Pointer Precision to Raw Input Kernel Driver

**Target:** Windows 10 / 11 x64 (KMDF 1.15)  
**Configuration:** 800 DPI @ 2000 Hz Sub-pixel Fixed-Point Kernel Pipeline
**Zero Local Visual Studio Installation Required** (Cloud GitHub Actions + vscode.dev Workflow)

---

## ⚡ 100% Cloud Build Pipeline (No Visual Studio on your PC)

You can edit code in **https://vscode.dev** and compile your `.sys` binary in the cloud via GitHub Actions:

1. Create a repository on GitHub (e.g. `github.com/your-username/WinEPPFilter`).
2. Push or upload this project folder.
3. Open **`https://vscode.dev/github/your-username/WinEPPFilter`** (or press `.` on GitHub).
4. Edit any C/C++ source code directly in your browser.
5. Click **Commit & Push** in the vscode.dev Source Control tab.
6. The included `.github/workflows/build-driver.yml` triggers automatically on Microsoft's Windows runner (with MSBuild and WDK pre-installed).
7. Download the signed `WinEPPFilter.sys`, `.inf`, `.cer`, and `DiagnosticApp.exe` zip from the GitHub Actions **Artifacts** tab!

---

## 💻 Installing the Compiled Driver on Windows

Once you download your compiled artifact:
1. Extract the compiled artifact zip.
2. Right-click `scripts/install.bat` and select **Run as Administrator**.
3. Launch `DiagnosticApp.exe` to verify real-time 2000 Hz $< 2.0$ μs telemetry!
