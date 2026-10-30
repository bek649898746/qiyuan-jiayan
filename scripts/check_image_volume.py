# -*- coding: utf-8 -*-
"""check_image_volume —— `-o` 产物镜像体积判据（施工单《产物体积根因》§四 1–3 条）。

为什么分两档（2026-10-28 · R891）：
  施工单把「96 MB 地板」与「运行库无 DCE」写成一条总判据（`.data` ≤2 MB / SizeOfImage ≤3 MB /
  `.text` Raw ≤8 KB）。但这是**两个独立根因**，删地板只解决第一个：
    · `--stage floor`：地板已撤 ⟹ `.data` 从 96 MB 降到 ~9 MB（运行库真实静态区），
      SizeOfImage 同步降到 ~9.3 MB；这一档证明「虚胖的 87 MB 死空间」没了。
    · `--stage dce`（默认）：施工单原判据（≤2 MB / ≤3 MB / ≤8 KB），衡量「运行库按需链接」是否落地；
      当前仍红 ⟹ 对应台账 `VOLUME-DCE-RT`（blocked）。**不为让它变绿而放宽阈值。**

用法:
  python scripts/check_image_volume.py [--cc qcc_zh_v1.exe] [--stage floor|dce]
退出码: 0 = 该档达标；1 = 未达标；2 = 环境未就绪（编译器不存在，不当绿）
"""
import argparse
import io
import os
import struct
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

STAGES = {
    # 档位: (.data VSize 上限, SizeOfImage 上限, .text RawSize 上限)
    'floor': (10 * 1048576, 11 * 1048576, 300 * 1024),   # 撤 96MB 地板后实测 9.0 / 9.28 / 280KB
    'static': (2 * 1048576, 3 * 1048576, 1 << 30),        # 撤 8MB 静态区地板后 .data/SizeOfImage 达标（.text 不限）
    'dce': (2 * 1048576, 3 * 1048576, 8 * 1024),          # 施工单 §四 1–3 条原文（含 .text ≤8KB）
}


def pe_metrics(path):
    d = open(path, 'rb').read()
    pe = struct.unpack_from('<I', d, 0x3C)[0]
    if d[pe:pe + 4] != b'PE\0\0':
        raise ValueError('not PE: %s' % path)
    nsec = struct.unpack_from('<H', d, pe + 6)[0]
    optsz = struct.unpack_from('<H', d, pe + 20)[0]
    opt = pe + 24
    size_img = struct.unpack_from('<I', d, opt + 56)[0]
    sec = opt + optsz
    info = {}
    for i in range(nsec):
        off = sec + i * 40
        name = d[off:off + 8].rstrip(b'\0').decode('latin1')
        vsize, va, rawsz, rawptr = struct.unpack_from('<IIII', d, off + 8)
        info[name] = {'vsize': vsize, 'va': va, 'raw': rawsz, 'ptr': rawptr}
    return size_img, info, len(d)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(ROOT, 'qcc_zh_v1.exe'))
    ap.add_argument('--stage', choices=sorted(STAGES), default='dce')
    # `--product <旧编译器>`：**仅修前取证**用（与 `run_criteria --product` 同语义）——
    # 让「修前红」与「修后绿」是同一支判据、同一串命令，只在末尾多一个旧产物参数。
    ap.add_argument('--product', default=None,
                    help='修前腿：用这个旧编译器产物代替 --cc（同源判据，只换产物）')
    a = ap.parse_args()
    a.cc = a.product or a.cc
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    if not os.path.exists(cc):
        print('[FAIL] 环境未就绪: 找不到编译器 %s（本判据未执行，不判绿）' % cc)
        return 2

    cap_data, cap_img, cap_text = STAGES[a.stage]
    w = tempfile.mkdtemp(prefix='qcc_vol_')
    src = os.path.join(w, 'hello.c')
    io.open(src, 'w', encoding='utf-8', newline='\n').write(
        '#include <stdio.h>\nint main(void){ printf("VOL-OK\\n"); return 0; }\n')
    out = os.path.join(w, 'hello.exe')
    r = subprocess.run([cc, src, '-o', out], cwd=ROOT, capture_output=True, text=True,
                       encoding='utf-8', errors='replace')
    if r.returncode != 0 or not os.path.exists(out):
        print('[FAIL] 编译失败 rc=%d: %s' % (r.returncode, (r.stderr or '')[:200]))
        return 1

    size_img, secs, fsz = pe_metrics(out)
    data = secs.get('.data', {}).get('vsize', 0)
    text_raw = secs.get('.text', {}).get('raw', 0)
    print('[%s] .data VSize=%.2fMB (cap %.2f) · SizeOfImage=%.2fMB (cap %.2f) · '
          '.text Raw=%.1fKB (cap %.1f) · file=%.2fMB'
          % (a.stage, data / 1048576.0, cap_data / 1048576.0,
             size_img / 1048576.0, cap_img / 1048576.0,
             text_raw / 1024.0, cap_text / 1024.0, fsz / 1048576.0))
    bad = []
    if data > cap_data:
        bad.append('.data %.2fMB > %.2fMB' % (data / 1048576.0, cap_data / 1048576.0))
    if size_img > cap_img:
        bad.append('SizeOfImage %.2fMB > %.2fMB' % (size_img / 1048576.0, cap_img / 1048576.0))
    if text_raw > cap_text:
        bad.append('.text Raw %.1fKB > %.1fKB' % (text_raw / 1024.0, cap_text / 1024.0))
    if bad:
        print('[FAIL] %s: %s' % (a.stage, '; '.join(bad)))
        return 1
    print('[PASS] %s 三指标达标' % a.stage)
    return 0


if __name__ == '__main__':
    sys.exit(main())
