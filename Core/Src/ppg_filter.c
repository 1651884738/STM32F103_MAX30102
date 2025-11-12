#include "ppg_filter.h"
#include <string.h>
#include <math.h>

// Butterworth 4阶带通滤波器系数 (0.5-4 Hz @ 100Hz采样率)
// 使用 Python scipy: butter(4, [0.5, 4], 'bandpass', fs=100, output='sos')
// 级联两个二阶节 (Second-Order Sections)
static const BiquadCoeff_t butterworth_sos[NUM_SOS_SECTIONS] = {
    // 第一个二阶节
    {
        .b0 = 0.00743916f,
        .b1 = 0.0f,
        .b2 = -0.00743916f,
        .a1 = -1.86319070f,
        .a2 = 0.87439781f
    },
    // 第二个二阶节
    {
        .b0 = 1.0f,
        .b1 = 0.0f,
        .b2 = -1.0f,
        .a1 = -1.94632328f,
        .a2 = 0.95124514f
    }
};

/**
 * @brief 初始化滤波器状态
 * @param filter 滤波器状态指针
 */
void PPG_Filter_Init(PPG_FilterState_t *filter) {
    memset(filter, 0, sizeof(PPG_FilterState_t));
    filter->detrend_index = 0;
    filter->detrend_sum = 0.0f;
    filter->detrend_filled = 0;
    filter->smooth_index = 0;
    filter->dc_value = 0.0f;
    filter->ac_squared_sum = 0.0f;
    filter->sample_count = 0;
}

/**
 * @brief 二阶节滤波器（Direct Form II Transposed）
 * @param input 输入信号
 * @param coeff 滤波器系数
 * @param state 滤波器状态
 * @return 滤波后的输出
 */
static float biquad_filter(float input, const BiquadCoeff_t *coeff, BiquadState_t *state) {
    // Direct Form II Transposed 实现
    float output = coeff->b0 * input + state->x1;
    state->x1 = coeff->b1 * input - coeff->a1 * output + state->x2;
    state->x2 = coeff->b2 * input - coeff->a2 * output;
    return output;
}

/**
 * @brief 去趋势处理（减去移动平均基线）
 * @param filter 滤波器状态指针
 * @param value 输入值
 * @return 去趋势后的值
 */
static float detrend_signal(PPG_FilterState_t *filter, float value) {
    // 更新移动平均窗口
    if (filter->detrend_filled) {
        // 减去旧值
        filter->detrend_sum -= filter->detrend_buffer[filter->detrend_index];
    }

    // 添加新值
    filter->detrend_buffer[filter->detrend_index] = value;
    filter->detrend_sum += value;
    filter->detrend_index++;

    if (filter->detrend_index >= DETREND_WINDOW_SIZE) {
        filter->detrend_index = 0;
        filter->detrend_filled = 1;
    }

    // 计算移动平均（基线）
    uint16_t count = filter->detrend_filled ? DETREND_WINDOW_SIZE : filter->detrend_index;
    if (count == 0) count = 1;  // 防止除零

    float baseline = filter->detrend_sum / count;

    // 保存DC值用于血氧计算
    filter->dc_value = baseline;

    // 返回去趋势后的信号
    return value - baseline;
}

/**
 * @brief 处理单个原始样本
 * @param filter 滤波器状态指针
 * @param raw_value 原始ADC值（18位）
 * @return 滤波后的AC信号
 */
float PPG_Filter_Process(PPG_FilterState_t *filter, uint32_t raw_value) {
    float signal = (float)raw_value;

    // 1. 去趋势（去除基线漂移）
    float detrended = detrend_signal(filter, signal);

    // 2. Butterworth 4阶带通滤波 (级联两个二阶节)
    float filtered = detrended;
    for (uint8_t i = 0; i < NUM_SOS_SECTIONS; i++) {
        filtered = biquad_filter(filtered, &butterworth_sos[i], &filter->biquad_states[i]);
    }

    // 3. 后处理平滑（3点移动平均）
    filter->smooth_buffer[filter->smooth_index] = filtered;
    filter->smooth_index = (filter->smooth_index + 1) % SIGNAL_SMOOTH_SIZE;

    float smoothed = 0.0f;
    for (uint8_t i = 0; i < SIGNAL_SMOOTH_SIZE; i++) {
        smoothed += filter->smooth_buffer[i];
    }
    smoothed /= SIGNAL_SMOOTH_SIZE;

    // 4. 计算AC RMS（用于血氧计算）
    filter->ac_squared_sum += smoothed * smoothed;
    filter->sample_count++;

    return smoothed;
}

/**
 * @brief 获取DC分量
 * @param filter 滤波器状态指针
 * @return DC值
 */
float PPG_Filter_GetDC(PPG_FilterState_t *filter) {
    return filter->dc_value;
}

/**
 * @brief 获取AC RMS值
 * @param filter 滤波器状态指针
 * @return AC RMS值
 */
float PPG_Filter_GetACRMS(PPG_FilterState_t *filter) {
    if (filter->sample_count == 0) {
        return 0.0f;
    }

    float mean_squared = filter->ac_squared_sum / filter->sample_count;
    float rms = sqrtf(mean_squared);

    // 重置累加器
    filter->ac_squared_sum = 0.0f;
    filter->sample_count = 0;

    return rms;
}
