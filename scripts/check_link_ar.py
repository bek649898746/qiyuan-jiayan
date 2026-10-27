# -*- coding: utf-8 -*-
"""check_link_ar —— **`ar` 多成员库 + `--link` 按需拉取**（台账 D56 · 2026-10-25 启元递刀）。

审计实测（`D:\\_objmode_test`）：
  · `-c` + `jyld` + **2 成员** lib ⟹ 产物 **5,120 B**（单文件模式 287,232 B，−98.2%）
  · **全量运行库当 1 个成员** ⟹ **347,648 B**（比单文件还大 21% ⟹ “统一载体”= 收益全吐回去）
  · MS `lib.exe` 打的是 MS 库格式，`jyld` 读不了 ⟹ 必须自己做 **GNU ar**

判据（四条，全过才 rc=0）：
  ① **ar 写器可调用**：`jyar.exe` 或 `jyld.exe --ar`（**真打一次**并校验 `!<arch>\\n` 魔数 +
     符号索引成员；不认 `--help` 里出现的字样）
  ② **2 成员库能被 `jyld` 链接并运行**：库提供 `f0()/f1()`，`main` 调两者 ⟹ 链接 rc=0 且
     **运行退码 = 42**（按需拉取真生效，不是把整个库塞进去）
  ③ **空 `主` 形态产物 ≤ 8 KB**：`-c` + `jyld` ⟹ 产物 ≤ 8192 B 且 `/主`（或 `main`）可运行 rc=0
  ④ **红线**：`--link` 产物 **不得大于** 同程序单文件模式产物（防“统一载体”把收益吃掉）；
     `--link` = `scripts/qclink.py`（qcc -c + jyld 两段式驱动，见该文件头注释：qcc 无进程派生原语）

用法: python scripts/check_link_ar.py [--cc qcc_zh_v1.exe]
退出码: 0 = 四条全过；1 = 未实现/未达标（缺陷仍在）；2 = 环境未就绪
"""
import argparse
import io
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
QCLINK = os.path.join(ROOT, 'scripts', 'qclink.py')


def find_tool(names):
    for n in names:
        for d in (ROOT, os.path.join(ROOT, 'tools'), os.path.join(ROOT, 'build'), os.path.join(ROOT, 'srclib')):
            p = os.path.join(d, n)
            if os.path.exists(p):
                return p
    return ''


def run(cmd, timeout=300):
    r = subprocess.run(cmd, capture_output=True, timeout=timeout)
    return r.returncode, ((r.stdout or b'') + (r.stderr or b'')).decode('utf-8', 'replace')


def write(p, text):
    io.open(p, 'w', encoding='utf-8', newline='\n').write(text)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join(ROOT, 'qcc_zh_v1.exe'))
    a = ap.parse_args()
    cc = a.cc if os.path.isabs(a.cc) else os.path.join(ROOT, a.cc)
    jyld = find_tool(('jyld.exe', 'jyld'))
    if not os.path.exists(cc) or not jyld:
        print('[FAIL] 环境未就绪: cc=%r jyld=%r（本判据未执行，不判绿）' % (cc, jyld))
        return 2
    w = tempfile.mkdtemp(prefix='qcc_linkar_')

    # ---------- ① ar 写器：真打一次 ----------
    write(os.path.join(w, 'f0.c'), 'int f0(void){ return 7; }\n')
    write(os.path.join(w, 'f1.c'), 'int f1(void){ return 35; }\n')
    objs = []
    ok = True
    for i in range(2):
        o = os.path.join(w, 'f%d.obj' % i)
        rc, msg = run([cc, '-c', os.path.join(w, 'f%d.c' % i), '-o', o])
        if rc != 0:
            ok = False
            print('   编译 f%d.c 失败 rc=%d: %s' % (i, rc, msg.strip()[:120]))
        objs.append(o)
    lib = os.path.join(w, 'libtwo.a')
    ar_mode = ''
    rc, msg = run([jyld, '--ar', lib] + objs)
    if rc == 0 and os.path.exists(lib):
        head = io.open(lib, 'rb').read(68)
        if head[:8] == b'!<arch>\n' and head[8:9] == b'/':
            ar_mode = 'jyld --ar'
    ok1 = ok and bool(ar_mode)
    print('[① ar 写器 ] %s ⟹ %s' % (ar_mode or '未实现', 'OK' if ok1 else 'RED'))
    if not ok1:
        print('=== ar 多成员库 + --link: FAIL（① ar 写器不可用 ⟹ 后续三条无法执行）===')
        return 1

    # ---------- ② 2 成员库 → jyld 链接 → 运行 = 42 ----------
    write(os.path.join(w, 'm.c'),
          'int f0(void);\nint f1(void);\nint main(void){ return f0() + f1(); }\n')
    mo = os.path.join(w, 'm.obj')
    rc, msg = run([cc, '-c', os.path.join(w, 'm.c'), '-o', mo])
    exe2 = os.path.join(w, 'two.exe')
    rc2, msg2 = run([jyld, mo, lib, '-o', exe2])
    r2 = subprocess.run([exe2], capture_output=True) if os.path.exists(exe2) else None
    ok2 = (rc == 0 and rc2 == 0 and r2 is not None and r2.returncode == 42)
    print('[② 2成员库 ] 链接 rc=%d 运行 rc=%s（期望 42）⟹ %s'
          % (rc2, r2.returncode if r2 else 'N/A', 'OK' if ok2 else 'RED'))
    if not ok2:
        print('    ' + (msg2.strip().split('\n')[-1] if msg2.strip() else '')[:140])

    # ---------- ③ 空 main ≤ 8 KB ----------
    write(os.path.join(w, 'e.c'), 'int main(void){ return 0; }\n')
    eo = os.path.join(w, 'e.obj')
    ee = os.path.join(w, 'e.exe')
    rc, _ = run([cc, '-c', os.path.join(w, 'e.c'), '-o', eo])
    rc3, _ = run([jyld, eo, '-o', ee])
    size = os.path.getsize(ee) if os.path.exists(ee) else -1
    r3 = subprocess.run([ee], capture_output=True) if size > 0 else None
    ok3 = (rc == 0 and rc3 == 0 and 0 < size <= 8192 and r3 is not None and r3.returncode == 0)
    print('[③ 空main   ] 产物 %d B（阈值 8192）运行 rc=%s ⟹ %s'
          % (size, r3.returncode if r3 else 'N/A', 'OK' if ok3 else 'RED'))

    # ---------- ④ --link ≤ 单文件 ----------
    src = os.path.join(w, 'p.c')
    write(src, '#包含 <stdio.h>\nint main(void){ printf("LINK %d\\n", 42); return 0; }\n')
    link_exe = os.path.join(w, 'p_link.exe')
    full_exe = os.path.join(w, 'p_full.exe')
    rc4, msg4 = run(['python', '-X', 'utf8', QCLINK, src, '-o', link_exe, '--cc', cc, '--jyld', jyld],
                    timeout=600)
    rc5, _ = run([cc, src, '-o', full_exe], timeout=600)
    s_link = os.path.getsize(link_exe) if os.path.exists(link_exe) else -1
    s_full = os.path.getsize(full_exe) if os.path.exists(full_exe) else -1
    out_link = subprocess.run([link_exe], capture_output=True) if s_link > 0 else None
    out_full = subprocess.run([full_exe], capture_output=True) if s_full > 0 else None
    same = (out_link is not None and out_full is not None
            and out_link.stdout == out_full.stdout and out_link.returncode == out_full.returncode == 0)
    ok4 = (rc4 == 0 and rc5 == 0 and 0 < s_link <= s_full and same)
    print('[④ --link   ] 链接模式 %d B vs 单文件 %d B（须 ≤）；输出一致=%s ⟹ %s'
          % (s_link, s_full, same, 'OK' if ok4 else 'RED'))
    if not ok4 and msg4.strip():
        print('    ' + msg4.strip().split('\n')[-1][:140])

    allok = ok1 and ok2 and ok3 and ok4
    print('=== ar 多成员库 + --link: %s ===' % ('PASS' if allok else 'FAIL'))
    return 0 if allok else 1


if __name__ == '__main__':
    sys.exit(main())
