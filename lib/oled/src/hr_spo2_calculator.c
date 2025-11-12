#include "hr_spo2_calculator.h"
#include <math.h>
#include <stdint.h>

// =================================================================================
// 核心流程
// 原始信号 (raw_ir, raw_red) → 滤波 → 纯净的AC信号 (ac_ir, ac_red) → 结合原始信号计算 →
// DC分量 (dc_ir, dc_red) → 计算SpO2 & 计算心率
// =================================================================================

// SpO2计算：
// 血氧饱和度的计算依赖于红色光（Red）和红外光（IR）信号的交流（AC）和直流（DC）分量。
// DC分量：代表了血液、组织和骨骼吸收的恒定光量，可以看作是原始信号的平均值。
// 信号的基线或平均值。代表静态的、不变化的部分。
// AC分量：代表了随心跳搏动，动脉血容量变化引起的光量变化，也就是我们滤波后得到的信号。
// 信号围绕基线波动的部分。代表动态的、变化的部分。
// 接下来要计算R值（氧合血红蛋白与脱氧血红蛋白的比值）R = (ACred/DCred)/(ACir/DCir)
// 即 R = (ACred * DCir) / ( DCred * ACir)
// 血氧饱和度：SpO2 = 104 - 17R


// 心率计算：
// 率就是计算心跳的频率。在纯净的PPG波形上，每个波峰就对应一次心跳。
// 我们只需要测量相邻波峰之间的时间间隔，就可以换算出心率。
// current_peak_index 是当前峰值的序号
// last_peak_index 是上一个峰值的序号
// Interval_in_samples = current_peak_index - last_peak_index
//  Fs 是采样频率 (这里是 100 Hz)
// Heart_Rate (BPM) = (60 * Fs) / Interval_in_samples

//
// 参考：https://www.analog.com/cn/resources/analog-dialogue/raqs/raq-issue-230.html
//
//
// =================================================================================
// 算法实现
// =================================================================================




/**
 * @brief  处理单次PPG采样数据，分离AC和DC分量
 * @param  RD_in   当前红色光的原始ADC值
 * @param  IR_in   当前红外光的原始ADC值
 * @param  pRD_ac  指向存储红色光AC分量的指针
 * @param  pRD_dc  指向存储红色光DC分量的指针
 * @param  pIR_ac  指向存储红外光AC分量的指针
 * @param  pIR_dc  指向存储红外光DC分量的指针
 */
void filter_ppg_signal(int32_t RD_in, int32_t IR_in,
                         int32_t *pRD_ac, int32_t *pRD_dc,
                         int32_t *pIR_ac, int32_t *pIR_dc)
{
    // --- 声明为函数内的静态变量  ---
    // 'static' 关键字让这些变量只在第一次调用此函数时被初始化为0。
    // 在后续的调用中，它们会保持上一次计算结束时的值。
    // 这正是IIR滤波器状态变量所需要的特性。

    // 高通滤波器 (AC) 的状态变量
    static float wn = 0.0f; // 红色通道AC滤波器的历史值
    static float xn = 0.0f; // 红外通道AC滤波器的历史值

    // 低通滤波器 (DC) 的状态变量
    static float yn = 0.0f; // 红色通道DC滤波器的历史值
    static float zn = 0.0f; // 红外通道DC滤波器的历史值

    // 中间计算变量 (不需要static，因为每次都会被重新计算)
    float w;
    float x;

    // =================================================================================
    // 2. 滤波器计算
    // =================================================================================

    // -- 高通IIR滤波器，提取AC信号 --
    // 红色通道
    w = (float)RD_in + 0.99f * wn;
    *pRD_ac = -(int32_t)(w - wn);
    wn = w;

    // 红外通道
    x = (float)IR_in + 0.99f * xn;
    *pIR_ac = -(int32_t)(x - xn);
    xn = x;

    // -- 低通IIR滤波器，提取DC信号 --
    // 红色通道
    yn = 0.99f * yn + 0.01f * (float)RD_in;
    *pRD_dc = (int32_t)yn;

    // 红外通道
    zn = 0.99f * zn + 0.01f * (float)IR_in;
    *pIR_dc = (int32_t)zn;
}