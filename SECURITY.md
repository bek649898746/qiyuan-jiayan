# 安全政策（Security Policy）

> 适用仓库：`qiyuan-jiayan`（甲言 Jiayan — 中文书写编程语言与自举编译器）
> 本政策自 2026-10-25 起生效。

## 一、报告漏洞

**请不要通过公开 Issue 报告安全问题**——公开即等于把利用方式发给了所有人。

请通过下列任一**私密**渠道：

| 渠道 | 用法 |
|:--|:--|
| **GitHub Security Advisory**（推荐） | 直接打开 <https://github.com/bek649898746/qiyuan-jiayan/security/advisories/new> 提交（私密、可协作、可发 CVE） |
| **Gitee 私信** | 私信 <https://gitee.com/zheng-yuhe-from-guangdong>（`@zheng-yuhe-from-guangdong`），标题注明 `[SECURITY]` |

### 我们会做什么

| 阶段 | 时限 |
|:--|:--|
| 确认收到 | **7 天内** |
| 初步评估（是否属于安全范围、影响面） | 14 天内 |
| 修复或缓解 | 视严重度：高危 **30 天内**，其余按排期 |
| 公开致谢 | 修复发布时在 `CHANGELOG.md` 致谢（如你愿意署名） |

### 协调披露

我们按**协调披露**办事：修复发布后 **90 天**或双方约定的时间点，才公开技术细节。
若你已公开，我们会立即转入应急流程而非指责研究者。

## 二、适用范围

甲言是**编译器与工具链**（编译器 / 汇编器 `asm_zh` / 链接器 `jyld` / 驱动 `jycc`）。
以下属于安全问题：

| 类别 | 说明 | 示例 |
|:--|:--|:--|
| **编译期越权** | 编译动作本身做了不该做的事 | 源码通过 `嵌入` / 宏 / `#包含` 读取源码树之外的文件 |
| **产物被静默篡改** | 源码与产物语义不一致且无告警 | 注释里的 `*/` 提前结束注释导致代码段被吞（见 `scripts/diag_comment_stray.c`） |
| **确定性破坏** | 相同输入产生不同产物 | 自举链多代哈希不等、产物哈希受 cwd / 环境影响 |
| **拒绝服务** | 特制源码让编译器崩溃或死循环 | 深递归宏、超大静态表撑爆 |
| **供应链** | 构建/校验流程可被源码内容绕过 | CI 或校验脚本的判定结果受被编译源码影响 |
| **制品完整性** | 分发的 `qcc_zh_seed.exe` 与源码不自洽 | 种子的 SHA256 前 8 位 ≠ `scripts/build_zh.ps1` 的 `$expected` |

## 三、明确不在范围内

- **被编译程序自身**的运行时漏洞（那是该程序的问题，不是编译器的问题）
- 攻击者**已经能修改源码**的场景（此时他不需要攻击编译器）
- 单纯的功能缺失或**编译错误** —— 请走普通 Issue 并附错误码（`A1001` / `A5001` / `A9001` …）
- **已知且已披露**的限制（见 [`KNOWN_ISSUES_v1.0.md`](KNOWN_ISSUES_v1.0.md) 与 `SUPPORT_MATRIX.md` 的「限制」区）

## 四、支持版本

| 版本 | 支持状态 | 说明 |
|:--|:--|:--|
| `main`（默认分支，持续更新） | ✅ **积极维护** | 安全问题优先在此修复 |
| `jiayan-v1.0-stable`（2026-08-24 封存标签） | ⚠️ **仅记录，不修复** | 1.0 历史快照，保留用于审计与复现 |
| 其它历史提交 / 分支 | ❌ 不支持 | 请勿基于旧快照做生产部署 |

> 我们的版本口径是**自举不动点**（`v1 == v2 == $expected`），不是语义化版本号。
> 当前参考值见 `README.md` 顶部状态块（权威源始终是 `scripts/build_zh.ps1` 的 `$expected`）。

## 五、研究者善意条款（Safe Harbor）

对**善意**的安全研究，我们承诺：

- 不因你**按本政策**做的测试而采取法律行动或向平台投诉；
- 只要你**不**破坏数据、不干扰他人、不公开未修复的细节，我们视你为协作者；
- 在你的同意下，在 `CHANGELOG.md` 与致谢名单中署名。

## 六、评分上的两处有意扣分（不是疏漏）

若你以 OpenSSF Scorecard 一类的自动化评分评估本项目，有两项分数是**我们主动接受**的：

- **`Binary-Artifacts` 扣分**：仓库里确有 7 个二进制，但它们全部是**承重的**，不是编译残留：
  1. **`qcc_zh_seed.exe`**（15.8 MB）—— **自举起点**。自举的定义就是「用它编自己」，没有起点就无法开始（Go 需要 go1.4、Rust 需要 stage0，同理）。
  2. **`v14_self/cpu64_v14_self.exe`**（96 KB）—— **血缘链条第一环**（2026-07-13/14 第一个能自举的编译器），与其 723 行源码一起放在仓库，让「主源从哪来」这件事可查、可跑、可验。
  3. **`scripts/ir_stage1_baseline/*.exe`**（5 个，各 393 KB）—— **IR 阶段1 对拍基线**，被 `scripts/check_ir_stage1.py` 使用。基线必须钉死在一个**已知良好**的版本上；若改成运行时现场生成，基线会随编译器一起变，对拍判据立刻失效。

  ⟹ 把这些移走能提高评分，但会**破坏构建入口或判据有效性**。这属于「功能换评分」，与 `Token-Permissions` 是同一类取舍。
- **`Token-Permissions` 扣分**：`status-sync` 工作流需要 `permissions: contents: write`。它每次推送到默认分支后重算 README 顶部的状态块，有变化就自动提交回去；这条链路是「**对外数字始终跟 HEAD 一致**」的唯一保障 —— 主线约 1.5 提交/小时，依赖人手更新必然脱节，此前正是因此出现过「同一指标多套并行值」。我们不采用「换用一个 GitHub App 令牌绕开检测」的做法：那只是把写权限藏起来，并没有更安全。

⟹ 请按「**功能换评分**」理解这两项，而不是「不会配最小权限」。

## 七、如何验证你下载的二进制

从 `v2.1.0` 起，Release 资产附带 **Sigstore 签名**（keyless：用 GitHub OIDC 换取短时证书，项目**不保存任何私钥**）。

下载 `qcc_zh_seed.exe`、`qcc_zh_seed.exe.sig`、`qcc_zh_seed.exe.pem` 三个文件后，安装 [cosign](https://docs.sigstore.dev/cosign/system_config/installation/) 并执行：

```bash
cosign verify-blob \
  --signature qcc_zh_seed.exe.sig \
  --certificate qcc_zh_seed.exe.pem \
  --certificate-identity-regexp 'https://github.com/bek649898746/qiyuan-jiayan/.github/workflows/release-sign\.yml@.*' \
  --certificate-oidc-issuer 'https://token.actions.githubusercontent.com' \
  qcc_zh_seed.exe
```

看到 `Verified OK` 说明两件事：

1. 这个字节串**确实由本仓库的签名工作流产出**（证书身份来自 GitHub OIDC，不可能伪造）；
2. 该记录已写入 **Rekor 公开透明日志**，事后无法抵赖。

> ⚠️ **只用 sha256 不够**：哈希只能证明「两份文件内容一致」，不能证明「它来自本项目」—— 任何人都能给出与你下载文件相同的哈希。签名证明的是**出处**。

## 八、Security 页上的告警分别是什么

本仓库把 **OpenSSF Scorecard** 的结果也上传到「Security → Code scanning」（便于逐条追踪供应链实践），
因此那一页的告警**不是代码缺陷**，而是供应链评分里的扣分项。逐条说明：

| 告警规则 | 数量 | 是什么 | 为什么这样处置 |
|:--|--:|:--|:--|
| `Binary-Artifacts` | 7 | 自举种子 `qcc_zh_seed.exe`、血缘第一环 `v14_self/cpu64_v14_self.exe`、IR 对拍基线 `scripts/ir_stage1_baseline/*.exe` ×5 | 全部是**承重件**（没有种子就无法自举；基线现场生成则对拍失效）。已标记 `won't fix`，理由见上一节 |
| `Token-Permissions` | 2 | `status-sync`（状态块自动提交）与 `release-sign`（Sigstore 签名）需要写权限 | **功能换评分**的主动取舍，见上一节 |
| `Code-Review` | **1** | 本仓库**目前没有第二个评审者** | **保留未关闭。** 这不是误报，也不是取舍 —— 它是真的。只有出现外部评审之后，这条才该消失 |

**⟹ CodeQL 自身的代码缺陷告警：0 条。** 上面 10 条中，9 条是已记录的设计取舍（已 dismiss 并附理由），
剩下的 1 条 `Code-Review` 我们**故意留着** —— 关闭它等于假装有人评审过。

## 九、相关文档

- 编码/确定性铁律：`.gitattributes` 末尾说明 + [`scripts/verify_all.ps1`](scripts/verify_all.ps1)（编码闸 + 自举收敛）
- 已知问题：[`KNOWN_ISSUES_v1.0.md`](KNOWN_ISSUES_v1.0.md)
- 支持矩阵：[`SUPPORT_MATRIX.md`](SUPPORT_MATRIX.md)
- 复现与自证：[`manual/快速上手.md`](manual/快速上手.md)

---
_本政策参考 OpenSSF `Security-Policy` 与 `CII Best Practices` 的公开要求编写（不构成对该两项评分的承诺）。_
