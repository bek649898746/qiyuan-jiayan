#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_ledger_blocked —— 台账 blocked 条目的现状闸（复现「仍未落地」）。

每条 blocked 都对应一个**尚未实现的特征**（IR 打通 / 宏三笔债 / MCP / 知识图谱 /
IR 接管 / RV64 / P23 / 产物体积 / OpenSSF）。本脚本按特征做存在性断言：
特征不存在 ⟹ 打印 GAP 并 rc=1（blocked 的 `repro_expect: red`）。

用法: python scripts/check_ledger_blocked.py
"""
import io
import os
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'srclib_jiayan', 'qcc_zh.jy')

# (ID, 人读名, 判定函数)
def _117_implicit_int(s, d):
    """ORD-20261104-117 的**剩余缺口**（2026-11-06 虾米 收窄后）= 「未知类型名 ⟹ 隐式 int」静默。

    收窄依据（实测，非推测）：
      · P1「两处代码驻宏抽成函数」—— **已证伪**：`跳过于` / `型词尺寸壹` 的宏体直接读写
        调用方的 6 个局部（词位 / 新尺寸 / 新基 / 指元素二 / 字段浮待），抽 `静` 函数不可能等义；
        源内 R558/R559 注释已记「417/418 抽函数后解析期行为变更」。
        ⟹ 「代码驻宏已清零」这个判据**永远不可能成立**，已替换为 `check_macro_code_resident.py`
        （要求每条代码驻宏都有「为何不可抽」的记录；现 2 条均有据、判绿）。
      · P3（D1046 否定自同宏）已修 —— 台账 ORD-20261104-104 = fixed。
      · P4 复现：`未知类型 x = 5;` 编译 rc=0 且静默按 int（返回 5）；`_Atomic` / `_Bool` /
        `inline` / `限制` / `双精度` 五种**无名关键字形态实测全部正确**（3 / 1 / 5 / 7 / 2）。

    故本函数做**行为复现**而不是关键字 grep：编译器若仍接受未知类型名 ⟹ GAP（等语义裁定）。
    """
    import os
    import shutil
    import subprocess
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import qcc_tmp                        # ORD-20261102-90: 临时目录统一走 qcc_tmp（不许裸 tempfile）
    cc = os.path.join(ROOT, 'qcc_zh_v2.exe')
    if not os.path.exists(cc):
        return False                      # 无编译器 ⟹ 不冒充「已落地」
    d = qcc_tmp.tmp_dir('qcc_blk117')
    try:
        p = os.path.join(d, 'p.c')
        io.open(p, 'w', encoding='utf-8', newline='\n').write(
            '未知类型 x = 5;\nint main(void){ return x; }\n')
        r = subprocess.run([cc, p, '-o', os.path.join(d, 'p.exe')],
                           capture_output=True, timeout=180)
        return r.returncode != 0          # 拒绝 ⟹ 缺口已收口
    except Exception:
        return False
    finally:
        shutil.rmtree(d, ignore_errors=True)


def _133_thread_body(s, d):
    """ORD-20261107-133：A3120 的**线程体可达范围**（施工单 ③ 的第二段）是否已落地。

    行为复现：线程体函数里裸写文件级全局，编译器若报 A3120 ⟹ 已落地；不报 ⟹ GAP（等口径裁定）。
    """
    import os
    import shutil
    import subprocess
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import qcc_tmp
    cc = os.path.join(ROOT, 'qcc_zh_v2.exe')
    if not os.path.exists(cc):
        return False
    d = qcc_tmp.tmp_dir('qcc_blk133')
    try:
        p = os.path.join(d, 'p.c')
        io.open(p, 'w', encoding='utf-8', newline='\n').write(
            'int 计 = 0;\n'
            'void 累计加(void *a) { (void)a; 计 = 计 + 1; }\n'
            'int main(void){ 结构化并发 失败即停 { 线程 t 跑 累计加, 0; } return 计; }\n')
        r = subprocess.run([cc, p, '-o', os.path.join(d, 'p.exe')],
                           capture_output=True, timeout=180)
        out = (r.stdout + r.stderr).decode('utf-8', 'replace')
        return r.returncode != 0 and 'A3120' in out
    except Exception:
        return False
    finally:
        shutil.rmtree(d, ignore_errors=True)


CHECKS = [
    ('ORD-20261104-108', 'IR 打通路线（IR pass 读端）', lambda s, d: 'IR 读端' in s),
    ('ORD-20261107-133', 'A3120 线程体可达范围', _133_thread_body),
    ('ORD-20261103-97', '甲言 MCP 接口（协议字段映射）', lambda s, d: 'MCP' in s),
    ('ORD-20261103-99', '知识图谱 AI 查询接口', lambda s, d: '知识图谱查询' in s),
    ('ORD-20261103-102', 'IR 接管产物生成（S0→S4）', lambda s, d: 'IR 接管' in s),
    ('ORD-20261102-88', 'Linux x64 原生支持（RV64/后端矩阵）', lambda s, d: 'RV64' in s),
    ('ORD-20261102-87', 'P23 平台接口预留', lambda s, d: 'P23' in s),
    ('ORD-20261102-89', '产物体积分析 + 按需裁入', lambda s, d: '按需裁入' in s),
    ('ORD-20261102-92', 'OpenSSF 银牌可做部分', lambda s, d: 'OpenSSF' in s),
]


def main():
    src = io.open(SRC, 'rb').read().decode('utf-8', 'surrogateescape')
    docs = ''
    for n in ('README.md', 'llms.txt', '_LONG_MARCH_MASTER.md'):
        p = os.path.join(ROOT, n)
        if os.path.exists(p):
            docs += io.open(p, 'rb').read().decode('utf-8', 'surrogateescape')
    gaps = []
    for rid, name, fn in CHECKS:
        try:
            ok = fn(src, docs)
        except Exception:
            ok = False
        print('[%s] %-12s %s' % ('PASS' if ok else 'GAP', rid, name))
        if not ok:
            gaps.append(rid)
    if gaps:
        print('=== 台账 blocked 现状: 仍有 %d 条未落地 -> %s ===' % (len(gaps), gaps))
        return 1
    print('=== 台账 blocked 现状: 全部落地 ===')
    return 0


if __name__ == '__main__':
    sys.exit(main())
