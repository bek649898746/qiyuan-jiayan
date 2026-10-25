# -*- coding: utf-8 -*-
"""check_objmode_diag —— **`-c` 模式必须在编译期报 A1001/A9007，不许推迟到链接期**（G-D48-COBJDIAG）。

病灶（2026-10-24 现场）：`return cuont;` 用 `-c` 编译 rc=0，obj 里留未定义符号，
jyld 才报 `undefined symbol` ⟹ file:line:col 丢失、错误暴露过晚；gcc -c 立即报 undeclared。

判据（4 例）：
  ① 未声明标识符 → `-c` 必须 rc≠0 且输出含 A1001
  ② 调用参数个数不符 → `-c` 必须 rc≠0
  ③ 未知类型名 → `-c` 必须 rc≠0
  ④ **反例（必须保持绿）**：`extern` 跨 TU 声明 + 使用 → `-c` rc=0（真跨 TU 不许误拦）

用法: python scripts/check_objmode_diag.py [--cc qcc_zh_seed.exe]
退出码: 0 = 4 例全符；1 = 有例不符（缺陷仍在）
"""
import argparse
import io
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

NEG = [
    ('undeclared', 'int main(void){ return cuont; }\n', 'A1001'),
    ('arity', 'int f(int a);\nint main(void){ return f(); }\n', None),
    ('unknown-type', 'nosuchtype x;\nint main(void){ return 0; }\n', None),
]
POS = ('extern-cross-tu', 'extern int g;\nint main(void){ return g; }\n')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(ROOT, 'qcc_zh_seed.exe'))
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    if not os.path.exists(cc):
        # 2026-10-24（判据方打回）: 缺主体不得当绿 —— rc=2 = 环境未就绪，不是 PASS
        print('[FAIL] 环境未就绪: 找不到编译器 %s（本判据未执行，不判绿）' % cc)
        return 2
    w = tempfile.mkdtemp(prefix='qcc_objdiag_')
    bad = []
    for name, text, want in NEG:
        src = os.path.join(w, name + '.c')
        io.open(src, 'w', encoding='utf-8', newline='\n').write(text)
        obj = os.path.join(w, name + '.o')
        r = subprocess.run([cc, '-c', src, '-o', obj], capture_output=True, timeout=180)
        msg = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')
        ok = r.returncode != 0 and (want is None or want in msg)
        print('[%-14s] rc=%d %s' % (name, r.returncode, 'OK' if ok else 'DEFERRED'))
        if not ok:
            bad.append(name)
    name, text = POS
    src = os.path.join(w, name + '.c')
    io.open(src, 'w', encoding='utf-8', newline='\n').write(text)
    r = subprocess.run([cc, '-c', src, '-o', os.path.join(w, name + '.o')],
                       capture_output=True, timeout=180)
    ok = r.returncode == 0
    print('[%-14s] rc=%d %s' % (name, r.returncode, 'OK' if ok else 'FALSE-POSITIVE'))
    if not ok:
        bad.append(name)
    print('=== -c 编译期诊断: %s ===' % ('PASS' if not bad else 'FAIL'))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
