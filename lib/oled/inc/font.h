#ifndef __FONT_H
#define __FONT_H

typedef struct font
{
	char index[4];
	unsigned char typeface[32];
} font;

//8*6 ASCII
extern const unsigned char asc2_0806[][6]; 

//12*6 ASCII
extern const unsigned char asc2_1206[95][12];

//16*8 ASCII
extern const unsigned char asc2_1608[][16];

//24*12 ASICII
extern const unsigned char asc2_2412[][36];

//16*16 GBK
extern const font chs_font[12];

// 宽高61*58
extern const unsigned char icon_Bili[];

// 宽高48*48
extern const unsigned char icon_IDE[];

#endif

