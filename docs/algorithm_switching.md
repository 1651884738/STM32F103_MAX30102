# 算法切换使用说明

## 如何在 main.c 中切换算法

本项目提供两种心率和血氧计算算法，您可以通过简单的宏定义切换：

### 方法 1: 时域峰值检测算法（默认）

**文件位置**: `Core/Src/main.c`

找到以下代码段（约第55行）：
```c
#define USE_ALGORITHM_METHOD1       // 方法1: 时域峰值检测 (默认)
// #define USE_ALGORITHM_METHOD2    // 方法2: 频域DPT变换
```

**特点**:
- 快速响应（~5秒初始化）
- 低内存占用（~2KB）
- 适合实时监测应用
- 成熟稳定的时域算法

### 方法 2: 频域DPT变换算法

**文件位置**: `Core/Src/main.c`

修改宏定义为：
```c
// #define USE_ALGORITHM_METHOD1    // 方法1: 时域峰值检测 (默认)
#define USE_ALGORITHM_METHOD2       // 方法2: 频域DPT变换
```

**特点**:
- 高精度（~10秒初始化）
- 中等内存占用（~8KB）
- 基于Analog Devices论文
- 抗噪声能力强
- 频谱分析（161点分辨率）

## 切换步骤

### 步骤 1: 编辑宏定义

打开 `Core/Src/main.c`，找到约第 **45-66行** 的算法选择部分：

```c
/*****************************************************************************
 * 算法方法选择
 * 取消注释其中一行来选择使用的算法：
 * - USE_ALGORITHM_METHOD1: 时域峰值检测算法 (默认)
 *   特点：快速响应(~5秒)，低内存(~2KB)，适合实时监测
 * - USE_ALGORITHM_METHOD2: 频域DPT变换算法
 *   特点：高精度(~10秒)，中等内存(~8KB)，基于ADI论文，抗噪声强
 *****************************************************************************/
#define USE_ALGORITHM_METHOD1       // 方法1: 时域峰值检测 (默认)
// #define USE_ALGORITHM_METHOD2    // 方法2: 频域DPT变换
```

**选择方法1**: 保持默认（如上所示）

**选择方法2**: 修改为：
```c
// #define USE_ALGORITHM_METHOD1    // 方法1: 时域峰值检测 (默认)
#define USE_ALGORITHM_METHOD2       // 方法2: 频域DPT变换
```

### 步骤 2: 重新编译

```bash
# 清理旧的编译文件
make clean

# 重新编译
make

# 或使用 STM32CubeIDE
# 右键项目 -> Clean Project
# 右键项目 -> Build Project
```

### 步骤 3: 烧录固件

```bash
# 使用 st-flash
st-flash write build/MAX30102.bin 0x8000000

# 或使用 STM32CubeIDE
# 右键项目 -> Run As -> STM32 C/C++ Application
```

### 步骤 4: 观察输出

通过串口（115200 bps）查看输出，您会看到：

**方法1输出示例**:
```
========================================
  Algorithm: Method 1 - Time Domain Peak Detection
  Features: Fast response (~5s), Low memory (~2KB)
========================================

[Method1] HR: 78.3 BPM (Valid)
[Method1] SpO2: 97.5 %
```

**方法2输出示例**:
```
========================================
  Algorithm: Method 2 - DPT Frequency Domain
  Features: High precision (~10s), Based on ADI paper
  Buffer: 1000 samples (10 seconds)
  Period range: 40-200 samples (150-30 bpm)
========================================

[Method2] HR: 77.8 BPM | Peak Period: 77 samples (Valid)
[Method2] SpO2: 97.8 %
```

## 安全检查

项目包含编译时检查，确保您只选择一种方法：

**错误1: 两种方法都选择**
```c
#define USE_ALGORITHM_METHOD1
#define USE_ALGORITHM_METHOD2
```
**编译错误**: `Error: Cannot use both methods simultaneously.`

**错误2: 都不选择**
```c
// #define USE_ALGORITHM_METHOD1
// #define USE_ALGORITHM_METHOD2
```
**编译错误**: `Error: Must select one algorithm method.`

## 性能对比

| 指标 | 方法1 | 方法2 |
|------|-------|-------|
| 初始化时间 | ~5秒 | ~10秒 |
| 内存占用 | ~2KB | ~8KB |
| CPU占用 | <30% | <50% |
| 心率精度 | ±3 bpm | ±2 bpm |
| 抗噪声 | 中 | 高 |
| 更新频率 | 2.5秒 | 1秒 |

## 何时选择哪种方法？

### 选择方法1（时域峰值检测）

- ✅ 需要快速响应（5秒内）
- ✅ 内存受限的场景
- ✅ CPU性能有限
- ✅ 实时监测应用
- ✅ 环境相对稳定

### 选择方法2（频域DPT变换）

- ✅ 需要更高精度
- ✅ 噪声环境较强
- ✅ 可以接受较长初始化
- ✅ 需要频谱分析
- ✅ 基于学术论文验证

## 常见问题

### Q1: 可以同时运行两种算法进行对比吗？

A: 理论上可以，但不推荐在主程序中这样做（会占用大量内存和CPU）。如需对比，请参考 `Core/Src/main_usage_example.c` 中的对比模式示例。

### Q2: 切换算法后数值差异很大？

A: 正常现象。两种算法的计算方式不同：
- 方法1使用时域峰值间隔计算
- 方法2使用频谱峰值位置计算

通常差异在 ±5 bpm 以内属于正常范围。

### Q3: 方法2初始化需要多久？

A: 方法2需要填充10秒的数据缓冲区（1000样本@100Hz），因此初始化约需10秒。这是为了获得高精度频谱分析所必需的。

### Q4: 能否调整初始化时间？

A: 可以修改 `ppg_algorithm_v2.h` 中的参数：
```c
#define DPT_BUFFER_SIZE  1000  // 减小此值可缩短初始化，但会降低精度
```

### Q5: 串口输出前缀 [Method1]/[Method2] 能否去掉？

A: 可以在 `main.c` 中修改 printf 语句，去掉前缀标识。

## 技术支持

如有问题，请：
1. 查看 `README.md` 的详细算法说明
2. 阅读 `docs/algorithm.md` 了解算法原理
3. 查看 `CHANGELOG.md` 了解版本更新

## 参考资料

- **方法1**: 自研算法，基于传统DSP信号处理
- **方法2**: 基于 [Analog Devices RAQ-230 论文](https://www.analog.com/cn/resources/analog-dialogue/raqs/raq-issue-230.html)

---

**最后更新**: 2025-01-15
**版本**: v1.1.0
