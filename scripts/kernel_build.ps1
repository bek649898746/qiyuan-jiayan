# ============================================================
# 甲言内核 · 拼装 boot + kernel 并打 entry 补丁（纯 PowerShell，零 Python）
# 用法:
#   .\kernel_build.ps1 -Kernel kernel_v58
# 产出: scratch_test\_<Kernel>_nboot.bin  （可直接喂 qemu -kernel）
#
# 2026-10-28 启元（越岗执行）：替换 scripts/_qemu_kernel.sh 里的 python3 补丁段。
#   原脚本依赖 WSL + bash + python3；本脚本只用 MinGW as/objcopy + .NET，
#   在 Windows 侧即可完成。判据：同样的输入产出逐字节相同的 nboot.bin。
# ============================================================
param(
    [Parameter(Mandatory=$true)][string]$Kernel,
    [string]$RepoRoot = 'C:\Users\Administrator\Desktop\qiyuan-jiayan',
    [string]$Compiler = 'qcc_x86.exe',
    [switch]$KeepTemp
)

$ErrorActionPreference = 'Stop'
$AS       = 'C:\mingw-w64\bin\as.exe'
$OBJCOPY  = 'C:\mingw-w64\bin\objcopy.exe'
$LD       = 'C:\mingw-w64\bin\ld.exe'
$LOAD_ADDR = 0x100000      # boot 段加载地址（boot.S 里 -Ttext 0x100000）
$STUB_MARK = 0x200000      # boot.S 里 mov r12, 0x200000 的哨兵值

function Say($m){ Write-Host $m }

# ---------- 0. 前置检查 ----------
foreach($t in @($AS,$OBJCOPY)){
    if(-not (Test-Path $t)){ throw "缺工具: $t" }
}
$srcJy = Join-Path $RepoRoot "tests\kernel\$Kernel.c"
if(-not (Test-Path $srcJy)){ throw "找不到内核源码: $srcJy" }
$bootS = Join-Path $RepoRoot 'tests\kernel\boot.S'
if(-not (Test-Path $bootS)){ throw "找不到 boot.S: $bootS" }
$qcc = Join-Path $RepoRoot $Compiler
if(-not (Test-Path $qcc)){ throw "找不到编译器: $qcc" }

$scratch = Join-Path $RepoRoot 'scratch_test'
New-Item -ItemType Directory -Force -Path $scratch | Out-Null
$tmp = Join-Path $env:TEMP ("_kb_" + [guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Force -Path $tmp | Out-Null

try {
    # ---------- 1. 甲言编译内核 → bin（并用 [BIN] 行取 entry offset）----------
    $kBin = Join-Path $scratch "$Kernel.bin"
    Say "[1/4] 甲言编译 $Kernel.c -> $Kernel.bin"
    # ⚠️ 编译器把 [BIN] entry offset 写到 **stderr**。Stop + 2>&1 会把它当致命错误
    #    （同 scripts/verify_90s.ps1 头部记录的那个坑）⟹ 这里临时降到 Continue。
    $prevEAP = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $raw = (& $qcc -bin $srcJy -o $kBin 2>&1 | Out-String)
    $rc  = $LASTEXITCODE
    $ErrorActionPreference = $prevEAP
    if($rc -ne 0){ throw "甲言编译失败 rc=$rc`n$raw" }
    # ⚠️ 编译器可能输出**多行** [BIN] entry offset（实测 kernel_v59 出现 3 次：
    #    0x3B818 / 0x3B977 / 0x3B977，前一行是中间产物）。真实产物对应**最后一行**，
    #    取第一个会把补丁打飞 ⟹ 必须取最后一次匹配。2026-10-28 启元实测踩到。
    $ms = [regex]::Matches($raw, 'entry offset:\s*0x([0-9A-Fa-f]+)')
    if($ms.Count -eq 0){ throw "取不到 entry offset，编译器输出:`n$raw" }
    $entryOff = [Convert]::ToInt32($ms[$ms.Count-1].Groups[1].Value, 16)
    if($ms.Count -gt 1){
        Say ("      注意：编译器输出了 {0} 次 entry offset（{1}），取最后一次" -f $ms.Count,(($ms | ForEach-Object { '0x'+$_.Groups[1].Value }) -join ' / '))
    }
    Say ("      entry offset = 0x{0:X}   bin = {1:N0} B" -f $entryOff,(Get-Item $kBin).Length)

    # ---------- 2. boot.S -> boot.bin ----------
    # ⚠️ 2026-10-28 启元实测的**崩溃根因**：不能 `as` 后直接 `objcopy -O binary -j .text`。
    #    as 产出的 .o 里 .text 带 15+ 条 dir32 重定位项，objcopy 只抽字节、**不做重定位**，
    #    于是 `lgdt (gdtdesc)` 填成 0x00000108（应为 0x00100108）、`movl $pd,%edi` 填成 0x3000
    #    （应为 0x00103000）—— 只差 22 字节，但 GDT 加载失败 ⟹ 内核立刻 triple fault，
    #    QEMU 打一行 "Booting from ROM.." 就退出，**没有任何串口输出**（我卡了两轮才定位）。
    #
    #    必须先用 ld 做重定位。MinGW 的 ld 不支持 `-m elf_i386`（只支持 i386pe/i386pep），
    #    但 `-m i386pe --image-base 0x100000 -Ttext 0x100000` 能正确解析成 0x100000 基址，
    #    产出的 PE 其 .text **无 RELOC 标记**（已重定位）；再 objcopy -O binary 全抽、
    #    取前 0x4000 字节（= as 对 .text 的 2**12 对齐大小）即得正确 boot.bin。
    Say "[2/4] boot.S -> boot.bin（ld 重定位 → objcopy 抽 flat）"
    $bootO   = Join-Path $tmp 'boot.o'
    $bootExe = Join-Path $tmp 'boot.exe'
    $bootAll = Join-Path $tmp 'boot_all.bin'
    $bootBin = Join-Path $tmp 'boot.bin'
    & $AS --32 $bootS -o $bootO
    if($LASTEXITCODE -ne 0){ throw "as 失败 rc=$LASTEXITCODE" }
    & $LD -m i386pe --image-base 0x100000 -Ttext 0x100000 --defsym kernel_main=0 $bootO -o $bootExe
    if($LASTEXITCODE -ne 0){ throw "ld 失败 rc=$LASTEXITCODE（MinGW ld 需要 -m i386pe，不是 elf_i386）" }
    & $OBJCOPY -O binary $bootExe $bootAll
    if($LASTEXITCODE -ne 0){ throw "objcopy 失败 rc=$LASTEXITCODE" }

    $allB = [System.IO.File]::ReadAllBytes($bootAll)
    if($allB.Length -lt 0x4000){ throw ("boot_all.bin 只有 {0:N0} B，不足 16KB" -f $allB.Length) }
    $bootB = New-Object byte[] 0x4000
    [Array]::Copy($allB,0,$bootB,0,0x4000)
    # Multiboot 头自检（1BADB002 / 00010003 / E4514FFB 三字和模 2^32 == 0）
    $magic=[BitConverter]::ToUInt32($bootB,0)
    if($magic -ne 0x1BADB002){ throw ("boot.bin 首字不是 Multiboot magic（实得 0x{0:X8}）" -f $magic) }
    # ⚠️ PowerShell 里字面量 0xFFFFFFFF 是 int32 = -1，`-band 0xFFFFFFFF` 等于没做掩码
    #    （2026-10-28 启元连踩两次）。直接判「三字和 == 2^32」最稳。
    $sum = [uint64]([BitConverter]::ToUInt32($bootB,0)) + [uint64]([BitConverter]::ToUInt32($bootB,4)) + [uint64]([BitConverter]::ToUInt32($bootB,8))
    if($sum -ne [uint64]0x100000000){ throw ("boot.bin Multiboot 校验和不合格：三字和 = 0x{0:X}（应为 0x100000000）" -f $sum) }
    [System.IO.File]::WriteAllBytes($bootBin,$bootB)
    $bootLen = 0x4000
    Say ("      boot.bin = {0:N0} B  (Multiboot 头校验通过)" -f $bootLen)

    # ---------- 3. 拼接 ----------
    Say "[3/4] 拼接 boot + kernel"
    $bootB = [System.IO.File]::ReadAllBytes($bootBin)
    $kB    = [System.IO.File]::ReadAllBytes($kBin)
    $c = New-Object byte[] ($bootB.Length + $kB.Length)
    [Array]::Copy($bootB,0,$c,0,$bootB.Length)
    [Array]::Copy($kB,0,$c,$bootB.Length,$kB.Length)
    Say ("      合计 {0:N0} B" -f $c.Length)

    # ---------- 4. 打 entry 补丁 ----------
    # 找 `48 BC <imm64=0x200000>`，其后若是 `E9 rel32`，把 rel 改成真实入口
    Say "[4/4] 打 entry 补丁"
    $patched = $false
    for($i = 0; $i -lt $c.Length - 14; $i++){
        if($c[$i] -ne 0x48 -or $c[$i+1] -ne 0xBC){ continue }
        $imm = [BitConverter]::ToUInt64($c, $i+2)
        if($imm -ne [uint64]$STUB_MARK){ continue }
        $jmpOff = $i + 10
        if($c[$jmpOff] -ne 0xE9){ continue }

        $entryAbs = $LOAD_ADDR + $bootLen + $entryOff
        $nextIp   = $LOAD_ADDR + $jmpOff + 5
        $rel      = $entryAbs - $nextIp
        [Array]::Copy([BitConverter]::GetBytes([int32]$rel), 0, $c, $jmpOff + 1, 4)

        $out = Join-Path $scratch ("_{0}_nboot.bin" -f $Kernel)
        [System.IO.File]::WriteAllBytes($out, $c)
        Say ("      命中哨兵 @0x{0:X}，entry=0x{1:X}，rel={2}" -f $jmpOff,$entryAbs,$rel)
        Say ("      产出 {0}  ({1:N0} B)" -f $out,$c.Length)
        $patched = $true
        break
    }
    if(-not $patched){ throw "未命中哨兵 48 BC / 0x200000 / E9 —— boot.S 或编译器产物变了？" }

    Say ""
    Say "完成。跑 QEMU："
    Say ("  qemu-system-x86_64 -M q35 -kernel `"{0}`" -device nvme,drive=nvme0,serial=JIAYAN -nographic -no-reboot -m 128M" -f $out)
}
finally {
    if(-not $KeepTemp){ Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue }
    else { Say "临时目录保留: $tmp" }
}
