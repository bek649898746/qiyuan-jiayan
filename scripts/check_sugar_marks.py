# -*- coding: utf-8 -*-
"""守卫: 语法糖内 **不得** 手写 `节浮标[...] = 1` 打标。

由来 (2026-09-19 第 133 站 C143):
  `糖后标(块, 0)` 已在每个糖构造收尾被调用 (59 个调用点), 且对 糖内建出的
  下标读节点 / 变量引用节点 的 判据 与 手写 逐字相同 ⟹ 糖内手写打标是 **纯冗余**。
  该站删除 21 处手写并确立: **糖后标() 是糖内浮性打标的唯一出口**。

判据: 糖区内 (糖定义行 起 ~600 行内) 出现 `节浮标[...] = 1` 即视为违规,
      除非命中 WHITELIST (判据来源不同类的糖, 需逐条登记)。

用法: python scripts/check_sugar_marks.py        # exit 1 = 有违规
"""
import io, os, re, sys
sys.stdout.reconfigure(encoding='utf-8')

REP = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(REP, 'srclib_jiayan', 'qcc_zh.jy')

# 白名单: 行号 -> 理由 (判据不是「变量浮性」而是别的来源, 属另一类, 逐条登记)
WHITELIST = {
    14242: '载荷字段糖: 判据是 变载[].类型序 (结构体字段类型), 非 变量是否浮/指浮',
}

RE_SUGAR = re.compile(r'!比较字符串\(词名表\[词位\],\s*"([^"]+)"\)')
RE_MARK = re.compile(r'节浮标\[[^\]]*\]\s*=\s*1')
RE_HELPER = re.compile(r'静 空 糖后标\(')
RE_CALL = re.compile(r'糖后标\(块,\s*0\);')


def main():
    lines = io.open(SRC, encoding='utf-8', newline='').read().split('\n')
    # helper 自身区间 (从 定义 到 函数体结束的 '}' 行)
    hstart = next((i for i, L in enumerate(lines) if RE_HELPER.search(L)), -1)
    hend = -1
    if hstart >= 0:
        for j in range(hstart + 1, min(len(lines), hstart + 60)):
            if lines[j].strip() == '}':
                hend = j
                break

    bad = []
    for i, L in enumerate(lines):
        m = RE_SUGAR.search(L)
        if not m or '词变量' not in L:
            continue
        sugar = m.group(1)
        # 糖 定义 的 判据: 该 词变量 分支 内 必须 有 糖后标(块, 0) 调用 (否则 是 关键字表 / 特性词表 / 变身 路径)
        window = range(i, min(len(lines), i + 600))
        call = next((j for j in window if RE_CALL.search(lines[j])), -1)
        if call < 0:
            continue
        for j in range(i, call + 1):
            if RE_MARK.search(lines[j]) and not RE_HELPER.search(lines[j]):
                if hstart >= 0 and hstart <= j <= hend:
                    continue  # helper 自身
                if (j + 1) in WHITELIST:
                    continue
                bad.append((j + 1, sugar, lines[j].strip()[:120]))

    if bad:
        print('[失败 FAIL] 糖内 手写 节浮标 打标 %d 处 (糖后标 已是唯一出口, 见 第 133 站 C143):' % len(bad))
        for ln, sugar, txt in bad:
            print('  L%-6d [%s] %s' % (ln, sugar, txt))
        print('\n修法: 删掉手写打标, 保证 节点 挂在 块 子树 (深 <= 12) 内即可; 若确需例外, 登记进 WHITELIST 并写明理由。')
        return 1
    print('[通过 PASS] 糖内 无 手写 节浮标 打标 (糖后标 唯一出口 成立; 白名单 %d 条)' % len(WHITELIST))
    return 0


if __name__ == '__main__':
    sys.exit(main())
