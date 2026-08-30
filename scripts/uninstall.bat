@echo off
REM =========================================================================
REM WinEPPFilter - Clean Uninstaller
REM =========================================================================

net session >nul 2>&1
if %errorLevel% neq 0 (
    echo [ERROR] Must be run as Administrator.
    pause
    exit /b 1
)

echo [1/2] Stopping WinEPPFilter service...
net stop WinEPPFilter >nul 2>&1

echo [2/2] Removing driver package with pnputil...
pnputil /delete-driver oem*.inf /uninstall /force

echo Restoring default Mouse UpperFilters in registry...
reg delete "HKLM\SYSTEM\CurrentControlSet\Control\Class\{4d36e96f-e325-11ce-bfc1-08002be10318}" /v "UpperFilters" /f >nul 2>&1

echo [SUCCESS] WinEPPFilter uninstalled completely.
pause
