# -*- coding: utf-8 -*-
"""T6-4 错构（错误 → IR 坐标）判据器。

判据（真写入 + **与真实 CFG 块表交叉验证**，不接受「定义存在」或「任意非负值」）：
  ① 码生成期错误（未声明标识符）的 `--diagnostics=json` 必须带 `ir` 对象：
     `node`（节点序，>=0）· `block`（CFG 块号，>=0）· `func`（当前函数名，非空）· `table`（错表计数，>=1）。
  ② 交叉验证：用**同结构**对照探针（常量替换未声明名）+ `QCC_DUMP=ir` 读出真实块表，
     断言 `block` == 「node 落在 [始,终一] 的那个块号」。首版 `记错构` 减 1 曾报 377（真实 378），
     本步即为抓该 bug 而设。
  ③ 负向：解析期错误（括号不匹配）**不得**伪造 IR 坐标 —— `ir` 缺失或 `node < 0`。
  ④ 灵敏度：实现前 `ir` 字段不存在 ⟹ ① 必红；块号差 1 ⟹ ② 必红。

用法: python scripts/check_ir_errmap.py [编译器]
退出码: 0 全过 ; 1 有断言失败。
"""
import json
import os
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
CC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'qcc_zh_v2.exe')
OUT = os.path.join(ROOT, 'build', 'ir_errmap')
os.makedirs(OUT, exist_ok=True)

PROBE = 'scripts/probe/ir_errmap_probe.c'
OK_PROBE = 'scripts/probe/ir_errmap_ok_probe.c'
NEG = 'scripts/probe/ir_errmap_parse_neg_probe.c'
RE_BLOCK = re.compile(r'^\s*块\[(\d+)\] 始=(\d+) 终一=(\d+)', re.M)

fails = []


def _env_no_dump():
    return {k: v for k, v in os.environ.items() if k != 'QCC_DUMP'}


def diag(src, tag):
    exe = os.path.join(OUT, tag + '.exe')
    r = subprocess.run([CC, '--diagnostics=json', src, '-o', exe],
                       capture_output=True, text=True, encoding='utf-8', errors='replace',
                       env=_env_no_dump())
    # 诊断 JSON 走 stdout（实测）; 兼容未来切到 stderr ⟹ 两路合并后取 JSON 行
    txt = ((r.stdout or '') + '\n' + (r.stderr or '')).strip()
    for line in reversed(txt.splitlines()):
        line = line.strip()
        if line.startswith('{') and '"diagnostics"' in line:
            try:
                return json.loads(line), r
            except Exception:
                return None, r
    return None, r


def cfg_blocks():
    """用同结构对照探针 + QCC_DUMP=ir 取真实块表 [(块号, 始, 终一)]。"""
    exe = os.path.join(OUT, 'ok.exe')
    env = dict(os.environ)
    env['QCC_DUMP'] = 'ir'
    r = subprocess.run([CC, OK_PROBE, '-o', exe], capture_output=True, text=True,
                       encoding='utf-8', errors='replace', env=env)
    txt = (r.stdout or '') + '\n' + (r.stderr or '')
    blocks = [(int(a), int(b), int(c)) for a, b, c in RE_BLOCK.findall(txt)]
    return blocks, r


# ① 码生成期错误必须带 IR 坐标
d, r = diag(PROBE, 'probe')
if not d:
    fails.append('探针未产出 JSON 诊断 (rc=%s, stdout=%r)' % (r.returncode, (r.stdout or '')[:200]))
else:
    e = d.get('diagnostics', [{}])[0]
    ir = e.get('ir')
    print('诊断 JSON: ' + json.dumps(e, ensure_ascii=False))
    if not isinstance(ir, dict):
        fails.append('码生成期错误缺 `ir` 对象 (实际: %r)' % (ir,))
    else:
        if not (isinstance(ir.get('node'), int) and ir['node'] >= 0):
            fails.append('`ir.node` 非有效节点序: %r' % (ir.get('node'),))
        if not (isinstance(ir.get('block'), int) and ir['block'] >= 0):
            fails.append('`ir.block` 非有效块号: %r' % (ir.get('block'),))
        if not (isinstance(ir.get('func'), str) and ir['func'].strip()):
            fails.append('`ir.func` 为空: %r' % (ir.get('func'),))
        if not (isinstance(ir.get('table'), int) and ir['table'] >= 1):
            fails.append('`ir.table` < 1（错表未真写入）: %r' % (ir.get('table'),))
        if e.get('code') != 'A1001':
            fails.append('探针错误码非 A1001: %r' % (e.get('code'),))

        # ② 与真实块表交叉验证
        blocks, rb = cfg_blocks()
        if not blocks:
            fails.append('QCC_DUMP=ir 未产出块表 (rc=%s)' % (rb.returncode,))
        else:
            node = ir.get('node')
            hit = [b for b in blocks if b[1] <= node <= b[2]]
            if not hit:
                fails.append('节点 %r 不落在任何块区间（块表 %d 块）' % (node, len(blocks)))
            else:
                real = hit[0]
                print('块表交叉验证: node=%d ⟹ 块[%d] 始=%d 终一=%d（报出 block=%r）' % (
                    node, real[0], real[1], real[2], ir.get('block')))
                if ir.get('block') != real[0]:
                    fails.append('block 与真实块表不一致: 报 %r ≠ 块[%d]（node=%d 区间 [%d,%d]）' % (
                        ir.get('block'), real[0], node, real[1], real[2]))

# ③ 解析期错误不得伪造 IR 坐标
#   注: 解析期错误当前走裸文本 `[ERR]`（不经 `发错误`）⟹ 无 JSON 亦合规;
#   若将来并入 `发错误`, 则必须不带有效 node（`ir` 缺失或 node<0）。
d2, r2 = diag(NEG, 'neg')
if d2 is not None:
    e2 = d2.get('diagnostics', [{}])[0]
    ir2 = e2.get('ir')
    if isinstance(ir2, dict) and isinstance(ir2.get('node'), int) and ir2['node'] >= 0:
        fails.append('解析期错误伪造了 IR 坐标: %r' % (ir2,))
elif r2.returncode == 0:
    fails.append('负向探针不应编译成功')

if fails:
    print('FAIL: T6-4 错构 判据')
    for f in fails:
        print('  - ' + f)
    sys.exit(1)
print('PASS: T6-4 错构 —— 码生成期错误带 node/block/func/table；block == 真实块表；解析期不伪造')
