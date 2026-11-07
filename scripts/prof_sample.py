# -*- coding: utf-8 -*-
"""qcc 采样剖面仪（Windows, 无外部依赖）。

原理：编译器自带的 `QCC_DBG_FMAP=1` 会打印每个函数的 [起, 止) 码偏移；
本脚本启动目标 exe，按固定间隔 **挂起线程 → 读 Rip → 恢复**，
把 Rip 归一到「函数」，输出扁平直方图（含未知地址桶）。

为什么需要它：`GetSystemTimeAsFileTime` 探针在每调用点插桩，
在 6M 级调用上会自我放大（实测：插桩说 11s，消融只省 1s）。
采样不改被测量程序的任何代码，是这里唯一可信的量具。

用法:
  python scripts/prof_sample.py --cc qcc_zh_v2.exe --src srclib_jiayan/qcc_zh.jy
       [--out build/prof.exe] [--interval 0.004] [--warm 3.0]
"""
import argparse
import ctypes
import ctypes.wintypes as wt
import os
import re
import subprocess
import sys
import time

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

k32 = ctypes.WinDLL('kernel32', use_last_error=True)

TH32CS_SNAPTHREAD = 0x00000004
TH32CS_SNAPMODULE = 0x00000008
TH32CS_SNAPMODULE32 = 0x00000010
THREAD_SUSPEND_RESUME = 0x0002
THREAD_GET_CONTEXT = 0x0008
THREAD_QUERY_INFORMATION = 0x0040
CONTEXT_CONTROL = 0x00100001
CONTEXT_SIZE = 0x4D0
RIP_OFF = 248


class THREADENTRY32(ctypes.Structure):
    _fields_ = [('dwSize', wt.DWORD), ('cntUsage', wt.DWORD),
                ('th32ThreadID', wt.DWORD), ('th32OwnerProcessID', wt.DWORD),
                ('tpBasePri', ctypes.c_long), ('tpDeltaPri', ctypes.c_long),
                ('dwFlags', wt.DWORD)]


class MODULEENTRY32(ctypes.Structure):
    _fields_ = [('dwSize', wt.DWORD), ('th32ModuleID', wt.DWORD),
                ('th32ProcessID', wt.DWORD), ('GlblcntUsage', wt.DWORD),
                ('ProccntUsage', wt.DWORD), ('modBaseAddr', ctypes.POINTER(ctypes.c_byte)),
                ('modBaseSize', wt.DWORD), ('hModule', wt.HMODULE),
                ('szModule', ctypes.c_char * 256), ('szExePath', ctypes.c_char * 260)]


def module_base(pid):
    snap = k32.CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid)
    if snap == -1:
        return None
    me = MODULEENTRY32()
    me.dwSize = ctypes.sizeof(MODULEENTRY32)
    base = None
    if k32.Module32First(snap, ctypes.byref(me)):
        base = ctypes.cast(me.modBaseAddr, ctypes.c_void_p).value
    k32.CloseHandle(snap)
    return base


def thread_ids(pid):
    snap = k32.CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0)
    if snap == -1:
        return []
    te = THREADENTRY32()
    te.dwSize = ctypes.sizeof(THREADENTRY32)
    out = []
    ok = k32.Thread32First(snap, ctypes.byref(te))
    while ok:
        if te.th32OwnerProcessID == pid:
            out.append(te.th32ThreadID)
        ok = k32.Thread32Next(snap, ctypes.byref(te))
    k32.CloseHandle(snap)
    return out


def read_rip(tid):
    h = k32.OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, False, tid)
    if not h:
        return None
    try:
        if k32.SuspendThread(h) == 0xFFFFFFFF:
            return None
        try:
            buf = (ctypes.c_char * (CONTEXT_SIZE + 16))()
            addr = ctypes.addressof(buf)
            aligned = (addr + 15) & ~15
            ctypes.memset(aligned, 0, CONTEXT_SIZE)
            ctypes.c_uint32.from_address(aligned + 48).value = CONTEXT_CONTROL
            if k32.GetThreadContext(h, ctypes.c_void_p(aligned)):
                return ctypes.c_uint64.from_address(aligned + RIP_OFF).value
            return None
        finally:
            k32.ResumeThread(h)
    finally:
        k32.CloseHandle(h)


def parse_fmap(path):
    rows = []
    pat = re.compile(r'\[FMAP\] (.+?) (\d+) (\d+)\s*$')
    for l in open(path, 'rb').read().decode('utf-8', 'replace').splitlines():
        m = pat.search(l)
        if m:
            rows.append((int(m.group(2)), int(m.group(3)), m.group(1)))
    rows.sort()
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v2.exe')
    ap.add_argument('--src', default='srclib_jiayan/qcc_zh.jy')
    ap.add_argument('--out', default=os.path.join('build', 'prof_sample_out.exe'))
    ap.add_argument('--interval', type=float, default=0.004)
    ap.add_argument('--warm', type=float, default=3.0)
    ap.add_argument('--fmap', default=os.path.join('build', 'fmap.txt'))
    a = ap.parse_args()

    # 1) 取 FMAP（另起一次编译；与采样不同进程）
    env = dict(os.environ); env['QCC_DBG_FMAP'] = '1'
    with open(a.fmap, 'wb') as f:
        subprocess.run([a.cc, a.src, '-o', a.out, '--profile'], env=env,
                       stdout=f, stderr=subprocess.STDOUT, timeout=1800)
    rows = parse_fmap(a.fmap)
    print('FMAP 函数段 = %d' % len(rows))

    # 2) 采样
    env2 = dict(os.environ); env2.pop('QCC_DBG_FMAP', None)
    p = subprocess.Popen([a.cc, a.src, '-o', a.out], env=env2,
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(a.warm)
    base = module_base(p.pid)
    if base is None:
        p.kill(); print('取 module base 失败'); return 2
    print('ImageBase = 0x%x  pid=%d' % (base, p.pid))

    hist = {}
    total = 0
    t_end = time.time() + 600
    while p.poll() is None and time.time() < t_end and total < 200000:
        for tid in thread_ids(p.pid):
            rip = read_rip(tid)
            if rip is None:
                continue
            hist[rip] = hist.get(rip, 0) + 1
            total += 1
        time.sleep(a.interval)
    rc = p.poll()
    print('采样点 = %d  rc=%s' % (total, rc))

    # 3) 归一
    import bisect
    starts = [r[0] for r in rows]
    fn_hits = {}
    unknown = 0
    for rip, n in hist.items():
        off = rip - base - 0x1000
        i = bisect.bisect_right(starts, off) - 1
        if 0 <= i < len(rows) and rows[i][1] > off:
            fn_hits[rows[i][2]] = fn_hits.get(rows[i][2], 0) + n
        else:
            unknown += n
    print('\n== 按函数（采样点数，降序）==')
    for name, n in sorted(fn_hits.items(), key=lambda x: -x[1])[:40]:
        print('%8d  %5.1f%%  %s' % (n, 100.0 * n / max(total, 1), name))
    print('%8d  %5.1f%%  <未知 区间/运行库>' % (unknown, 100.0 * unknown / max(total, 1)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
