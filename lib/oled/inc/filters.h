//
// Created by Administrator on 2025/11/8.
//

#ifndef FILTERS_H
#define FILTERS_H

#include "main.h"
// #include "arm_math.h"


// --- 去趋势滤波器定义 ---
typedef struct {
    float last_y; // 上一次的基线估算值
} DetrendFilter;

// --- 带通滤波器定义 ---
#define NUM_SECTIONS 4 // 我们有4个二阶节

typedef struct {
    // 每个二阶节需要2个状态变量(w[n-1], w[n-2])
    // 所以4个节总共需要 4 * 2 = 8 个状态变量
    float state[NUM_SECTIONS * 2];
} BiquadCascadeFilter;

// --- 整体流水线 ---
typedef struct {
    DetrendFilter detrend;
    BiquadCascadeFilter bandpass;
} PPG_Pipeline;

// --- 函数声明 ---
void ppg_pipeline_init(PPG_Pipeline* pipeline, float initial_value);
float ppg_pipeline_process(PPG_Pipeline* pipeline, float raw_sample);



#endif //FILTERS_H
