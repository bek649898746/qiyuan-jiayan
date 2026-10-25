# -*- coding: utf-8 -*-
"""check_missing_semicolon —— **缺分号/缺等号 必须报错，不许静默出产物**（台账 G-D49-SEMI 判据）。

病灶（2026-10-24 现场）：`int main(void){return 0}`（缺 `;`）/ `int g 1;`（缺 `=`）等 5 个场景
qcc 全 rc=0 并产出 276KB exe；gcc 全部报 `expected ';'`。静默吞 token 属「结果不确定」。

判据（5 例，任一「rc==0」即红）：
  ① 语句后缺 `;`（`return 0` 后直接 `}`）
  ② 声明后缺 `;`（`int a = 1` 后接下一语句）
  ③ 两条语句之间缺 `;`
  ④ 初始化缺 `=`（`int g 1;`）
  ⑤ 函数原型后缺 `;`

用法: python scripts/check_missing_semicolon.py [--cc qcc_zh_seed.exe]
退出码: 0 = 5 例全部报错（缺陷已修）；1 = 至少 1 例静默通过（缺陷仍在）
"""
import argparse
import io
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

CASES = [
    ('stmt-no-semi', 'int main(void){ return 0 }\n'),
    ('decl-no-semi', 'int main(void){ int a = 1\n return a; }\n'),
    ('two-stmt-no-semi', 'int main(void){ int a = 1 int b = 2; return a + b; }\n'),
    ('no-equals', 'int g 1;\nint main(void){ return g; }\n'),
    ('proto-no-semi', 'int f(void)\nint main(void){ return f(); }\nint f(void){ return 7; }\n'),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(ROOT, 'qcc_zh_seed.exe'))
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    if not os.path.exists(cc):
        # 2026-10-24（判据方打回）: 缺主体不得当绿 —— rc=2 = 环境未就绪，不是 PASS
        print('[FAIL] 环境未就绪: 找不到编译器 %s（本判据未执行，不判绿）' % cc)
        return 2
    w = tempfile.mkdtemp(prefix='qcc_semi_')
    bad = []
    for name, text in CASES:
        src = os.path.join(w, name + '.c')
        io.open(src, 'w', encoding='utf-8', newline='\n').write(text)
        out = os.path.join(w, name + '.exe')
        r = subprocess.run([cc, src, '-o', out], capture_output=True, timeout=180)
        msg = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace').strip()
        silent = r.returncode == 0
        print('[%-18s] rc=%d %s%s' % (name, r.returncode, 'SILENT' if silent else 'REJECT',
                                      '' if silent else ' | ' + msg.split('\n')[-1][:100]))
        if silent:
            bad.append(name)
    print('=== 缺分号/缺等号 静默检测: %s（静默 %d/%d）===' % ('PASS' if not bad else 'FAIL', len(bad), len(CASES)))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
