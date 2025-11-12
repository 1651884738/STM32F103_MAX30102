//
// Created by Administrator on 2025/11/6.
//

#ifndef SOFT_I2C_H
#define SOFT_I2C_H

#include "../../../Core/Inc/main.h"

/*==================================================================================
 * 1. 用户配置区 (User Configuration)
 *================================================================================*/

// 定义SCL和SDA引脚
#define I2C_SCL_PORT      GPIOA
#define I2C_SCL_PIN       GPIO_PIN_10
#define I2C_SDA_PORT      GPIOA
#define I2C_SDA_PIN       GPIO_PIN_11

// I2C通信速率控制 (单位: 微秒 us)
// 通过调整半周期延时来控制SCL频率。
// 100kHz -> 半周期 5us
// 400kHz -> 半周期 1.25us (可能需要更精确的延时函数)
#define I2C_HALF_PERIOD_DELAY   5  // 对应约100kHz

/*==================================================================================
 * 2. 接口函数声明 (Interface Function Declarations)
 *================================================================================*/

/**
 * @brief 初始化软件I2C所需的GPIO和延时函数
 */
void Soft_I2C_Init(void);

/**
 * @brief 向指定的7位设备地址写入一个字节
 * @param slave_addr: 7位从机地址
 * @param data: 要写入的字节
 * @return 0: 成功, 1: 失败 (无ACK)
 */
uint8_t Soft_I2C_Write_Byte(uint8_t slave_addr, uint8_t data);

/**
 * @brief 从指定的7位设备地址读取一个字节
 * @param slave_addr: 7位从机地址
 * @param p_data: 指向用于存储读取数据的变量的指针
 * @return 0: 成功, 1: 失败 (无ACK)
 */
uint8_t Soft_I2C_Read_Byte(uint8_t slave_addr, uint8_t* p_data);

/**
 * @brief 向I2C设备的指定寄存器写入一个字节
 * @param slave_addr: 7位从机地址
 * @param reg_addr: 寄存器地址
 * @param data: 要写入的数据
 * @return 0: 成功, 1: 失败 (无ACK)
 */
uint8_t Soft_I2C_Write_Reg(uint8_t slave_addr, uint8_t reg_addr, uint8_t data);

/**
 * @brief 从I2C设备的指定寄存器读取一个字节
 * @param slave_addr: 7位从机地址
 * @param reg_addr: 寄存器地址
 * @param p_data: 指向用于存储读取数据的变量的指针
 * @return 0: 成功, 1: 失败 (无ACK)
 */
uint8_t Soft_I2C_Read_Reg(uint8_t slave_addr, uint8_t reg_addr, uint8_t* p_data);

/**
 * @brief 从I2C设备的指定寄存器地址开始，连续读取多个字节
 * @param slave_addr: 7位从机地址
 * @param reg_addr: 起始寄存器地址
 * @param p_data: 指向数据缓冲区的指针
 * @param count: 要读取的字节数
 * @return 0: 成功, 1: 失败 (无ACK)
 */
uint8_t Soft_I2C_Read_Regs(uint8_t slave_addr, uint8_t reg_addr, uint8_t* p_data, uint16_t count);


#endif //SOFT_I2C_H
