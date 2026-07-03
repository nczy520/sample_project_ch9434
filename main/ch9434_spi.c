/*
 * CH9434 SPI low-level implementation.
 *
 * The CH9434A protocol uses 1 CS-low pulse per register access, with
 * 2 bytes inside that pulse (address byte, then 1us delay, then data
 * byte, then 3us delay, then CS high). For "bulk" FIFO transfers,
 * each byte is its OWN (CS-low) transaction - the WCH reference
 * driver loops ch9434_write_reg per byte.
 *
 *   WRITE: CS low, [addr], 1us, [data], 3us, CS high.
 *   READ : CS low, [addr], 3us, [0xFF dummy -> data on MISO], 1us, CS high.
 *
 * We send both bytes of a single access in ONE spi_device_transmit call
 * (2 bytes = 16 bits, CS held low for the whole transaction), then add
 * the required post-CS delay.
 */

#include <string.h>
#include "esp_log.h"
#include "driver/spi_master.h"
#include "rom/ets_sys.h"

#include "ch9434_spi.h"
#include "ch9434_regs.h"

#define TAG "ch9434_spi"

/* Pin assignment (ESP32-S3 default SPI2 pins) */
#define PIN_NUM_MOSI    11
#define PIN_NUM_MISO    13
#define PIN_NUM_SCK     12
#define PIN_NUM_CS      10

/* SPI host used on ESP32-S3. SPI0/1 are reserved for flash/PSRAM. */
#define SPI_HOST_CH9434 SPI2_HOST

/* Per-spec delays (microseconds). */
#define CH9434A_DELAY_ADDR_TO_DATA_US  1   /* WRITE: address -> data    */
#define CH9434A_DELAY_DATA_TO_CS_US     3   /* WRITE: data    -> CS high */
#define CH9434A_DELAY_READ_ADDR_US     3   /* READ:  address -> read    */
#define CH9434A_DELAY_READ_DONE_US     1   /* READ:  read    -> CS high */

static spi_device_handle_t s_dev = NULL;
static bool s_initialized = false;

/* -------------------------------------------------------------------------- */
/*                          SPI bus init / deinit                             */
/* -------------------------------------------------------------------------- */

esp_err_t ch9434_spi_bus_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = PIN_NUM_MOSI,
        .miso_io_num     = PIN_NUM_MISO,
        .sclk_io_num     = PIN_NUM_SCK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 16,
    };

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = CH9434_SPI_CLOCK_HZ,
        .mode           = 0,                   /* SPI mode 0 (CPOL=0, CPHA=0) per WCH EVT */
        .spics_io_num   = PIN_NUM_CS,
        .queue_size     = 4,
        .flags          = 0,
    };

    esp_err_t ret = spi_bus_initialize(SPI_HOST_CH9434, &bus_cfg, SPI_DMA_DISABLED);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = spi_bus_add_device(SPI_HOST_CH9434, &dev_cfg, &s_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_add_device failed: %s", esp_err_to_name(ret));
        spi_bus_free(SPI_HOST_CH9434);
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "CH9434 SPI bus ready (MOSI=%d MISO=%d SCK=%d CS=%d @%d Hz)",
             PIN_NUM_MOSI, PIN_NUM_MISO, PIN_NUM_SCK, PIN_NUM_CS, CH9434_SPI_CLOCK_HZ);
    return ESP_OK;
}

void ch9434_spi_bus_deinit(void)
{
    if (!s_initialized) {
        return;
    }
    spi_bus_remove_device(s_dev);
    spi_bus_free(SPI_HOST_CH9434);
    s_dev = NULL;
    s_initialized = false;
}

/* -------------------------------------------------------------------------- */
/*                       Single-byte register accesses                        */
/*                                                                            */
/* Each register access occupies ONE CS-low pulse containing 2 bytes:         */
/*   tx[0] = OP | reg_addr                                                   */
/*   tx[1] = data byte (or 0xFF for read)                                     */
/* We rely on the 8us-per-byte @1MHz clock to provide the required 1us / 3us  */
/* inter-byte gap, then explicitly add the post-CS delay.                    */
/* -------------------------------------------------------------------------- */

/* One CS-low pulse with 2 bytes: tx[0]=OP|reg, tx[1]=data/0xFF.
 * The first RX byte is junk (CH9434 didn't output anything in response to
 * the address), the second RX byte carries the read data. */
static esp_err_t ch9434_spi_xfer2(uint8_t op, uint8_t reg, uint8_t data_byte,
                                  uint8_t *rx_byte, uint8_t post_delay_us)
{
    uint8_t tx[2] = { (uint8_t)(op | reg), data_byte };
    uint8_t rx[2] = { 0, 0 };

    spi_transaction_t t = {
        .length    = 16,            /* 2 bytes = 16 bits, CS held low */
        .tx_buffer = tx,
        .rx_buffer = rx,
    };

    esp_err_t ret = spi_device_transmit(s_dev, &t);
    if (ret != ESP_OK) {
        return ret;
    }
    if (rx_byte) {
        *rx_byte = rx[1];
    }
    if (post_delay_us) {
        ets_delay_us(post_delay_us);
    }
    return ESP_OK;
}

esp_err_t ch9434_spi_write_reg(uint8_t reg, uint8_t val)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    /* WRITE: address | data, both in one CS-low pulse. Inter-byte delay
     * (1us) is provided by SPI clock time (8us per byte at 1 MHz).
     * Post-CS delay = 3us per WCH spec. */
    return ch9434_spi_xfer2(CH9434_REG_OP_WRITE, reg, val, NULL,
                            CH9434A_DELAY_DATA_TO_CS_US);
}

esp_err_t ch9434_spi_read_reg(uint8_t reg, uint8_t *val)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (val == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    /* READ: address byte then 0xFF dummy to clock out the data byte.
     * Inter-byte delay (3us) is provided by SPI clock time at 1 MHz.
     * Post-CS delay = 3us per WCH spec. */
    return ch9434_spi_xfer2(CH9434_REG_OP_READ, reg, 0xFF, val,
                            CH9434A_DELAY_DATA_TO_CS_US);
}

/* -------------------------------------------------------------------------- */
/*                          Bulk FIFO transfers                                */
/*                                                                            */
/* The CH9434A does not have a true burst mode; each byte of a FIFO is        */
/* transferred as a separate (CS-low) 2-byte (address+data) transaction.       */
/* -------------------------------------------------------------------------- */

esp_err_t ch9434_spi_write_bytes(uint8_t reg, const uint8_t *data, uint16_t len)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    for (uint16_t i = 0; i < len; i++) {
        esp_err_t ret = ch9434_spi_write_reg(reg, data[i]);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    return ESP_OK;
}

esp_err_t ch9434_spi_read_bytes(uint8_t reg, uint8_t *data, uint16_t len)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    for (uint16_t i = 0; i < len; i++) {
        esp_err_t ret = ch9434_spi_read_reg(reg, &data[i]);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    return ESP_OK;
}
