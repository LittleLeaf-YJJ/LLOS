#include <llos_graphic.h>

static ll_graphic_drawDotCB_t ll_graphic_drawDotCB;

void LLOS_Graphic_Register_DrawDot(ll_graphic_drawDotCB_t graphic_drawDotCB)
{
	if(graphic_drawDotCB != NULL)ll_graphic_drawDotCB = graphic_drawDotCB;
	else LL_LOG_E("LLOS_Graphic_Register_DrawDot ", "para NULL!\r\n");
}

void LLOS_Graphic_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
	if(ll_graphic_drawDotCB == NULL)
	{
		LL_LOG_E("LLOS_Graphic_DrawLine ", "Callback NULL!\r\n");
		return;
	}
	
    uint16_t Xpoint, Ypoint;
    int16_t dx, dy;
    int16_t XAddway, YAddway;
    int16_t Esp;
    int8_t dottedLen;
    Xpoint = x1;
    Ypoint = y1;
	
    dx = (int16_t)x2 - (int16_t)x1 >= 0 ? x2 - x1 : x1 - x2;
    dy = (int16_t)y2 - (int16_t)y1 <= 0 ? y2 - y1 : y1 - y2;

    XAddway = x1 < x2 ? 1 : -1;
    YAddway = y1 < y2 ? 1 : -1;

    Esp = dx + dy;
    dottedLen = 0;

    for(;;)
    {
        dottedLen++;
        ll_graphic_drawDotCB(Xpoint, Ypoint, color);
        if((Esp << 1) >= dy)
        {
            if(Xpoint == x2)break;
            Esp += dy;
            Xpoint += XAddway;
        }
        if((Esp << 1) <= dx)
        {
            if(Ypoint == y2)break;
            Esp += dx;
            Ypoint += YAddway;
        }
    }
}

void LLOS_Graphic_DrawRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, ll_newState_t isFill, uint16_t color)
{
	if(ll_graphic_drawDotCB == NULL)
	{
		LL_LOG_E("LLOS_Graphic_DrawRectangle ", "Callback NULL!\r\n");
		return;
	}
	
    uint16_t i;
	
    if(isFill)
    {
		for(i = y; i < y + h; i++)LLOS_Graphic_DrawLine(x, i, x + w, i, color);
    }
    else
    {
        LLOS_Graphic_DrawLine(x, y, x + w, y, color);
        LLOS_Graphic_DrawLine(x, y, x, y + h, color);
        LLOS_Graphic_DrawLine(x + w, y + h, x + w, y, color);
        LLOS_Graphic_DrawLine(x + w, y + h, x, y + h, color);
    }
}
void LLOS_Graphic_DrawRoundedRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t r, ll_newState_t isFill, uint16_t color)
{
	if(ll_graphic_drawDotCB == NULL)
	{
		LL_LOG_E("LLOS_Graphic_DrawRoundedRectangle ", "Callback NULL!\r\n");
		return;
	}
	
	uint16_t i;

    if(isFill)
    {
    	i = y;
    	for( ; i < y + r; i++)
    	{
    		LLOS_Graphic_DrawLine(x + (r - (i - y)), i, x + w - 1 - (r - (i - y)), i, color);
		}
    	for( ; i < y + h - r; i++)
    	{
    		LLOS_Graphic_DrawLine(x, i, x + w - 1, i, color);
		}
    	for( ; i < y + h; i++)
    	{
    		LLOS_Graphic_DrawLine(x + r - (y + h - i), i, x + w - 1 - r + (y + h - i), i, color);
		}
    }
    else
    {
        LLOS_Graphic_DrawLine(x + r, y, x + w - 1 - r, y, color);
        LLOS_Graphic_DrawLine(x + w - 1, y + r, x + w - 1, y + h - 1 - r, color);
        LLOS_Graphic_DrawLine(x + w - 1 - r, y + h - 1, x + r, y + h - 1, color);
        LLOS_Graphic_DrawLine(x, y + h - 1 - r, x, y + r, color);

        LLOS_Graphic_DrawLine(x + r, y, x, y + r, color);
        LLOS_Graphic_DrawLine(x + w - 1 - r, y, x + w - 1, y + r, color);
        LLOS_Graphic_DrawLine(x + w - 1, y + h - 1 - r, x + w - 1 - r, y + h - 1, color);
        LLOS_Graphic_DrawLine(x, y + h - 1 - r, x + r, y + h - 1, color);
	}
}
void LLOS_Graphic_DrawCircle(uint16_t x, uint16_t y, uint16_t r, ll_newState_t isFill, uint16_t color)
{
	if(ll_graphic_drawDotCB == NULL)
	{
		LL_LOG_E("LLOS_Graphic_DrawCircle ", "Callback NULL!\r\n");
		return;
	}
	
	int16_t sCurrentX, sCurrentY;
	int16_t sError;

     sCurrentX = 0;
     sCurrentY = r;
     sError = 3 - (r << 1);

     while(sCurrentX <= sCurrentY)
     {
         int16_t sCountY = 0;
         if(isFill)
         {
             for(sCountY = sCurrentX; sCountY <= sCurrentY; sCountY++)
             {
                 ll_graphic_drawDotCB(x + sCurrentX, y + sCountY, color);
                 ll_graphic_drawDotCB(x - sCurrentX, y + sCountY, color);
                 ll_graphic_drawDotCB(x - sCountY,   y + sCurrentX, color);
                 ll_graphic_drawDotCB(x - sCountY,   y - sCurrentX, color);
                 ll_graphic_drawDotCB(x - sCurrentX, y - sCountY, color);
                 ll_graphic_drawDotCB(x + sCurrentX, y - sCountY, color);
                 ll_graphic_drawDotCB(x + sCountY,   y - sCurrentX, color);
                 ll_graphic_drawDotCB(x + sCountY,   y + sCurrentX, color);
             }
		}
		else
		{
			ll_graphic_drawDotCB(x + sCurrentX, y + sCurrentY, color);
			ll_graphic_drawDotCB(x - sCurrentX, y + sCurrentY, color);
			ll_graphic_drawDotCB(x - sCurrentY, y + sCurrentX, color);
			ll_graphic_drawDotCB(x - sCurrentY, y - sCurrentX, color);
			ll_graphic_drawDotCB(x - sCurrentX, y - sCurrentY, color);
			ll_graphic_drawDotCB(x + sCurrentX, y - sCurrentY, color);
			ll_graphic_drawDotCB(x + sCurrentY, y - sCurrentX, color);
			ll_graphic_drawDotCB(x + sCurrentY, y + sCurrentX, color);
		}
		sCurrentX++;
		if(sError < 0)sError += (4 * sCurrentX + 6);
		else
		{
			sError += (10 + 4 * (sCurrentX - sCurrentY));
			sCurrentY--;
		}
     }
}
/* 扫描线活动表 */
struct edge_t
{
	uint16_t startY;
    float startX, endX;
    float slopeInv; /* 斜率的倒数 */
};
/* 交换两个整数值 */
static void SwapInt(uint16_t *a, uint16_t *b)
{
	uint16_t temp = *a;
    *a = *b;
    *b = temp;
}
/* 初始化边的信息 */
static void InitEdge(struct edge_t *e, uint16_t yStart, float xStart, float xEnd, float slopeInverse)
{
    e->startY = yStart;
    e->startX = xStart;
    e->endX = xEnd;
    e->slopeInv = slopeInverse;
}

void LLOS_Graphic_DrawTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, ll_newState_t isFill, uint16_t color)
{
	if(ll_graphic_drawDotCB == NULL)
	{
		LL_LOG_E("LLOS_Graphic_DrawTriangle ", "Callback NULL!\r\n");
		return;
	}
	
	if(isFill)
	{
		/* 扫描线算法 */
	    /* 对三个点按y坐标进行排序 */
	    if(y1 > y2)
	    {
	    	SwapInt(&y1, &y2);
	    	SwapInt(&x1, &x2);
	    }
	    if(y1 > y3)
	    {
	    	SwapInt(&y1, &y3);
	    	SwapInt(&x1, &x3);
	    }
	    if(y2 > y3)
	    {
	    	SwapInt(&y2, &y3);
	    	SwapInt(&x2, &x3);
	    }

	    /* 初始化三条边的信息 */
	    struct edge_t edges[3];
	    InitEdge(&edges[0], y1, x1, x2, (float)(x2 - x1) / (y2 - y1));
	    InitEdge(&edges[1], y2, x2, x3, (float)(x3 - x2) / (y3 - y2));
	    InitEdge(&edges[2], y1, x1, x3, (float)(x3 - x1) / (y3 - y1));

	    /* 逐行扫描，填充三角形 */
	    for(uint16_t y = y1; y <= y3; y++)
	    {
	        /* 找到当前行对应的边 */
	    	int16_t leftX = -1, rightX = -1;
	        for(uint16_t i = 0; i < 3; i++)
	        {
	            if(y >= edges[i].startY)
	            {
	                if(leftX == -1 || edges[i].startX < edges[leftX].startX)leftX = i;
	                if(rightX == -1 || edges[i].startX > edges[rightX].startX)rightX = i;
	            }
	        }

	        /* 画水平线填充三角形 */
	        if (leftX != -1 && rightX != -1)
	        	LLOS_Graphic_DrawLine((uint16_t)edges[leftX].startX, y, (uint16_t)edges[rightX].startX, y, color);

	        /* 更新边的起始x坐标 */
	        edges[leftX].startX += edges[leftX].slopeInv;
	        edges[rightX].startX += edges[rightX].slopeInv;
	    }
	}
	else
	{
		LLOS_Graphic_DrawLine(x1, y1, x2, y2, color);
		LLOS_Graphic_DrawLine(x1, y1, x3, y3, color);
		LLOS_Graphic_DrawLine(x2, y2, x3, y3, color);
	}
}
