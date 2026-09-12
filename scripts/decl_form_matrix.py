#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""声明形态 覆盖矩阵 —— 一等 闸 指标 (启元 复盘 序8, 2026-09-13)

动机 (R1-R4 复盘 的 共同 教训):
  同一个 语义 常常 有 **多条 独立 解析路径** (C `union` vs 中文 `联`; 具名 typedef vs 匿名 typedef vs
  中文 `类型 名 = 构 {...}`)。锚 只 覆盖 一条 路径 时, 另一条 路径 **整条 零格** —— 而 闸 依然 全绿。
  R1 (union 语义) · R2 (匿名 typedef 尾填充) · R3 (别名 sizeof 静默 4) · R4 (结构词 前缀 成员取址)
  四条 全部 是 "零格 路径", 不是 "锚 写少了"。

本脚本 把 **声明形态** 枚举 成 固定 清单, 每 形态 必须 至少 有一个 **入闸 锚** (`@EXPECTED` + CORE_FEAT) 命中,
否则 判 红灯 (返回 1)。形态 计数 直接 从 锚 源文件 文本 抽取, 不 依赖 人工 维护 清单。

用法:
  python scripts/decl_form_matrix.py            # 表 + 退出码
  python scripts/decl_form_matrix.py --quiet    # 只 输出 零格 (红灯)
退出码: 0 = 全 形态 有 入闸 锚; 1 = 存在 零格
"""
import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCRIPTS = os.path.join(ROOT, 'scripts')
RUNNER = os.path.join(SCRIPTS, 'run_tests_zh.py')

# ---------------------------------------------------------------- 形态 清单
# (形态 名, 说明, [正则...] (任一 命中 即 算 该 锚 覆盖 本 形态))
FORMS = [
    ('C 具名结构体', 'struct Tag { ... }', [r'\bstruct\s+\w+\s*\{']),
    ('C 具名联合', 'union Tag { ... }', [r'\bunion\s+\w+\s*\{']),
    ('C 匿名 typedef struct', 'typedef struct { ... } X;', [r'\btypedef\s+struct\s*\{']),
    ('C 具名 typedef struct', 'typedef struct Tag { ... } X;', [r'\btypedef\s+struct\s+\w+\s*\{']),
    ('C 匿名 typedef union', 'typedef union { ... } X;', [r'\btypedef\s+union\s*\{']),
    ('C 具名 typedef union', 'typedef union Tag { ... } X;', [r'\btypedef\s+union\s+\w+\s*\{']),
    ('中文 具名结构体', '构 标 { ... }', [r'构\s+\w+\s*\{']),
    ('中文 具名联合', '联/联合 标 { ... }', [r'(?:^|[^A-Za-z_])(?:联|联合)\s+\w+\s*\{']),
    ('中文别名 → 具名类型', '类型 X = 构 标;', [r'类型\s+\w+\s*=\s*(?:构|联|联合|枚|枚举)\s+\w+\s*;']),
    ('中文别名 → 匿名体', '类型 X = 构 { ... };', [r'类型\s+\w+\s*=\s*(?:构|联|联合|枚|枚举)\s*\{']),
    ('中文关键字 类型定义', '类型定义 构 { ... } X;', [r'类型定义\s']),
    ('别名 → 别名', '类型 X = Y; (Y 已是别名)', [r'类型\s+\w+\s*=\s*\w+\s*;']),
    ('成员取址 反射 (结构词前缀)', '大小(((struct T *)0)->字段)', [r'(?:大小|sizeof)\s*\(\s*\(\s*\(\s*(?:构|struct|union|联|联合)\s+\w+\s*\*']),
    ('成员取址 反射 (别名/型词)', '大小(((T *)0)->字段)', [r'(?:大小|sizeof)\s*\(\s*\(\s*\(\s*\w+\s*\*']),
    ('结构体数组成员字段', '整数 数据[N]; (结构体内)', [r'(?:整|整数|字|字符|双|短|长|int|char|long|short|double)\s+\w+\s*\[\s*\d+\s*\]\s*;']),
    ('位域字段', '无符号 位:3;', [r'\w+\s*:\s*\d+\s*;']),
    ('函数指针字段', '整数 (*函)(整数);', [r'\(\s*\*\s*\w+\s*\)\s*\([^)]*\)\s*;']),
    ('嵌套结构体字段', '构 内 { ... } 嵌;', [r'(?:构|struct)\s*(?:\w+\s*)?\{[^{}]*\}\s*\w+\s*;']),
    ('结构体指针字段', '构 N *下;', [r'(?:构|struct)\s+\w+\s*\*\s*\w+\s*;']),
    ('联合 跨成员 读写', 'u.i 写 → u.c 读 (覆盖 断言)', [r'\.\s*\w+\s*\[\s*0\s*\]\s*=\s*\d+']),
]


def core_feat_set():
    """从 run_tests_zh.py 抽 CORE_FEAT 白名单 的 锚 文件 名"""
    try:
        t = io.open(RUNNER, encoding='utf-8', errors='replace').read()
    except Exception:
        return set()
    m = re.search(r'CORE_FEAT\s*=\s*\[(.*?)\n\]', t, re.S)
    if not m:
        return set()
    return set(re.findall(r"'scripts/([^']+\.c)'", m.group(1)))


def anchors():
    """全部 带 @EXPECTED 的 锚 (含 入闸 与 未入闸)"""
    out = {}
    for f in sorted(os.listdir(SCRIPTS)):
        if not f.endswith('.c'):
            continue
        p = os.path.join(SCRIPTS, f)
        try:
            t = io.open(p, encoding='utf-8', errors='replace').read()
        except Exception:
            continue
        if '@EXPECTED' in t:
            out[f] = t
    return out


def main():
    quiet = '--quiet' in sys.argv
    core = core_feat_set()
    allanchors = anchors()
    empty, rows = [], []
    for name, desc, pats in FORMS:
        hit_all = [f for f, t in allanchors.items() if any(re.search(p, t, re.M) for p in pats)]
        hit_core = [f for f in hit_all if f in core]
        rows.append((name, desc, len(hit_all), len(hit_core), hit_core[:3]))
        if not hit_core:
            empty.append((name, desc, len(hit_all)))

    if not quiet:
        print('== 声明形态 覆盖矩阵 (锚 = scripts/*.c 且 含 @EXPECTED; 入闸 = CORE_FEAT) ==')
        print('%-30s %-34s %6s %6s  %s' % ('形态', '样例', '有锚', '入闸', '入闸 锚 (前 3)'))
        for name, desc, na, nc, samp in rows:
            flag = '  << 零格' if nc == 0 else ''
            print('%-30s %-34s %6d %6d  %s%s' % (name, desc, na, nc, ','.join(samp), flag))
        print('形态 总数 %d, 零格 %d' % (len(rows), len(empty)))

    if empty:
        print('[红灯] 以下 形态 无 入闸 锚 (整条 解析路径 零覆盖):')
        for name, desc, na in empty:
            print('   - %s (%s) — 有锚 %d, 入闸 0' % (name, desc, na))
        return 1
    if not quiet:
        print('[绿灯] 全 %d 个 声明形态 均 有 入闸 锚' % len(rows))
    return 0


if __name__ == '__main__':
    sys.exit(main())
