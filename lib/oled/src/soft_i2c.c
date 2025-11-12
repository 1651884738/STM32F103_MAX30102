//
// Created by Administrator on 2025/11/6.
//

#include "../inc/soft_i2c.h"
#include "../inc/delay.h"


/*==================================================================================
 * 1. 底层GPIO与延时 (Low-level GPIO and Delay)
 *================================================================================*/

// 宏定义简化GPIO操作
#define I2C_SCL_SET()     HAL_GPIO_WritePin(I2C_SCL_PORT, I2C_SCL_PIN, GPIO_PIN_SET)
#define I2C_SCL_CLR()     HAL_GPIO_WritePin(I2C_SCL_PORT, I2C_SCL_PIN, GPIO_PIN_RESET)

#define I2C_SDA_SET()     HAL_GPIO_WritePin(I2C_SDA_PORT, I2C_SDA_PIN, GPIO_PIN_SET)
#define I2C_SDA_CLR()     HAL_GPIO_WritePin(I2C_SDA_PORT, I2C_SDA_PIN, GPIO_PIN_RESET)

#define I2C_SDA_READ()    HAL_GPIO_ReadPin(I2C_SDA_PORT, I2C_SDA_PIN)

// 更严谨的做法：SDA输入/输出模式切换
// private static inline void SDA_Mode_Out(void) {
//     GPIO_InitTypeDef GPIO_InitStruct = {0};
//     GPIO_InitStruct.Pin = I2C_SDA_PIN;
//     GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
//     GPIO_InitStruct.Pull = GPIO_NOPULL;
//     GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
//     HAL_GPIO_Init(I2C_SDA_PORT, &GPIO_InitStruct);
// }
// private static inline void SDA_Mode_In(void) {
//     GPIO_InitTypeDef GPIO_InitStruct = {0};
//     GPIO_InitStruct.Pin = I2C_SDA_PIN;
//     GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
//     GPIO_InitStruct.Pull = GPIO_NOPULL; // 外部已有上拉
//     HAL_GPIO_Init(I2C_SDA_PORT, &GPIO_InitStruct);
// }


/*==================================================================================
 * 2. I2C协议时序层 (I2C Protocol Timing Layer) - 静态私有函数
 *================================================================================*/

// START信号
static void i2c_start(void) {
    I2C_SDA_SET();
    I2C_SCL_SET();
    delay_us(I2C_HALF_PERIOD_DELAY);
    I2C_SDA_CLR();
    delay_us(I2C_HALF_PERIOD_DELAY);
    I2C_SCL_CLR();
}

// STOP信号
static void i2c_stop(void) {
    I2C_SCL_CLR();
    I2C_SDA_CLR();
    delay_us(I2C_HALF_PERIOD_DELAY);
    I2C_SCL_SET();
    delay_us(I2C_HALF_PERIOD_DELAY);
    I2C_SDA_SET();
}

// 发送一个字节
static void i2c_write_byte(uint8_t byte) {
    uint8_t i;
    for (i = 0; i < 8; i++) {
        if (byte & 0x80) {
            I2C_SDA_SET();
        } else {
            I2C_SDA_CLR();
        }
        delay_us(I2C_HALF_PERIOD_DELAY);
        I2C_SCL_SET();
        delay_us(I2C_HALF_PERIOD_DELAY);
        I2C_SCL_CLR();
        byte <<= 1;
    }
}

// 读取一个字节
static uint8_t i2c_read_byte(void) {
    uint8_t i, byte = 0;
    I2C_SDA_SET(); // 释放SDA总线，由从机驱动
    for (i = 0; i < 8; i++) {
        byte <<= 1;
        I2C_SCL_SET();
        delay_us(I2C_HALF_PERIOD_DELAY);
        if (I2C_SDA_READ() == GPIO_PIN_SET) {
            byte |= 0x01;
        }
        I2C_SCL_CLR();
        delay_us(I2C_HALF_PERIOD_DELAY);
    }
    return byte;
}

// 等待ACK
// 返回 0: 收到ACK, 1: 收到NACK
static uint8_t i2c_wait_ack(void) {
    uint8_t ack_status;
    I2C_SDA_SET(); // 释放SDA
    delay_us(I2C_HALF_PERIOD_DELAY);
    I2C_SCL_SET();
    delay_us(I2C_HALF_PERIOD_DELAY);

    // 如果使用SDA模式切换，这里应先切换到输入模式
    // SDA_Mode_In();
    ack_status = I2C_SDA_READ();
    // SDA_Mode_Out();

    I2C_SCL_CLR();
    return ack_status;
}

// 发送ACK
static void i2c_send_ack(void) {
    I2C_SDA_CLR();
    delay_us(I2C_HALF_PERIOD_DELAY);
    I2C_SCL_SET();
    delay_us(I2C_HALF_PERIOD_DELAY);
    I2C_SCL_CLR();
    I2C_SDA_SET(); // 释放总线
}

// 发送NACK
static void i2c_send_nack(void) {
    I2C_SDA_SET();
    delay_us(I2C_HALF_PERIOD_DELAY);
    I2C_SCL_SET();
    delay_us(I2C_HALF_PERIOD_DELAY);
    I2C_SCL_CLR();
}




/*==================================================================================
 * 3. I2C通信接口层 (I2C Communication Interface Layer) - 公有函数
 *================================================================================*/

void Soft_I2C_Init(void) {
    // GPIO已由CubeMX初始化，这里可以确保总线处于空闲状态
    I2C_SCL_SET();
    I2C_SDA_SET();
}

uint8_t Soft_I2C_Write_Reg(uint8_t slave_addr, uint8_t reg_addr, uint8_t data) {
    uint8_t status = 0;

    __disable_irq(); // 关键操作期间屏蔽中断，保证时序完整

    i2c_start();

    // 1. 发送设备写地址
    i2c_write_byte(slave_addr << 1 | 0x00); // 地址左移一位，最后一位为0表示写
    if (i2c_wait_ack() != 0) {
        status = 1; // 设备地址无响应
        goto write_fail;
    }

    // 2. 发送寄存器地址
    i2c_write_byte(reg_addr);
    if (i2c_wait_ack() != 0) {
        status = 1; // 寄存器地址无响应
        goto write_fail;
    }

    // 3. 发送数据
    i2c_write_byte(data);
    if (i2c_wait_ack() != 0) {
        status = 1; // 数据无响应
        goto write_fail;
    }

write_fail:
    i2c_stop();
    __enable_irq(); // 恢复中断
    return status;
}

uint8_t Soft_I2C_Read_Reg(uint8_t slave_addr, uint8_t reg_addr, uint8_t* p_data) {
    uint8_t status = 0;

    __disable_irq();

    i2c_start();

    // 1. 发送设备写地址，告诉它我们要访问哪个寄存器
    i2c_write_byte(slave_addr << 1 | 0x00);
    if (i2c_wait_ack() != 0) {
        status = 1;
        goto read_fail;
    }
    i2c_write_byte(reg_addr);
    if (i2c_wait_ack() != 0) {
        status = 1;
        goto read_fail;
    }

    // 2. 发送Repeated Start和设备读地址
    i2c_start();
    i2c_write_byte(slave_addr << 1 | 0x01); // 最后一位为1表示读
    if (i2c_wait_ack() != 0) {
        status = 1;
        goto read_fail;
    }

    // 3. 读取数据
    *p_data = i2c_read_byte();

    // 4. 发送NACK (因为只读一个字节)
    i2c_send_nack();

read_fail:
    i2c_stop();
    __enable_irq();
    return status;
}


uint8_t Soft_I2C_Read_Regs(uint8_t slave_addr, uint8_t reg_addr, uint8_t* p_data, uint16_t count) {
    uint8_t status = 0;
    uint16_t i;

    if (count == 0) return 0;

    __disable_irq();

    i2c_start();

    // 1. 发送设备写地址和起始寄存器地址
    i2c_write_byte(slave_addr << 1 | 0x00);
    if (i2c_wait_ack() != 0) { status = 1; goto read_multi_fail; }

    i2c_write_byte(reg_addr);
    if (i2c_wait_ack() != 0) { status = 1; goto read_multi_fail; }

    // 2. 发送Repeated Start和设备读地址
    i2c_start();
    i2c_write_byte(slave_addr << 1 | 0x01);
    if (i2c_wait_ack() != 0) { status = 1; goto read_multi_fail; }

    // 3. 连续读取数据
    for (i = 0; i < count; i++) {
        p_data[i] = i2c_read_byte();
        if (i == count - 1) {
            // 读取最后一个字节后，发送NACK
            i2c_send_nack();
        } else {
            // 读取非最后一个字节后，发送ACK
            i2c_send_ack();
        }
    }

read_multi_fail:
    i2c_stop();
    __enable_irq();
    return status;
}


/**
 * @brief 向I2C总线上的从机写入一个字节 (不带寄存器地址)
 * @param slave_addr: 7位从机地址 (注意: 这里传入的是7位地址，函数内部会处理读写位)
 * @param data: 要写入的字节
 * @return 0: 成功 (收到ACK), 1: 失败 (无ACK)
 */
uint8_t Soft_I2C_Write_Byte(uint8_t slave_addr, uint8_t data) {
    uint8_t status = 0;

    // 确保slave_addr是7位，并将读写位设置为写(0)
    uint8_t device_write_addr = (slave_addr << 1) | 0x00;

    __disable_irq(); // 屏蔽中断，保证时序的完整性

    i2c_start();

    // 1. 发送设备地址 + 写标志
    i2c_write_byte(device_write_addr);
    if (i2c_wait_ack() != 0) {
        status = 1; // 从机没有响应ACK
        goto write_byte_fail;
    }

    // 2. 发送数据字节
    i2c_write_byte(data);
    if (i2c_wait_ack() != 0) {
        status = 1; // 从机没有响应ACK
    }

    write_byte_fail:
        i2c_stop();
    __enable_irq(); // 恢复中断
    return status;
}

/**
 * @brief 从I2C总线上的从机读取一个字节 (不带寄存器地址)
 * @param slave_addr: 7位从机地址 (注意: 这里传入的是7位地址，函数内部会处理读写位)
 * @param p_data: 指向用于存储读取数据的变量的指针
 * @return 0: 成功, 1: 失败 (无ACK)
 */
uint8_t Soft_I2C_Read_Byte(uint8_t slave_addr, uint8_t* p_data) {
    uint8_t status = 0;

    // 确保slave_addr是7位，并将读写位设置为读(1)
    uint8_t device_read_addr = (slave_addr << 1) | 0x01;

    __disable_irq();

    i2c_start();

    // 1. 发送设备地址 + 读标志
    i2c_write_byte(device_read_addr);
    if (i2c_wait_ack() != 0) {
        status = 1; // 从机没有响应ACK
        goto read_byte_fail;
    }

    // 2. 读取一个数据字节
    *p_data = i2c_read_byte();

    // 3. 发送NACK，告诉从机我们已经收到了最后一个字节
    i2c_send_nack();

    read_byte_fail:
        i2c_stop();
    __enable_irq();
    return status;
}
