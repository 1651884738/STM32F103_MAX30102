#ifndef PPG_ALGORITHM_H
#define PPG_ALGORITHM_H

#include <stdint.h>

// 心率计算配置
#define HR_BUFFER_SIZE      250    // 心率计算缓冲区大小（250个样本 = 2.5秒@100Hz）
#define MIN_PEAK_DISTANCE   40     // 峰值之间最小距离（样本数），对应最大心率150bpm
#define MAX_PEAK_DISTANCE   250    // 峰值之间最大距离（样本数），对应最小心率24bpm
#define PEAK_THRESHOLD      0.5f   // 峰值检测阈值系数（相对于标准差）

// 心率平滑参数
#define HR_MEDIAN_FILTER_SIZE  7   // 使用中位数滤波平滑心率（增加到7）
#define HR_EMA_ALPHA       0.2f    // 心率EMA平滑系数（稍微提高响应速度）
#define MAX_HR_CHANGE      6.0f    // 单次最大心率变化（bpm），防止突变

// 心率计算状态结构体
typedef struct {
    float buffer[HR_BUFFER_SIZE];       // AC信号缓冲区
    uint16_t buffer_index;               // 当前索引
    uint8_t buffer_full;                 // 缓冲区是否已满

    uint16_t last_peak_index;            // 上一个峰值位置（全局索引）
    uint16_t global_index;               // 全局样本索引

    float hr_history[HR_MEDIAN_FILTER_SIZE];  // 心率历史（用于中位数滤波）
    uint8_t hr_history_index;
    uint8_t hr_history_count;

    float last_hr;                       // 上次计算的心率
    float ema_hr;                        // EMA平滑后的心率
    uint8_t hr_valid;                    // 心率是否有效
    uint8_t stable_count;                // 稳定计数器
} HR_State_t;

// 血氧计算状态结构体
typedef struct {
    float r_history[10];                 // R值历史（用于平滑）
    uint8_t r_history_index;
    uint8_t r_history_count;

    float last_spo2;                     // 上次计算的SpO2
    uint8_t spo2_valid;                  // SpO2是否有效
} SpO2_State_t;

// 函数声明
void HR_Init(HR_State_t *hr_state);
void HR_AddSample(HR_State_t *hr_state, float ac_value);
float HR_Calculate(HR_State_t *hr_state);
uint8_t HR_IsValid(HR_State_t *hr_state);

void SpO2_Init(SpO2_State_t *spo2_state);
float SpO2_Calculate(SpO2_State_t *spo2_state, float red_ac_rms, float red_dc,
                      float ir_ac_rms, float ir_dc);
uint8_t SpO2_IsValid(SpO2_State_t *spo2_state);

#endif // PPG_ALGORITHM_H
