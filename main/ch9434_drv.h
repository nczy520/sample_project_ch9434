/*
 * CH9434 low-level register read/write helpers.
 *
 * These wrap the SPI bus primitives with named per-UART register accessors
 * to keep the call sites readable.
 */
#ifndef CH9434_DRV_H
#define CH9434_DRV_H

#include <stdint.h>
#include "esp_err.h"
#include "ch9434_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Single-byte register helpers (full address passed in). */
esp_err_t ch9434_write_reg(uint8_t addr, uint8_t val);
esp_err_t ch9434_read_reg(uint8_t addr, uint8_t *val);

/* Modify a register (read-modify-write) keeping the listed bits intact. */
esp_err_t ch9434_modify_reg(uint8_t addr, uint8_t clear_mask, uint8_t set_mask);

/* Per-UART convenience accessors. */
esp_err_t ch9434_uart_write_scr(uint8_t uart, uint8_t val);
esp_err_t ch9434_uart_read_scr(uint8_t uart, uint8_t *val);

esp_err_t ch9434_uart_write_lcr(uint8_t uart, uint8_t val);
esp_err_t ch9434_uart_read_lcr(uint8_t uart, uint8_t *val);

esp_err_t ch9434_uart_write_mcr(uint8_t uart, uint8_t val);
esp_err_t ch9434_uart_read_mcr(uint8_t uart, uint8_t *val);

esp_err_t ch9434_uart_read_lsr(uint8_t uart, uint8_t *val);
esp_err_t ch9434_uart_read_iir(uint8_t uart, uint8_t *val);

esp_err_t ch9434_uart_write_dll(uint8_t uart, uint8_t val);
esp_err_t ch9434_uart_write_dlm(uint8_t uart, uint8_t val);
esp_err_t ch9434_uart_read_dll(uint8_t uart, uint8_t *val);
esp_err_t ch9434_uart_read_dlm(uint8_t uart, uint8_t *val);

/* FIFO count for the given UART (read-only). The CH9434 splits the count
 * across two consecutive 8-bit registers (low / high). */
esp_err_t ch9434_uart_get_rx_fifo_len(uint8_t uart, uint8_t *len_lo, uint8_t *len_hi);
esp_err_t ch9434_uart_get_tx_fifo_len(uint8_t uart, uint8_t *len_lo, uint8_t *len_hi);

/* FIFO data IO. */
esp_err_t ch9434_uart_write_fifo(uint8_t uart, const uint8_t *data, uint16_t len);
esp_err_t ch9434_uart_read_fifo(uint8_t uart, uint8_t *data, uint16_t len);

/* Global clock control. */
esp_err_t ch9434_write_clk_ctrl(uint8_t val);

#ifdef __cplusplus
}
#endif

#endif /* CH9434_DRV_H */
