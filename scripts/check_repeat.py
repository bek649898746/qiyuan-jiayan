# -*- coding: utf-8 -*-
"""check_repeat —— **`--repeat N` 对任意 N 都必须可重入**（台账 D38b 判据）。

背景（2026-10-24 · skeptic 打回）：
  D38 只把 `--repeat 2` 修绿（R814），`--repeat 5` 第 3+ 次仍崩 0xC0000005；
  该残余此前只写在证据 README 的散文里 ⟹ 本轮改为**可复跑判据 + 独立台账行**。

判据（矩阵，任一格非 0 即红）：
  模式 ∈ {`-o`, `-S`, `-c`} × N ∈ {2, 3, 5}
  每格要求：rc=0 且（`-o`/`-S`/`-c` 非 `-S` 时）输出含 `[REPEAT]`；`-S` 只断言 rc=0 与产物存在。

用法: python scripts/check_repeat.py [--cc qcc_zh_seed.exe] [--modes -o,-S,-c] [--ns 2,3,5]
退出码: 0 = 全矩阵 PASS；1 = 有格失败（缺陷仍在）
"""
import argparse
import io
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = '#包含 <stdio.h>\n函数 整 主(空) { 格式化 "OK\\n"; 返 0; }\n'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(ROOT, 'qcc_zh_seed.exe'))
    ap.add_argument('--modes', default='-o,-S,-c')
    ap.add_argument('--ns', default='2,3,5')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    if not os.path.exists(cc):
        # 2026-10-24（判据方打回）: 缺主体不得当绿 —— rc=2 = 环境未就绪，不是 PASS
        print('[FAIL] 环境未就绪: 找不到编译器 %s（本判据未执行，不判绿）' % cc)
        return 2
    w = tempfile.mkdtemp(prefix='qcc_repeat_')
    src = os.path.join(w, 'p.c')
    io.open(src, 'w', encoding='utf-8', newline='\n').write(SRC)
    bad = []
    tripped = None
    for mode in [m.strip() for m in a.modes.split(',') if m.strip()]:
        if tripped:
            break
        for n in [x.strip() for x in a.ns.split(',') if x.strip()]:
            ext = {'-o': '.exe', '-S': '.asm', '-c': '.o'}[mode]
            out = os.path.join(w, 'o%s%s%s' % (mode.replace('-', ''), n, ext))
            if mode == '-o':
                args = [cc, '--repeat', n, src, '-o', out]
            else:
                args = [cc, mode, '--repeat', n, src, '-o', out]
            r = subprocess.run(args, capture_output=True, timeout=900)
            msg = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')
            ok = r.returncode == 0 and (mode == '-S' or '[REPEAT]' in msg)
            print('[%s N=%s] rc=%d %s' % (mode, n, r.returncode, 'OK' if ok else 'FAIL'))
            if not ok:
                bad.append('%s/N=%s(rc=%d)' % (mode, n, r.returncode))
                # 熔断（Agents.md 崩溃风暴铁律）：崩溃格 本身 已在 编译进程 里 触发 一次 0xC0000005，
                # 后续格 不再跑 —— D38b 只需要 一个 失败格 即可 判定；避免 单轮 连撞 6 次。
                if r.returncode in (3221225477, -1073741819, 3221225725):
                    tripped = bad[-1]
                    break
    if tripped:
        print('[熔断] %s 崩溃后停止，其余格未跑（判据只需一个失败格）' % tripped)
    print('=== --repeat N 矩阵: %s ===' % ('PASS' if not bad else 'FAIL -> ' + ', '.join(bad)))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
