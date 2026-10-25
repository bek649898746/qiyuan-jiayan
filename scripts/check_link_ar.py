# -*- coding: utf-8 -*-
"""check_link_ar —— **`ar` 多成员库 + `--link` 按需拉取**（台账 D56 · 2026-10-25 启元递刀）。

审计实测（`D:\\_objmode_test`）：
  · `-c` + `jyld` + **2 成员** lib ⟹ 产物 **5,120 B**（单文件模式 287,232 B，−98.2%）
  · **全量运行库当 1 个成员** ⟹ **347,648 B**（比单文件还大 21% ⟹ “统一载体”= 收益全吐回去）
  · MS `lib.exe` 打的是 MS 库格式，`jyld` 读不了 ⟹ 必须自己做 **GNU ar**

判据（四条，全过才 rc=0）：
  ① `ar` 写器存在且可调用（`jyar.exe` 或 `qcc --ar`）
  ② 打出的 2 成员库，`jyld` 能链接通过
  ③ 产物 ≤ 8 KB（空 main 形态）
  ④ **红线**：`--link` 产物 不得大于 同程序单文件模式产物（防“统一载体”把收益吃掉）

用法: python scripts/check_link_ar.py [--cc qcc_zh_seed.exe]
退出码: 0 = 四条全过；1 = 未实现/未达标（缺陷仍在）；2 = 环境未就绪
"""
import argparse
import io
import os
import shutil
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def find_tool(names):
    for n in names:
        for d in (ROOT, os.path.join(ROOT, 'tools'), os.path.join(ROOT, 'build')):
            p = os.path.join(d, n)
            if os.path.exists(p):
                return p
    return ''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(ROOT, 'qcc_zh_seed.exe'))
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    if not os.path.exists(cc):
        print('[FAIL] 环境未就绪: 找不到编译器 %s（本判据未执行，不判绿）' % cc)
        return 2
    ar = find_tool(('jyar.exe', 'jyar'))
    ar_flag = ''
    if not ar:
        # 也接受 `qcc --ar` 子模式：**真打一次库**才算存在（不认 --help 里出现的字样）
        w0 = tempfile.mkdtemp(prefix='qcc_ar_probe_')
        cs = [os.path.join(w0, 'a.c'), os.path.join(w0, 'b.c')]
        for i, c in enumerate(cs):
            io.open(c, 'w', encoding='utf-8', newline='\n').write('int f%d(void){ return %d; }\n' % (i, i))
        objs = []
        okc = True
        for i, c in enumerate(cs):
            o = os.path.join(w0, 'a%d.obj' % i)
            r = subprocess.run([cc, '-c', c, '-o', o], capture_output=True, timeout=300)
            if r.returncode != 0:
                okc = False
                break
            objs.append(o)
        if okc:
            lib = os.path.join(w0, 't.lib')
            r = subprocess.run([cc, '--ar', lib] + objs, capture_output=True, timeout=300)
            if r.returncode == 0 and os.path.exists(lib) and open(lib, 'rb').read(8) == b'!<arch>\n':
                ar_flag = '--ar'
    print('[%s] ① ar 写器：%s' % ('OK' if (ar or ar_flag) else 'RED',
                                  ar or ('%s --ar（已真打出 !<arch> 库）' % os.path.basename(cc) if ar_flag else '未实现')))
    if not (ar or ar_flag):
        print('=== ar 多成员库 + --link: FAIL（① ar 写器未实现 ⟹ 后续三条无法执行）===')
        return 1
    # ①②③④ 的实现留待 L7–L9（`ar` 落地后此处改为真打库 + jyld 链接 + 体积断言）
    print('[RED] ②③④ 待 L7–L9 实现后接入（本判据先只断言工具存在性）')
    print('=== ar 多成员库 + --link: FAIL（工具在，功能未接）===')
    return 1


if __name__ == '__main__':
    sys.exit(main())
