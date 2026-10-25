#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_fnstatic_cap —— 静态函数名表「无固定上限」判据（2026-10-23 虾米）。

病灶（修前）：`净深一[1024][64]` 固定数组 ⟹ 第 1025 个 `static` 函数名即
`A5001 静态函数表 fn_static 满`；1024→8192 只是扩容，第 8193 个仍硬失败。

修法：`字 **净深一` 指针表 + `净深保` 按需倍增（无固定上限，每名单独 64B）。

判据：**N=4000** 个 distinct `static` 函数必须 rc=0（旧表 1024 会当场拒），
且编译日志里不得出现 `静态函数表 / fn_static` 的 A5001。

⚠️ 上限为何停在 4000：`参数映射表 度伍[4096]` 仍是固定数组（全局去重、跨函数只增），
任何 >4096 函数的载体都会先撞它。把它改成动态（R804）试过并**已回退**——
静态首段在 objmode 自链接后运行期 0xC0000005（`check_objmode_self.py` ④），证据在
`grok-build/_evidence/20261023_fnstatic_dyn/`。故本条只断言「fn_static 不封顶」，
fn_param 作为独立债务记录，不在此处冒充。

用法: python scripts/check_fnstatic_cap.py [--cc qcc_zh_seed.exe] [--n 4000]
退出: 0 通过 / 1 有红 / 2 缺件
"""
import argparse
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_seed.exe')
    ap.add_argument('--n', type=int, default=4000)
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    if not os.path.exists(cc):
        print('[SKIP] 编译器不存在: %s' % cc); return 2

    tmp = tempfile.mkdtemp(prefix='fnstatic_')
    src = os.path.join(tmp, 'cap.c')
    obj = os.path.join(tmp, 'cap.o')
    with open(src, 'w', encoding='utf-8') as f:
        for i in range(a.n):
            f.write('static int fs%d(void) { return %d; }\n' % (i, i % 7))
        f.write('int main(void) { return fs0() + fs%d(); }\n' % (a.n - 1))

    p = subprocess.run([cc, '-c', src, '-o', obj], capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=900)
    out = ((p.stdout or '') + (p.stderr or '')).strip()
    first = out.split('\n')[0][:150] if out else ''
    ok = (p.returncode == 0) and ('静态函数表' not in out) and ('fn_static' not in out)
    if ok:
        sz = os.path.getsize(obj) / 1048576.0 if os.path.exists(obj) else 0
        print('[fn_static %d] PASS（rc=0，%.1f MB .o；旧表 1024 会拒）' % (a.n, sz))
    else:
        print('[fn_static %d] FAIL rc=%d %s' % (a.n, p.returncode, first))
    print('=== 静态函数表 无固定上限 判据: %s ===' % ('PASS' if ok else 'RED'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
