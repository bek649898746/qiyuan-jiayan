# -*- coding: utf-8 -*-
"""check_ir_table_comments —— IR 侧表注释一致性两处的永久闸（施工单 2026-10-27）。

背景：代码是对的，注释在骗人 ——
  ① `方层` 零写点，却没登记「本版无源」 ⟹ 下一个人不知道它该不该接；
  ② `尾午一` 注释写「至今零写点 / 不可校准」，而刀九 (2026-10-14) 已真落位
     （4 写 + 真消费 + 溢出守卫 + QCC_DBG_PASSCAP 负控）⟹ 旧结论会让人不敢校准容量。

判据只读注释行，不改任何逻辑。
"""
import argparse
import os
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(REPO, 'srclib_jiayan', 'qcc_zh.jy')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--file', default=None, help='要检查的 qcc_zh.jy（默认仓库主源；用于修前/修后对照）')
    a = ap.parse_args()
    src = a.file or SRC
    lines = open(src, encoding='utf-8').read().splitlines()
    fails = []

    idx = [i for i, l in enumerate(lines) if '静 整 方层[' in l]
    if len(idx) != 1:
        fails.append('方层 定义行数 = %d（期望 1）' % len(idx))
    else:
        # 注释可跨多行：取定义行起的连续注释块（到 */ 或 8 行）
        blk = lines[idx[0]:idx[0] + 8]
        text = '\n'.join(blk)
        if '本版无源' not in text:
            fails.append('方层 定义块缺「本版无源」登记')
        if '零写点' not in text:
            fails.append('方层 定义块缺「为什么无源」的说明（零写点）')

    idx = [i for i, l in enumerate(lines) if '#define 尾午一 64' in l]
    if len(idx) != 1:
        fails.append('尾午一 定义行数 = %d（期望 1）' % len(idx))
    else:
        blk = lines[idx[0]:idx[0] + 8]
        text = '\n'.join(blk)
        if '已过期' not in text:
            fails.append('尾午一 注释缺「本条已过期」时点标注')
        if '刀九' not in text or '分析注册()' not in text:
            fails.append('尾午一 注释缺「刀九已真落位」的指向（刀九 / 分析注册()）')
        if '至今 零写点' in text:
            fails.append('尾午一 注释仍保留已被推翻的「至今 零写点」结论')

    n = sum(l.count('本版无源') for l in lines)
    if n < 6:
        fails.append('「本版无源」登记数 = %d（期望 >= 6）' % n)

    if fails:
        print('[FAIL] IR 侧表注释一致性: %d 项' % len(fails))
        for f in fails:
            print('   - ' + f)
        return 1
    print('[PASS] IR 侧表注释一致性: 方层已登记 / 尾午一已过期+指向刀九 / 本版无源 %d 处' % n)
    return 0


if __name__ == '__main__':
    sys.exit(main())
