# -*- coding: utf-8 -*-
# ⚠️ 本文件 = `scripts/_r790_scan.py` 的 **入仓副本**（逐字节 相同，sha256=a98db39de1809928 …）。
# 原因（判据 4）: 原文件名 被 `.gitignore` 的 `_*.py`（L116）命中 ⟹ `git ls-files` **不可见**
#   ⟹ 干净 clone 上 台账 的 zlib 门（本文件的 `--cc <引擎> zlib`）跑不动。
# 同步纪律: 若 原 `_r790_scan.py` 有改动，必须 **重跑本复制**（sha256 会变，可校验）。
# 用法: python scripts\qiyuan_ledger_zlib_scan.py --cc <引擎> --parallel 4 zlib
"""R790: 12 套语料 **逐文件 首错 聚类** 扫描器 (开发 自查 用; 官方 计数 仍 以 各 *_build.py 为准)。

用法:
    python scripts\\_r790_scan.py --cc <引擎> lua zlib libpng libjpeg redis sqlite tcc
    python scripts\\_r790_scan.py --cc <引擎> --all
    python scripts\\_r790_scan.py --cc <引擎> --all --parallel 8

产出:
    stdout = 逐套 通过数 + 首错 聚类 表 (按 族 大小 降序)
    JSON   = %TEMP%\\grok-goal-2cf400d2eaad\\implementer\\r790_clusters.json

纪律: 语料区 **只读**; 产物 一律 落 scratch 的 out_* 目录; 不 改 任何 语料 文件。
"""
import io, json, os, re, subprocess, sys, time
from concurrent.futures import ThreadPoolExecutor

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
QJ = r'C:\Users\Administrator\Desktop\qiyuan-jiayan'
C = r'D:\qiyuan_home\corpus'
GIT = r'C:\Users\Administrator\Desktop\git-2.45.2'
S = os.environ['TEMP'] + r'\grok-goal-2cf400d2eaad\implementer'
OS_ = os.path.join(S, 'out_r790')

# ---------- 各套 口径 (逐字 抄 自 语料 自带 *_build.py) ----------
SETS = {
    'lua': dict(root='lua-5.4.7', glob='*.c', flags=['-I', '{root}'], to=60),
    'zlib': dict(root='zlib-1.3.1',
                 files=['adler32.c', 'compress.c', 'crc32.c', 'deflate.c', 'gzclose.c', 'gzlib.c',
                        'gzread.c', 'gzwrite.c', 'infback.c', 'inffast.c', 'inflate.c', 'inftrees.c',
                        'trees.c', 'uncompr.c', 'zutil.c'],
                 flags=['-I', '{root}'], to=120),
    'libpng': dict(root='libpng-1.6.43', glob='*.c',
                   flags=['-I', C + r'\png_compat.h', '-I', '{root}', '-I', C + r'\zlib-1.3.1'], to=120),
    'libjpeg': dict(root='libjpeg-turbo-3.0.3', glob='*.c', flags=['-I', '{root}'], to=60),
    'redis': dict(root=r'redis-7.2.5\src', glob='*.c',
                  flags=['-I', C + r'\redis_compat.h', '-I', '{root}', '-D', 'BYTE_ORDER=1234',
                         '-D', 'LITTLE_ENDIAN=1234', '-D', 'BIG_ENDIAN=4321',
                         '-D', '__ATOMIC_RELAXED=0', '-D', '__ATOMIC_SEQ_CST=5'], to=60),
    'curl': dict(root=r'curl-curl-8_9_1\lib', glob='*.c',
                 flags=['-I', C + r'\curl_compat.h', '-D', '_WIN32=1', '-D', '__MINGW32__=1',
                        '-D', '__GNUC__=5', '-D', 'BUILDING_LIBCURL=1',
                        '-D', 'CURL_DISABLE_LDAP=1', '-D', 'CURL_DISABLE_LDAPS=1'], to=60),
    'sqlite': dict(root=r'sqlite-amalgamation-3530400\sqlite-amalgamation-3530400',
                   files=['sqlite3.c', 'shell.c'],
                   flags=['-I', '{root}', '-D', 'SQLITE_OMIT_LOAD_EXTENSION=1',
                          '-D', 'SQLITE_OMIT_DEPRECATED=1'], to=180),
    'tcc': dict(root='tinycc-release_0_9_27', glob='*.c', flags=['-I', '{root}'], to=60),
    'cpython': dict(root='cpython-3.12.5', multi=['Python', 'Objects', 'Parser', 'Programs'],
                    skip=['frozen.c', '_bootstrap_python.c', 'dynload_hpux.c', 'dynload_aix.c',
                          '_freeze_importlib.c'],
                    flags=['-I', C + r'\cpython_compat.h', '-D', 'Py_BUILD_CORE=1',
                           '-D', 'Py_BUILD_CORE_BUILTIN=1', '-D', 'MS_WINDOWS=1'], to=120),
    'ffmpeg': dict(root='FFmpeg-n6.1.1', multi=['libavformat', 'libavfilter', 'libavdevice'],
                   flags=['-I', C + r'\ffmpeg_compat.h', '-D', '_WIN32=1', '-D', 'HAVE_AV_CONFIG_H=1'], to=60),
    'openssl': dict(root='openssl-openssl-3.3.1', multi=['crypto'],
                    flags=['-I', C + r'\openssl_compat.h'], to=60),
    # git 2.45.2 —— **补盲区** (2026-10-13 虾米 · 依 启元 施工单 加分项 ⑧):
    #   本战役 12 套 里 一直 没有 git, 而 它 是 **最大 一套** (587 个 .c, 施工单 口径 563 文件)。
    #   旗标 逐字 照 施工单 / 仪器 D:\qiyuan_home\git_rebuild\compile_git_par.ps1 (唯一变量 = 引擎)。
    #   ⚠️ 权威 全量 复算 仍以 该 仪器 为准 (它 带 skip 表); 本 套 供 常规 闸 快速 对照。
    'git': dict(root=GIT, glob='*.c',
                multi=['.', 'builtin', 'compat', r'compat\regex', 'negotiator', 'refs', 'reftable',
                       'sha1dc', 'trace2', 'xdiff', 't'],
                flags=['-I', QJ + r'\compat_prelude.h', '-I', os.path.join(GIT, r'compat\regex'), '-I', GIT,
                       '-D', '__STDC_VERSION__=199901L', '-D', '__GNUC__=5', '-D', '_WIN32=1', '-D', 'REG_STARTEND=1'],
                to=240),
    'pg': dict(root='postgres-REL_16_4', multi=[r'src\common', r'src\port'],
               skip=['pg_crc32c_sse42_choose.c', 'pg_crc32c_sb8.c'],
               flags=['-D', 'FRONTEND=1'], to=60),
}


def filelist(name, cfg):
    root = os.path.join(C, cfg['root'])
    out = []
    if 'files' in cfg:
        out = [f for f in cfg['files']]
    elif 'multi' in cfg:
        for sub in cfg['multi']:
            d = os.path.join(root, sub)
            for f in sorted(os.listdir(d)):
                if f.endswith('.c'):
                    out.append(os.path.join(sub, f))
    else:
        out = sorted(f for f in os.listdir(root) if f.endswith('.c'))
    sk = set(cfg.get('skip', []))
    return [f for f in out if os.path.basename(f) not in sk]


NORM_NUM = re.compile(r'\d+')
NORM_ID = re.compile(r"'[^']*'")


def norm(msg):
    m = NORM_ID.sub("'X'", msg)
    m = NORM_NUM.sub('N', m)
    return m[:110]


def first_err(out):
    """取 首个 [ERR] 行 的 代码 + 消息头 (甲言 诊断 印 stdout/stderr 均 收)。"""
    for l in out.split('\n'):
        i = l.find('[ERR]')
        if i >= 0:
            rest = l[i + 5:].strip()
            code = rest.split(' ')[0] if rest else '?'
            return code, norm(rest[len(code):].strip())
    for l in out.split('\n'):
        if '找不到文件' in l:
            return 'E-INC', norm(l.strip())
    return '?-无诊断', ''


def run_one(cc, name, cfg, rel, extra):
    root = os.path.join(C, cfg['root'])
    flags = [x.replace('{root}', root) for x in cfg['flags']]
    oname = os.path.join(OS_, name, rel.replace('/', '_').replace('\\', '_')[:-2] + '.o')
    os.makedirs(os.path.dirname(oname), exist_ok=True)
    t0 = time.time()
    try:
        r = subprocess.run([cc, '-c', os.path.join(root, rel), '-o', oname] + flags + extra,
                           capture_output=True, timeout=cfg['to'])
    except subprocess.TimeoutExpired:
        return rel, 'TO', '超时', round(time.time() - t0, 1)
    o = ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')
    if r.returncode == 0:
        return rel, 'OK', '', round(time.time() - t0, 1)
    code, msg = first_err(o)
    return rel, code, msg, round(time.time() - t0, 1)


def main():
    args = sys.argv[1:]
    cc = None
    if '--cc' in args:
        i = args.index('--cc')
        cc = args[i + 1]
        del args[i:i + 2]
    par = 1
    if '--parallel' in args:
        i = args.index('--parallel')
        par = int(args[i + 1])
        del args[i:i + 2]
    # ── 并行度硬上限（2026-10-13 事故根因护栏）────────────────────────────
    # 并发过高 ⟹ 多个 qcc_* 进程在内存压力下，某个「全局表基址」指针失效/未分配
    # ⟹ 写 NULL 段错误(0xC0000005) ⟹ 崩溃风暴 × 每崩一份「满转储」(约 1.4GB)
    # ⟹ 秒级灌满 C 盘 ⟹ 桌面死机。实测：--parallel 12 崩、--parallel 4/6 不崩。
    # 铁律：脚本层钳住 —— 任何 --parallel N>CAP 一律降级并告警（不静默）。
    PARALLEL_CAP = 4
    if par > PARALLEL_CAP:
        print('[安全钳制] --parallel %d 超硬上限 %d ⟹ 自动降为 %d '
              '(高并发触发编译器段错误+满转储灌盘; 见 2026-10-13 事故)'
              % (par, PARALLEL_CAP, PARALLEL_CAP))
        par = PARALLEL_CAP
    if '--all' in args:
        args = [k for k in SETS]
    if not args:
        args = ['lua', 'zlib', 'libpng', 'libjpeg', 'redis', 'sqlite', 'tcc']
    cc = cc or os.path.join(QJ, 'qcc_x86_step32.exe')
    if not os.path.exists(cc):
        print('引擎 不存在:', cc)
        return 1
    print('引擎 =', cc, '| 目录', args, '| 并行', par)
    rep = {}
    for name in args:
        if name not in SETS:
            print('跳过 未知 套:', name)
            continue
        cfg = SETS[name]
        files = filelist(name, cfg)
        t0 = time.time()
        res = []
        if par > 1:
            with ThreadPoolExecutor(max_workers=par) as ex:
                futs = [ex.submit(run_one, cc, name, cfg, f, []) for f in files]
                for fu in futs:
                    res.append(fu.result())
        else:
            for f in files:
                res.append(run_one(cc, name, cfg, f, []))
        ok = sum(1 for r in res if r[1] == 'OK')
        to = sum(1 for r in res if r[1] == 'TO')
        cl = {}
        for rel, code, msg, dt in res:
            if code == 'OK':
                continue
            key = code + ' | ' + msg
            e = cl.setdefault(key, [0, []])
            e[0] += 1
            if len(e[1]) < 3:
                e[1].append(os.path.basename(rel))
        print('\n=== %-8s %d/%d 通过%s  %.0fs' % (name, ok, len(files),
                                                 ('  (超时 %d)' % to) if to else '', time.time() - t0))
        for key, (n, ex) in sorted(cl.items(), key=lambda kv: -kv[1][0])[:12]:
            print('   %3d× %s' % (n, key[:150]))
            print('        例: %s' % ' '.join(ex))
        rep[name] = dict(ok=ok, total=len(files), to=to,
                         clusters=[dict(n=v[0], key=k, ex=v[1]) for k, v in
                                   sorted(cl.items(), key=lambda kv: -kv[1][0])])
    io.open(os.path.join(S, 'r790_clusters.json'), 'w', encoding='utf-8', newline='\n').write(
        json.dumps(rep, ensure_ascii=False, indent=1))
    print('\nJSON →', os.path.join(S, 'r790_clusters.json'))
    return 0


sys.exit(main())
