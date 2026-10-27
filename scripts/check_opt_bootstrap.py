# -*- coding: utf-8 -*-
"""优化器自举闭环 判据器（T10-2 · plan 验收 3 的 gating 观测）

断言链（三环链 `qcc -S` → `opt_zh` → `asm_zh`）：

  ① 小用例：三环链产物 与 编译器直产 的 rc/stdout 逐字一致，且优化真生效（体积下降）
  ② 编译器本体（`--full`）：用三环链构建编译器 `Zo` ⟹ 它编编译器自身两级
     ⟹ 观察 `v1o == v2o`（优化链下的自举不动点）；并与未优化链产物 `Z6` 比对
  ③ 优化器自身负向用例：不该动的形态必须原样保留

用法: python scripts/check_opt_bootstrap.py [--full] [--cc <exe>] [--work <dir>]
退出码: 0 全过 ; 1 有断言失败。
"""
import io, os, re, sys, subprocess, hashlib, struct

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
CC = 'qcc_zh_v2.exe'
FULL = '--full' in sys.argv
WORK = os.path.join('build', 'opt_boot')
if '--cc' in sys.argv:
    CC = sys.argv[sys.argv.index('--cc') + 1]
if '--work' in sys.argv:
    WORK = sys.argv[sys.argv.index('--work') + 1]
os.makedirs(WORK, exist_ok=True)

if FULL:
    # 编译编译器本体需要更宽松的资源
    pass

OPT = 'opt_zh.exe'
# R821c: 负控入口 —— 指向改动前的 8 类优化器时，分支自检（9 类断言）必须变红。
if '--opt' in sys.argv: OPT = sys.argv[sys.argv.index('--opt') + 1]
if '--asm' in sys.argv: ASM = sys.argv[sys.argv.index('--asm') + 1]
ASM = 'asm_zh.exe'
if '--opt' in sys.argv:          # 允许指向待装的临时构建（用于装前自检）
    OPT = sys.argv[sys.argv.index('--opt') + 1]
if '--asm' in sys.argv:
    ASM = sys.argv[sys.argv.index('--asm') + 1]
SRC = os.path.join('srclib_jiayan', 'qcc_zh.jy')

fails = []


def sh(args, timeout=3600):
    return subprocess.run(args, capture_output=True, timeout=timeout)


def run(exe):
    try:
        r = subprocess.run([exe], capture_output=True, timeout=120)
        return r.returncode, (r.stdout + r.stderr).decode('utf-8', 'replace')
    except subprocess.TimeoutExpired:
        return None, '<timeout>'


def h8(p):
    return hashlib.sha256(io.open(p, 'rb').read()).hexdigest()[:8].upper()


def text_size(p):
    """(.text SizeOfRawData, 文件字节, .text 有效内容长度 = 最后一个非零字节 + 1)
    SizeOfRawData 是 0x1000 对齐值 ⟹ 小额收益会被对齐页掩盖；有效内容长度才是收益真实落点。"""
    d = io.open(p, 'rb').read()
    pe = struct.unpack_from('<I', d, 0x3C)[0]
    n = struct.unpack_from('<H', d, pe + 6)[0]
    o = struct.unpack_from('<H', d, pe + 20)[0]
    b = pe + 24 + o
    for i in range(n):
        q = b + 40 * i
        nm = d[q:q + 8].rstrip(b'\x00').decode('latin1')
        if nm == '.text':
            vsz, va, rsz, ro = struct.unpack_from('<IIII', d, q + 8)
            body = d[ro:ro + rsz]
            j = len(body) - 1
            while j >= 0 and body[j] == 0:
                j -= 1
            return rsz, len(d), j + 1
    return -1, len(d), -1


print('== 优化器自举闭环 判据 (cc=%s full=%s) ==' % (CC, FULL))

# ---------- ① 负向用例：优化器不能动「不该动」的形态 ----------
NEG = os.path.join(WORK, 'neg.asm')
NEG_OUT = os.path.join(WORK, 'neg_out.asm')
NEG_TEXT = (
    '    减栈 8\n'
    '    存栈64 [rsp+0], r0\n'      # 正向: 应变成 压栈 r0
    '    减栈 16\n'
    '    存栈64 [rsp+0], r0\n'      # 负向: 减栈 16 ≠ 8 ⟹ 不动
    '    减栈 8\n'
    '    存栈64 [rsp+8], r0\n'      # 负向: 操作数不是 [rsp+0] ⟹ 不动
    '    取栈64 r0, [rsp+16]\n'
    '    加栈 8\n'                  # 负向: 取栈偏移 16 ⟹ 不动（本判据器针对的 bug）
    '    取栈64 r0, [rsp+0]\n'
    '    加栈 8\n'                  # 正向: 应变 弹栈 r0
    '    存帧32 [rbp-4], r0\n'      # 负向: 不是栈 spill ⟹ 不动
    '    减栈 8\n'
    '    存栈64 [rsp+0], r1\n'      # 回归: r1(=rcx) 1 字节（曾被误判为 2 字节 ⟹ 手写常量错 1；现由汇编器实测）
    '    减栈 8\n'
    '    存栈64 [rsp+0], r8\n'      # 回归: r8 走 REX ⟹ 2 字节（现由汇编器实测）
    '    取栈64 r10, [rsp+0]\n'
    '    加栈 8\n'                  # 回归: r10 走 REX ⟹ 2 字节（现由汇编器实测）
    # ---- 标号介入（①②）：带标号落入 减栈/存栈 之间 ⟹ 绝不能合并 ----
    '    减栈 8\n'
    '.L920:\n'
    '    存栈64 [rsp+0], r0\n'
    '    取栈64 r0, [rsp+0]\n'
    '.L921:\n'
    '    加栈 8\n'
    # ---- ③ 抵消对：加栈 N + 减栈 N（逐字同） ⟹ 两条都删 ----
    '    加栈 24\n'
    '    减栈 24\n'                 # 正向: 两条都声明删（实测 14）
    '    加栈 24\n'
    '    减栈 40\n'                 # 负向: 操作数不同 ⟹ 两条原样
    '    加栈 32\n'
    '.L922:\n'
    '    减栈 32\n'                 # 负向: 标号落在两行之间 ⟹ 两条原样
    # ---- ④ 不可达消除：跳转 后紧跟 跳转/返回 ⟹ 删后条 ----
    '    跳转 .L930\n'
    '    跳转 .L931\n'              # 正向: 声明删后条（实测 5）
    '    跳转 .L932\n'
    '    返回\n'                    # 正向: 声明删返回（实测 1）
    '    跳转 .L933\n'
    '    移动 r0, 1\n'              # 负向: 后条顺序可达 ⟹ 原样
    '    跳转 .L934\n'
    '.L935:\n'
    '    跳转 .L936\n'              # 负向: 标号落点 ⟹ 后条可达 ⟹ 原样
    # ---- ⑤ 复制传播 + 死值：移64/移32 rX,rY / rZ,rX / rX,存 ⟹ 移64 rZ,rY ----
    '    移64 r2, r3\n'
    '    移64 r4, r2\n'
    '    移64 r2, [rbp-8]\n'        # 正向: .替 + 移64 r4, r3（净 3）
    '    移64 r5, r6\n'
    '    移64 r7, r8\n'
    '    移64 r5, [rbp-16]\n'       # 负向: 第二行源 r8 ≠ 第一行目的 r5 ⟹ 三行原样
    '    移64 r9, r10\n'
    '    移64 r11, r9\n'
    '    移64 r12, [rbp-24]\n'      # 负向: 第三行目的 r12 ≠ 第一行目的 r9 ⟹ 三行原样
    '    移64 r13, r14\n'
    '    移32 r15, r13\n'
    '    移64 r13, [rbp-32]\n'      # 负向: 宽度不同（移64/移32 混用）⟹ 三行原样
    '    移64 r3, r4\n'
    '.L937:\n'
    '    移64 r6, r3\n'             # 负向: 标号落在两行之间 ⟹ 原样
    # ---- ⑥ 跳落到下一行（目标标号唯一才可删） ----
    '    跳转 .L950\n'
    '.L950:\n'                     # 正向: 声明删跳转 + 标号必须保留
    '    跳转短 .L951\n'
    '.L951:\n'                     # 正向: 声明删跳转短
    '    为零跳 .L952\n'
    '.L952:\n'                     # 正向: 声明删条件跳转
    '    跳转 .L953\n'
    '.L954:\n'                     # 负向: 后行标号名不同 ⟹ 原样
    '    跳转 .L955\n'
    '    移动 r0, 9\n'              # 负向: 后行不是标号 ⟹ 原样
    '    环跳 .L956\n'
    '.L956:\n'                     # 负向: 环跳 会递减计数寄存器（有副作用）⟹ 原样
    '    调用 .L957\n'
    '.L957:\n'                     # 负向: 调用 需压返回地址 ⟹ 原样
    '    跳转 .L958\n'
    '.L958:\n'                     # 负向: 目标标号**重名**（下方另有 .L958:）⟹ asm_zh 取最后定义 ⟹ 守卫必须拦住
    '    移动 r0, 8\n'
    '.L958:\n'
    '    移动 r0, 7\n'
    # ---- ⑤ 守卫加固：第三行**仍读 rX** 时绝不可删第一行（skeptic 复现 A/B + 别名 C/D）----
    '    移64 r0, r1\n'
    '    移64 r2, r0\n'
    '    移64 r0, r0\n'              # 负向 A: 第三行是自移（读+写 rX）⟹ 原样
    '.L962:\n'                       # 隔断：否则 ⑧ 会（健全地）删掉上面那行自移（其后 r0 被覆盖）
    '    移64 r0, r1\n'
    '    移64 r2, r0\n'
    '    移64 r0, [r0+8]\n'          # 负向 B: 第三行以 rX 作内存基址（读 rX）⟹ 原样
    '    移64 r5, r1\n'
    '    移64 r2, r5\n'
    '    移64 r5, [rbp-8]\n'         # 负向 C: 别名——目的 r5 即 rbp，源 [rbp-8] 读 rX ⟹ 原样
    '    移64 r0, r1\n'
    '    移64 r2, r0\n'
    '    移64 r0, [rbp-8]\n'         # 正向 D: rbp ≠ r0 ⟹ 应优化成 移64 r2, r1
    '    移64 r0, r1\n'
    '    移64 r2, r0\n'
    '    移64 r0, [rXX+8]\n'         # 负向 E: 源含「**寄存器式但认不出**」的 token(rXX) ⟹ 保守拒绝（原样）
    '    移64 r6, r7\n'
    '    移64 r8, r6\n'
    '    移64 r6, [XYZ+8]\n'         # 正向 F: 符号基址 XYZ 不是可疑寄存器（R881 命名空间收紧）⟹ 应优化
    # ---- ⑦ 帧/栈「存→取 同址」（实测通道：只声明删/替）----
    '    存帧64 [rbp-8], r0\n'
    '    取帧64 r0, [rbp-8]\n'        # 正向: 同寄 ⟹ 删取
    '    存帧64 [rbp-8], r0\n'
    '    取帧64 r3, [rbp-8]\n'        # 正向: 异寄 ⟹ 删取 + 替之为 移64 r3, r0
    '    存帧64 [rbp-8], r0\n'
    '    取帧64 r0, [rbp-16]\n'       # 负向: 位移不同 ⟹ 原样
    '    存帧32 [rbp-8], r0\n'
    '    取帧64 r0, [rbp-8]\n'        # 负向: 宽度不同 ⟹ 原样
    '    存帧64 [rbp-8], r0\n'
    '    取栈64 r0, [rsp+0]\n'        # 负向: 族不同 ⟹ 原样
    '    存帧64 [rbp-8], r0\n'
    '.L960:\n'
    '    取帧64 r0, [rbp-8]\n'        # 负向: 标号介入（非相邻）⟹ 原样
    '    取帧64 r0, [rbp-8]\n'
    '    存帧64 [rbp-8], r0\n'        # 负向: 取在存之前 ⟹ 原样
    # ---- ⑧ 死值写覆盖（前条 mov 类写 rX，紧跟纯写覆盖且不读 rX）----
    '    移动 r0, 77\n'
    '    移64 r0, r1\n'              # 正向: 前条死值 ⟹ 声明删「移动 r0, 77」
    '    移动 r0, 9\n'
    '    加 r0, 3\n'                 # 负向: 后条是**读-改-写**（加）⟹ 原样（隐式读 rX）
    '    移动 r0, 5\n'
    '    符号扩展 r0, r0\n'          # 负向: 后条读 rX（符号扩展）⟹ 原样
    '    移动 r0, 4\n'
    '    移64 r9, r1\n'              # 负向: 后条写别的寄存器 ⟹ 原样
    '    移动 r5, 3\n'
    '    移64 r5, [rbp-8]\n'         # 负向: 别名（rbp==r5）后条源读 rX ⟹ 原样
    '    移动 r0, 2\n'
    '.L961:\n'
    '    移64 r0, r1\n'              # 负向: 标号介入 ⟹ 原样
    # ---- ⑨ 零栈调整：加栈 0 / 减栈 0 ⇹ 声明删（隔离行 避开 ③ 抵消对）----
    '加栈 0\n'                 # 正向: 零栈调整 ⇹ 声明删
    '存帧32 [rbp-4], r0\n'       # 隔离行（① 负向）
    '减栈 0\n'                 # 正向: 另一形态
    '加栈 4\n'                 # 负向: 非 0 ⇹ 原样
)
io.open(NEG, 'w', encoding='utf-8', newline='\n').write(NEG_TEXT)
r = sh([OPT, NEG, '-o', NEG_OUT]) if os.path.exists(OPT) else None
if r is None or r.returncode != 0:
    fails.append('负向用例: opt_zh 未就绪或失败')
    print('  [FAIL] opt_zh 未就绪')
else:
    got = io.open(NEG_OUT, encoding='utf-8', errors='replace').read()
    checks = [
        ('    压栈 r0' in got, '正向1 减栈8+存[rsp+0] ⟹ 压栈 r0'),
        ('    弹栈 r0' in got, '正向2 取[rsp+0]+加栈8 ⟹ 弹栈 r0'),
        ('    减栈 16' in got, '负向1 减栈 16 原样保留'),
        ('    存栈64 [rsp+8], r0' in got, '负向2 [rsp+8] 原样保留'),
        ('    取栈64 r0, [rsp+16]' in got, '负向3 [rsp+16] 原样保留'),
        ('    存帧32 [rbp-4], r0' in got, '负向4 帧存 原样保留'),
        # 实测通道形态：① 先声明删两条、再 `.替`、再发射替换体 ⟹ 字节由汇编器干跑实测（无手写常量）
        ('.删     减栈 8' in got and '.替\n    压栈 r0' in got, '实测通道1 ① 声明删两条 + .替 + 压栈 r0'),
        ('.删     存栈64 [rsp+0], r0' in got, '实测通道2 ① 声明删「存栈64 [rsp+0], r0」'),
        # 回归: 三种寄存器宽度的替换体形态都在（其长度现由汇编器实测，不再有手写常量）
        ('.替\n    压栈 r1' in got, '回归 r1(=rcx) 走实测通道'),
        ('.替\n    压栈 r8' in got, '回归 r8 走实测通道'),
        ('.替\n    弹栈 r10' in got, '回归 r10 走实测通道'),
        # —— ①② 标号介入：不得合并 ——
        ('    减栈 8\n.L920:\n    存栈64 [rsp+0], r0' in got, '负向5 标号介入 减栈/存栈 ⟹ 原样'),
        ('    取栈64 r0, [rsp+0]\n.L921:\n    加栈 8' in got, '负向6 标号介入 取栈/加栈 ⟹ 原样'),
        # —— ③ 抵消对 ——
        ('.删     加栈 24' in got and '.删     减栈 24' in got, '阳性3 加栈24+减栈24 ⟹ 两条都**声明删**'),
        ('    加栈 24\n    减栈 40\n' in got, '负向7 操作数不同的 加栈/减栈 ⟹ 原样'),
        ('    加栈 32\n.L922:\n    减栈 32' in got, '负向8 标号介入 加栈/减栈 ⟹ 原样'),
        # —— ④ 不可达消除 ——
        ('    跳转 .L930\n.删     跳转 .L931' in got, '阳性4 跳转后跟跳转 ⟹ 声明删后条'),
        ('    跳转 .L932\n.删     返回' in got, '阳性5 跳转后跟返回 ⟹ 声明删返回'),
        ('    跳转 .L933\n    移动 r0, 1' in got, '负向9 跳转后跟顺序可达指令 ⟹ 原样'),
        ('    跳转 .L934\n.L935:\n    跳转 .L936' in got, '负向10 标号落点 ⟹ 后条可达 ⟹ 原样'),
        # —— ⑤ 复制传播 + 死值 ——
        ('.替\n    移64 r4, r3' in got, '阳性6 复制传播 ⟹ .替 + 移64 r4, r3'),
        ('    移64 r5, r6\n    移64 r7, r8\n    移64 r5, [rbp-16]' in got, '负向11 第二行源≠第一行目的 ⟹ 原样'),
        ('    移64 r9, r10\n    移64 r11, r9\n    移64 r12, [rbp-24]' in got, '负向12 第三行目的≠第一行目的 ⟹ 原样'),
        ('    移64 r13, r14\n    移32 r15, r13' in got, '负向13 宽度不同 ⟹ 原样'),
        ('    移64 r3, r4\n.L937:\n    移64 r6, r3' in got, '负向14 标号介入 ⟹ 原样'),
        # —— ⑥ 跳落到下一行 ——
        ('.删     跳转 .L950\n.L950:' in got, '阳性7 跳转<下一行标号> ⟹ 声明删跳转 + 标号保留'),
        ('.删     跳转短 .L951\n.L951:' in got, '阳性8 跳转短<下一行标号> ⟹ 声明删'),
        ('.删     为零跳 .L952\n.L952:' in got, '阳性9 条件跳转<下一行标号> ⟹ 声明删'),
        ('    跳转 .L953\n.L954:' in got, '负向15 后行标号名不同 ⟹ 原样'),
        ('    跳转 .L955\n    移动 r0, 9' in got, '负向16 后行不是标号 ⟹ 原样'),
        ('    环跳 .L956\n.L956:' in got, '负向17 环跳 有副作用 ⟹ 原样'),
        ('    调用 .L957\n.L957:' in got, '负向18 调用 需压返回地址 ⟹ 原样'),
        ('    跳转 .L958\n.L958:' in got, '负向19 目标标号重名 ⟹ 守卫拦住 ⟹ 原样'),
        # —— ⑤ 守卫加固（第三行仍读 rX 时不可删第一行）——
        ('    移64 r0, r1\n    移64 r2, r0\n    移64 r0, r0' in got, '负向20 第三行自移(读+写 rX) ⟹ 原样（skeptic 复现 A）'),
        ('    移64 r0, r1\n    移64 r2, r0\n    移64 r0, [r0+8]' in got, '负向21 第三行以 rX 为内存基址 ⟹ 原样（skeptic 复现 B）'),
        ('    移64 r5, r1\n    移64 r2, r5\n    移64 r5, [rbp-8]' in got, '负向22 别名(r5==rbp) 源读 rX ⟹ 原样'),
        ('.替\n    移64 r2, r1' in got, '阳性10 源不读 rX(rbp≠r0) ⟹ .替 + 移64 r2, r1'),
        ('    移64 r0, r1\n    移64 r2, r0\n    移64 r0, [rXX+8]' in got, '负向23 源含「寄存器式但认不出」的 token(rXX) ⟹ 保守拒绝（原样）'),
        ('.替\n    移64 r8, r7' in got, '正向 F 符号基址 [XYZ+8] 不是可疑寄存器 ⟹ 允许优化（R881 命名空间收紧）'),
        # —— ⑦ 帧/栈 存→取 同址（实测通道）——
        ('.删     取帧64 r0, [rbp-8]' in got, '阳性11 同寄 存→取 ⟹ 声明删取（实测通道）'),
        ('.替\n    移64 r3, r0' in got, '阳性12 异寄 存→取 ⟹ 声明删取 + 替之以 移64 r3, r0'),
        ('    存帧64 [rbp-8], r0\n    取帧64 r0, [rbp-16]' in got, '负向24 位移不同 ⟹ 原样'),
        ('    存帧32 [rbp-8], r0\n    取帧64 r0, [rbp-8]' in got, '负向25 宽度不同 ⟹ 原样'),
        ('    存帧64 [rbp-8], r0\n    取栈64 r0, [rsp+0]' in got, '负向26 族不同 ⟹ 原样'),
        ('    存帧64 [rbp-8], r0\n.L960:\n    取帧64 r0, [rbp-8]' in got, '负向27 标号介入（非相邻）⟹ 原样'),
        ('    取帧64 r0, [rbp-8]\n    存帧64 [rbp-8], r0' in got, '负向28 取在存之前 ⟹ 原样'),
        # —— ⑧ 死值写覆盖 ——
        ('.删     移动 r0, 77' in got, '阳性13 死值写覆盖 ⟹ 声明删前条'),
        ('    移动 r0, 9\n    加 r0, 3' in got, '负向29 后条是读-改-写(加) ⟹ 原样（隐式源）'),
        ('    移动 r0, 5\n    符号扩展 r0, r0' in got, '负向30 后条读 rX(符号扩展) ⟹ 原样'),
        ('    移动 r0, 4\n    移64 r9, r1' in got, '负向31 后条写别的寄存器 ⟹ 原样'),
        ('    移动 r5, 3\n    移64 r5, [rbp-8]' in got, '负向32 别名(rbp==r5) 后条源读 rX ⟹ 原样'),
        ('    移动 r0, 2\n.L961:\n    移64 r0, r1' in got, '负向33 标号介入 ⟹ 原样'),
        # —— ⑨ 零栈调整 ——
        ('.删 加栈 0' in got, '阳性14 零栈调整 ⇹ 声明删'),
        ('加栈 4\n' in got, '负向 34 加栈 4 ⇹ 原样'),
    ]
    # ---- 分支自检：9 条 pass **每条**都必须在本用例里至少命中一次（否则判据器没覆盖到该 pass）----
    rep = re.search(r'优化 \d+ 处 \[([^\]]*)\]', (r.stdout + r.stderr).decode('utf-8', 'replace'))
    if not rep:
        fails.append('分支自检: 未取到 opt_zh 的分类计数行')
    else:
        segs = [x.strip() for x in rep.group(1).split('/')]
        nums = [int(re.search(r'(\d+)$', s).group(1)) for s in segs if re.search(r'(\d+)$', s)]
        # 收尾行类数 随 R880（+G-DCE/⑪/⑫）与 R881（+LSP）增长 ⟹ 只断言**经典 9 类**全命中；
        # 元数据驱动的那几类 由 check_opt_meta_consume / check_opt_lsp / check_opt_per_pass 覆盖。
        if len(nums) < 9 or any(x == 0 for x in nums[:9]):
            fails.append('分支自检: 分类计数 %s 的前 9 类（经典 pass）未全部命中' % nums)
        else:
            print('    [自检] 经典 9 类 pass 分支计数 = %s ⟹ 全部被本用例覆盖 ✓（G-DCE/⑪/⑫/LSP 由各自判据器覆盖）'
                  % nums[:9])
    ok = all(c[0] for c in checks)
    print('  负向/正向用例: %s' % ('全过' if ok else 'FAIL'))
    for good, name in checks:
        if not good:
            fails.append('负向用例失败: ' + name)
    if ok:
        print('    [OK] %d 项断言全过（含 bug 回归：取栈64 [rsp+16] 原样 / r1 单字符寄存器长度判定）' % len(checks))

# ---------- ② 小用例三环链 行为等价 + 体积下降 ----------
CASES = [('t_min.c', 'int main(void){return 42;}\n', 42, ''),
         ('t_prt.c', '#include <stdio.h>\nint main(void){ printf("OPTB 42\\n"); return 0; }\n', 0, 'OPTB 42')]
for name, src, erc, eout in CASES:
    p = os.path.join(WORK, name)
    io.open(p, 'w', encoding='utf-8', newline='\n').write(src)
    base = os.path.join(WORK, name[:-2])
    r1 = sh([CC, p, '-o', base + '_d.exe'])
    r2 = sh([CC, '-S', p, '-o', base + '_s.exe'])
    if r2.returncode != 0:
        fails.append('%s: -S 失败' % name); continue
    asm_txt = base + '_s.exe.asm'
    opt_txt = base + '_o.asm'
    r3 = sh([OPT, asm_txt, '-o', opt_txt])
    r4 = sh([ASM, opt_txt, '-o', base + '_a.exe'])
    if r3.returncode != 0 or r4.returncode != 0:
        fails.append('%s: 优化链失败 (opt=%d asm=%d)' % (name, r3.returncode, r4.returncode)); continue
    rc1, o1 = run(base + '_d.exe')
    rc2, o2 = run(base + '_a.exe')
    tn = text_size(base + '_d.exe'); to = text_size(base + '_a.exe')
    mz = re.search(r'总缩=(-?\d+)', (r4.stdout + r4.stderr).decode('utf-8', 'replace'))
    tz = int(mz.group(1)) if mz else 0
    # 判据口径: 不能只看 `.asm` 文本大小（实测通道的声明行会让文本略变长）⟹ 改看**产物 .text 有效内容**严格更小
    #           且「差值 == 汇编器自报总缩」（非循环、第三方可复算）
    ok = (rc1 == rc2 == erc) and (eout in o2) and (to[2] < tn[2]) and ((tn[2] - to[2]) == tz)
    print('  %s  直产 rc=%s out=%r | 优化链 rc=%s out=%r | .text 有效 %d→%d (Δ=%d) 汇编器报总缩=%d | %s'
          % (name, rc1, o1[:20], rc2, o2[:20], tn[2], to[2], to[2] - tn[2], tz, 'OK' if ok else 'FAIL'))
    if not ok:
        fails.append('%s: 行为/体积断言失败' % name)

# ---------- ③ 编译器本体自举闭环（--full） ----------
if FULL:
    base = os.path.join(WORK, 'qcc_self')
    print('  [full] 三环链构建编译器本体 ...')
    r = sh([CC, '-S', SRC, '-o', base + '_s.exe'], timeout=3600)
    if r.returncode != 0:
        fails.append('full: qcc -S 编译器本体失败'); print('    [FAIL] -S')
    else:
        r = sh([OPT, base + '_s.exe.asm', '-o', base + '_o.asm'], timeout=3600)
        r2 = sh([ASM, base + '_o.asm', '-o', base + '_zo.exe'], timeout=3600)
        _zo_out = (r2.stdout + r2.stderr).decode('utf-8', 'replace')   # 单源真相：缩短总量由汇编器自报
        if r.returncode != 0 or r2.returncode != 0:
            fails.append('full: 三环链构建编译器本体失败'); print('    [FAIL] 链')
        else:
            zo = base + '_zo.exe'
            r = sh([zo, SRC, '-o', base + '_v1o.exe'], timeout=3600)
            r2 = sh([base + '_v1o.exe', SRC, '-o', base + '_v2o.exe'], timeout=3600)
            if r.returncode != 0 or r2.returncode != 0:
                fails.append('full: 优化链编译器自举失败'); print('    [FAIL] 自举')
            else:
                h1 = h8(base + '_v1o.exe'); h2 = h8(base + '_v2o.exe')
                ok = (h1 == h2)
                print('    优化链构建的编译器: 自举 v1o=%s v2o=%s ⟹ %s' % (h1, h2, '收敛' if ok else '未收敛'))
                if not ok:
                    fails.append('full: 自举未收敛 %s != %s' % (h1, h2))
                # ---- 验收 3（编译器本体）：同一份 -S 产物走 2 环 vs 3 环，比较 .text 实际字节 ----
                sh([ASM, base + '_s.exe.asm', '-o', base + '_s2.exe'], timeout=3600)
                # 缩短总量 = **汇编器自报总缩**（文报通道 + 实测通道）。注：`.asm` 里的 `.缩总` 只是**文报通道**，
                # 自本条起不再用它比对差值（skeptic 指出：那会让 `.text` 差值 211946 != 161278 ⟹ 断言恒假）。
                m = re.search(r'总缩=(-?\d+)', _zo_out)
                sz = int(m.group(1)) if m else 0
                mw = re.search(r'文报缩=(-?\d+)', _zo_out)
                ms = re.search(r'实测缩=(-?\d+)', _zo_out)
                wenz = int(mw.group(1)) if mw else 0
                shiz = int(ms.group(1)) if ms else 0
                tn = text_size(base + '_s2.exe'); to = text_size(zo)
                print('    编译器本体 2 环链: .text rsize=%d 有效=%d 文件=%d' % (tn[0], tn[2], tn[1]))
                print('    编译器本体 3 环链: .text rsize=%d 有效=%d 文件=%d' % (to[0], to[2], to[1]))
                print('    差值 (3环−2环)  : rsize=%+d 有效=%+d 文件=%+d   (汇编器自报 总缩=%d = 文报 %d + 实测 %d)'
                      % (to[0] - tn[0], to[2] - tn[2], to[1] - tn[1], sz, wenz, shiz))
                if to[2] < tn[2] and (tn[2] - to[2] == sz):
                    print('    ⟹ 编译器本体收益落地：.text 有效内容严格更小，且差值 == 汇编器自报总缩 ✓')
                else:
                    fails.append('full: 编译器本体收益断言失败 (有效 %d→%d, 总缩=%d)' % (tn[2], to[2], sz))
                    print('    ⟹ [FAIL] 编译器本体收益断言失败')
                # ---- 行为等价：3 环链编译器 vs 2 环链编译器（同一输入 rc/stdout 逐字一致）----
                bp = os.path.join(WORK, 't_prt.c')
                sh([base + '_s2.exe', bp, '-o', base + '_be_n.exe'])
                sh([zo, bp, '-o', base + '_be_o.exe'])
                rn, on = run(base + '_be_n.exe'); ro, oo = run(base + '_be_o.exe')
                okb = (rn == ro and on == oo)
                print('    行为等价 (t_prt.c): 2环 rc=%s out=%r | 3环 rc=%s out=%r ⟹ %s'
                      % (rn, on[:16], ro, oo[:16], '逐字一致' if okb else '★不一致'))
                if not okb:
                    fails.append('full: 编译器本体行为不等价')

print('== 结果: %s ==' % ('全部 通过' if not fails else 'FAIL=%d' % len(fails)))
for f in fails:
    print('   -', f)
sys.exit(0 if not fails else 1)
