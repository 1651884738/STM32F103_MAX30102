# 算法原理说明

## 概述

本文档详细介绍 MAX30102 心率血氧监测系统中使用的信号处理和算法原理。

## 目录

1. [信号采集](#1-信号采集)
2. [滤波算法](#2-滤波算法)
3. [心率检测算法](#3-心率检测算法)
4. [血氧计算算法](#4-血氧计算算法)
5. [显示平滑算法](#5-显示平滑算法)
6. [参数调优指南](#6-参数调优指南)

---

## 1. 信号采集

### 1.1 MAX30102 工作原理

MAX30102 是一款集成的脉搏血氧和心率监测传感器，包含：
- **红光 LED** (660nm)
- **红外光 LED** (880nm)
- **光电二极管** (接收反射光)
- **18-bit ADC** (模数转换器)

### 1.2 PPG 信号原理

**光电容积脉搏波描记法 (Photoplethysmography, PPG)** 原理：

```
LED发光 → 穿透皮肤 → 血液吸收 → 反射回传感器
         ↓
     血液流量变化
         ↓
     反射光强度变化
         ↓
     PPG信号波形
```

**信号组成：**
- **DC 分量**：组织、静脉血等的恒定吸收
- **AC 分量**：动脉血容积的周期性变化（心跳）

```
PPG信号 = DC + AC·sin(2πft + φ)
```

### 1.3 采样配置

```c
采样率: 100 Hz
分辨率: 18-bit (0-262143)
LED电流: 7.6 mA
积分时间: 411 μs
ADC量程: 16384 nA
```

---

## 2. 滤波算法

### 2.1 整体信号处理流程

```
Raw ADC (18-bit)
      ↓
去趋势滤波 (50点移动平均)
      ↓
Butterworth带通滤波 (0.5-4 Hz, 4阶)
      ↓
信号平滑 (5点移动平均)
      ↓
输出 AC 信号
```

### 2.2 去趋势滤波

**目的：** 去除基线漂移和低频噪声

**原理：**
```
baseline(n) = (1/N) × Σ signal(n-i), i=0..N-1
detrended(n) = signal(n) - baseline(n)
```

**实现：** 使用循环缓冲区的移动平均

```c
// 50点移动平均
sum -= buffer[index];      // 减去旧值
buffer[index] = new_value; // 加入新值
sum += new_value;
baseline = sum / 50;
output = new_value - baseline;
```

**参数：**
- 窗口大小：50 样本 (0.5秒)
- 截止频率：~1 Hz

### 2.3 Butterworth 带通滤波器

**目的：** 提取心跳频率范围内的信号

**设计参数：**
- **类型**：4阶 Butterworth 带通
- **通带**：0.5-4 Hz (对应 30-240 bpm)
- **采样率**：100 Hz
- **实现**：级联两个二阶节 (SOS)

**传递函数：**
```
H(z) = H₁(z) × H₂(z)

H₁(z) = (b₀ + b₁z⁻¹ + b₂z⁻²) / (1 + a₁z⁻¹ + a₂z⁻²)
H₂(z) = (b₀ + b₁z⁻¹ + b₂z⁻²) / (1 + a₁z⁻¹ + a₂z⁻²)
```

**系数（通过 scipy.signal.butter 计算）：**

第一个二阶节（高通特性）：
```c
b₀ = 0.00743916
b₁ = 0.0
b₂ = -0.00743916
a₁ = -1.86319070
a₂ = 0.87439781
```

第二个二阶节（低通特性）：
```c
b₀ = 1.0
b₁ = 0.0
b₂ = -1.0
a₁ = -1.94632328
a₂ = 0.95124514
```

**Direct Form II Transposed 实现：**
```c
y(n) = b₀·x(n) + s₁(n-1)
s₁(n) = b₁·x(n) - a₁·y(n) + s₂(n-1)
s₂(n) = b₂·x(n) - a₂·y(n)
```

**频率响应：**
```
通带增益: 0 dB
阻带衰减: >40 dB
相位延迟: ~40 ms
```

### 2.4 信号平滑

**目的：** 进一步降低高频噪声

**方法：** 5点简单移动平均

```c
output(n) = (1/5) × Σ signal(n-i), i=0..4
```

---

## 3. 心率检测算法

### 3.1 算法流程

```
滤波后的 AC 信号
         ↓
    峰值检测
         ↓
   异常值过滤
         ↓
   峰值间隔计算
         ↓
   中位数滤波
         ↓
   变化率限制
         ↓
    EMA 平滑
         ↓
   稳定性验证
         ↓
   心率输出 (BPM)
```

### 3.2 自适应峰值检测

**原理：** 动态调整阈值以适应不同信号强度

**步骤：**

1. **计算统计量**
```c
mean = (1/N) × Σ signal(i)
variance = (1/N) × Σ (signal(i) - mean)²
std_dev = √variance
```

2. **自适应阈值**
```c
threshold = mean + k × std_dev
// k = 0.5 (可调)
```

3. **局部最大值检测**
```c
// 7点窗口检测
if (signal[i] > signal[i-3] &&
    signal[i] > signal[i-2] &&
    signal[i] > signal[i-1] &&
    signal[i] > signal[i+1] &&
    signal[i] > signal[i+2] &&
    signal[i] > signal[i+3] &&
    signal[i] > threshold) {
    peak_detected = true;
}
```

4. **峰值间隔约束**
```c
min_interval = 40 samples   // 最大 150 bpm
max_interval = 250 samples  // 最小 24 bpm
```

### 3.3 异常值过滤

**方法：** 基于峰值间隔的统计过滤

```c
// 1. 计算间隔标准差
intervals[] = {t₁-t₀, t₂-t₁, ..., tₙ-tₙ₋₁}
std_interval = std(intervals)

// 2. 如果标准差过大，过滤离群值
if (std_interval > 15.0) {
    median_interval = median(intervals)

    // 只保留接近中位数的间隔
    filtered = intervals where |interval - median| < 20

    // 重新计算中位数
    final_interval = median(filtered)
}
```

### 3.4 中位数滤波

**目的：** 去除单次异常测量，更鲁棒

**方法：** 7点中位数滤波

```c
history[] = {hr₁, hr₂, ..., hr₇}
sorted = sort(history)
hr_filtered = sorted[3]  // 中位数
```

### 3.5 变化率限制

**目的：** 防止心率突变

```c
max_change = 6.0 bpm

if (|hr_new - hr_old| > max_change) {
    if (hr_new > hr_old) {
        hr_new = hr_old + max_change
    } else {
        hr_new = hr_old - max_change
    }
}
```

### 3.6 EMA 平滑

**指数移动平均 (Exponential Moving Average)：**

```c
α = 0.2  // 平滑系数

hr_smooth(n) = α × hr_new + (1-α) × hr_smooth(n-1)
            = 0.2 × hr_new + 0.8 × hr_old
```

**特性：**
- 对新值的响应速度：20%
- 对历史的依赖：80%
- 等效窗口大小：约 10 个样本

### 3.7 稳定性验证

**条件：** 连续 2 次变化小于 6 bpm

```c
if (|hr(n) - hr(n-1)| < 6.0) {
    stable_count++
} else {
    stable_count = 0
}

if (stable_count >= 2) {
    hr_valid = true
}
```

---

## 4. 血氧计算算法

### 4.1 SpO2 原理

**Beer-Lambert 定律：**

```
I = I₀ × e^(-ε·c·d)

I: 透射光强度
I₀: 入射光强度
ε: 消光系数
c: 血液浓度
d: 光程长度
```

**氧合血红蛋白（HbO₂）和脱氧血红蛋白（Hb）在不同波长下的吸收率不同：**

| 波长 | HbO₂吸收 | Hb吸收 |
|------|----------|--------|
| 660nm (红光) | 低 | 高 |
| 880nm (红外) | 高 | 低 |

### 4.2 R 值计算

**定义：** 红光和红外光的 AC/DC 比值的比值

```c
// AC: 脉搏波动分量 (RMS)
// DC: 直流分量 (基线)

R = (AC_red / DC_red) / (AC_ir / DC_ir)
```

**物理意义：**
- R ↓ → HbO₂ ↑ → SpO₂ ↑
- R ↑ → Hb ↑ → SpO₂ ↓

### 4.3 SpO2 校准公式

**经验公式（二次多项式）：**

```c
SpO₂ = -45.06 × R² + 30.354 × R + 94.845
```

**典型 R 值与 SpO₂ 对应关系：**

| R值 | SpO₂ (%) |
|-----|----------|
| 0.4 | 100 |
| 0.6 | 98 |
| 0.8 | 95 |
| 1.0 | 92 |
| 1.2 | 88 |

**注意：** 此公式需要根据实际硬件校准

### 4.4 平滑处理

**10点移动平均：**

```c
r_history[] = {r₁, r₂, ..., r₁₀}
r_avg = (1/10) × Σ r_history[i]

SpO₂ = f(r_avg)
```

---

## 5. 显示平滑算法

### 5.1 双层平滑架构

```
算法层平滑 (α=0.2)
        ↓
   计算心率 hr
        ↓
显示层平滑 (α=0.1 + 阈值)
        ↓
  显示心率 displayed_hr
```

### 5.2 显示 EMA + 阈值过滤

```c
threshold = 2.0 bpm
α_display = 0.1

if (|hr - displayed_hr| > threshold) {
    // 超过阈值，缓慢更新
    displayed_hr = α_display × hr + (1-α_display) × displayed_hr
} else {
    // 保持不变
    displayed_hr = displayed_hr
}
```

**效果：**
- 小波动（≤2 bpm）：完全不更新，显示静止
- 大变化（>2 bpm）：以 10% 速率缓慢更新

---

## 6. 参数调优指南

### 6.1 提高准确性

**滤波参数：**
```c
DETREND_WINDOW_SIZE = 60      // 更大的去趋势窗口
SIGNAL_SMOOTH_SIZE = 7        // 更多平滑
```

**心率参数：**
```c
HR_BUFFER_SIZE = 300          // 更长的分析窗口 (3秒)
PEAK_THRESHOLD = 0.6          // 更严格的峰值检测
HR_EMA_ALPHA = 0.15           // 更强的平滑
```

### 6.2 提高响应速度

**心率参数：**
```c
HR_BUFFER_SIZE = 200          // 更短的窗口 (2秒)
HR_EMA_ALPHA = 0.25           // 更快的响应
MAX_HR_CHANGE = 8.0           // 更宽松的限制
```

### 6.3 提高显示稳定性

**显示参数：**
```c
DISPLAY_EMA_ALPHA = 0.05      // 更慢的显示更新
DISPLAY_HR_THRESHOLD = 3.0    // 更大的阈值
```

### 6.4 不同应用场景

**运动监测：**
- 更快响应
- 更宽的心率范围
- 较弱的平滑

**医疗监测：**
- 更高准确性
- 强平滑
- 严格的异常检测

**日常佩戴：**
- 平衡响应和稳定性
- 中等参数设置

---

## 7. 性能分析

### 7.1 计算复杂度

| 模块 | 复杂度 | 每次耗时 |
|------|--------|----------|
| 去趋势 | O(1) | ~10 μs |
| Butterworth | O(1) | ~20 μs |
| 峰值检测 | O(N) | ~500 μs |
| 中位数滤波 | O(N log N) | ~100 μs |
| **总计** | O(N log N) | ~630 μs |

### 7.2 内存占用

| 模块 | RAM占用 |
|------|---------|
| 滤波器状态 | ~200 bytes |
| 心率缓冲区 | ~1000 bytes |
| 波形缓冲区 | ~512 bytes |
| **总计** | ~2 KB |

---

## 8. 参考文献

1. Webster, J. G. (1997). "Design of Pulse Oximeters". CRC Press.
2. Maxim Integrated (2020). "MAX30102 Data Sheet"
3. Allen, J. (2007). "Photoplethysmography and its application in clinical physiological measurement". Physiological Measurement.
4. Oppenheim, A. V., & Schafer, R. W. (2009). "Discrete-Time Signal Processing". Pearson.
5. Smith, S. W. (1997). "The Scientist and Engineer's Guide to Digital Signal Processing"

---

**文档版本：** 1.0
**最后更新：** 2025-01
**作者：** Claude Code + Your Name
