@echo off
REM =========================================================================
REM WinEPPFilter - Windows 10/11 x64 Kernel Driver Automated Installer
REM Must be run as Administrator!
REM =========================================================================

net session >nul 2>&1
if %errorLevel% neq 0 (
    echo [ERROR] This script must be run as Administrator.
    pause
    exit /b 1
)

echo [1/4] Enabling Windows Test-Signing Mode...
bcdedit /set testsigning on
if %errorLevel% neq 0 (
    echo [!] Note: If Secure Boot is enabled in BIOS, please disable Secure Boot first.
)

echo [2/4] Creating and installing self-signed driver test certificate...
makecert -r -pe -ss "WinEPPTestStore" -n "CN=WinEPP Test Certificate" WinEPP.cer
certmgr /add WinEPP.cer /s /r localMachine root
certmgr /add WinEPP.cer /s /r localMachine trustedpublisher

echo [3/4] Signing WinEPPFilter.sys kernel driver package...
signtool sign /v /s "WinEPPTestStore" /n "WinEPP Test Certificate" /t http://timestamp.digicert.com x64\Release\WinEPPFilter\WinEPPFilter.sys

echo [4/4] Installing UpperFilter driver package via pnputil...
pnputil /add-driver x64\Release\WinEPPFilter\WinEPPFilter.inf /install

echo =========================================================================
echo [SUCCESS] WinEPPFilter driver installed successfully!
echo If prompted, reboot your PC to complete filter chain attachment.
echo =========================================================================
pause
