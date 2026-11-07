#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_publish_refs —— 发布镜像是否真的带上了当前修复（126③④ 的部署面）。

为什么单独一个脚本
------------------
本地 `publish` 分支修好 ≠ 用户下载到的仓库修好了。已发布的快照在
`origin/publish` / `gitee/main` / `gitcode/main` / `atomgit/main` 上，
判据必须能回答「线上那份是不是旧货」。

输出契约（机器可读，供 `check_failopen_negatives.py` G 与台账 blocked 行消费）：

    [CASE P ref=<ref> sha=<short>] PASS
    [CASE P ref=<ref> sha=<short>] FAIL <原因>

退出码：0 = 全部镜像都已带上修复；1 = 有镜像落在后面（= 缺陷在线上仍然活着）。

注意：本脚本**不推送**。推送是共用外部状态，需大哥点头；因此它红了就说明
「本地已修、线上未部署」，台账里对应一条 `blocked` 行等裁定。
"""
import os
import subprocess
import sys

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RELEASE_NAMES = ('main', 'publish', 'master')


def _git(*args):
    p = subprocess.run(['git'] + list(args), cwd=ROOT, capture_output=True)
    return p.returncode, p.stdout


def mirror_refs():
    """远端跟踪分支里的发布线（…/main、…/publish、…/master），且带 llms.txt。"""
    rc, out = _git('for-each-ref', '--format=%(refname:short)', 'refs/remotes/')
    refs = [x for x in out.decode('utf-8', 'surrogateescape').splitlines() if x]
    got = []
    for r in refs:
        if r.rsplit('/', 1)[-1] not in RELEASE_NAMES:
            continue
        if _git('cat-file', '-e', '%s:llms.txt' % r)[0] == 0:
            got.append(r)
    return sorted(got)


def short(ref):
    rc, out = _git('rev-parse', '--short', ref)
    return out.decode('utf-8', 'surrogateescape').strip() if rc == 0 else '?'


def problems(ref):
    """该镜像缺什么。返回 (问题清单, 是否带 llms 对象)。"""
    bad = []
    rc, out = subprocess.run([sys.executable, os.path.join(ROOT, 'scripts', 'gen_llms_status.py'),
                              '--check', '--ref', ref],
                             cwd=ROOT, capture_output=True).returncode, None
    if rc != 0:
        bad.append('llms 事实块缺失或与现算不一致')
    rc, blob = _git('show', '%s:.github/workflows/status-sync.yml' % ref)
    if rc != 0:
        bad.append('缺 .github/workflows/status-sync.yml')
    else:
        txt = blob.decode('utf-8', 'surrogateescape')
        if 'gen_llms_status' not in txt:
            bad.append('status-sync 未覆盖 llms')
        if '不判失败' in txt:
            bad.append('status-sync 仍 fail-open（跳过不判失败）')
    return bad


def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument('--ref', default='',
                    help='只检查指定 ref（修前腿用：如 `--ref b2b04f19` 复现「部署前」）')
    a = ap.parse_args()

    refs = [a.ref] if a.ref else mirror_refs()
    if not refs:
        print('[SKIP] 无发布镜像（远端跟踪分支里没有带 llms.txt 的 main/publish/master）')
        return 0
    red = []
    for r in refs:
        bad = problems(r)
        tag = '[CASE P ref=%s sha=%s]' % (r, short(r))
        if bad:
            red.append(r)
            print('%s FAIL %s' % (tag, '；'.join(bad)))
        else:
            print('%s PASS' % tag)
    if red:
        print('=== 发布镜像: FAIL %d/%d 落后（%s）===' % (len(red), len(refs), ', '.join(red)))
        return 1
    print('=== 发布镜像: PASS %d/%d 均已带上修复 ===' % (len(refs), len(refs)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
