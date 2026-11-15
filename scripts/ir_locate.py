#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ir_locate.py —— 「字节差 → 节点 → 源码行」一键定位（零探针法）

为什么需要：
    2026-11-13 那轮长线攻坚里，定位一个字节差平均要 2-3 次手工工具调用：
    `ir_fn_diff` 拿差异序号 → objdump 找地址 → `QCC_DBG_ALL` 编一次拿 `[ALLX]` →
    找最内层节点 → `QCC_DBG_CH` 再编一次拿形态与 `[CH2]` 的源码行。
    这套流程在 §130（shl → 节点 988/行 12882）、§131（mov → 节点 650）、
    §135（6 条 → 节点 1176/1178）上连续验证有效，本脚本把它固定下来。

关键前提（血泪，见台账 §104 / §113）：
    * `[ALLX]` 的 `ci/co` 与 `.o` 的偏移**同坐标**（K = 0）；
      早期用 `min_ci - 函数起点` 算出的 26 是「函数序言不进 IR发节点」造成的假差。
    * 探针是代码，会改变 AST ⟹ 本脚本每次都用**同一份源码**编译，
      节点号只在本次运行内有效，不要拿它的结果去对另一次编译。

用法：
    python scripts\\ir_locate.py --file lvm.c --fn luaV_concat --idx 143
    python scripts\\ir_locate.py --file lua.c --fn lua_settop --addr 0xd5a

输出：
    - 该指令在 `.o` 中的地址
    - 覆盖它的**最内层节点**（以及 2 层外层，便于看父子）
    - 该节点的 `[CH]`/`[CH2]` 全字段（码/型/态/素宽/免解/首/行…）
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC_DIR = r'D:\qcc_corpus\lua-5.4.7\src'
OBJDUMP = r'C:\mingw-w64\bin\objdump.exe'


def compile_one(cc, src, out, ir=True, extra=None):
    env = dict(os.environ)
    if ir:
        env['QCC_CODEGEN'] = 'ir'
    else:
        env.pop('QCC_CODEGEN', None)
    env['QCC_RT_PATH'] = os.path.join(REPO, 'srclib', 'qcc_rt.c')
    if extra:
        env.update(extra)
    p = subprocess.run([cc, src, '-c', '-o', out], capture_output=True, env=env,
                       cwd=REPO, encoding='utf-8', errors='replace')
    return p.returncode, (p.stdout or '') + (p.stderr or '')


def func_instrs(obj, fn):
    t = subprocess.run([OBJDUMP, '-d', '--no-show-raw-insn', obj],
                       capture_output=True).stdout.decode('utf-8', 'replace')
    cur, out = None, []
    for line in t.splitlines():
        if line[:1] in '0123456789abcdef' and '<' in line:
            cur = line.split('<')[1].split('>')[0]
            continue
        if cur == fn and '\t' in line:
            addr = line.split(':')[0].strip()
            out.append((int(addr, 16), line.rstrip()))
    return out


def _decode_err(raw):
    """stderr 编码是逐文件不同的：编译大文件时是 UTF-16LE（含大量零字节），
    小文件/浅路径是 UTF-8。**不能**用 try/except 判 —— `utf-16-le` 对任何偶数
    长度字节串都不抛异常，会把 UTF-8 文本解成乱码 ⟹ `'[ALLX]' in l` 全假 ⟹
    误报「不命中」。判据取零字节占比（实测 ldebug/lundump 为 UTF-8，lstring/lvm 为 UTF-16LE）。"""
    if raw.count(b'\x00') > len(raw) // 10:
        return raw.decode('utf-16-le', errors='replace')
    return raw.decode('utf-8', errors='replace')


def parse_allx(err_path, fn):
    raw = open(err_path, 'rb').read()
    s = _decode_err(raw)
    rows = []
    for l in s.split('\n'):
        if '[ALLX]' in l and fn in l:
            nums = re.findall(r'-?\d+', l)
            if len(nums) >= 4:
                rows.append(tuple(int(x) for x in nums[:4]))  # n, ci, co, 行
    return rows


def parse_ch(err_path, fn, node):
    raw = open(err_path, 'rb').read()
    s = _decode_err(raw)
    out = []
    for l in s.split('\n'):
        if '[CH' in l and fn in l and re.search(r'節=|节=|鑺\?%d' % node, l) and str(node) in l:
            out.append(l.strip())
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    ap.add_argument('--file', required=True, help='语料文件名，如 lvm.c')
    ap.add_argument('--fn', required=True, help='函数名')
    ap.add_argument('--idx', type=int, default=None, help='函数内指令序号（与 ir_fn_diff 的 @N 同口径，从 0 起）')
    ap.add_argument('--addr', default=None, help='直接给 .o 内地址（如 0xd5a）')
    a = ap.parse_args()

    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    if not os.path.exists(cc):
        print('[环境] 找不到编译器:', cc); return 2
    src = os.path.join(SRC_DIR, a.file)
    if not os.path.exists(src):
        print('[环境] 找不到语料:', src); return 2

    tmp = tempfile.mkdtemp(prefix='irloc_', dir=os.environ.get('TEMP'))
    ir_obj = os.path.join(tmp, 'ir.obj')

    rc, out = compile_one(cc, src, ir_obj)
    if rc != 0:
        print('[IR] 编译失败:', out[-400:]); return 3

    instrs = func_instrs(ir_obj, a.fn)
    if not instrs:
        print('[定位] 函数 %s 在 IR 产物里没有指令' % a.fn); return 4

    if a.addr:
        tgt = int(a.addr, 16) if not a.addr.startswith('0x') else int(a.addr, 16)
    elif a.idx is not None:
        if a.idx >= len(instrs):
            print('[定位] idx=%d 超出函数指令数 %d' % (a.idx, len(instrs))); return 5
        tgt = instrs[a.idx][0]
    else:
        print('[定位] 需要 --idx 或 --addr'); return 6

    print('函数 %s：共 %d 条指令' % (a.fn, len(instrs)))
    for addr, line in instrs:
        if abs(addr - tgt) <= 8:
            mark = ' <==' if addr == tgt else ''
            print('  0x%05x  %s%s' % (addr, line.split('\t')[-1].strip(), mark))

    allx_err = os.path.join(tmp, 'allx.err')
    compile_one(cc, src, os.path.join(tmp, 'x.o'), extra={'QCC_DBG_ALL': '1'})
    # 需要 stderr：重新跑一次并重定向
    env = dict(os.environ, QCC_CODEGEN='ir', QCC_DBG_ALL='1',
               QCC_RT_PATH=os.path.join(REPO, 'srclib', 'qcc_rt.c'))
    p = subprocess.run([cc, src, '-c', '-o', os.path.join(tmp, 'y.o')],
                       capture_output=True, env=env, cwd=REPO)
    open(allx_err, 'wb').write(p.stderr or b'')

    rows = parse_allx(allx_err, a.fn)
    hits = [r for r in rows if r[1] <= tgt < r[2]]
    hits.sort(key=lambda r: r[2] - r[1])
    if not hits:
        print('[ALLX] 目标地址不在任何节点的发射区间内 ⟹ 该字节可能来自 legacy 直发或回放')
        return 0
    print('\n[ALLX] 覆盖 0x%05x 的节点（最内层在前）：' % tgt)
    for n, ci, co, ln in hits[:4]:
        print('  n=%-6d ci=%-6d co=%-6d 行=%d' % (n, ci, co, ln))

    node = hits[0][0]
    ch_err = os.path.join(tmp, 'ch.err')
    env2 = dict(env, QCC_DBG_CH='1')
    env2.pop('QCC_DBG_ALL', None)
    p2 = subprocess.run([cc, src, '-c', '-o', os.path.join(tmp, 'z.o')],
                        capture_output=True, env=env2, cwd=REPO)
    open(ch_err, 'wb').write(p2.stderr or b'')
    print('\n[CH] 节点 %d 的形态：' % node)
    for l in parse_ch(ch_err, a.fn, node)[:4]:
        print(' ', l[:230])
    return 0


if __name__ == '__main__':
    sys.exit(main())
