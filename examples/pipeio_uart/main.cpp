#include <avr/io.h>
#include <avr/interrupt.h>
#include "../../include/EventLoopAVR.h"

#define BAUD_RATE 9600
#define CLOCK_FREQ 16000000

constexpr long BAUD = (CLOCK_FREQ/(BAUD_RATE*16UL)-1);

void uart_send_byte(char c)
{
    while(!(UCSR0A & (1<<UDRE0)));  // wait for empty transmit buffer
    UDR0 = c;
}

char uart_buffer[100];  // buffer for received data
PipeIO<uart_send_byte> uart(uart_buffer, sizeof(uart_buffer));  // use uart as a PipeIO object

ISR(USART_RX_vect, ISR_BLOCK)
{
    uint8_t c = UDR0;   // read byte from UDR0
    if(uart.flags() & (uint8_t)PipeIOFlags::RECVBUSY)
        return;         // read busy -> ignore, no echo
    if(c == 8)  // backspace
        uart.buffer_pop();
    else
        uart.buffer_push(c);
    UDR0 = c;           // echo back
    uart.checkEvents(); // direct execute the callback function, NOTICE: not execute in the eventloop!
}

int main()
{
    // initialize uart
    UBRR0H = (uint8_t)(BAUD>>8);    // set baud rate
    UBRR0L = (uint8_t)(BAUD);
    UCSR0C = (1<<UCSZ01)|(1<<UCSZ00);   // asynchronous mode, no parity, 1 stop bit, 8 bit data
    UCSR0B = (1<<RXEN0)|(1<<TXEN0)|(1<<RXCIE0); // enable receiver and transmitter and receive complete interrupt

    uart.onData = [](PipeIO<uart_send_byte>* self, char* prev)  // onData callback of uart receive
    {
        char *buffer = self->buffer();
        uint8_t length = self->length();
        for(auto i=prev; i<buffer+length; i++)  // send the content recieved since last time
        {
            if(*i == '\0')
                break;
            uart_send_byte(*i);
        }
    };

    uart.onFull = [](PipeIO<uart_send_byte>* self)  // onFull callback of uart recieve
    {
        uart << "full\r\n";
        self->buffer_clear();
    };

    // the pipeio supports output streaming operator for some simple types
    uart << "Hello world! " << (int32_t)114514 << (float)1919.810 << " " << (void*)uart_send_byte << "\r\n";
    return 0;
}
