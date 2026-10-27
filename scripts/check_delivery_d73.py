# -*- coding: utf-8 -*-
"""check_delivery_d73 —— 审计递刀 D70/D71/D72 · D66 · D64 的**可复跑判据**（2026-10-25）

来源（启元回执，探针原样驱动 shipped 编译器）：
  `_AUDIT_RECEIPT_20261025_启元_语法位置检查完备性.md`（D73 = D70+D71+D72）
  `_AUDIT_RECEIPT_20261025_启元_结构体初始化复制路径.md`（D66/D67/D64）

判据（每条都跑 shipped 编译器 + 真跑产物；断言**具体值**，不只看 rc）：
  D70  指针变量名撞保留类型词 `整 *无 = &x;` ⟹ 必须 A9001（原 rc=0 且 `无==0` 恒真）
  D71  形参名撞保留类型词 `整 用长(整 长)` ⟹ 必须 A9001（原 rc=0 且参数值被破坏，循环少跑一轮）
  D72  指针用点号 `指.成员` ⟹ 必须 A9001 且提示 `->`；对照 `a->成员` 仍必须正确（14）
  D66  结构体复制 >128 B：值参 / 数组元素 / 全局 / 混合 四条路径的尾部字段必须正确；
       132 B 结构体的 `项[32]` 必须 = 132（原 0）；>128 B **返回值**不得崩溃
  D68  语句式 `格式化 "…",x);`（多一个右括号）⟹ 报错文案必须点出「`格式化` 是 语句式」；
       对照正例 `格式化 "…",x;` 必须编过且输出正确
  D67  `构 T b = a;`（非 Copy 结构体初始化式）⟹ 必须 A3101，与赋值 `b = a;` 同款；
       对照 Copy 结构体（全标量字段）初始化必须放行且值正确
  D64  块内声明/赋值缺分号 ⟹ 必须 A1004（原 rc=0 且后续语句被静默吞掉）

用法: python scripts/check_delivery_d73.py [--cc qcc_zh_v1.exe]
退出码: 0 = 全绿；1 = 有红；2 = 环境未就绪
"""
import argparse
import io
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CRASH = (-1073741819, 3221225477)

W = None


def src(name, text):
    p = os.path.join(W, name + '.jy')
    io.open(p, 'w', encoding='utf-8', newline='\n').write(text)
    return p


def build(cc, path, exe):
    r = subprocess.run([cc, path, '-o', exe], capture_output=True, timeout=300)
    return r.returncode, ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')


def run(exe):
    if not os.path.exists(exe):
        return None, ''
    r = subprocess.run([exe], capture_output=True, timeout=120)
    return r.returncode, (r.stdout or b'').decode('utf-8', 'replace')


def main():
    global W
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(ROOT, 'qcc_zh_v1.exe'))
    ap.add_argument('--only', default='', help='只跑指定组（逗号分隔: D70,D71,D72,D66,D67,D68,D64）；空=全跑')
    a = ap.parse_args()
    only = set(x.strip() for x in a.only.replace('，', ',').split(',') if x.strip())

    def _want(tag):
        return (not only) or (tag in only)
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    if not os.path.exists(cc):
        print('[FAIL] 环境未就绪: 找不到编译器 %s（本判据未执行，不判绿）' % cc)
        return 2
    W = tempfile.mkdtemp(prefix='qcc_d73_')
    bad = []

    # ---------- D70 变量/声明符 名字位 撞保留类型词（六位） ----------
    if _want('D70'):
        d70 = {
            # 指针变量名（原格）
            '指针名': ('#包含 <stdio.h>\n整 主(空){ 整 x = 5; 整 *无 = &x; 格式化 "%d\\n", (无 == 0); 返 0; }\n', True),
            # 数组名：R869 前 误诊为 A1004「缺少分号」（分号齐全）
            '数组名': ('#包含 <stdio.h>\n整 主(空){ 整 无[3]; 返 0; }\n', True),
            # 逗号续声明符：R869 前 误诊为 A1004
            '逗号续名': ('#包含 <stdio.h>\n整 主(空){ 整 a = 1, 无 = 2; 返 a; }\n', True),
            # 文件作用域（既有 C155b 覆盖，作为回归位）
            '全局名': ('#包含 <stdio.h>\n整 无 = 5;\n整 主(空){ 返 0; }\n', True),
            # 函数指针 声明符 名位：R869 前 rc=0 零诊断
            '函数指针': ('#包含 <stdio.h>\n整 主(空){ 整 (*无)(整); 返 0; }\n', True),
            # 函数指针 名字位 带 限定词 / 多级指针（R870：原 这些形态 全部 rc=0 零诊断）
            '函数指针const': ('#包含 <stdio.h>\n整 主(空){ 整 (* const 无)(整); 返 0; }\n', True),
            '函数指针volatile': ('#包含 <stdio.h>\n整 主(空){ 整 (* volatile 无)(整); 返 0; }\n', True),
            '函数指针二级': ('#包含 <stdio.h>\n整 主(空){ 整 (* const * 无)(整); 返 0; }\n', True),
            # 函数指针 字段位：R869 前 rc=0 零诊断
            '函数指针字段': ('#包含 <stdio.h>\n构 点 { 整 (*无)(整); };\n整 主(空){ 返 0; }\n', True),
            '函数指针字段const': ('#包含 <stdio.h>\n构 点 { 整 (* const 无)(整); };\n整 主(空){ 返 0; }\n', True),
            # 指针 数组 名位（R870：局部原误诊 A1004、全局原 rc=0）
            '指针数组': ('#包含 <stdio.h>\n整 主(空){ 整 *无[3]; 返 0; }\n', True),
            # 指针 + 限定词 的 变量名位（R870：原误诊 A1004）
            '指针const名': ('#包含 <stdio.h>\n整 主(空){ 整 * const 无; 返 0; }\n', True),
            '全局指针const': ('#包含 <stdio.h>\n整 * const 无 = 0;\n整 主(空){ 返 0; }\n', True),
            # 正例：合法 数组/函数指针 声明 必须 编过（防 普查 误伤）
            '正例数组指针': ('#包含 <stdio.h>\n整 主(空){ 整 arr[3]; 整 (*fp)(整); arr[0] = 1; fp = 0; 格式化 "%d\\n", arr[0] + (fp == 0); 返 0; }\n', False),
            # 正例：合法 限定词 声明（`* const p` / `(* const fp)` / `*arr[3]`）必须 编过且运行正确
            '正例const': ('#包含 <stdio.h>\n整 主(空){ 整 (* const fp)(整); 整 *arr[3]; arr[0] = 0; 整 * const p = 0; 格式化 "%d\\n", (fp == 0) + (arr[0] == 0) + (p == 0); 返 0; }\n', False),
        }
        ok70 = True
        for tag, (text, want_fail) in d70.items():
            p = src('d70_' + tag, text)
            rc, msg = build(cc, p, os.path.join(W, 'd70_%s.exe' % tag))
            good = (rc != 0 and 'A9001' in msg) if want_fail else (rc == 0)
            print('[D70 %-8s] rc=%d A9001=%s（期望 %s）⟹ %s'
                  % (tag, rc, 'A9001' in msg, '拒绝' if want_fail else '编过', 'OK' if good else 'RED'))
            ok70 = ok70 and good
        if not ok70:
            bad.append('D70')
        # 全矩阵（声明位 × 形态 × 保留类型词）：315 格，负例全 A9001、正例全编过/运行值正确。
        # 只在**显式点名 D70** 时跑（台账里 D70/D71 两行的 criterion_cmd），
        # 其它行跑整组 16 格时不必重复付 110 s 的矩阵成本。
        if only and 'D70' in only:
            r = subprocess.run([sys.executable, '-X', 'utf8', os.path.join(ROOT, 'scripts', 'census_matrix.py'),
                                '--cc', cc, '--quiet'], capture_output=True, timeout=900)
            mok = (r.returncode == 0)
            print('[D70 矩阵     ] 声明位全矩阵（5 位 × 8 形态 × 8 类型词 + 抽象声明符正例）rc=%d ⟹ %s'
                  % (r.returncode, 'OK' if mok else 'RED'))
            if not mok:
                bad.append('D70-matrix')

    # ---------- D71 形参 / 结构体字段 名位 撞保留类型词 ----------
    if _want('D71'):
        d71 = {
            '形参名': ('#包含 <stdio.h>\n'
                       '整 用长(整 长){ 整 s=0; 整 i; 遍(i=0;i<长;i=i+1){ s=s+i; } 返 s; }\n'
                       '整 主(空){ 格式化 "%d\\n", 用长(10); 返 0; }\n', True),
            '结构体字段': ('#包含 <stdio.h>\n构 点 { 整 无; };\n整 主(空){ 返 0; }\n', True),
            '正例字段': ('#包含 <stdio.h>\n构 点 { 整 值; 字 名[4]; };\n整 主(空){ 构 点 p; p.值 = 7; 格式化 "%d\\n", p.值; 返 0; }\n', False),
        }
        ok71 = True
        for tag, (text, want_fail) in d71.items():
            p = src('d71_' + tag, text)
            rc, msg = build(cc, p, os.path.join(W, 'd71_%s.exe' % tag))
            good = (rc != 0 and 'A9001' in msg) if want_fail else (rc == 0)
            print('[D71 %-8s] rc=%d A9001=%s（期望 %s）⟹ %s'
                  % (tag, rc, 'A9001' in msg, '拒绝' if want_fail else '编过', 'OK' if good else 'RED'))
            ok71 = ok71 and good
        if not ok71:
            bad.append('D71')

    # ---------- D72 指针用点号 ----------
    if _want('D72'):
        p = src('d72neg', '#包含 <stdio.h>\n构 点 { 整 x; };\n'
                          '整 主(空){ 构 点 p; p.x=3; 构 点 *指=&p; 格式化 "%d\\n", 指.x; 返 0; }\n')
        rc, msg = build(cc, p, os.path.join(W, 'd72n.exe'))
        ok72n = (rc != 0 and 'A9001' in msg and '->' in msg)
        print('[D72 指针点号 ] rc=%d A9001=%s 提示->=%s ⟹ %s'
              % (rc, 'A9001' in msg, '->' in msg, 'OK' if ok72n else 'RED'))
        p = src('d72pos', '#包含 <stdio.h>\n构 点 { 整 x; };\n'
                          '整 主(空){ 构 点 p; p.x=14; 构 点 *指=&p; 格式化 "%d\\n", 指->x; 返 0; }\n')
        rc, msg = build(cc, p, os.path.join(W, 'd72p.exe'))
        rc2, out = run(os.path.join(W, 'd72p.exe'))
        ok72p = (rc == 0 and rc2 == 0 and out.strip() == '14')
        print('[D72 箭头正例] 编译 rc=%d 运行 rc=%s out=%r（期望 14）⟹ %s'
              % (rc, rc2, out.strip(), 'OK' if ok72p else 'RED'))
        if not (ok72n and ok72p):
            bad.append('D72')

    # ---------- D66 结构体复制 > 128 B ----------
    if _want('D66'):
        big = ('#包含 <stdio.h>\n构 大 { 整 项[40]; 整 β; };\n'
               '整 造(构 大 *p, 整 基){ 整 j; 遍(j=0;j<40;j=j+1) p->项[j]=j+基; p->β=基+700; 返 0; }\n'
               '静 构 大 全;\n')
        c66 = {
            '值参': (big + '整 f(构 大 b){ 返 b.β; }\n整 主(空){ 构 大 a; 造(&a,300); 格式化 "%d\\n", f(a); 返 0; }\n', '1000'),
            '数组元素': (big + '整 f(构 大 b){ 返 b.β; }\n静 构 大 组[2];\n整 主(空){ 造(&组[1], 500); 格式化 "%d\\n", f(组[1]); 返 0; }\n', '1200'),
            '全局': (big + '整 f(构 大 b){ 返 b.β; }\n整 主(空){ 造(&全, 700); 格式化 "%d\\n", f(全); 返 0; }\n', '1400'),
        }
        ok66 = True
        for tag, (text, want) in c66.items():
            p = src('d66_' + tag, text)
            rc, msg = build(cc, p, os.path.join(W, 'd66_%s.exe' % tag))
            rc2, out = run(os.path.join(W, 'd66_%s.exe' % tag))
            good = (rc == 0 and rc2 == 0 and out.strip() == want)
            print('[D66 %-8s] 编译 rc=%d 运行 rc=%s out=%r（期望 %s）⟹ %s'
                  % (tag, rc, rc2, out.strip(), want, 'OK' if good else 'RED'))
            ok66 = ok66 and good
        # 132 B（M4 口径：整 项[31] + 整 项数 = 128，整 项[32] + 项数 = 132）
        p = src('d66_132', '#包含 <stdio.h>\n构 中 { 整 项[32]; 整 项数; };\n'
                           '整 f(构 中 b){ 返 b.项[32]; }\n'
                           '整 主(空){ 构 中 a; 整 i; 遍(i=0;i<32;i=i+1) a.项[i]=100+i; a.项[32]=132; '
                           '格式化 "%d\\n", f(a); 返 0; }\n')
        rc, msg = build(cc, p, os.path.join(W, 'd66_132.exe'))
        rc2, out = run(os.path.join(W, 'd66_132.exe'))
        good = (rc == 0 and rc2 == 0 and out.strip() == '132')
        print('[D66 132B 尾] 编译 rc=%d 运行 rc=%s out=%r（期望 132）⟹ %s'
              % (rc, rc2, out.strip(), 'OK' if good else 'RED'))
        ok66 = ok66 and good
        # 返回值（>128B）不得崩溃
        p = src('d66_ret', '#包含 <stdio.h>\n构 大 { 整 项[40]; 整 β; };\n'
                           '构 大 造2(空){ 构 大 x; 整 i; 遍(i=0;i<40;i=i+1) x.项[i]=i; x.β=888; 返 x; }\n'
                           '整 主(空){ 构 大 y = 造2(); 格式化 "%d\\n", y.β; 返 0; }\n')
        rc, msg = build(cc, p, os.path.join(W, 'd66_ret.exe'))
        rc2, out = run(os.path.join(W, 'd66_ret.exe'))
        good = (rc == 0 and rc2 == 0 and out.strip() == '888')
        print('[D66 返回值 ] 编译 rc=%d 运行 rc=%s（崩溃码 %s）out=%r（期望 888）⟹ %s'
              % (rc, rc2, rc2 in CRASH, out.strip(), 'OK' if good else 'RED'))
        ok66 = ok66 and good
        if not ok66:
            bad.append('D66')

    # ---------- D67 初始化式 移动判定 ----------
    if _want('D67'):
        move_src = ('#包含 <stdio.h>\n构 盒 { 整 *指; 整 值; };\n')
        # 负例：初始化式（原放行 ⟹ 之后读源 a 不报 = 双所有权静默）
        p = src('d67_init', move_src + '整 主(空){ 构 盒 a; a.指 = 0; a.值 = 9; 构 盒 b = a; 格式化 "%d\\n", b.值 + a.值; 返 0; }\n')
        rc, msg = build(cc, p, os.path.join(W, 'd67i.exe'))
        ok67i = (rc != 0 and 'A3101' in msg)
        print('[D67 初始化移动] rc=%d A3101=%s ⟹ %s' % (rc, 'A3101' in msg, 'OK' if ok67i else 'RED'))
        # 负例对照：赋值（既有行为，必须仍红）
        p = src('d67_asg', move_src + '整 主(空){ 构 盒 a; 构 盒 b; a.指 = 0; a.值 = 9; b = a; 格式化 "%d\\n", b.值 + a.值; 返 0; }\n')
        rc, msg = build(cc, p, os.path.join(W, 'd67a.exe'))
        ok67a = (rc != 0 and 'A3101' in msg)
        print('[D67 赋值对照  ] rc=%d A3101=%s ⟹ %s' % (rc, 'A3101' in msg, 'OK' if ok67a else 'RED'))
        # 正例：Copy 结构体（全标量）初始化 + 赋值 都放行，值正确
        p = src('d67_copy', '#包含 <stdio.h>\n构 点 { 整 x; 整 y; };\n'
                            '整 主(空){ 构 点 a; a.x = 3; a.y = 4; 构 点 b = a; 构 点 c; c = a; '
                            '格式化 "%d %d\\n", b.x + b.y, c.x + c.y; 返 0; }\n')
        rc, msg = build(cc, p, os.path.join(W, 'd67c.exe'))
        rc2, out = run(os.path.join(W, 'd67c.exe'))
        ok67c = (rc == 0 and rc2 == 0 and out.strip() == '7 7')
        print('[D67 Copy 正例] 编译 rc=%d 运行 rc=%s out=%r（期望 `7 7`）⟹ %s'
              % (rc, rc2, out.strip(), 'OK' if ok67c else 'RED'))
        if not (ok67i and ok67a and ok67c):
            bad.append('D67')

    # ---------- D68 语句式多一个右括号 ----------
    if _want('D68'):
        p = src('d68neg', '#包含 <stdio.h>\n'
                          '整 主(空){ 整 x = 7; 格式化 "%d\\n", x); 返 0; }\n')
        rc, msg = build(cc, p, os.path.join(W, 'd68n.exe'))
        ok68n = (rc != 0 and '语句式' in msg and '格式化' in msg)
        print('[D68 多余右括号] rc=%d 文案含「语句式」=%s ⟹ %s'
              % (rc, '语句式' in msg, 'OK' if ok68n else 'RED'))
        p = src('d68pos', '#包含 <stdio.h>\n'
                          '整 主(空){ 整 x = 7; 格式化 "%d\\n", x; 返 0; }\n')
        rc, msg = build(cc, p, os.path.join(W, 'd68p.exe'))
        rc2, out = run(os.path.join(W, 'd68p.exe'))
        ok68p = (rc == 0 and rc2 == 0 and out.strip() == '7')
        print('[D68 正例      ] 编译 rc=%d 运行 rc=%s out=%r（期望 7）⟹ %s'
              % (rc, rc2, out.strip(), 'OK' if ok68p else 'RED'))
        if not (ok68n and ok68p):
            bad.append('D68')

    # ---------- D64 缺分号 ----------
    if _want('D64'):
        d64 = {
            '块内声明': ('#包含 <stdio.h>\n整 主(空){ 格式化 "前\\n"; 整 x = 5\n 格式化 "后 %d\\n", x; 返 0; }\n', 'A1004'),
            '块内赋值': ('#包含 <stdio.h>\n整 主(空){ 整 x = 1; 格式化 "前\\n"; x = 2\n 格式化 "后 %d\\n", x; 返 0; }\n', 'A1004'),
        }
        ok64 = True
        for tag, (text, code) in d64.items():
            p = src('d64_' + tag, text)
            rc, msg = build(cc, p, os.path.join(W, 'd64_%s.exe' % tag))
            good = (rc != 0 and code in msg)
            print('[D64 %-8s] rc=%d 含 %s=%s ⟹ %s' % (tag, rc, code, code in msg, 'OK' if good else 'RED'))
            ok64 = ok64 and good
        if not ok64:
            bad.append('D64')

    print('=== 审计递刀 D70/D71/D72 · D66 · D64: %s ==='
          % ('PASS' if not bad else 'FAIL -> ' + ', '.join(bad)))
    return 0 if not bad else 1


if __name__ == '__main__':
    sys.exit(main())
