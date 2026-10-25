#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""BLUEPAINT 判据（函数宏）—— C99 6.10.3.4p2：正在被替换的宏名在重扫描中不再展开。

## 病灶（本轮已修）

`#define error(...) (error(__VA_ARGS__), const_error())`（git-compat-util.h 原样）是**自引用宏**：
标准要求内层 `error` **原样留下**（不再展开），于是展开结果是 `(error(__VA_ARGS__), const_error())`，
内层是对**真函数**的调用。修前编译器把它转成 `A6108 宏 自引用/递归 未实现` 响亮错误 ⟹
git 语料编不过；sqlite3.c 的 `GLOBAL` 自引用同病（修前首个错误即 A6108，修后越过预处理、
推进到下一个真实缺陷）。

## 判据

4 个形态，全部自足标准 C，gcc 真值写在 EXPECT：

| 形态 | gcc 真值 | 说明 |
|:--|:--|:--|
| git 惯用法（强） | `R 7 1` | 逗号表达式取 `const_error()`=7，且 `已调=1` 证明内层**真的调用了**函数 error |
| `#define f(x) f(x)` + 真函数 | `R 42` | 最简自引用 |
| 中文名多参数自引用 | `R 42` | 中文标识符 + 两参 |
| 普通嵌套宏 | `R 7` | 防矫枉过正（宏仍要正常展开） |

> **同根未修（另立 blocked 行 `BLUEPAINT-OBJ`）**：**对象宏**自引用 `#define X X` / `#define X (X)`
> 仍不正确（实测 `int X=7; printf(X)` 得 0；`#define X (X)` 报 A1001）—— 那是标识符解析侧，
> 与函数宏重扫描不是同一条路径，判据见 `scripts/check_bluepaint_obj.py`。

用法：
    python scripts/check_bluepaint.py             # 默认 qcc_zh_v1.exe
    python scripts/check_bluepaint.py --cc qcc_pd.exe
退出码：0 = 全 PASS，1 = 有 FAIL，2 = 环境未就绪
"""
import argparse
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORK = os.path.join(ROOT, 'scratch_bluepaint')

CASES = {
    'self_fn_git': (
        '#include <stdio.h>\n'
        'int 已调 = 0;\n'
        'int error(const char *s) { (void)s; 已调 = 1; return 42; }\n'
        'int const_error(void) { return 7; }\n'
        '#define error(...) (error(__VA_ARGS__), const_error())\n'
        'int main(void) { int r = error("boom"); printf("R %d %d\\n", r, 已调); return 0; }\n',
        'R 7 1'),
    'self_fn_simple': (
        '#include <stdio.h>\n'
        'int f(int v) { return v + 1; }\n'
        '#define f(x) f(x)\n'
        'int main(void) { printf("R %d\\n", f(41)); return 0; }\n',
        'R 42'),
    'self_fn_multi': (
        '#include <stdio.h>\n'
        'int 加(整 a, 整 b) { 返 a + b; }\n'
        '#define 加(a, b) (加(a, b) + 0)\n'
        'int main(void) { printf("R %d\\n", 加(40, 2)); return 0; }\n',
        'R 42'),
    'nested_ok': (
        '#include <stdio.h>\n'
        '#define N 5\n'
        '#define M (N + 2)\n'
        'int main(void) { printf("R %d\\n", M); return 0; }\n',
        'R 7'),
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    ap.add_argument('--only', default='')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    if not os.path.exists(cc):
        print('[SKIP] 环境未就绪: %s 不存在' % cc)
        return 2
    os.makedirs(WORK, exist_ok=True)
    print('# BLUEPAINT 判据（函数宏）  cc=%s  (C99 6.10.3.4p2)' % os.path.basename(cc))
    total = bad = 0
    for name, (src, exp) in CASES.items():
        if a.only and a.only not in name:
            continue
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
            print('[%-16s] FAIL 编译 rc=%d  %s' % (name, r.returncode, (msg[0] if msg else '')[:110]))
            continue
        e = subprocess.run([exe], capture_output=True, timeout=60)
        out = e.stdout.decode('utf-8', 'replace').strip()
        good = (e.returncode == 0 and out == exp)
        if not good:
            bad += 1
        print('[%-16s] %s  %-8r  期望 %-6r' % (name, 'PASS' if good else 'FAIL', out, exp))
    print('# 汇总: %d/%d PASS' % (total - bad, total))
    return 0 if bad == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
