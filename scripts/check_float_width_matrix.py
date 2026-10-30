# -*- coding: utf-8 -*-
"""check_float_width_matrix —— 浮性×宽度 的**探针矩阵**（criterion 2）。

为什么这样设计（2026-10-29 实测修正）：
  最初想用 `QCC_DBG_FW` 断言「凡按 double 消费的节点必被 `发浮取` 装载过」（C8n ⊆ L），
  但实测发现 **`发浮取` 不是唯一装载出口** —— 变量/成员/取址各有内联浮取，于是该不变式
  既有假阳（前插运行库自身的节点）又有假阴（内联路径的节点）。故改为**逐站点位模式证据**：
  每一例的**输出**本身就钉住了装载宽度（4 字节 float 的位模式 / 8 字节 double / 整数装载），
  这与定这条判据的初衷（R899/R900 的装载-消费分裂）等价，且不依赖实现细节。

矩阵（10 例，计划要求 ≥9）：每行给出「站点 → 期望装载宽度 → 位模式证据 → 结论」。
用法: python scripts/check_float_width_matrix.py [--cc qcc_zh_v1.exe]
退出: 0 = 10 例全绿；1 = 有红
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

# (站点, 锚文件或 None, 期望输出, 装载宽度, 该宽度由什么证据钉住)
CASES = [
    ('typeof 双精度链', 'feat_zh_typeof_ptr_ok.c', 'P0=1.50 P1=2.50 PP=2.50', 8,
     'pp[0][1] 取到 2.50 ⟹ 8B double 装载（若按 4B 读会得 0.00）'),
    ('ptrptr 链', 'feat_zh_ptrptr_chain_ok.c', 'PASS', 8,
     '局部二级 `**q` 得 1.5/2.5 ⟹ 解引用链按 double 装载（曾 rc=6）'),
    ('单精度·局部', 'feat_zh_float4.c', 'PASS 2.5 3.5', 4,
     '`*(无整*)&a == 0x3F800000` ⟹ 4B 存储/装载（8B 会改写邻字节）'),
    ('单精度·字段', 'feat_zh_float_field.c', 'PASS', 4, '字段位模式断言 ⟹ 4B'),
    ('单精度·函数返回', 'feat_zh_float_fn.c', 'PASS', 4, '数组/返回值位模式 ⟹ 4B'),
    ('逗号表达式浮点', 'feat_zh_comma_expr_float_ok.c', 'CE 2.50 3.50 2.50 4.50', 8,
     '`(x, 2.5)` 作实参得 4.50 ⟹ 值=右操作数且按 double 消费'),
    ('取址基读', 'feat_zh_deref_addr_width_ok.c', 'DAW 3.50 1234567890123 4.50 5.50 3.50', 8,
     '`*(&v)` 得 3.50、`*(&w)` 得完整 64 位 ⟹ 8B 装载（曾把地址当值）'),
    ('转型基读', 'feat_zh_float_ptr_cast_ok.c', 'FPW 3.50 3.50 1.50', 4,
     '`*(单精度*)&g` 得 3.50 ⟹ 4B+升 double（曾得 1080033280.00）'),
    ('泛型实例化', None, 'MONOEXPR 7 7.5', 8,
     '`取大[双]` 作变参得 7.5 ⟹ 实例化调用按 double 装载（曾得 0.0）'),
    ('转型到整型指针', None, 'CAST 3F800000', 4,
     '`*(无整*)&a` 得位模式 ⟹ **整数**装载（若按 double 会得 00000000/A9002）'),
]

GEN = '''#包含 <stdio.h>
泛型函数 取大(T) = 函数 T 取大(T x, T y) { 返 ((x) > (y) ? (x) : (y)); }
实例 取大(整);
实例 取大(双);
函数 整 主(空) { 格式化 "MONOEXPR %d %.1f\\n", 取大[整](3, 7), 取大[双](3.5, 7.5); 返 0; }
'''

CASTINT = '''#include <stdio.h>
int main(void) {
    float a; a = 1.0f;
    int v = *(无整*)&a;
    printf("CAST %08X\\n", v);
    return 0;
}
'''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    w = tempfile.mkdtemp(prefix='qcc_fw_matrix_')
    env = dict(os.environ, TEMP=r'D:\qcc_tmp', TMP=r'D:\qcc_tmp')
    gen = os.path.join(w, 'gen.c')
    io.open(gen, 'w', encoding='utf-8', newline='\n').write(GEN)
    castint = os.path.join(w, 'castint.c')
    io.open(castint, 'w', encoding='utf-8', newline='\n').write(CASTINT)

    print('=== 浮性×宽度 探针矩阵（位模式证据口径）===')
    print('cc = %s' % cc)
    print('%-24s %-4s %-6s %s' % ('站点', '宽度', '结论', '证据'))
    fails = []
    for name, fn, want, width, why in CASES:
        src = os.path.join(REPO, 'scripts', fn) if fn else (gen if '泛型' in name else castint)
        exe = os.path.join(w, re.sub(r'\W+', '_', name) + '.exe')
        r = subprocess.run([cc, src, '-o', exe], capture_output=True, text=True, encoding='utf-8',
                           errors='replace', timeout=300, env=env)
        if r.returncode != 0:
            print('%-24s %-4d 编译失败  %s' % (name, width, ((r.stdout or '') + (r.stderr or '')).strip().split('\n')[0][:70]))
            fails.append(name + ' 编不过')
            continue
        q = subprocess.run([exe], capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=60)
        out = (q.stdout or '').strip()
        ok = (q.returncode == 0) and (want in out)
        print('%-24s %-4d %-6s %s' % (name, width, 'OK' if ok else 'FAIL', why[:60]))
        if not ok:
            fails.append('%s 输出 %r 不含 %r' % (name, out[:40], want))
    print('=== 矩阵: %s ===' % ('PASS（10/10 宽度一致）' if not fails else 'FAIL %s' % fails[:3]))
    return 0 if not fails else 1


if __name__ == '__main__':
    sys.exit(main())
