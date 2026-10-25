#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""BLUEPAINT-OBJ 判据（**对象宏自引用**）—— 2026-10-23 已闭环（3/3）。

## 修前 / 修后（同一脚本、同一门槛）

| 形态 | 标准 C（gcc） | 修前（≤ 2026-10-22） | 修后（D1085A0B） |
|:--|:--|:--|:--|
| `#define X X` + `int X = 7; printf("%d", X)` | `R 7` | `R 0`（静默错值） | **`R 7`** |
| `#define A B` / `#define B A` + `int A = 3` | `R 3` | `R 0`（静默错值） | **`R 3`** |
| `#define X (X)` + `int X = 7; printf("%d", X)` | `R 7` | A1001 未声明 X | **`R 7`** |

## 两条根因（都修在根上，不是补丁）

1. **非数值对象宏被当成数值宏 0 登记**：`#define` 处理无条件 `外零()` 写 `终复[]`（数值宏表），
   没解析到数字时值 = 0；而词法器见到**任何**命中 `终复[]` 的标识符就直接变成常数 0
   ⟹ `#define X X` 之后每个 `X` 读 0，`Y = X;` 连 A1001 都不报（它已经不是标识符）。
   修：只有真解析到数值字面量才登记；`defined(X)` 的存在性判据补查对象宏表（与 L346 同口径）。
2. **文本层对象宏展开（`实退一`）没有名字栈**：递归展开宏体时无 C99 6.10.3.4p2 蓝漆
   ⟹ `#define X (X)` 逐层套括号到深度上限（32）⟹ `int (X) = 7;` 变成 32 层括号 ⟹ A1001。
   修：与 `副组` 共用名字栈（`漆中/漆压/漆弹`），命中已展开的名字原样输出。

函数宏自引用（git `error` 惯用法、sqlite3 `GLOBAL`）由 `check_bluepaint.py` 覆盖（4/4）。

用法：python scripts/check_bluepaint_obj.py [--cc <exe>]
退出码：0 = 全 PASS；1 = 有 FAIL
"""
import argparse
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORK = os.path.join(ROOT, 'scratch_bluepaint')

CASES = {
    'obj_self': (
        '#include <stdio.h>\n'
        '#define X X\n'
        'int main(void) { int X = 7; printf("R %d\\n", X); return 0; }\n',
        'R 7'),
    'obj_mutual': (
        '#include <stdio.h>\n'
        '#define A B\n'
        '#define B A\n'
        'int main(void) { int A = 3; printf("R %d\\n", A); return 0; }\n',
        'R 3'),
    'obj_paren': (
        '#include <stdio.h>\n'
        '#define X (X)\n'
        'int main(void) { int X = 7; printf("R %d\\n", X); return 0; }\n',
        'R 7'),
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    if not os.path.exists(cc):
        print('[SKIP] 环境未就绪: %s 不存在' % cc)
        return 2
    os.makedirs(WORK, exist_ok=True)
    print('# BLUEPAINT-OBJ 判据（对象宏自引用）  cc=%s' % os.path.basename(cc))
    total = bad = 0
    for name, (src, exp) in CASES.items():
        total += 1
        p = os.path.join(WORK, name + '.c')
        with open(p, 'w', encoding='utf-8', newline='\n') as f:
            f.write(src)
        exe = os.path.join(WORK, name + '.exe')
        r = subprocess.run([cc, p, '-o', exe], cwd=ROOT, capture_output=True,
                           env=dict(os.environ, QCC_SMALL_HEAP='1'), timeout=300)
        if r.returncode != 0:
            bad += 1
            msg = (r.stderr or r.stdout).decode('utf-8', 'replace').strip().splitlines()
            print('[%-12s] FAIL 编译 rc=%d  %s' % (name, r.returncode, (msg[0] if msg else '')[:110]))
            continue
        e = subprocess.run([exe], capture_output=True, timeout=60)
        out = e.stdout.decode('utf-8', 'replace').strip()
        good = (e.returncode == 0 and out == exp)
        if not good:
            bad += 1
        print('[%-12s] %s  %-8r  期望 %-6r' % (name, 'PASS' if good else 'FAIL', out, exp))
    print('# 汇总: %d/%d PASS' % (total - bad, total))
    return 0 if bad == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
