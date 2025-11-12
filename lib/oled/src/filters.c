//
// Created by Administrator on 2025/11/8.
//

#include "../inc/filters.h"

#include <string.h>


/*波形上看，raw_ir,raw_red随着手指按压、身体移动而缓慢地上下漂移
 * 先经过一个高通滤波器来“去趋势”，滤除低于 0.5 Hz 的频率成分
 * 0.5Hz对应的心率是每分钟30次，正常人是远大于此值的
 *
* matlab中获取


/*
 * 提取纯净的PPG信号，需要一个带通滤波器，调研后Butterworth是最合适的
 *一个健康的成年人，静息心率很少低于40 BPM。运动员或睡眠时可能更低，比如35 BPM。
 *剧烈运动时的极限心率，一般不会超过220 BPM。对于普通应用，考虑到240 BPM已经绰绰有余。
 *换算成赫兹 (Hz)：
 *最低频率 = 35 BPM / 60 ≈ 0.6 Hz
 *最高频率 = 240 BPM / 60 = 4.0 Hz
 *
*/

// 4th-Order Butterworth Bandpass, Fs=100Hz, Fpass=[0.6, 4.0]Hz
// Decomposed into 4 second-order sections (SOS)
// --- 带通滤波器系数 (从MATLAB复制) ---
#define NUM_SECTIONS 4
// 每个二阶节的系数格式: {b0, b1, b2, -a1, -a2}
static const float SOS_COEFFS[NUM_SECTIONS][5] = {
    {0.099979762229f, -0.199959524861f, 0.099979762021f, 1.697215985808f, -0.729102623507f}, // Section 1
    {0.099979762229f, 0.199968778676f, 0.099989016875f, 1.813347588903f, -0.869120453549f}, // Section 2
    {0.099979762229f, 0.199950270240f, 0.099970508439f, 1.919920662676f, -0.922262266196f}, // Section 3
    {0.099979762229f, -0.199959524055f, 0.099979762437f, 1.976420085788f, -0.977900900550f}, // Section 4
};

// --- 去趋势滤波器系数 ---
// 使用一阶高通滤波器，截止频率0.5Hz，采样率100Hz
// 公式: y[n] = alpha * (y[n-1] + x[n] - x[n-1])
// alpha = RC / (RC + dt), RC = 1/(2*pi*Fc), dt=1/Fs
// Fc=0.5, Fs=100 -> alpha = 0.969
#define DETREND_ALPHA 0.9690673947f

//======================================================================
// SECTION 2: INDIVIDUAL FILTER IMPLEMENTATIONS
//======================================================================

// --- 手写去趋势滤波器 ---
static void detrend_init(DetrendFilter* filter, float initial_value) {
    // 这里我们用低通滤波器来估算基线，更直观
    // y[n] = alpha * y[n-1] + (1 - alpha) * x[n]
    filter->last_y = initial_value;
}

static float detrend_apply(DetrendFilter* filter, float input) {
    // 更新基线估算值
    filter->last_y = (DETREND_ALPHA * filter->last_y) + (1.0f - DETREND_ALPHA) * input;
    // 返回 AC 分量
    return input - filter->last_y;
}

// --- 手写级联二阶节带通滤波器 ---
static void biquad_cascade_init(BiquadCascadeFilter* filter) {
    // 清空所有状态变量
    memset(filter->state, 0, sizeof(filter->state));
}

static float biquad_cascade_apply(BiquadCascadeFilter* filter, float input) {
    // 对输入信号，依次通过每一个二阶节
    float current_input = input;

    for (int i = 0; i < NUM_SECTIONS; ++i) {
        // 每个二阶节使用直接II型转置结构
        // 数学公式:
        //   y[n] = b0*x[n] + w1[n-1]
        //   w1[n] = b1*x[n] - a1*y[n] + w2[n-1]
        //   w2[n] = b2*x[n] - a2*y[n]

        // 从系数数组中获取当前节的系数
        const float b0 = SOS_COEFFS[i][0];
        const float b1 = SOS_COEFFS[i][1];
        const float b2 = SOS_COEFFS[i][2];
        const float na1 = SOS_COEFFS[i][3]; // 已经是 -a1
        const float na2 = SOS_COEFFS[i][4]; // 已经是 -a2

        // 获取当前节的状态变量 (w1[n-1] 和 w2[n-1])
        // 每个节用2个状态变量，所以第i节的状态变量在 state[i*2] 和 state[i*2+1]
        float w1_old = filter->state[i * 2];
        float w2_old = filter->state[i * 2 + 1];

        // 1. 计算当前节的输出 y[n]
        float output = b0 * current_input + w1_old;

        // 2. 计算并更新当前节的状态变量 w1[n] 和 w2[n]
        // 注意这里的 a1, a2 在公式里是减，但我们的系数已经是 -a1, -a2，所以用加法
        float w1_new = b1 * current_input + na1 * output + w2_old;
        float w2_new = b2 * current_input + na2 * output;

        // 将新状态存回状态数组
        filter->state[i * 2] = w1_new;
        filter->state[i * 2 + 1] = w2_new;

        // 3. 将当前节的输出作为下一节的输入
        current_input = output;
    }

    // 最后一个节的输出就是整个滤波器的最终输出
    return current_input;
}

//======================================================================
// SECTION 3: PPG PIPELINE (PUBLIC INTERFACE)
//======================================================================

void ppg_pipeline_init(PPG_Pipeline* pipeline, float initial_value) {
    detrend_init(&pipeline->detrend, initial_value);
    biquad_cascade_init(&pipeline->bandpass);
}

float ppg_pipeline_process(PPG_Pipeline* pipeline, float raw_sample) {
    // 1. 去趋势
    float ac_sample = detrend_apply(&pipeline->detrend, raw_sample);

    // 2. 带通滤波
    float filtered_sample = biquad_cascade_apply(&pipeline->bandpass, ac_sample);

    return filtered_sample;
}













