#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_corpus_zlib —— D6 真实语料 zlib 1.3.1 编译+运行档判据。

收口的历史缺陷（2026-11-03 · 虾米，全部已根治）：
  1. `extern char * const z_errmsg[10];`（zlib zutil.h）的 `*` 之后 `const`
     未被消费 ⟹ 整条 extern 声明被静默丢弃 ⟹ 单文件「假绿」/批编译 A6110。
  2. `(*configuration_table[s->level].func)(s, flush)` —— `表达式层级` 的成员基
     只认 变量/解引用，`tab[0].func`（下标基）落 -2 ⟹ 调用点多解引用一次 ⟹
     读函数首 8 字节当地址 ⟹ 0xC0000005。
  3. `typedef struct {...} T;` 的**标量数组字段** `int heap[2*L_CODES+1]` 走手写
     维度分支，只乘首因子 2 ⟹ 字段 8 字节（应 2292）⟹ 后续字段全错位。
  4. `typedef 名 const *字段`（`code const *lencode`）的 `const` 阻断 `*` 消费 ⟹
     指针字段按 typedef 基类型 4B 登记 ⟹ inflate_state 比 gcc 少 12 字节。
  5. typedef 指针字段继承了 typedef 结构体的**自身对齐**（4）⟹ 8 字节指针落在
     4 对齐偏移 ⟹ 布局再偏移。

断言：
  1. 最小锚 A：extern const 指针数组 + 先使用后定义（单文件/批编译）；
  2. 最小锚 B：`typedef struct` 内 `code *` / `code const *` 字段 按 8 字节
     且 8 对齐（sizeof=32 / p=8 / q=16）；
  3. zlib 15 个 .c 逐文件 `-D NO_MEMCPY -c` 全部 rc=0；
  4. 运行档：10 个核心 .c + 往返探针 → `srclib/jyld.exe` 链接 → 运行，
     输出 `ZLIB_OK <clen> 32`（compress→uncompress 往返，数据逐字节一致）。

用法: python scripts/check_corpus_zlib.py [--cc qcc_zh_seed.exe] [--zlib <dir>]
退出码: 0 = PASS / 1 = RED / 2 = 语料未就绪（SKIP，显式打印）
"""
import argparse
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, 'scripts'))
from qcc_tmp import tmp_dir  # noqa: E402

DEFAULT_ZLIB = os.environ.get('QCC_ZLIB_DIR', r'D:\qcc_corpus\zlib-1.3.1')

SOURCES = [
    'adler32.c', 'compress.c', 'crc32.c', 'deflate.c', 'gzclose.c', 'gzlib.c',
    'gzread.c', 'gzwrite.c', 'infback.c', 'inffast.c', 'inflate.c', 'inftrees.c',
    'trees.c', 'uncompr.c', 'zutil.c',
]
# 运行档：只需 compress/uncompress 路径（deflate/inflate/trees/zutil/adler/crc）
RUN_SOURCES = ['adler32.c', 'compress.c', 'crc32.c', 'deflate.c', 'inflate.c',
               'inffast.c', 'inftrees.c', 'trees.c', 'uncompr.c', 'zutil.c']

ANCHOR_DEF = 'char * const z_errmsg[10] = { "a","b","c","d","e","f","g","h","i","j" };\n'
ANCHOR_USE = (
    'extern char * const z_errmsg[10];\n'
    'int pick(int e) { return z_errmsg[e][0]; }\n'
)

ANCHOR_LAYOUT = r'''#include <stdio.h>
typedef struct code_s { unsigned char op; unsigned char bits; unsigned short val; } code;
struct S { int a; code const *p; code *q; int tail; };
int main(void) {
    struct S s;
    printf("LAYOUT %d %d %d %d\n", (int)sizeof(struct S),
      (int)((char*)&s.p-(char*)&s), (int)((char*)&s.q-(char*)&s), (int)((char*)&s.tail-(char*)&s));
    return 0;
}
'''

ROUNDTRIP = r'''#include <stdio.h>
#include "zlib.h"
int main(void) {
    unsigned char in[32], out[128], back[128];
    uLongf olen = sizeof(out), blen = sizeof(back);
    int i, r, ok = 1;
    for (i = 0; i < 32; i++) in[i] = (unsigned char)(i * 7 + 1);
    r = compress(out, &olen, in, sizeof(in));
    if (r != 0) { printf("COMPRESS_FAIL %d\n", r); return 1; }
    r = uncompress(back, &blen, out, olen);
    if (r != 0) { printf("UNCOMPRESS_FAIL %d\n", r); return 2; }
    if (blen != sizeof(in)) { printf("LEN_FAIL %lu\n", (unsigned long)blen); return 3; }
    for (i = 0; i < 32; i++) if (back[i] != in[i]) ok = 0;
    if (!ok) { printf("DATA_FAIL\n"); return 4; }
    printf("ZLIB_OK %lu %lu\n", (unsigned long)olen, (unsigned long)blen);
    return 0;
}
'''


def run(cc, args, cwd):
    return subprocess.run([cc] + args, cwd=cwd, capture_output=True, text=True,
                          encoding='utf-8', errors='replace')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(REPO, 'qcc_zh_seed.exe'))
    ap.add_argument('--zlib', default=DEFAULT_ZLIB)
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    bad = []
    if not os.path.exists(cc):
        print('[RED] 编译器不存在: %s' % cc)
        return 1
    if not os.path.isdir(a.zlib):
        print('[SKIP] zlib 语料未就绪: %s（设 QCC_ZLIB_DIR 或 --zlib）' % a.zlib)
        return 2
    jyld = os.path.join(REPO, 'srclib', 'jyld.exe')
    if not os.path.exists(jyld):
        print('[RED] 缺链接器: %s' % jyld)
        return 1

    w = tmp_dir('qcc_zlib_anchor')

    # 锚 A：extern const 指针数组（先使用后定义；修前 A6110）
    use = os.path.join(w, 'use.c')
    deff = os.path.join(w, 'def.c')
    open(use, 'w', encoding='utf-8').write(ANCHOR_USE)
    open(deff, 'w', encoding='utf-8').write(ANCHOR_DEF)
    for name, files in (('anchor-single', [use, deff]), ('anchor-batch', [use, deff])):
        r = run(cc, ['-c'] + files + ['-o', os.path.join(w, name + '.o')], w)
        out = (r.stdout or '') + (r.stderr or '')
        if r.returncode != 0 or 'A6110' in out or 'A1001' in out:
            bad.append('%s rc=%d %s' % (name, r.returncode, out.strip().replace('\n', ' ')[:160]))
        else:
            print('PASS %-14s extern const 指针数组（先使用后定义）' % name)

    # 锚 B：typedef 结构体指针字段布局
    lay = os.path.join(w, 'layout.c')
    open(lay, 'w', encoding='utf-8').write(ANCHOR_LAYOUT)
    r = run(cc, ['-c', lay, '-o', os.path.join(w, 'layout.o')], w)
    if r.returncode != 0:
        bad.append('layout 编译 rc=%d %s' % (r.returncode, (r.stderr or '')[:160]))
    else:
        r = run(jyld, [os.path.join(w, 'layout.o'), '-o', os.path.join(w, 'layout.exe')], w)
        if r.returncode != 0:
            bad.append('layout 链接 rc=%d' % r.returncode)
        else:
            rr = subprocess.run([os.path.join(w, 'layout.exe')], capture_output=True, text=True,
                                encoding='utf-8', errors='replace')
            line = (rr.stdout or '').strip()
            if line != 'LAYOUT 32 8 16 24':
                bad.append('layout 输出 %r（应 LAYOUT 32 8 16 24）' % line[:120])
            else:
                print('PASS %-14s typedef 指针字段 8B/8 对齐' % 'anchor-layout')

    # 1：zlib 15 个源文件逐文件编译
    ok = 0
    for src in SOURCES:
        p = os.path.join(a.zlib, src)
        if not os.path.exists(p):
            bad.append('缺语料文件 %s' % src)
            continue
        outf = os.path.join(w, src + '.o')
        r = run(cc, ['-D', 'NO_MEMCPY', '-c', src, '-o', outf], a.zlib)
        out = (r.stdout or '') + (r.stderr or '')
        if r.returncode != 0 or 'A1001' in out or 'A6110' in out:
            bad.append('%s rc=%d %s' % (src, r.returncode, out.strip().replace('\n', ' ')[:160]))
        elif not os.path.exists(outf) or os.path.getsize(outf) == 0:
            bad.append('%s 产物为空' % src)
        else:
            ok += 1
    print('zlib 1.3.1 单文件编译: %d/%d' % (ok, len(SOURCES)))

    # 2：运行档 —— compress/uncompress 往返
    objs = []
    for src in RUN_SOURCES:
        p = os.path.join(a.zlib, src)
        if not os.path.exists(p):
            bad.append('缺运行档语料 %s' % src)
            continue
        o = os.path.join(w, 'run_' + src + '.o')
        r = run(cc, ['-D', 'NO_MEMCPY', '-c', src, '-o', o], a.zlib)
        if r.returncode != 0:
            bad.append('运行档 %s rc=%d' % (src, r.returncode))
        else:
            objs.append(o)
    rt = os.path.join(w, 'roundtrip.c')
    open(rt, 'w', encoding='utf-8').write(ROUNDTRIP)
    rto = os.path.join(w, 'roundtrip.o')
    r = run(cc, ['-I', a.zlib, '-c', rt, '-o', rto], w)
    if r.returncode != 0:
        bad.append('roundtrip 编译 rc=%d %s' % (r.returncode, (r.stderr or '')[:160]))
    else:
        exe = os.path.join(w, 'roundtrip.exe')
        r = run(jyld, objs + [rto, '-o', exe], w)
        if r.returncode != 0:
            bad.append('roundtrip 链接 rc=%d' % r.returncode)
        else:
            rr = subprocess.run([exe], capture_output=True, text=True,
                                encoding='utf-8', errors='replace')
            line = (rr.stdout or '').strip()
            if not line.startswith('ZLIB_OK ') or not line.endswith(' 32'):
                bad.append('roundtrip 输出 %r（应 ZLIB_OK <n> 32；rc=%d）' % (line[:120], rr.returncode))
            else:
                print('PASS %-14s compress→uncompress 往返（%s）' % ('run-roundtrip', line))

    if bad:
        print('\n'.join('[RED] ' + x for x in bad))
        print('== zlib 编译+运行档判据: RED（%d 项）==' % len(bad))
        return 1
    print('== zlib 编译+运行档判据: PASS（15/15 + 2 锚 + 往返）==')
    return 0


if __name__ == '__main__':
    sys.exit(main())
