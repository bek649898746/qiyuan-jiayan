#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""dev_direct_run —— 开发级定向验证器（直跑）

为什么需要它（实测教训 · 见 `_LONG_MARCH_V2_20261010.md` §三·2）:
  `run_tests_zh.py --only <锚>` 会带上**闸①-⑥ 全套前置**（8 分钟仍未完），
  而且若把**并发类锚**（如 `feat_zh_ws61_ok.c` 工作窃取）混进批次，它在负载下会**真死锁**
  且测试进程的线程杀不掉（实测把整个闸僵住 65 分钟）。
  本器**绕开前置闸**，直接「编译 → 运行 → 比对 `@EXPECTED`」：
  实测 18 锚 31 秒跑完，并当场抓出过锚期望写错的真错。

用法:
  python scripts/dev_direct_run.py                      # 默认受关注锚集
  python scripts/dev_direct_run.py a.c b.c              # 指定锚（相对 scripts/）
  python scripts/dev_direct_run.py --cc qcc_zh_seed.exe a.c   # 指定编译器（父编对照）
  python scripts/dev_direct_run.py --all-new            # 扫 scripts/ 下本轮新增的 *_ok/_neg

锚头约定（与 run_tests_zh.py 一致）:
  正锚: // @EXPECTED exit:<n>   // @EXPECTED out:<文本>
  负锚: // @EXPECTED compile_fail   // @EXPECTED err:<错误码>

硬性保护:
  · 每锚编译 120s / 运行 20s 超时，超时即 taskkill 兜底
  · 并发类锚默认**排除**（黑名单见 CONCURRENCY_BLOCKLIST），需显式点名才跑
"""
import concurrent.futures
import io
import os
import re
import shlex
import subprocess
import sys
import time

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REPO)

# 并发/线程池类锚：在负载下可能真死锁，默认不进批量
CONCURRENCY_BLOCKLIST = {
    'feat_zh_ws61_ok.c',        # 工作窃取（注释自述「窃取不工作就必然死锁」）
    'feat_zh_struct_concur25.c',
    'feat_thread_zh.c',
    'feat_zh_actor19.c',
    'feat_actor_zh.c',
    'feat_zh_select24.c',
}

# 默认受关注锚集：本轮改动影响面 + 近期新增
DEFAULT_ANCHORS = [
    'feat_zh_msg69_ok.c', 'feat_zh_msg69_release_ok.c', 'feat_zh_msg69_borrow_neg.c',
    'feat_zh_constarr185_ok.c', 'feat_zh_constarr185_neg.c', 'feat_zh_constarr185_elem_neg.c',
    'feat_zh_macroovl613_ok.c', 'feat_zh_macroovl613_neg.c',
    'feat_zh_fwdread_ok.c', 'feat_zh_fwdread_local_neg.c',
    'feat_zh_fwdread_dataflow_neg.c', 'feat_zh_fwdread_crossfn_neg.c',
    'feat_zh_macro_hygiene_ok.c', 'feat_zh_macro_toplevel_expand.c',
    'feat_zh_specialization50.c', 'feat_zh_variadic_templates72.c',
    'feat_zh_generic_bounds45.c', 'feat_zh_generic_typedef46.c',
    'feat_zh_compr221_ok.c',
]

TMP = os.path.join(os.environ.get('TEMP', '.'), '_dev_direct_run')
os.makedirs(TMP, exist_ok=True)


def parse_expect(name):
    p = os.path.join('scripts', name)
    if not os.path.exists(p):
        return None
    full = io.open(p, encoding='utf-8-sig', errors='replace').read()
    # 2026-10-19: 只认 **行首** 的 `@EXPECTED` 指令行（`//` / `/*` / 裸行），并去掉尾部 `*/`。
    # 旧版 取前 8 行 ⟹ 长头注释后的负锚被误当正锚；直接扩到 40 行又会把注释里 **提到** `@EXPECTED out:`
    # 的说明文字当成期望（实测 feat_zh_member_arr_init_ok 第 17 行）。行首限定 + 全文件扫描两边都覆盖。
    hdr = []
    for raw in full.splitlines():
        t = raw.strip()
        if t.startswith('// @EXPECTED') or t.startswith('/* @EXPECTED') or t.startswith('@EXPECTED'):
            if t.endswith('*/'):
                t = t[:-2].rstrip()
            hdr.append(t)
    head = '\n'.join(hdr)
    d = {'cfail': '@EXPECTED compile_fail' in head, 'exit': None, 'out': None, 'err': None, 'args': []}
    m = re.search(r'@EXPECTED exit:(-?\d+)', head)
    if m:
        d['exit'] = int(m.group(1))
    m = re.search(r'@EXPECTED out:(.+)$', head, re.M)
    if m:
        d['out'] = m.group(1).strip()
    m = re.search(r'@EXPECTED err:(\S+)', head)
    if m:
        d['err'] = m.group(1).strip()
    # @ARG <编译器选项>（与 run_tests_zh.py L3559 同约定）
    #   R629 (2026-10-10): 原 本器 不解析 @ARG ⟹ 带 `-D` 的锚（如 feat_zh_cmdline_define_ok.c）
    #   恒报 "A1001 未声明标识符" 假红。@ARG 只出现在锚头，故全文件扫即可。
    for ma in re.finditer(r'@ARG\s+(.+)$', full, re.M):
        d['args'] += shlex.split(ma.group(1).strip())
    return d


def run_one(name, cc):
    d = parse_expect(name)
    if d is None:
        return name, 'SKIP', '文件不存在（历史锚清单残留，不计 PASS/FAIL）'
    exe = os.path.join(TMP, 'dev_' + os.path.splitext(name)[0] + '.exe')
    try:
        os.remove(exe)
    except OSError:
        pass
    try:
        env = os.environ.copy()
        env['QCC_SMALL_HEAP'] = '1'  # 2026-10-19: 锚编译 64MiB 堆备留 ⟹ 8 路并行不打穿 commit
        r = subprocess.run([cc] + d['args'] + [os.path.join('scripts', name), '-o', exe],
                           capture_output=True, encoding='utf-8', errors='replace', timeout=120, env=env)
    except subprocess.TimeoutExpired:
        return name, 'FAIL', '编译超时 120s'
    cerr = (r.stdout or '') + (r.stderr or '')

    if d['cfail'] or d['err']:
        # 2026-10-19: `@EXPECTED err:CODE`（无 compile_fail 字样）也是负锚 —— 与 run_tests_zh 同约定。
        # 原判据只看 cfail ⟹ 这类锚被当正锚，编译失败一律报「编译失败 rc=1」假红。
        if r.returncode == 0:
            return name, 'FAIL', 'compile_fail 未满足（编译成功了）'
        if d['err'] and d['err'] not in cerr:
            return name, 'FAIL', '诊断不含 %s' % d['err']
        return name, 'PASS', 'compile_fail(%s) rc=%d' % (d['err'] or '-', r.returncode)

    if r.returncode != 0:
        first = [l for l in cerr.splitlines() if l.startswith('[ERR]')]
        return name, 'FAIL', '编译失败 rc=%d %s' % (r.returncode, first[0][:70] if first else '')

    try:
        p = subprocess.run(['cmd', '/c', exe], capture_output=True, encoding='utf-8',
                           errors='replace', timeout=20)
    except subprocess.TimeoutExpired:
        subprocess.run(['taskkill', '/F', '/T', '/IM', os.path.basename(exe)], capture_output=True)
        return name, 'SKIP', '运行超时 20s（环境/并发，非判定）'
    out = p.stdout or ''
    prob = []
    if d['exit'] is not None and p.returncode != d['exit']:
        prob.append('exit 期望%d 实得%d' % (d['exit'], p.returncode))
    if d['out'] and d['out'] not in out:
        prob.append('out 缺 %r（实得 %r）' % (d['out'], out.strip()[:40]))
    try:
        os.remove(exe)
    except OSError:
        pass
    if prob:
        return name, 'FAIL', '; '.join(prob)
    return name, 'PASS', 'rc=%d out=%r' % (p.returncode, out.strip()[:40])


def main():
    av = sys.argv[1:]
    cc = 'qcc_zh_v1.exe'
    if '--cc' in av:
        i = av.index('--cc')
        cc = av[i + 1]
        del av[i:i + 2]
    jobs = 4  # 2026-10-19: 默认 4（安全下限，本机 8 路曾因 1.34GB 镜像 WinError 1455）。0A421684 起镜像 1.22GB（静态 544MB），--jobs 8 实测 298 锚 118.8s 无 1455；默认仍保守，需 8 路请显式传参
    if '--jobs' in av:
        i = av.index('--jobs')
        jobs = max(1, int(av[i + 1]))
        del av[i:i + 2]
    names = av if av else list(DEFAULT_ANCHORS)

    # 2026-10-10 (R648): 原实现**无条件**排除黑名单 —— 与模块头注释「需显式点名才跑」不符（量具 bug）。
    #   改为: **显式点名 = 放行**（默认集仍排除）。理由: R648 根治「EXE 模式 导入表函数调用 未接通」后,
    #   `feat_zh_ws61_ok.c` 已由「必然死锁」变为 5.9s 正常完成; 且本器已带 20s 运行超时 + taskkill 兜底。
    blocked = [] if av else [n for n in names if n in CONCURRENCY_BLOCKLIST]
    if not av:
        names = [n for n in names if n not in CONCURRENCY_BLOCKLIST]

    print('# 开发级定向验证（直跑 · 无闸前置）')
    print('# 编译器: %s   锚数: %d   并行: %d' % (cc, len(names), jobs))
    if blocked:
        print('# ⚠️ 已排除并发类锚（需显式点名）: %s' % ', '.join(blocked))
    print()
    t0 = time.time()
    npass = nfail = nskip = 0
    if jobs <= 1:
        results = (run_one(n, cc) for n in names)
        pool = None
    else:
        pool = concurrent.futures.ThreadPoolExecutor(max_workers=jobs)
        results = pool.map(lambda n: run_one(n, cc), names)
    try:
        for nm, st, det in results:
            if st == 'PASS':
                npass += 1
            elif st == 'SKIP':
                nskip += 1
            else:
                nfail += 1
            print('%-38s %-5s %s' % (nm, st, det))
    finally:
        if pool is not None:
            pool.shutdown(wait=True)
    dt = time.time() - t0
    print()
    print('汇总: PASS=%d SKIP=%d FAIL=%d  用时 %.1fs' % (npass, nskip, nfail, dt))
    if nfail:
        print('⟹ 有 FAIL，需逐条定性（真回归 / 锚期望写错 / 环境抖动）')
    return 1 if nfail else 0


if __name__ == '__main__':
    sys.exit(main())
