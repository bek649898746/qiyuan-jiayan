/* 甲言内核 — Agent 类型定义 (Gate 3)
 * 编译时定死: 无动态分配, 无运行时创建
 * 第一版: C struct 宏展开 (语法糖后续)
 */

#ifndef KERNEL_AGENT_H
#define KERNEL_AGENT_H

#include "kernel/kernel_mem.h"

/* ================================================================
 * Agent 状态枚举
 * ================================================================ */
#define AGENT_THINKING  0   /* 思考态: 推理中 */
#define AGENT_TOOL      1   /* 工具态: 调用工具中 */
#define AGENT_WAITING   2   /* 等待态: 等待依赖 */

/* ================================================================
 * Agent 能力位掩码
 * ================================================================ */
#define AGENT_CAN_READ   0x01  /* 读 Tensor池 */
#define AGENT_CAN_WRITE  0x02  /* 写 Tensor池 */
#define AGENT_CAN_EXEC   0x04  /* 执行代码 */
#define AGENT_CAN_NET    0x08  /* 网络访问 */

/* ================================================================
 * Token 预算 (不是时间片!)
 * Agent 调度不按毫秒——按 token 数
 * ================================================================ */
#define TOKEN_BUDGET_THINK   128    /* 思考引擎: 128 tokens/轮 */
#define TOKEN_BUDGET_CODE    4096   /* 代码Agent: 4096 tokens/轮 */
#define TOKEN_BUDGET_REVIEW  1024   /* 审查Agent: 1024 tokens/轮 */
#define TOKEN_BUDGET_TEST    0      /* 测试Agent: 不限(工具态) */

/* ================================================================
 * KV Cache 槽位 (HBM 内固定分区)
 * ================================================================ */
#define KV_SLOT_THINK   0   /* 思考引擎 KV 槽 */
#define KV_SLOT_CODE    1   /* 代码Agent KV 槽 */
#define KV_SLOT_REVIEW  2   /* 审查Agent KV 槽 */
#define KV_SLOT_TEST    3   /* 测试Agent KV 槽 */

/* ================================================================
 * Agent 结构体 (编译时大小固定)
 * 每个 Agent 编译时实例化，不创建/销毁/fork
 * ================================================================ */
typedef struct {
    /* --- 持久状态 --- */
    字 *名称;               /* Agent 名称 (字符串常量) */
    整  kv_slot;            /* HBM KV Cache 槽位号 */
    整  token_budget;       /* 每轮 token 预算 (0=无限) */
    整  state;              /* 当前状态: AGENT_THINKING/TOOL/WAITING */
    整  gpu_id;             /* 绑定 GPU 编号 (0=GPU0) */

    /* --- 内存区域 (编译时固定地址) --- */
    无 长 *work_mem;        /* 工作内存基址 (Agent 分区) */
    整  work_mem_size;      /* 工作内存大小 (字节) */

    /* --- 能力位掩码 --- */
    整  capabilities;       /* AGENT_CAN_READ | WRITE | EXEC | NET */

    /* --- 调度依赖 --- */
    整  dep_count;          /* 依赖的 Agent 数量 */
    整  deps[4];            /* 依赖 Agent 的索引 (最多4个) */

} Agent;

/* ================================================================
 * 调度管线条目 (编译时静态展开)
 * ================================================================ */
typedef struct {
    整 agent_idx;           /* Agent 索引 */
    整 budget;              /* 本轮 token 预算 */
} SchedEntry;

/* ================================================================
 * 四个 Agent 编译时声明
 * 
 * 实例地址: 使用 kernel_mem.h 中定义的 Agent 分区基址
 * Agent 结构体大小: sizeof(Agent) ≈ 64 字节 (编译时常量)
 * 首个 Agent 位于其分区基址
 * ================================================================ */

/* Agent 0: 思考引擎 */
#define AGENT0_NAME        "思考引擎"
#define AGENT0_KV_SLOT     KV_SLOT_THINK
#define AGENT0_BUDGET      TOKEN_BUDGET_THINK
#define AGENT0_MEM_BASE    AGENT_THINK_BASE
#define AGENT0_MEM_SIZE    AGENT_THINK_SIZE
#define AGENT0_CAPS        (AGENT_CAN_READ | AGENT_CAN_NET)
#define AGENT0_GPU         0

/* Agent 1: 代码Agent */
#define AGENT1_NAME        "代码Agent"
#define AGENT1_KV_SLOT     KV_SLOT_CODE
#define AGENT1_BUDGET      TOKEN_BUDGET_CODE
#define AGENT1_MEM_BASE    AGENT_CODE_BASE
#define AGENT1_MEM_SIZE    AGENT_CODE_SIZE
#define AGENT1_CAPS        (AGENT_CAN_READ | AGENT_CAN_WRITE | AGENT_CAN_EXEC)
#define AGENT1_GPU         0

/* Agent 2: 审查Agent */
#define AGENT2_NAME        "审查Agent"
#define AGENT2_KV_SLOT     KV_SLOT_REVIEW
#define AGENT2_BUDGET      TOKEN_BUDGET_REVIEW
#define AGENT2_MEM_BASE    AGENT_REVIEW_BASE
#define AGENT2_MEM_SIZE    AGENT_REVIEW_SIZE
#define AGENT2_CAPS        (AGENT_CAN_READ)
#define AGENT2_GPU         0

/* Agent 3: 测试Agent */
#define AGENT3_NAME        "测试Agent"
#define AGENT3_KV_SLOT     KV_SLOT_TEST
#define AGENT3_BUDGET      TOKEN_BUDGET_TEST
#define AGENT3_MEM_BASE    AGENT_TEST_BASE
#define AGENT3_MEM_SIZE    AGENT_TEST_SIZE
#define AGENT3_CAPS        (AGENT_CAN_EXEC)
#define AGENT3_GPU         0

/* ================================================================
 * 调度管线 (编译时静态展开)
 * 
 * 执行顺序: 思考引擎 → 代码Agent → 审查Agent + 测试Agent(并行)
 * ================================================================ */
#define SCHED_PIPELINE_SIZE  4

/* 调度管线条目 (用于初始化) */
#define SCHED_ENTRY_THINK   { 0, TOKEN_BUDGET_THINK }
#define SCHED_ENTRY_CODE    { 1, TOKEN_BUDGET_CODE }
#define SCHED_ENTRY_REVIEW  { 2, TOKEN_BUDGET_REVIEW }
#define SCHED_ENTRY_TEST    { 3, TOKEN_BUDGET_TEST }

#endif /* KERNEL_AGENT_H */
