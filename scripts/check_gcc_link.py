# -*- coding: utf-8 -*-
"""check_gcc_link.py — **闸外检查**: 甲言编出的 .obj 经 gcc 链接后必须正常运行.

## 为什么必须在闸外

`run_tests_zh.py`（闸）只做「**单文件 → 甲言编译 → 运行**」。
而第 262 站 ③ 那类缺陷（内联库函数用 32 位寄存器承载 64 位指针）**只在特定条件下暴露**:

    甲言自己链接自己     → 映像基址低（0x400000 量级）→ 32 位截断后恰好仍合法 → **测不出来**
    交给 gcc/ld 链接     → 基址 0x140000000、运行时 0x7ff6... → 截断地址立刻非法 → **必崩**

⟹ 「甲言能产出给别人调用的库」这条路径，**闸够不着** —— 这正是它能潜伏很久的原因。

## 用法

    python scripts/check_gcc_link.py          # 用当前最新链产物
    python scripts/check_gcc_link.py <qcc.exe>

退出码: 0 = 通过 · 1 = 失败（含编译/链接/运行/断言 任一）
"""
import os
import subprocess
import sys

根 = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
工作区 = os.path.join(根, 'scratch_gcclink')

MODULE = os.path.join(根, 'scripts', 'gate_gcc_link_module.c')
DRIVER = os.path.join(根, 'scripts', 'gate_gcc_link_driver.c')

期望 = ['G1=428A2F98', 'G2=0', 'G3=1', 'G4=1', 'GATE-GCC-LINK OK']


def 找编译器():
    for n in range(7, 0, -1):
        p = os.path.join(根, 'qcc_zh_v%d.exe' % n)
        if os.path.exists(p):
            return p
    p = os.path.join(根, 'qcc_x86.exe')
    return p if os.path.exists(p) else None


def 跑(cmd):
    p = subprocess.run(cmd, cwd=工作区, capture_output=True, timeout=300)
    return p.returncode, ((p.stdout or b'') + (p.stderr or b'')).decode('utf-8', 'replace')


def main():
    qcc = sys.argv[1] if len(sys.argv) > 1 else 找编译器()
    if not qcc or not os.path.exists(qcc):
        print('[闸外检查 gcc 链接] ❌ 找不到编译器'); return 1
    os.makedirs(工作区, exist_ok=True)
    obj = os.path.join(工作区, 'module.obj')
    app = os.path.join(工作区, 'app.exe')
    for p in (obj, app):
        if os.path.exists(p):
            os.remove(p)

    print('[闸外检查 gcc 链接] 编译器 = %s' % os.path.basename(qcc))

    rc, msg = 跑([qcc, '-c', MODULE, '-o', obj])
    if rc != 0 or not os.path.exists(obj):
        print('  ❌ ① 甲言 -c 失败 rc=%d\n%s' % (rc, msg.strip()[:300])); return 1
    print('  ✓ ① 甲言 -c  → %d 字节 .obj' % os.path.getsize(obj))

    rc, msg = 跑(['gcc', DRIVER, obj, '-o', app])
    if rc != 0 or not os.path.exists(app):
        print('  ❌ ② gcc 链接失败 rc=%d\n%s' % (rc, msg.strip()[:300])); return 1
    print('  ✓ ② gcc 链接 → app.exe')

    rc, out = 跑([app])
    if rc != 0:
        print('  ❌ ③ 运行崩溃 rc=%d (0x%08X)' % (rc, rc & 0xFFFFFFFF))
        print('     输出: %s' % out.strip()[:200])
        print('     ⟹ 典型签名: 内联库函数指针 32 位截断（第 262 站 ③）')
        return 1
    out = out.strip()
    miss = [w for w in 期望 if w not in out]
    if miss:
        print('  ❌ ④ 输出断言失败，缺: %s' % miss)
        print('     实际: %s' % out.replace('\n', ' | ')[:300]); return 1
    print('  ✓ ③ 运行 rc=0 · ④ 输出断言通过')
    print('  === 闸外检查 gcc 链接: PASS ===')
    return 0


if __name__ == '__main__':
    sys.exit(main())
