# -*- coding: utf-8 -*-
"""check_mech_real —— **机制类条目的真路径行为判据**（不是「已注册/源码里出现」）。

背景（2026-10-26 goal）：台账里一批 `settled` 的机制条目（泛型约束 / 类型类特化 / 模板元编程 CTFE /
变参模板 / 英文 inline 边界 / compat_prelude 的 regex 类型别名卫生）此前只有「登记表有条目」类旁证。
本脚本对每条机制写**最小程序**，用 shipped 编译器编成可执行并**真跑**，断言运行期输出/错误码：

  ① 泛型约束（正例）：实现接口的类型 → `双倍(21) == 42`；负例：未实现接口的类型必须 **A9006** 拒绝
  ② 类型类特化：同名多版本按接口分派 —— `面积(数)=100`、`面积(矩)=200`、`面积(5)=-1`（缺省）
  ③ 模板元编程 CTFE：`编译期 (求阶乘(6)) == 720`（递归 + 嵌套求值）
  ④ 变参模板：`调四(1,2,3,4) == 10`（尾包逗号连接 + 实参转发）
  ⑤ 英文 inline 边界：`static inline int sq(7) == 49`（不展开但可调用，值正确）
  ⑥ compat_prelude regex 类型别名卫生：自定义 `regmatch_t` 结构体 + `-I compat_prelude.h`，
     成员写后读必须 `3 9 11 13`（若 prelude 又把 regex_t/regmatch_t 定义成对象宏，这里必错）

用法: python -X utf8 scripts/check_mech_real.py [--cc qcc_zh_v1.exe]
退出码: 0 = 六条机制行为全符；1 = 有红；2 = 环境未就绪
"""
import argparse
import io
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PRELUDE = os.path.join(ROOT, 'compat_prelude.h')

GENERIC_POS = '''#包含 <stdio.h>
接口 可加 { 加 }
构 数 { 整 v; }
整 数加(构 数 *a) { 返 a->v; }
实现 可加 于 数 { 加 = 数加 }
泛型 双倍: 可加(x) = (((调用 可加 加(&x)) + (调用 可加 加(&x))));
函数 整 主(空) { 构 数 n; n.v = 21; 格式化 "%d\\n", 双倍(n); 返 0; }
'''

GENERIC_NEG = '''#包含 <stdio.h>
接口 可加 { 加 }
构 无接口 { 整 v; }
泛型 双倍: 可加(x) = (x);
函数 整 主(空) { 构 无接口 z; z.v = 1; 格式化 "%d\\n", 双倍(z); 返 0; }
'''

SPEC = '''#包含 <stdio.h>
接口 可加 { 加 }
接口 可乘 { 乘 }
构 数 { 整 v; }
整 数加(构 数 *a) { 返 a->v; }
实现 可加 于 数 { 加 = 数加 }
构 矩 { 整 w, h; }
整 矩乘(构 矩 *a) { 返 a->w * a->h; }
实现 可乘 于 矩 { 乘 = 矩乘 }
整 面积加(整 值) { 返 值; }
整 面积乘(整 值) { 返 值; }
泛型 面积: 可加(x) = (面积加(100));
泛型 面积: 可乘(x) = (面积乘(200));
泛型 面积(x) = (-1);
函数 整 主(空) {
    构 数 n; n.v = 7; 整 a = 面积(n);
    构 矩 m; m.w = 3; m.h = 4; 整 b = 面积(m);
    整 c = 面积(5);
    格式化 "%d %d %d\\n", a, b, c;
    返 (a == 100 && b == 200 && c == -1) ? 0 : 1;
}
'''

CTFE = '''#包含 <stdio.h>
整 求阶乘(整 n) { 若 (n <= 1) 返 1; 返 n * 求阶乘(n - 1); }
函数 整 主(空) { 整 a = 编译期 (求阶乘(6)); 格式化 "%d\\n", a; 返 0; }
'''

VARARG = '''#包含 <stdio.h>
整 总和(整 a, 整 b, 整 c, 整 d) { 返 a + b + c + d; }
泛型 调四(首, 尾...) = (总和(首, 尾));
函数 整 主(空) { 整 r = 调四(1, 2, 3, 4); 格式化 "%d\\n", r; 返 0; }
'''

INLINE_EN = '''#include <stdio.h>
static inline int sq(int x){ return x * x; }
int main(void){ printf("%d\\n", sq(7)); return 0; }
'''

REGEX_HYGIENE = '''#include <stdio.h>
typedef struct { int rm_so; int rm_eo; } regmatch_t;
int main(void){
    regmatch_t m[2];
    m[0].rm_so = 3; m[0].rm_eo = 9;
    m[1].rm_so = 11; m[1].rm_eo = 13;
    printf("%d %d %d %d\\n", m[0].rm_so, m[0].rm_eo, m[1].rm_so, m[1].rm_eo);
    return 0;
}
'''

FLOAT_H = '''#include <stdio.h>
#include <float.h>
int main(void){
    printf("%d %d %d %d\\n", DBL_MANT_DIG, FLT_MANT_DIG, DBL_DIG, FLT_RADIX);
    printf("%d %d\\n", (int)(DBL_EPSILON * 1e16), (int)(FLT_EPSILON * 1e7));
    printf("%d\\n", DBL_MAX > 1e308);
    return 0;
}
'''


def build_run(cc, text, tag, w, extra=None):
    src = os.path.join(w, tag + '.c')
    io.open(src, 'w', encoding='utf-8', newline='\n').write(text)
    exe = os.path.join(w, tag + '.exe')
    cmd = [cc, src, '-o', exe] + (extra or [])
    r = subprocess.run(cmd, capture_output=True, timeout=300)
    msg = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')
    if r.returncode != 0 or not os.path.exists(exe):
        return r.returncode, '', msg
    p = subprocess.run([exe], capture_output=True, timeout=120)
    return r.returncode, (p.stdout or b'').decode('utf-8', 'replace').strip(), msg


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(ROOT, 'qcc_zh_v1.exe'))
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    if not os.path.exists(cc) or not os.path.exists(PRELUDE):
        print('[FAIL] 环境未就绪: cc 或 compat_prelude.h 缺失（本判据未执行，不判绿）')
        return 2
    w = tempfile.mkdtemp(prefix='qcc_mech_')
    bad = []

    cases = [
        ('泛型约束-正例', GENERIC_POS, '42', None),
        ('类型类特化', SPEC, '100 200 -1', None),
        ('模板元编程CTFE', CTFE, '720', None),
        ('变参模板', VARARG, '10', None),
        ('英文inline边界', INLINE_EN, '49', None),
        ('prelude-regex卫生', REGEX_HYGIENE, '3 9 11 13', ['-I', PRELUDE]),
        ('内建float.h', FLOAT_H, '53 24 15 2\n2 1\n1', None),
    ]
    for tag, text, want, extra in cases:
        rc, out, msg = build_run(cc, text, 'm_%s' % tag.replace('-', '_'), w, extra)
        ok = (rc == 0 and out == want)
        print('[%-16s] 编译 rc=%d 运行 out=%r（期望 %r）⟹ %s' % (tag, rc, out, want, 'OK' if ok else 'RED'))
        if not ok:
            print('    ' + msg.strip().split('\n')[-1][:180])
            bad.append(tag)

    # 负例：未实现接口的类型必须被 A9006 拒（不是静默放行，也不是别的码）
    rc, out, msg = build_run(cc, GENERIC_NEG, 'm_generic_neg', w)
    ok = (rc != 0 and 'A9006' in msg)
    print('[%-16s] 编译 rc=%d A9006=%s ⟹ %s' % ('泛型约束-负例', rc, 'A9006' in msg, 'OK' if ok else 'RED'))
    if not ok:
        print('    ' + msg.strip().split('\n')[-1][:180])
        bad.append('泛型约束-负例')

    print('=== 机制真路径: %d 条，红 %d 条 ⟹ %s ==='
          % (len(cases) + 1, len(bad), 'PASS' if not bad else 'FAIL -> ' + ', '.join(bad)))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
