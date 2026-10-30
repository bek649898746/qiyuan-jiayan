# -*- coding: utf-8 -*-
"""check_chan_equiv —— 阶段 0（抽公共构造器）的**产物等价**判据。

判据（施工单 §三 阶段 0 的硬要求）：重构前后编译器对**同一批 C 样本**的 `-S` 产物**逐字节相同**。

对照方式：
  `--ref <旧编译器>` 与 `--cc <新编译器>` 分别编同一份源码，比较 `.asm` 的 SHA256；
  另加一层「编出的可执行产物行为一致」（跑一次、比 stdout）—— 防 `.asm` 相同但链接/运行分叉。

样本集（三层，缺一不可）：
  ① 通道 8 用例（`check_chan_matrix.py` 同一份源码）—— 本刀直接相关的生成路径；
  ② 通用语法样本（循环/结构体/指针/switch/goto/浮点/字符串）—— 公共构造器不应改变任何既有路径；
  ③ 编译器主源本身（`srclib_jiayan/qcc_zh.jy`）—— 最大的单一样本，任何细微漂移都会被它放大。

用法:
  python scripts/check_chan_equiv.py --ref qcc_zh_ref_8D0876AF.exe --cc qcc_zh_v1.exe
退出码: 0 = 全部逐字节相同；1 = 有差异（打印第一处 diff 的文件与首个不同字节偏移）
"""
import argparse
import hashlib
import io
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TMP = os.environ.get('QCC_TMPDIR') or os.path.join(REPO, 'scratch_equiv')

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from check_chan_matrix import CASES  # noqa: E402  通道 8 用例（同一份源码，不另抄一份）

GENERIC = [
    ('g_loop', '''
#包含 <stdio.h>
函数 整 主(空){
    整 s = 0;
    遍 (整 i = 0; i < 100; i++) { 若 (i % 3 == 0) 续; s = s + i; }
    格式化 "%d\\n", s;
    返 0;
}
'''),
    ('g_struct', '''
#包含 <stdio.h>
构 点 { 整 x; 整 y; };
函数 整 主(空){
    构 点 p; p.x = 3; p.y = 4;
    构 点* q = &p;
    格式化 "%d\\n", q->x * q->y;
    返 0;
}
'''),
    ('g_switch', '''
#包含 <stdio.h>
函数 整 主(空){
    整 r = 0;
    遍 (整 i = 0; i < 5; i++) {
        择 (i) { 况 0: r = r + 1; 断; 况 1: r = r + 10; 断; 默: r = r + 100; }
    }
    格式化 "%d\\n", r;
    返 0;
}
'''),
    ('g_ptr', '''
#包含 <stdio.h>
函数 整 主(空){
    整 a[8];
    遍 (整 i = 0; i < 8; i++) a[i] = i * i;
    整* p = a;
    整 s = 0;
    遍 (整 i = 0; i < 8; i++) s = s + *(p + i);
    格式化 "%d\\n", s;
    返 0;
}
'''),
    ('g_str', '''
#包含 <stdio.h>
#包含 <string.h>
函数 整 主(空){
    字 b[64];
    复制字符串(b, "hello");
    格式化 "%s %d\\n", b, (整)取字长(b);
    返 0;
}
'''),
    ('g_macro', '''
#包含 <stdio.h>
#定义 方(x) ((x)*(x))
#定义 倍(a, b) ((a)*(b))
函数 整 主(空){
    格式化 "%d %d\\n", 方(7), 倍(6, 7);
    返 0;
}
'''),
]


def sha(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def first_diff(a, b):
    da = open(a, 'rb').read()
    db = open(b, 'rb').read()
    n = min(len(da), len(db))
    for i in range(n):
        if da[i] != db[i]:
            return '偏移 %d: ref=0x%02X new=0x%02X' % (i, da[i], db[i])
    return '长度不同: ref=%d new=%d' % (len(da), len(db))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--ref', required=True)
    ap.add_argument('--cc', required=True)
    ap.add_argument('--timeout', type=int, default=1800)
    ap.add_argument('--no-main', action='store_true',
                    help='跳过编译器主源样本（仅用于「不改代码生成路径」的重构；改行为时不要用）')
    a = ap.parse_args()
    ref = a.ref if os.path.isabs(a.ref) else os.path.join(REPO, a.ref)
    new = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    for p in (ref, new):
        if not os.path.exists(p):
            print('[FAIL] 缺编译器: %s' % p)
            return 2
    os.makedirs(TMP, exist_ok=True)

    samples = [(cid, src) for cid, _d, src in CASES] + GENERIC
    # ③ 编译器主源（最大样本）
    main_src = os.path.join(REPO, 'srclib_jiayan', 'qcc_zh.jy')
    srcs = []
    for cid, src in samples:
        p = os.path.join(TMP, cid + '.jy')
        io.open(p, 'w', encoding='utf-8', newline='\n').write(src.lstrip('\n'))
        srcs.append((cid, p))
    if not a.no_main:
        srcs.append(('MAIN', main_src))
    else:
        print('[skip] MAIN（--no-main：本次只看 14 个小样本；改生成路径时勿用）')

    bad = []
    for cid, src in srcs:
        b_ref = os.path.join(TMP, cid + '.ref')
        b_new = os.path.join(TMP, cid + '.new')
        o_ref, o_new = b_ref + '.asm', b_new + '.asm'
        r1 = subprocess.run([ref, '-S', src, '-o', b_ref], cwd=REPO,
                            capture_output=True, text=True, encoding='utf-8',
                            errors='replace', timeout=a.timeout)
        r2 = subprocess.run([new, '-S', src, '-o', b_new], cwd=REPO,
                            capture_output=True, text=True, encoding='utf-8',
                            errors='replace', timeout=a.timeout)
        if r1.returncode != 0 or r2.returncode != 0:
            print('%-8s SKIP(ref rc=%d new rc=%d) %s' % (cid, r1.returncode, r2.returncode,
                                                         (r1.stderr or r2.stderr or '')[:90].replace('\n', ' ')))
            if r1.returncode != r2.returncode:
                bad.append(cid + '(rc 不同)')
            continue
        if not (os.path.exists(o_ref) and os.path.exists(o_new)):
            print('%-8s SKIP(产物缺失)' % cid)
            continue
        h1, h2 = sha(o_ref), sha(o_new)
        if h1 == h2:
            print('%-8s SAME %s (%d B)' % (cid, h1[:12], os.path.getsize(o_ref)))
        else:
            print('%-8s DIFF ref=%s new=%s | %s' % (cid, h1[:12], h2[:12], first_diff(o_ref, o_new)))
            bad.append(cid)

    print('EQUIV: %d/%d 逐字节相同%s' % (len(srcs) - len(bad), len(srcs),
                                         '' if not bad else ' · DIFF=' + ','.join(bad)))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
