# -*- coding: utf-8 -*-
"""ORD-59 判据：**Windows 平台标识 预定义 + 内置 `<windows.h>` 最小子集**。

病灶（2026-10-30 修前）：`#if defined(_WIN32)` 在甲言下 **判假**（gcc 判真）⟹ lua `lprefix.h` 的
`LUA_USE_WINDOWS` 不生效 ⟹ 整条 Windows 分支不走：
  · `loadlib.c` 的 `setprogdir` 用 `<windows.h>` 的 `MAX_PATH`/`DWORD`，但头被跳过 ⟹ 函数定义失败
    ⟹ 调用点 `A1001 未声明函数 'setprogdir'`；
  · `lmathlib.c` 的 `#if defined(Rand64)` 与 `#else`（no Rand64）**两条都编译** ⟹ `lu_int32` 先用后定义 ⟹ A1004。
  实测：加 `-D _WIN32=1 -D _WIN64=1` 后 `lmathlib.c` 直接 rc=0。

修法（两处）：
  ① 内置宏补 `_WIN32=1` / `_WIN64=1`（诚实：甲言生成 x86-64 Windows 代码，与 gcc 同口径）；
  ② 新增内置头 `<windows.h>` 最小子集（`MAX_PATH` / `DWORD` / `BOOL` / `HANDLE` / `HMODULE` /
     `FARPROC` / `FORMAT_MESSAGE_*` / `FormatMessageA` / `LoadLibraryA` / `GetProcAddress` /
     `GetModuleFileNameA` / `FreeLibrary` / `SetDllDirectoryA` / `GetLastError`）—— 只补 lua 实际用到的，
     不冒充完整 SDK；缺的符号仍 A1001（响亮）。

判据五格（每格 **编译 + 运行**，与 gcc 同源 oracle）：
  A `#if defined(_WIN32)` ⟹ 走真分支（运行期打印 W32）
  B `#if defined(_WIN64)` ⟹ 走真分支（打印 W64）
  C `#include <windows.h>` 后 `MAX_PATH`/`DWORD`/`FormatMessageA` 可用
  D 对照：非平台代码不受影响
  E 负控：`#if defined(_WIN32_ZZZ)` 仍判假（不得把任意下划线名都当真）

用法: python scripts/check_ord59_win32_platform.py [--cc qcc_zh_v1.exe]
"""
import argparse
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.environ.get('QCC_TMPDIR') or os.path.join(REPO, 'scratch_ord59')

CASES = [
    ('A_win32_true',
     '#include <stdio.h>\n'
     'int main (void) {\n'
     '#if defined(_WIN32)\n'
     '  printf("W32\\n");\n'
     '#else\n'
     '  printf("NO32\\n");\n'
     '#endif\n'
     '  return 0;\n'
     '}\n', 'W32'),
    ('B_win64_true',
     '#include <stdio.h>\n'
     'int main (void) {\n'
     '#if defined(_WIN64)\n'
     '  printf("W64\\n");\n'
     '#else\n'
     '  printf("NO64\\n");\n'
     '#endif\n'
     '  return 0;\n'
     '}\n', 'W64'),
    ('C_windows_h',
     '#include <windows.h>\n'
     '#include <stdio.h>\n'
     'int main (void) {\n'
     '  DWORD n = (DWORD)MAX_PATH;\n'
     '  void *p1 = (void *)FormatMessageA;   /* 只验证「声明存在」，不真调（真调依赖系统行为） */\n'
     '  void *p2 = (void *)LoadLibraryA;\n'
     '  void *p3 = (void *)GetProcAddress;\n'
     '  printf("WH %lu %d\\n", (unsigned long)n, (p1 != 0 && p2 != 0 && p3 != 0));\n'
     '  return 0;\n'
     '}\n', 'WH 260 1'),
    ('D_control',
     '#include <stdio.h>\n'
     'int main (void) { printf("CTL\\n"); return 0; }\n', 'CTL'),
    ('E_neg_unknown_platform',
     '#include <stdio.h>\n'
     'int main (void) {\n'
     '#if defined(_WIN32_ZZZ_NOT_REAL)\n'
     '  printf("BAD\\n");\n'
     '#else\n'
     '  printf("OK\\n");\n'
     '#endif\n'
     '  return 0;\n'
     '}\n', 'OK'),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or 'qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    if not os.path.exists(cc):
        print('[SKIP] 编译器不存在: %s' % cc)
        return 0
    os.makedirs(OUT, exist_ok=True)
    bad = 0
    for name, src, want_out in CASES:
        c = os.path.join(OUT, name + '.c')
        exe = os.path.join(OUT, name + '.exe')
        with open(c, 'w', encoding='utf-8', newline='\n') as f:
            f.write(src)
        p = subprocess.run([cc, c, '-o', exe], cwd=REPO, capture_output=True,
                           text=True, encoding='utf-8', errors='replace')
        if p.returncode != 0:
            print('[FAIL] %-24s 编译 rc=%d %s' % (name, p.returncode,
                                                  ((p.stderr or '').strip().splitlines() or [''])[0][:80]))
            bad += 1
            continue
        r = subprocess.run([exe], capture_output=True, text=True, encoding='utf-8', errors='replace')
        got = (r.stdout or '').strip().replace('\r', '')
        ok = (got == want_out)
        if not ok:
            bad += 1
        print('[%s] %-24s out=%r 期望=%r' % ('PASS' if ok else 'FAIL', name, got, want_out))
    print('==== check_ord59_win32_platform: %s (%d/%d) ====' % ('PASS' if bad == 0 else 'FAIL',
                                                               len(CASES) - bad, len(CASES)))
    return 0 if bad == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
