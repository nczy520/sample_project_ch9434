/*
 * SPI bus abstraction for CH9434.
 *
 * The CH9434 expects an SPI-mode-0 (CPOL=0, CPHA=0) master. Each CS-low
 * transaction carries one command byte and one data byte, with a >1us gap
 * between them. This module implements that timing using the ESP-IDF SPI
 * master driver.
 */
#ifndef CH9434_SPI_H
#define CH9434_SPI_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Default SPI clock for the CH9434 (200 kHz - safe for the chip's MISO
 * timing on ESP32-S3; the chip datasheet lists 16 MHz max but in
 * practice the MISO edge is too slow above ~250 kHz with this MCU's
 * SPI peripheral). */
#define CH9434_SPI_CLOCK_HZ                 (200 * 1000)

/**
 * Initialise the SPI bus used to talk to the CH9434.
 *
 * Pins used (ESP32-S3 default SPI2 mapping):
 *   MOSI = GPIO 11
 *   MISO = GPIO 13
 *   SCK  = GPIO 12
 *   CS   = GPIO 10
 *
 * @return ESP_OK on success, otherwise an esp_err_t.
 */
esp_err_t ch9434_spi_bus_init(void);

/**
 * Deinitialise the SPI bus. Safe to call multiple times.
 */
void ch9434_spi_bus_deinit(void);

/**
 * Write a single register byte.
 *
 * @param reg   8-bit register address (without the OP bit).
 * @param val   Value to write.
 * @return ESP_OK on success, otherwise an esp_err_t.
 */
esp_err_t ch9434_spi_write_reg(uint8_t reg, uint8_t val);

/**
 * Read a single register byte.
 *
 * @param reg   8-bit register address (without the OP bit).
 * @param val   Output pointer that receives the read value.
 * @return ESP_OK on success, otherwise an esp_err_t.
 */
esp_err_t ch9434_spi_read_reg(uint8_t reg, uint8_t *val);

/**
 * Write multiple bytes to a register (FIFO bulk write).
 *
 * @param reg      First register address (without the OP bit).
 * @param data     Pointer to bytes to send.
 * @param len      Number of bytes to send.
 * @return ESP_OK on success, otherwise an esp_err_t.
 */
esp_err_t ch9434_spi_write_bytes(uint8_t reg, const uint8_t *data, uint16_t len);

/**
 * Read multiple bytes from a register (FIFO bulk read).
 *
 * @param reg      Register address (without the OP bit).
 * @param data     Output buffer.
 * @param len      Number of bytes to read.
 * @return ESP_OK on success, otherwise an esp_err_t.
 */
esp_err_t ch9434_spi_read_bytes(uint8_t reg, uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* CH9434_SPI_H */
