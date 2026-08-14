# -*- coding: utf-8 -*-
"""ci.yml 加 qemu job."""
p = '.github/workflows/ci.yml'
s = open(p, encoding='utf-8').read()
if 'qemu' not in s.lower():
    qemu_job = '''
  qemu:
    runs-on: windows-latest
    needs: tests
    steps:
      - uses: actions/checkout@v5

      - name: 编译宿主编译器
        run: gcc -O2 -Wall -Werror srclib/qcc_x86.c -o qcc_x86.exe

      - name: WSL 安装 QEMU (渐进式, 失败不阻塞)
        continue-on-error: true
        run: wsl -e bash -c "sudo apt-get update && sudo apt-get install -y qemu-system-x86 binutils"

      - name: QEMU 内核验证 (NVMe 环回 LOOPBACK-PASS)
        continue-on-error: true
        run: wsl -e bash scripts/_qemu_kernel.sh kernel_v36
'''
    s = s.rstrip() + '\n' + qemu_job
    open(p, 'w', encoding='utf-8').write(s)
    print('qemu job added')
else:
    print('already has qemu')
