/*
 * Bitbanged SPI master for the CH9434. Used as a fallback when the
 * ESP-IDF SPI master driver produces unreliable reads.
 *
 * Pins: MOSI=11, MISO=13, SCK=12, CS=10.
 */
#ifndef CH9434_SPI_BITBANG_H
#define CH9434_SPI_BITBANG_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ch9434_bb_spi_bus_init(void);
void ch9434_bb_spi_bus_deinit(void);

esp_err_t ch9434_bb_spi_write_reg(uint8_t reg, uint8_t val);
esp_err_t ch9434_bb_spi_read_reg(uint8_t reg, uint8_t *val);
esp_err_t ch9434_bb_spi_write_bytes(uint8_t reg, const uint8_t *data, uint16_t len);
esp_err_t ch9434_bb_spi_read_bytes(uint8_t reg, uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* CH9434_SPI_BITBANG_H */
