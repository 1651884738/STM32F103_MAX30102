#include "../inc/oled.h"

uint8_t OLED_GRAM[129][8];

/**
 * @brief 向OLED发送一个字节
*/
void OLED_TransmitByte(uint8_t data, OLED_TransmitMode mode) {
	uint8_t send_buf[2] = { 0 };
	if (mode)
		send_buf[0] = 0x40;
	else
		send_buf[0] = 0x00;
	send_buf[1] = data;
	HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDRESS, send_buf, 2, 1);
}

/**
 * @brief 初始化OLED屏幕
*/
void OLED_Init(void) {
	OLED_TransmitByte(0xAE, OLED_TRANSMIT_CMD); /*display off*/
	OLED_TransmitByte(0x02, OLED_TRANSMIT_CMD); /*set lower column address*/
	OLED_TransmitByte(0x10, OLED_TRANSMIT_CMD); /*set higher column address*/
	OLED_TransmitByte(0x40, OLED_TRANSMIT_CMD); /*set display start line*/
	OLED_TransmitByte(0xB0, OLED_TRANSMIT_CMD); /*set page address*/

	OLED_TransmitByte(0x81, OLED_TRANSMIT_CMD); /*contract control*/
	OLED_TransmitByte(0xcf, OLED_TRANSMIT_CMD); /*128*/

	OLED_TransmitByte(0xA1, OLED_TRANSMIT_CMD); /*set segment remap*/
	OLED_TransmitByte(0xA6, OLED_TRANSMIT_CMD); /*normal / reverse*/
	OLED_TransmitByte(0xA8, OLED_TRANSMIT_CMD); /*multiplex ratio*/
	OLED_TransmitByte(0x3F, OLED_TRANSMIT_CMD); /*duty = 1/64*/
	OLED_TransmitByte(0xad, OLED_TRANSMIT_CMD); /*set charge pump enable*/
	OLED_TransmitByte(0x8b, OLED_TRANSMIT_CMD); /* 0x8B 内供 VCC */
	OLED_TransmitByte(0x33, OLED_TRANSMIT_CMD); /*0X30---0X33 set VPP 9V */
	OLED_TransmitByte(0xC8, OLED_TRANSMIT_CMD); /*Com scan direction*/
	OLED_TransmitByte(0xD3, OLED_TRANSMIT_CMD); /*set display offset*/
	OLED_TransmitByte(0x00, OLED_TRANSMIT_CMD); /* 0x20 */

	OLED_TransmitByte(0xD5, OLED_TRANSMIT_CMD); /*set osc division*/
	OLED_TransmitByte(0x80, OLED_TRANSMIT_CMD);

	OLED_TransmitByte(0xD9, OLED_TRANSMIT_CMD); /*set pre-charge period*/
	OLED_TransmitByte(0x1f, OLED_TRANSMIT_CMD); /*0x22*/

	OLED_TransmitByte(0xDA, OLED_TRANSMIT_CMD); /*set COM pins*/
	OLED_TransmitByte(0x12, OLED_TRANSMIT_CMD);
	
	OLED_TransmitByte(0xdb, OLED_TRANSMIT_CMD); /*set vcomh*/
	OLED_TransmitByte(0x40, OLED_TRANSMIT_CMD);

	OLED_ClearBuffer();
	OLED_Refresh();
	OLED_TransmitByte(0xAF, OLED_TRANSMIT_CMD); /*display ON*/
}


/**
 * @brief 设置颜色模式 黑底白字或白底黑字
 * 
 * @param ColorMode 颜色模式COLOR_NORMAL/COLOR_REVERSED
 * 
*/
void OLED_SetColorMode(OLED_ColorMode mode) {
	if (mode == OLED_COLOR_NORMAL) {
		OLED_TransmitByte(0xA6, OLED_TRANSMIT_CMD); //正常显示
	}
	if (mode == OLED_COLOR_REVERSED) {
		OLED_TransmitByte(0xA7, OLED_TRANSMIT_CMD); //反色显示
	}
}

/**
 * @brief 设置显示方向 0°或180°
 * 
 * @param Orientation 显示方向
 * 
*/
void OLED_SetOrientation(OLED_Orientation mode) {
	if (mode == OLED_Orientation_0) {
		OLED_TransmitByte(0xC8, OLED_TRANSMIT_CMD); //正常显示
		OLED_TransmitByte(0xA1, OLED_TRANSMIT_CMD);
	}
	if (mode == OLED_Orientation_180) {
		OLED_TransmitByte(0xC0, OLED_TRANSMIT_CMD); //反转显示
		OLED_TransmitByte(0xA0, OLED_TRANSMIT_CMD);
	}
}

/**
 * @brief 开启OLED显示
*/
void OLED_DisPlay_On(void) {
	OLED_TransmitByte(0x8D, OLED_TRANSMIT_CMD); //电荷泵使能
	OLED_TransmitByte(0x14, OLED_TRANSMIT_CMD); //开启电荷泵
	OLED_TransmitByte(0xAF, OLED_TRANSMIT_CMD); //点亮屏幕
}

/**
 * @brief 关闭OLED显示
*/
void OLED_DisPlay_Off(void) {
	OLED_TransmitByte(0x8D, OLED_TRANSMIT_CMD); //电荷泵使能
	OLED_TransmitByte(0x10, OLED_TRANSMIT_CMD); //关闭电荷泵
	OLED_TransmitByte(0xAE, OLED_TRANSMIT_CMD); //关闭屏幕
}

/**
 * @brief 将缓存内容更新到屏幕显示
*/
void OLED_Refresh(void) {
	uint8_t i, n;
	uint8_t send_buf[129] = { 0 };
	for (i = 0; i < 8; i++) {
		OLED_TransmitByte(0xb0 + i, OLED_TRANSMIT_CMD); //设置行起始地址
		OLED_TransmitByte(0x02, OLED_TRANSMIT_CMD);   //设置低列起始地址
		OLED_TransmitByte(0x10, OLED_TRANSMIT_CMD);   //设置高列起始地址
		send_buf[0] = 0x40;
		for (n = 0; n < 128; n++) {
			send_buf[n + 1] = OLED_GRAM[n][i];
		}
		HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDRESS, send_buf, 129, 20);
	}
}

/**
 * @brief 缓存清屏
*/
void OLED_ClearBuffer(void) {
	uint8_t i, n;
	for (i = 0; i < 8; i++) {
		for (n = 0; n < 128; n++) {
			OLED_GRAM[n][i] = 0;   //清除所有数据
		}
	}
//	OLED_Refresh();   //更新显示
}

/**
 * @brief 画一个像素点
 * 
 * @param x X坐标 [0,127]
 * @param y Y坐标 [0,63]
 * @param mode 颜色模式
*/
void OLED_DrawPixel(uint8_t x, uint8_t y, OLED_ColorMode mode) {
	uint8_t i, m, n;
	i = y / 8;
	m = y % 8;
	n = 1 << m;
	if (!mode)
		OLED_GRAM[x][i] |= n;
	else {
		OLED_GRAM[x][i] = ~OLED_GRAM[x][i];
		OLED_GRAM[x][i] |= n;
		OLED_GRAM[x][i] = ~OLED_GRAM[x][i];
	}
}

/**
 * @brief 画一条直线
 * 
 * @param x0 起点X坐标 [0,127]
 * @param y0 起点Y坐标 [0,63]
 * @param x1 终点X坐标 [0,127]
 * @param y1 终点Y坐标 [0,63]
 * @param mode 颜色模式
*/
void OLED_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, OLED_ColorMode mode) {
	int xerr = 0, yerr = 0, delta_x, delta_y, distance;
	int incx, incy, uRow, uCol;
	delta_x = x1 - x0; //计算坐标增量
	delta_y = y1 - y0;
	uRow = x0;//画线起点坐标
	uCol = y0;
	if (delta_x > 0) {
		incx = 1;
	} 
	else if (delta_x == 0) {
		incx = 0;
	}
	else {
		incx = -1;
		delta_x = -delta_x;
	}
	if (delta_y > 0) {
		incy = 1;
	}
	else if (delta_y == 0) {
		incy = 0; 
	}
	else {
		incy = -1;
		delta_y = -delta_y;
	}
	if (delta_x > delta_y) {
		distance = delta_x;
	} 
	else {
		distance = delta_y;
	}
	for (int t = 0; t <= distance + 1; t++) {
		OLED_DrawPixel(uRow, uCol, mode);
		xerr += delta_x;
		yerr += delta_y;
		if (xerr > distance) {
			xerr -= distance;
			uRow += incx;
		}
		if (yerr > distance) {
			yerr -= distance;
			uCol += incy;
		}
	}
}

/**
 * @brief 画一个填充的矩形
 * 
 * @param x0 左端点X坐标 [0,127]
 * @param y0 上端点Y坐标 [0,63]
 * @param x1 右端点X坐标 [0,127]
 * @param y1 下端点Y坐标 [0,63]
 * @param mode 颜色模式
*/
void OLED_DrawRectangleFilled(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, OLED_ColorMode mode) {
	// 判断输入参数是否合理
	if (x0 >= x1 || y0 >= y1) {
		return;
	}
	for (int i = y0; i <= y1; i++) {
		OLED_DrawLine(x0, i, x1, i, mode);
	}
}

/**
 * @brief 画一个空心的矩形
 * 
 * @param x0 左端点X坐标 [0,127]
 * @param y0 上端点Y坐标 [0,63]
 * @param x1 右端点X坐标 [0,127]
 * @param y1 下端点Y坐标 [0,63]
 * @param mode 颜色模式
*/
void OLED_DrawRectangle(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, OLED_ColorMode mode) {
	// 判断输入参数是否合理
	if (x0 >= x1 || y0 >= y1) {
		return;
	}
	OLED_DrawLine(x0, y0, x1, y0, mode);
	OLED_DrawLine(x0, y1, x1, y1, mode);
	OLED_DrawLine(x0, y0, x0, y1, mode);
	OLED_DrawLine(x1, y0, x1, y1, mode);
}

/**
 * @brief 空心圆辅助函数
 * 
 * @note 参考开源图形库u8g2
 * 
*/
void _OLED_DrawCircleSection(uint8_t x, uint8_t y, uint8_t x0, uint8_t y0, OLED_DrawCircleOption option, OLED_ColorMode mode) {
    /* upper right */
    if ( option & CIRCLE_UPPER_RIGHT ) {
	  OLED_DrawPixel(x0 + x, y0 - y, mode);
      OLED_DrawPixel(x0 + y, y0 - x, mode);
    }
    
    /* upper left */
    if ( option & CIRCLE_UPPER_LEFT ) {
      OLED_DrawPixel(x0 - x, y0 - y, mode);
      OLED_DrawPixel(x0 - y, y0 - x, mode);
    }
    
    /* lower right */
    if ( option & CIRCLE_LOWER_LEFT ) {
      OLED_DrawPixel(x0 + x, y0 + y, mode);
      OLED_DrawPixel(x0 + y, y0 + x, mode);
    }
    
    /* lower left */
    if ( option & CIRCLE_LOWER_RIGHT ) {
      OLED_DrawPixel(x0 - x, y0 + y, mode);
      OLED_DrawPixel(x0 - y, y0 + x, mode);
    }
}

/**
 * @brief 实心圆辅助函数
 * 
 * @note 参考开源图形库u8g2
 * 
*/
void _OLED_DrawCircleSectionFilled(uint8_t x, uint8_t y, uint8_t x0, uint8_t y0, OLED_ColorMode mode) {
	OLED_DrawLine(x0 + x, y0 - y, x0 - x, y0 - y, mode);
	OLED_DrawLine(x0 + y, y0 - x, x0 - y, y0 - x, mode);
	OLED_DrawLine(x0 + x, y0 + y, x0 - x, y0 + y, mode);
	OLED_DrawLine(x0 - y, y0 + x, x0 + y, y0 + x, mode);
}

/**
 * @brief 画一个空心的圆形
 * 
 * @param x0 圆心X坐标
 * @param y0 圆心Y坐标
 * @param rad 圆半径
 * @param option 绘制圆的部分
 * @param mode 颜色模式
 * 
 * @note 参考开源图形库u8g2
 * 
*/
void OLED_DrawCircle(uint8_t x0, uint8_t y0, uint8_t rad, OLED_DrawCircleOption option, OLED_ColorMode mode) {
    int f;
    int ddF_x;
    int ddF_y;
    uint16_t x;
    uint16_t y;

    f = 1;
    f -= rad;
    ddF_x = 1;
    ddF_y = 0;
    ddF_y -= rad;
    ddF_y *= 2;
    x = 0;
    y = rad;

    _OLED_DrawCircleSection(x, y, x0, y0, option, mode);
    
    while ( x < y ) {
      if (f >= 0) {
        y--;
        ddF_y += 2;
        f += ddF_y;
      }
      x++;
      ddF_x += 2;
      f += ddF_x;

      _OLED_DrawCircleSection(x, y, x0, y0, option, mode);    
    }
}

/**
 * @brief 画一个实心的圆形
 * 
 * @param x0 圆心X坐标
 * @param y0 圆心Y坐标
 * @param rad 圆半径
 * @param mode 颜色模式
 * 
 * @note 参考开源图形库u8g2
 * 
*/
void OLED_DrawCircleFilled(uint8_t x0, uint8_t y0, uint8_t rad, OLED_ColorMode mode) {
    int f;
    int ddF_x;
    int ddF_y;
    uint16_t x;
    uint16_t y;

    f = 1;
    f -= rad;
    ddF_x = 1;
    ddF_y = 0;
    ddF_y -= rad;
    ddF_y *= 2;
    x = 0;
    y = rad;

    _OLED_DrawCircleSectionFilled(x, y, x0, y0, mode);
    
    while ( x < y ) {
      if (f >= 0) {
        y--;
        ddF_y += 2;
        f += ddF_y;
      }
      x++;
      ddF_x += 2;
      f += ddF_x;

      _OLED_DrawCircleSectionFilled(x, y, x0, y0, mode);    
    }
}

/**
 * @brief: 在指定位置显示一个ASCII字符
 * 
 * @param x X坐标 [0,127]
 * @param y Y坐标 [0,63]
 * @param c 要显示的字符
 * @param font 字体 8(6x8)/12(6x12)/16(8x16)/24(12x24)
 * @param mode 颜色模式
 * 
 * */
void OLED_PrintChar(uint8_t x, uint8_t y, char c, uint8_t font, OLED_ColorMode mode) {
	uint8_t i, m, temp, size2, chr1;
	uint8_t x0 = x, y0 = y;
	if (font == 8)
		size2 = 6;
	else
		size2 = (font / 8 + ((font % 8) ? 1 : 0)) * (font / 2); //得到字体一个字符对应点阵集所占的字节数
	chr1 = c - ' ';  //计算偏移后的值
	for (i = 0; i < size2; i++) {
		if (font == 8)
			temp = asc2_0806[chr1][i]; //调用0806字体
		else if (font == 12)
			temp = asc2_1206[chr1][i]; //调用1206字体
		else if (font == 16)
			temp = asc2_1608[chr1][i]; //调用1608字体
		else if (font == 24)
			temp = asc2_2412[chr1][i]; //调用2412字体
		else
			return;
		for (m = 0; m < 8; m++) {
			if (temp & 0x01)
				OLED_DrawPixel(x, y, mode);
			else
				OLED_DrawPixel(x, y, !mode);
			temp >>= 1;
			y++;
		}
		x++;
		if ((font != 8) && ((x - x0) == font / 2)) {
			x = x0;
			y0 = y0 + 8;
		}
		y = y0;
	}
}

/**
 * @brief: 在指定位置显示ASCII字符串
 * 
 * @param x X坐标 [0,127]
 * @param y Y坐标 [0,63]
 * @param s 要显示的字符串
 * @param font 字体 8(6x8)/12(6x12)/16(8x16)/24(12x24)
 * @param mode 颜色模式
 * 
 * */
void OLED_PrintString(uint8_t x, uint8_t y, char *s, uint8_t font, OLED_ColorMode mode) {
	while ((*s >= ' ') && (*s <= '~')) //判断是不是非法字符!
	{
		OLED_PrintChar(x, y, *s, font, mode);
		if (font == 8)
			x += 6;
		else
			x += font / 2;
		s++;
	}
}

/**
 * @brief: 在指定位置显示中文字符
 * 
 * @param x X坐标 [0,127]
 * @param y Y坐标 [0,63]
 * @param c 要显示的字符，用双引号表达
 * @param font 字体 16(16x16)
 * @param mode 颜色模式
 * 
 * @return 显示成功/失败
 *
 * */
OLED_Ret OLED_PrintChineseChar(uint8_t x, uint8_t y, char c[4], uint8_t font, OLED_ColorMode mode) {
	uint8_t m, temp;
	int32_t font_len, char_index = -1;
	uint8_t x0 = x, y0 = y;
	uint16_t i, size3 = (font / 8 + ((font % 8) ? 1 : 0)) * font;  //得到字体一个字符对应点阵集所占的字节数
	// 在字库中寻找对应字符
	if (font == 16) {
		// 16*16字体
		font_len = sizeof(chs_font)/sizeof(struct font);
		for (int j = 0; j < font_len; j++) {
			if(chs_font[j].index[0] == c[0] && chs_font[j].index[1] == c[1] && chs_font[j].index[2] == c[2] && chs_font[j].index[3] == c[3]) {
				char_index = j;
				break;
			}
		}
		// 如果找不到该字符直接返回错误
		if (char_index == -1) {
			return OLED_FAILED;
		}
		// 显示字符
		for (i = 0; i < size3; i++) {
			temp = chs_font[char_index].typeface[i];
			for (m = 0; m < 8; m++) {
				if (temp & 0x01)
					OLED_DrawPixel(x, y, !mode);
				else
					OLED_DrawPixel(x, y, mode);
				temp >>= 1;
				y++;
			}
			x++;
			if ((x - x0) == font) {
				x = x0;
				y0 = y0 + 8;
			}
			y = y0;
		}
		return OLED_SUCCESS;
	} 
	// 找不到指定字体，返回错误
	return OLED_FAILED;
}

// 检查当前字符是否是GBK编码
uint8_t is_gbk(char c) {
    return (c >= 0x81 && c <= 0xFE);
}

// 检查当前字符是否是ASCII编码
uint8_t is_ascii(char c) {
    return (c >= 0x20 && c <= 0x7E);
}

/**
 * @brief 显示中/英文混合字符串
 * 
 * @param x X坐标 [0,127]
 * @param y Y坐标 [0,63]
 * @param c 要显示的字符串，用双引号表达
 * @param font 字体 16(16x16)
 * @param mode 颜色模式
 * 
*/
void OLED_PrintChinese(uint8_t x, uint8_t y, char *s, uint8_t font, OLED_ColorMode mode) {
	char chr[4];
	int i = 0;
    while (s[i] != '\0') {
        if ((s[i] & 0x80) == 0x00) {
            // 1字节字符，即ASCII字符
			OLED_PrintChar(x, y, s[i], font, mode);
			// 移动光标
			if (font == 8)
				x += 6;
			else
				x += font / 2;
            i += 1;
        } else if ((s[i] & 0xE0) == 0xC0) {
            // 2字节字符
			chr[0] = s[i];
			chr[1] = s[i + 1];
			chr[2] = 0;
			chr[3] = 0;
			// 如果该字符成功显示，则移动光标
			if(!OLED_PrintChineseChar(x, y, chr, font, mode)){
				x += font;
			}
            i += 2;
        } else if ((s[i] & 0xF0) == 0xE0) {
            // 3字节字符
			chr[0] = s[i];
			chr[1] = s[i + 1];
			chr[2] = s[i + 2];
			chr[3] = 0;
			// 如果该字符成功显示，则移动光标
			if(!OLED_PrintChineseChar(x, y, chr, font, mode)){
				x += font;
			}
            i += 3;
        } else if ((s[i] & 0xF8) == 0xF0) {
            // 4字节字符
			chr[0] = s[i];
			chr[1] = s[i + 1];
			chr[2] = s[i + 2];
			chr[3] = s[i + 3];
			// 如果该字符成功显示，则移动光标
			if(!OLED_PrintChineseChar(x, y, chr, font, mode)){
				x += font;
			}
            i += 4;
        }
    }
}

/**
 * @brief 显示BMP图像
 * 
 * @param x X左端点坐标 [0,127]
 * @param y Y上端点坐标 [0,63]
 * @param size_x 图像宽度
 * @param size_y 图像高度
 * @param pic 要显示的图像
 * @param mode 颜色模式
 * 
*/
void OLED_DrawPicture(uint8_t x, uint8_t y, uint8_t size_x, uint8_t size_y, const unsigned char pic[], OLED_ColorMode mode) {
	uint16_t j = 0;
	uint8_t i, n, temp, m;
	uint8_t x0 = x, y0 = y;
	size_y = size_y / 8 + ((size_y % 8) ? 1 : 0);
	for(n = 0; n < size_y; n ++) {
		for(i = 0; i < size_x; i ++) {
			temp = pic[j];
			j ++;
			for(m = 0; m < 8; m ++) {
				if(temp & 0x01) {
					OLED_DrawPixel(x, y, !mode);
				}
				else {
					OLED_DrawPixel(x, y, mode);
				}
				temp >>= 1;
				y ++;
			}
			x ++;
			if((x - x0) == size_x) {
				x = x0;
				y0 = y0 + 8;
			}
			y = y0;
     	}
	 }
}

