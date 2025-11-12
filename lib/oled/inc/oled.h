#ifndef __OLED_H__
#define __OLED_H__

#include "../../../Core/Inc/main.h"
#include "../../../Core/Inc/i2c.h"
#include "string.h"
#include "font.h"

#define OLED_CMD  0	//写命令
#define OLED_DATA 1	//写数据
#define OLED_ADDRESS 0x7A //OLED器件地址

typedef enum {
	OLED_TRANSMIT_CMD = 0,
	OLED_TRANSMIT_DATA
} OLED_TransmitMode;

typedef enum {
	OLED_COLOR_NORMAL = 0,
	OLED_COLOR_REVERSED
} OLED_ColorMode;

typedef enum {
	OLED_Orientation_0 = 0,
	OLED_Orientation_180
} OLED_Orientation;

typedef enum {
	OLED_SUCCESS = 0,
	OLED_FAILED
} OLED_Ret;

typedef enum {
	CIRCLE_UPPER_RIGHT = 0x01,
	CIRCLE_UPPER_LEFT = 0x02,
	CIRCLE_LOWER_LEFT = 0x04,
	CIRCLE_LOWER_RIGHT = 0x08,
	CIRCLE_ALL = 0x0F
} OLED_DrawCircleOption;

// 发送一个字节
void OLED_TransmitByte(uint8_t data, OLED_TransmitMode mode);

// 设置颜色反转
void OLED_SetColorMode(OLED_ColorMode mode);

// 设置显示方向
void OLED_SetOrientation(OLED_Orientation mode);

// 开启OLED显示
void OLED_DisPlay_On(void);

// 关闭OLED显示
void OLED_DisPlay_Off(void);

// 将缓存内容更新到屏幕显示
void OLED_Refresh(void);

// 缓存清屏
void OLED_ClearBuffer(void);

// 初始化OLED
void OLED_Init();

// 画一个像素点
void OLED_DrawPixel(uint8_t x, uint8_t y, OLED_ColorMode mode);

// 画一条直线
void OLED_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, OLED_ColorMode mode);

// 画一个填充的矩形
void OLED_DrawRectangleFilled(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, OLED_ColorMode mode);

// 画一个空心的矩形
void OLED_DrawRectangle(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, OLED_ColorMode mode);

// 空心圆辅助函数
void _OLED_DrawCircleSection(uint8_t x, uint8_t y, uint8_t x0, uint8_t y0, uint8_t option, OLED_ColorMode mode);

// 实心圆辅助函数
void _OLED_DrawCircleSectionFilled(uint8_t x, uint8_t y, uint8_t x0, uint8_t y0, OLED_ColorMode mode);

// 画一个空心圆形
void OLED_DrawCircle(uint8_t x0, uint8_t y0, uint8_t rad, uint8_t option, OLED_ColorMode mode);

// 画一个实心圆形
void OLED_DrawCircleFilled(uint8_t x0, uint8_t y0, uint8_t rad, OLED_ColorMode mode);

// 显示ASCII字符
void OLED_PrintChar(uint8_t x, uint8_t y, char _char, uint8_t size1, uint8_t mode);

// 显示字符串
void OLED_PrintString(uint8_t x, uint8_t y, char *_string, uint8_t size1, uint8_t mode);

// 显示汉字
OLED_Ret OLED_PrintChineseChar(uint8_t x, uint8_t y, char c[2], uint8_t font, OLED_ColorMode mode);

// 显示中/英文混合字符串
void OLED_PrintChinese(uint8_t x, uint8_t y, char *s, uint8_t font, OLED_ColorMode mode);

// 显示图像
void OLED_DrawPicture(uint8_t x, uint8_t y, uint8_t size_x, uint8_t size_y, const unsigned char pic[], OLED_ColorMode mode);

#endif
