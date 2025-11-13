/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "string.h"
#include "../../lib/oled/inc/oled.h"
#include "stdio.h"
#include "stdlib.h"
#include "math.h"
#include "../../lib/oled/inc/max30102.h"
#include "ppg_filter.h"
#include "ppg_algorithm.h"
#include "ppg_algorithm_v2.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/*****************************************************************************
 * 算法方法选择
 * 取消注释其中一行来选择使用的算法：
 * - USE_ALGORITHM_METHOD1: 时域峰值检测算法 (默认)
 *   特点：快速响应(~5秒)，低内存(~2KB)，适合实时监测
 * - USE_ALGORITHM_METHOD2: 频域DPT变换算法
 *   特点：高精度(~10秒)，中等内存(~8KB)，基于ADI论文，抗噪声强
 *****************************************************************************/
#define USE_ALGORITHM_METHOD1       // 方法1: 时域峰值检测 (默认)
// #define USE_ALGORITHM_METHOD2    // 方法2: 频域DPT变换

// 确保只选择了一种方法
#if defined(USE_ALGORITHM_METHOD1) && defined(USE_ALGORITHM_METHOD2)
    #error "Error: Cannot use both methods simultaneously. Please comment out one."
#endif

#if !defined(USE_ALGORITHM_METHOD1) && !defined(USE_ALGORITHM_METHOD2)
    #error "Error: Must select one algorithm method. Please uncomment one method."
#endif

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// 波形显示宏定义
#define WAVE_WIDTH  128       // 波形宽度（屏幕宽度）
#define WAVE_HEIGHT 40        // 波形高度
#define WAVE_Y_OFFSET 24      // 波形Y轴偏移（数值显示下方）
#define WAVE_SAMPLE_INTERVAL 2   // 每2个样本取一个点显示

// 全局中断标志位
volatile uint8_t max30102_interrupt_flag = 0;

// 用于printf重定向到UART
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif
PUTCHAR_PROTOTYPE {
  HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF);
  return ch;
}


/**
  * @brief  外部中断回调函数
  * @param  GPIO_Pin: 触发中断的引脚
  * @retval None
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  // 判断是哪个引脚触发了中断
  // 现在中断线连接在 PB1 上，所以我们需要判断 GPIO_PIN_1
  if (GPIO_Pin == GPIO_PIN_1)
  {
    // 是我们的MAX30102中断，设置标志位
    max30102_interrupt_flag = 1;
  }

  // 如果还有其他外部中断，可以在这里用 else if 继续判断
  // else if (GPIO_Pin == GPIO_PIN_1) { ... }
}


/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_TIM2_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */



  /*******************************oled init********************************************/
  // 刚上电时STM32比OLED启动快，因此需要等待一段时间再初始化OLED
  HAL_Delay(20);
  // 初始化OLED
  OLED_Init();
  // 设置OLED显示模式：正常/反色
  OLED_SetColorMode(OLED_COLOR_NORMAL);
  // 设置OLED显示方向：0°/180°
  OLED_SetOrientation(OLED_Orientation_0);
  // 清空显示缓冲区
  OLED_ClearBuffer();
  // 将缓存内容更新到屏幕显示
  OLED_Refresh();
  /***********************************************************************************/

  printf("OLED Init Ok\r\n");




  /*******************************soft i2c********************************************/
  // !!! 启动TIM2定时器 !!!
  HAL_TIM_Base_Start(&htim2);
  Soft_I2C_Init(); // 初始化软件I2C的GPIO

  /***********************************************************************************/

  printf("MAX30102 Test Program\r\n");


  // 测试通信
  uint8_t part_id = MAX30102_ReadPartID();
  printf("MAX30102 Part ID: 0x%02X\r\n", part_id);
  if (part_id != 0x15) {
    printf("Error: MAX30102 not found!\r\n");
    while(1);
  }

  // 初始化MAX30102
  if (MAX30102_Init() != 0) {
    printf("MAX30102 Init Failed!\r\n");
    while(1);
  } else {
    printf("MAX30102 Init Success!\r\n");
  }

  /* USER CODE END 2 */

  /* USER CODE BEGIN WHILE */

  /**************************************************************************
   * 算法初始化 - 根据宏定义选择不同的算法
   **************************************************************************/

#ifdef USE_ALGORITHM_METHOD1
  printf("\r\n========================================\r\n");
  printf("  Algorithm: Method 1 - Time Domain Peak Detection\r\n");
  printf("  Features: Fast response (~5s), Low memory (~2KB)\r\n");
  printf("========================================\r\n\r\n");

  // 方法1: 时域峰值检测算法
  PPG_FilterState_t red_filter, ir_filter;
  PPG_Filter_Init(&red_filter);
  PPG_Filter_Init(&ir_filter);

  HR_State_t hr_state;
  SpO2_State_t spo2_state;
  HR_Init(&hr_state);
  SpO2_Init(&spo2_state);

  // 方法1特有的显示平滑变量
  float displayed_hr = 0.0f;
  #define DISPLAY_EMA_ALPHA 0.1f
  #define DISPLAY_HR_THRESHOLD 2.0f

#endif

#ifdef USE_ALGORITHM_METHOD2
  printf("\r\n========================================\r\n");
  printf("  Algorithm: Method 2 - DPT Frequency Domain\r\n");
  printf("  Features: High precision (~10s), Based on ADI paper\r\n");
  printf("  Buffer: %d samples (10 seconds)\r\n", DPT_BUFFER_SIZE);
  printf("  Period range: %d-%d samples (%d-%d bpm)\r\n",
         DPT_MIN_PERIOD, DPT_MAX_PERIOD,
         (int)(6000.0f / DPT_MAX_PERIOD), (int)(6000.0f / DPT_MIN_PERIOD));
  printf("========================================\r\n\r\n");

  // 方法2: 频域DPT变换算法
  DPT_State_t dpt_state;
  DPT_Init(&dpt_state);

#endif

  // 原始数据变量
  uint32_t raw_red, raw_ir;

  // 计算相关变量
  uint16_t sample_counter = 0;  // 采样计数器
  float heart_rate = 0.0f;
  float spo2 = 0.0f;
  char display_buf[32];

  // 显示平滑变量（方法2可选）
  float displayed_spo2 = 0.0f;

  // 波形显示相关变量
  float wave_buffer[WAVE_WIDTH]; // 波形缓冲区
  uint8_t wave_index = 0;        // 当前波形索引
  uint8_t wave_sample_counter = 0; // 波形采样计数器
  uint16_t display_counter = 0;     // 波形刷新计数器

  // 初始化波形缓冲区
  for (uint16_t i = 0; i < WAVE_WIDTH; i++) {
      wave_buffer[i] = 0.0f;
  }

  printf("Starting PPG signal processing...\r\n");

  while (1)
  {
      // 读取FIFO数据
      MAX30102_ReadFifo(&raw_red, &raw_ir);

      // 检查信号强度（确保手指放好）
      if (raw_red > 100000 && raw_ir > 100000) {

/**************************************************************************
 * 算法处理 - 根据宏定义选择不同的算法
 **************************************************************************/

#ifdef USE_ALGORITHM_METHOD1
          // ========== 方法1: 时域峰值检测算法 ==========

          // 1. 滤波处理
                     float ac_red = PPG_Filter_Process(&red_filter, raw_red);
                     float ac_ir = PPG_Filter_Process(&ir_filter, raw_ir);

                     // 2. 添加IR信号到心率缓冲区（优化内存使用）
                                float ir_dc = PPG_Filter_GetDC(&ir_filter);
                                HR_AddSample(&hr_state, ac_ir, ir_dc);

          // 2.1 更新波形显示（降采样）
          wave_sample_counter++;
          if (wave_sample_counter >= WAVE_SAMPLE_INTERVAL) {
              wave_sample_counter = 0;
              wave_buffer[wave_index] = ac_ir;
              wave_index = (wave_index + 1) % WAVE_WIDTH;
          }

          // 3. 每250个样本（2.5秒@100Hz）计算一次心率和血氧并更新显示
          sample_counter++;
          if (sample_counter >= 250) {
              sample_counter = 0;

              // 计算心率
              heart_rate = HR_Calculate(&hr_state);

              // 获取AC RMS和DC值
              float red_ac_rms = PPG_Filter_GetACRMS(&red_filter);
              float red_dc = PPG_Filter_GetDC(&red_filter);
              float ir_ac_rms = PPG_Filter_GetACRMS(&ir_filter);
              float ir_dc = PPG_Filter_GetDC(&ir_filter);

              // 计算血氧
              spo2 = SpO2_Calculate(&spo2_state, red_ac_rms, red_dc, ir_ac_rms, ir_dc);

              // === 显示平滑处理 ===
              // 1. 心率显示平滑
              if (HR_IsValid(&hr_state)) {
                  if (displayed_hr == 0.0f) {
                      displayed_hr = heart_rate;
                  } else {
                      float hr_diff = fabsf(heart_rate - displayed_hr);
                      if (hr_diff > DISPLAY_HR_THRESHOLD) {
                          displayed_hr = DISPLAY_EMA_ALPHA * heart_rate +
                                        (1.0f - DISPLAY_EMA_ALPHA) * displayed_hr;
                      }
                  }
              }

              // 2. 血氧显示平滑
              if (SpO2_IsValid(&spo2_state)) {
                  if (displayed_spo2 == 0.0f) {
                      displayed_spo2 = spo2;
                  } else {
                      displayed_spo2 = DISPLAY_EMA_ALPHA * spo2 +
                                      (1.0f - DISPLAY_EMA_ALPHA) * displayed_spo2;
                  }
              }

              // 输出结果
              if (HR_IsValid(&hr_state)) {
                  printf("[Method1] HR: %.1f BPM (Valid)\r\n", heart_rate);
              } else {
                  printf("[Method1] HR: %.1f BPM (Acquiring...)\r\n", heart_rate);
              }

              if (SpO2_IsValid(&spo2_state)) {
                  printf("[Method1] SpO2: %.1f %%\r\n", spo2);
              } else {
                  printf("[Method1] SpO2: --\r\n");
              }
#endif

#ifdef USE_ALGORITHM_METHOD2
          // ========== 方法2: 频域DPT变换算法 ==========

          // 1. DPT处理（内部包含IIR滤波和变换）
          DPT_Process(&dpt_state, raw_red, raw_ir);

          // 2. 更新波形显示（使用IR的AC信号）
          // 注意：DPT内部已经提取了AC信号，这里我们简化处理
          wave_sample_counter++;
          if (wave_sample_counter >= WAVE_SAMPLE_INTERVAL) {
              wave_sample_counter = 0;
              // 使用原始信号的相对变化作为波形（简化）
              static uint32_t last_ir = 0;
              if (last_ir > 0) {
                  wave_buffer[wave_index] = (float)((int32_t)raw_ir - (int32_t)last_ir);
              }
              last_ir = raw_ir;
              wave_index = (wave_index + 1) % WAVE_WIDTH;
          }

          // 3. 每250个样本（2.5秒@100Hz）更新显示
          sample_counter++;
          if (sample_counter >= 250) {
              sample_counter = 0;

              // 获取心率和血氧
              heart_rate = DPT_GetHeartRate(&dpt_state);
              spo2 = DPT_GetSpO2(&dpt_state);

              // 血氧显示平滑
              if (DPT_IsSpO2Valid(&dpt_state)) {
                  if (displayed_spo2 == 0.0f) {
                      displayed_spo2 = spo2;
                  } else {
                      displayed_spo2 = 0.15f * spo2 + 0.85f * displayed_spo2;
                  }
              }

              // 输出结果
              if (DPT_IsHeartRateValid(&dpt_state)) {
                  uint16_t peak_period = DPT_GetPeakPeriod(&dpt_state);
                  printf("[Method2] HR: %.1f BPM | Peak Period: %d samples (Valid)\r\n",
                         heart_rate, peak_period);
              } else {
                  printf("[Method2] HR: %.1f BPM (Acquiring...)\r\n", heart_rate);
              }

              if (DPT_IsSpO2Valid(&dpt_state)) {
                  printf("[Method2] SpO2: %.1f %%\r\n", spo2);
              } else {
                  printf("[Method2] SpO2: --\r\n");
              }
#endif

/**************************************************************************
 * OLED显示更新 - 两种方法共用
 **************************************************************************/

#if defined(USE_ALGORITHM_METHOD1) || defined(USE_ALGORITHM_METHOD2)
              // === OLED显示更新 ===
              OLED_ClearBuffer();

              // 1. 显示数值（顶部一行）
#ifdef USE_ALGORITHM_METHOD1
              if (HR_IsValid(&hr_state) && displayed_hr > 0.0f) {
                  sprintf(display_buf, "HR:%.0f", displayed_hr);
              } else {
                  sprintf(display_buf, "HR:--");
              }
#endif
#ifdef USE_ALGORITHM_METHOD2
              if (DPT_IsHeartRateValid(&dpt_state) && heart_rate > 0.0f) {
                  sprintf(display_buf, "HR:%.0f", heart_rate);
              } else {
                  sprintf(display_buf, "HR:--");
              }
#endif
              OLED_PrintChinese(0, 0, display_buf, 12, OLED_COLOR_NORMAL);

#ifdef USE_ALGORITHM_METHOD1
              if (SpO2_IsValid(&spo2_state) && displayed_spo2 > 0.0f) {
                  sprintf(display_buf, "SpO2:%.0f%%", displayed_spo2);
              } else {
                  sprintf(display_buf, "SpO2:--");
              }
#endif
#ifdef USE_ALGORITHM_METHOD2
              if (DPT_IsSpO2Valid(&dpt_state) && displayed_spo2 > 0.0f) {
                  sprintf(display_buf, "SpO2:%.0f%%", displayed_spo2);
              } else {
                  sprintf(display_buf, "SpO2:--");
              }
#endif
              OLED_PrintChinese(64, 0, display_buf, 12, OLED_COLOR_NORMAL);

              // 2. 绘制波形边框
              OLED_DrawRectangle(0, WAVE_Y_OFFSET - 1, WAVE_WIDTH - 1, WAVE_Y_OFFSET + WAVE_HEIGHT, OLED_COLOR_NORMAL);

              // 3. 找到波形的最大值和最小值（用于归一化）
              float wave_min = wave_buffer[0];
              float wave_max = wave_buffer[0];
              for (uint8_t i = 1; i < WAVE_WIDTH; i++) {
                  if (wave_buffer[i] < wave_min) wave_min = wave_buffer[i];
                  if (wave_buffer[i] > wave_max) wave_max = wave_buffer[i];
              }

              // 4. 绘制波形
              float wave_range = wave_max - wave_min;
              if (wave_range > 1.0f) { // 确保有足够的信号幅度
                  for (uint8_t x = 0; x < WAVE_WIDTH - 1; x++) {
                      // 归一化到波形高度范围
                      uint8_t y1 = WAVE_Y_OFFSET + WAVE_HEIGHT - 1 -
                                   (uint8_t)((wave_buffer[x] - wave_min) / wave_range * (WAVE_HEIGHT - 2));
                      uint8_t y2 = WAVE_Y_OFFSET + WAVE_HEIGHT - 1 -
                                   (uint8_t)((wave_buffer[x + 1] - wave_min) / wave_range * (WAVE_HEIGHT - 2));

                      // 限制Y坐标范围
                      if (y1 < WAVE_Y_OFFSET) y1 = WAVE_Y_OFFSET;
                      if (y1 >= WAVE_Y_OFFSET + WAVE_HEIGHT) y1 = WAVE_Y_OFFSET + WAVE_HEIGHT - 1;
                      if (y2 < WAVE_Y_OFFSET) y2 = WAVE_Y_OFFSET;
                      if (y2 >= WAVE_Y_OFFSET + WAVE_HEIGHT) y2 = WAVE_Y_OFFSET + WAVE_HEIGHT - 1;

                      // 画线连接两个点
                      OLED_DrawLine(x, y1, x + 1, y2, OLED_COLOR_NORMAL);
                  }
              }

              // 5. 在当前采样位置绘制游标（竖线）
              uint8_t cursor_x = wave_index;
              if (cursor_x < WAVE_WIDTH) {
                  OLED_DrawLine(cursor_x, WAVE_Y_OFFSET, cursor_x, WAVE_Y_OFFSET + WAVE_HEIGHT - 1, OLED_COLOR_REVERSED);
              }

              OLED_Refresh();
          }
#endif  // End of OLED display update

      } else {
          // 信号太弱，提示用户
          if (sample_counter == 0) {  // 避免频繁打印
              printf("Signal weak - Please place finger properly\r\n");

              // 显示提示信息
              OLED_ClearBuffer();
              OLED_PrintChinese(10, 20, "Please place", 12, OLED_COLOR_NORMAL);
              OLED_PrintChinese(10, 36, "finger on", 12, OLED_COLOR_NORMAL);
              OLED_PrintChinese(10, 52, "sensor", 12, OLED_COLOR_NORMAL);
              OLED_Refresh();
          }
          sample_counter = (sample_counter + 1) % 100;  // 每秒显示一次
      }


    // if (max30102_interrupt_flag == 1) {
      // max30102_interrupt_flag = 0;
      // printf("MAX30102 max30102_interrupt_flag is %d\r\n",max30102_interrupt_flag);
      // uint8_t status1, status2;
      // MAX30102_ReadInterruptStatus(&status1, &status2);
      // printf("MAX30102 Interrupt Status1: 0x%02X\r\n", status1);
      // printf("MAX30102 Interrupt Status2: 0x%02X\r\n", status2);

      // 检查是否是PPG_RDY或A_FULL中断
      // if (status1 & 0xC0) {
        // 循环读取FIFO中的所有样本
        // 这里简单处理，假设每次中断只读一次，实际应用中应根据FIFO指针计算可读样本数
        // MAX30102_ReadFifo(&red_val, &ir_val); /*包含了大量的GPIO操作和delay_us()延时,可能需要几百微秒*/

        // 将数据通过串口发送给MATLAB
        // 格式: "R,value1,I,value2\n"
        // printf("R,%lu,I,%lu\n", red_val, ir_val);
      // }

    // }



    // 这里可以加一个小的延时，避免过于频繁地查询I2C总线
    // HAL_Delay(1);



    // HAL_UART_Transmit(&huart2, (uint8_t*)"123456", strlen("123456"), 100);
    // HAL_UART_Transmit(&huart2, (uint8_t*)"\r\n", strlen("\r\n"), 100);
    // HAL_Delay(5000);

    // // 中英文字符串混合显示
    // OLED_ClearBuffer();
    // OLED_PrintChinese(0, 0, "THANKES", 16, OLED_COLOR_REVERSED);
    // OLED_Refresh();
    // HAL_Delay(500);
    // OLED_PrintChinese(0, 22, "You look cool", 16, OLED_COLOR_NORMAL);
    // OLED_Refresh();
    // HAL_Delay(500);
    // OLED_PrintChinese(0, 44, "\\^o^/", 16, OLED_COLOR_NORMAL);
    // OLED_Refresh();
    // HAL_Delay(1500);
    // // 显示变量值
    // int count = 0;
    // char buf[10] = {0};
    // OLED_ClearBuffer();
    // for(;;) {
    //   sprintf(buf, "%d %%", count);
    //   OLED_PrintChinese(40, 20, buf, 24, OLED_COLOR_NORMAL);
    //   OLED_Refresh();
    //   HAL_Delay(15);
    //   if(count++ > 99){
    //     break;
    //   }
    // }
    // HAL_Delay(1000);
    // // 直线绘制
    // OLED_ClearBuffer();
    // for (int i = 0; i < 128; i+=8) {
    //   OLED_DrawLine(0, 0, i, 63, OLED_COLOR_NORMAL);
    //   OLED_DrawLine(127, 0, 127 - i, 63, OLED_COLOR_NORMAL);
    //   OLED_Refresh();
    //   HAL_Delay(30);
    // }
    // HAL_Delay(1500);
    // // 矩形绘制
    // OLED_ClearBuffer();
    // for (int i = 0; i < 64; i+=8) {
    //   OLED_DrawRectangle(i, i/2, 127 - i, 63 - i/2, OLED_COLOR_NORMAL);
    //   OLED_Refresh();
    //   HAL_Delay(35);
    // }
    // HAL_Delay(1500);
    // // 矩形圆形
    // OLED_ClearBuffer();
    // for (int i = 63; i > 0; i-=8) {
    //   OLED_DrawCircle(64, 32, i/2, CIRCLE_ALL, OLED_COLOR_NORMAL);
    //   OLED_Refresh();
    //   HAL_Delay(40);
    // }
    // HAL_Delay(1500);
    // // 图片显示1
    // OLED_ClearBuffer();
    // OLED_DrawPicture(40, 7, 48, 48, icon_IDE, OLED_COLOR_NORMAL);
    // OLED_Refresh();
    // HAL_Delay(1700);
    // // 图片显示2
    // OLED_ClearBuffer();
    // OLED_DrawPicture(33, 2, 61, 58, icon_Bili, OLED_COLOR_NORMAL);
    // OLED_Refresh();
    // HAL_Delay(1700);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
