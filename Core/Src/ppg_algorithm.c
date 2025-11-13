#include "ppg_algorithm.h"
#include <string.h>
#include <math.h>

/**
 * @brief 中位数滤波辅助函数（用于排序）
 */
static int compare_float(const void *a, const void *b) {
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    return (fa > fb) - (fa < fb);
}

/**
 * @brief 计算数组的中位数
 */
static float median_filter(float *data, uint8_t size) {
    if (size == 0) return 0.0f;

    // 创建临时数组用于排序
    float temp[HR_MEDIAN_FILTER_SIZE];
    for (uint8_t i = 0; i < size; i++) {
        temp[i] = data[i];
    }

    // 简单冒泡排序（数据量小，性能足够）
    for (uint8_t i = 0; i < size - 1; i++) {
        for (uint8_t j = 0; j < size - i - 1; j++) {
            if (temp[j] > temp[j + 1]) {
                float t = temp[j];
                temp[j] = temp[j + 1];
                temp[j + 1] = t;
            }
        }
    }

    // 返回中位数
    if (size % 2 == 0) {
        return (temp[size/2 - 1] + temp[size/2]) / 2.0f;
    } else {
        return temp[size/2];
    }
}

/**
 * @brief 初始化心率状态
 * @param hr_state 心率状态指针
 */
void HR_Init(HR_State_t *hr_state) {
    memset(hr_state, 0, sizeof(HR_State_t));
    hr_state->buffer_index = 0;
    hr_state->buffer_full = 0;
    hr_state->last_peak_index = 0;
    hr_state->global_index = 0;
    hr_state->last_hr = 0.0f;
    hr_state->ema_hr = 0.0f;
    hr_state->hr_valid = 0;
    hr_state->hr_history_index = 0;
    hr_state->hr_history_count = 0;
    hr_state->stable_count = 0;
    
    // 初始化增量统计
    hr_state->rolling_mean = 0.0f;
    hr_state->rolling_variance = 0.0f;
    hr_state->rolling_count = 0;
    
    // 初始化信号质量评估（简化版）
    hr_state->recent_dc_value = 0.0f;
    hr_state->peak_amplitude = 0.0f;
    hr_state->ac_dc_ratio = 0.0f;
    hr_state->signal_quality = 0;
    hr_state->consecutive_invalid = 0;
}

/**
 * @brief 添加AC样本到心率缓冲区（增量更新统计）
 * @param hr_state 心率状态指针
 * @param ac_value AC信号值
 * @param dc_value DC信号值
 */
void HR_AddSample(HR_State_t *hr_state, float ac_value, float dc_value) {
    // 存储AC值
    hr_state->buffer[hr_state->buffer_index] = ac_value;
    hr_state->buffer_index++;
    hr_state->global_index++;

    if (hr_state->buffer_index >= HR_BUFFER_SIZE) {
        hr_state->buffer_index = 0;
        hr_state->buffer_full = 1;
    }

    // 更新DC值（简化版）
    hr_state->recent_dc_value = dc_value;

    // Welford's algorithm for incremental mean and variance
    if (hr_state->rolling_count == 0) {
        hr_state->rolling_mean = ac_value;
        hr_state->rolling_variance = 0.0f;
    } else {
        float delta = ac_value - hr_state->rolling_mean;
        hr_state->rolling_mean += delta / (hr_state->rolling_count + 1);
        float delta2 = ac_value - hr_state->rolling_mean;
        hr_state->rolling_variance += delta * delta2;
    }
    hr_state->rolling_count++;

    // 保持滚动窗口大小
    if (hr_state->rolling_count > HR_BUFFER_SIZE) {
        // 移除最旧的样本（简化处理，重新计算）
        hr_state->rolling_count = HR_BUFFER_SIZE;
        // 重新计算统计量（仅在窗口满时偶尔执行）
        if (hr_state->rolling_count == HR_BUFFER_SIZE && hr_state->buffer_full) {
            float sum = 0.0f;
            for (uint16_t i = 0; i < HR_BUFFER_SIZE; i++) {
                sum += hr_state->buffer[i];
            }
            hr_state->rolling_mean = sum / HR_BUFFER_SIZE;
            
            float var_sum = 0.0f;
            for (uint16_t i = 0; i < HR_BUFFER_SIZE; i++) {
                float diff = hr_state->buffer[i] - hr_state->rolling_mean;
                var_sum += diff * diff;
            }
            hr_state->rolling_variance = var_sum / HR_BUFFER_SIZE;
        }
    }

    // 更新AC/DC比值
    if (dc_value > 1000.0f) {  // 避免除零
        float ac_rms = sqrtf(hr_state->rolling_variance);
        hr_state->ac_dc_ratio = ac_rms / dc_value;
    }
}

/**
 * @brief 评估信号质量
 * @param hr_state 心率状态指针
 * @return 信号质量等级 (0=差, 1=中, 2=好)
 */
static uint8_t assess_signal_quality(HR_State_t *hr_state) {
    uint8_t quality = 0;  // 默认差
    
    // 检查AC/DC比值
    if (hr_state->ac_dc_ratio >= MIN_AC_DC_RATIO) {
        quality++;
    }
    
    // 检查标准差（信号强度）
    float std_dev = sqrtf(hr_state->rolling_variance);
    if (std_dev >= 5.0f) {
        quality++;
    }
    
    // 检查峰峰值幅度
    if (hr_state->peak_amplitude >= MIN_PEAK_AMPLITUDE) {
        quality++;
    }
    
    // 将质量映射到0-2范围
    if (quality >= 3) {
        return 2;  // 好
    } else if (quality >= 2) {
        return 1;  // 中
    } else {
        return 0;  // 差
    }
}

/**
 * @brief 重置心率状态（当信号质量差时）
 * @param hr_state 心率状态指针
 */
void HR_Reset(HR_State_t *hr_state) {
    // 保持缓冲区和基本状态
    // 只重置平滑和验证相关的状态
    
    hr_state->hr_history_index = 0;
    hr_state->hr_history_count = 0;
    hr_state->last_hr = 0.0f;
    hr_state->ema_hr = 0.0f;
    hr_state->hr_valid = 0;
    hr_state->stable_count = 0;
    hr_state->consecutive_invalid = 0;
    
    // 重置信号质量
    hr_state->signal_quality = 0;
    hr_state->peak_amplitude = 0.0f;
    hr_state->recent_dc_value = 0.0f;
}

/**
 * @brief 获取当前信号质量
 * @param hr_state 心率状态指针
 * @return 信号质量等级 (0=差, 1=中, 2=好)
 */
uint8_t HR_GetSignalQuality(HR_State_t *hr_state) {
    return hr_state->signal_quality;
}

/**
 * @brief 查找峰值并计算心率
 * @param hr_state 心率状态指针
 * @return 心率值（BPM）
 */
float HR_Calculate(HR_State_t *hr_state) {
    // 如果缓冲区未满，返回上次心率
    if (!hr_state->buffer_full) {
        return hr_state->last_hr;
    }

    // 1. 使用增量统计（避免O(N)扫描）
    float mean = hr_state->rolling_mean;
    float variance = hr_state->rolling_variance / hr_state->rolling_count;
    float std_dev = sqrtf(variance);

    // 2. 评估信号质量
    hr_state->signal_quality = assess_signal_quality(hr_state);
    
    // 如果信号质量差，增加无效计数
    if (hr_state->signal_quality == 0) {
        hr_state->consecutive_invalid++;
        if (hr_state->consecutive_invalid >= INVALID_RESET_THRESHOLD) {
            HR_Reset(hr_state);
        }
        hr_state->hr_valid = 0;
        return hr_state->last_hr;
    } else {
        hr_state->consecutive_invalid = 0;
    }

    // 3. 自适应阈值（根据信号质量调整）
    float threshold_multiplier = (hr_state->signal_quality == 2) ? 0.4f : 
                               (hr_state->signal_quality == 1) ? 0.5f : 0.6f;
    float threshold = mean + threshold_multiplier * std_dev;

    // 4. 查找峰值（使用更严格的条件）
    typedef struct {
        uint16_t index;
        float value;
    } Peak_t;

    Peak_t peaks[20];
    uint8_t peak_count = 0;
    float min_val = mean, max_val = mean;

    // 查找所有峰值，同时计算峰峰值幅度
    for (uint16_t i = 3; i < HR_BUFFER_SIZE - 3; i++) {
        // 跟踪最小最大值
        if (hr_state->buffer[i] < min_val) min_val = hr_state->buffer[i];
        if (hr_state->buffer[i] > max_val) max_val = hr_state->buffer[i];
        
        // 检查是否是局部最大值（使用更大的窗口：前后各3个样本）
        if (hr_state->buffer[i] > hr_state->buffer[i - 1] &&
            hr_state->buffer[i] > hr_state->buffer[i - 2] &&
            hr_state->buffer[i] > hr_state->buffer[i - 3] &&
            hr_state->buffer[i] > hr_state->buffer[i + 1] &&
            hr_state->buffer[i] > hr_state->buffer[i + 2] &&
            hr_state->buffer[i] > hr_state->buffer[i + 3] &&
            hr_state->buffer[i] > threshold) {

            // 检查与上一个峰值的距离
            if (peak_count == 0 || (i - peaks[peak_count - 1].index) >= MIN_PEAK_DISTANCE) {
                peaks[peak_count].index = i;
                peaks[peak_count].value = hr_state->buffer[i];
                peak_count++;
                if (peak_count >= 20) break;
            }
        }
    }
    
    // 更新峰峰值幅度
    hr_state->peak_amplitude = max_val - min_val;

    // 如果峰值太少，无法计算心率
    if (peak_count < 2) {
        hr_state->consecutive_invalid++;
        if (hr_state->consecutive_invalid >= INVALID_RESET_THRESHOLD) {
            HR_Reset(hr_state);
        }
        hr_state->hr_valid = 0;
        return hr_state->last_hr;
    }

    // 5. 计算所有峰值间隔，并过滤异常值
    float intervals[19];
    uint8_t valid_interval_count = 0;

    for (uint8_t i = 1; i < peak_count; i++) {
        uint16_t interval = peaks[i].index - peaks[i - 1].index;

        // 只保留合理范围内的间隔
        if (interval >= MIN_PEAK_DISTANCE && interval <= MAX_PEAK_DISTANCE) {
            intervals[valid_interval_count] = (float)interval;
            valid_interval_count++;
        }
    }

    if (valid_interval_count < 2) {
        hr_state->consecutive_invalid++;
        if (hr_state->consecutive_invalid >= INVALID_RESET_THRESHOLD) {
            HR_Reset(hr_state);
        }
        hr_state->hr_valid = 0;
        return hr_state->last_hr;
    }

    // 6. 计算间隔的中位数和标准差（更鲁棒）
    float median_interval = median_filter(intervals, valid_interval_count);

    // 计算标准差，过滤离群值
    float mean_interval = 0.0f;
    for (uint8_t i = 0; i < valid_interval_count; i++) {
        mean_interval += intervals[i];
    }
    mean_interval /= valid_interval_count;

    float interval_variance = 0.0f;
    for (uint8_t i = 0; i < valid_interval_count; i++) {
        float diff = intervals[i] - mean_interval;
        interval_variance += diff * diff;
    }
    interval_variance /= valid_interval_count;
    float std_interval = sqrtf(interval_variance);

    // 如果间隔标准差过大，说明心率不稳定
    if (std_interval > 15.0f && valid_interval_count > 2) {
        // 重新筛选，只保留接近中位数的间隔
        float filtered_intervals[19];
        uint8_t filtered_count = 0;

        for (uint8_t i = 0; i < valid_interval_count; i++) {
            if (fabsf(intervals[i] - median_interval) < 20.0f) {
                filtered_intervals[filtered_count] = intervals[i];
                filtered_count++;
            }
        }

        if (filtered_count >= 2) {
            median_interval = median_filter(filtered_intervals, filtered_count);
        }
    }

    // 7. 计算心率 (BPM)
    // 心率 = 60 / (间隔时间) = 60 / (median_interval / 100) = 6000 / median_interval
    float hr = 6000.0f / median_interval;

    // 8. 合理性检查
    if (hr < 30.0f || hr > 180.0f) {
        hr_state->consecutive_invalid++;
        if (hr_state->consecutive_invalid >= INVALID_RESET_THRESHOLD) {
            HR_Reset(hr_state);
        }
        hr_state->hr_valid = 0;
        return hr_state->last_hr;
    }

    // 9. 存储到历史并进行中位数滤波
    hr_state->hr_history[hr_state->hr_history_index] = hr;
    hr_state->hr_history_index = (hr_state->hr_history_index + 1) % HR_MEDIAN_FILTER_SIZE;
    if (hr_state->hr_history_count < HR_MEDIAN_FILTER_SIZE) {
        hr_state->hr_history_count++;
    }

    // 计算中位数滤波后的心率
    float filtered_hr = median_filter(hr_state->hr_history, hr_state->hr_history_count);

    // 10. 变化率限制（防止突变）
    if (hr_state->ema_hr > 0.0f) {
        float diff = filtered_hr - hr_state->ema_hr;
        // 限制单次变化幅度
        if (diff > MAX_HR_CHANGE) {
            filtered_hr = hr_state->ema_hr + MAX_HR_CHANGE;
        } else if (diff < -MAX_HR_CHANGE) {
            filtered_hr = hr_state->ema_hr - MAX_HR_CHANGE;
        }
    }

    // 11. EMA平滑（指数移动平均）
    if (hr_state->ema_hr == 0.0f) {
        // 首次初始化
        hr_state->ema_hr = filtered_hr;
    } else {
        // EMA公式: EMA(t) = alpha * value(t) + (1-alpha) * EMA(t-1)
        hr_state->ema_hr = HR_EMA_ALPHA * filtered_hr + (1.0f - HR_EMA_ALPHA) * hr_state->ema_hr;
    }

    // 12. 稳定性检查（需要连续几次稳定的测量）
    if (hr_state->hr_history_count >= 2) {
        // 检查变化是否在合理范围内
        float diff = fabsf(hr_state->ema_hr - hr_state->last_hr);
        if (diff < 6.0f || hr_state->last_hr == 0.0f) {
            hr_state->stable_count++;
        } else {
            hr_state->stable_count = 0;
        }

        // 需要连续2次稳定测量（约6秒）
        if (hr_state->stable_count >= 2) {
            hr_state->hr_valid = 1;
            hr_state->consecutive_invalid = 0;  // 重置无效计数
        }
    }

    hr_state->last_hr = hr_state->ema_hr;
    return hr_state->ema_hr;
}

/**
 * @brief 检查心率是否有效
 * @param hr_state 心率状态指针
 * @return 1=有效, 0=无效
 */
uint8_t HR_IsValid(HR_State_t *hr_state) {
    return hr_state->hr_valid;
}

/**
 * @brief 初始化血氧状态
 * @param spo2_state 血氧状态指针
 */
void SpO2_Init(SpO2_State_t *spo2_state) {
    memset(spo2_state, 0, sizeof(SpO2_State_t));
    spo2_state->r_history_index = 0;
    spo2_state->r_history_count = 0;
    spo2_state->last_spo2 = 0.0f;
    spo2_state->spo2_valid = 0;
}

/**
 * @brief 计算血氧饱和度
 * @param spo2_state 血氧状态指针
 * @param red_ac_rms 红光AC RMS值
 * @param red_dc 红光DC值
 * @param ir_ac_rms 红外光AC RMS值
 * @param ir_dc 红外光DC值
 * @return SpO2值 (%)
 */
float SpO2_Calculate(SpO2_State_t *spo2_state, float red_ac_rms, float red_dc,
                      float ir_ac_rms, float ir_dc) {
    // 检查除零
    if (red_dc < 1000.0f || ir_dc < 1000.0f || ir_ac_rms < 1.0f) {
        spo2_state->spo2_valid = 0;
        return spo2_state->last_spo2;
    }

    // 计算R值
    // R = (AC_red / DC_red) / (AC_ir / DC_ir)
    float r = (red_ac_rms / red_dc) / (ir_ac_rms / ir_dc);

    // R值合理性检查
    if (r < 0.1f || r > 2.0f) {
        spo2_state->spo2_valid = 0;
        return spo2_state->last_spo2;
    }

    // 存储R值到历史
    spo2_state->r_history[spo2_state->r_history_index] = r;
    spo2_state->r_history_index = (spo2_state->r_history_index + 1) % 10;
    if (spo2_state->r_history_count < 10) {
        spo2_state->r_history_count++;
    }

    // 计算平均R值
    float avg_r = 0.0f;
    for (uint8_t i = 0; i < spo2_state->r_history_count; i++) {
        avg_r += spo2_state->r_history[i];
    }
    avg_r /= spo2_state->r_history_count;

    // SpO2计算公式（经验公式，可能需要根据实际情况校准）
    // 使用更常见的二次多项式公式
    float spo2 = -45.060f * avg_r * avg_r + 30.354f * avg_r + 94.845f;

    // SpO2合理性检查
    if (spo2 < 70.0f || spo2 > 100.0f) {
        spo2_state->spo2_valid = 0;
        return spo2_state->last_spo2;
    }

    spo2_state->last_spo2 = spo2;
    spo2_state->spo2_valid = 1;

    return spo2;
}

/**
 * @brief 检查血氧是否有效
 * @param spo2_state 血氧状态指针
 * @return 1=有效, 0=无效
 */
uint8_t SpO2_IsValid(SpO2_State_t *spo2_state) {
    return spo2_state->spo2_valid;
}

/**
 * @brief 重置血氧状态（当信号质量差时）
 * @param spo2_state 血氧状态指针
 */
void SpO2_Reset(SpO2_State_t *spo2_state) {
    spo2_state->r_history_index = 0;
    spo2_state->r_history_count = 0;
    spo2_state->last_spo2 = 0.0f;
    spo2_state->spo2_valid = 0;
}
