# -*- coding: utf-8 -*-
import ctypes
from ctypes import wintypes as w
import sys

k32 = ctypes.WinDLL('kernel32', use_last_error=True)

class STARTUPINFO(ctypes.Structure):
    _fields_ = [("cb", w.DWORD), ("lpReserved", w.LPWSTR), ("lpDesktop", w.LPWSTR),
                ("lpTitle", w.LPWSTR), ("dwX", w.DWORD), ("dwY", w.DWORD),
                ("dwXSize", w.DWORD), ("dwYSize", w.DWORD), ("dwXCountChars", w.DWORD),
                ("dwYCountChars", w.DWORD), ("dwFillAttribute", w.DWORD),
                ("dwFlags", w.DWORD), ("wShowWindow", w.WORD), ("cbReserved2", w.WORD),
                ("lpReserved2", ctypes.POINTER(ctypes.c_byte)),
                ("hStdInput", w.HANDLE), ("hStdOutput", w.HANDLE), ("hStdError", w.HANDLE)]

class PROCESS_INFORMATION(ctypes.Structure):
    _fields_ = [("hProcess", w.HANDLE), ("hThread", w.HANDLE),
                ("dwProcessId", w.DWORD), ("dwThreadId", w.DWORD)]

si = STARTUPINFO()
si.cb = ctypes.sizeof(STARTUPINFO)
pi = PROCESS_INFORMATION()
cmd = w.LPWSTR(sys.argv[1])
ok = k32.CreateProcessW(None, cmd, None, None, False, 0, None, None, ctypes.byref(si), ctypes.byref(pi))
if not ok:
    print('CreateProcess failed, error', ctypes.get_last_error())
else:
    k32.WaitForSingleObject(pi.hProcess, 5000)
    code = w.DWORD()
    k32.GetExitCodeProcess(pi.hProcess, ctypes.byref(code))
    print('exit code', code.value)
    k32.CloseHandle(pi.hProcess)
    k32.CloseHandle(pi.hThread)
