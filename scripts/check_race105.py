#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_race105 —— ORD-20261104-105 ③：编译期数据竞争检查（A3120）。

大哥 2026-11-06 裁定：**仅甲言模式默认开**。
规则（施工单 §三 最保守一条）：文件级可变全局在 `结构化并发 { … }` 块内被裸写 ⟹ A3120。

判据（真跑编译器，五个用例）：
  负例 1  `结构化并发 { g = 1; }`    ⟹ rc≠0 且含 A3120
  负例 2  `结构化并发 { g += 1; }`   ⟹ rc≠0 且含 A3120（复合赋值同口）
  正例 1  块内只写局部变量            ⟹ rc=0（局部是每线程自己的栈，不共享）
  正例 2  块外写全局                  ⟹ rc=0
  正例 3  纯 C 源（无中文关键字 ⟹ 甲言模式=0）写全局 ⟹ rc=0（C 兼容）

用法: python scripts/check_race105.py
退出码: 0 = 五例全符
"""
import io
import os
import shutil
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'scripts'))
import qcc_tmp

CC = os.path.join(ROOT, 'qcc_zh_v2.exe')

CASES = [
    ('负例 块内裸写 =', 'int g = 0;\nint main(void){ 整 甲 = 1; 结构化并发 { g = 1; } return g + 甲; }\n', False),
    ('负例 块内复合 +=', 'int g = 0;\nint main(void){ 整 甲 = 1; 结构化并发 { g += 1; } return g + 甲; }\n', False),
    ('正例 块内写局部', 'int main(void){ 整 甲 = 1; 结构化并发 { 整 乙 = 2; 乙 = 3; } return 甲; }\n', True),
    ('正例 块外写全局', 'int g = 0;\nint main(void){ 整 甲 = 1; g = 5; return g + 甲; }\n', True),
    ('正例 C 模式', 'int g = 0;\nint main(void){ g = 5; return g; }\n', True),
    # ---- 线程体可达范围（施工单 ③ 第二段；ORD-20261107-133 收口后并入本判据）----
    ('负例 线程体裸写全局', 'int 计 = 0;\nvoid 累计加(void *a) { (void)a; 计 = 计 + 1; }\n'
                          'int main(void){ 结构化并发 失败即停 { 线程 t 跑 累计加, 0; } return 计; }\n', False),
    ('正例 线程体加锁写法', 'int 计 = 0;\nvoid 累计加(void *a) { (void)a; 原子 加(&计, 1); }\n'
                           'int main(void){ 结构化并发 失败即停 { 线程 t 跑 累计加, 0; } return 计; }\n', True),
    ('正例 非线程体写全局', 'int 计 = 0;\nvoid 累计加(void *a) { (void)a; 计 = 计 + 1; }\n'
                           'int main(void){ 计 = 3; return 计; }\n', True),
]


def main():
    if not os.path.exists(CC):
        print('[SKIP] 缺编译器 qcc_zh_v2.exe')
        return 2
    d = qcc_tmp.tmp_dir('qcc_race105')
    bad = []
    try:
        for name, src, expect_ok in CASES:
            p = os.path.join(d, 'r.c')
            io.open(p, 'w', encoding='utf-8', newline='\n').write(src)
            r = subprocess.run([CC, p, '-o', p + '.exe'], cwd=ROOT,
                               capture_output=True, timeout=300)
            out = (r.stdout + r.stderr).decode('utf-8', 'replace')
            hit = 'A3120' in out
            if expect_ok:
                ok = (r.returncode == 0 and not hit)
            else:
                ok = (r.returncode != 0 and hit)
            print('[%s] %-18s rc=%d A3120=%s' % ('PASS' if ok else 'FAIL', name, r.returncode, hit))
            if not ok:
                bad.append(name)
    finally:
        shutil.rmtree(d, ignore_errors=True)
    if bad:
        print('=== ORD-105 ③ 数据竞争检查: FAIL -> %s ===' % bad)
        return 1
    print('=== ORD-105 ③ 数据竞争检查: PASS（3 负例 + 5 正例，含线程体可达范围）===')
    return 0


if __name__ == '__main__':
    sys.exit(main())
