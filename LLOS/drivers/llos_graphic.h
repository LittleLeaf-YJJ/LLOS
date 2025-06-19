/*
 * 绘图模块
 * @author LittleLeaf All rights reserved
 * @version V1.0.0
 * @date 2025/03/08
 */
#ifndef __LLOS_GRAPHIC_H
#define __LLOS_GRAPHIC_H

#include <llos.h>

#ifdef __cplusplus
 extern "C" {
#endif

typedef void (*ll_graphic_drawDotCB_t)(uint16_t x1, uint16_t y1, uint16_t color);

void LLOS_Graphic_Register_DrawDot(ll_graphic_drawDotCB_t graphic_drawDotCB);
void LLOS_Graphic_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void LLOS_Graphic_DrawRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, ll_newState_t isFill, uint16_t color);
void LLOS_Graphic_DrawRoundedRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t r, ll_newState_t isFill, uint16_t color);
void LLOS_Graphic_DrawCircle(uint16_t x, uint16_t y, uint16_t r, ll_newState_t isFill, uint16_t color);
void LLOS_Graphic_DrawTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, ll_newState_t isFill, uint16_t color);

#ifdef __cplusplus
 }
#endif
 
 #endif
