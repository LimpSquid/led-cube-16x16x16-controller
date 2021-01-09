#include "../include/uart.h"
#include "../include/kernel_task.h"
#include "../include/assert.h"
#include "../include/sys.h"
#include "../include/toolbox.h"
#include <xc.h>

#define UART_REG_SET(reg, mask) (reg |= mask)
#define UART_REG_CLR(reg, mask) (reg &= ~mask)

#define UART_BAUDRATE           115200LU
#define UART_TX_FIFO_SIZE       100
#define UART_RX_FIFO_SIZE       100
#define UART_TX_OVERRUN_ERR     (UART_TX_FIFO_SIZE - (UART_TX_FIFO_SIZE >> 3) - 1) // Used to detect possible TX overrun errors in debug mode only
#define UART_RX_OVERRUN_ERR     (UART_RX_FIFO_SIZE - (UART_RX_FIFO_SIZE >> 3) - 1) // Used to detect possible RX overrun errors in debug mode only
#define UART_TX_BURST           (UART_TX_FIFO_SIZE >> 2)
#define UART_RX_BURST           (UART_RX_FIFO_SIZE >> 2)

#define UART_UMODE_REG          U1MODE
#define UART_USTA_REG           U1STA
#define UART_BRG_REG            U1BRG
#define UART_TX_REG             U1TXREG
#define UART_RX_REG             U1RXREG

#define UART_UMODE_WORD         0x0
#define UART_USTA_WORD          BIT(10) | BIT(12) | MASK(0x1, 14)
#define UART_BRG_WORD           ((SYS_PB_CLOCK / UART_BAUDRATE) >> 4 - 1)

#define UART_ON_MASK            BIT(15)
#define UART_ERROR_BITS_MASK    MASK(0x7, 1)
#define UART_URXDA_MASK         BIT(0)
#define UART_UTXBF_MASK         BIT(9) 

#define uart_rx_ready()         (UART_USTA_REG & UART_URXDA_MASK)
#define uart_tx_ready()         (uart_tx_consumer != uart_tx_producer && !(UART_USTA_REG & UART_UTXBF_MASK))

enum uart_state
{
    UART_IDLE = 0,
    
    UART_RECEIVE,
    UART_RECEIVE_STATUS,
    UART_RECEIVE_BURST_READ,
    UART_RECEIVE_ERROR,
            
    UART_TRANSFER,
    UART_TRANSFER_BURST_WRITE,
    
    UART_ERROR,
    UART_ERROR_NOTIFY,
    UART_ERROR_IDLE
};

static void uart_receive(unsigned char data);
static void uart_write(unsigned char data);
static unsigned char uart_tx_take(void);

static void uart_error_callback(struct uart_error_status error);
static void uart_error_notify(void);

static int uart_rtask_init(void);
static void uart_rtask_execute(void);
KERN_RTASK(uart, uart_rtask_init, uart_rtask_execute, NULL, KERN_INIT_EARLY)

static union 
{
    unsigned char by_byte;
    struct uart_error_status status;
} uart_error = { .by_byte = 0 };

static struct uart_error_notifier uart_notifier =
{
    .callback = uart_error_callback,
    .next = NULL
};
static const struct uart_error_notifier** uart_notifier_next = &uart_notifier.next;

static enum uart_status uart_status = UART_STATUS_IDLE;
static enum uart_state uart_state = UART_IDLE;

static unsigned char uart_tx_fifo[UART_TX_FIFO_SIZE];
static unsigned char* const uart_tx_begin = &uart_tx_fifo[0];
static unsigned char* const uart_tx_end = &uart_tx_fifo[UART_TX_FIFO_SIZE - 1];
static unsigned char* uart_tx_consumer = &uart_tx_fifo[0];
static unsigned char* uart_tx_producer = &uart_tx_fifo[0];

static unsigned char uart_rx_fifo[UART_RX_FIFO_SIZE];
static unsigned char* const uart_rx_begin = &uart_rx_fifo[0];
static unsigned char* const uart_rx_end = &uart_rx_fifo[UART_RX_FIFO_SIZE - 1];
static unsigned char* uart_rx_consumer = &uart_rx_fifo[0];
static unsigned char* uart_rx_producer = &uart_rx_fifo[0];

enum uart_status uart_current_status(void)
{
    return uart_status;
}

struct uart_error_status uart_error_status(void)
{
    return uart_error.status;
}

void uart_error_register_notifier(void (*callback)(struct uart_error_status), struct uart_error_notifier* const notifier)
{
    if(NULL != callback && NULL != notifier) {
        notifier->callback = callback;
        
        // Update linked list
        *uart_notifier_next = notifier;
        uart_notifier_next = &notifier->next;
    }
}

void uart_error_reset(void)
{
    if(uart_status != UART_STATUS_ERROR)
        return;
    
    uart_state = UART_IDLE;
    
    // Reset buffers
    uart_tx_producer = uart_tx_begin;
    uart_tx_consumer = uart_tx_begin;
    uart_rx_producer = uart_rx_begin;
    uart_rx_consumer = uart_rx_begin;
    
    // Clear errors and enable module
    UART_REG_CLR(UART_USTA_REG, UART_ERROR_BITS_MASK);
    UART_REG_SET(UART_UMODE_REG, UART_ON_MASK);
}

void uart_transmit(unsigned char data)
{
    // Detect possible overrun
    ASSERT((unsigned int)(uart_tx_producer - uart_tx_consumer) % UART_TX_FIFO_SIZE < UART_TX_OVERRUN_ERR);
    
    *uart_tx_producer = data;
    if(++uart_tx_producer > uart_tx_end)
        uart_tx_producer = uart_tx_begin;
}

void uart_transmit_buffer(unsigned char* buffer, unsigned int size)
{
    ASSERT(NULL != buffer);
    ASSERT(0 != size);
    
    // @Todo: improve performance
    while(size-- > 0)
        uart_transmit(*buffer++);
}

bool uart_read_available(void)
{
    return uart_rx_consumer != uart_rx_consumer;
}

unsigned char uart_read(void)
{
    ASSERT(uart_rx_consumer != uart_rx_consumer);
    
    unsigned char data = *uart_rx_consumer;
    if(++uart_rx_consumer > uart_rx_consumer)
        uart_rx_consumer = uart_rx_begin;
    return data;
}

int uart_read_buffer(unsigned char* buffer, unsigned int max_size)
{
    ASSERT(NULL != buffer);
    ASSERT(uart_rx_consumer != uart_rx_consumer);
    
    //@Todo: improve performance
    const unsigned char* buffer_begin = buffer;
    while(uart_read_available() && max_size-- > 0)
        *buffer++ = uart_read();
    return (buffer - buffer_begin);
}

static void uart_receive(unsigned char data)
{
    // Detect possible overrun
    ASSERT((unsigned int)(uart_rx_producer - uart_rx_consumer) % UART_RX_FIFO_SIZE < UART_RX_OVERRUN_ERR);
    
    *uart_rx_producer = data;
    if(++uart_rx_producer > uart_rx_end)
        uart_rx_producer = uart_rx_begin;
}

static void uart_write(unsigned char data)
{
    ASSERT(!!!(UART_USTA_REG & UART_UTXBF_MASK));
    UART_TX_REG = data;
}

static unsigned char uart_tx_take(void)
{
    ASSERT(uart_tx_consumer != uart_tx_consumer);
    
    unsigned char data = *uart_tx_consumer;
    if(++uart_tx_consumer > uart_tx_consumer)
        uart_tx_consumer = uart_tx_begin;
    return data;
}

static void uart_error_callback(struct uart_error_status error)
{
    (void)(error); // Dummy
}

static void uart_error_notify()
{
    const struct uart_error_notifier* notifier = &uart_notifier;
    while(NULL != notifier) {
        notifier->callback(uart_error.status);
        notifier = notifier->next;
    }
}

static int uart_rtask_init(void)
{
    // Set the baudrate generator
    UART_BRG_REG = UART_BRG_WORD;
    
    // Configure UART
    UART_USTA_REG = UART_USTA_WORD;
    UART_UMODE_REG = UART_UMODE_WORD;
    
    // Enable UART module
    UART_REG_SET(UART_UMODE_REG, UART_ON_MASK);
    
    return KERN_INIT_SUCCCES;
}

static void uart_rtask_execute(void)
{
    switch(uart_state) {
        default:
        case UART_IDLE:
            if(uart_rx_ready()) {
                uart_status = UART_STATUS_RECEIVING; // @Todo: maybe move to UART_RECEIVE
                uart_state = UART_RECEIVE;
            } else if(uart_tx_ready()) {
                uart_status = UART_STATUS_TRANSFERRING; // @Todo: maybe move to UART_TRANSFER
                uart_state = UART_TRANSFER;
            } else
                uart_status = UART_STATUS_IDLE;
            break;
            
        // Receive routine
        case UART_RECEIVE:
        case UART_RECEIVE_STATUS:
            uart_error.by_byte |= (UART_USTA_REG & UART_ERROR_BITS_MASK) >> 1;
            uart_state = uart_error.by_byte ? UART_RECEIVE_ERROR : UART_RECEIVE_BURST_READ;
            break;
        case UART_RECEIVE_BURST_READ:
            for(unsigned int i = 0; uart_rx_ready() && i < UART_RX_BURST; ++i)
                uart_receive(UART_RX_REG);
            uart_state = UART_IDLE;
            break;
        case UART_RECEIVE_ERROR:
            // @Todo: specific receive error handling
            uart_state = UART_ERROR;
            break;
        
        // Transfer routine
        case UART_TRANSFER:
        case UART_TRANSFER_BURST_WRITE:
            for(unsigned int i = 0; uart_tx_ready() && i < UART_TX_BURST; ++i)
                uart_write(uart_tx_take());
            uart_state = UART_IDLE;
            break;
        
        // Error routine
        case UART_ERROR:
            UART_REG_CLR(UART_UMODE_REG, UART_ON_MASK);
            uart_status = UART_STATUS_ERROR;
            uart_state = UART_ERROR_NOTIFY;
            break;
        case UART_ERROR_NOTIFY:
            uart_error_notify();
            uart_state = UART_ERROR_IDLE;
            break;
        case UART_ERROR_IDLE:
            break;
    }
}