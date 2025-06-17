/*
 * 作者: LittleLeaf All rights reserved
 */
#include <llos_ssd1306.h>
#include <llos_ssd1306_font.h>

static uint8_t arg, ll_screenNum;
static uint32_t idN;
static struct ll_SSD1306_conf_t *iSSD1306_conf;

static void I8080_WriteByteCB(uint8_t data, enum ll_SSD1306_cmdType_t cmd)
{
	if(iSSD1306_conf[idN].devDC != NULL)
	{
		arg = ll_reset;
		iSSD1306_conf[idN].devSPI_I2C->ctrlCB(iSSD1306_conf[idN].devCS, iSSD1306_conf[idN].pinCSorAddr, &arg);

		LLOS_Device_WritePin(iSSD1306_conf[idN].devDC, iSSD1306_conf[idN].pinDC, (uint8_t)cmd);
		LLOS_Device_Write(iSSD1306_conf[idN].devSPI_I2C, 0, 0, &data, 1);
		
		arg = ll_set;
		iSSD1306_conf[idN].devSPI_I2C->ctrlCB(iSSD1306_conf[idN].devCS, iSSD1306_conf[idN].pinCSorAddr, &arg);
	}
	else
	{
		uint8_t buffer[2];
		
		buffer[0] = cmd ? 0x40:0x00;
		buffer[1] = data;
		
		LLOS_Device_Write(iSSD1306_conf[idN].devSPI_I2C, (uint8_t)iSSD1306_conf[idN].pinCSorAddr << 1, 0, buffer, 2);
	}
}
static void I8080_DMAWriteCB(const uint8_t *pic, uint32_t len)
{
	if(iSSD1306_conf[idN].devDC != NULL)
	{
		arg = ll_reset;
		iSSD1306_conf[idN].devSPI_I2C->ctrlCB(iSSD1306_conf[idN].devCS, iSSD1306_conf[idN].pinCSorAddr, &arg);
		
		LLOS_Device_WritePin(iSSD1306_conf[idN].devDC, iSSD1306_conf[idN].pinDC, (uint8_t)ll_SSD1306_cmd_Data);
		LLOS_Device_DMAWrite(iSSD1306_conf[idN].devSPI_I2C, 0, 0, pic, len);
		
		arg = ll_set;
		iSSD1306_conf[idN].devSPI_I2C->ctrlCB(iSSD1306_conf[idN].devCS, iSSD1306_conf[idN].pinCSorAddr, &arg);
	}
	else
	{
		LLOS_Device_DMAWrite(iSSD1306_conf[idN].devSPI_I2C, iSSD1306_conf[idN].pinCSorAddr << 1, 0, pic, len);
	}
}

void LLOS_SSD1306_HAL_Init(struct ll_SSD1306_conf_t *SSD1306_conf, uint8_t screenNum)
{
	uint32_t size;

	if(SSD1306_conf == NULL || screenNum <= 0 || SSD1306_conf->devSPI_I2C == NULL)
	{
		LL_LOG_E("LLOS_SSD1306_HAL_Init ", "para error!\r\n");
		while(1);
	}
	
	ll_screenNum = screenNum;
	
	size = sizeof(struct ll_SSD1306_conf_t) * ll_screenNum;
	iSSD1306_conf = LLOS_malloc(size);
	if(iSSD1306_conf == NULL)
	{
		LL_LOG_E("LLOS_SSD1306_HAL_Init ", "SSD1306_conf malloc null!\r\n");
		while(1);
	}
	memcpy(iSSD1306_conf, SSD1306_conf, size);
	
	for(size = 0; size < ll_screenNum; size++)
	{
		LLOS_SSD1306_HAL_Select(size);
		LLOS_SSD1306_Init(&iSSD1306_conf[size].screenConf);
	}
}

void LLOS_SSD1306_HAL_Select(uint8_t id)
{
	if(id >= ll_screenNum)
	{
		LL_LOG_E("LLOS_SSD1306_Select ", "id >= ll_screenNum!\r\n");
		return;
	}
	idN = id;
}

void LLOS_SSD1306_Init(struct ll_SSD1306_screenConf_t *screenConf)
{
	if(screenConf == NULL)
	{
		LL_LOG_E("LLOS_SSD1306_Init ", "para NULL!\r\n");
		while(1);
	}

	I8080_WriteByteCB(0xAE, ll_SSD1306_cmd_Cmd);	/* 关闭OLED -- turn off oled panel */

	if(screenConf->isMirrot)	/* 设置段重映射 -- Set SEG / Column Mapping     0xA0左右反置（复位值） 0xA1正常（重映射值） */
		I8080_WriteByteCB(0xA0, ll_SSD1306_cmd_Cmd);
	else
		I8080_WriteByteCB(0xA1, ll_SSD1306_cmd_Cmd);

	if(screenConf->isInvert)	/* 设置行输出扫描方向 -- Set COM / Row Scan Direction   0xc0上下反置（复位值） 0xC8正常（重映射值） */
		I8080_WriteByteCB(0xC0, ll_SSD1306_cmd_Cmd);
	else
		I8080_WriteByteCB(0xC8, ll_SSD1306_cmd_Cmd);

	if(screenConf->isInvertPhase)	/* 设置显示方式(正常/反显) -- set normal display (0xA6 / 0xA7) */
		I8080_WriteByteCB(0xA7, ll_SSD1306_cmd_Cmd);
	else
		I8080_WriteByteCB(0xA6, ll_SSD1306_cmd_Cmd);

	I8080_WriteByteCB(0x81, ll_SSD1306_cmd_Cmd);	/* 设置对比度 -- set contrast control register (0x00~0x100) */
	I8080_WriteByteCB(screenConf->brightness, ll_SSD1306_cmd_Cmd);    /* \ Set SEG Output Current Brightness */

	I8080_WriteByteCB(0xD5, ll_SSD1306_cmd_Cmd);	/* 设置显示时钟分频因子/振荡器频率 -- set display clock divide ratio/oscillator frequency */
	I8080_WriteByteCB(0x80, ll_SSD1306_cmd_Cmd);	/* \ set divide ratio, Set Clock as 100 Frames/Sec */

	I8080_WriteByteCB(0xD9, ll_SSD1306_cmd_Cmd);	/* 设置预充电期间的持续时间 -- set pre-charge period */
	I8080_WriteByteCB(0xF1, ll_SSD1306_cmd_Cmd);	/* \ Set Pre-Charge as 15 Clocks & Discharge as 1 Clock */

	I8080_WriteByteCB(0xDB, ll_SSD1306_cmd_Cmd);	/* 调整VCOMH调节器的输出 -- set vcomh (0x00 / 0x20 / 0x30) */
	I8080_WriteByteCB(0x20, ll_SSD1306_cmd_Cmd);	/* \ Set VCOM Deselect Level */

	if(screenConf->type == ll_SSD1306_screenType_128x64)
	{
		screenConf->width = 128;
		screenConf->height = 64;

		I8080_WriteByteCB(0xA8, ll_SSD1306_cmd_Cmd);	/* 设置多路传输比率 -- set multiplex ratio (16 to 63) */
		I8080_WriteByteCB(0x3F, ll_SSD1306_cmd_Cmd);	/* \ 1 / 64 duty */

		I8080_WriteByteCB(0xDA, ll_SSD1306_cmd_Cmd);	/* 设置列引脚硬件配置 -- set com pins hardware configuration */
		I8080_WriteByteCB(0x12, ll_SSD1306_cmd_Cmd);	/* \ Sequential COM pin configuration，Enable COM Left/Right remap */
	}
	else if(screenConf->type == ll_SSD1306_screenType_128x32)
	{
		screenConf->width = 128;
		screenConf->height = 32;

		I8080_WriteByteCB(0xA8,ll_SSD1306_cmd_Cmd);	/* 设置多路传输比率 -- set multiplex ratio (16 to 63) */
		I8080_WriteByteCB(0x1F,ll_SSD1306_cmd_Cmd);	/* \ 1 / 32 duty */

		I8080_WriteByteCB(0xDA,ll_SSD1306_cmd_Cmd);	/* 设置列引脚硬件配置 -- set com pins hardware configuration */
		I8080_WriteByteCB(0x02,ll_SSD1306_cmd_Cmd);	/* \ Sequential COM pin configuration，Disable COM Left/Right remap */
	}

	I8080_WriteByteCB(0x40, ll_SSD1306_cmd_Cmd);	/* 设置设置屏幕（GDDRAM）起始行 -- Set Display Start Line (0x40~0x7F) */

	I8080_WriteByteCB(0xD3, ll_SSD1306_cmd_Cmd);	/* 设置显示偏移 -- set display offset (0x00~0x3F) */
	I8080_WriteByteCB(0x00, ll_SSD1306_cmd_Cmd);	/* \ not offset */

	I8080_WriteByteCB(0x8D, ll_SSD1306_cmd_Cmd);	/* 电荷泵设置 -- set Charge Pump enable / disable (0x14 / 0x10) */
	I8080_WriteByteCB(0x14, ll_SSD1306_cmd_Cmd);	/* \ Enable charge pump during display on */

	I8080_WriteByteCB(0xA4, ll_SSD1306_cmd_Cmd);	/* 全局显示开启(黑屏/亮屏) -- Entire Display On (0xA4 / 0xA5) */

	LLOS_SSD1306_Fill(0x00);				/* 清屏 */
	I8080_WriteByteCB(0xAF, ll_SSD1306_cmd_Cmd);	/* 打开显示 */
}

void LLOS_SSD1306_ScreenEN(ll_newState_t newState)
{
	I8080_WriteByteCB(0x8D, ll_SSD1306_cmd_Cmd);  	/* 升压使能 */
	if(newState)
	{
		I8080_WriteByteCB(0x14, ll_SSD1306_cmd_Cmd);  /* 启用升压使能 */
		I8080_WriteByteCB(0xAF, ll_SSD1306_cmd_Cmd);  /* 打开显示 */
	}
	else
	{
		I8080_WriteByteCB(0x10, ll_SSD1306_cmd_Cmd);  /* 禁用升压使能 */
		I8080_WriteByteCB(0xAE, ll_SSD1306_cmd_Cmd);  /* 关闭显示 */
	}
}

void LLOS_SSD1306_Fill(uint16_t color)
{
	uint16_t i, j;
    for(i = 0; i < iSSD1306_conf[idN].screenConf.height >> 3; i++)
    {
        I8080_WriteByteCB(0xB0 + i, ll_SSD1306_cmd_Cmd);
        I8080_WriteByteCB(0x00 + iSSD1306_conf[idN].screenConf.xOffset, ll_SSD1306_cmd_Cmd);
        I8080_WriteByteCB(0x10, ll_SSD1306_cmd_Cmd);

        for(j = 0; j < iSSD1306_conf[idN].screenConf.width; j++)
            I8080_WriteByteCB(color, ll_SSD1306_cmd_Data);
    }
}

void LLOS_SSD1306_SetPos(uint16_t x, uint16_t y)
{
	I8080_WriteByteCB(0xB0 + y, ll_SSD1306_cmd_Cmd);
	I8080_WriteByteCB((x + iSSD1306_conf[idN].screenConf.xOffset & 0x0F), ll_SSD1306_cmd_Cmd);
	I8080_WriteByteCB(((x + iSSD1306_conf[idN].screenConf.xOffset & 0xF0) >> 4) | 0x10, ll_SSD1306_cmd_Cmd);
}

void LLOS_SSD1306_DrawPic(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *pic)
{
	uint16_t i, j, k = 0;

	if(iSSD1306_conf[idN].isDMA)
	{
		for (i = 0; i < 8; i++)
		{
			LLOS_SSD1306_SetPos(0, i);
			I8080_DMAWriteCB(pic + 128 * i, 128);
		}
	}
	else
	{
		for(j = y; j < y + h; j++)
		{
			i = x;
			LLOS_SSD1306_SetPos(i, j);
			for( ; i < x + w; i++)
				I8080_WriteByteCB(pic[k++], ll_SSD1306_cmd_Data);
		}
	}
}

void LLOS_SSD1306_GetSize(uint16_t *w, uint16_t *h)
{
	*w = iSSD1306_conf[idN].screenConf.width;
	*h = iSSD1306_conf[idN].screenConf.height;
}

static void LLOS_SSD1306_ShowChar(uint16_t x, uint16_t y, const char chr, enum ll_SSD1306_sizeFont_t sizeFont)
{
	uint8_t i;
	char ch;
	
	ch = chr;
	ch -= ' ';
	
	if(sizeFont == ll_SSD1306_sizeFont_6x8)
	{
		LLOS_SSD1306_SetPos(x, y);
		for(i = 0; i < 6; i++)
			I8080_WriteByteCB(ll_SSD1306_font_ASCII6x8[ch][i], ll_SSD1306_cmd_Data);
	}
	else
    {
        LLOS_SSD1306_SetPos(x, y); /* 填充第一页 */
        for(i = 0; i < 8; i++)
            I8080_WriteByteCB(ll_SSD1306_font_ASCII8x16[ch][i], ll_SSD1306_cmd_Data); /* 写入数据 */
        LLOS_SSD1306_SetPos(x, y + 1); /* 填充第二页 */
        for(i = 0; i < 8; i++)
            I8080_WriteByteCB(ll_SSD1306_font_ASCII8x16[ch][i + 8], ll_SSD1306_cmd_Data);
    }
}
static void LLOS_SSD1306_ShowStr(uint16_t x, uint16_t y, const char *str, enum ll_SSD1306_sizeFont_t sizeFont)
{
	while(*str != '\0')
	{
		if(*str == '\r' && *(str + 1) == '\n')
		{
			if(sizeFont == ll_SSD1306_sizeFont_6x8)++y; /* 换行 */
			else if(sizeFont == ll_SSD1306_sizeFont_8x16)y += 2; /* 换行 */
			x = 0;
			str += 2;
		}
		if((x > (iSSD1306_conf[idN].screenConf.width - 6) && sizeFont == ll_SSD1306_sizeFont_6x8)||(x > (iSSD1306_conf[idN].screenConf.width - 8) && sizeFont == ll_SSD1306_sizeFont_8x16)) /* 自动换行 */
		{
			x = 0;
			if(sizeFont == ll_SSD1306_sizeFont_6x8)++y; /* 换行 */
			else if(sizeFont == ll_SSD1306_sizeFont_8x16)y += 2; /* 换行 */
		}
		
		LLOS_SSD1306_ShowChar(x, y, *str, sizeFont);
		
		if(sizeFont == ll_SSD1306_sizeFont_6x8)x += 6;
		else if(sizeFont == ll_SSD1306_sizeFont_8x16)x += 8;
			
		str++; /* 字符串指针移动 */
	}
}

void LLOS_SSD1306_ShowNumFormat(uint16_t x, uint16_t y, float num, const char *format, enum ll_SSD1306_sizeFont_t sizeFont)
{
    char s[100], *p;
	bool isFloat = false;
	uint8_t i = 0;
	
	p = (char *)format;
	while(*p++)
	{
		if(format[i] == '%' && format[i+1] == 'f')
		{
			isFloat = true;
			break;
		}
		else if(format[i] == '%' && format[i+1] == '.' && format[i+3] == 'f')
		{
			isFloat = true;
			break;
		}
		else
		{
			isFloat = false;
		}
		
		i++;
	}
	
    if(isFloat)
	{
		sprintf(s, format, num);
	}
	else
	{
        int32_t i = (int32_t)num;
        sprintf(s, format, i);
    }
	
    LLOS_SSD1306_ShowStr(x, y, s, sizeFont);
}

void LLOS_SSD1306_ShowString(uint16_t x, uint16_t y, const char *str)
{
	const unsigned char *s = (const unsigned char *)str;
	
#ifdef ll_SSD1306_font_CN1616
	uint8_t chinese_num = sizeof(ll_SSD1306_font_CN16x16) / sizeof(ll_SSD1306_font_CN_t); /* 计算字库字符数 */
	uint8_t index,i;
#endif
	while(*s != '\0')
	{
		if(*s == '\r' && *(s + 1) == '\n')
		{
			y += 2; /* 换行 */
			x = 0;
			s += 2 ;
		}
#ifdef ll_SSD1306_font_CN1616
		if((*s) > 127) /* 如果是中文 */
		{
			if(x > iSSD1306_conf[idN].screenConf.width - 16) /* 自动换行 */
			{
				x = 0;
				y += 2; /* 换行 */
			}
			for(index = 0; index < chinese_num; index++) /* 循环查找 */
			{
				if(ll_SSD1306_font_CN16x16[index].CN_index[0] == *s && ll_SSD1306_font_CN16x16[index].CN_index[1] == *(s + 1) && ll_SSD1306_font_CN16x16[index].CN_index[2] == *(s + 2))
				{			
					LLOS_SSD1306_SetPos(x, y);
					/* 从字库中查找字模填充第一页 */		
					for(i = 0; i < 16; i++)
						I8080_WriteByteCB(ll_SSD1306_font_CN16x16[index].CN_library[i], ll_SSD1306_cmd_Data);
					
					LLOS_SSD1306_SetPos(x, y + 1); /* 一个汉字占两页，坐标跳转下一页 */
					
					/* 从字库中查找字模填充第二页 */		
					for(i = 0; i < 16; i++)
						I8080_WriteByteCB(ll_SSD1306_font_CN16x16[index].CN_library[i + 16], ll_SSD1306_cmd_Data);
					
					x += 16; /* x指针往后移16位，为显示下一个汉字做准备 */
					if(x > iSSD1306_conf[idN].screenConf.width - 16) /* 自动换行 */
					{
						x = 0;
						y += 2;
					}	
				}
			}
			s += 3;	
			index = 0;
		}
#endif
		else 
		{
			if(x > iSSD1306_conf[idN].screenConf.width - 8) /* 自动换行 */
			{
				x = 0;
				y += 2; /* 换行 */
			}
			LLOS_SSD1306_ShowChar(x, y, *s, ll_SSD1306_sizeFont_8x16);
			x += 8;
			s++;
		}
	}
}
uint8_t g_aLcdBuf[128][64 >> 3];
void LLOS_SSD1306_DrawDot(uint16_t x, uint16_t y, uint16_t dummy)
{
    uint8_t PageNumber = y >> 3;
    LLOS_SSD1306_SetPos(x, PageNumber);
    g_aLcdBuf[x][PageNumber] |= 1 << (y & 7);

    I8080_WriteByteCB(g_aLcdBuf[x][PageNumber], ll_SSD1306_cmd_Data);
}
