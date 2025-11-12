//
// Created by Administrator on 2025/11/6.
//

#include "../inc/max30102.h"




/**
 * @brief 软复位MAX30102
 * @return 0: 成功, 1: 失败
 */
uint8_t MAX30102_Reset(void) {
    // 设置RESET位 (Bit 6)
    if (Soft_I2C_Write_Reg(MAX30102_I2C_ADDR, REG_MODE_CONFIG, 0x40) != 0) {
        return 1; // I2C写入失败
    }

    // 等待RESET位自动清零
    uint8_t reg_val;
    uint32_t timeout = 100; // 设置超时
    do {
        HAL_Delay(1); // 延时1ms
        if (Soft_I2C_Read_Reg(MAX30102_I2C_ADDR, REG_MODE_CONFIG, &reg_val) != 0) {
            return 1; // I2C读取失败
        }
        if (timeout-- == 0) return 1; // 超时
    } while (reg_val & 0x40);

    return 0;
}

/**
 * @brief 读取Part ID (用于测试通信是否正常)
 * @return Part ID (正常应为0x15) 或 0xFF (读取失败)
 */
uint8_t MAX30102_ReadPartID(void) {
    uint8_t part_id;
    if (Soft_I2C_Read_Reg(MAX30102_I2C_ADDR, REG_PART_ID, &part_id) == 0) {
        return part_id;
    }
    return 0xFF;
}

/**
 * @brief 读取中断状态寄存器
 */
void MAX30102_ReadInterruptStatus(uint8_t *status1, uint8_t *status2) {
    Soft_I2C_Read_Reg(MAX30102_I2C_ADDR, REG_INTR_STATUS_1, status1);
    Soft_I2C_Read_Reg(MAX30102_I2C_ADDR, REG_INTR_STATUS_2, status2);
}

/**
 * @brief 初始化MAX30102并进行配置
 * @return 0: 成功, 1: 失败
 */
uint8_t MAX30102_Init(void) {
    if (MAX30102_Reset() != 0) {
        return 1;
    }

    // 1. 中断配置：使能 A_FULL_EN (FIFO Almost Full) 和 PPG_RDY_EN
    Soft_I2C_Write_Reg(MAX30102_I2C_ADDR, REG_INTR_ENABLE_1, 0xC0);
    Soft_I2C_Write_Reg(MAX30102_I2C_ADDR, REG_INTR_ENABLE_2, 0x00);

    // 2. FIFO配置
    // 清空FIFO读写指针和溢出计数器
    Soft_I2C_Write_Reg(MAX30102_I2C_ADDR, REG_FIFO_WR_PTR, 0x00);
    Soft_I2C_Write_Reg(MAX30102_I2C_ADDR, REG_OVF_COUNTER, 0x00);
    Soft_I2C_Write_Reg(MAX30102_I2C_ADDR, REG_FIFO_RD_PTR, 0x00);

    // FIFO配置: 采样平均=不平均, FIFO不翻转, FIFO中断触发阈值=剩余15个样本
    Soft_I2C_Write_Reg(MAX30102_I2C_ADDR, REG_FIFO_CONFIG, 0x0F); // SMP_AVE=1, FIFO_ROLLOVER_EN=0, FIFO_A_FULL=15

    // 3. 模式配置: SpO2 模式
    Soft_I2C_Write_Reg(MAX30102_I2C_ADDR, REG_MODE_CONFIG, 0x03);

    // 4. SpO2 ADC 配置
    // SpO2 ADC Range=16384nA, Sample Rate=100Hz, Pulse Width=411us (18-bit)
    Soft_I2C_Write_Reg(MAX30102_I2C_ADDR, REG_SPO2_CONFIG, 0x27); // SPO2_ADC_RGE=0, SPO2_SR=1, LED_PW=3

    // 5. LED电流配置 (0x24 约等于 7.6mA, 这是一个比较安全的起始值)
    Soft_I2C_Write_Reg(MAX30102_I2C_ADDR, REG_LED1_PA, 0x24); // Red LED
    Soft_I2C_Write_Reg(MAX30102_I2C_ADDR, REG_LED2_PA, 0x24); // IR LED

    return 0;
}

/**
 * @brief 从FIFO中读取一组Red和IR数据 (3+3=6字节)
 * @param pun_red_led: 存储Red ADC值的指针
 * @param pun_ir_led: 存储IR ADC值的指针
 */
void MAX30102_ReadFifo(uint32_t *pun_red_led, uint32_t *pun_ir_led) {
    uint8_t buffer[6];

    // 使用多字节连续读取函数
    Soft_I2C_Read_Regs(MAX30102_I2C_ADDR, REG_FIFO_DATA, buffer, 6);

    // 组合3个字节为一个32位数据 (数据是18位左对齐的)
    *pun_red_led = ((uint32_t)buffer[0] << 16) | ((uint32_t)buffer[1] << 8) | buffer[2];
    *pun_ir_led  = ((uint32_t)buffer[3] << 16) | ((uint32_t)buffer[4] << 8) | buffer[5];

    // 只取有效的18位
    *pun_red_led &= 0x03FFFF;
    *pun_ir_led  &= 0x03FFFF;
}