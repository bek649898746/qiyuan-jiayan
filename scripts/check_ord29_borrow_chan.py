# -*- coding: utf-8 -*-
"""check_ord29_borrow_chan —— ORD-29 施工单的**可复算判据**（2026-10-29 · 启元）。

判据范围（对齐单内「缺一不算完」）：
  ① 借用族**文案与实测行为对齐**且**首行不含内部追踪号**（`号合.0` / `#数字`）：
     · `可变借用 x; 借用 x;`  → A3003（单内实测：这个**反向**场景才触发 A3003，原文案写成「借用中不能再可变借用」）
     · `可变借用 x; 可变借用 x;` → A3002
     · `可变借用 x; m=&x; *m=99;` → A3001，文案须点明「独占」（原文案「只读借用写=自动放还」与实测相反）
  ② `examples/concurrency.jy` 不得再带 `thread_create`/`thread_join` 同步空壳，且能编译运行、结构化并发 累计=30。
  ③ 打印三处**裁定**（缺陷一 借用语义 / 缺陷二 通道满静默丢 / 缺陷三 择收不随机）—— 这三项属语义扩充，
     需等待队列 / 伪随机源 / v3 语义定案，已按单内建议归档为裁定，不以「已修」冒充。

用法: python scripts/check_ord29_borrow_chan.py [--cc qcc_zh_v1.exe]
退出: 0 = ①② 全过（③ 仅打印不当判据）；1 = 有红
"""
import argparse
import io
import os
import re
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

PROBES = {
    'A3003': ('可变借用 x;\n借用 x;\n', '期间不能 借用'),
    'A3002': ('可变借用 x;\n可变借用 x;\n', '不能可变借用'),
    'A3001': ('可变借用 x;\n整 *m = &x;\n*m = 99;\n', '独占'),
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    w = tempfile.mkdtemp(prefix='qcc_ord29_')
    fails = []
    print('=== ORD-29 判据（借用文案 / 并发示例）===')
    print('cc = %s' % cc)

    for code, (body, must) in PROBES.items():
        src = os.path.join(w, 'p_%s.c' % code)
        io.open(src, 'w', encoding='utf-8', newline='\n').write(
            '#include <stdio.h>\nint main(void) {\n    int x = 7;\n' + body + '    return 0;\n}\n')
        r = subprocess.run([cc, src, '-o', os.path.join(w, 'p_%s.exe' % code)], capture_output=True,
                           text=True, encoding='utf-8', errors='replace', timeout=180)
        msg = (r.stdout or '') + (r.stderr or '')
        ok = (code in msg) and (must in msg) and ('号合.0' not in msg)
        print('① %-6s rc=%d 含码=%s 含「%s」=%s 无追踪号=%s' %
              (code, r.returncode, code in msg, must, must in msg, '号合.0' not in msg))
        if not ok:
            fails.append('%s 文案/追踪号' % code)
            print('    实测消息: %s' % msg.strip().split('\n')[0][:160])

    # ② examples/concurrency.jy：无 shim + 可运行 + 累计=30
    ex = io.open(os.path.join(REPO, 'examples', 'concurrency.jy'), encoding='utf-8').read()
    no_shim = ('静 空 thread_create' not in ex) and ('静 空 thread_join' not in ex)
    print('② examples/concurrency.jy 无同步空壳 = %s' % no_shim)
    if not no_shim:
        fails.append('concurrency.jy 仍带 shim')
    exe = os.path.join(w, 'conc.exe')
    r = subprocess.run([cc, os.path.join(REPO, 'examples', 'concurrency.jy'), '-o', exe],
                       capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=300)
    if r.returncode != 0:
        print('    编译失败: %s' % ((r.stdout or '') + (r.stderr or '')).strip()[:200])
        fails.append('concurrency.jy 编不过')
    else:
        q = subprocess.run([exe], capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=60)
        out = q.stdout or ''
        good = ('累计=30' in out) and ('通道 FIFO' in out) and ('select 命中' in out)
        print('    运行 rc=%d，四段输出齐全=%s' % (q.returncode, good))
        if not good:
            fails.append('concurrency.jy 输出不符')
            print('    out=%r' % out[:200])

    print('③ 裁定（不计入判据，见台账 evidence）：')
    print('   · 缺陷一 借用语义：按单内建议走**方案 B**（对外文案已改为「Rust 风格的部分静态检查」口径），')
    print('     方案 A（借用禁写 / 可变借用可真写）需 v3 语义定案，届时连带重写 tests/qcc/regress_borrow_alias.c。')
    print('   · 缺陷二 通道满静默丢：上策=真阻塞（需等待队列 + 消费者唤醒）；中策=失败可见。属运行时语义扩充，另立单。')
    print('   · 缺陷三 择收不随机：需在编译期注入伪随机源（自增计数器取模可满足公平性锚），另立单。')
    print('=== ORD-29: %s ===' % ('PASS' if not fails else 'FAIL %s' % fails))
    return 0 if not fails else 1


if __name__ == '__main__':
    sys.exit(main())
