#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""gen_baseline —— A2 基线快照（可复跑、确定性输出）。

字段：不动点 / 种子 SHA / 主源行数与静态声明 / 锚数 / 行为测试数 /
5 类代表产物 SHA / 生成时间。所有数字现场计算，不读旁证。
用法: python scripts/gen_baseline.py [--out baseline_20261102.json]
"""
import argparse
import hashlib
import importlib.util
import json
import os
import re
import sys
import time

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)


def sha8(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()[:8]


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', default='baseline_20261102.json')
    a = ap.parse_args()

    ps = open(os.path.join(HERE, 'build_zh.ps1'), encoding='utf-8', errors='replace').read()
    m = re.search(r"\$expected\s*=\s*'([0-9A-Fa-f]{8})'", ps)
    expected = m.group(1) if m else None

    src = os.path.join(ROOT, 'srclib_jiayan', 'qcc_zh.jy')
    lines = open(src, encoding='utf-8', errors='replace').read().splitlines()
    jing = [l for l in lines if l.lstrip().startswith('静 ')]

    spec = importlib.util.spec_from_file_location('rtz', os.path.join(HERE, 'run_tests_zh.py'))
    rtz = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(rtz)
    anchors = len(rtz.CORE_FEAT)

    beh = 0
    for sub in ('qcc', 'behavior'):
        d = os.path.join(ROOT, 'tests', sub)
        if os.path.isdir(d):
            beh += len([f for f in os.listdir(d) if f.endswith('.c')])

    products = {}
    base = os.path.join(HERE, 'ir_stage1_baseline')
    for name in ('plain', 'struct', 'float', 'pointer', 'channel'):
        p = os.path.join(base, name + '.exe')
        products[name] = sha256(p) if os.path.exists(p) else None

    seed = os.path.join(ROOT, 'qcc_zh_seed.exe')
    out = {
        'generated_at': time.strftime('%Y-%m-%d %H:%M:%S'),
        'expected': expected,
        'seed_sha8': sha8(seed) if os.path.exists(seed) else None,
        'main_source': {
            'path': 'srclib_jiayan/qcc_zh.jy',
            'lines': len(lines),
            'bytes': os.path.getsize(src),
            'static_decls': len(jing),
            'static_arrays': len([l for l in jing if '[' in l]),
            'static_scalars': len([l for l in jing if '[' not in l]),
            'comment_lines': len([l for l in lines if l.lstrip().startswith(('/*', '//', '*'))]),
        },
        'anchors': anchors,
        'behavior_tests': beh,
        'products_sha256': products,
        'bootstrap_sec': None,  # 由 build_zh.ps1 现场输出；本快照不重跑自举
    }
    path = os.path.join(ROOT, a.out)
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(out, f, ensure_ascii=False, indent=2)
        f.write('\n')
    print(json.dumps(out, ensure_ascii=False, indent=2))
    print('written', path)


if __name__ == '__main__':
    main()
