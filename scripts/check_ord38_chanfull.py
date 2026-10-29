# -*- coding: utf-8 -*-
"""ORD-38 复现：① 通道满静默丢弃 ② 细化只查声明初值。blocked（两个设计决策待拍板）。

用法: python scripts/check_ord38_chanfull.py [--cc qcc_zh_v1.exe]
"""
import argparse, io, os, subprocess, sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TMP = os.environ.get('QCC_TMPDIR') or os.path.join(REPO, 'scratch_ord38')

CHAN = (
    '#包含 <stdio.h>\n'
    '函数 整 主(空){\n'
    '    通道 q, 2;\n'
    '    送 q, 1;\n'
    '    送 q, 2;\n'
    '    送 q, 3;\n'
    '    送 q, 4;\n'
    '    整 a=0; 整 b=0; 整 c=0;\n'
    '    收 q, a;\n'
    '    收 q, b;\n'
    '    收 q, c;\n'
    '    格式化 "满后收: %d %d %d\\n", a, b, c;\n'
    '    返 0;\n'
    '}\n'
)
REFINE = (
    '#包含 <stdio.h>\n'
    '函数 整 主(空){\n'
    '    细化 整(0, 100) 甲 = 50;\n'
    '    甲 = 150;\n'
    '    格式化 "赋值越界后 甲=%d\\n", 甲;\n'
    '    返 0;\n'
    '}\n'
)


def build_run(cc, name, src):
    os.makedirs(TMP, exist_ok=True)
    p = os.path.join(TMP, name + '.jy')
    exe = os.path.join(TMP, name + '.exe')
    io.open(p, 'w', encoding='utf-8').write(src)
    r = subprocess.run([cc, p, '-o', exe], cwd=REPO, capture_output=True, text=True,
                       encoding='utf-8', errors='replace')
    if r.returncode != 0:
        return r.returncode, (r.stderr or '')[:200], ''
    q = subprocess.run([exe], capture_output=True, text=True, encoding='utf-8', errors='replace')
    return q.returncode, '', (q.stdout or '').strip()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or 'qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    fails = []
    rc, err, out = build_run(cc, 'chan_full', CHAN)
    if rc != 0:
        fails.append('通道复现程序编译失败: %s' % err)
    else:
        print('[chan] 输出: %s' % out)
        if out.endswith('1 2 0'):
            fails.append('通道满仍静默丢弃（收 1 2 0，丢弃不可见）')
    rc, err, out = build_run(cc, 'refine_assign', REFINE)
    if rc == 0 and '150' in out:
        print('[refine] 输出: %s' % out)
        fails.append('细化 赋值越界仍放行（甲=150）')
    elif rc != 0:
        print('[refine] 已被拦截（rc=%d）' % rc)
    if fails:
        print('[REPRO] 缺陷复现 %d 项 ⟹ blocked 诚实:' % len(fails))
        for f in fails:
            print('   - ' + f)
        return 1
    print('[PASS] 两条均按规格收敛 ⟹ ORD-38 应转 fixed')
    return 0


if __name__ == '__main__':
    sys.exit(main())
