# -*- coding: utf-8 -*-
"""check_profile_v3 —— `--profile` 编译剖面 v3 判据（ORD-20261031-76）。

断言四件事（全部 由 发布编译器 真跑）：
  1. `--profile` 输出 **五段齐全**：【阶段】【规模】【诊断】【内置探针】【工作集估算】
  2. 八个热点计数 **均 > 0**（记用途/建节/发抽象/查变量/变量是否浮/变量型序/外部符号查/块）
  3. **默认路径零污染**：同一源 带/不带 `--profile` 的产物 **逐字节相同**
  4. `--help` 列出 `--profile`

用法: python scripts/check_profile_v3.py [--cc qcc_zh_v1.exe]
退出码: 0 = 四条全过；1 = 有红；2 = 环境未就绪
"""
import argparse
import hashlib
import io
import os
import re
import subprocess
import sys
import sys as _sys_tmp
_sys_tmp.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402  A1

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORK = tmp_dir('scratch_profile_v3')

SRC = '''# 包含 <stdio.h>
构 点 { 整 x; 整 y; };
静 整 加(整 a, 整 b){ 返 a + b; }
函数 整 主(空){
    整 s = 0; 整 i;
    遍 (i = 0; i < 10; i = i + 1) { s = 加(s, i); }
    构 点 p; p.x = s; p.y = s * 2;
    格式化 "%d %d\\n", p.x, p.y;
    返 0;
}
'''


def sha8(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for c in iter(lambda: f.read(1 << 20), b''):
            h.update(c)
    return h.hexdigest().upper()


def decode(raw):
    for enc in ('utf-8', 'gbk', 'utf-16'):
        try:
            return raw.decode(enc)
        except Exception:
            continue
    return raw.decode('utf-8', 'replace')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or 'qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    os.makedirs(WORK, exist_ok=True)
    src = os.path.join(WORK, 'profile_v3.jy')
    io.open(src, 'w', encoding='utf-8', newline='\n').write(SRC)

    out_plain = os.path.join(WORK, 'plain.exe')
    out_prof = os.path.join(WORK, 'prof.exe')
    r1 = subprocess.run([cc, src, '-o', out_plain], cwd=REPO, capture_output=True)
    r2 = subprocess.run([cc, '--profile', src, '-o', out_prof], cwd=REPO, capture_output=True)
    if r1.returncode != 0 or r2.returncode != 0:
        print('[编译红] plain rc=%d prof rc=%d\n%s' % (r1.returncode, r2.returncode,
              decode((r2.stdout or b'') + (r2.stderr or b''))[:400]))
        return 1
    txt = decode((r2.stdout or b'') + (r2.stderr or b''))

    ok = True
    secs = ['【阶段】', '【规模】', '【诊断】', '【内置探针】', '【工作集估算】']
    miss = [s for s in secs if s not in txt]
    print('[1 五段] %s %s' % ('PASS' if not miss else 'FAIL', '' if not miss else '缺 %s' % miss))
    ok = ok and not miss

    names = ['记用途', '建节', '发抽象', '查变量', '变量是否浮', '变量型序', '外部符号查', '块']
    bad = []
    for n in names:
        m = re.search(re.escape(n) + r'\s+(\d+)\s*次', txt)
        if not m or int(m.group(1)) <= 0:
            bad.append(n)
    print('[2 八热点] %s %s' % ('PASS' if not bad else 'FAIL', '' if not bad else '零/缺: %s' % bad))
    ok = ok and not bad

    same = sha8(out_plain) == sha8(out_prof)
    print('[3 零污染] %s plain=%s prof=%s' % ('PASS' if same else 'FAIL', sha8(out_plain)[:8], sha8(out_prof)[:8]))
    ok = ok and same

    h = subprocess.run([cc, '--help'], cwd=REPO, capture_output=True)
    htxt = decode((h.stdout or b'') + (h.stderr or b''))
    has = '--profile' in htxt
    print('[4 --help] %s' % ('PASS' if has else 'FAIL'))
    ok = ok and has

    if ok:
        print('[PASS] --profile v3 五段齐全 · 八热点非零 · 默认路径逐字节相同 · --help 有开关')
        return 0
    print('[FAIL] --profile v3 有红')
    return 1


if __name__ == '__main__':
    sys.exit(main())
