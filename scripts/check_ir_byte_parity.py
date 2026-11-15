#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_ir_byte_parity.py —— IR 真实语料逐字节等价判据（判据 C 的硬口径）

为什么要有它：
    `check_ir_lua.py` 只统计 rc=0，把「编得过但产物不同」也算通过。
    2026-11-12 实测：rc=0 的 15 个文件里有 4 个（lfunc/lstate/ltm/lundump）
    产物与 legacy 不同 —— 静默错产物。本脚本按 SHA256 逐字节判。

判据：
    每个 lua .c 在 legacy 与 IR 两条路径下 rc=0 且 .o SHA256 相同 ⟹ SAME。
    IR 侧失败必须带 IR-BACKEND-UNIMPL（响亮拒收），否则记 IR-FAIL-SILENT。

ratchet：
    --write-baseline FILE 记录本轮的 SAME 清单；
    --baseline FILE      上轮 SAME、本轮不 SAME ⟹ 判红（不许回退）。

用法：
    python scripts\\check_ir_byte_parity.py
    python scripts\\check_ir_byte_parity.py --cc qcc_zh_v2.exe --corpus D:\\qcc_corpus\\lua-5.4.7\\src
    python scripts\\check_ir_byte_parity.py --write-baseline scripts\\ir_parity_baseline.txt

退出码：0 = 全 SAME；1 = 有 DIFF/静默失败/回退；2 = 环境未就绪
"""
import argparse
import hashlib
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_CORPUS = r'D:\qcc_corpus\lua-5.4.7\src'
FALLBACK_CORPUS = os.path.join(ROOT, 'tests', 'corpus', 'lua-5.4.7', 'src')


def sha(path):
    if not os.path.exists(path):
        return None
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def build(exe, src, mode, out):
    env = dict(os.environ)
    env['QCC_CODEGEN'] = mode
    rt = os.path.join(ROOT, 'srclib', 'qcc_rt.c')
    if os.path.exists(rt):
        env['QCC_RT_PATH'] = rt
    p = subprocess.run([exe, src, '-c', '-o', out], capture_output=True, text=True,
                       encoding='utf-8', errors='replace', env=env)
    return p.returncode, (p.stdout or '') + (p.stderr or '')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(ROOT, 'qcc_zh_v2.exe'))
    ap.add_argument('--corpus', default=None)
    ap.add_argument('--baseline', default=None)
    ap.add_argument('--write-baseline', default=None)
    ap.add_argument('--json', default=None, help='把逐文件结果写成 JSON')
    a = ap.parse_args()

    exe = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    if not os.path.exists(exe):
        print(f'[环境] 找不到编译器: {exe}')
        return 2
    corpus = a.corpus or (DEFAULT_CORPUS if os.path.isdir(DEFAULT_CORPUS) else FALLBACK_CORPUS)
    if not os.path.isdir(corpus):
        print(f'[环境] 找不到语料: {corpus}')
        return 2

    files = sorted(f for f in os.listdir(corpus) if f.endswith('.c'))
    tmp = tempfile.mkdtemp(prefix='irpar_', dir=os.environ.get('TEMP'))
    rows = []

    for fn in files:
        src = os.path.join(corpus, fn)
        a_o = os.path.join(tmp, fn + '.legacy.o')
        b_o = os.path.join(tmp, fn + '.ir.o')
        for p in (a_o, b_o):
            if os.path.exists(p):
                os.remove(p)
        lrc, lout = build(exe, src, '', a_o)
        irc, iout = build(exe, src, 'ir', b_o)
        if lrc != 0 and irc != 0:
            cat = 'BOTH-FAIL'
        elif lrc != 0:
            cat = 'LEGACY-FAIL-ONLY'
        elif irc != 0:
            cat = 'IR-UNIMPL' if 'IR-BACKEND-UNIMPL' in iout else 'IR-FAIL-SILENT'
        else:
            cat = 'SAME' if sha(a_o) == sha(b_o) else 'DIFF'
        last = ''
        for line in (iout or '').splitlines():
            if 'IR-BACKEND-UNIMPL' in line:
                last = line.strip()
                break
        rows.append({'file': fn, 'cat': cat, 'legacy_rc': lrc, 'ir_rc': irc, 'ir_msg': last})

    from collections import Counter
    cnt = Counter(r['cat'] for r in rows)
    print('=' * 78)
    print('  IR 真实语料逐字节等价判据')
    print(f'  编译器: {exe}')
    print(f'  语料  : {corpus}（{len(rows)} 个 .c）')
    print('=' * 78)
    print(f'  SAME = {cnt.get("SAME", 0)} / {len(rows)}')
    for k in ('DIFF', 'IR-UNIMPL', 'IR-FAIL-SILENT', 'LEGACY-FAIL-ONLY', 'BOTH-FAIL'):
        if cnt.get(k):
            print(f'  {k} = {cnt[k]}')

    for k in ('DIFF', 'IR-FAIL-SILENT', 'LEGACY-FAIL-ONLY', 'BOTH-FAIL'):
        hits = [r for r in rows if r['cat'] == k]
        if hits:
            print(f'\n--- {k} ---')
            for r in hits:
                print(f'  {r["file"]:<16} legacy_rc={r["legacy_rc"]} ir_rc={r["ir_rc"]}  {r["ir_msg"][:100]}')

    unimpl = [r for r in rows if r['cat'] == 'IR-UNIMPL']
    if unimpl:
        print('\n--- IR-UNIMPL 形态分布 ---')
        fam = Counter()
        for r in unimpl:
            import re
            m = re.search(r'形态=([^\s]+)', r['ir_msg'])
            fam[m.group(1) if m else r['ir_msg'][:30]] += 1
        for k, v in fam.most_common():
            print(f'  {v:>2}  {k}')
        print('\n--- IR-UNIMPL 逐文件 ---')
        for r in unimpl:
            print(f'  {r["file"]:<16} {r["ir_msg"][:120]}')

    same = [r['file'] for r in rows if r['cat'] == 'SAME']
    if a.write_baseline:
        with open(a.write_baseline, 'w', encoding='utf-8') as f:
            f.write('\n'.join(same) + '\n')
        print(f'\n基线已写入 {a.write_baseline}（{len(same)} 条）')

    regress = []
    if a.baseline and os.path.exists(a.baseline):
        prev = {l.strip() for l in open(a.baseline, encoding='utf-8') if l.strip()}
        regress = sorted(prev - set(same))
        if regress:
            print('\n--- 回退（上轮 SAME、本轮不 SAME）---')
            for x in regress:
                print(f'  {x}')

    if a.json:
        import json as _json
        _json.dump(rows, open(a.json, 'w', encoding='utf-8'), ensure_ascii=False, indent=1)

    ok = (cnt.get('SAME', 0) == len(rows)) and not regress
    print()
    if ok:
        print(f'[PASS] 逐字节等价：{cnt.get("SAME", 0)}/{len(rows)} 全 SAME')
        return 0
    print(f'[FAIL] 逐字节等价：SAME={cnt.get("SAME", 0)}/{len(rows)}'
          + (f'，回退 {len(regress)}' if regress else ''))
    return 1


if __name__ == '__main__':
    sys.exit(main())
