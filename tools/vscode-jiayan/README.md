# 甲言 Jiayan — VSCode 扩展

**甲言语言支持**：语法高亮 + 诊断（悬停/补全/跳转待做）。

## ⚠️ 诚实分层（这一点很重要）

本扩展的**外壳**是 JS —— 因为 **VSCode 扩展本体只能用 JS/TS**，这是平台硬性要求，不是偷懒。
**但语言逻辑与数据全部由甲言自举产出**：

| 组件 | 语言 | 作用 |
|:--|:--|:--|
| `jylsp.exe` | **甲言写**（`jylsp.jy`）| LSP 语言服务：握手 + 诊断（`Content-Length` 帧走 Win32 直读）|
| `jy_diag.exe` | **甲言写**（`jy_diag.jy`）| 诊断 JSON → LSP 帧（含转义）|
| `jy_kw.exe` | **甲言写**（`jy_kw.jy`）| 从编译器源码提取 **535 词关键字表** ⟹ 本扩展的语法高亮就来自它 |
| `jy_check.exe` | **甲言写**（`jy_check.jy`）| 诊断渲染成中文报告 |
| `extension.js` / `lspclient.js` | JS（壳）| 只做：启动进程 · 收发字节 · 转 VSCode API |

**⟹ 能写的部分全部用甲言写了；JS 只负责 VSCode 要求它负责的那一小层。**

## 分工（诊断数据怎么来的）

```
编辑器壳（JS）      保存/打开 .jy 文件时：
                      cp.execFile(qcc, ['--dj', 文件])  ⟹ 写 <文件>.jyan.json
                                ↓
jylsp.exe（甲言）   收到 didSave/didOpen ⟹ 读约定文件 ⟹ 转协议 ⟹ 发 publishDiagnostics
                                ↓
编辑器              显示诊断（红/黄波浪线 + 问题面板）
```

**为什么壳跑子进程**：`jylsp` 自己跑要走 `_popen`（**CRT 未初始化 ⟹ 不可用**）或 `CreateProcess`+管道（重）；
而**跑子进程本来就是编辑器壳的强项** ⟹ 这样分工最稳、且职责清晰 ✓

## 安装（手工放入）

```
%USERPROFILE%\.vscode\extensions\qiyuan.jiayan-0.1.0\
├── package.json
├── extension.js
├── lspclient.js
├── language-configuration.json
└── syntaxes\jiayan.tmLanguage.json
```
放好后**重启 VSCode** 生效。

## 设置（`jiayan.*`）

| 键 | 说明 |
|:--|:--|
| `jiayan.jylspPath` | `jylsp.exe` 路径（留空则找扩展目录 / PATH）|
| `jiayan.qccPath` | 编译器路径 |
| `jiayan.rtPath` | `qcc_rt.c` 路径（写进 `QCC_RT_PATH`）|
| `jiayan.jyCheckPath` | `jy_check.exe` 路径 |

## 命令

- **甲言: 编译当前文件（qcc --dj）** — 产生诊断并显示
- **甲言: 显示诊断（jy_check 渲染成中文报告）** — 输出面板看完整中文报告
- **甲言: 重启语言服务（jylsp.exe）**

## 验证（**不依赖 VSCode**）

```powershell
cd <扩展目录>
node test_lsp.js        # 真进程 + 真协议：spawn jylsp.exe，发 initialize/didSave，收 publishDiagnostics
```
**实测 PASS**：`收到帧数=3 · initialize 响应=true · publishDiagnostics=true（诊断数 1）` ✓
`lspclient.js` 就是扩展里用的那份客户端代码 ⟹ **这里过 = 编辑器里是同一份代码** ✓

## 已知限制（如实）

- 补全 / 跳转 / 悬停 **未做**
- `didChange` 的**增量文本同步未做**（诊断在保存/切文件时刷新）
- `uri` 的 **URL 解码未做**（含中文/空格的路径暂不支持）
- 语法高亮的「函数名」规则是启发式（`标识符(` 视为函数名）

_甲言语言支持扩展 · 启元 · 2026-10-14_
