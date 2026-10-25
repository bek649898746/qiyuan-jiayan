# -*- coding: utf-8 -*-
"""check_dag_path —— **`--dag` / `--refs` 与 `-graph` 的输出路径约定必须一致**（G-D50-DAGREFS）。

病灶（2026-10-24 现场）：
  `qcc --dag dag.json x.c -o x.exe` ⟹ rc=1「无法打开 dag.json」（把路径当输入源文件）；
  `qcc --dag x.c -o x.exe` ⟹ rc=0，JSON 打 stdout；
  `qcc -graph out.json x.c -o x.exe` ⟹ 能带路径。
三个接口三种约定，用户无法从 `--help` 分辨。

判据（两条）：
  ① `--dag <out.json> <src> -o <exe>` 必须 rc=0 且 out.json 存在、可解析 JSON；
  ② `--refs <out.json> <src>` 必须 rc=0 且 out.json 存在（或 `--refs` 明确写「输出到 stdout」且零参数 rc=0）。

用法: python scripts/check_dag_path.py [--cc qcc_zh_seed.exe]
退出码: 0 = 两约定一致；1 = 至少一条不符（缺陷仍在）
"""
import argparse
import io
import json
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = '#包含 <stdio.h>\n整 加一(整 x) { 返 x + 1; }\n函数 整 主(空) { 返 加一(1); }\n'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(ROOT, 'qcc_zh_seed.exe'))
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    if not os.path.exists(cc):
        # 2026-10-24（判据方打回）: 缺主体不得当绿 —— rc=2 = 环境未就绪，不是 PASS
        print('[FAIL] 环境未就绪: 找不到编译器 %s（本判据未执行，不判绿）' % cc)
        return 2
    w = tempfile.mkdtemp(prefix='qcc_dag_')
    src = os.path.join(w, 'p.c')
    io.open(src, 'w', encoding='utf-8', newline='\n').write(SRC)
    exe = os.path.join(w, 'p.exe')
    bad = []

    dag = os.path.join(w, 'dag.json')
    r = subprocess.run([cc, '--dag', dag, src, '-o', exe], capture_output=True, timeout=180)
    ok = r.returncode == 0 and os.path.exists(dag)
    if ok:
        try:
            json.load(io.open(dag, encoding='utf-8'))
        except Exception:
            ok = False
    print('[--dag <path>] rc=%d exists=%s %s' % (r.returncode, os.path.exists(dag), 'OK' if ok else 'REJECTED'))
    if not ok:
        bad.append('dag')

    refs = os.path.join(w, 'refs.json')
    r = subprocess.run([cc, '--refs', refs, src], capture_output=True, timeout=180)
    ok = r.returncode == 0 and os.path.exists(refs)
    print('[--refs <path>] rc=%d exists=%s %s' % (r.returncode, os.path.exists(refs), 'OK' if ok else 'REJECTED'))
    if not ok:
        bad.append('refs')

    print('=== --dag/--refs 路径约定: %s ===' % ('PASS' if not bad else 'FAIL'))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
