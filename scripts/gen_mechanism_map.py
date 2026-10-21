# -*- coding: utf-8 -*-
"""生成 D-COUNT 的「三口径映射」CSV（291 项 × 源码/锚 命中）。

口径：
  A = 清单存在（291 行，编号 1..291）
  B = 源码命中（关键词出现在 `srclib_jiayan/qcc_zh.jy`）
  C = 锚命中（关键词出现在 `scripts/feat_zh_*.c` 的文件名或内容）

产物：`grok-build/_data/mechanisms_291_map.csv`
      `num,key,src_hit,anchor_hit`

可复算：`check_count_calibers.py` 的判据 ⑦ 会实时重算并与本 CSV 逐行比对；
清单被改（删/改标题）或源码/锚被改（删关键词）⟹ CSV 不一致 ⟹ 判据红。
"""
import glob
import io
import os
import re
import sys

GB = r'C:\Users\Administrator\Desktop\grok-build'
JY = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
MECH = os.path.join(GB, '_data', 'mechanisms_291.txt')
OUT = os.path.join(GB, '_data', 'mechanisms_291_map.csv')
SRC = os.path.join(JY, 'srclib_jiayan', 'qcc_zh.jy')

def key_of(title):
    t = re.sub(r'[★☆*\s]', '', title)
    t = re.sub(r'P[012]', '', t)
    t = re.split(r'[（(]', t)[0]
    m = re.match(r'[\u4e00-\u9fa5]{2,}', t)
    if m:
        return m.group(0)[:6]
    m = re.match(r'[A-Za-z_][A-Za-z0-9_]{1,}', t)
    return m.group(0)[:12] if m else t[:6]


def build(mech_path=None):
    src = io.open(SRC, encoding='utf-8', errors='replace').read()
    anchors = []
    for p in glob.glob(os.path.join(JY, 'scripts', 'feat_zh_*.c')):
        anchors.append((os.path.basename(p), io.open(p, encoding='utf-8', errors='replace').read()))
    lines = []
    for line in io.open(mech_path or MECH, encoding='utf-8', errors='replace').read().split('\n'):
        if not line.strip():
            continue
        m = re.match(r'\s*(\d+)\t?(.*)$', line)
        if not m:
            continue
        num = int(m.group(1))
        title = m.group(2).strip()
        key = key_of(title)
        src_hit = 1 if key and key in src else 0
        anc_hit = 0
        for name, body in anchors:
            if key and (key in name or key in body):
                anc_hit = 1
                break
        lines.append('%d,%s,%d,%d' % (num, key, src_hit, anc_hit))
    return lines


def main():
    out = OUT
    mech = None
    if '--out' in sys.argv:
        out = sys.argv[sys.argv.index('--out') + 1]
    if '--mech' in sys.argv:
        mech = sys.argv[sys.argv.index('--mech') + 1]
    lines = build(mech)
    hdr = 'num,key,src_hit,anchor_hit'
    io.open(out, 'w', encoding='utf-8', newline='\n').write(hdr + '\n' + '\n'.join(lines) + '\n')
    b = sum(1 for l in lines if l.endswith(',1,0') or l.endswith(',1,1'))
    c = sum(1 for l in lines if l.endswith(',0,1') or l.endswith(',1,1'))
    print('rows=%d  src_hit=%d  anchor_hit=%d' % (len(lines), b, c))
    print('written', out)


if __name__ == '__main__':
    sys.exit(main())
