# -*- coding: utf-8 -*-
"""S4 判据: 外部调用 **第 5 个及以后** 实参 是否真被传到被调。

病灶 (2026-10-21 实测): 传参序列 是
    压实参([rsp+…]) → 压栈 r15 → `and rsp,-16` → 减 32 影子 → 调导入
而 5/6… 号实参 按 **对齐前** 的 [rsp+32+8(k-4)] 写死 ⟹ `and rsp,-16` 挪了 rsp ⟹
被调 在 [新 rsp+32] 读到 影子空间 的 垃圾 ⟹ 第 5 个及以后 实参 **静默丢失**。

判据（确定性, 不依赖文件系统）:
 ① MultiByteToWideChar(0,0,"AB",-1,宽,128)  —— 第 5 参 = 输出缓冲
    修前: 返回 3（只问长度）但 宽[] 全 0；修后: 宽 = {65,66,0}
 ② WideCharToMultiByte(0,0,宽,-1,回,128,0,0) —— 第 5/6/7/8 参 全在栈上
    修前: 回[] 不被写；修后: 回 = "AB"

用法: python scripts/check_call_args5.py [--cc <exe>]
退出: 0 PASS / 1 FAIL
"""
import argparse
import os
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REPO)

SRC = r'''
整 主(空) {
    字 源[32];
    源[0] = 'A'; 源[1] = 'B'; 源[2] = 0;
    短 宽[128];
    整 n = (整)MultiByteToWideChar(0, 0, 源, -1, 宽, 128);
    printf("MB=%d %d %d %d\n", n, (整)宽[0], (整)宽[1], (整)宽[2]);
    字 回[128];
    回[0] = 0;
    整 m = (整)WideCharToMultiByte(0, 0, 宽, -1, 回, 128, 0, 0);
    printf("WC=%d %d %d\n", m, (整)回[0], (整)回[1]);
    返 0;
}
'''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default=os.path.join('build', 'dev_fast.exe'))
    a = ap.parse_args()

    d = tempfile.mkdtemp(prefix='s4_')
    src = os.path.join(d, 't.jy')
    exe = os.path.join(d, 't.exe')
    open(src, 'w', encoding='utf-8-sig').write(SRC)

    r = subprocess.run([a.cc, src, '-o', exe], capture_output=True, text=True,
                       encoding='utf-8', errors='replace')
    if r.returncode != 0:
        print('[FAIL] 编译失败 rc=%d\n%s' % (r.returncode, (r.stdout + r.stderr)[-800:]))
        return 1
    r = subprocess.run([exe], capture_output=True, text=True, encoding='utf-8', errors='replace')
    out = (r.stdout or '') + (r.stderr or '')
    print(out.strip())

    checks = [
        ('MB 第 5 参(输出缓冲) 真到位', 'MB=3 65 66 0' in out),
        ('WC 第 5/6/7/8 参(缓冲/长度/双 NULL) 真到位', 'WC=3 65 66' in out),
    ]
    ok = True
    for name, good in checks:
        print('  [%s] %s' % ('PASS' if good else 'FAIL', name))
        ok = ok and good
    print('=== S4 判据: %s ===' % ('PASS' if ok else 'FAIL（>=5 实参 仍被丢弃）'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
