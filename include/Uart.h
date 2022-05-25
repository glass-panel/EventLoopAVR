#ifndef __UART_H__
    #define __UART_H__

#include "PipeIO.h"

void uart_init();
void uart_send_byte(char c);
using PipeUart = PipeIO<uart_send_byte>;
extern PipeIO<uart_send_byte> uart;
extern char uart_buffer[100];

#endif