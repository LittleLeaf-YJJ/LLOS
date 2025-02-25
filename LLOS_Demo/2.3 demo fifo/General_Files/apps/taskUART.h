#ifndef __TASKUART_H
#define __TASKUART_H

#include <llos.h>
#include <llos_fifo.h>

void Task_UART_Init(void);

extern llos_fifo_t fifoUART;

#endif
