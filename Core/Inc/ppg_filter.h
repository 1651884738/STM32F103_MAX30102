#ifndef PPG_FILTER_H
#define PPG_FILTER_H

#include <stdint.h>

// 去趋势滤波器参数（移动平均窗口，用于计算基线）
#define DETREND_WINDOW_SIZE  32    // 去趋势窗口大小（减少内存使用）

// 信号后处理平滑
#define SIGNAL_SMOOTH_SIZE   5     // 输出信号额外平滑窗口（5点移动平均）

// Butterworth 4阶带通滤波器 (0.5-4 Hz @ 100Hz采样率)
// 通过级联两个2阶节实现
// 使用 scipy.signal 设计: butter(4, [0.5, 4], 'bandpass', fs=100, output='sos')
#define NUM_SOS_SECTIONS  2  // 两个二阶节

// 二阶节系数结构体 (SOS格式: b0, b1, b2, a1, a2)
typedef struct {
    float b0, b1, b2;  // 分子系数
    float a1, a2;      // 分母系数（a0=1已归一化）
} BiquadCoeff_t;

// 二阶节状态
typedef struct {
    float x1, x2;  // 输入延迟
    float y1, y2;  // 输出延迟
} BiquadState_t;

// 单通道滤波器状态结构体
typedef struct {
    // 去趋势部分
    float detrend_buffer[DETREND_WINDOW_SIZE];
    uint8_t detrend_index;
    float detrend_sum;
    uint8_t detrend_filled;

    // Butterworth滤波器状态（级联两个二阶节）
    BiquadState_t biquad_states[NUM_SOS_SECTIONS];

    // 后处理平滑
    float smooth_buffer[SIGNAL_SMOOTH_SIZE];
    uint8_t smooth_index;

    // DC和AC统计
    float dc_value;
    float ac_squared_sum;
    uint32_t sample_count;
} PPG_FilterState_t;

// 函数声明
void PPG_Filter_Init(PPG_FilterState_t *filter);
float PPG_Filter_Process(PPG_FilterState_t *filter, uint32_t raw_value);
float PPG_Filter_GetDC(PPG_FilterState_t *filter);
float PPG_Filter_GetACRMS(PPG_FilterState_t *filter);

#endif // PPG_FILTER_H
