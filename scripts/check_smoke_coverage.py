# -*- coding: utf-8 -*-
"""check_smoke_coverage —— **jyld 冒烟必须覆盖 heap 符号维度**（G-D47-SMOKEFAKE 判据）。

病灶（2026-10-24）：`check_jyld_link_smoke.py` 5/5 PASS，但 5 个源里**没有一个** malloc/calloc
⟹ 不覆盖 `__qcc_heap_counter` 维度 ⟹ 真实工程（xmalloc 分 TU）必撞而 smoke 恒绿（假绿）。

判据（静态检查测试资产本身，确定性）：
  ① `check_jyld_link_smoke.py` 的源码文本必须出现 `malloc(` 或 `calloc(`；
  ② 必须出现 **两个** .o 都含堆调用的用例（即 `malloc`/`calloc` 出现次数 ≥ 2 且存在 `comp(` 两次）；
  ③ 该用例的两 obj 链接断言（`link(` + `expr`）必须在脚本里可定位。

用法: python scripts/check_smoke_coverage.py
退出码: 0 = smoke 已覆盖 heap 维度；1 = 未覆盖（假绿仍在）；2 = 被检脚本缺失（环境未就绪，不判绿）
"""
import io
import os
import re
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SMOKE = os.path.join(ROOT, 'scripts', 'check_jyld_link_smoke.py')


def main():
    if not os.path.exists(SMOKE):
        # 2026-10-24: 找不到被检对象 不得当绿（rc=2 = 环境未就绪）
        print('[FAIL] 找不到被检脚本 %s（未执行，不判绿）' % SMOKE)
        return 2
    s = io.open(SMOKE, encoding='utf-8', errors='replace').read()
    heap = len(re.findall(r'\b(malloc|calloc)\s*\(', s))
    has_multi = s.count('comp(') >= 2 and s.count('link(') >= 2
    ok = heap >= 2 and has_multi
    print('[heap calls in smoke] %d（需 ≥2，覆盖两个 obj）' % heap)
    print('[multi-obj link calls] comp=%d link=%d' % (s.count('comp('), s.count('link(')))
    print('=== smoke heap 覆盖: %s ===' % ('PASS' if ok else 'FAIL（假绿：heap 维度未覆盖）'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
