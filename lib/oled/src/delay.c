//
// Created by Administrator on 2025/11/6.
//

#include "../inc/delay.h"


// 外部声明定时器句柄，它在 main.c 中定义
extern TIM_HandleTypeDef htim2;

/**
  * @brief  精确的微秒级延时函数 (基于TIM2)
  * @param  us: 要延时的微秒数 (最大值 65535)
  * @retval None
  */
void delay_us(uint16_t us)
{
    // 1. 将定时器的计数器清零
    __HAL_TIM_SET_COUNTER(&htim2, 0);

    // 2. 等待，直到计数器的值达到或超过期望的延时微秒数
    //    使用 while 循环进行忙等待 (blocking delay)
    while (__HAL_TIM_GET_COUNTER(&htim2) < us);
}