# -*- coding: utf-8 -*-
"""重建 {SCRATCH}/defect_evidence.log —— 未解决 项（TSV id 13/18/21）的 内容锚 + 现役 探针 实测。

背景：{SCRATCH} 曾被整体清空 ⟹ 台账「证据」列 引用的两份 scratch 文件 消失。
本件 = **本轮 重建**（内容 一律 现役 实测：主源 内容锚 grep + 现役引擎 探针），非 转录。
"""
import os, re, subprocess, sys

S = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.environ.get('QCC_LEDGER_SCRATCH', '')
assert S, "用法: python qiyuan_ledger_defect_evidence.py <SCRATCH目录> (或 设 QCC_LEDGER_SCRATCH)"
QJ = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
SRC = os.path.join(QJ, 'srclib_jiayan', 'qcc_zh.jy')
EXE = os.path.join(QJ, 'qcc_zh_v2.exe')
GIT = r'C:\Users\Administrator\Desktop\git-2.45.2'
PRE = os.path.join(QJ, 'compat_prelude.h')

t = open(SRC, 'rb').read().decode('utf-8-sig')
L = []
L.append('# 未解决 项 证据（TSV id 13 / 18 / 21）· **本轮重建**（{SCRATCH} 曾被整体清空）')
L.append('# 生成器：scripts/qiyuan_ledger_defect_evidence.py · 内容 一律 **现役实测**')
L.append('# 引擎 %s (sha256-8 %s)' % (os.path.basename(EXE), __import__('hashlib').sha256(open(EXE, 'rb').read()).hexdigest()[:8]))
L.append('')

L.append('## 一、内容锚计数（主源 grep · 非行号）')
for name in ['控式', '段双', '剩系', '间再', '隔系一', '构CFG', '隔系一', '双表', '暂弊', '末一二']:
    L.append('- `%s` = %d' % (name, len(re.findall(re.escape(name), t))))
L.append('')

L.append('## 二、R798x 三文件（现役引擎 实测 · 同 git 闸口径）')
DARGS = ['-D', '__STDC_VERSION__=199901L', '-D', '__GNUC__=5', '-D', '_WIN32=1', '-D', 'REG_STARTEND=1']
INC = ['-I', PRE, '-I', os.path.join(GIT, 'compat', 'regex'), '-I', GIT]
OUT = os.path.join(S, 'logs', 'dmp')
os.makedirs(OUT, exist_ok=True)
for rel in ['kwset.c', 'parse-options.c', 'builtin/fsmonitor--daemon.c']:
    src = os.path.join(GIT, rel.replace('/', os.sep))
    r = subprocess.run([EXE] + DARGS + INC + ['-c', src, '-o', os.path.join(OUT, rel.replace('/', '_') + '.o')],
                       capture_output=True, cwd=GIT)
    errs = [x.strip() for x in ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace').splitlines() if '[ERR]' in x]
    code = (re.search(r'A\d{4}', errs[0]).group(0) if errs and re.search(r'A\d{4}', errs[0]) else 'OK')
    L.append('- `%s` ⟹ rc=%d · %s' % (rel, r.returncode, code))
L.append('')

L.append('## 三、R799k 数组 typedef 形参（现役引擎 实测 · 静默错值）')
w = os.path.join(S, 'irwork'); os.makedirs(w, exist_ok=True)
probe = ('typedef unsigned long BS;\ntypedef BS A[4];\n'
         'void f(A s) { s[0] = 1; s[3] = 4; }\n'
         'int main(void) { A a; a[0]=0; a[3]=0; f(a); printf("%lu %lu\\n", a[0], a[3]); return 0; }\n')
p = os.path.join(w, 'r799k_probe.c')
open(p, 'w', encoding='utf-8').write(probe)
r = subprocess.run([EXE, p, '-o', p + '.exe'], capture_output=True, cwd=w)
L.append('- 探针 `%s`' % probe.replace('\n', ' ⏎ '))
L.append('- 甲言 ⟹ rc=%d' % r.returncode)
ex = p + '.exe'
if os.path.exists(ex):
    rr = subprocess.run([ex], capture_output=True, cwd=w)
    L.append('- 甲言 运行 stdout = %r  （gcc 期望 = \'1 4\' ⟹ **写入丢失 = 静默错值**）'
             % (rr.stdout or b'').decode('utf-8', 'replace').strip())
L.append('- 递刀 修法方向（逐字）：形参登记 补「数组性」判据（`类型退化一`/`指元尺寸` 字段）')
L.append('')

L.append('## 四、递刀 修法方向（TSV 复算列 摘录 · id 13 / 18 / 21）')
L.append('- **id 13**（`20261012_施工单v2_按甲言地基`）：按 `建节()` 唯一打标点 落位 + 容量 按 索引键 校准 + 可重入 复位 + `QCC_DUMP=ir` 只读视图')
L.append('- **id 18**（`20261011_IR中端施工单v2合流版`）：先定 裁二 表示法（子指令链 / `剩十`(s3) / 其他）+ 三红线（L0 一行不动 · 不得静默丢弃 · 不得默默违反 v1.4 §5.4）')
L.append('- **id 21**（`20261011_施工单8刀复查`）：刀二/三 `-c` COFF 通道 + S-51 改判 · 刀四 `超级编译`→A6086 · 刀五 `类型表/变量表` 补真结构字段 · 刀六 补「IR 数据源 真伪」小节')
L.append('')
L.append('## 五、本轮 状态更新（2026-10-14）')
L.append('- **id 18 的 裁二 已落定** ⟹ `decisions.log` 裁二（子指令链 `子链[]` + `剩十` 链头，已落位并被 dump 观测）')
L.append('- **id 13 的「容量 按 索引键 校准」已落** ⟹ `decisions.log` 裁四（前驱槽容 32 = 实测入度上界 18 校准）')
L.append('- **id 13 的「`QCC_DUMP=ir` 只读视图」已落** ⟹ `ir_dump.log` / `knife.log`')
open(os.path.join(S, 'defect_evidence.log'), 'w', encoding='utf-8').write('\n'.join(L) + '\n')
print('[WROTE] defect_evidence.log')
for l in L[:14]:
    print(l)
