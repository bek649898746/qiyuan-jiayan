#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""qclink —— 按需链接驱动（台账 D56 的 `--link` 载体）

为什么是独立驱动而不是 qcc 的开关：qcc 自身**没有进程派生原语**（源码里既无 `system()` 封装、
kernel32 导入表也只到 `CreateProcessW` 声明，缺 `GetExitCodeProcess`，加名字会位移单文件模式
的导入槽 ⟹ 直接影响不动点）。所以 `--link` 以「编译器 + 链接器」两段式驱动落地：
每个工具都保持单一职责，驱动只做参数编排与临时文件清理。

用法:
  python scripts/qclink.py a.c -o a.exe            # 单 TU 按需链接
  python scripts/qclink.py a.c -o a.exe --lib x.a  # 附加静态库（GNU ar，本仓 jyld --ar 打的）
  python scripts/qclink.py a.c -o a.exe --keep     # 保留中间 .obj 便于排障

退出码: 0 = 链接成功；1 = 失败；2 = 环境未就绪（找不到 qcc / jyld）
"""
import argparse
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def find(name, extra=()):
    for d in (ROOT, os.path.join(ROOT, 'build'), os.path.join(ROOT, 'srclib')) + tuple(extra):
        p = os.path.join(d, name)
        if os.path.exists(p):
            return p
    return ''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('src')
    ap.add_argument('-o', dest='out', required=True)
    ap.add_argument('--cc', default='')
    ap.add_argument('--jyld', default='')
    ap.add_argument('--lib', action='append', default=[])
    ap.add_argument('--keep', action='store_true')
    a = ap.parse_args()

    cc = a.cc or find('qcc_zh_v1.exe') or find('qcc_zh_seed.exe')
    jyld = a.jyld or find('jyld.exe')
    if not cc or not jyld:
        print('[FAIL] 环境未就绪: qcc=%r jyld=%r' % (cc, jyld))
        return 2

    obj = os.path.splitext(a.out)[0] + '.obj'
    r = subprocess.run([cc, '-c', a.src, '-o', obj], capture_output=True)
    msg = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')
    if r.returncode != 0:
        print(msg.strip())
        print('[FAIL] 编译阶段 rc=%d' % r.returncode)
        return 1

    cmd = [jyld, obj] + list(a.lib) + ['-o', a.out]
    r2 = subprocess.run(cmd, capture_output=True)
    msg2 = ((r2.stdout or b'') + (r2.stderr or b'')).decode('utf-8', 'replace')
    if r2.returncode != 0:
        print(msg2.strip())
        print('[FAIL] 链接阶段 rc=%d' % r2.returncode)
        return 1
    if not a.keep:
        try:
            os.remove(obj)
        except OSError:
            pass
    size = os.path.getsize(a.out) if os.path.exists(a.out) else -1
    print('OK: %s (link mode, %d bytes)' % (a.out, size))
    return 0


if __name__ == '__main__':
    sys.exit(main())
