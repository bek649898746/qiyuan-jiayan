@echo off
rem ============================================================
rem Jiayan test quiet launcher - prevent network drops from CPU saturation
rem (Brother's issue 2026-09-09: full test run maxes CPU -> remote session drops)
rem Method: Idle priority + parallel 2 -> normal-priority network/remote always wins
rem Usage:
rem   run_tests_quiet.cmd --zh-feat-all --parallel 2
rem   run_tests_quiet.cmd --zh-feat --parallel 2
rem   run_tests_quiet.cmd          (default: en full)
rem ============================================================
cd /d %~dp0\..
rem Idle priority (child qcc/python processes inherit Idle automatically)
start /low /wait /b python -u scripts\run_tests_zh.py %*
echo.
echo [done] exit code %ERRORLEVEL%
exit /b %ERRORLEVEL%
