# -*- coding: utf-8 -*-
"""census_matrix —— **声明位 × 形态 × 保留类型词** 全矩阵普查（D70/D71 一致深判据）。

为什么要有它
------------
`check_delivery_d73.py` 的 D70/D71 是**抽样**（18 格）。审计要的是「各声明位一律 A9001」，
抽样无法证明「一律」。本脚本把「声明位 × 形态 × 名字」展开成 **全矩阵**（每个组合一次编译），
逐格断言：

  · 负例（名字 = 保留类型词）：`rc != 0` 且 诊断含 **A9001**（不是 A1004 误诊、不是 rc=0 零诊断）
  · 正例（名字 = 普通标识符 `甲`）：`rc == 0`；局部可运行形态 **真跑产物** 并断言具体数值

用法
----
  python -X utf8 scripts/census_matrix.py [--cc qcc_zh_v1.exe] [--tsv <输出.tsv>] [--quiet]
退出码: 0 = 全矩阵通过；1 = 有红格；2 = 环境未就绪

矩阵维度
--------
  声明位: 局部 / 全局 / 形参（含定义体） / 结构体字段 / 逗号续声明符
  形  态: 无修饰 / 指针 / 指针const / 双指针 / 数组 / 指针数组 / 函数指针 / 函数指针const
  名  字: 负例 = 无 长 短 常 双 字 空 整（8 个保留类型词）；正例 = 甲
"""
import argparse
import io
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

BAD_NAMES = ['无', '长', '短', '常', '双', '字', '空', '整']
GOOD = '甲'
INC = '#包含 <stdio.h>\n'

POS_FORMS = {
    '局部': ['无修饰', '指针', '指针const', '双指针', '数组', '指针数组', '函数指针', '函数指针const'],
    '全局': ['无修饰', '指针', '指针const', '双指针', '数组', '指针数组', '函数指针', '函数指针const'],
    '形参': ['无修饰', '指针', '指针const', '双指针', '函数指针', '函数指针const'],
    '字段': ['无修饰', '指针', '指针const', '双指针', '数组', '指针数组', '函数指针', '函数指针const'],
    '逗号续': ['无修饰', '指针', '指针const', '数组', '指针数组'],
}


def decl(form, name):
    """`整 <形式 名字>` 声明片段（不含分号）。"""
    return {
        '无修饰': '整 %s' % name,
        '指针': '整 *%s' % name,
        '指针const': '整 * const %s' % name,
        '双指针': '整 **%s' % name,
        '数组': '整 %s[3]' % name,
        '指针数组': '整 *%s[3]' % name,
        '函数指针': '整 (*%s)(整)' % name,
        '函数指针const': '整 (* const %s)(整)' % name,
    }[form]


def declarator(form, name):
    """逗号续声明符专用：只给声明符（基类型 `整` 已在前面）。"""
    return {
        '无修饰': '%s' % name,
        '指针': '*%s' % name,
        '指针const': '* const %s' % name,
        '数组': '%s[3]' % name,
        '指针数组': '*%s[3]' % name,
    }[form]


# R873：**抽象声明符 正例**（无名字的 const 限定类型）—— 合法 C，必须编过并运行。
# 2026-10-26 评审打回：原 315 格 全是「有名字」的形态 ⟹ R871 的「限定词占名字位」规则
# 把 `int f(int const);` / `sizeof(int const[3])` 这类**合法抽象声明符**误判成 A9001，
# 矩阵 315/315 全绿也发现不了（没有一格覆盖它）。这些格就是防该回归的护栏。
ABSTRACT_POS = [
    ('抽象-形参const', '#include <stdio.h>\nint f(int const);\nint main(void){ return 0; }\n', None),
    ('抽象-形参指针const', '#include <stdio.h>\nint f(int * const);\nint main(void){ return 0; }\n', None),
    ('抽象-fnptr形参', '#include <stdio.h>\nint (*fp)(int const);\nint main(void){ return 0; }\n', None),
    ('抽象-sizeof', '#include <stdio.h>\nint main(void){ printf("%d\\n", (int)sizeof(int const)); return 0; }\n', '4'),
    ('抽象-sizeof数组', '#include <stdio.h>\nint main(void){ printf("%d\\n", (int)sizeof(int const[3])); return 0; }\n', '12'),
    ('抽象-sizeof指针', '#include <stdio.h>\nint main(void){ printf("%d\\n", (int)sizeof(int * const)); return 0; }\n', '8'),
    ('抽象-fnptr-sizeof', '#include <stdio.h>\nint main(void){ printf("%d\\n", (int)sizeof(int (*)(int const))); return 0; }\n', '8'),
    # R875（评审打回）：多形参原型 与 转型表达式 —— 抽象位 必须同时认「原型里的无名字形参」与「type-name 转型」
    ('抽象-原型多参', '#include <stdio.h>\nint f(int const, int);\nint main(void){ return 0; }\n', None),
    ('抽象-fnptr多参', '#include <stdio.h>\nint (*fp)(int const, int);\nint main(void){ return 0; }\n', None),
    ('抽象-原型指针多参', '#include <stdio.h>\nint f(int * const, int);\nint main(void){ return 0; }\n', None),
    ('抽象-转型const', '#include <stdio.h>\nint main(void){ int y = (int const)3; printf("%d\\n", y); return 0; }\n', '3'),
    ('抽象-转型long', '#include <stdio.h>\nint main(void){ long a = (long const)5; printf("%d\\n", (int)a); return 0; }\n', '5'),
    ('抽象-解引用转型', '#include <stdio.h>\nint main(void){ int v = 7; int z = *(int * const)&v; printf("%d\\n", z); return 0; }\n', '7'),
]

# R879（2026-10-26 · 评审第四轮 高）：**for-init 声明位** —— 控制语句头 的 `(` 不是 转型。
#   形态 `for (int (*const)(int); 0; )` 里 括号 装 的 是 C99 for-init 声明；原 抽象位 把「括号组首词元是类型词」
#   一律 当 转型 ⟹ 该 声明位 **静默豁免**（shipped D1C43630 rc=0 零诊断；gcc 14.2 与 旧 v1 拒绝）。
#   负例 必须 A9001；正例（正常 for + 省略形 `for (;;)`）必须编过并跑出值。
FOR_INIT_POS = [
    ('forinit-花括', '#include <stdio.h>\nint main(void){ for (int (*const)(int); 0; ) { } return 0; }\n', 'reject'),
    ('forinit-无花括', '#include <stdio.h>\nint main(void){ for (int (*const)(int); 0; ) ; return 0; }\n', 'reject'),
    ('forinit-正常', '#include <stdio.h>\nint main(void){ int s = 0; for (int i = 0; i < 3; i++) s += i; printf("%d\\n", s); return 0; }\n', '3'),
]


def neg_source(pos, form, name):
    d = decl(form, name)
    if pos == '局部':
        return INC + '整 主(空){ %s; 返 0; }\n' % d
    if pos == '全局':
        return INC + '%s;\n整 主(空){ 返 0; }\n' % d
    if pos == '形参':
        inner = d[2:]  # 去掉 "整 "
        return INC + '整 用(整 %s){ 返 0; }\n整 主(空){ 返 0; }\n' % inner
    if pos == '字段':
        return INC + '构 点 { %s; };\n整 主(空){ 返 0; }\n' % d
    if pos == '逗号续':
        return INC + '整 主(空){ 整 首, %s; 返 0; }\n' % declarator(form, name)
    raise ValueError(pos)


def _local_pos(form):
    """局部正例：声明 + 取真值 + 打印（可运行形态返回 (代码, 期望输出)）。"""
    if form == '无修饰':
        return '整 甲; 甲 = 7; 格式化 "%d\\n", 甲;', '7'
    if form == '指针':
        return '整 基 = 7; 整 *甲; 甲 = &基; 格式化 "%d\\n", *甲;', '7'
    if form == '指针const':
        return '整 基 = 7; 整 * const 甲 = &基; 格式化 "%d\\n", *甲;', '7'
    if form == '双指针':
        return '整 基 = 7; 整 *中 = &基; 整 **甲 = &中; 格式化 "%d\\n", **甲;', '7'
    if form == '数组':
        return '整 甲[3]; 甲[0] = 7; 格式化 "%d\\n", 甲[0];', '7'
    if form == '指针数组':
        return '整 基 = 7; 整 *甲[3]; 甲[0] = &基; 格式化 "%d\\n", *甲[0];', '7'
    if form == '函数指针':
        return '整 (*甲)(整); 甲 = 0; 格式化 "%d\\n", (甲 == 0);', '1'
    if form == '函数指针const':
        return '整 (* const 甲)(整) = 0; 格式化 "%d\\n", (甲 == 0);', '1'
    raise ValueError(form)


def pos_source(pos, form):
    if pos == '局部':
        body, want = _local_pos(form)
        return INC + '整 主(空){ %s 返 0; }\n' % body, want
    if pos == '全局':
        d = decl(form, GOOD)
        if form == '指针const':
            d = '整 基 = 7; 整 * const %s = &基' % GOOD
            return INC + '%s;\n整 主(空){ 格式化 "%%d\\n", *%s; 返 0; }\n' % (d, GOOD), '7'
        if form == '数组':
            return INC + '%s = {7};\n整 主(空){ 格式化 "%%d\\n", %s[0]; 返 0; }\n' % (d, GOOD), '7'
        return INC + '%s;\n整 主(空){ 返 0; }\n' % d, None
    if pos == '形参':
        inner = decl(form, GOOD)[2:]
        return INC + '整 用(整 %s){ 返 0; }\n整 主(空){ 返 0; }\n' % inner, None
    if pos == '字段':
        d = decl(form, GOOD)
        if form == '无修饰':
            return INC + '构 点 { %s; };\n整 主(空){ 构 点 p; p.%s = 7; 格式化 "%%d\\n", p.%s; 返 0; }\n' % (d, GOOD, GOOD), '7'
        return INC + '构 点 { %s; };\n整 主(空){ 返 0; }\n' % d, None
    if pos == '逗号续':
        return INC + '整 主(空){ 整 首, %s; 首 = 1; (空)首; 返 0; }\n' % declarator(form, GOOD), None
    raise ValueError(pos)


def run(cc, text, workdir, tag, exe=False, want=None):
    src = os.path.join(workdir, tag + '.c')
    io.open(src, 'w', encoding='utf-8', newline='\n').write(text)
    out = os.path.join(workdir, tag + ('.exe' if exe else '.o'))
    cmd = [cc, src, '-o', out]
    if not exe:
        cmd = [cc, '-c', src, '-o', out]
    r = subprocess.run(cmd, capture_output=True, timeout=180)
    msg = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')
    rc2, got = None, ''
    if exe and r.returncode == 0 and os.path.exists(out):
        p = subprocess.run([out], capture_output=True, timeout=60)
        rc2 = p.returncode
        got = (p.stdout or b'').decode('utf-8', 'replace').strip()
    return r.returncode, msg, rc2, got


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(ROOT, 'qcc_zh_v1.exe'))
    ap.add_argument('--tsv', default='')
    ap.add_argument('--quiet', action='store_true')
    # R878（评审第四轮）：**定点复跑** —— `--only <子串>` 只跑抽象声明符里 tag 含该子串的格
    #   （例：`--only 抽象-转型` 只跑三格转型）。用途是让评审打回的**具体形态**能单独出证据，
    #   不必每次付整矩阵 110 s。判据口径不变：仍然编得过 + 运行值相符。
    ap.add_argument('--only', default='')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    if not os.path.exists(cc):
        print('[FAIL] 环境未就绪: 找不到编译器 %s（本判据未执行，不判绿）' % cc)
        return 2
    # 指纹：让每条日志自带「测的是哪个编译器」。$expected 口径 = exe 的 SHA256 前 8 位，
    # 故这一行能直接对上 build_zh.ps1 的不动点（防「拿旧 exe 当 shipped」的误判）。
    import hashlib
    fp = hashlib.sha256(open(cc, 'rb').read()).hexdigest()[:8].upper()
    print('[指纹] %s  SHA256[:8] = %s' % (os.path.basename(cc), fp))
    w = tempfile.mkdtemp(prefix='qcc_census_')
    rows, bad, total = [], [], 0

    if a.only:
        for tag, text, want in ABSTRACT_POS:
            if a.only not in tag:
                continue
            total += 1
            exe = want is not None
            rc, msg, rc2, got = run(cc, text, w, 'a_%s' % tag, exe=exe)
            ok = rc == 0 and (not exe or (rc2 == 0 and got == want))
            code = 'OK' if ok else ('rc=%d' % rc if rc != 0 else 'out=%r(期望 %r)' % (got, want))
            rows.append(('抽象声明符', tag, '—', '正', rc, code, 'OK' if ok else 'RED'))
            if want is None:
                print('[%-22s] 编译 rc=%d %s' % (tag, rc, code))
            else:
                print('[%-22s] 编译 rc=%d 运行 out=%r（期望 %r） %s' % (tag, rc, got, want, code))
            if not ok:
                bad.append(('抽象声明符', tag, '—', rc, code))
        for tag, text, want in FOR_INIT_POS:
            if a.only not in tag:
                continue
            total += 1
            exe = want != 'reject'
            rc, msg, rc2, got = run(cc, text, w, 'f_%s' % tag, exe=exe)
            if want == 'reject':
                ok = rc != 0 and 'A9001' in msg
                code = 'A9001' if 'A9001' in msg else ('NONE' if rc == 0 else 'OTHER')
            else:
                ok = rc == 0 and rc2 == 0 and got == want
                code = 'OK' if ok else ('rc=%d' % rc if rc != 0 else 'out=%r(期望 %r)' % (got, want))
            rows.append(('for-init', tag, '—', '负' if want == 'reject' else '正', rc, code, 'OK' if ok else 'RED'))
            print('[%-22s] rc=%d %s' % (tag, rc, code))
            if not ok:
                bad.append(('for-init', tag, '—', rc, code))
        print('\n=== 定向抽象位格: %d 格，红 %d 格 ⟹ %s ==='
              % (total, len(bad), 'PASS' if not bad else 'FAIL'))
        return 0 if not bad else 1

    for pos, forms in POS_FORMS.items():
        for form in forms:
            for name in BAD_NAMES:
                total += 1
                rc, msg, _, _ = run(cc, neg_source(pos, form, name), w, 'n_%s_%s_%s' % (pos, form, name))
                code = 'A9001' if 'A9001' in msg else ('A1004' if 'A1004' in msg else ('NONE' if rc == 0 else 'OTHER'))
                ok = rc != 0 and code == 'A9001'
                rows.append((pos, form, name, '负', rc, code, 'OK' if ok else 'RED'))
                if not ok:
                    bad.append((pos, form, name, rc, code))

            total += 1
            text, want = pos_source(pos, form)
            # 局部/全局 有期望值时真跑产物；其余编译即可
            exe = want is not None
            rc, msg, rc2, got = run(cc, text, w, 'p_%s_%s' % (pos, form), exe=exe, want=want)
            ok = rc == 0 and (not exe or (rc2 == 0 and got == want))
            code = 'OK' if ok else ('rc=%d' % rc if rc != 0 else 'out=%r(期望 %r)' % (got, want))
            rows.append((pos, form, GOOD, '正', rc, code, 'OK' if ok else 'RED'))
            if not ok:
                bad.append((pos, form, GOOD + '(正例)', rc, code))

    for tag, text, want in ABSTRACT_POS:
        total += 1
        exe = want is not None
        rc, msg, rc2, got = run(cc, text, w, 'a_%s' % tag, exe=exe)
        ok = rc == 0 and (not exe or (rc2 == 0 and got == want))
        code = 'OK' if ok else ('rc=%d' % rc if rc != 0 else 'out=%r(期望 %r)' % (got, want))
        rows.append(('抽象声明符', tag, '—', '正', rc, code, 'OK' if ok else 'RED'))
        if not ok:
            bad.append(('抽象声明符', tag, '—', rc, code))

    for tag, text, want in FOR_INIT_POS:
        total += 1
        exe = want != 'reject'
        rc, msg, rc2, got = run(cc, text, w, 'f_%s' % tag, exe=exe, want=(want if exe else None))
        if want == 'reject':
            ok = rc != 0 and 'A9001' in msg
            code = 'A9001' if 'A9001' in msg else ('A1004' if 'A1004' in msg else ('NONE' if rc == 0 else 'OTHER'))
        else:
            ok = rc == 0 and rc2 == 0 and got == want
            code = 'OK' if ok else ('rc=%d' % rc if rc != 0 else 'out=%r(期望 %r)' % (got, want))
        rows.append(('for-init', tag, '—', '负' if want == 'reject' else '正', rc, code, 'OK' if ok else 'RED'))
        if not ok:
            bad.append(('for-init', tag, '—', rc, code))

    if a.tsv:
        io.open(a.tsv, 'w', encoding='utf-8', newline='\n').write(
            'position\tform\tname\tkind\trc\tcode\tverdict\n' +
            '\n'.join('\t'.join(str(x) for x in r) for r in rows) + '\n')
    if not a.quiet:
        for r in rows:
            if r[6] == 'RED':
                print('[RED] %-4s / %-10s / %-2s → rc=%s %s' % (r[0], r[1], r[2], r[4], r[5]))
        print('=== 声明位普查矩阵: %d 格，红 %d 格 ⟹ %s ==='
              % (total, len(bad), 'PASS' if not bad else 'FAIL'))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
