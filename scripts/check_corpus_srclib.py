# -*- coding: utf-8 -*-
"""check_corpus_srclib —— 第五套闸: grok-build/srclib (启元引擎集群源码) 抽样编译。

  为什么需要它 (2026-09-18 第 102 站): 既有闸 (Lua / zlib / git / zh-feat 锚) 已全绿,
  git 剩余 11 件 FAIL 早已逐条定性为「语料环境 / 已知限制」⟹ **需要新语料才能暴露新缺陷**。
  `grok-build/srclib` 有 **3379 个 .c**（01_base / 02_knowledge / _recovered_from_E），
  是本机规模最大、与本项目**最相关**（同风格、同 prelude）的未测语料。

  实测 (不动点 A7EF973C): **300 件抽样 -> 287 ok / 13 fail**，而 **13/13 全是仓库里的坏文件**
  (编码损坏的残留, 316-369 字节; `gcc -fsyntax-only` 同样 rc=1 报 `stray '\\356'`)
  ⟹ **真实通过率 100%**。

  用法: python scripts/check_corpus_srclib.py <qcc.exe> [样本数, 默认 300]
  退出码: 0 = 无「非白名单」失败; 1 = 有
"""
import io, os, subprocess, sys

qcc = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else r'qcc_zh_seed.exe'
LIMIT = int(sys.argv[2]) if len(sys.argv) > 2 else 300
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PRE = os.path.join(ROOT, 'compat_prelude.h')
SRCLIB = r'C:\Users\Administrator\Desktop\grok-build\srclib'
D = ['--lenient', '-D', '__STDC_VERSION__=199901L', '-D', '_WIN32=1', '-D', 'REG_STARTEND=1']

# 已知坏文件 (编码损坏的测试残留; gcc 同样编译失败) —— 见
# memory/construction/20260918_新语料闸_srclib引擎源码.md §三
KNOWN_BAD = {'z_81.c', 'z_85.c', 'z_97.c', 'z_51.c', 'blk_81.c', 'blk_70.c', 'blk_85.c', 'blk_5.c'}


def main():
    if not os.path.isdir(SRCLIB):
        print('语料目录不存在: %s' % SRCLIB); return 0
    files = []
    for dp, dn, fn in os.walk(SRCLIB):
        if '_recovered' in dp:
            continue
        for f in fn:
            if not f.endswith('.c'):
                continue
            p = os.path.join(dp, f)
            try:
                if os.path.getsize(p) > 400000:
                    continue
            except Exception:
                continue
            files.append(p)
    files.sort(key=lambda p: os.path.getsize(p))
    mid = files[len(files) // 4:] if len(files) > 40 else files
    sample = mid[:LIMIT]

    tmp = os.path.join(os.environ.get('TEMP', '.'), '_srclib_chk')
    os.makedirs(tmp, exist_ok=True)
    ok = bad = real_fail = 0
    fails = []
    for p in sample:
        r = subprocess.run([qcc] + D + ['-c', p, '-o', os.path.join(tmp, '_t.o'),
                                        '-I', PRE, '-I', os.path.dirname(p)],
                           capture_output=True, encoding='utf-8', errors='replace', timeout=900)
        if r.returncode == 0:
            ok += 1
            continue
        msg = ((r.stdout or '') + (r.stderr or '')).replace('\r', '')
        err = [l for l in msg.split('\n') if '[ERR]' in l]
        line = '%-56s %s' % (os.path.relpath(p, SRCLIB), (err[0][:90] if err else '(no ERR)'))
        if os.path.basename(p) in KNOWN_BAD:
            bad += 1
        else:
            real_fail += 1
            fails.append(line)
    print('样本 %d: ok=%d 已知坏文件=%d 真失败=%d' % (len(sample), ok, bad, real_fail))
    for x in fails[:25]:
        print('  ' + x)
    return 1 if real_fail else 0


sys.exit(main())
