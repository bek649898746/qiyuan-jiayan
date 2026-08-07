/* 甲言内核 — 编译时内存布局 (Gate 2)
 * 物理地址空间: DDR5 (CPU) + HBM (GPU) 独立地址域（不同总线）
 * 包含: qcc_x86_new.exe -bin tests/kernel/kernel_v5.c -o scratch_test/kernel_v5.bin
 */

/* ================================================================
 * DDR5 物理地址域 (256GB, CPU 内存控制器)
 * ================================================================ */
#define DDR5_BASE    0x00000000
#define DDR5_SIZE    (256LL * 1024 * 1024 * 1024)

/* 内核固定区 */
#define KERNEL_BASE  0x00100000   /* 1MB (Multiboot 标准加载地址) */
#define KERNEL_MAX   0x00100000   /* 内核最大 1MB (当前 ~33KB) */

/* ---- Agent 工作区起点 ---- */
#define AGENT_BASE   0x01000000   /* 16MB */

/* Agent 分区 (编译时定死, 无 malloc) */
#define AGENT_THINK_BASE    (AGENT_BASE)
#define AGENT_THINK_SIZE    (256 * 1024 * 1024)  /* 思考引擎 256MB */
#define AGENT_CODE_BASE     (AGENT_THINK_BASE + AGENT_THINK_SIZE)
#define AGENT_CODE_SIZE     (512 * 1024 * 1024)  /* 代码Agent 512MB */
#define AGENT_REVIEW_BASE   (AGENT_CODE_BASE + AGENT_CODE_SIZE)
#define AGENT_REVIEW_SIZE   (256 * 1024 * 1024)  /* 审查Agent 256MB */
#define AGENT_TEST_BASE     (AGENT_REVIEW_BASE + AGENT_REVIEW_SIZE)
#define AGENT_TEST_SIZE     (128 * 1024 * 1024)  /* 测试Agent 128MB */

/* Agent 间通道 */
#define AGENT_CHANNEL_BASE  (AGENT_TEST_BASE + AGENT_TEST_SIZE)
#define AGENT_CHANNEL_SIZE  (64 * 1024 * 1024)   /* 通道 64MB */

/* Agent 栈区 */
#define AGENT_STACK_BASE    (AGENT_CHANNEL_BASE + AGENT_CHANNEL_SIZE)
#define AGENT_STACK_SIZE    (64 * 1024 * 1024)   /* 栈 64MB */

/* 代码向量索引 */
#define VECIDX_BASE         0x40000000           /* 1GB */
#define VECIDX_SIZE         (8LL * 1024 * 1024 * 1024)  /* 8GB */

/* ================================================================
 * HBM 物理地址域 (80GB, GPU 显存控制器, 独立总线)
 * CPU 通过 PCIe BAR 窗口访问
 * ================================================================ */
#define HBM_BASE          0x00000000  /* GPU 地址空间起点 */
#define HBM_SIZE          (80LL * 1024 * 1024 * 1024)

/* KV Cache 池 */
#define KVCACHE_BASE      0x00000000
#define KVCACHE_THINK     (64 * 1024 * 1024)   /* 思考引擎 KV 64MB */
#define KVCACHE_CODE      (64 * 1024 * 1024)   /* 代码Agent KV 64MB */
#define KVCACHE_REVIEW    (32 * 1024 * 1024)   /* 审查Agent KV 32MB */
#define KVCACHE_TEST      (16 * 1024 * 1024)   /* 测试Agent KV 16MB */
#define KVCACHE_FREE      (64 * 1024 * 1024)   /* 空闲KV池   64MB */

/* 模型权重区 */
#define WEIGHTS_BASE       0x81000000           /* ~2GB 偏移 */
#define WEIGHTS_LLM_SIZE   (16LL * 1024 * 1024 * 1024)  /* LLM ~16GB */
#define WEIGHTS_EMBED_SIZE (1LL * 1024 * 1024 * 1024)   /* 嵌入表 ~1GB */
#define WEIGHTS_CODE_SIZE  (2LL * 1024 * 1024 * 1024)   /* 代码嵌入 ~2GB */
#define HBM_END            0xC0000000           /* ~3GB 终点 */

/* ================================================================
 * MMIO 物理地址域 (x86 固定)
 * ================================================================ */
#define MMIO_GPU_CTRL    0xF0000000  /* GPU 控制寄存器 */
#define MMIO_NVME_CTRL   0xF1000000  /* NVMe 控制器 */
#define MMIO_NIC_CTRL    0xF2000000  /* 网卡寄存器 */
#define MMIO_APIC        0xFEE00000  /* Local APIC */

/* ================================================================
 * Agent 类型定义 (第一版: C struct 宏展开)
 * ================================================================ */
#define AGENT_STATE_SIZEOF   512  /* Agent 状态结构体大小 (暂定) */

/* Agent 状态 */
#define AGENT_THINKING  0
#define AGENT_TOOL      1
#define AGENT_WAITING   2

/* Agent 能力位掩码 */
#define AGENT_CAN_READ   0x01
#define AGENT_CAN_WRITE  0x02
#define AGENT_CAN_EXEC   0x04
#define AGENT_CAN_NET    0x08
