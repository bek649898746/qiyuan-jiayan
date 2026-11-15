#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_ir_registry.py —— IR 形态登记表判据（判据 A）

为什么要有它（启元 2026-11-11 施工单·判据 A）：
    一批 IR-BACKEND-UNIMPL 报错都写着「登记表见 scripts/check_ir_registry.py」，
    而该文件长期不存在 ⟹ 「完整打通」没有尺子：打不通报成功、打通了报不出。

本脚本做三件事：
  1. 从 srclib_jiayan/qcc_zh.jy **静态提取**全部形态名
     （`IR形态名()` 分类器的返回值 + 所有 `形态=` 报错字面量）；
  2. 与 scripts/ir_registry.json 对账：源码里有、表里没有 ⟹ 未登记；
  3. `--summary` 打印「已登记 N / 未登记 M」与三态计数。

status 语义：
  implemented = IR 后端已有发射路径（实测未触发拒收）
  refused     = 有意拒收（不变量破坏类，永不入 IR，必须写理由）
  pending     = 尚未打通（UNIMPL 可达）

用法：
    python scripts\\check_ir_registry.py --summary
    python scripts\\check_ir_registry.py --init          # 补齐登记表骨架（保留已有条目）
    python scripts\\check_ir_registry.py --allow-pending # 只卡「未登记」

退出码：0 = 通过；1 = 有未登记或有 pending（默认）；2 = 环境问题
"""
import argparse
import json
import os
import re
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'srclib_jiayan', 'qcc_zh.jy')
REG = os.path.join(ROOT, 'scripts', 'ir_registry.json')


def read_src():
    if not os.path.exists(SRC):
        print(f'[环境] 找不到主源: {SRC}')
        sys.exit(2)
    with open(SRC, encoding='utf-8', errors='replace') as f:
        return f.read()


def _match_block(src, start):
    """从 start 处的 '{' 起做括号配平，返回块体。"""
    depth = 0
    i = start
    n = len(src)
    while i < n:
        c = src[i]
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                return src[start + 1:i]
        i += 1
    return src[start + 1:]


def extract_form_names(src):
    """提取全部形态名：分类器返回值 + 报错字面量。"""
    names = set()

    # 1) IR形态名() 分类器的 返 "..." 字面量
    m = re.search(r'IR形态名\s*\([^)]*\)\s*\{', src)
    if m:
        body = _match_block(src, m.end() - 1)
        for nm in re.findall(r'返\s*"([^"]+)"', body):
            names.add(nm)

    # 2) 所有 `形态=` 报错字面量（跳过 %s 动态插值）
    for nm in re.findall(r'形态=([^\s"%\\]+)', src):
        nm = nm.strip('，,；;（）()')
        if nm:
            names.add(nm)

    # 2b) 不带 `形态=` 前缀的 UNIMPL 报错（如 `IR-BACKEND-UNIMPL: 存储形态未支持 ...`）
    for nm in re.findall(r'IR-BACKEND-UNIMPL:\s*([^\s%"\\]+)', src):
        nm = nm.strip('，,；;（）():')
        if nm:
            names.add(nm)

    # 3) 归一化带 %d 的形态名
    norm = set()
    for nm in names:
        nm = re.sub(r'%d', 'N', nm)
        if nm not in ('%s', 'N'):
            norm.add(nm)
    return sorted(norm)


def load_registry():
    if not os.path.exists(REG):
        return {}
    with open(REG, encoding='utf-8') as f:
        data = json.load(f)
    return data.get('形态', data)


def save_registry(forms):
    data = {
        '_meta': {
            '判据': 'scripts/check_ir_registry.py',
            '说明': 'implemented=已有发射路径 / refused=有意拒收(须理由) / pending=未打通',
        },
        '形态': forms,
    }
    with open(REG, 'w', encoding='utf-8') as f:
        json.dump(data, f, ensure_ascii=False, indent=2, sort_keys=True)
        f.write('\n')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--summary', action='store_true')
    ap.add_argument('--init', action='store_true')
    ap.add_argument('--allow-pending', action='store_true')
    a = ap.parse_args()

    src = read_src()
    forms = extract_form_names(src)
    reg = load_registry()

    if a.init:
        changed = 0
        for nm in forms:
            if nm not in reg:
                reg[nm] = {'status': 'pending', 'reason': '', 'evidence': ''}
                changed += 1
        save_registry(reg)
        print(f'登记表已补齐：新增 {changed} 条，共 {len(reg)} 条 -> {REG}')

    missing = [nm for nm in forms if nm not in reg]
    extra = [nm for nm in reg if nm not in forms]
    by = {'implemented': [], 'refused': [], 'pending': [], '未登记': missing}
    for nm in forms:
        st = (reg.get(nm) or {}).get('status', 'pending')
        by.setdefault(st, []).append(nm)

    if a.summary:
        print('=' * 72)
        print('  IR 形态登记表（判据 A）')
        print('=' * 72)
        print(f'  源码形态名总数      : {len(forms)}')
        print(f'  已登记              : {len(forms) - len(missing)}')
        print(f'  未登记              : {len(missing)}')
        print(f'    · implemented     : {len(by.get("implemented", []))}')
        print(f'    · refused         : {len(by.get("refused", []))}')
        print(f'    · pending         : {len(by.get("pending", []))}')
        if missing:
            print('\n  --- 未登记（源码有、表里无）---')
            for nm in missing:
                print(f'    {nm}')
        if by.get('pending'):
            print('\n  --- pending（尚未打通）---')
            for nm in by['pending']:
                print(f'    {nm}')
        if by.get('refused'):
            print('\n  --- refused（有意拒收）---')
            for nm in by['refused']:
                r = (reg.get(nm) or {}).get('reason', '')
                print(f'    {nm}  —— {r or "【缺理由】"}')
        if extra:
            print(f'\n  [提示] 表里有、源码已无: {len(extra)} 条（可清理）')

    bad_pending = [] if a.allow_pending else by.get('pending', [])
    refused_no_reason = [nm for nm in by.get('refused', []) if not (reg.get(nm) or {}).get('reason')]

    ok = not missing and not bad_pending and not refused_no_reason
    if not a.summary:
        print(f'已登记 {len(forms) - len(missing)} / 未登记 {len(missing)} / pending {len(by.get("pending", []))}')
    print()
    if ok:
        print(f'[PASS] 形态登记表：已登记 {len(forms) - len(missing)} 项，未登记 0，pending 0')
        return 0
    msgs = []
    if missing:
        msgs.append(f'未登记 {len(missing)}')
    if bad_pending:
        msgs.append(f'pending {len(bad_pending)}')
    if refused_no_reason:
        msgs.append(f'refused 缺理由 {len(refused_no_reason)}')
    print(f'[FAIL] 形态登记表：{" / ".join(msgs)}')
    return 1


if __name__ == '__main__':
    sys.exit(main())
