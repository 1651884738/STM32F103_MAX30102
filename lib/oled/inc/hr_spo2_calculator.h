//
// Created by Administrator on 2025/11/10.
//

#ifndef HR_SPO2_CALCULATOR_H
#define HR_SPO2_CALCULATOR_H
#include <stdint.h>


void filter_ppg_signal(int32_t RD_in, int32_t IR_in,
                         int32_t *pRD_ac, int32_t *pRD_dc,
                         int32_t *pIR_ac, int32_t *pIR_dc);

#endif //HR_SPO2_CALCULATOR_H
