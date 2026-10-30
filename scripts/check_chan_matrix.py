# -*- coding: utf-8 -*-
"""check_chan_matrix —— 通道机制 8 用例**行为基线**矩阵（施工单《通道机制结构根治》§5.1）。

定位（重要）：这是**行为基线闸**，不是「契约合规闸」。
  · 施工单 §5.1 列的 8 个用例，当前实现有的合规、有的不合规（① 静默丢弃 ② 空读旧值
    ③ 关闭后送不收约束 ④ 元素尺寸硬编码 4 ⑤ 锁标未用 ⑥ 通道不可传参）。
  · 这些「不合规」是**待大哥拍板的产品语义**（见 ORD-20261029-38），因此本闸**不判它们红**；
    它判的是「行为不许在重构中悄悄漂移」—— 阶段 0（抽公共构造器）要求行为逐字节不变，
    阶段 1（加计数）要求除新增字段外不变。
  · 基线值来自 `8D0876AF`（2026-10-30）实测；任何改动导致矩阵变化都必须显式更新本表并写明理由。

用法:
  python scripts/check_chan_matrix.py [--cc qcc_zh_v1.exe]          # 与基线比对（默认）
  python scripts/check_chan_matrix.py --print                        # 只打印实测（供更新基线）
退出码: 0 = 8/8 与基线一致；1 = 有漂移（打印 diff）
"""
import argparse
import io
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TMP = os.environ.get('QCC_TMPDIR') or os.path.join(REPO, 'scratch_chanmtx')

# ---- 8 个用例（施工单 §5.1 逐条）: (id, 说明, 甲言源码, 期望观测) ----
CASES = [
    ('c1', '容量2 · 送2收2 · 顺序且无丢弃', '''
#包含 <stdio.h>
函数 整 主(空){
    通道 q, 2;
    送 q, 1; 送 q, 2;
    整 a=0; 整 b=0;
    收 q, a; 收 q, b;
    格式化 "c1 %d %d\\n", a, b;
    返 0;
}
'''),
    ('c2', '容量2 · 送4收4 · 满时丢弃(静默) + 后两次取空', '''
#包含 <stdio.h>
函数 整 主(空){
    通道 q, 2;
    送 q, 1; 送 q, 2; 送 q, 3; 送 q, 4;
    整 a=0; 整 b=0; 整 c=0; 整 d=0;
    收 q, a; 收 q, b; 收 q, c; 收 q, d;
    格式化 "c2 %d %d %d %d\\n", a, b, c, d;
    返 0;
}
'''),
    ('c3', '容量2 · 满后 背压送 有空位时成功', '''
#包含 <stdio.h>
函数 整 主(空){
    通道 q, 2;
    送 q, 1; 送 q, 2;
    整 a=0;
    收 q, a;              /* 腾出一个空位 */
    背压 送 q, 9;
    整 b=0;
    收 q, b;
    格式化 "c3 %d %d\\n", a, b;
    返 0;
}
'''),
    ('c4', '空通道收 · v 保持原值(旧值语义)', '''
#包含 <stdio.h>
函数 整 主(空){
    通道 q, 2;
    整 v = 77;
    收 q, v;
    格式化 "c4 %d\\n", v;
    返 0;
}
'''),
    ('c5', '关闭后 送/收 行为(当前: 不读 closed 标)', '''
#包含 <stdio.h>
函数 整 主(空){
    通道 q, 2;
    关闭通道 q, 2;
    送 q, 5;
    整 v = 0;
    收 q, v;
    格式化 "c5 %d closed=%d\\n", v, 通道已关闭(q);
    返 0;
}
'''),
    ('c6', '就绪(q) 空/半满/满/就绪送 四种', '''
#包含 <stdio.h>
函数 整 主(空){
    通道 q, 2;
    整 r0 = 就绪(q);
    送 q, 1;
    整 r1 = 就绪(q);
    送 q, 2;
    整 r2 = 就绪(q);
    整 s2 = 就绪送(q);
    整 a=0;
    收 q, a;
    整 s1 = 就绪送(q);
    格式化 "c6 %d %d %d 送满=%d 送半=%d\\n", r0, r1, r2, s2, s1;
    返 0;
}
'''),
    ('c7', '通道作参数传给函数(当前不可; 记录编译结果)', '''
#包含 <stdio.h>
函数 空 灌(通道 c){
    送 c, 1;
}
函数 整 主(空){
    通道 q, 2;
    灌(q);
    整 a=0;
    收 q, a;
    格式化 "c7 %d\\n", a;
    返 0;
}
'''),
    ('c8', '两个通道互不干扰', '''
#包含 <stdio.h>
函数 整 主(空){
    通道 q, 2;
    通道 r, 2;
    送 q, 1; 送 r, 2; 送 q, 3;
    整 a=0; 整 b=0; 整 c=0;
    收 q, a; 收 r, b; 收 q, c;
    格式化 "c8 %d %d %d 就绪r=%d\\n", a, b, c, 就绪(r);
    返 0;
}
'''),
]

# 基线（8D0876AF · 2026-10-30 实测）。'<compile-fail>' 表示当前编译器拒绝该形态。
BASELINE = {
    'c1': 'ok|c1 1 2',
    'c2': 'ok|c2 1 2 0 0',
    'c3': 'ok|c3 1 2',   # 实测: 收 1（腾位）→ 背压送 9 入队 → 再收得 2（FIFO 里 2 在 9 前）
    'c4': 'ok|c4 77',
    'c5': 'ok|c5 5 closed=1',
    'c6': 'ok|c6 0 1 1 送满=0 送半=1',
    'c7': 'compile-fail',
    'c8': 'ok|c8 1 2 3 就绪r=0',
}


def run_case(cc, cid, src):
    os.makedirs(TMP, exist_ok=True)
    p = os.path.join(TMP, cid + '.jy')
    exe = os.path.join(TMP, cid + '.exe')
    io.open(p, 'w', encoding='utf-8', newline='\n').write(src.lstrip('\n'))
    r = subprocess.run([cc, p, '-o', exe], cwd=REPO, capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=300)
    if r.returncode != 0:
        return 'compile-fail', ((r.stderr or '').strip().splitlines() or [''])[0][:120]
    q = subprocess.run([exe], capture_output=True, text=True, encoding='utf-8',
                       errors='replace', timeout=120)
    out = (q.stdout or '').strip().replace('\r', '')
    return 'ok|' + out, ''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or 'qcc_zh_v1.exe')
    ap.add_argument('--print', dest='do_print', action='store_true', help='只打印实测（更新基线用）')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    if not os.path.exists(cc):
        print('[FAIL] 编译器不存在: %s' % cc)
        return 2

    drift = []
    for cid, desc, src in CASES:
        got, err = run_case(cc, cid, src)
        base = BASELINE.get(cid, '')
        same = (got == base)
        tag = 'BASE' if same else 'DRIFT'
        print('[%s] %-2s %-34s got=%-40s%s' % (tag, cid, desc, got,
                                               '' if same else ('  base=' + base)))
        if not same:
            drift.append(cid)
            if err:
                print('       err: %s' % err)
    print('MATRIX: %d/%d 与基线一致%s' % (len(CASES) - len(drift), len(CASES),
                                          '' if not drift else ' · DRIFT=' + ','.join(drift)))
    print('note: 基线取自 8D0876AF（2026-10-30）。c7=compile-fail 是**当前缺陷**（通道不可传参），'
          '不是期望语义；修好它时须显式更新本表。')
    if a.do_print:
        print('--- 更新用（BASELINE 块）---')
        for cid, desc, src in CASES:
            got, _ = run_case(cc, cid, src)
            print("    '%s': '%s'," % (cid, got))
        return 0
    return 0 if not drift else 1


if __name__ == '__main__':
    sys.exit(main())
