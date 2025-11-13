#ifndef PPG_ALGORITHM_H
#define PPG_ALGORITHM_H

#include <stdint.h>

// 心率计算配置
#define HR_BUFFER_SIZE      160    // 心率计算缓冲区大小（160个样本 = 1.6秒@100Hz，进一步减少内存）
#define MIN_PEAK_DISTANCE   40     // 峰值之间最小距离（样本数），对应最大心率150bpm
#define MAX_PEAK_DISTANCE   160    // 峰值之间最大距离（样本数），对应最小心率37.5bpm
#define PEAK_THRESHOLD      0.5f   // 峰值检测阈值系数（相对于标准差）

// 信号质量评估参数
#define MIN_AC_DC_RATIO     0.01f  // 最小AC/DC比值，用于信号质量评估
#define MIN_PEAK_AMPLITUDE  10.0f  // 最小峰峰值幅度，用于信号质量评估
#define SIGNAL_QUALITY_WINDOW 32  // 信号质量评估窗口大小（减少内存使用）

// 心率平滑参数
#define HR_MEDIAN_FILTER_SIZE  5   // 使用中位数滤波平滑心率（减少内存）
#define HR_EMA_ALPHA       0.2f    // 心率EMA平滑系数（稍微提高响应速度）
#define MAX_HR_CHANGE      6.0f    // 单次最大心率变化（bpm），防止突变
#define INVALID_RESET_THRESHOLD 2 // 无效信号重置阈值（连续无效次数）

// 心率计算状态结构体
typedef struct {
    float buffer[HR_BUFFER_SIZE];       // AC信号缓冲区
    uint16_t buffer_index;               // 当前索引
    uint8_t buffer_full;                 // 缓冲区是否已满

    uint16_t last_peak_index;            // 上一个峰值位置（全局索引）
    uint16_t global_index;               // 全局样本索引

    // 增量统计 (Welford's algorithm)
    float rolling_mean;                  // 滚动均值
    float rolling_variance;              // 滚动方差
    uint16_t rolling_count;              // 滚动计数

    // 信号质量评估（简化版减少内存）
    float recent_dc_value;               // 最近DC值（简化为单个值）
    float peak_amplitude;                // 峰峰值幅度
    float ac_dc_ratio;                   // AC/DC比值
    uint8_t signal_quality;              // 信号质量标志 (0=差, 1=中, 2=好)
    uint8_t consecutive_invalid;        // 连续无效计数

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
void HR_AddSample(HR_State_t *hr_state, float ac_value, float dc_value);
float HR_Calculate(HR_State_t *hr_state);
uint8_t HR_IsValid(HR_State_t *hr_state);
uint8_t HR_GetSignalQuality(HR_State_t *hr_state);
void HR_Reset(HR_State_t *hr_state);

void SpO2_Init(SpO2_State_t *spo2_state);
float SpO2_Calculate(SpO2_State_t *spo2_state, float red_ac_rms, float red_dc,
                      float ir_ac_rms, float ir_dc);
uint8_t SpO2_IsValid(SpO2_State_t *spo2_state);
void SpO2_Reset(SpO2_State_t *spo2_state);

#endif // PPG_ALGORITHM_H
