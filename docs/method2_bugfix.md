# 方法2 DPT算法修复说明

## 问题描述

用户反馈方法2（DPT频域算法）测得的数据与真实值相差巨大。

## 修复的关键问题

### 1. IIR高通滤波器缺少负号 ⚠️ **严重问题**

**位置**: `ppg_algorithm_v2.c:256`

**问题**: 论文中高通滤波器公式是：
```c
RD_ac = -(int32_t)(w - wn)  // 注意负号
```

**修复前**:
```c
filter->ac_value = (int32_t)(w - filter->w_n);  // 缺少负号
```

**修复后**:
```c
filter->ac_value = -(int32_t)(w - filter->w_n);  // 添加负号
```

**影响**: 这会导致AC信号极性反转，可能导致心率检测完全错误。

---

### 2. DPT相位旋转方向错误 ⚠️ **严重问题**

**位置**: `ppg_algorithm_v2.c:289`

**问题**: 滑动DPT的正确公式是：
```
T_new = e^(-j*2*pi/period) * (T_old - x_old + x_new)
```
注意是 **负号**，因为窗口向前滑动时相位向后旋转。

**修复前**:
```c
float phase_increment = TWO_PI / (float)period;  // 正向旋转，错误！
```

**修复后**:
```c
float phase_increment = -TWO_PI / (float)period;  // 负向旋转，正确
```

**影响**: 相位旋转方向错误会导致频谱完全不正确，心率检测失败。

---

### 3. 频谱幅度未归一化 ⚠️ **中等问题**

**位置**: `ppg_algorithm_v2.c:363`

**问题**: 不同周期的DPT幅度不可直接比较，需要归一化。

**修复前**:
```c
dpt->magnitude[i] = sqrtf(real * real + imag * imag);
```

**修复后**:
```c
float magnitude_raw = sqrtf(real * real + imag * imag);
dpt->magnitude[i] = magnitude_raw / (float)period;  // 归一化
```

**影响**: 未归一化可能导致峰值检测偏向较长周期，影响心率准确性。

---

### 4. 循环缓冲区索引计算问题 ⚠️ **潜在问题**

**位置**: `ppg_algorithm_v2.c:329`

**问题**: 获取旧样本的索引计算需要考虑buffer_index已经递增。

**修复前**:
```c
uint16_t old_idx = (dpt->buffer_index + DPT_BUFFER_SIZE - period) % DPT_BUFFER_SIZE;
```

**修复后**:
```c
uint16_t current_idx = dpt->buffer_index;  // 保存当前索引
dpt->buffer_index = (dpt->buffer_index + 1) % DPT_BUFFER_SIZE;  // 递增
uint16_t old_idx = (current_idx + DPT_BUFFER_SIZE - period + 1) % DPT_BUFFER_SIZE;
```

**影响**: 索引错误会导致使用错误的旧样本，影响DPT计算准确性。

---

### 5. 阈值调整 ℹ️ **优化**

**位置**: `ppg_algorithm_v2.c:33-34`

**修复前**:
```c
#define MIN_DC_VALUE            1000
#define MIN_PEAK_MAGNITUDE      100.0f
```

**修复后**:
```c
#define MIN_DC_VALUE            10000       // 提高到适合MAX30102
#define MIN_PEAK_MAGNITUDE      0.5f        // 降低以适应归一化
```

**影响**:
- `MIN_DC_VALUE`提高是因为MAX30102的ADC范围较大（18-bit）
- `MIN_PEAK_MAGNITUDE`降低是因为归一化后幅度变小

---

## 测试建议

修复后请按以下步骤测试：

### 1. 重新编译

```bash
make clean
make
```

### 2. 观察串口输出

修复后应该看到类似输出：
```
========================================
  Algorithm: Method 2 - DPT Frequency Domain
  Features: High precision (~10s), Based on ADI paper
  Buffer: 1000 samples (10 seconds)
  Period range: 40-200 samples (150-30 bpm)
========================================

[Method2] HR: 78.5 BPM | Peak Period: 77 samples (Valid)
[Method2] SpO2: 97.2 %
```

### 3. 检查心率范围

- 心率应该在 30-150 bpm 范围内
- 初始化约需10秒（填充1000样本缓冲区）
- 心率应该与方法1接近（误差±5 bpm内）

### 4. 检查血氧范围

- SpO2应该在 70-100% 范围内
- SpO2应该与方法1接近（误差±3%内）

### 5. 调试信息

如果仍有问题，可以在 `ppg_algorithm_v2.c` 中添加调试输出：

```c
// 在 DPT_Process 函数中添加
if (state->peak_period > 0 && sample_counter % 100 == 0) {
    printf("[Debug] Peak: %d samples, HR: %.1f, DC_red: %d, DC_ir: %d\r\n",
           state->peak_period, state->heart_rate,
           state->red_filter.dc_value, state->ir_filter.dc_value);
}
```

---

## 理论验证

### DPT递归公式推导

对于滑动窗口DPT，当窗口包含样本 `x[i-Tn+1], ..., x[i]` 时：

```
T_i(Tn) = sum_{k=0}^{Tn-1} x[i-Tn+1+k] * e^(-j*2*pi*k/Tn)
```

当窗口滑动到 `x[i-Tn+2], ..., x[i+1]` 时：

```
T_{i+1}(Tn) = sum_{k=0}^{Tn-1} x[i-Tn+2+k] * e^(-j*2*pi*k/Tn)
            = e^(-j*2*pi/Tn) * sum_{k=1}^{Tn} x[i-Tn+1+k] * e^(-j*2*pi*k/Tn)
            = e^(-j*2*pi/Tn) * [T_i(Tn) - x[i-Tn+1] + x[i+1]]
            = e^(-j*2*pi/Tn) * [T_old - x_old + x_new]
```

这证明了负向相位旋转是正确的。

---

## 参考文献

- Analog Devices RAQ-230: "A Novel Discrete Period Transform Method for Processing Physiological Signals"
- Figure 14: IIR Filter Implementation (注意AC提取的负号)
- Equation 3: DPT Recursive Update Formula

---

**修复完成时间**: 2025-01-15
**修复的关键问题**: 5个
**建议重新测试**: 是

如果修复后仍有问题，请提供：
1. 串口输出的心率和SpO2数值
2. 实际心率（使用其他设备测量）
3. 手指放置是否稳定
4. raw_red 和 raw_ir 的大致范围
