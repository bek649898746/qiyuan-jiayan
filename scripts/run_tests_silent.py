#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
甲言测试静默启动器 v2 — Idle 优先级 + 限核 (大哥断网问题根治 2026-09-09)
原理:
  1. Idle 优先级: CPU 竞争时让路给网络/远程会话 (Normal)
  2. 限核 (前 N-4 核): 物理留出 4 核给系统/网卡中断/远程 — 双保险
子进程 (python worker + qcc 编译) 自动继承优先级 + 亲和性 (CreateProcess 继承)
用法: python scripts/run_tests_silent.py --zh-feat-all --parallel 2
      python scripts/run_tests_silent.py --zh-feat
      python scripts/run_tests_silent.py            (en 全量)
"""
import ctypes, subprocess, sys, os

# 0. 正确声明 Win32 API (64 位下 HANDLE 必须 c_void_p, 否则截断静默失败)
k32 = ctypes.windll.kernel32
k32.GetCurrentProcess.restype = ctypes.c_void_p
k32.SetPriorityClass.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
k32.SetPriorityClass.restype = ctypes.c_bool
k32.SetProcessAffinityMask.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
k32.SetProcessAffinityMask.restype = ctypes.c_bool

hproc = k32.GetCurrentProcess()

# 1. Idle 优先级 (0x40 = IDLE_PRIORITY_CLASS)
ok1 = k32.SetPriorityClass(hproc, 0x40)
if not ok1:
    sys.stderr.write(f"[silent] WARN SetPriorityClass failed err={ctypes.get_last_error()}\n")

# 2. 限核: 逻辑核数 - 4, 留 4 核给系统/网络 (双保险)
def logical_cores():
    try:
        return os.cpu_count() or 16
    except Exception:
        return 16

n = logical_cores()
keep = max(4, n - 4)          # 留 4 核
mask = (1 << keep) - 1        # 用前 keep 核
ok2 = k32.SetProcessAffinityMask(hproc, mask)
if not ok2:
    sys.stderr.write(f"[silent] WARN SetProcessAffinityMask failed err={ctypes.get_last_error()}\n")
sys.stderr.write(f"[silent] Idle={ok1} affinity({keep}/{n}核)={ok2}\n")
sys.stderr.flush()

# 3. 跑测试 (子进程继承 Idle + affinity)
repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(repo)
args = [sys.executable, '-u', os.path.join('scripts', 'run_tests_zh.py')] + sys.argv[1:]
rc = subprocess.call(args)
sys.exit(rc)
