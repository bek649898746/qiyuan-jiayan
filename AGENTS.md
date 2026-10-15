# AGENTS.md —— 给 AI coding agent 的项目约定

> 你是一个正在修改本仓库的 AI agent。**先读这一页。**
> 下面这些是**硬约束**，不是风格偏好 —— 每一条背后都有事故教训。违反的改动会被评审拒绝。

---

## 一、这是什么项目

**甲言（jiayan）**：一门**中文原生**编程语言 + **自举编译器**工具链。

- 关键字可用中文（`整` `若` `返` … 共 **42 个**），**兼容 C**，中英可混写
- 编译器**能编译自己**（自举）：多代产物 SHA256 **逐字节相同**
- 直出 Windows x86-64 **PE**，零外部依赖
- 附带中文汇编器 `asm_zh`、COFF 链接器 `jyld`、编译驱动 `jycc`

---

## 二、🔴 硬约束（违反必被拒）

### 1. `.ps1` 必须 UTF-8 with BOM

PowerShell 5.1 对**无 BOM** 的 `.ps1` 按系统 ANSI(GBK) 解码 ⟹ 中文变乱码 ⟹ 引号错位 ⟹
**整个脚本解析失败**（实测报文：`Missing expression after unary operator '--'`）。

本项目已出两次同类事故：

| 时间 | 事故 |
|:--|:--|
| 2026-08-23 | UTF-8 被按 GBK 读+写 ⟹ 双重编码，中文逐字变空格 |
| 2026-10-15 | 编辑工具剥掉 BOM ⟹ 语法都过不了的脚本被推上远端 |

**⟹ 任何编辑工具改过 `.ps1` 之后，必须重新验证并规范化 BOM：**

```powershell
$p = "<该文件>"
$t = [System.IO.File]::ReadAllText($p, [System.Text.Encoding]::UTF8)
[System.IO.File]::WriteAllText($p, $t, (New-Object System.Text.UTF8Encoding($true)))
# 再跑 PSParser 复检
```

`scripts/verify_all.ps1` 的**编码闸**会拦下缺 BOM 的脚本。

### 2. 不要写死会变动的数字

不动点 / 行数 / 通过数会随源码**每次合法修改**而变化。必须写时，请注明「以脚本实测为准」。

> 实测教训：2026-10-15 发现 `scripts/build.ps1` 的 `$expected` 滞后 44 天，
> 导致**任何用户 clone 后照文档跑都看到红色 FAIL 并当场中断** —— 而自举其实完全成功。

### 3. 改编译器源码 = 双版同步

`srclib/qcc_x86.c`（C 版）与 `srclib_jiayan/qcc_work.jy`（甲言版）是**逻辑镜像**。
**只改一边 = PR 会被拒。**

### 4. 内部材料永不提交

`docs/` · `memory/` · `_audit_inbox/` 属内部材料（**当前均已 0 件入索引**）。
禁用 `git add -A` / `git add .`，**只用精确路径**。依据：`_启元禁令.md`。

---

## 三、改完必须验证（硬判据）

```powershell
powershell -ExecutionPolicy Bypass -File scripts/verify_all.ps1
```

**正确性判据 = 自举链「多代哈希全等」**（如 `v1==v2==v3`），
**不是**比对脚本里写死的 `$expected`。必须 `FAIL=0` 才算过。

---

## 四、目录职责

| 目录 | 内容 |
|:--|:--|
| `srclib/` | 编译器与工具链（C 版）：`qcc_x86.c` · `qcc_rt.c` · `asm_zh.c` · `jyld.c` · `jycc.c` |
| `srclib_jiayan/` | 甲言中文源码（`qcc_zh.jy` 为主源） |
| `seed/` | 种子（自举起点） |
| `tests/` | 测试（`qcc` / `behavior` / `kernel` / `unit` / `loong`） |
| `scripts/` | 工具脚本。⚠️ **目前也混放了约 1630 个测试用例**（`feat_*.c`），迁移计划见 `_audit_inbox` 的施工单 |
| `manual/` | **面向使用者的公开手册**（快速上手 / AI 上手 / 中文关键字表） |
| `docs/` `memory/` `_audit_inbox/` | **内部材料 —— 永不提交** |

---

## 五、AI 接口（本项目特有，值得优先利用）

甲言在**编译期**导出机器可读数据，AI 可直接查询而不必读全文：

| 接口 | 命令 | 用途 |
|:--|:--|:--|
| **代码知识图谱** | `qcc_x86.exe -graph 图谱.json <src>`<br>+ `python scripts/graph_query.py 图谱.json <8 种查询>` | 谁调谁 / 扇入扇出 / 孤立函数 / 结构体风险 |
| **结构化诊断** | `qcc_x86.exe --diagnostics=json <src>`（简写 `--dj`） | 错误码 + 位置 + **修复建议** + **出错源码行** |

详见 [`manual/AI快速上手.md`](manual/AI快速上手.md)。

> ⚠️ **`-graph` 后紧跟的是「输出文件名」，不是源文件**。写反会把源文件**覆盖**成 JSON（已实测踩到）。

---

## 六、提交信息格式

参考 [`CONTRIBUTING.md`](CONTRIBUTING.md)。可用
`<动词> <改动>.<新不动点> <SHA 前 16 位>`，
或 `fix(范围): …` / `docs: …` / `chore: …` **加详细正文**（本项目提交信息普遍很详尽，请保持）。
