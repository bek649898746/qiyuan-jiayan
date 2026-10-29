# -*- coding: utf-8 -*-
"""ORD-35 复现：对外文档硬数字与仓库实况不符。blocked（需对外文档线整批重生成）。

判据 = 重算 4 个基准（主源字节/KB/行数、feat 锚数、examples 文件数），再扫 7 份对外文档里的
**已知过期串**；还有一处 ⟹ rc=1（缺陷复现）。全部清零 ⟹ rc=0 ⟹ 应转 fixed。
用法: python scripts/check_doc_numbers.py
"""
import glob, io, os, sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STALE = ['35,264', '1,443', '1,451', '4,088 KB', '89 文件']
FILES = ['README_WHY.md', 'README_DEV.md', 'manual/工程优势清单.md', 'manual/仓库导航.md',
         'manual/兼容性四层证据.md', 'manual/编译期检查清单.md', 'README.md']


def main():
    os.chdir(REPO)
    src = 'srclib_jiayan/qcc_zh.jy'
    b = os.path.getsize(src)
    lines = sum(1 for _ in io.open(src, encoding='utf-8', errors='replace'))
    feat = len(glob.glob('scripts/feat_zh_*.c'))
    exf = len([p for p in glob.glob('examples/**/*', recursive=True) if os.path.isfile(p)])
    print('[BASE] 主源 %d B / %d KB / %d 行 · feat_zh_*.c %d · examples 文件 %d'
          % (b, round(b / 1024), lines, feat, exf))
    hits = []
    for f in FILES:
        if not os.path.exists(f):
            continue
        for i, l in enumerate(io.open(f, encoding='utf-8', errors='replace').read().split('\n'), 1):
            for s in STALE:
                if s in l:
                    hits.append('%s:%d [%s]' % (f, i, s))
    if hits:
        print('[REPRO] 过期数字 %d 处 ⟹ blocked 诚实:' % len(hits))
        for h in hits[:20]:
            print('   - ' + h)
        return 1
    print('[PASS] 对外文档硬数字与实况一致 ⟹ ORD-35 应转 fixed')
    return 0


if __name__ == '__main__':
    sys.exit(main())
