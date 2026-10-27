#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""regress_opt_fold —— ⑮「压栈回读」历史打回形态的**新旧对照**（证据件，不是判据）。

两个形态（都是评审真打回过、会改语义的），各自挂在**第一次修好它之前**的那一版上做对照：
  ① `wd0` 部分宽度写：`存栈32 [rsp+40], r1` 只写低 4 字节。`01e81edc`（R881i）把高 4 字节留成残值；
  ② `ca0` callee 读 outgoing 区：被调者 `取栈64 r1,[rsp+48]` 读的正是调用者被压的那一格
     （A = 调用点 rsp + 40；callee 入口 rsp = C-8 ⟹ [rsp+48]）。`5acfe09c`（R881k）删掉 `压栈` ⟹ 读到残值。

对每个形态：该版旧优化器与当前 `opt_zh.exe` 各走一遍两链（未优化 / 优化），断言：
  · 旧版 **改语义**（两链退出码不同）—— 证明确是反例；
  · 新版 **两链一致**，且优化后 asm 里**仍保留那条 `压栈`**（写者不删）、回读那行已删。

用法: python scripts/regress_opt_fold.py
退出码: 0 = 对照成立；1 = 不符合预期；2 = 环境未就绪
"""
import io
import os
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)

PB = '''_入口:
    压栈 r5
    压栈 r3
    移64 r5, r4
    减栈 64
'''
PT = '''    加栈 64
    弹栈 r3
    弹栈 r5
    返回
'''

# name -> (造件, 旧版 git 短 sha)
SHAPES = {
    'wd0 部分宽度写（存栈32 后取栈64）': (PB + '''    移动 r2, 0
    按位反64 r2
    压栈 r2
    弹栈 r2
    移动 r0, 7
    压栈 r0
    减栈 40
    取栈64 r0, [rsp+40]
    移动 r1, 0
    存栈32 [rsp+40], r1
    取栈64 r1, [rsp+40]
    逻辑右移64 r1, 32
    加栈 48
    移64 r0, r1
''' + PT, '01e81edc'),
    'ca0 callee 读 outgoing 区（[rsp+48]）': (PB + '''    移动 r0, 7
    压栈 r0
    减栈 40
    取栈64 r0, [rsp+40]
    移64 r1, r0
    调用 .Lget
    加栈 48
    移64 r0, r1
''' + PT + '''.Lget:
    取栈64 r1, [rsp+48]
    返回
''', '5acfe09c'),
}


def with_meta(code):
    lines = [l for l in code.split('\n') if l.strip()]
    n = len([l for l in lines if not l.strip().endswith(':')])
    meta = ['; @QCCMETA 1 ins=%d funcs=1 loops=0 nodes=%d' % (n, n)]
    meta += ['; @I %d f=1 n=%d k=1 t=-1 l=-1 d=0' % (i, i) for i in range(n)]
    meta.append('; @QCCMETA-END')
    return '\n'.join(lines) + '\n' + '\n'.join(meta) + '\n'


def run(cmd, t=900):
    r = subprocess.run(cmd, capture_output=True, timeout=t)
    return r.returncode, (r.stdout + r.stderr).decode('utf-8', 'replace')


def build_old(sha, work):
    exe = os.path.join('build', 'control_opt_%s.exe' % sha)
    if os.path.exists(exe):
        return exe
    src = os.path.join(work, 'old_%s.jy' % sha)
    r = subprocess.run(['git', 'show', '%s:srclib/opt_zh.jy' % sha], capture_output=True)
    if r.returncode != 0:
        return None
    io.open(src, 'wb').write(r.stdout)
    rc, out = run(['qcc_zh_v1.exe', src, '-o', exe])
    if rc != 0:
        print('[FAIL] %s 现编失败\n%s' % (sha, out[:300]))
        return None
    print('[基线] %s 由 git %s 现编' % (exe, sha))
    return exe


def main():
    work = os.path.join(os.environ.get('TEMP', '.'), 'qcc_regress_fold')
    os.makedirs(work, exist_ok=True)
    for p in ('opt_zh.exe', 'asm_zh.exe', 'qcc_zh_v1.exe'):
        if not os.path.exists(p):
            print('[FAIL] 环境未就绪（缺 %s）' % p)
            return 2

    print('== ⑮ 历史打回形态：旧（各形态首破版）vs 新（opt_zh.exe）==')
    bad = 0
    for name, (body, sha) in SHAPES.items():
        old = build_old(sha, work)
        if not old:
            bad += 1
            continue
        p = os.path.join(work, name.split()[0] + '.asm')
        io.open(p, 'w', encoding='utf-8', newline='\n').write(with_meta(body))
        rc, _ = run(['asm_zh.exe', p, '-o', p + '.n.exe'])
        if rc != 0:
            print('  %-34s ★未优化链汇编失败 rc=%d' % (name, rc))
            bad += 1
            continue
        e0 = subprocess.run([p + '.n.exe'], capture_output=True).returncode & 0xFFFFFFFF
        rows = []
        for tag, exe in (('旧', old), ('新', 'opt_zh.exe')):
            po = p + '.' + ('old' if tag == '旧' else 'new') + '.asm'
            rc1, out = run([exe, p, '-o', po])
            m = re.search(r'压栈回读 (\d+)', out)
            fold = int(m.group(1)) if m else 0
            rc2, _ = run(['asm_zh.exe', po, '-o', po + '.exe'])
            e = subprocess.run([po + '.exe'], capture_output=True).returncode & 0xFFFFFFFF if not (rc1 or rc2) else -1
            txt = io.open(po, encoding='utf-8', errors='replace').read().split('\n')
            keep = not any(l.startswith('.删') and '压栈' in l for l in txt)
            rows.append((tag, fold, e, keep))
        (t1, f1, e1, k1), (t2, f2, e2, k2) = rows
        ok = (e1 != e0) and (e2 == e0) and k2
        bad += 0 if ok else 1
        print('  %-34s 未优化 exit=%d（旧版 %s）' % (name, e0, sha))
        print('      旧: 折叠=%d 优化后 exit=%d %s'
              % (f1, e1, '⟹ 语义被改（反例成立 ✓）' if e1 != e0 else '（未改 ⟹ 反例不成立）'))
        print('      新: 折叠=%d 优化后 exit=%d · 写者保留=%s %s'
              % (f2, e2, '是' if k2 else '否', '✓' if ok else '★不符合预期'))
    print('== 结果: %s ==' % ('新旧对照成立' if bad == 0 else 'FAIL=%d' % bad))
    return 0 if bad == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
