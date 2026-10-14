# -*- coding: utf-8 -*-
"""判据器: **用户行号口径** 两投影同族 —— DWARF `decl_line` 与 括号配对诊断 `行 %d`。

病灶（结构性，2026-10-15 定位）:
  `剩伴 = 1 + (在 **宏展开后** 文本的 前 `串双`(展开前运行库字节数) 字节里的换行数)` ——
  拿「展开前的字节长度」切「展开后的文本」⟹ 两套坐标系混用 ⟹ 展开使运行库段变短时 `剩伴` 多算
  用户源开头若干行 ⟹ 所有「词法之后」的行号（DWARF decl_line / 括号配对诊断 / 发错误 …）系统性偏小；
  偏多少取决于用户源前几行（实测 −1 / −2 / −5）。而 `__LINE__` 在词法期取值（`剩伴==0` ⟹ 退回 `反进`）故恰好正确。

修法（单一权威，量在最终文本上 ⟹ 免疫一切文本变换）:
  ① 前插时在 **运行库与用户源之间** 发一行内部边界指令 `#行界`（与 `#line` 同族，指令可穿过展开器）；
  ② 词法器在**最终文本**上遇到它 ⟹ `用户段始行 = 当前行`（该点 `当前行` == 用户第 1 行绝对号 − 1）；
  ③ 全部消费点改走 `行基() = 用户段始行 ? 用户段始行 : (剩伴 ? 剩伴 : 反进)`（后者仅作无运行库路径兜底）。

判据（三条，全部驱动 **在跑的** 编译器）:
  ① 结构: 哨兵发射 + `#行界` 识别 + 标定 + `行基()` 唯一权威（原始公式全源只准 1 次 = 行基 体内）
  ② 投影① DWARF: 用户函数 `decl_line` == 真源行
  ③ 投影② 括号配对: 受控矩阵 4 种行前填充 × 目标行 {1,2,3,4,6} ⟹ 诊断 `行 N` 必须 == 真源行

用法:
  python scripts/check_lineno.py [<引擎.exe>]     # 默认 ./qcc_zh_v2.exe
  python scripts/check_lineno.py --selftest       # 对 **修复前引擎** (qcc_zh_th.exe) 跑 ②③ ⟹ 必须报违
⚠️ `check_dwarf289.py` **忽略 argv**，走 find_qcc()（`QCC_TEST_BIN` → v7..v1）⟹ 本判据器以 **环境变量** 传引擎。
退出码: 0 = 全过；1 = 有违例
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'srclib_jiayan', 'qcc_zh.jy')
违 = []


def 记(m):
    违.append(m)
    print('  ❌ ' + m)


def 结构核对(文本):
    print('[① 结构·单一权威]')
    for n, d, 期望 in ((len(re.findall(r'复制内存\(元二一 \+ 旁戊 \+ 1, "#行界\\n", 8\)', 文本)),
                        '前插器 发 `#行界` 哨兵', 1),
                       (len(re.findall(r'行界标 = !比前n\(串值 \+ 元, "行界", 6\)', 文本)), '词法器 识别 `#行界`', 1),
                       (len(re.findall(r'用户段始行 = 当前行;', 文本)), '词法器 标定 `用户段始行`', 1),
                       (len(re.findall(r'静 整 行基\(空\)', 文本)), '唯一权威 `行基()` 定义', 1),
                       (len(re.findall(r'行基\(\)', 文本)), '消费点 走 `行基()`', None),
                       (len(re.findall(r'\(剩伴 \? 剩伴 : 反进\)', 文本)), '原始公式（只准 1 次=行基体内）', 1)):
        if 期望 is None:
            print('  ✅ %s: %d 处' % (d, n))
            if n < 8:
                记('%s 仅 %d 处（疑似 漏改消费点）' % (d, n))
        elif n == 期望:
            print('  ✅ %s: %d' % (d, n))
        else:
            记('%s 期望 %d 实得 %d' % (d, 期望, n))


填 = {'空行': lambda n: '\n' * n, 'ASCII注释': lambda n: ''.join('/*x*/\n' for _ in range(n)),
      '中文注释': lambda n: ''.join('/* 注释 */\n' for _ in range(n)),
      '语句行': lambda n: ''.join('整 v%d = %d;\n' % (i, i) for i in range(n))}


def dwarf核对(cc):
    print('[② 投影① · DWARF decl_line]')
    env = dict(os.environ)
    env['QCC_TEST_BIN'] = os.path.abspath(cc)      # ⚠️ 必须 env: 检查器 走 find_qcc, 忽略 argv
    r = subprocess.run([sys.executable, os.path.join(ROOT, 'scripts', 'check_dwarf289.py')],
                       capture_output=True, timeout=900, cwd=ROOT, env=env)
    o = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')
    for l in o.split('\n'):
        if '[FAIL]' in l:
            print('  ' + l.strip()[:120])
    if r.returncode == 0:
        print('  ✅ %s: rc=0（无 [FAIL]）' % os.path.basename(cc))
    else:
        记('DWARF: %s rc=%d ⟹ decl_line 与 真源行 不符' % (os.path.basename(cc), r.returncode))


def 括号配对矩阵(cc):
    print('[③ 投影② · 括号配对 诊断行号 受控矩阵]')
    错 = 总 = 0
    for name, fn in 填.items():
        行 = []
        for n in (0, 1, 2, 3, 5):
            L = n + 1
            p = os.path.join(tempfile.gettempdir(), 'chk_ln_%s_%d.c' % (name, L))
            open(p, 'w', encoding='utf-8', newline='\n').write(fn(n) + '整 a = 1);\n')
            r = subprocess.run([cc, p, '-o', os.path.join(tempfile.gettempdir(), 'chk_ln.exe')],
                               capture_output=True, timeout=180, cwd=ROOT)
            o = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')
            m = re.search(r'行 (\d+)', o)
            R = int(m.group(1)) if m else None
            总 += 1
            ok = (R == L)
            if not ok:
                错 += 1
            行.append('L%d→%s%s' % (L, R, '' if ok else '(✗)'))
        print('  %-8s %s' % (name, '  '.join(行)))
    if 错:
        记('括号配对 矩阵 %d/%d 格 行号 != 真源行' % (错, 总))
    else:
        print('  ✅ 全部 %d 格 行号 == 真源行' % 总)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('cc', nargs='?', default=os.path.join(ROOT, 'qcc_zh_v2.exe'))
    ap.add_argument('--selftest', action='store_true')
    a = ap.parse_args()
    结构核对(open(SRC, 'rb').read().decode('utf-8-sig', errors='replace'))
    if a.selftest:
        旧 = os.path.join(ROOT, 'qcc_zh_th.exe')
        print('\n=== 负向自证: 对 **修复前引擎** %s ⟹ 必须 报违 ===' % os.path.basename(旧))
        if not os.path.exists(旧):
            print('  ⚠️ 缺 修复前引擎 ⟹ 跳过'); return 0
        n0 = len(违)
        dwarf核对(旧)
        括号配对矩阵(旧)
        if len(违) > n0:
            print('  ✅ 负向自证 通过: 修复前引擎 报违 %d 条 ⟹ 判据 非空转' % (len(违) - n0))
            return 0
        print('  ❌ 负向自证 失败: 修复前引擎 未报违 ⟹ 判据 空转'); return 1
    dwarf核对(a.cc)
    括号配对矩阵(a.cc)
    print()
    if 违:
        print('==== 违例 %d 条 ====' % len(违))
        for v in 违:
            print('  - ' + v)
        return 1
    print('==== 全绿（结构 + DWARF + 括号配对 两投影）====')
    return 0


if __name__ == '__main__':
    sys.exit(main())
