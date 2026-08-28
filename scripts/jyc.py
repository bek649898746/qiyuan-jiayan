#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""甲言 编译器客户端 jyc.py — AI 工作流封装 (2026-08-28 真接入②)
一条命令完成: 编译+诊断 / 接口 / 引用 / DAG 自证 — AI 不用拼裸 CLI 输出.

用法:
  python scripts/jyc.py compile <file.c> [-o out.exe] [--cache] [--json]
  python scripts/jyc.py interface <file.c>
  python scripts/jyc.py refs <file.c>
  python scripts/jyc.py dag <file.c> [--json]
输出 (stdout, 单行 JSON):
  compile   → {"ok":bool,"out":str,"artifact":str?,"diagnostics":[...]}
  interface → {"interface":{...}}
  refs      → {"refs":[{name,sites}]}
  dag       → {"dag":{"version","stages":{...}}}
"""
import os
import sys
import json
import subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
QCC = os.path.join(ROOT, 'qcc_x86.exe')


def _run(args):
    return subprocess.run([QCC] + args, capture_output=True,
                          encoding='utf-8', errors='replace')


def parse_diags(stderr):
    """--dj 模式: stderr 里每行一个 JSON 诊断对象 (或 diagnostics 数组包装)."""
    diags = []
    for line in stderr.splitlines():
        line = line.strip()
        if not line.startswith('{'):
            continue
        try:
            d = json.loads(line)
        except ValueError:
            continue
        if 'diagnostics' in d:
            diags.extend(d['diagnostics'])
        elif 'code' in d:
            diags.append(d)
    return diags


def compile_file(src, out='a.exe', cache=False):
    args = ['--dj']
    if cache:
        args.append('--cache')
    r = _run(args + [src, '-o', out])
    artifact = None
    if r.returncode == 0:
        # 产物哈希从 stderr 的 SELFCHECK/缓存行取, 或直接算
        try:
            import hashlib
            artifact = hashlib.sha256(open(out, 'rb').read()).hexdigest()
        except OSError:
            pass
    return {
        'ok': r.returncode == 0,
        'out': out,
        'artifact': artifact,
        'diagnostics': parse_diags(r.stderr),
        'stderr_tail': r.stderr.strip()[-200:],
    }


def _payload(args, key):
    r = _run(args)
    if r.returncode != 0:
        return {'ok': False, 'stderr': r.stderr.strip()[-300:]}
    for line in r.stdout.splitlines():
        if '"%s"' % key in line:
            try:
                return {'ok': True, key: json.loads(line)[key]}
            except ValueError:
                return {'ok': False, 'stderr': 'payload parse failed: ' + line[:200]}
    return {'ok': False, 'stderr': 'no %s payload in stdout' % key}


def interface(src):
    return _payload(['--interface', src], 'interface')


def refs(src):
    return _payload(['--refs', src], 'refs')


def dag(src):
    return _payload(['--dag', src], 'dag')


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 0
    cmd = argv[1]
    if cmd == 'compile':
        if len(argv) < 3:
            print('jyc compile <file.c> [-o out] [--cache]')
            return 1
        src = argv[2]
        out = 'a.exe'
        cache = '--cache' in argv
        if '-o' in argv:
            out = argv[argv.index('-o') + 1]
        print(json.dumps(compile_file(src, out, cache), ensure_ascii=False))
    elif cmd == 'interface':
        if len(argv) < 3:
            return 1
        print(json.dumps(interface(argv[2]), ensure_ascii=False))
    elif cmd == 'refs':
        if len(argv) < 3:
            return 1
        print(json.dumps(refs(argv[2]), ensure_ascii=False))
    elif cmd == 'dag':
        if len(argv) < 3:
            return 1
        print(json.dumps(dag(argv[2]), ensure_ascii=False))
    else:
        print(__doc__)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
