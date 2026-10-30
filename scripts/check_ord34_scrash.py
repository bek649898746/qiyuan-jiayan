# -*- coding: utf-8 -*-
"""ORD-34 复现：`-S` 编**编译器主源**崩溃（0xC0000005）。blocked（需专门一轮定位）。

判据 = 用 --cc 指定的编译器跑 `-S srclib_jiayan/qcc_zh.jy`。
  rc 为 0xC0000005 / >= 0xC0000000 ⟹ 崩溃复现（rc=1，blocked 诚实）
  rc == 0 且 .asm 非空 ⟹ 崩溃消失 ⟹ 应转 fixed
用法: python scripts/check_ord34_scrash.py [--cc qcc_zh_v1.exe]
"""
import argparse, os, subprocess, sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TMP = os.environ.get('QCC_TMPDIR') or os.path.join(REPO, 'scratch_ord34')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or 'qcc_zh_v1.exe')
    ap.add_argument('--timeout', type=int, default=900)
    ap.add_argument('--opt', default=os.environ.get('QCC_OPT_BIN') or 'opt_zh.exe',
                    help='优化器路径（判据二：-S 产物必须能被它读取）')
    ap.add_argument('--keep', action='store_true', help='保留 opt_zh 输出（默认删，省磁盘）')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    os.makedirs(TMP, exist_ok=True)
    out = os.path.join(TMP, 'main_S.exe')
    asm = out + '.asm'
    for p in (out, asm):
        if os.path.exists(p):
            os.remove(p)
    try:
        p = subprocess.run([cc, '-S', 'srclib_jiayan/qcc_zh.jy', '-o', out], cwd=REPO,
                           capture_output=True, text=True, encoding='utf-8', errors='replace',
                           timeout=a.timeout)
        rc = p.returncode
    except subprocess.TimeoutExpired:
        print('[FAIL] -S 超时（%ds）' % a.timeout)
        return 2
    size = os.path.getsize(asm) if os.path.exists(asm) else 0
    if rc != 0 or size <= 0:
        print('[REPRO] -S 编主源崩溃复现: rc=%d (0x%08X) asm=%d 字节 ⟹ blocked 诚实' % (rc, rc & 0xFFFFFFFF, size))
        return 1

    # 判据二（ORD-48 · 2026-10-30 虾米）：`.asm` 非空**不够** —— 还必须**可被 opt_zh 读取**。
    # 修前事实：opt_zh 对 9,630,163 行产物 `ERROR: 输入 … 超出 上限 4194304` ⟹ rc=1，
    # 而本脚本只断言 rc==0 + size>0 ⟹ 判据空心（skeptic 打回点）。
    opt = a.opt if os.path.isabs(a.opt) else os.path.join(REPO, a.opt)
    if not os.path.exists(opt):
        print('[FAIL] 缺优化器 %s ⟹ 「-S 产物可被 opt_zh 读取」无法成立（不是 SKIP）' % opt)
        return 2
    opt_out = out + '.opt.asm'
    if os.path.exists(opt_out):
        os.remove(opt_out)
    try:
        q = subprocess.run([opt, asm, '-o', opt_out], cwd=REPO, capture_output=True, text=True,
                           encoding='utf-8', errors='replace', timeout=a.timeout)
    except subprocess.TimeoutExpired:
        print('[FAIL] opt_zh 读 -S 产物超时（%ds）⟹ 判据二不成立' % a.timeout)
        return 2
    osize = os.path.getsize(opt_out) if os.path.exists(opt_out) else 0
    omsg = ((q.stdout or '') + (q.stderr or '')).strip().replace('\n', ' | ')
    if q.returncode != 0 or osize == 0:
        print('[FAIL] opt_zh 拒绝 -S 产物: rc=%d 输出=%d 字节（输入 %d 字节）' % (q.returncode, osize, size))
        print('       opt_zh: %s' % omsg[:400])
        return 1
    print('[PASS] -S 编主源不再崩溃（asm=%d 字节）且 opt_zh 可读（rc=0 输出=%d 字节）⟹ ORD-34 应转 fixed'
          % (size, osize))
    print('       opt_zh: %s' % omsg[:300])
    if not a.keep:
        try:
            os.remove(opt_out)
        except OSError:
            pass
    return 0


if __name__ == '__main__':
    sys.exit(main())
