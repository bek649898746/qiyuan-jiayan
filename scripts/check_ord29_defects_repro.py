# -*- coding: utf-8 -*-
"""check_ord29_defects_repro —— ORD-29 施工单里**未修的三条**的可复算复现（裁定用，不是修复判据）。

为什么存在（2026-10-29 · skeptic 1 打回）：`ORD-20261027-29` 曾被记成 `fixed`，但单内
「缺一不算完」的五项里只有两项（缺陷四 文案/追踪号、缺陷五 并发示例 shim）落地；
缺陷一（借用语义）、缺陷二（通道满静默丢）、缺陷三（择收不随机）只做了**裁定**。
按台账口径，这种情况必须记 `settled` 且 **每条缺陷都有一条可复跑的复现命令** —— 本脚本就是那三条。

它断言的是**当前（已裁定、未修）的行为**：
  ① 共享借用期内仍可写（`借用 x; 整 *r=&x; *r=99;` → 编译通过且 x 变 99）—— 与 Rust `&T` 不同
  ② 可变借用期内**通过独占指针写也被拦**（`可变借用 x; 整 *m=&x; *m=99;` → A3001）—— 「可变借用」实为独占冻结
  ③ 容量 2 的通道送 5 收 5 → `11 22 0 0 0`（后三次**静默丢弃**，rc=0 无告警）
  ④ 两通道都就绪跑 20 次 → 全部命中 `例 0`（`例 1` 饿死，选择不随机）

若将来真修了其中一条，本脚本会**红** —— 那是提醒：同步更新裁定与文档，而不是让文档继续骗人。

用法: python scripts/check_ord29_defects_repro.py [--cc qcc_zh_v1.exe]
退出: 0 = 四条都与裁定一致（缺陷仍在）；1 = 有行为已变（需更新裁定）
"""
import argparse
import io
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

BORROW_SHARED = '''#include <stdio.h>
int main(void) {
    int x = 7;
    借用 x;
    int *r = &x;
    *r = 99;
    格式化 "SHARED x=%d\\n", x;
    return 0;
}
'''

BORROW_MUT = '''#include <stdio.h>
int main(void) {
    int x = 7;
    可变借用 x;
    int *m = &x;
    *m = 99;
    格式化 "MUT x=%d\\n", x;
    return 0;
}
'''

CHAN = '''#include <stdio.h>
int main(void) {
    通道 q, 2;
    送 q, 11; 送 q, 22; 送 q, 33; 送 q, 44; 送 q, 55;
    整 a = 0, b = 0, c = 0, d = 0, e = 0;
    收 q, a; 收 q, b; 收 q, c; 收 q, d; 收 q, e;
    格式化 "CHAN %d %d %d %d %d\\n", a, b, c, d, e;
    return 0;
}
'''

SEL = '''#include <stdio.h>
int main(void) {
    通道 p, 8; 通道 r, 8;
    整 n0 = 0, n1 = 0, k = 0, v = 0, i = 0;
    遍 (i = 0; i < 20; i++) {
        送 p, 100 + i; 送 r, 200 + i;
        择收(就绪(p), 就绪(r)) { 例 0: 收 p, v; n0++; 例 1: 收 r, v; n1++; 缺: k++; }
    }
    格式化 "SEL %d %d %d\\n", n0, n1, k;
    return 0;
}
'''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    w = tempfile.mkdtemp(prefix='qcc_ord29_repro_')
    fails = []
    print('=== ORD-29 三条未修缺陷的复现（裁定 · 2026-10-29）===')
    print('cc = %s' % cc)

    def build_run(name, src, expect_out, expect_rc=0):
        p = os.path.join(w, name + '.c')
        exe = os.path.join(w, name + '.exe')
        io.open(p, 'w', encoding='utf-8', newline='\n').write(src)
        r = subprocess.run([cc, p, '-o', exe], capture_output=True, text=True,
                           encoding='utf-8', errors='replace', timeout=180)
        if r.returncode != expect_rc:
            msg = ((r.stdout or '') + (r.stderr or '')).strip().split('\n')[0][:110]
            print('  %-14s 编译 rc=%d（期望 %d）%s' % (name, r.returncode, expect_rc, msg))
            return None
        if expect_rc != 0:
            print('  %-14s 编译 rc=%d（与裁定一致）' % (name, r.returncode))
            return 'RC%d' % r.returncode
        q = subprocess.run([exe], capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=60)
        out = (q.stdout or '').strip()
        ok = expect_out in out
        print('  %-14s out=%r 期望含 %r -> %s' % (name, out[:60], expect_out, ok))
        return out if ok else None

    o1 = build_run('A_shared_write', BORROW_SHARED, 'SHARED x=99')
    if o1 is None:
        fails.append('缺陷一 共享借用期可写 的行为已变')
    o2 = build_run('B_mut_readonly', BORROW_MUT, '', expect_rc=1)
    if o2 is None:
        fails.append('缺陷一 可变借用冻结 的行为已变')
    o3 = build_run('C_chan_drop', CHAN, 'CHAN 11 22 0 0 0')
    if o3 is None:
        fails.append('缺陷二 通道满静默丢 的行为已变')
    o4 = build_run('D_sel_starve', SEL, 'SEL 20 0 0')
    if o4 is None:
        fails.append('缺陷三 择收饥饿 的行为已变')

    print('---')
    print('裁定：① `可变借用` 是「独占冻结」而非 Rust `&mut`（共享借用期反而可写）⟹ 对外口径改「Rust 风格的部分静态检查」；')
    print('      ② 通道满静默丢弃（需真阻塞或失败可见）⟹ 另立单；③ 择收固定先命中小下标（需伪随机源）⟹ 另立单。')
    print('=== 结论: %s ===' % ('PASS（缺陷仍在，与裁定一致）' if not fails else 'FAIL %s' % fails))
    return 0 if not fails else 1


if __name__ == '__main__':
    sys.exit(main())
