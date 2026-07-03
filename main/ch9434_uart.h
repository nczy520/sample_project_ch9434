/*
 * CH9434 UART high-level API: chip init, sub-UART configuration and IO.
 *
 * Chip model: CH9434A timing (CH9434M is a repackaged CH9434A).
 */
#ifndef CH9434_UART_H
#define CH9434_UART_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "ch9434_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CH9434A serial port reference clock with the chip's default clock
 * configuration (clk_ctrl = 0x00): sys_frequency = 32 MHz. */
#define CH9434_SYS_FREQ_HZ                  32000000UL

/* Public baud rate selection. Standard PC values. */
typedef enum {
    CH9434_BAUD_1200    = 1200,
    CH9434_BAUD_2400    = 2400,
    CH9434_BAUD_4800    = 4800,
    CH9434_BAUD_9600    = 9600,
    CH9434_BAUD_19200   = 19200,
    CH9434_BAUD_38400   = 38400,
    CH9434_BAUD_57600   = 57600,
    CH9434_BAUD_115200  = 115200,
    CH9434_BAUD_230400  = 230400,
    CH9434_BAUD_460800  = 460800,
    CH9434_BAUD_921600  = 921600,
} ch9434_baud_t;

typedef enum {
    CH9434_PARITY_NONE = 0,
    CH9434_PARITY_ODD,
    CH9434_PARITY_EVEN,
} ch9434_parity_t;

typedef enum {
    CH9434_STOPBITS_1 = 1,
    CH9434_STOPBITS_2 = 2,
} ch9434_stopbits_t;

typedef enum {
    CH9434_DATABITS_5 = 5,
    CH9434_DATABITS_6 = 6,
    CH9434_DATABITS_7 = 7,
    CH9434_DATABITS_8 = 8,
} ch9434_databits_t;

typedef struct {
    ch9434_baud_t     baud;
    ch9434_databits_t data_bits;
    ch9434_stopbits_t stop_bits;
    ch9434_parity_t   parity;
    bool              use_fifo;       /* enable 256-byte FIFO */
    bool              hw_flow_ctrl;   /* enable auto RTS/CTS (where wired) */
} ch9434_uart_config_t;

/**
 * Initialise the CH9434 chip.
 *
 * Performs the SPI bus init, then runs the recommended startup sequence
 * (clock-mode register write + delay) and validates communication by
 * writing 0x55 to the UART0 scratch register and reading it back.
 *
 * @return ESP_OK if SPI comms to CH9434 are working, otherwise error.
 */
esp_err_t ch9434_chip_init(void);

/**
 * Configure a sub-UART (0..3). Wakes the channel if it was in sleep mode.
 */
esp_err_t ch9434_uart_set_config(uint8_t uart, const ch9434_uart_config_t *cfg);

/**
 * Send bytes through the sub-UART. Splits the payload into chunks that
 * fit the TX FIFO.
 */
esp_err_t ch9434_uart_write(uint8_t uart, const uint8_t *data, uint16_t len);

/**
 * Read up to `max_len` bytes from the sub-UART RX FIFO. Returns the
 * number of bytes actually read.
 */
esp_err_t ch9434_uart_read(uint8_t uart, uint8_t *data, uint16_t max_len, uint16_t *out_len);

/**
 * Convenience: number of bytes available to read from the RX FIFO.
 */
esp_err_t ch9434_uart_available(uint8_t uart, uint16_t *count);

/**
 * Print the LSR register (useful for debugging framing errors etc).
 */
void ch9434_uart_dump_lsr(uint8_t uart, uint8_t lsr);

#ifdef __cplusplus
}
#endif

#endif /* CH9434_UART_H */
