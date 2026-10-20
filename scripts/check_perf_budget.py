# -*- coding: utf-8 -*-
"""check_perf_budget — 性能/体积类 fixed 项的**可复跑守卫**（2026-10-21 虾米）

背景：PERF-PAD / PERF-PAR / PERF-IMAGE / COMMIT-8 的原判据是**历史 A/B 计时/内存测量**，
在当前树上无法逐字复现。本判据把它们的**结论形态**固化成可跑的预算断言：

  --image   （PERF-IMAGE）种子 exe 的 `.data` VirtualSize ≤ 1400 MB，文件 ≤ 20 MB
  --commit  （COMMIT-8）`SizeOfImage` ≤ 2 GB 且 `.data` vsize ≤ 1400 MB（commit 余量的必要条件）
  --pad     （PERF-PAD）同一源连编两次，产物 **逐字节相同**（跳转/垫基址 稳定化 的直接后果）
  --par     （PERF-PAR）`run_tests_zh.py --fast --parallel 4` 全绿（4 路并行通道可用、无 1455）

用法: python scripts/check_perf_budget.py --image|--commit|--pad|--par [--cc <exe>]
"""
import argparse
import hashlib
import os
import struct
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REPO)

DATA_CAP = 1400 * 1024 * 1024
FILE_CAP = 20 * 1024 * 1024
IMG_CAP = 2 * 1024 * 1024 * 1024

SRC = '''整 主(空) { printf("PAD %d\\n", 42); 返 0; }
'''


def pe_image(cc):
    b = open(cc, 'rb').read()
    pe = struct.unpack_from('<I', b, 0x3c)[0]
    opt = pe + 0x18
    size_img = struct.unpack_from('<I', b, opt + 56)[0]
    machine, nsec = struct.unpack_from('<HH', b, pe + 4)
    off = pe + 0x18 + struct.unpack_from('<H', b, pe + 20)[0]
    data = None
    for i in range(nsec):
        base = off + 40 * i
        name = b[base:base + 8].rstrip(b'\0').decode('latin1')
        vsize = struct.unpack_from('<I', b, base + 8)[0]
        if name == '.data':
            data = vsize
    return size_img, data, len(b)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_seed.exe')
    ap.add_argument('--image', action='store_true')
    ap.add_argument('--commit', action='store_true')
    ap.add_argument('--pad', action='store_true')
    ap.add_argument('--par', action='store_true')
    a = ap.parse_args()
    mode = 'image' if a.image else 'commit' if a.commit else 'pad' if a.pad else 'par'

    if mode in ('image', 'commit'):
        size_img, data, fsz = pe_image(a.cc)
        ok_img = size_img <= IMG_CAP
        ok_data = data is not None and data <= DATA_CAP
        ok_file = fsz <= FILE_CAP
        if mode == 'image':
            ok = ok_data and ok_file
            print('[%s] .data VirtualSize=%.1fMB (cap %.0fMB) · 文件=%.1fMB (cap %.0fMB)'
                  % ('PASS' if ok else 'FAIL', (data or 0) / 1048576, DATA_CAP / 1048576,
                     fsz / 1048576, FILE_CAP / 1048576))
            print('=== PERF-IMAGE 守卫（静态镜像预算）: %s ===' % ('PASS' if ok else 'FAIL'))
        else:
            ok = ok_img and ok_data
            print('[%s] SizeOfImage=%.1fMB (cap %.0fMB) · .data VirtualSize=%.1fMB'
                  % ('PASS' if ok else 'FAIL', size_img / 1048576, IMG_CAP / 1048576, (data or 0) / 1048576))
            print('=== COMMIT-8 守卫（镜像/提交预算）: %s ===' % ('PASS' if ok else 'FAIL'))
        return 0 if ok else 1

    if mode == 'pad':
        d = tempfile.mkdtemp(prefix='perfpad_')
        src = os.path.join(d, 'p.jy')
        open(src, 'w', encoding='utf-8-sig').write(SRC)
        hs = []
        for k in range(2):
            exe = os.path.join(d, 'p%d.exe' % k)
            r = subprocess.run([a.cc, src, '-o', exe], capture_output=True, text=True,
                               encoding='utf-8', errors='replace')
            if r.returncode != 0:
                print('[FAIL] 编译失败 rc=%d\n%s' % (r.returncode, ((r.stdout or '') + (r.stderr or ''))[-300:]))
                return 1
            hs.append(hashlib.sha256(open(exe, 'rb').read()).hexdigest()[:16])
        ok = hs[0] == hs[1]
        print('[%s] 两次编译产物 %s / %s' % ('PASS' if ok else 'FAIL', hs[0], hs[1]))
        print('=== PERF-PAD 守卫（产物确定性）: %s ===' % ('PASS' if ok else 'FAIL'))
        return 0 if ok else 1

    # --par
    r = subprocess.run([sys.executable, 'scripts/run_tests_zh.py', '--fast', '--parallel', '4'],
                       capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=900)
    out = ((r.stdout or '') + (r.stderr or ''))
    ok = (r.returncode == 0 and 'FAIL=0' in out.replace(' ', ''))
    tail = [l for l in out.strip().split('\n') if 'PASS=' in l or '全绿' in l]
    print('[%s] 4 路 --fast：%s' % ('PASS' if ok else 'FAIL', tail[-1] if tail else out.strip()[-120:]))
    print('=== PERF-PAR 守卫（4 路并行通道）: %s ===' % ('PASS' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
