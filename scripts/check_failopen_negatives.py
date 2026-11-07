#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_failopen_negatives —— fail-open 族负例判据（ORD-20261105-126 / ORD-20261106-129）。

每组都构造**错误输入**，要求进程 exit≠0 且打印可捕获标记：
  A. 缺 qcc_rt.c          ⟹ rc≠0 + "[ERR] 运行库"
  B. IR 受限档满          ⟹ rc≠0 + "IR-DANG-FULL"
  C. jypkg 清单缺字段     ⟹ rc=4 + "[ERR] 包表"
  D. 判据缺 seed          ⟹ rc≠0 + "[FAIL] D 组"

用法: python scripts/check_failopen_negatives.py [--cc qcc_zh_v1.exe] [--skip-dang]
退出码: 0 = 全组 fail-closed；1 = 有组仍 fail-open
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def _ledger_repo():
    return os.environ.get('QCC_LEDGER_REPO') or os.path.join(os.path.dirname(ROOT), 'grok-build')


def _git(*args):
    import subprocess as _sp
    p = _sp.run(['git'] + list(args), cwd=ROOT, capture_output=True)
    return p.returncode, p.stdout


def _release_refs():
    """判据对象 = **同时带** `llms.txt` 与 `.github/workflows/status-sync.yml` 的本地分支。

    理由：126③④ 是「对外单页数字会漂 + 自动同步漏了它」这一对缺陷。
    只有同时装了「被同步的对象」和「负责同步的机制」的 ref 才是**活发布线**；
    历史上那些只有 llms.txt 的旧分支是快照，不构成待修对象。
    """
    rc, out = _git('for-each-ref', '--format=%(refname:short)', 'refs/heads/')
    refs = [x for x in out.decode('utf-8', 'surrogateescape').splitlines() if x]
    live = []
    for r in refs:
        a = _git('cat-file', '-e', '%s:llms.txt' % r)[0] == 0
        b = _git('cat-file', '-e', '%s:.github/workflows/status-sync.yml' % r)[0] == 0
        if a and b:
            live.append(r)
    return live


def _gen_llms_check(ref):
    """跑真正的生成器做比对（不是 grep 源码）。"""
    rc, out = run([sys.executable, os.path.join(ROOT, 'scripts', 'gen_llms_status.py'),
                   '--check', '--ref', ref], ROOT, timeout=900)
    return rc, out


def _stale_refs():
    """有 llms.txt、但**没有** `GEN:LLMS:FACTS` 块的本地分支（历史快照）。

    它们不是待修对象，但正好可当 **G 的负例样本**：
    若生成器对它们也报绿，说明比对是空转的。
    """
    rc, out = _git('for-each-ref', '--format=%(refname:short)', 'refs/heads/')
    refs = [x for x in out.decode('utf-8', 'surrogateescape').splitlines() if x]
    stale = []
    for r in refs:
        rc1, blob = _git('show', '%s:llms.txt' % r)
        if rc1 != 0:
            continue
        txt = blob.decode('utf-8', 'surrogateescape')
        if 'GEN:LLMS:FACTS:BEGIN' not in txt:
            stale.append(r)
    return stale


def _remote_llms_refs():
    """远端跟踪分支里带 llms.txt 的那些 —— 它们是**已部署快照**，不是待修对象。

    本地判据不把远端镜像算作通过（那是自欺），但要把「还没推上去」这件事**说出来**，
    否则读者会以为线上已经绿了。
    """
    rc, out = _git('for-each-ref', '--format=%(refname:short)', 'refs/remotes/')
    refs = [x for x in out.decode('utf-8', 'surrogateescape').splitlines() if x]
    got = []
    for r in refs:
        # 只看发布线（…/main、…/publish、…/master），dependabot 之类的临时分支不算
        if r.rsplit('/', 1)[-1] not in ('main', 'publish', 'master'):
            continue
        if _git('cat-file', '-e', '%s:llms.txt' % r)[0] == 0:
            got.append(r)
    return got


def _short(ref):
    rc, out = _git('rev-parse', '--short', ref)
    return out.decode('utf-8', 'surrogateescape').strip() if rc == 0 else '?'


def _mirror_blocked_row():
    """台账里有没有一条 `blocked` 行在等「发布镜像」的裁定？

    有 ⟹ 镜像落后是**已挂账的外部条件**，判据输出 `EXTERNAL-BLOCKED row=<ID>`；
    没有 ⟹ 镜像落后就是**未收口的缺陷**，G/H 必须判红。绝不允许「绿 + 自我承认未修」。
    """
    import json
    p = os.path.join(_ledger_repo(), '_TASKLIST.json')
    try:
        d = json.load(open(p, encoding='utf-8'))
    except Exception:
        return None
    for r in d.get('rows', []):
        if r.get('status') != 'blocked':
            continue
        blob = ' '.join(str(r.get(k, '')) for k in ('title', 'next_step', 'evidence'))
        if '镜像' in blob and '裁定' in str(r.get('next_step') or ''):
            return r.get('id')
    return None


def _mirror_cases(tag, mirrors, is_bad):
    """把远端镜像逐条落成机器状态行：PASS / EXTERNAL-BLOCKED row=<ID> / FAIL。"""
    behind = [r for r in mirrors if is_bad(r)]
    if not behind:
        return ['[CASE %s ref=%s sha=%s] PASS' % (tag, r, _short(r)) for r in mirrors], None
    row = _mirror_blocked_row()
    if not row:
        return (['[CASE %s ref=%s sha=%s] FAIL 镜像落后且台账没有对应的 blocked 行'
                 % (tag, r, _short(r)) for r in behind],
                '镜像落后（%s）而台账无对应 blocked 行 ⟹ 属未收口缺陷' % ', '.join(behind))
    return (['[CASE %s ref=%s sha=%s] EXTERNAL-BLOCKED row=%s' % (tag, r, _short(r), row)
             for r in behind], None)


def _check_llms_numbers():
    """126③ llms 数字：对发布线用生成器现算比对，并用历史快照做负例自证非空转。"""
    refs = _release_refs()
    if not refs:
        return ('[SKIP] G llms 数字（无发布线：没有任何本地分支同时带 llms.txt 与 status-sync.yml'
                ' —— 126③ 对象确实不存在）', 'skip')
    bad = []
    for r in refs:
        rc, out = _gen_llms_check(r)
        if rc != 0:
            bad.append('%s rc=%d\n%s' % (r, rc, out.strip()))
    if bad:
        return ('[FAIL] G llms 数字与机器事实不一致（%s）：\n%s'
                % (', '.join(refs), '\n'.join(bad)[:2500]), 'fail')

    # 负例自证 1（干净 clone 也能跑）：生成器自检 —— 4 组合成坏输入必须全部判红。
    rc, out = run([sys.executable, os.path.join(ROOT, 'scripts', 'gen_llms_status.py'),
                   '--self-test'], ROOT, timeout=300)
    if rc != 0:
        return ('[FAIL] G 比对器自证失败（gen_llms_status.py --self-test rc=%d）：\n%s'
                % (rc, (out or '').strip()[:1200]), 'fail')
    neg = ('[PASS] G- 负例：生成器自检 5/5（正常绿 / 块内改数红 / 缺块红 / '
           '块外重复行数红 / 块外重复多语言规模红）')

    # 负例自证 2（有历史快照时追加）：拿「有 llms.txt 但无事实块」的真实 ref 喂同一比对 ⟹ 必须判红。
    stale = _stale_refs()
    if stale:
        rc2, _ = _gen_llms_check(stale[0])
        if rc2 == 0:
            return ('[FAIL] G 比对空转：%s 的 llms.txt 没有事实块却报绿 —— 判据没在比数字'
                    % stale[0], 'fail')
        neg += ('\n[PASS] G- 补充：%s（历史快照，无事实块）⟹ 判红 rc=%d（比对非空转）'
                % (stale[0], rc2))

    # 远端镜像 = 已部署快照：逐条落成 PASS / EXTERNAL-BLOCKED row=… / FAIL。
    lines, why = _mirror_cases('G', _remote_llms_refs(), lambda r: _gen_llms_check(r)[0] != 0)
    if why:
        return ('[FAIL] G %s\n%s' % (why, '\n'.join(lines)), 'fail')
    if lines:
        neg += '\n' + '\n'.join(lines)
    return ('[PASS] G llms 数字由生成器现算比对通过（refs: %s）\n%s'
            % (', '.join(refs), neg), 'ok')


def _status_sync_problems(label, txt, gen_bytes):
    """126④ 的纯规则：给定 status-sync.yml 文本与发布线上的生成器字节，返回问题清单。

    抽成纯函数是为了让 `--self-test` / H- 负例能直接喂**合成输入**，
    证明这三条规则确实会咬人（而不是只对着现实中已修好的文件报绿）。
    """
    bad = []
    if 'gen_llms_status' not in txt:
        bad.append('%s: 同步链里没有 gen_llms_status ⟹ llms.txt 仍靠人肉抄写' % label)
    for f in ('llms.txt', 'llms-full.txt'):
        if ('git add' in txt) and (f not in txt):
            bad.append('%s: `git add` 不含 %s ⟹ 生成后不会被提交' % (label, f))
    if '不判失败' in txt:
        bad.append('%s: 生成器缺失时仍「跳过（不判失败）」⟹ 同步机制本身 fail-open' % label)
    if gen_bytes is None:
        bad.append('%s: 缺 scripts/gen_llms_status.py（同步机制无法现算）' % label)
    return bad


def _check_status_sync():
    """126④ 文档同步：发布线的 status-sync.yml 必须把 llms 纳入同步且 fail-closed。"""
    refs = _release_refs()
    if not refs:
        return ('[SKIP] H status-sync（无发布线：没有任何本地分支同时带 llms.txt 与 status-sync.yml'
                ' —— 126④ 对象确实不存在）', 'skip')
    here = open(os.path.join(ROOT, 'scripts', 'gen_llms_status.py'), 'rb').read()
    bad = []
    first_txt = None
    for r in refs:
        rc, out = _git('show', '%s:.github/workflows/status-sync.yml' % r)
        if rc != 0:
            bad.append('%s: 读不到 status-sync.yml' % r)
            continue
        txt = out.decode('utf-8', 'surrogateescape')
        if first_txt is None:
            first_txt = txt
        rc1, pub = _git('show', '%s:scripts/gen_llms_status.py' % r)
        pub = pub if rc1 == 0 else None
        bad.extend(_status_sync_problems(r, txt, pub))
        # 同一生成器必须在两条线上逐字节一致，否则发布线跑的是另一份实现
        if pub is not None and pub.replace(b'\r\n', b'\n') != here.replace(b'\r\n', b'\n'):
            bad.append('%s: scripts/gen_llms_status.py 与施工线版本不一致 ⟹ 两处实现会漂' % r)
    if bad:
        return ('[FAIL] H status-sync 未覆盖 llms 或自身 fail-open：\n  - %s' % '\n  - '.join(bad), 'fail')

    # 负例自证：把**真实的那份 yml** 逐条改坏，三条规则必须每次都咬住。
    probe = first_txt
    neg_fail = []
    variants = [
        ('删掉 gen_llms_status', probe.replace('gen_llms_status', 'gen_readme_status')),
        ('git add 漏 llms-full.txt', probe.replace('llms-full.txt', '')),
        ('恢复「不判失败」', probe.replace('exit 1', "Write-Host '跳过（不判失败）'; exit 0", 1)),
    ]
    for label, mutated in variants:
        if mutated == probe:
            neg_fail.append('%s：改写没生效（原文里没有该串）⟹ 无法自证' % label)
            continue
        if not _status_sync_problems(label, mutated, b'x'):
            neg_fail.append('%s：改坏了却仍报绿 ⟹ 规则空转' % label)
    if not _status_sync_problems('缺生成器', probe, None):
        neg_fail.append('缺生成器：却仍报绿 ⟹ 规则空转')
    if neg_fail:
        return ('[FAIL] H 负例自证失败：\n  - %s' % '\n  - '.join(neg_fail), 'fail')
    neg = ('[PASS] H- 负例：同一份 yml 改坏 3 处（无 gen_llms_status / git add 漏 llms-full.txt / '
           '恢复「不判失败」）+ 缺生成器，4/4 全部被咬住')

    # 远端镜像：机制在那边是不是也还在旧状态 —— 同样落成 PASS / EXTERNAL-BLOCKED row=… / FAIL。
    def _mirror_bad(r):
        rc, blob = _git('show', '%s:.github/workflows/status-sync.yml' % r)
        if rc != 0:
            return True
        rc1, pub = _git('show', '%s:scripts/gen_llms_status.py' % r)
        return bool(_status_sync_problems(r, blob.decode('utf-8', 'surrogateescape'),
                                          pub if rc1 == 0 else None))

    lines, why = _mirror_cases('H', _remote_llms_refs(), _mirror_bad)
    if why:
        return ('[FAIL] H %s\n%s' % (why, '\n'.join(lines)), 'fail')
    if lines:
        neg += '\n' + '\n'.join(lines)
    return ('[PASS] H status-sync 已把 llms 纳入同步且 fail-closed（refs: %s）\n%s'
            % (', '.join(refs), neg), 'ok')


def _tamper_and_expect_red(fname, mutate, label):
    """附加组：篡改台账仓某文件后跑它自己的 check_tasklist，必须判红（证明闸真读数字）。"""
    ledger = _ledger_repo()
    tk = os.path.join(ledger, 'scripts', 'check_tasklist.py')
    p = os.path.join(ledger, fname)
    if not (os.path.exists(p) and os.path.exists(tk)):
        return ('[SKIP] %s（台账仓不可用：%s）' % (label, ledger), 'skip')
    bak = p + '.bak_failopen'
    shutil.copyfile(p, bak)
    try:
        t = open(p, 'rb').read().decode('utf-8', 'surrogateescape')
        open(p, 'w', encoding='utf-8', newline='\n').write(mutate(t))
        rc, out = run([sys.executable, tk], ledger, timeout=900)
        if rc != 0:
            return ('[PASS] %s ⟹ 台账闸判红 rc=%d' % (label, rc), 'ok')
        return ('[FAIL] %s ⟹ 台账闸仍绿（说明它没读这个数字）' % label, 'fail')
    finally:
        shutil.copyfile(bak, p)
        try:
            os.remove(bak)
        except OSError:
            pass


def run(cmd, cwd, timeout=1800):
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True,
                       encoding='utf-8', errors='replace', timeout=timeout)
    return r.returncode, (r.stdout or '') + (r.stderr or '')


# ---- 故障注入：在同一进程内 import 被测脚本、替换依赖、调它自己的 main() ----
def _inject(script_name, patcher, argv):
    """跑 `scripts/<script_name>` 的 main()，期间由 patcher 注入故障。

    这样测的是**它自己的代码路径**（不是 grep 源码、也不是另写一份逻辑）。
    """
    import importlib.util
    import io as _io
    path = os.path.join(ROOT, 'scripts', script_name)
    spec = importlib.util.spec_from_file_location('_inj_' + script_name[:-3].replace('-', '_'), path)
    mod = importlib.util.module_from_spec(spec)
    saved_argv = sys.argv
    sys.argv = [script_name] + argv
    buf = _io.StringIO()
    buf.reconfigure = lambda **k: None      # 被测脚本 import 时会 sys.stdout.reconfigure(...)
    old_stdout = sys.stdout
    sys.stdout = buf
    undo = None
    try:
        spec.loader.exec_module(mod)
        undo = patcher()
        rc = mod.main() if hasattr(mod, 'main') else 99
    except SystemExit as e:
        rc = e.code if isinstance(e.code, int) else 99
    except Exception as e:                       # 未捕获异常也算 fail-closed（rc≠0）
        buf.write('\n[exception] %r\n' % (e,))
        rc = 98
    finally:
        if undo:
            undo()
        sys.stdout = old_stdout
        sys.argv = saved_argv
    return rc, buf.getvalue()


def _patch_missing_main():
    """把 `os.path.exists(...qcc_zh.jy)` 注入成 False —— 触发 D2「缺主源」分支。"""
    real = os.path.exists

    def fake(p):
        try:
            if str(p).replace('\\', '/').endswith('srclib_jiayan/qcc_zh.jy'):
                return False
        except Exception:
            pass
        return real(p)

    os.path.exists = fake
    return lambda: setattr(os, 'exists', real)


def _patch_remove_oserror():
    """把 `os.remove` 对 **`.opt.asm`** 的调用换成抛 OSError 的桩。

    只针对那一个后缀：`check_ord34_scrash.py` 里对它的删除**只有收尾那一处**
    （前置清理会因为文件不存在而跳过），因此注入点就是它自己的「清理临时文件」分支。
    """
    real = os.remove

    def fake(p, *a, **k):
        if str(p).replace('\\', '/').endswith('.opt.asm'):
            raise OSError(13, 'injected failure (skeptic #0 F)')
        return real(p, *a, **k)

    os.remove = fake
    return lambda: setattr(os, 'remove', real)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(ROOT, 'qcc_zh_v1.exe'))
    ap.add_argument('--dang-lines', type=int, default=150000)
    ap.add_argument('--skip-dang', action='store_true')
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    red = []
    skips = []

    with tempfile.TemporaryDirectory(prefix='qcc_failopen_') as tmp:
        # A. missing runtime
        d = os.path.join(tmp, 'a')
        os.makedirs(d)
        c2 = os.path.join(d, 'cc.exe')
        shutil.copyfile(cc, c2)
        src = os.path.join(d, 't.jy')
        open(src, 'w', encoding='utf-8').write('整 主(空) {\n  整 a = 1;\n  返 a;\n}\n')
        env = dict(os.environ)
        env.pop('QCC_RT_PATH', None)
        rc, txt = run([c2, 't.jy', '-o', 't.exe'], d, timeout=300)
        ok = rc != 0 and '[ERR] 运行库' in txt
        print('[%s] A 缺运行库 rc=%d marker=%s' % ('PASS' if ok else 'FAIL', rc, '[ERR] 运行库' in txt))
        if not ok:
            red.append('A')

        # B. IR restricted table full
        if not a.skip_dang:
            d = os.path.join(tmp, 'b')
            os.makedirs(d)
            src = os.path.join(d, 'big.jy')
            parts = ['整 主(空) {', '  整 a = 0;', '  整 b = 1;']
            parts += ['  a = b + 1;'] * a.dang_lines
            parts += ['  返 a;', '}', '']
            open(src, 'w', encoding='utf-8').write('\n'.join(parts))
            rc, txt = run([cc, src, '-o', os.path.join(d, 'big.exe')], ROOT)
            ok = rc != 0 and 'IR-DANG-FULL' in txt
            print('[%s] B IR 档满 rc=%d marker=%s' % ('PASS' if ok else 'FAIL', rc, 'IR-DANG-FULL' in txt))
            if not ok:
                red.append('B')
        else:
            print('[SKIP] B IR 档满（--skip-dang）')
            skips.append('B')

        # C. jypkg missing field
        d = os.path.join(tmp, 'c')
        os.makedirs(d)
        pkg = os.path.join(d, 'jy_pkg.exe')
        rc, txt = run([cc, 'tools/jypkg/jy_pkg.jy', '-o', pkg], ROOT)
        if rc != 0 or not os.path.exists(pkg):
            print('[FAIL] C jypkg 构建失败 rc=%d' % rc)
            red.append('C')
        else:
            bad = os.path.join(d, 'bad.json')
            open(bad, 'w', encoding='utf-8').write('{"包":"app","包表":[{"名":"libb"}]}\n')
            rc, txt = run([pkg, 'install', bad, d], d, timeout=300)
            ok = rc != 0 and '[ERR] 包表' in txt
            print('[%s] C jypkg 缺字段 rc=%d marker=%s' % ('PASS' if ok else 'FAIL', rc, '[ERR] 包表' in txt))
            if not ok:
                red.append('C')

        # D. criterion without seed
        rc, txt = run([sys.executable, os.path.join(ROOT, 'scripts', 'check_build_env_not_baked.py'),
                       '--seed', os.path.join(tmp, 'no_such_seed.exe')], ROOT, timeout=900)
        ok = rc != 0 and '[FAIL] D 组' in txt
        print('[%s] D 判据缺 seed rc=%d marker=%s' % ('PASS' if ok else 'FAIL', rc, '[FAIL] D 组' in txt))
        if not ok:
            red.append('D')

        # E. **第二处假 PASS**：`check_build_env_not_baked.py` 的 D2「缺主源」分支。
        #    做法 = 在同一进程里 import 目标脚本、把 `os.path.exists(srclib_jiayan/qcc_zh.jy)`
        #    注入成 False，再调**它自己的 main()** ⟹ 必须 return 1 且打印「不得报绿」。
        rc, txt = _inject('check_build_env_not_baked.py', _patch_missing_main, ['--seed', os.path.join(ROOT, 'qcc_zh_seed.exe')])
        ok = rc != 0 and '不得报绿' in txt
        print('[%s] E 缺主源 rc=%d marker=%s' % ('PASS' if ok else 'FAIL', rc, '不得报绿' in txt))
        if not ok:
            red.append('E')

        # F. **吞异常**：`check_ord34_scrash.py` 的清理 OSError 分支。
        #    注入：把它自己的 `os.remove` 换成抛 OSError 的桩，再调它的 main() ⟹ 必须 return 1 + [FAIL]
        rc, txt = _inject('check_ord34_scrash.py', _patch_remove_oserror, [])
        ok = rc != 0 and '[FAIL] 清理临时文件失败' in txt
        print('[%s] F 注入 OSError rc=%d marker=%s' % ('PASS' if ok else 'FAIL', rc, '清理临时文件失败' in txt))
        if not ok:
            red.append('F')

    # ---- 126③④ 的真实对象：llms.txt 数字 / .github/workflows/status-sync.yml 文档同步 ----
    # skeptic #0 打回点：原来这里拿「篡改 _GEN_COUNTS / _ATLAS_ENTRY 再跑台账闸」冒充 G/H ——
    # 那测的是**台账闸**，不是 126③④ 的对象。现在按**对象本身**判：
    #   · 对象存在 ⟹ 用生成器从该 ref 的树现算比对，做得成 ⟹ PASS，做不成 ⟹ FAIL；
    #   · 对象不存在（没有任何本地分支同时带 llms.txt 与 status-sync.yml）⟹ 显式 SKIP；
    #   · SKIP 会被记进 `skips`，收尾那行**不再声称「全部 fail-closed」**（旧版就是在这撒谎）。
    _g = _check_llms_numbers()
    print(_g[0])
    if _g[1] == 'fail':
        red.append('G')
    elif _g[1] == 'skip':
        skips.append('G')
    _h = _check_status_sync()
    print(_h[0])
    if _h[1] == 'fail':
        red.append('H')
    elif _h[1] == 'skip':
        skips.append('H')

    # G2/H2（附加，不在 126 清单里）：证明台账闸**真读数字** —— 篡改计数 ⟹ 必须判红。
    # 计数会随台账增长（325 → 326 …），所以不能写死某个数，只把**首个数**加一。
    def _bump_first(t):
        return __import__('re').sub(r'^(\d+)',
                                    lambda m: str(int(m.group(1)) + 1),
                                    t, count=1, flags=__import__('re').M)

    _g2 = _tamper_and_expect_red('_GEN_COUNTS.txt', _bump_first, 'G2 计数篡改')
    print(_g2[0])
    if _g2[1] == 'fail':
        red.append('G2')
    _h2 = _tamper_and_expect_red('_ATLAS_ENTRY.md', lambda t: __import__('re').sub(r'blocked \d+', 'blocked 0', t, count=1), 'H2 文档篡改')
    print(_h2[0])
    if _h2[1] == 'fail':
        red.append('H2')

    if red:
        print('=== fail-open 负例: FAIL -> %s ===' % red)
        return 1
    if skips:
        print('=== fail-open 负例: PASS（但 %s 为 SKIP —— 对象缺席，未验证，不是 fail-closed）==='
              % ', '.join(skips))
    else:
        cases = [l for l in (_g[0] + '\n' + _h[0]).splitlines() if l.startswith('[CASE ')]
        blocked = [l for l in cases if 'EXTERNAL-BLOCKED' in l]
        print('=== fail-open 负例: PASS（A–H 8/8；SKIP=0；附证 G-/H- 各 1；'
              '镜像 CASE %d 条，其中 EXTERNAL-BLOCKED %d 条）==='
              % (len(cases), len(blocked)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
