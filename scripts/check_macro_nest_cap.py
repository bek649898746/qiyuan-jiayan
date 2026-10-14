# -*- coding: utf-8 -*-
"""判据器: 甲言 原生 `宏` 展开 嵌套门槛 与 **承载数组** 是否同宽 (五处同步) + 阈值边界 + 口径。

背景 (SF-5/SF-6/SF-8 · 2026-10-15 虾米):
  第 3848d2e8 把 宏展开 嵌套门槛 24→64, 只改了 **判定点** (`半吉 >= 64` / `半吉 < 64`) 与 写入侧
  (`顺转 = 半吉`), 而 **承载数组** 仍 [24]:
      静 整 净列[24]; 静 整 短捌[24]; 静 整 旁六[24][16][2];
  由 `顺转 = 半吉` (半吉 可达 63) ⟹ 嵌套 ≥24 时 **越界写** 净列/短捌 (≤160B) 与 旁六 (≤5120B);
  同族 卫生宏 栈 `卫栈原[32][32][64] / 卫栈新[32][32][80] / 卫栈数[32]` 亦未同步, 而
  `深薄一` 的 `卫深 = 顺转` (可达 63) ⟹ `查卫生` 以 `层 = 卫深-1` **越界读**。
  且 A9010 拒绝消息 仍写「超过上限 24」⟹ 用户按 24 诊断, 实际拒绝点 64。

判据 (三组, 全部 驱动 **在跑的** 编译器, 不重实现被抄对象):
  ① 静态·五处同步 —— 直读主源, 断言 数组声明 / 守卫 / 消息 口径 **三处一致** (无 24 残留)
  ② 动态·阈值边界 —— 深度 63 ⟹ rc 0 且 值=7; 深度 64 ⟹ rc≠0 + `A9010` + 「超过上限 64」
  ③ 动态·原越界面 —— 深度 24 / 30 / 40 / 60 ⟹ 全部 rc 0 且 值=7 (旧 [24] 时 这些 深度 即 越界)

用法:
  python scripts/check_macro_nest_cap.py [<编译器.exe>]        # 默认 .\\qcc_zh_v2.exe
  python scripts/check_macro_nest_cap.py --selftest            # 负向自证: 把判据 打靶 到 [24] 假源 ⟹ 必须 FAIL
退出码: 0 = 全过; 1 = 有 违例 (逐条 打印 违例行)
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stderr.reconfigure(encoding='utf-8', errors='replace')

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'srclib_jiayan', 'qcc_zh.jy')
门槛 = 64

违例 = []


def 记违(条):
    违例.append(条)
    print('  ❌ ' + 条)


def 静态核对(文本, 标签='主源'):
    """① 五处同步: 数组声明 / 判定点 / 守卫 / 拒绝消息 口径 必须 **全部** 与 门槛 同宽。"""
    print('[① 静态·五处同步] %s' % 标签)
    def 需(正, 描述):
        实 = re.findall(正, 文本)
        if 实:
            print('  ✅ %s  (%d 处: %s)' % (描述, len(实), ', '.join(sorted(set(实))[:4])))
        else:
            记违('%s 未命中 模式 %s' % (描述, 正))

    # 承载数组 (第②处)
    需(r'静 整 净列\[\d+\]', '承载数组 净列')
    需(r'静 整 短捌\[\d+\]', '承载数组 短捌')
    需(r'静 整 旁六\[\d+\]\[16\]\[2\]', '承载数组 旁六')
    # 卫生宏栈 (同族 第②处)
    需(r'静 字 卫栈原\[\d+\]\[32\]\[64\]', '卫生栈 卫栈原')
    需(r'静 字 卫栈新\[\d+\]\[32\]\[80\]', '卫生栈 卫栈新')
    需(r'静 整 卫栈数\[\d+\]', '卫生栈 卫栈数')
    # 判定点 (第③处)
    需(r'半吉 >= %d' % 门槛, '判定点 半吉 >= 门槛')
    需(r'半吉 < %d' % 门槛, '判定点 半吉 < 门槛')
    # 卫深 守卫 (第③处·同族)
    需(r'卫深 < %d' % 门槛, '守卫 卫深 < 门槛')
    # 拒绝消息 (第④处)
    需(r'嵌套层数 超过上限 %d' % 门槛, '拒绝消息 口径 == 门槛')

    # 硬断言: 三个 负向 模式 必须 **不存在**
    for 负, 描 in [
        (r'净列\[24\]', '承载数组 净列 残留 [24]'),
        (r'短捌\[24\]', '承载数组 短捌 残留 [24]'),
        (r'旁六\[24\]', '承载数组 旁六 残留 [24]'),
        (r'卫栈原\[32\]', '卫生栈 卫栈原 残留 [32]'),
        (r'卫栈新\[32\]', '卫生栈 卫栈新 残留 [32]'),
        (r'卫栈数\[32\]', '卫生栈 卫栈数 残留 [32]'),
        (r'卫深 < 32', '守卫 残留 卫深 < 32'),
        (r'嵌套层数 超过上限 24', '拒绝消息 残留 口径 24'),
    ]:
        if re.search(负, 文本):
            记违(描)


def 生成(深度):
    """甲言 原生 `宏` 链: L0(x)=x，Lk(x)=L(k-1)(x)。**注意: 必须用 甲言 `宏`, 不是 `#define`**
    （`#define` 走 词法期 函宏引擎 `副组`, 与 `半吉` 无关 ⟹ 深度 200 也不触发）。"""
    链 = ''.join('宏 L%d(x) = L%d(x);\n' % (i, i - 1) for i in range(1, 深度 + 1))
    return ('宏 L0(x) = x;\n' + 链
            + '\n整数 主(空) { 格式化打印("NEST %d\\n", L' + str(深度) + '(7)); 返 0; }\n')


def 动态核对(CC, 深度组, 期望):
    print('[%s] %s' % (期望[0], 期望[1]))
    临时 = tempfile.mkdtemp(prefix='mnc_')
    for 深度 in 深度组:
        源 = os.path.join(临时, 'nest%d.c' % 深度)
        exe = os.path.join(临时, 'nest%d.exe' % 深度)
        open(源, 'w', encoding='utf-8', newline='\n').write(生成(深度))
        r = subprocess.run([CC, 源, '-o', exe], capture_output=True, cwd=ROOT)
        o = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')
        if 期望[0] == '② 动态·阈值边界 放行侧' or 期望[0].startswith('③'):
            # 放行 侧: rc 0 + stdout NEST 7
            值 = ''
            if r.returncode == 0 and os.path.exists(exe):
                rr = subprocess.run([exe], capture_output=True, timeout=60)
                值 = ((rr.stdout or b'') + (rr.stderr or b'')).decode('utf-8', 'replace').strip()
            if r.returncode != 0 or 值 != 'NEST 7':
                记违('深度 %d 应 放行 (rc=0 且 NEST 7), 实得 rc=%d stdout=%r' % (深度, r.returncode, 值))
            else:
                print('  ✅ 深度 %-3d rc=0 NEST 7' % 深度)
        else:
            # 拒绝 侧: rc != 0 + A9010 + 口径
            好 = (r.returncode != 0) and ('A9010' in o) and ('超过上限 %d' % 门槛 in o)
            if 好:
                print('  ✅ 深度 %-3d rc=%d A9010 口径 %d' % (深度, r.returncode, 门槛))
            else:
                记违('深度 %d 应 响亮拒绝 (rc!=0 + A9010 + 口径 %d), 实得 rc=%d 命中A9010=%s 命中口径=%s'
                     % (深度, 门槛, r.returncode, 'A9010' in o, ('超过上限 %d' % 门槛) in o))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('cc', nargs='?', default=os.path.join(ROOT, 'qcc_zh_v2.exe'))
    ap.add_argument('--selftest', action='store_true', help='负向自证: 判据 必须 能 对 [24] 假源 报违')
    a = ap.parse_args()

    文本 = open(SRC, 'rb').read().decode('utf-8-sig', errors='replace')

    if a.selftest:
        print('=== 负向自证: 把 主源 文本 回退 到 [24] 旧规格 ⟹ ① 必须 报违 ===')
        旧 = (文本.replace('净列[64]; 静 整 短捌[64]; 静 整 旁六[64][16][2]',
                           '净列[24]; 静 整 短捌[24]; 静 整 旁六[24][16][2]')
                  .replace('若 (卫深 < 64)', '若 (卫深 < 32)')
                  .replace('嵌套层数 超过上限 64', '嵌套层数 超过上限 24'))
        if 旧 == 文本:
            print('  ❌ 自证 打靶 未生效 (模式 未命中) ⟹ 判据 可能 空转'); return 1
        静态核对(旧, '假源 [24] 旧规格')
        if not 违例:
            print('  ❌ 负向自证 失败: 旧规格 未 报违 ⟹ 判据 空转'); return 1
        print('  ✅ 负向自证 通过: 旧规格 报违 %d 条 ⟹ 判据 非空转' % len(违例))
        return 0

    print('判据器: 宏展开 嵌套门槛 与 承载数组 同宽 (门槛 = %d)' % 门槛)
    print('编译器: %s' % a.cc)
    if not os.path.exists(a.cc):
        print('  ❌ 找不到 编译器 %s' % a.cc); return 1
    静态核对(文本)
    if os.path.exists(a.cc):
        动态核对(a.cc, [63], ('② 动态·阈值边界 放行侧', '深度 63 = 门槛内 最后 一档 ⟹ 必须 放行'))
        动态核对(a.cc, [64, 65, 70], ('② 动态·阈值边界 拒绝侧', '深度 >= 门槛 ⟹ 必须 响亮 A9010 + 口径一致'))
        动态核对(a.cc, [24, 30, 40, 60], ('③ 动态·原越界面[22]', '深度 24/30/40/60 = 旧 [24] 的 越界区 ⟹ 现 必须 安全放行'))

    print()
    if 违例:
        print('==== 违例 %d 条 ====' % len(违例))
        for v in 违例:
            print('  - ' + v)
        return 1
    print('==== 全绿 (五处同步 + 阈值边界 + 原越界面) ====')
    return 0


if __name__ == '__main__':
    sys.exit(main())
