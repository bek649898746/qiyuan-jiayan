# 种子二进制（入库说明）

\qcc_zh_seed.exe\ 是 \.gitignore\ 里 \*.exe\ 的**唯一例外**（2026-10-23 强制入库）：
审计常在干净检出上跑判据，而种子不入库时只能拿到旧二进制 → 判据结果与 HEAD 源码不对应
（表现为 union 维度 A9001/HANG、自举哈希对不上）。

- 当前种子 = 不动点 **4BDB13D2**
- SHA256 BDB13D2CA4AD39A59DFC1DC031C1F7A09A88FAF8A284E0DD916C1A68A1EC52B- 更新纪律：\uild_zh.ps1\ 收敛后复制 \qcc_zh_v2.exe → qcc_zh_seed.exe\，并同步本文件与 \\。
