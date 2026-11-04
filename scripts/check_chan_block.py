# -*- coding: utf-8 -*-
"""check_chan_block — 通道「满」行为判据（ORD-20261029-38 裁定 B + ORD-20261101-81 安全阀）。

裁定（2026-10-31 大哥）：`送 q, v;` 满时**阻塞**（与 `背压 送` 同义），不再静默丢弃。
安全阀（ORD-20261101-81）：**满时不可能有消费者**（通道只能声明在函数体内、不可作实参传递）
⟹ 阻塞点必须响亮中止（stderr `[ERR] ... A5007` + 非零退出），不得永久静默挂起。

判据四条腿（都能当场跑）：
  A 不误伤：cap=2 送 2 收 2 ⟹ 5s 内正常完成，输出 `1 2`
  B 满且无消费者：cap=2 送 3 ⟹ 必须给出**可观测结果**：非零退出 + `A5007` 诊断；
    若 5s 仍挂起 ⟹ **判红**（挂死=静默死锁，绝不再判成通过）
  C 空收：非阻塞 + 空读数 + 值保持
  D 线程池在场：`线程池 池,2; 提交…;` 后送满 ⟹ 同样必须 `A5007` 中止
    （通道只能声明在函数体内且不可传参 ⟹ 池线程不可能消费它；旧实现按「有其他线程」放行 ⟹ 照样静默挂死）
  E 阻塞可用（B3）：文件级 `通道 数, 8;` + 两个工作线程 ── 产 0..99 / 消 0..99 逐个校验 ⟹
    10s 内输出 `MT100 OK`、rc=0（「两个线程通过同一个通道传 100 个数」= 阻塞语义的唯一硬证明）

用法: python scripts/check_chan_block.py [--cc qcc_zh_v1.exe]
"""
from qcc_tmp import tmp_dir
import argparse
import io
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TMP = os.environ.get('QCC_TMPDIR') or tmp_dir('chan_block')
TIMEOUT = 15.0

OK_SRC = (
    '#包含 <stdio.h>\n'
    '函数 整 主(空){\n'
    '    通道 q, 2;\n'
    '    送 q, 1; 送 q, 2;\n'
    '    整 a=0; 整 b=0;\n'
    '    收 q, a; 收 q, b;\n'
    '    格式化 "%d %d\\n", a, b;\n'
    '    返 0;\n'
    '}\n'
)

BLOCK_SRC = (
    '#包含 <stdio.h>\n'
    '函数 整 主(空){\n'
    '    通道 q, 2;\n'
    '    送 q, 1; 送 q, 2; 送 q, 3;\n'
    '    格式化 "NO-BLOCK\\n", 0;\n'
    '    返 0;\n'
    '}\n'
)

EMPTY_RECV_SRC = (
    '#包含 <stdio.h>\n'
    '函数 整 主(空){\n'
    '    通道 q, 2;\n'
    '    整 v = 99;\n'
    '    收 q, v;\n'
    '    收 q, v;\n'
    '    格式化 "空读 v=%d 计=%d\\n", v, __ch_q_empty;\n'
    '    返 0;\n'
    '}\n'
)

MT_SRC = (
    '#包含 <stdio.h>\n'
    '通道 数, 8;\n'
    '静 整 产(空 *p) { 遍 (整 i = 0; i < 100; i++) { 送 数, i; } 返 0; }\n'
    '静 整 消(空 *p) { 遍 (整 i = 0; i < 100; i++) { 循环 (就绪(数) == 0) { } 整 v = -1; 收 数, v; '
    '若 (v != i) { 格式化 "BAD %d %d\\n", i, v; 返 1; } } 返 0; }\n'
    '函数 整 主(空) { 线程池 池, 2; 提交 池, 产, (空 *)0; 提交 池, 消, (空 *)0; 等待池 池; '
    '格式化 "MT100 OK\\n", 0; 返 0; }\n'
)

POOL_SRC = (
    '#包含 <stdio.h>\n'
    '静 整 活(空 *p) { 返 0; }\n'
    '函数 整 主(空){\n'
    '    线程池 池, 2;\n'
    '    提交 池, 活, (空 *)0;\n'
    '    通道 q, 2;\n'
    '    送 q, 1; 送 q, 2; 送 q, 3;\n'
    '    格式化 "NO-BLOCK\\n", 0;\n'
    '    返 0;\n'
    '}\n'
)


def build(cc, name, src):
    os.makedirs(TMP, exist_ok=True)
    p = os.path.join(TMP, name + '.jy')
    exe = os.path.join(TMP, name + '.exe')
    io.open(p, 'w', encoding='utf-8', newline='\n').write(src)
    r = subprocess.run([cc, p, '-o', exe], cwd=REPO, capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=180)
    return r.returncode, (r.stderr or ''), exe


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or 'qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)

    fails = []
    skips = []

    # 腿 A：不误伤
    rc, err, exe = build(cc, 'ok', OK_SRC)
    if rc != 0:
        fails.append('编译失败（不溢出的正常用例）: %s' % err[:200])
    else:
        q = subprocess.run([exe], capture_output=True, text=True,
                           encoding='utf-8', errors='replace', timeout=TIMEOUT)
        out = (q.stdout or '').strip()
        print('[A 不误伤] rc=%d out=%r' % (q.returncode, out))
        if q.returncode != 0 or out != '1 2':
            fails.append('不溢出用例行为异常: rc=%d out=%r（期望 rc=0 / "1 2"）' % (q.returncode, out))

    # 腿 B：满且无消费者 —— 期望**可观测结果**（非零退出 + A5007），不再把超时当绿
    rc, err, exe = build(cc, 'block', BLOCK_SRC)
    if rc != 0:
        fails.append('编译失败（溢出用例）: %s' % err[:200])
    else:
        try:
            q = subprocess.run([exe], capture_output=True, text=True,
                               encoding='utf-8', errors='replace', timeout=TIMEOUT)
            out = (q.stdout or '').strip()
            errt = (q.stderr or '').strip()
            if q.returncode != 0 and 'A5007' in errt:
                print('[B 满即阻塞] 无消费者 ⟹ 响亮中止 rc=%d err=%r' % (q.returncode, errt[:160]))
            elif q.returncode != 0:
                fails.append('溢出用例非零退出但缺 A5007 诊断: rc=%d out=%r err=%r'
                             % (q.returncode, out[:100], errt[:160]))
            else:
                print('[B 满即阻塞] 进程 %.0fs 内跑完 rc=0 out=%r ⟹ 仍在静默丢弃' % (TIMEOUT, out))
                fails.append('送 满仍不阻塞（进程 %.0fs 内跑完，out=%r）⟹ 仍在静默丢弃' % (TIMEOUT, out))
        except subprocess.TimeoutExpired:
            # 超时 **不是** 通过：挂起 = 静默死锁，判红（旧产物没有安全阀）。
            print('[B 满即阻塞] FAIL：%.0fs 超时挂起且无 A5007 —— 该产物无安全阀（挂死=红）' % TIMEOUT)
            fails.append('送 满且无消费者时永久挂起（%.0fs 超时，无 A5007）⟹ 静默死锁' % TIMEOUT)

    # 腿 C：空通道收 必须 **非阻塞 + 空读数+1 + v 保持原值**（施工单 §4.1 契约，
    #       2026-10-31 ORD-60 子项④：原「静默保持旧值」缺可观测性，现由 __ch_q_empty 计数补上）
    rc, err, exe = build(cc, 'empty_recv', EMPTY_RECV_SRC)
    if rc != 0:
        fails.append('编译失败（空收用例）: %s' % err[:200])
    else:
        try:
            q = subprocess.run([exe], capture_output=True, text=True,
                               encoding='utf-8', errors='replace', timeout=TIMEOUT)
            out = (q.stdout or '').strip()
            print('[C 空收] rc=%d out=%r' % (q.returncode, out))
            if q.returncode != 0 or '空读 v=99 计=2' not in out:
                fails.append('空收契约不符（期望 rc=0 / 空读 v=99 计=2，实得 rc=%d out=%r）' % (q.returncode, out))
        except subprocess.TimeoutExpired:
            fails.append('空收不应阻塞（施工单 §4.1 要求非阻塞），但 %.0fs 未返回' % TIMEOUT)

    # 腿 D：线程池在场（其它线程存在）也必须中止 —— 通道不可跨线程 ⟹ 池线程救不了这个死锁
    rc, err, exe = build(cc, 'pool', POOL_SRC)
    if rc != 0:
        fails.append('编译失败（线程池溢出用例）: %s' % err[:200])
    else:
        try:
            q = subprocess.run([exe], capture_output=True, text=True,
                               encoding='utf-8', errors='replace', timeout=TIMEOUT)
            out = (q.stdout or '').strip()
            errt = (q.stderr or '').strip()
            if q.returncode != 0 and 'A5007' in errt:
                print('[D 线程池] 无消费者 ⟹ 响亮中止 rc=%d err=%r' % (q.returncode, errt[:160]))
            elif q.returncode != 0:
                fails.append('线程池溢出用例非零退出但缺 A5007: rc=%d out=%r err=%r'
                             % (q.returncode, out[:100], errt[:160]))
            else:
                print('[D 线程池] rc=0 out=%r ⟹ 未中止' % out)
                fails.append('线程池 + 满通道仍跑完（rc=0, out=%r）⟹ 未中止' % out)
        except subprocess.TimeoutExpired:
            print('[D 线程池] FAIL：%.0fs 超时挂起且无 A5007 ⟹ 静默死锁（池线程不可能消费局部通道）' % TIMEOUT)
            fails.append('线程池 + 满通道永久挂起（%.0fs 超时，无 A5007）⟹ 静默死锁' % TIMEOUT)

    # 腿 E（B3）：两个线程经同一个通道传 100 个数 —— 阻塞可用的唯一硬证明
    rc, err, exe = build(cc, 'mt100', MT_SRC)
    if rc != 0:
        print('[E 阻塞可用] 编译失败（文件级通道/跨线程）：%s' % err[:200])
        fails.append('B3 编译失败（文件级通道未实现）：%s' % err[:160])
    else:
        okn = 0
        last = ''
        for i in range(3):
            try:
                q = subprocess.run([exe], capture_output=True, text=True,
                                   encoding='utf-8', errors='replace', timeout=10.0)
                out = (q.stdout or '').strip()
                last = out
                if q.returncode == 0 and 'MT100 OK' in out:
                    okn += 1
            except subprocess.TimeoutExpired:
                last = 'TIMEOUT'
        print('[E 阻塞可用] 三线程? 否 —— 两线程 × 100 数：3 次中 %d 次 MT100 OK（最后一次=%r）' % (okn, last))
        if okn != 3:
            fails.append('B3 两线程 × 100 数：3 次中仅 %d 次成功（最后=%r）' % (okn, last))

    if fails:
        print('[FAIL] 通道满阻塞判据未通过 %d 项:' % len(fails))
        for f in fails:
            print('   - ' + f)
        return 1
    print('[PASS] 通道 送 满：无消费者响亮中止（A5007），且不溢出不误伤')
    return 0


if __name__ == '__main__':
    sys.exit(main())
