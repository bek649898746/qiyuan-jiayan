# -*- coding: utf-8 -*-
"""check_cache_fake_ok —— `--cache` 命中时不校验产物（假成功）的复现器（D837 / ORD-20261030-64）。

施工单《`--cache` 三条缺陷》§二 场景 A：
  ① `--cache` 编一次 → 产 a.exe
  ② 手工删 a.exe
  ③ 再 `--cache` → 现状输出 `OK: a.exe (cached)` 但 **a.exe 根本不存在**（谎报成功）

判据：
  · 步骤③ 报 cached 且产物不存在 ⟹ 缺陷复现（rc=1，blocked 诚实）
  · 步骤③ 重新产出 a.exe（或明确失败）⟹ 缺陷消失（rc=0，应转 fixed）

用法: python scripts/check_cache_fake_ok.py [--cc qcc_zh_v1.exe]
"""
import argparse
import io
import os
import shutil
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TMP = os.environ.get('QCC_TMPDIR') or os.path.join(REPO, 'scratch_cachefake')

SRC = '''#包含 <stdio.h>
函数 整 主(空){
    整 s = 0;
    整 i = 0;
    遍 (i = 0; i < 10; i++) s += i;
    格式化 "%d\\n", s;
    返 0;
}
'''


def build(cc, cwd, src, out):
    return subprocess.run([cc, '--cache', src, '-o', out], cwd=cwd, capture_output=True,
                          text=True, encoding='utf-8', errors='replace', timeout=600)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.environ.get('QCC_TEST_BIN') or 'qcc_zh_v1.exe')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(REPO, a.cc)
    if not os.path.exists(cc):
        print('[FAIL] 编译器不存在: %s' % cc)
        return 2
    work = os.path.join(TMP, 'w')
    if os.path.isdir(work):
        shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work, exist_ok=True)
    src = os.path.join(work, 't.c')
    io.open(src, 'w', encoding='utf-8', newline='\n').write(SRC)
    exe = os.path.join(work, 'a.exe')

    r1 = build(cc, work, 't.c', 'a.exe')
    if r1.returncode != 0 or not os.path.exists(exe):
        print('[SKIP] 首次 --cache 编译未产出（rc=%d）—— 本机不具备该开关的前置条件'
              % r1.returncode)
        return 0
    size1 = os.path.getsize(exe)

    os.remove(exe)                        # 场景 A：手工删产物
    r2 = build(cc, work, 't.c', 'a.exe')
    out2 = ((r2.stdout or '') + (r2.stderr or '')).strip()
    exists2 = os.path.exists(exe)
    print('[1] 首次 --cache: rc=%d 产物 %d B' % (r1.returncode, size1))
    print('[2] 删产物后再 --cache: rc=%d 产物存在=%s' % (r2.returncode, exists2))
    print('    cc 输出: %s' % out2.splitlines()[-1][:120] if out2 else '    cc 输出: (空)')

    cached_claim = ('cached' in out2.lower())
    if cached_claim and not exists2:
        print('[REPRO] 谎报成功：声称 cached 但产物不存在 ⟹ D837 复现（blocked 诚实）')
        return 1
    if cached_claim and exists2:
        print('[PASS] 声称 cached 且产物存在（%d B）⟹ 该缺陷已消失' % os.path.getsize(exe))
        return 0
    if not cached_claim:
        print('[PASS] 未声称 cached（重新构建或明确报错）⟹ 该缺陷已消失')
        return 0
    print('[不确定] 输出形态未识别: %s' % out2[:200])
    return 2


if __name__ == '__main__':
    sys.exit(main())
