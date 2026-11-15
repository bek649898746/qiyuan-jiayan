#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""check_artifact_size —— ORD-20261107-138 / ORD-20261102-89 判据：运行库**链接期裁入**。

定档（2026-11-07 大哥四句 + 本刀）：
  最小程序 `整 主(空) { 返 0; }` 的产物 **≤ 现值的 50%**（385,536 B → ≤ 192,768 B）。

实现（2026-11-07 第二版 · 根治，非补丁）：
  编译器本来就把运行库源码整段前插、**逐函数**发射（229 个函数 / 379,595 字节，用户代码不到 1KB）。
  本刀把「哪些函数要发射」的决定推迟到**全部代码生成之后**：布局相一边发射一边记调用边
  （`补标号` 对函数标号的引用 + 函数取址），然后从入口 / 全局初始化做标记-清扫，终发相跳过
  入口不可达的**非用户源**函数。不做第二遍编译、不改运行库源码、不动自举链结构。

六条腿（缺一不算）：
  ① 体积：最小程序 ≤ 192,768 B，并记录与 gcc -O0 / 旧全量产物的倍数（只报告）；
  ② 真跑：printf / strcpy / strdup / strlen / 整数除法 的代表程序编译成功、输出逐字正确；
  ③ 归因：`QCC_NO_DCE=1` 必须变回全量（≥ 300,000 B 且 ≥ 2× 裁剪产物）——证明缩小来自本通道，
     不是「少发了别的什么」；
  ④ 边界：`-c`（目标文件）与 `-S`（汇编文本）在 DCE 开/关下**逐字节相同**（只裁单文件可执行）；
  ⑤ 应裁不漏：三个「只靠编译器生成名才可达」的糖族锚（vec / map / 模板元编程）必须编译 + 真跑；
  ⑥ 必失败桩：`QCC_DCE_TEST_DROP=<名>` 强行把被用到的函数标成不可达 ⟹ 编译器若仍要发射它，
     产物内必须留 **ud2 陷阱**（运行即非零退出 0xC000001D）并在编译期打印 `[DCE-STUB] <名>` ——
     绝不静默调到 0 地址。（生产路径更进一步：发现漏边时**响亮回退全量重发**，根本不发坏产物。）

退出码：0 = 六条全过；1 = 任一不达标。
"""
import io
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qcc_tmp import tmp_dir  # noqa: E402

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CC = os.path.join(ROOT, 'qcc_zh_v1.exe')
if '--cc' in sys.argv:
    CC = sys.argv[sys.argv.index('--cc') + 1]
RT = os.path.join(ROOT, 'srclib', 'qcc_rt.c')

BASE_SIZE = 385536          # 2026-11-07 前的实测值（裁剪前）
MAX_SIZE = BASE_SIZE // 2   # 192,768
SRC_MIN = '整 主(空) { 返 0; }\n'

PROBE = r'''#include <stdio.h>
#include <string.h>

static int half(int x) { return x / 2; }

int main(void) {
    int a = 100, b = half(a), c = a % 7;
    char buf[32];
    strcpy(buf, "abc");
    /* strdup 只由 qcc_rt.c 提供（不是编译器内建）—— ⑥ 的故障注入用它 */
    char *dup = strdup(buf);
    printf("%d %d %d %s %d\n", a, b, c, dup, (int)strlen(dup));
    free(dup);
    return 0;
}
'''
PROBE_WANT = '100 50 2 abc 3'

# ⑤ 糖族锚：这些符号是**编译器生成**的（__qcc_vec_* / __qcc_map_*），用户源码里不出现
#    ⟹ 是「按源码文本猜用名」方案必然漏的那一类，正是本判据要守的边界。
SUGAR = [
    ('feat_zh_vec_grow_ok.c', 'VEC OK'),
    ('feat_zh_map7.c', 'PASS'),
    ('feat_zh_template_meta68.c', '#68 模板元编程 CTFE 递归 PASS'),
]


def env_with(**kw):
    e = dict(os.environ)
    e['QCC_RT_PATH'] = RT
    for k, v in kw.items():
        if v is None:
            e.pop(k, None)
        else:
            e[k] = v
    return e


def run(argv, env=None, cwd=None):
    return subprocess.run(argv, cwd=cwd or ROOT, capture_output=True, env=env or env_with())


def main():
    if not os.path.exists(CC):
        print('[FAIL] 缺编译器 %s' % CC)
        return 1
    bad = []
    work = tmp_dir('artsize')
    try:
        # ① 最小程序体积
        src = os.path.join(work, 'min.jy')
        exe = os.path.join(work, 'min.exe')
        io.open(src, 'w', encoding='utf-8', newline='\n').write(SRC_MIN)
        r = run([CC, src, '-o', exe])
        if r.returncode != 0 or not os.path.exists(exe):
            print('[FAIL] 最小程序编译失败 rc=%d\n%s' % (r.returncode,
                                                     (r.stderr or r.stdout).decode('utf-8', 'replace')[:400]))
            return 1
        jy = os.path.getsize(exe)
        pct = 100.0 * jy / BASE_SIZE
        print('[量] 最小程序 %d B = 裁剪前 %d B 的 %.1f%%（上限 %d B = 50%%）'
              % (jy, BASE_SIZE, pct, MAX_SIZE))
        if jy > MAX_SIZE:
            bad.append('体积 %d B > %d B（未到 50%%）' % (jy, MAX_SIZE))

        gcc = None
        for cand in ('gcc', 'x86_64-w64-mingw32-gcc'):
            if subprocess.run([cand, '--version'], capture_output=True).returncode == 0:
                gcc = cand
                break
        if gcc:
            csrc = os.path.join(work, 'min_gcc.c')
            cexe = os.path.join(work, 'min_gcc.exe')
            io.open(csrc, 'w', encoding='utf-8', newline='\n').write('int main(void){return 0;}\n')
            q = subprocess.run([gcc, '-O0', csrc, '-o', cexe], capture_output=True)
            if q.returncode == 0 and os.path.exists(cexe):
                print('[参] gcc -O0 同程序 %d B ⟹ 甲言 %.1f×' % (os.path.getsize(cexe),
                                                             jy / os.path.getsize(cexe)))

        # ② 代表程序真跑
        psrc = os.path.join(work, 'probe.c')
        pexe = os.path.join(work, 'probe.exe')
        io.open(psrc, 'w', encoding='utf-8', newline='\n').write(PROBE)
        r = run([CC, psrc, '-o', pexe])
        if r.returncode != 0 or not os.path.exists(pexe):
            bad.append('代表程序编译失败 rc=%d' % r.returncode)
        else:
            q = subprocess.run([pexe], capture_output=True)
            got = (q.stdout + q.stderr).decode('utf-8', 'replace').strip()
            print('[跑] 代表程序 rc=%d 输出=%r（期望 %r）' % (q.returncode, got, PROBE_WANT))
            if q.returncode != 0 or got != PROBE_WANT:
                bad.append('代表程序输出不符')

        # ③ 归因：关掉本通道必须回到全量
        noexe = os.path.join(work, 'min_nodce.exe')
        r = run([CC, src, '-o', noexe], env=env_with(QCC_NO_DCE='1'))
        if r.returncode != 0 or not os.path.exists(noexe):
            bad.append('QCC_NO_DCE=1 最小程序编译失败 rc=%d' % r.returncode)
        else:
            full = os.path.getsize(noexe)
            print('[归因] QCC_NO_DCE=1 ⟹ %d B（%.1f× 裁剪产物）' % (full, full / float(jy)))
            if full < 300000 or full < 2 * jy:
                bad.append('关掉 DCE 后仅 %d B —— 缩小不是本通道造成的' % full)

        # ④ 边界：-c / -S 在开与关下必须逐字节相同
        for tag, extra in (('-c', ['-c']), ('-S', ['-S'])):
            outs = []
            for k, envk in enumerate((None, '1')):
                o = os.path.join(work, 'edge%s_%d.out' % (tag, k))
                r = run([CC] + extra + [psrc, '-o', o], env=env_with(QCC_NO_DCE=envk))
                if r.returncode != 0:
                    bad.append('%s 边界编译失败 rc=%d' % (tag, r.returncode))
                    outs = []
                    break
                outs.append(io.open(o, 'rb').read())
            if outs and outs[0] != outs[1]:
                bad.append('%s 产物在 DCE 开/关下不同（本通道只该管单文件可执行）' % tag)
            elif outs:
                print('[边界] %s 开/关逐字节相同（%d B）' % (tag, len(outs[0])))

        # ⑤ 应裁不漏：糖族锚（可达性由编译器生成的调用点提供）
        for name, want in SUGAR:
            a = os.path.join(ROOT, 'scripts', name)
            o = os.path.join(work, name + '.exe')
            r = run([CC, a, '-o', o])
            if r.returncode != 0 or not os.path.exists(o):
                bad.append('糖族锚 %s 编译失败 rc=%d' % (name, r.returncode))
                continue
            q = subprocess.run([o], capture_output=True)
            got = (q.stdout + q.stderr).decode('utf-8', 'replace').strip()
            print('[糖族] %s rc=%d out=%r' % (name, q.returncode, got[:60]))
            if q.returncode != 0 or want not in got:
                bad.append('糖族锚 %s 运行不符（期望含 %r）' % (name, want))

        # ⑥ 必失败桩：故障注入强行裁掉「被用到的」strdup ⟹ 产物内必须留桩
        #    编译期响亮打印 `[DCE-STUB] ... 'strdup'`；运行产物必须**非零退出**（桩 = exit 97），
        #    绝不允许静默调到 0 地址/静默返回 0。
        fsrc = os.path.join(work, 'probe_drop.exe')
        r = run([CC, psrc, '-o', fsrc], env=env_with(QCC_DCE_TEST_DROP='strdup'))
        out = (r.stdout + r.stderr).decode('utf-8', 'replace')
        loud = '[DCE-STUB]' in out and "'strdup'" in out
        rrun = None
        if os.path.exists(fsrc):
            q = subprocess.run([fsrc], capture_output=True)
            rrun = q.returncode
        # 0xC000001D 在 Python 里可能给有符号 (-1073741795) 或无符号 (3221225501) 两种口径
        print('[必失败桩] 编译期标记=%s 产物运行 rc=%s（期望 0xC000001D = ud2 陷阱）' % (loud, rrun))
        if r.returncode != 0 or not loud or rrun not in (-1073741795, 3221225501):
            bad.append('被裁函数可达时未留必失败桩（编译rc=%d 标记=%s 运行rc=%s）'
                       % (r.returncode, loud, rrun))

        if bad:
            print('[RED] ORD-20261107-138：%s' % '；'.join(bad))
            return 1
        print('=== ORD-20261107-138 判据: PASS（链接期裁入 · 六腿）===')
        return 0
    finally:
        import shutil
        shutil.rmtree(work, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
