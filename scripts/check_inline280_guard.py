# -*- coding: utf-8 -*-
"""#280 内联展开的**语义守卫**判据（2026-10-17 · skeptic 打回后新增）。

三个真实输入形态，必须与「同源非内联对照」**逐值一致**（不是看是否展开）：

  a 自递归体   `内联 函数 整 阶(整 n){返 n<=1?1:n*阶(n-1);}` ⟹ 阶(5)=120
  b 无名形参   `内联 函数 整 g(整){返 (整)1;}`                ⟹ g(5)=1
  c 非返体     `内联 函数 空 置一(整 x){x=1;}`                  ⟹ 置一(v) 不改调用者 v（v=5）

判据：内联版 stdout == 非内联对照版 stdout（逐字），且 == 期望值。
「展开」不是本判据的目标——**语义正确**才是（宁可退回普通调用）。

用法: python scripts/check_inline280_guard.py [--cc <exe>]
退出码: 0 全过 ; 1 有错值。
"""
import os
import subprocess
import sys
sys.stdout.reconfigure(encoding='utf-8', errors='replace')

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REPO)
CC = 'qcc_zh_v2.exe'
if '--cc' in sys.argv:
    CC = sys.argv[sys.argv.index('--cc') + 1]
BASE = os.path.join(os.environ.get('TEMP', '.'), '_check_inline280_guard')
os.makedirs(BASE, exist_ok=True)

CASES = {
    'rec': (
        '内联 函数 整 阶(整 n) { 返 n <= 1 ? 1 : n * 阶(n - 1); }\n'
        '函数 整 主(空) { 格式化 "REC %d\\n", 阶(5); 返 0; }\n',
        '函数 整 阶(整 n) { 返 n <= 1 ? 1 : n * 阶(n - 1); }\n'
        '函数 整 主(空) { 格式化 "REC %d\\n", 阶(5); 返 0; }\n',
        'REC 120'),
    'anon': (
        '内联 函数 整 g(整) { 返 (整)1; }\n'
        '函数 整 主(空) { 格式化 "ANON %d\\n", g(5); 返 0; }\n',
        '函数 整 g(整) { 返 (整)1; }\n'
        '函数 整 主(空) { 格式化 "ANON %d\\n", g(5); 返 0; }\n',
        'ANON 1'),
    'voidbody': (
        '内联 函数 空 置一(整 x) { x = 1; }\n'
        '函数 整 主(空) { 整 v = 5; 置一(v); 格式化 "VB %d\\n", v; 返 0; }\n',
        '函数 空 置一(整 x) { x = 1; }\n'
        '函数 整 主(空) { 整 v = 5; 置一(v); 格式化 "VB %d\\n", v; 返 0; }\n',
        'VB 5'),
}


def run_case(name, src, tag):
    p = os.path.join(BASE, name + '_' + tag + '.c')
    open(p, 'w', encoding='utf-8').write(src)
    exe = os.path.join(BASE, name + '_' + tag + '.exe')
    r = subprocess.run([CC, p, '-o', exe], capture_output=True, timeout=180)
    if r.returncode != 0:
        return None, ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')[:200]
    r2 = subprocess.run([exe], capture_output=True, timeout=60)
    return ((r2.stdout or b'') + (r2.stderr or b'')).decode('utf-8', 'replace').strip(), ''


def main():
    fails = []
    for name, (inline_src, plain_src, want) in CASES.items():
        got_i, err_i = run_case(name, inline_src, 'inl')
        got_p, err_p = run_case(name, plain_src, 'plain')
        ok = (got_i == got_p == want)
        print('%-9s inline=%-14r plain=%-14r want=%-10r %s %s' % (
            name, got_i, got_p, want, 'OK' if ok else 'MISMATCH',
            ('| ' + (err_i or err_p)) if (err_i or err_p) else ''))
        if not ok:
            fails.append(name)
    if fails:
        print('=== #280 守卫判据: FAIL (%s) ===' % ','.join(fails))
        return 1
    print('=== #280 守卫判据: PASS（递归/无名形参/非返体 均与对照一致）===')
    return 0


if __name__ == '__main__':
    sys.exit(main())
