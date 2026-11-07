#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_sc105 —— ORD-20261104-105 ②：结构化并发两态（失败即停 / 失败各自）。

大哥 2026-11-06 裁定：语法 `结构化并发 失败即停 { … }` / `失败各自 { … }`；
「失败 = 子线程返回非 0」；`失败即停` 失败后置块级取消，`失败各自` 不置；取消经 `检查取消()` 暴露。

五个用例（真跑编译器 + 真跑产物）：
  A  `结构化并发 失败即停 { }`            ⟹ rc=0（空块语法被接受）
  B  `结构化并发 失败各自 { }`            ⟹ rc=0
  C  普通 `结构化并发 { }`                ⟹ rc=0（旧行为不变）
  D  失败即停 + 子线程返回 5              ⟹ 运行后 `__qcc_sc_cancelled()` = 1
  E  失败各自 + 子线程返回 5              ⟹ 运行后 `__qcc_sc_cancelled()` = 0

用法: python scripts/check_sc105.py
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

DECL = 'int __qcc_sc_cancelled(void);\nint __qcc_sc_failed(void);\n'
# ⚠️ 线程体**不许裸写全局**（那是 A3120 的范围，见 check_race105.py）——本判据只测两态语义，
#    所以 bad 只返回值、长任 用 `原子 加` 记观测位。
FAILTHREAD = 'int bad(void *a) { (void)a; return 5; }\n'

CASES = [
    ('A 空块 失败即停', '#include <stdio.h>\nint main(void){ 结构化并发 失败即停 { } return 0; }\n', None),
    ('B 空块 失败各自', '#include <stdio.h>\nint main(void){ 结构化并发 失败各自 { } return 0; }\n', None),
    ('C 普通块不变', '#include <stdio.h>\nint main(void){ 结构化并发 { } return 0; }\n', None),
    ('D 失败即停 置取消', '#include <stdio.h>\n' + DECL + FAILTHREAD +
     'int main(void){ 结构化并发 失败即停 { 线程 t 跑 bad, 0; } return __qcc_sc_cancelled(); }\n', 1),
    ('E 失败各自 不置', '#include <stdio.h>\n' + DECL + FAILTHREAD +
     'int main(void){ 结构化并发 失败各自 { 线程 t 跑 bad, 0; } return __qcc_sc_cancelled(); }\n', 0),
    # F/G：**失败码本身**（skeptic #4：原判据只验取消位，没验「块以第一个失败码结束」）
    ('F 失败即停 记失败码', '#include <stdio.h>\n' + DECL + FAILTHREAD +
     'int main(void){ 结构化并发 失败即停 { 线程 t 跑 bad, 0; } return __qcc_sc_failed(); }\n', 5),
    ('G 失败各自 也记失败码', '#include <stdio.h>\n' + DECL + FAILTHREAD +
     'int main(void){ 结构化并发 失败各自 { 线程 t 跑 bad, 0; } return __qcc_sc_failed(); }\n', 5),
    # H：成功线程不置失败码（负例：不能「永远 5」）
    ('H 全成功 失败码为 0', '#include <stdio.h>\n' + DECL +
     'int ok(void *a) { (void)a; return 0; }\n'
     'int main(void){ 结构化并发 失败即停 { 线程 t 跑 ok, 0; } return __qcc_sc_failed(); }\n', 0),
    # I/J：**兄弟线程被取消**（施工单 §五 ② 用例 1）——一个线程失败，另一个正在跑的线程
    #      轮询 `__qcc_sc_cancelled()` 并因此提前退出（置 观=1）；失败各自 ⟹ 不取消（观=0）
    ('I 失败即停 兄弟被取消', '#include <stdio.h>\n' + DECL +
     'int 观 = 0;\n'
     'int 长任(void *a) { (void)a; 整 i = 0; 循环 (i < 20000000) { 若 (__qcc_sc_cancelled()) { 原子 加(&观, 1); 返 0; } i = i + 1; } 返 0; }\n' +
     FAILTHREAD +
     'int main(void){ 结构化并发 失败即停 { 线程 t1 跑 bad, 0; 线程 t2 跑 长任, 0; } return 观; }\n', 1),
    ('J 失败各自 兄弟不取消', '#include <stdio.h>\n' + DECL +
     'int 观 = 0;\n'
     'int 长任(void *a) { (void)a; 整 i = 0; 循环 (i < 2000000) { 若 (__qcc_sc_cancelled()) { 原子 加(&观, 1); 返 0; } i = i + 1; } 返 0; }\n' +
     FAILTHREAD +
     'int main(void){ 结构化并发 失败各自 { 线程 t1 跑 bad, 0; 线程 t2 跑 长任, 0; } return 观; }\n', 0),
]


def main():
    if not os.path.exists(CC):
        print('[SKIP] 缺编译器 qcc_zh_v2.exe')
        return 2
    d = qcc_tmp.tmp_dir('qcc_sc105')
    bad = []
    try:
        for name, src, want_rc in CASES:
            p = os.path.join(d, 's.c')
            io.open(p, 'w', encoding='utf-8', newline='\n').write(src)
            r = subprocess.run([CC, p, '-o', p + '.exe'], cwd=ROOT,
                               capture_output=True, timeout=300)
            if r.returncode != 0:
                print('[FAIL] %-18s 编译 rc=%d' % (name, r.returncode))
                bad.append(name)
                continue
            q = subprocess.run([p + '.exe'], capture_output=True, timeout=120)
            ok = (q.returncode == 0) if want_rc is None else (q.returncode == want_rc)
            print('[%s] %-18s 运行 rc=%d%s' % ('PASS' if ok else 'FAIL', name,
                                               q.returncode,
                                               '' if want_rc is None else ' (期望 %d)' % want_rc))
            if not ok:
                bad.append(name)
    finally:
        shutil.rmtree(d, ignore_errors=True)
    if bad:
        print('=== ORD-105 ② 结构化并发两态: FAIL -> %s ===' % bad)
        return 1
    print('=== ORD-105 ② 结构化并发两态: PASS（A–J：语法 + 取消位 + 失败码 + 成功负例 + 兄弟线程被取消）===')
    return 0


if __name__ == '__main__':
    sys.exit(main())
