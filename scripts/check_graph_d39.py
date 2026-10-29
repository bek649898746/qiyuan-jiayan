#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_graph_d39 —— `-graph` 输出路径 与 源/产物 同名时 **必须拒绝覆盖**（D39 P0）。

病灶（审计回执 `_AUDIT_RECEIPT_20261021_启元D39.md`）：
  `-graph` 的输出由 `副寅一()` 用 `打开文件(末高,"wb")` 直接截断写 ⟹
    qcc 源.jy -graph 源.jy                ⟹ 用户源码被覆盖成 JSON（不可恢复）
    qcc 源.c -o out.exe -graph out.exe    ⟹ 刚写好的产物被覆盖成 JSON
  两条实测 rc=0、零提示。

本判据走**编译器 CLI 真实入口**，三条都要过：
  [1] -graph == 输入源     ⟹ rc != 0，且源文件 SHA256 不变，stderr 含「输入源文件」
  [2] -graph == -o 产物    ⟹ rc != 0，且产物仍是合法 PE（MZ），stderr 含「编译产物」
  [3] -graph == 独立 json  ⟹ rc == 0，JSON 可解析且 entities 非空，产物存在

用法: python scripts/check_graph_d39.py [--cc qcc_zh_v1.exe]
退出码: 0 = 全过；1 = 有 RED
"""
import argparse, hashlib, json, os, subprocess, sys, tempfile
sys.stdout.reconfigure(encoding='utf-8', errors='replace')

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REPO)

SRC = u'#\u5305\u542b <stdio.h>\n\u51fd\u6570 \u6574 \u4e3b(\u7a7a) { \u683c\u5f0f\u5316 "D39 hello\\n"; \u8fd4 0; }\n'


def sha(p):
    return hashlib.sha256(open(p, 'rb').read()).hexdigest()


def run(cmd):
    p = subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8', errors='replace')
    return p.returncode, (p.stdout or '') + (p.stderr or '')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_seed.exe')  # 2026-10-21: 以哈希校验过的种子为准
    a = ap.parse_args()
    cc = a.cc
    if not os.path.exists(cc):
        # 2026-10-24: 缺主体 不得当绿（rc=2 = 环境未就绪）
        print('[FAIL] 编译器不存在: %s（未执行，不判绿）' % cc); return 2
    tmp = tempfile.mkdtemp(prefix='d39_')
    fails = []
    src = os.path.join(tmp, 't.jy')
    open(src, 'w', encoding='utf-8').write(SRC.replace('\\n', '\n'))

    # [1] -graph == 源
    h0 = sha(src)
    rc, log = run([cc, src, '-graph', src, '-o', os.path.join(tmp, 'a.exe')])
    h1 = sha(src)
    ok = (rc != 0 and h0 == h1 and u'\u8f93\u5165\u6e90\u6587\u4ef6' in log)
    print('[1] -graph=源: rc=%d 源SHA不变=%s 提示=%s' % (rc, h0 == h1, u'\u8f93\u5165\u6e90\u6587\u4ef6' in log))
    if not ok: fails.append('[1] -graph=源 未拒绝或源被改写')

    # [2] -graph == -o 产物
    out = os.path.join(tmp, 'b.exe')
    rc, log = run([cc, src, '-graph', out, '-o', out])
    pe = False
    if os.path.exists(out) and os.path.getsize(out) >= 2:
        b = open(out, 'rb').read(2); pe = (b == b'MZ')
    ok = (rc != 0 and pe and u'\u7f16\u8bd1\u4ea7\u7269' in log)
    print('[2] -graph=产物: rc=%d 产物仍是PE=%s 提示=%s' % (rc, pe, u'\u7f16\u8bd1\u4ea7\u7269' in log))
    if not ok: fails.append('[2] -graph=产物 未拒绝或产物被覆盖')

    # [3] -graph == 独立 json
    js = os.path.join(tmp, 'c.json')
    exe = os.path.join(tmp, 'c.exe')
    rc, log = run([cc, src, '-graph', js, '-o', exe])
    ent = -1
    try:
        ent = len(json.load(open(js, encoding='utf-8')).get('entities', []))
    except Exception:
        ent = -1
    ok = (rc == 0 and ent > 0 and os.path.exists(exe))
    print('[3] -graph=json: rc=%d entities=%d exe=%s' % (rc, ent, os.path.exists(exe)))
    if not ok: fails.append('[3] 正常 -graph 路径被误伤')

    import shutil; shutil.rmtree(tmp, ignore_errors=True)
    print()
    if fails:
        print('=== graph-d39: RED (%d 项) ===' % len(fails))
        for f in fails: print('  - ' + f)
        return 1
    print('=== graph-d39: PASS ===')
    return 0


if __name__ == '__main__':
    sys.exit(main())
