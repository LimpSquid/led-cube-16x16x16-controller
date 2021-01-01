#ifndef UART_H
#define	UART_H

#include <stdbool.h>

enum uart_work_status
{
    UART_STATUS_IDLE = 0,
    UART_STATUS_TRANSFERRING,
    UART_STATUS_RECEIVING,
    UART_STATUS_ERROR
};

struct uart_error_status
{
    unsigned char perr :1;
    unsigned char ferr :1;
    unsigned char oerr :1;
    unsigned char _reserved :5;
};

struct uart_error_notifier
{
    void (*callback)(struct uart_error_status error);
    const struct uart_error_notifier* next;
};

enum uart_work_status uart_work_status(void);

struct uart_error_status uart_error_status(void);
void uart_error_register_notifier(void (*callback)(struct uart_error_status), struct uart_error_notifier* const notifier);
void uart_error_reset(void);

void uart_transmit(unsigned char data);
void uart_transmit_buffer(unsigned char* buffer, unsigned int size);

bool uart_read_available(void);
unsigned char uart_read(void);
int uart_read_buffer(unsigned char* buffer, unsigned int max_size);

#endif	/* UART_H */