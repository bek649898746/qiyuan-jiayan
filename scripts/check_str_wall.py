# -*- coding: utf-8 -*-
"""check_str_wall —— **字符串字面量墙**（台账 D53 · 2026-10-25 结案）

历史：审计现场 `D:\\_opt_proto\\_str3\\`，13,500 条字面量，13,500×10（162 KB 源）可编，
13,500×18（297 KB 源）**静默 0xC0000005**。边界非单调（108/135/148/162/180/198/243/695 KB
混合 ✓✗✗✓），故「字节总量」不是触发条件 —— 这一点曾被误判过两次（提高配额 ⟹ 自举 v2 断）。

根因（R842/R843，2026-10-25，用 `scripts/addr2func.py` + gdb 定位）：
  `外陆一()` / `标劣()` 把字面量写进 `.data` 数据缓冲 `全伴`，扩容是 `下退 += 256` 的**线性步进**；
  而 `重分配` 在 bump 堆上**不能原地扩**（`全伴` 不是堆顶 ⟹ 走「新块 + 整块拷贝」，旧块永久泄漏，
  见 `srclib/qcc_rt.c` 的 realloc）。⟹ 容量 C 从 0 走到 S 要 S/256 步，每步拷贝并泄漏 ~C 字节
  ⟹ **时间 O(S²)、堆耗 O(S²)**。242 KB 源实测堆耗 > 512 MiB 竞技场 ⟹ realloc 内联 memcpy 写到
  竞技场外的未映射页 ⟹ 静默段错误。gdb 帧：`realloc+536`（`mov [r8],al`）← `外陆一+795/1057`。
修法：① `全伴留()` 几何扩容（≥4 KiB 起，按需 ×2）⟹ 时间/堆耗 均 O(S)；
      ② 容量级护栏（>256 MiB ⟹ A6113 响亮拒绝）取代原来的**输入形态配额**（条数>12,000 且
         字节>120,000）—— 后者会误拒合法程序，已在 R843 删除。

本判据四条（全过才 rc=0）：
  ① 162 KB / 297 KB / 567 KB / 1.1 MB 四档**全部** rc=0（②就是当年崩的那档）
  ② 每档产物**真跑**且输出等于该档首个字面量（编得过 ≠ 编对了）
  ③ 反回归：源码里**不得**再出现输入形态配额（`字面字节 > 120000`）
  ④ 反回归：容量护栏仍在（`全伴留` 含 `0x10000000` 与 `A6113`）

用法: python scripts/check_str_wall.py [--cc build/qcc_zh_v1.exe]
退出码: 0 = 全过；1 = 有红；2 = 环境未就绪（不判绿）
"""
import argparse
import io
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIVE = r'D:\_opt_proto\_str3'
SRC_JY = os.path.join(ROOT, 'srclib_jiayan', 'qcc_zh.jy')

# (名字, 条数, 每条字节) —— 条数固定 13,500，只改每条长度 ⟹ 唯一变量是「单条长度 / 数据区总量」
SCALES = [('m13500_10', 13500, 8), ('m13500_20', 13500, 18),
          ('m13500_40', 13500, 34), ('m13500_80', 13500, 66)]


def gen(path, n, per):
    items = ['"%s"' % (('s%06d' % i) + 'x' * max(0, per - 7)) for i in range(n)]
    src = ('\ufeff#包含 <stdio.h>\n静 字 *串表测试[%d] = { ' % n) + ', '.join(items) + \
          ' };\n静 整 主(空) { printf("%s\\n", 串表测试[0]); 返 0; }\n'
    io.open(path, 'w', encoding='utf-8', newline='\n').write(src)
    return os.path.getsize(path), items[0][1:-1]


def expected_first(path):
    """从源里取第一个字面量的内容（判据不信任生成器，直接读源）"""
    s = io.open(path, encoding='utf-8').read()
    i = s.index('{')
    j = s.index('"', i)
    k = s.index('"', j + 1)
    return s[j + 1:k]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(ROOT, 'build', 'qcc_zh_v1.exe'))
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    if not os.path.exists(cc):
        print('[FAIL] 环境未就绪: 找不到编译器 %s（本判据未执行，不判绿）' % cc)
        return 2
    w = tempfile.mkdtemp(prefix='qcc_strwall_')

    ok = True
    for name, n, per in SCALES:
        live = os.path.join(LIVE, name + '.jy')
        if os.path.exists(live):
            src = live
        else:
            src = os.path.join(w, name + '.c')
            gen(src, n, per)
        size = os.path.getsize(src)
        exp = expected_first(src)
        exe = os.path.join(w, name + '.exe')
        r = subprocess.run([cc, '-o', exe, src], capture_output=True, timeout=1800)
        msg = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')
        if r.returncode != 0:
            crash = r.returncode in (-1073741819, 3221225477)
            print('[%6d KB] rc=%d %s' % (size // 1024, r.returncode,
                                         'SILENT-CRASH' if crash else 'FAIL | ' + msg.strip()[:160]))
            ok = False
            continue
        run = subprocess.run([exe], capture_output=True, timeout=300)
        got = (run.stdout or b'').decode('utf-8', 'replace').strip()
        good = (run.returncode == 0 and got == exp)
        print('[%6d KB] rc=0 产物输出=%s（期望 %s）⟹ %s'
              % (size // 1024, ('[%s]' % got) if len(got) < 40 else 'len=%d' % len(got),
                 ('[%s]' % exp) if len(exp) < 40 else 'len=%d' % len(exp),
                 'OK' if good else 'FAIL'))
        ok = ok and good

    src_text = io.open(SRC_JY, encoding='utf-8').read()
    no_quota = '字面字节 > 120000' not in src_text
    print('[③ 反回归 ] 输入形态配额已删除（find `字面字节 > 120000`）⟹ %s'
          % ('OK' if no_quota else 'FAIL（配额复活 ⟹ 会误拒合法程序）'))
    guard = ('A6113' in src_text) and ('0x10000000' in src_text) and ('全伴留' in src_text)
    print('[④ 反回归 ] 容量护栏在（全伴留 + 0x10000000 + A6113）⟹ %s'
          % ('OK' if guard else 'FAIL（越界又会变成静默段错误）'))
    ok = ok and no_quota and guard

    print('=== 字符串字面量墙（4 档 × 13,500 条）: %s ===' % ('PASS' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
