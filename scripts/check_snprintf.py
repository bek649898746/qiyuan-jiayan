# -*- coding: utf-8 -*-
"""check_snprintf —— ORD-20261102-94：snprintf 族 size 语义（C99）判据。

判据（驱动真实发布编译器 + gcc oracle）：
  1. 甲言编译 `_audit_probe/snprintf_family.c` 的 stdout/rc 与 `gcc -O0 -w` oracle **逐行一致**；
  2. G1 截断后缓冲后 8 字节金丝雀仍全为 0xA5（输出含「金丝雀完好」）；
  3. size=1 时只写 buf[0]==0x00；
  4. `snprintf(NULL,0,...)` 返回 17 且不崩；
  5. 截断返回值仍是「本应写入数」（I1=10 / I2=8 / I3=10 / I4=11）。

用法: python scripts/check_snprintf.py [--cc qcc_zh_seed.exe]
退出码: 0 = PASS / 1 = RED
"""
import argparse
import io
import os
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROBE = os.path.join(REPO, '_audit_probe', 'snprintf_family.c')
INC = os.path.join(REPO, '_audit_probe', 'inc_snprintf')


def run(args, cwd, env=None):
    e = dict(os.environ)
    # ORD-94: 判据必须走编译器**自动**运行库选择（普通/WS 运行库均含 qcc_snprintf）。
    # 旧版强制 QCC_RT_PATH=srclib/qcc_rt.c ⟹ 掩盖了自动选择与 -c mini 两条路径。
    e.pop('QCC_RT_PATH', None)
    if env:
        e.update(env)
    r = subprocess.run(args, cwd=cwd, capture_output=True, env=e)
    return r.returncode, (r.stdout or b'').decode('utf-8', 'replace'), (r.stderr or b'').decode('utf-8', 'replace')


def include_cases(qcc, w, bad):
    """ORD-94 skeptic #2 回归：snprintf 族由 `# 包含` 的头文件 / `#define` / `-D` 引入时，
    运行库选择与前插**不得**依赖原始源文本（源文本看不到，词法期才展开）。"""
    d = os.path.join(w, 'inc')
    os.makedirs(d, exist_ok=True)
    for f in ('h3.jy', 'm3.jy', 'm4.jy', 'm5.jy'):
        shutil.copy(os.path.join(INC, f), os.path.join(d, f))
    jyld = os.path.join(REPO, 'srclib', 'jyld.exe')
    cases = [
        ('INC-EXE', 'm3.jy', [], 'm3.exe', True),
        ('INC-OBJ', 'm3.jy', [], 'm3.o', False),
        ('DMACRO-EXE', 'm4.jy', ['-D', 'FMT=snprintf'], 'm4.exe', True),
        ('DMACRO-OBJ', 'm4.jy', ['-D', 'FMT=snprintf'], 'm4.o', False),
        ('SRCMACRO-EXE', 'm5.jy', [], 'm5.exe', True),
        ('SRCMACRO-OBJ', 'm5.jy', [], 'm5.o', False),
    ]
    for tag, src, extra, outp, exe_mode in cases:
        if exe_mode:
            rc, out, err = run([qcc, src, '-o', outp] + extra, d)
            if rc != 0:
                print('FAIL %s 编译 rc=%d %r' % (tag, rc, err[:160]))
                bad.append(tag)
                continue
            r2, o2, e2 = run([os.path.join(d, outp)], d)
        else:
            rc, out, err = run([qcc, src, '-c', '-o', outp] + extra, d)
            if rc != 0:
                print('FAIL %s -c rc=%d %r' % (tag, rc, err[:160]))
                bad.append(tag)
                continue
            r3, o3, e3 = run([jyld, '-o', outp + '.exe', outp], d)
            if r3 != 0:
                print('FAIL %s jyld rc=%d %r' % (tag, r3, e3[:160]))
                bad.append(tag)
                continue
            r2, o2, e2 = run([os.path.join(d, outp + '.exe')], d)
        ok = r2 == 0 and o2.strip() == '10 abcdefg'
        print('%s %s | rc=%d out=%r' % ('PASS' if ok else 'FAIL', tag, r2, o2.strip()))
        if not ok:
            bad.append(tag)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(REPO, 'qcc_zh_seed.exe'))
    a = ap.parse_args()
    qcc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    w = tmp_dir('qcc_snprintf')
    bad = []

    rc, out, err = run([qcc, PROBE, '-o', 'after.exe'], w)
    if rc != 0 or not os.path.exists(os.path.join(w, 'after.exe')):
        print('[RED] 甲言编译探针失败 rc=%d %r' % (rc, err[:300]))
        bad.append('compile')
    rca, outa, erra = run([os.path.join(w, 'after.exe')], w)

    rc, out, err = run(['gcc', '-O0', '-w', PROBE, '-o', 'oracle.exe'], w)
    if rc != 0 or not os.path.exists(os.path.join(w, 'oracle.exe')):
        print('[RED] gcc oracle 编译失败 rc=%d %r' % (rc, err[:300]))
        bad.append('oracle')
    rco, outo, erro = run([os.path.join(w, 'oracle.exe')], w)

    if not bad:
        same = outa.strip().splitlines() == outo.strip().splitlines()
        print('%s 逐行一致 | after=%d oracle=%d' % ('PASS' if same else 'FAIL', len(outa.splitlines()), len(outo.splitlines())))
        if not same:
            bad.append('lines')
            for x, y in zip(outa.splitlines(), outo.splitlines()):
                if x != y:
                    print('  first diff:\n   after : %s\n   oracle: %s' % (x, y))
                    break
        ok = rca == 0 and rco == 0
        print('%s 两边 rc=0 | after=%d oracle=%d' % ('PASS' if ok else 'FAIL', rca, rco))
        if not ok:
            bad.append('rc')
        checks = [
            ('G1 金丝雀完好', 'G1-canary' in outa and '金丝雀完好' in outa and 'g2[0]=0xA5' in outa),
            ('G2 size=1 只写 NUL', 'G2-canary' in outa and 'buf[0]=0x00' in outa),
            ('I6 NULL/0 返回 17', 'I6-nullbuf' in outa and 'need=17' in outa),
            ('L1 5999 全量', '[PASS] L1-long' in outa and 'r=5999 len=5999' in outa),
            ('L2 5999 截断返回', '[PASS] L2-small' in outa and 'r=5999 len=15' in outa),
            ('L3 4096 全量', '[PASS] L3-4097' in outa and 'r=4096 len=4096' in outa),
            ('L4 20000 全量（无内部上限）', '[PASS] L4-20000-full' in outa and 'r=20000 len=20000' in outa),
            ('L5 20000 截断返回', '[PASS] L5-20000-trunc' in outa and 'r=20000 len=63' in outa),
            ('L6 4097 全量（4096+1 边界）', '[PASS] L6-4097-full' in outa and 'r=4097 len=4097' in outa),
            ('L7 4097 截断返回', '[PASS] L7-4097-trunc' in outa and 'r=4097 len=15' in outa),
            ('L8 6000 全量', '[PASS] L8-6000-full' in outa and 'r=6000 len=6000' in outa),
            ('L9 6000 截断返回', '[PASS] L9-6000-trunc' in outa and 'r=6000 len=15' in outa),
            ('S1 snprintf 名', '[PASS] S1-snprintf' in outa),
            ('S2 格式化限长 名', '[PASS] S2-jiayan' in outa),
            ('V1 vsnprintf 变体', '[PASS] V1-vsnprintf' in outa),
        ]
        for tag, ok in checks:
            print('%s %s' % ('PASS' if ok else 'FAIL', tag))
            if not ok:
                bad.append(tag)

    include_cases(qcc, w, bad)

    shutil.rmtree(w, ignore_errors=True)
    print('== snprintf 族判据: %s ==' % ('PASS' if not bad else 'FAIL (%s)' % ','.join(bad)))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
