//
// Created by Administrator on 2025/11/6.
//

#ifndef MAX30102_H
#define MAX30102_H

#include "main.h"
#include "soft_i2c.h"

// MAX30102 I2C 7位地址
#define MAX30102_I2C_ADDR   0x57


// 寄存器地址定义
#define REG_INTR_STATUS_1   0x00
#define REG_INTR_STATUS_2   0x01
#define REG_INTR_ENABLE_1   0x02
#define REG_INTR_ENABLE_2   0x03
#define REG_FIFO_WR_PTR     0x04
#define REG_OVF_COUNTER     0x05
#define REG_FIFO_RD_PTR     0x06
#define REG_FIFO_DATA       0x07
#define REG_FIFO_CONFIG     0x08
#define REG_MODE_CONFIG     0x09
#define REG_SPO2_CONFIG     0x0A
#define REG_LED1_PA         0x0C // Red
#define REG_LED2_PA         0x0D // IR
#define REG_PART_ID         0xFF

// 函数声明
uint8_t MAX30102_Init(void);
uint8_t MAX30102_Reset(void);
uint8_t MAX30102_ReadPartID(void);
void MAX30102_ReadFifo(uint32_t *pun_red_led, uint32_t *pun_ir_led);
void MAX30102_ReadInterruptStatus(uint8_t *status1, uint8_t *status2);





#endif //MAX30102_H
