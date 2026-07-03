/*
 * CH9434 UART high-level API - implementation.
 *
 * Chip model: CH9434A timing (CH9434M is functionally equivalent).
 * Reference clock = 32 MHz by default. Divisor is calculated per the
 * WCH formula: DLL_DLM = sys_frequency / 8 / baud (the chip divides
 * the reference clock by 8 internally before applying the 16-bit
 * divisor).
 */
#include <string.h>
#include "esp_log.h"
#include "rom/ets_sys.h"
#include "ch9434_uart.h"
#include "ch9434_drv.h"
#include "ch9434_spi.h"

#define TAG "ch9434_uart"

/* -------------------------------------------------------------------------- */
/*                          Chip bring-up                                      */
/* -------------------------------------------------------------------------- */

esp_err_t ch9434_chip_init(void)
{
    esp_err_t ret = ch9434_spi_bus_init();
    if (ret != ESP_OK) {
        return ret;
    }

    /* WCH debugging note: after power-on the chip needs time to fully
     * reset before any register access, otherwise the clock config
     * (and subsequent UART settings) won't stick. */
    ets_delay_us(100000);  /* 100 ms */

    /* (Clock config left at the chip's default for now; we'll re-enable
     * the multiplier once basic SPI comms are confirmed working.) */
    ret = ch9434_write_clk_ctrl(0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to write CLK_CTRL_CFG");
        return ret;
    }
    ets_delay_us(50000);

    /* Quick SPI sanity test: write 0x55 to UART0 SCR and read it back. */
    ret = ch9434_uart_write_scr(CH9434_UART0, 0x55);
    if (ret != ESP_OK) {
        return ret;
    }
    uint8_t v = 0;
    ret = ch9434_uart_read_scr(CH9434_UART0, &v);
    if (ret != ESP_OK) {
        return ret;
    }
    if (v != 0x55) {
        ESP_LOGE(TAG, "SCR loopback test FAILED: got 0x%02X, expected 0x55", v);
        return ESP_FAIL;
    }

    /* Try a different value to be sure. */
    ret = ch9434_uart_write_scr(CH9434_UART0, 0xA3);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = ch9434_uart_read_scr(CH9434_UART0, &v);
    if (ret != ESP_OK) {
        return ret;
    }
    if (v != 0xA3) {
        ESP_LOGE(TAG, "SCR loopback test FAILED: got 0x%02X, expected 0xA3", v);
        return ESP_FAIL;
    }

    /* Drain any garbage the SCR writes may have left in the RX FIFO
     * (e.g. stale data from chip startup or from SCR writes that the
     * chip accidentally echoed into the FIFO). */
    for (int u = 0; u < CH9434_UART_COUNT; u++) {
        uint8_t junk[64];
        uint16_t got = 0;
        ch9434_uart_read(u, junk, sizeof(junk), &got);
        ESP_LOGI(TAG, "Drained UART%d: %u bytes", u, got);
    }

    ESP_LOGI(TAG, "CH9434 communication OK (SCR test passed)");
    return ESP_OK;
}

/* -------------------------------------------------------------------------- */
/*                          Per-UART configuration                             */
/* -------------------------------------------------------------------------- */

esp_err_t ch9434_uart_set_config(uint8_t uart, const ch9434_uart_config_t *cfg)
{
    if (uart >= CH9434_UART_COUNT || cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Disable all interrupts while we reconfigure. */
    esp_err_t ret = ch9434_write_reg(CH9434_ADDR_IER(uart), 0x00);
    if (ret != ESP_OK) {
        return ret;
    }

    /* Build the LCR value. */
    uint8_t lcr = 0;
    switch (cfg->data_bits) {
    case CH9434_DATABITS_5: lcr |= CH9434_LCR_WLS_5; break;
    case CH9434_DATABITS_6: lcr |= CH9434_LCR_WLS_6; break;
    case CH9434_DATABITS_7: lcr |= CH9434_LCR_WLS_7; break;
    case CH9434_DATABITS_8: lcr |= CH9434_LCR_WLS_8; break;
    default:                lcr |= CH9434_LCR_WLS_8; break;
    }
    if (cfg->stop_bits == CH9434_STOPBITS_2) {
        lcr |= CH9434_LCR_STOP;
    }
    switch (cfg->parity) {
    case CH9434_PARITY_ODD:  lcr |= CH9434_LCR_PARITY_EN;                 break;
    case CH9434_PARITY_EVEN: lcr |= CH9434_LCR_PARITY_EN | CH9434_LCR_PARITY_EVEN; break;
    case CH9434_PARITY_NONE: default: break;
    }

    /* Set DLAB to access DLL/DLM. */
    ret = ch9434_write_reg(CH9434_ADDR_LCR(uart), (uint8_t)(lcr | CH9434_UARTx_BIT_DLAB));
    if (ret != ESP_OK) {
        return ret;
    }

    /* Divisor = sys_frequency / 8 / baud.  Use rounding. */
    uint32_t div = (10UL * CH9434_SYS_FREQ_HZ / 8U / (uint32_t)cfg->baud + 5U) / 10U;
    if (div == 0) {
        div = 1;
    }
    if (div > 0xFFFF) {
        div = 0xFFFF;
    }
    ret = ch9434_uart_write_dll(uart, (uint8_t)(div & 0xFF));
    if (ret != ESP_OK) {
        return ret;
    }
    ret = ch9434_uart_write_dlm(uart, (uint8_t)((div >> 8) & 0xFF));
    if (ret != ESP_OK) {
        return ret;
    }

    /* Clear DLAB and write final LCR. */
    ret = ch9434_write_reg(CH9434_ADDR_LCR(uart), lcr);
    if (ret != ESP_OK) {
        return ret;
    }

    /* Reset and enable the FIFO with a moderate trigger level. */
    uint8_t fcr = CH9434_FCR_RX_RESET | CH9434_FCR_TX_RESET;
    if (cfg->use_fifo) {
        fcr |= CH9434_FCR_ENABLE | CH9434_FCR_TRIG_8;
    }
    ret = ch9434_write_reg(CH9434_ADDR_FCR(uart), fcr);
    if (ret != ESP_OK) {
        return ret;
    }
    /* WCH note: after FIFO reset bits, wait 50us before next operation. */
    ets_delay_us(50);

    /* MCR: enable OUT2 so the IRQ pin (if used) is gated properly, set RTS/DTR high. */
    uint8_t mcr = CH9434_MCR_RTS | CH9434_MCR_DTR | CH9434_MCR_OUT2;
    if (cfg->hw_flow_ctrl) {
        mcr |= CH9434_MCR_AFE;
    }
    ret = ch9434_write_reg(CH9434_ADDR_MCR(uart), mcr);
    if (ret != ESP_OK) {
        return ret;
    }

    /* Enable RX interrupts so the chip can flag incoming data. */
    ret = ch9434_write_reg(CH9434_ADDR_IER(uart), CH9434_IER_RX);
    if (ret != ESP_OK) {
        return ret;
    }

    ESP_LOGI(TAG, "UART%u configured: %u bps, %u%c%u (div=%lu, sys=%lu Hz)",
             uart,
             (unsigned)cfg->baud,
             (unsigned)cfg->data_bits,
             (cfg->parity == CH9434_PARITY_NONE) ? 'N' :
             (cfg->parity == CH9434_PARITY_ODD)  ? 'O' : 'E',
             (cfg->stop_bits == CH9434_STOPBITS_1) ? 1 : 2,
             (unsigned long)div,
             (unsigned long)CH9434_SYS_FREQ_HZ);

    return ESP_OK;
}

/* -------------------------------------------------------------------------- */
/*                          Read / write helpers                               */
/* -------------------------------------------------------------------------- */

esp_err_t ch9434_uart_write(uint8_t uart, const uint8_t *data, uint16_t len)
{
    if (uart >= CH9434_UART_COUNT || (data == NULL && len > 0)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (len == 0) {
        return ESP_OK;
    }

    /* Bulk write to THR; the CH9434 chip will accept up to its TX FIFO
     * (1536 bytes for CH9434A) per write. We split into 128-byte chunks
     * to keep SPI transactions short and let the FIFO drain. */
    uint16_t offset = 0;
    while (offset < len) {
        uint16_t chunk = len - offset;
        if (chunk > 128) {
            chunk = 128;
        }
        esp_err_t ret = ch9434_uart_write_fifo(uart, &data[offset], chunk);
        if (ret != ESP_OK) {
            return ret;
        }
        offset = (uint16_t)(offset + chunk);
    }
    return ESP_OK;
}

esp_err_t ch9434_uart_read(uint8_t uart, uint8_t *data, uint16_t max_len, uint16_t *out_len)
{
    if (uart >= CH9434_UART_COUNT || data == NULL || out_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_len = 0;
    if (max_len == 0) {
        return ESP_OK;
    }

    /* Check how many bytes are waiting. */
    uint8_t lo = 0, hi = 0;
    esp_err_t ret = ch9434_uart_get_rx_fifo_len(uart, &lo, &hi);
    if (ret != ESP_OK) {
        return ret;
    }
    uint16_t available = (uint16_t)((hi << 8) | lo);
    if (available == 0) {
        return ESP_OK;
    }
    if (available > max_len) {
        available = max_len;
    }

    ret = ch9434_uart_read_fifo(uart, data, available);
    if (ret != ESP_OK) {
        return ret;
    }
    *out_len = available;
    return ESP_OK;
}

esp_err_t ch9434_uart_available(uint8_t uart, uint16_t *count)
{
    if (uart >= CH9434_UART_COUNT || count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t lo = 0, hi = 0;
    esp_err_t ret = ch9434_uart_get_rx_fifo_len(uart, &lo, &hi);
    if (ret != ESP_OK) {
        return ret;
    }
    *count = (uint16_t)((hi << 8) | lo);
    return ESP_OK;
}

void ch9434_uart_dump_lsr(uint8_t uart, uint8_t lsr)
{
    ESP_LOGW(TAG, "UART%u LSR=0x%02X%s%s%s%s%s%s%s",
             uart, lsr,
             (lsr & CH9434_LSR_DR)   ? " DR"   : "",
             (lsr & CH9434_LSR_OE)   ? " OE"   : "",
             (lsr & CH9434_LSR_PE)   ? " PE"   : "",
             (lsr & CH9434_LSR_FE)   ? " FE"   : "",
             (lsr & CH9434_LSR_BI)   ? " BI"   : "",
             (lsr & CH9434_LSR_THRE) ? " THRE" : "",
             (lsr & CH9434_LSR_TEMT) ? " TEMT" : "",
             (lsr & CH9434_LSR_FERR) ? " FERR" : "");
}
