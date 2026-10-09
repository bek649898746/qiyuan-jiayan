#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""t8_bisect_lobject —— T8「头文件声明漏读」定位器（可复算）

病灶（R622 收窄到单文件）:
  `lobject.h` 被 **部分读取** —— 末尾 L797-809 那批 `LUAI_FUNC` 声明未入符号表
  （`y1_lobject_alone.c` 单 include 即复现 A1001「未声明函数 'luaO_str2num'」，
   手抄同一条声明 `y4_handcopy.c` 绿 ⟹ 不是声明形态问题，是 **没读到**）。

三种探法（互为交叉验证）:
  --tail   ★主探法（无注入 · 无 confound）: 把 lobject.h **整份** include 进来，
           再用 `#ifdef <名>` 探「定义在第 M 行」的宏是否已登记 ⟹ 直接量出「读到哪一行」。
  --mre    最小复现: 四种宏形态（函数式/对象式 × 有无续行）+ 「删掉疑似触发行」对照，
           用来证伪/证实「某构造导致其后内容被吞」。
  --mre    末尾附**决定性**对照: 全量 lobject.h vs 删掉 L786-787 的 lobject.h，
           两者的 `luaO_str2num` 可见性若相同 ⟹ 该行 **不是** 中断原因（证伪）。
  --bisect marker 注入二分（次要）: 在第 N 行（**自动跳过 续行**，避免注入进宏体）后插入
           `LUAI_FUNC int qzmk(void);` + 调用点，看 marker 是否登记。

用法:
  python scripts/t8_bisect_lobject.py --tail   --cc <exe>
  python scripts/t8_bisect_lobject.py --mre    --cc <exe>
  python scripts/t8_bisect_lobject.py --bisect --cc <exe>
"""
import io
import os
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CORPUS = r'D:\qiyuan_home\corpus\lua-5.4.7'
LOBJ = os.path.join(CORPUS, 'lobject.h')
TMP = os.path.join(os.environ.get('TEMP', '.'), '_t8_bisect')
os.makedirs(TMP, exist_ok=True)

PREFIX = '#define LUA_CORE\n#include "lprefix.h"\n'
MARKER_DECL = 'LUAI_FUNC int qzmk(void);\nint qzuse(void) { return qzmk(); }\n'

# 「定义在 lobject.h 第 M 行」的宏 —— 可见性 = 读到哪一行
TAIL_MACROS = [
    ('LUA_TDEADKEY', 90),
    ('setdeadkey', 776),
    ('keyisdead', 777),
    ('lmod', 786),
    ('twoto', 790),
    ('sizenode', 791),
    ('UTF8BUFFSZ', 795),
]


def _run(cc, src):
    dst = os.path.join(TMP, os.path.splitext(os.path.basename(src))[0] + '.obj')
    try:
        os.remove(dst)
    except OSError:
        pass
    cmd = [os.path.join(REPO, cc) if not os.path.isabs(cc) else cc,
           '--lenient', '-D', '_WIN32=1', '-I', CORPUS, '-c', src, '-o', dst]
    r = subprocess.run(cmd, capture_output=True, encoding='utf-8', errors='replace',
                       timeout=120, cwd=REPO)
    return r.returncode, (r.stdout or '') + (r.stderr or '')


def _first(txt):
    for l in txt.splitlines():
        if l.startswith('[ERR]') or l.startswith('[WARN]'):
            return l[:130]
    return ''


def tail_probe(cc, lines, log):
    """整份 include + `#ifdef <宏>` 探针: 直接量出「读到哪一行」。"""
    print('== 主探法: 整份 include lobject.h, 用 #ifdef 探 各宏 是否已登记 ==')
    print('   (宏可见 ⟹ 该行 已读到; 不可见 ⟹ 读到该行之前就停了)')
    visible = []
    for name, m in TAIL_MACROS:
        src = os.path.join(TMP, 'tail_%s.c' % name)
        with io.open(src, 'w', encoding='utf-8', newline='\n') as f:
            f.write(PREFIX + ''.join(lines))
            f.write('\n#ifdef %s\nint qz_has_%s(void) { return 1; }\n#endif\n' % (name, name))
            f.write('#ifndef %s\nint qzmiss_%s(void);\nint qzuse(void) { return qzmiss_%s(); }\n#endif\n'
                    % (name, name, name))
        rc, txt = _run(cc, src)
        seen = ('qzmiss_%s' % name) not in txt
        visible.append((name, m, seen))
        print('  %-14s (L%-4d) rc=%d  已读到=%s   %s' % (name, m, rc, seen, _first(txt)))
        log.append('TAIL %-14s L%-4d rc=%d 读到=%s %s' % (name, m, rc, seen, _first(txt)))

    # 交叉: 同一次编译里, 直接把「读到哪」与「声明入表否」并列
    src = os.path.join(TMP, 'tail_both.c')
    with io.open(src, 'w', encoding='utf-8', newline='\n') as f:
        f.write(PREFIX + ''.join(lines))
        f.write('#ifdef UTF8BUFFSZ\nint qz_has_utf8(void) { return 1; }\n#endif\n')
        f.write('#ifndef UTF8BUFFSZ\nint qzmiss_utf8(void);\nint qzuse_utf8(void) { return qzmiss_utf8(); }\n#endif\n')
        f.write('\nint 使用者(void) { return (int)luaO_str2num("1", (void*)0); }\n')
    rc, txt = _run(cc, src)
    print('  [并列] UTF8BUFFSZ(L795) 与 luaO_str2num(L803) 同编译 rc=%d %s' % (rc, _first(txt)))
    log.append('TAIL both rc=%d %s' % (rc, _first(txt)))
    return visible


# ---- 最小复现（--mre）: 钉住「触发构造」到底是哪一种 ----
MRE_CASES = [
    ('mre_a_funclike_cont', '#define 甲(s,n) \\\n  ((s) & ((n)-1))\nint 用一(void){ return 甲(8,8); }\n'),
    ('mre_b_funclike_plain', '#define 乙(x) (1<<(x))\nint 用二(void){ return 乙(3); }\n'),
    ('mre_c_objlike_cont', '#define 丙 \\\n  7\nint 用三(void){ return 丙; }\n'),
]


def mre(cc, lines, log):
    print('== ① 三种宏形态 最小复现（各自独立文件）==')
    for name, body in MRE_CASES:
        src = os.path.join(TMP, name + '.c')
        with io.open(src, 'w', encoding='utf-8', newline='\n') as f:
            f.write(body)
        rc, txt = _run(cc, src)
        print('  %-24s rc=%d  %s' % (name, rc, _first(txt)))
        log.append('MRE %-24s rc=%d %s' % (name, rc, _first(txt)))

    print('== ② 决定性对照: 全量 lobject.h  vs  删掉 L786-787 ==')
    kept = [ln for i, ln in enumerate(lines) if i not in (785, 786)]
    for tag, body_lines in (('full', lines), ('no_lmod_786_787', kept)):
        src = os.path.join(TMP, 'nolm_%s.c' % tag)
        with io.open(src, 'w', encoding='utf-8', newline='\n') as f:
            f.write(PREFIX + ''.join(body_lines))
            f.write('\nint 使用者(void) { return (int)luaO_str2num("1", (void*)0); }\n')
        rc, txt = _run(cc, src)
        print('  %-16s rc=%d  %s' % (tag, rc, _first(txt)))
        log.append('NOLMOD %-16s rc=%d %s' % (tag, rc, _first(txt)))


def _marker_invisible(cc, N, lines, log):
    """在 **逻辑行** 第 N 行之后注入 marker（若该行 以 \\ 续行 则顺延到 宏体 结束）。"""
    j = N
    while j < len(lines) and lines[j].rstrip('\r\n').endswith('\\'):
        j += 1
    src = os.path.join(TMP, 'mk_%04d.c' % N)
    with io.open(src, 'w', encoding='utf-8', newline='\n') as f:
        f.write(PREFIX + ''.join(lines[:j] if j >= N else lines[:N]))
        f.write(MARKER_DECL)
    rc, out = _run(cc, src)
    if rc == 0:
        v = False
    elif 'qzmk' in out:
        v = True
    else:
        v = None
    log.append('MK N=%-4d(实注入 %d) rc=%d verdict=%s %s' % (N, j, rc, v, _first(out)))
    return v


def bisect(cc, lines, log):
    lo, hi = 1, len(lines)
    flo = _marker_invisible(cc, lo, lines, log)
    fhi = _marker_invisible(cc, hi, lines, log)
    log.append('# 端点: P(%d)=%s P(%d)=%s' % (lo, flo, hi, fhi))
    if flo is True or fhi is not True:
        log.append('# ⚠️ 端点 不满足单调前提 ⟹ 退化为 线性扫描 (末尾 120 行)')
        for N in range(max(1, len(lines) - 120), len(lines) + 1):
            if _marker_invisible(cc, N, lines, log) is True:
                return N
        return None
    while lo + 1 < hi:
        mid = (lo + hi) // 2
        if _marker_invisible(cc, mid, lines, log) is True:
            hi = mid
        else:
            lo = mid
    return hi


def decl_probe(cc, lines, log):
    """在 **整份** lobject.h 之后, 逐个「使用」L797-809 那批 LUAI_FUNC 声明的函数名,
       量出**首个未入符号表的名字**（A1001）⟹ 首个触发构造。"""
    print('== 声明探针: 整份 include 后 逐个 调用 L797-809 的 声明名 ==')
    for m in range(797, 810):
        src_line = lines[m - 1].rstrip('\r\n')
        if not src_line.strip() or 'luaO_' not in src_line:
            continue
        head = src_line.split('(')[0]
        nm = [t for t in head.replace('*', ' ').split() if t.startswith('luaO_')][-1]
        src = os.path.join(TMP, 'decl_%s.c' % nm)
        with io.open(src, 'w', encoding='utf-8', newline='\n') as f:
            f.write(PREFIX + ''.join(lines))
            f.write('\nint qzcall_%s(void) { return (int)%s(0); }\n' % (nm, nm))
        rc, txt = _run(cc, src)
        undeclared = ('A1001' in txt and nm in txt)
        other = _first(txt)
        print('  L%-4d %-18s rc=%d 未入符号表=%s  %s' % (m, nm, rc, undeclared,
                                                         ('' if undeclared else other)))
        log.append('DECL L%-4d %-18s rc=%d undeclared=%s %s' % (m, nm, rc, undeclared, other))

    print('== 形态对照（同一名字, 不同声明载体）==')
    for tag, body in (
        ('outer_extern', 'extern int 申一(void);\nint qzuse(void){ return 申一(); }\n'),
        ('outer_luaifunc', '#define LUAI_FUNC extern\nLUAI_FUNC int 申二(void);\nint qzuse(void){ return 申二(); }\n'),
    ):
        src = os.path.join(TMP, 'form_%s.c' % tag)
        with io.open(src, 'w', encoding='utf-8', newline='\n') as f:
            f.write(body)
        rc, txt = _run(cc, src)
        print('  %-16s rc=%d  %s' % (tag, rc, _first(txt)))
        log.append('FORM %-16s rc=%d %s' % (tag, rc, _first(txt)))

    src = os.path.join(TMP, 'form_after_big_inc.c')
    with io.open(src, 'w', encoding='utf-8', newline='\n') as f:
        f.write(PREFIX + ''.join(lines))
        f.write('\nLUAI_FUNC int 申三(void);\nint qzuse(void){ return 申三(); }\n')
    rc, txt = _run(cc, src)
    print('  %-16s rc=%d  %s' % ('inc_then_decl', rc, _first(txt)))
    log.append('FORM inc_then_decl rc=%d %s' % (rc, _first(txt)))


# ---- 根因最小复现（--root）: 「未知 类型名 作 声明 的 类型位」----
ROOT_CASES = [
    ('c1_extern_unknown', 'extern size_t 甲一(void);\nint 用一(void){ return (int)甲一(); }\n'),
    ('c2_extern_known', 'extern int 甲二(void);\nint 用二(void){ return 甲二(); }\n'),
    ('c3_typedef_first', 'typedef unsigned long size_t;\nextern size_t 甲三(void);\nint 用三(void){ return (int)甲三(); }\n'),
    ('c4_var_unknown_t', 'size_t 甲四;\nint 用四(void){ return 0; }\n'),
    ('c5_two_unknown', 'extern size_t 甲五(void);\nextern size_t 乙五(void);\nint 用五(void){ return (int)甲五() + (int)乙五(); }\n'),
    ('c6_param_unknown_t', 'extern int 甲六(size_t n);\nint 用六(void){ return 甲六(0); }\n'),
]


def root_probe(cc, lines, log):
    print('== 根因最小复现: 声明 的 类型位 是「未知标识符」==')
    for tag, body in ROOT_CASES:
        src = os.path.join(TMP, 'root_%s.c' % tag)
        with io.open(src, 'w', encoding='utf-8', newline='\n') as f:
            f.write(body)
        rc, txt = _run(cc, src)
        print('  %-22s rc=%d  %s' % (tag, rc, _first(txt)))
        log.append('ROOT %-22s rc=%d %s' % (tag, rc, _first(txt)))

    print('== 决定性: lobject.h 之前 补 `typedef unsigned long size_t;` ==')
    for tag, pre, post in (('with_typedef', 'typedef unsigned long size_t;\n', ''),
                           ('without', '', ''),
                           ('typedef_after', '', 'typedef unsigned long size_t;\n')):
        src = os.path.join(TMP, 'sz_%s.c' % tag)
        with io.open(src, 'w', encoding='utf-8', newline='\n') as f:
            f.write(PREFIX + pre + ''.join(lines) + post)
            f.write('\nint 使用者(void) { return (int)luaO_str2num("1", (void*)0); }\n')
        rc, txt = _run(cc, src)
        print('  %-16s rc=%d  %s' % (tag, rc, _first(txt)))
        log.append('SZ %-16s rc=%d %s' % (tag, rc, _first(txt)))


def main():
    av = sys.argv[1:]
    cc = 'qcc_zh_v1.exe'
    if '--cc' in av:
        i = av.index('--cc')
        cc = av[i + 1]
        del av[i:i + 2]
    with io.open(LOBJ, encoding='utf-8', errors='replace', newline='') as f:
        lines = f.read().splitlines(True)
    print('# 语料文件: %s  (%d 行)   编译器: %s' % (LOBJ, len(lines), cc))
    log = []
    if '--mre' in av:
        mre(cc, lines, log)
    elif '--decl' in av:
        decl_probe(cc, lines, log)
    elif '--root' in av:
        root_probe(cc, lines, log)
    elif '--bisect' in av:
        k = bisect(cc, lines, log)
        print('\n'.join(log))
        print('# ⟹ marker 首个「读不到」的行 N* = %s' % k)
    else:
        tail_probe(cc, lines, log)
    return 0


if __name__ == '__main__':
    sys.exit(main())
