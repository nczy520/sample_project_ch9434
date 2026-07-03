# CH9434 SPI-to-4xUART Driver for ESP32-S3

基于 ESP-IDF 6.0.2 的 CH9434 芯片驱动，实现通过 SPI 总线扩展 4 路 UART 串口。

## 目录

- [项目概述](#项目概述)
- [硬件连接](#硬件连接)
- [SPI 接口使用](#spi-接口使用)
- [UART 接口使用](#uart-接口使用)
- [测试说明](#测试说明)
- [编译与烧录](#编译与烧录)
- [API 参考](#api-参考)

## 项目概述

CH9434 是 WCH（沁恒微电子）推出的一款通过 SPI 接口扩展出 4 路独立 UART 的芯片。本项目提供了完整的驱动实现，包括：

- **SPI 总线层**：处理 SPI 时序和寄存器读写
- **寄存器层**：CH9434 寄存器映射和访问封装
- **UART API 层**：高层串口配置和数据收发接口
- **测试应用**：端到端测试验证全部功能

### 项目结构

```
main/
├── ch9434_spi.c/h      # SPI 总线驱动
├── ch9434_regs.h       # 寄存器定义
├── ch9434_drv.c/h      # 寄存器访问层
├── ch9434_uart.c/h     # UART 高层 API
├── test_app.c/h        # 测试应用
└── main.c              # 应用入口
```

## 硬件连接

### ESP32-S3 与 CH9434 SPI 连接

| ESP32-S3 引脚 | CH9434 引脚 | 功能 |
|--------------|-------------|------|
| GPIO 11      | MOSI        | SPI 主出从入 |
| GPIO 13      | MISO        | SPI 主入从出 |
| GPIO 12      | SCK         | SPI 时钟 |
| GPIO 10      | CS          | SPI 片选 |
| 3.3V         | VCC         | 电源（3.3V） |
| GND          | GND         | 地线 |

**注意**：CH9434 的 SPI 时钟频率建议不超过 250kHz，本项目默认使用 200kHz。

### CH9434 UART 引脚

CH9434 提供 4 路独立 UART，每路包含 TX、RX、RTS、CTS 引脚：

| UART 编号 | TX 引脚 | RX 引脚 | RTS 引脚 | CTS 引脚 |
|-----------|---------|---------|----------|----------|
| UART0     | TX0     | RX0     | RTS0     | CTS0     |
| UART1     | TX1     | RX1     | RTS1     | CTS1     |
| UART2     | TX2     | RX2     | RTS2     | CTS2     |
| UART3     | TX3     | RX3     | RTS3     | CTS3     |

## SPI 接口使用

### SPI 初始化

```c
#include "ch9434_spi.h"

esp_err_t ret = ch9434_spi_bus_init();
if (ret != ESP_OK) {
    // SPI 总线初始化失败
    return ret;
}
```

### 寄存器读写

```c
// 写入单个寄存器
esp_err_t ret = ch9434_spi_write_reg(reg_addr, value);

// 读取单个寄存器
uint8_t value;
esp_err_t ret = ch9434_spi_read_reg(reg_addr, &value);

// 批量写入（FIFO 写入）
esp_err_t ret = ch9434_spi_write_bytes(reg_addr, data, len);

// 批量读取（FIFO 读取）
esp_err_t ret = ch9434_spi_read_bytes(reg_addr, data, len);
```

### SPI 时序说明

CH9434 采用特殊的 SPI 时序：

- **写操作**：CS 低 → 地址字节 → 1us → 数据字节 → 3us → CS 高
- **读操作**：CS 低 → 地址字节 → 3us → 虚字节（读取数据）→ 1us → CS 高
- 每个字节传输都是独立的 CS 脉冲

## UART 接口使用

### 芯片初始化

```c
#include "ch9434_uart.h"

esp_err_t ret = ch9434_chip_init();
if (ret != ESP_OK) {
    // 芯片初始化失败，检查 SPI 连接
    return ret;
}
```

芯片初始化会自动完成：
1. SPI 总线初始化
2. 上电延迟（100ms）
3. 时钟控制寄存器配置
4. SCR 寄存器回环校验

### UART 配置

```c
ch9434_uart_config_t cfg = {
    .baud          = CH9434_BAUD_115200,  // 波特率
    .data_bits     = CH9434_DATABITS_8,   // 数据位
    .stop_bits     = CH9434_STOPBITS_1,   // 停止位
    .parity        = CH9434_PARITY_NONE,  // 校验位
    .use_fifo      = true,                // 启用 FIFO
    .hw_flow_ctrl  = false,               // 硬件流控
};

// 配置 UART0
esp_err_t ret = ch9434_uart_set_config(CH9434_UART0, &cfg);
```

#### 支持的配置选项

**波特率**：1200、2400、4800、9600、19200、38400、57600、115200、230400、460800、921600

**数据位**：5、6、7、8

**停止位**：1、2

**校验位**：无校验、奇校验、偶校验

### UART 数据收发

```c
// 发送数据
uint8_t tx_data[] = "Hello, CH9434!";
esp_err_t ret = ch9434_uart_write(CH9434_UART0, tx_data, sizeof(tx_data));

// 查询可用数据
uint16_t available;
ret = ch9434_uart_available(CH9434_UART0, &available);

// 读取数据
uint8_t rx_data[256];
uint16_t read_len;
ret = ch9434_uart_read(CH9434_UART0, rx_data, sizeof(rx_data), &read_len);
```

### 完整示例

```c
#include "ch9434_uart.h"

void app_main(void) {
    // 初始化芯片
    esp_err_t ret = ch9434_chip_init();
    if (ret != ESP_OK) {
        ESP_LOGE("app", "CH9434 init failed: %s", esp_err_to_name(ret));
        return;
    }

    // 配置 UART0（115200 8N1）
    ch9434_uart_config_t cfg = {
        .baud         = CH9434_BAUD_115200,
        .data_bits    = CH9434_DATABITS_8,
        .stop_bits    = CH9434_STOPBITS_1,
        .parity       = CH9434_PARITY_NONE,
        .use_fifo     = true,
        .hw_flow_ctrl = false,
    };
    ret = ch9434_uart_set_config(CH9434_UART0, &cfg);
    if (ret != ESP_OK) {
        ESP_LOGE("app", "UART0 config failed: %s", esp_err_to_name(ret));
        return;
    }

    // 发送数据
    uint8_t msg[] = "Hello from CH9434 UART0!\n";
    ch9434_uart_write(CH9434_UART0, msg, sizeof(msg));

    // 读取数据（假设 TX0 接 RX0 回环）
    uint8_t rx_buf[256];
    uint16_t rx_len;
    vTaskDelay(pdMS_TO_TICKS(100));
    ch9434_uart_read(CH9434_UART0, rx_buf, sizeof(rx_buf), &rx_len);
    
    ESP_LOGI("app", "Received %d bytes: %s", rx_len, rx_buf);
}
```

## 测试说明

### 测试连接

测试需要以下硬件连接：

| 连接 | 说明 |
|------|------|
| TX0 ↔ RX0 | UART0 回环测试 |
| TX1 → RX2 | UART1→UART2 交叉测试 |
| TX2 → RX1 | UART2→UART1 交叉测试 |

### 测试内容

- **UART0 回环测试**：发送随机长度（1-255字节）随机数据，验证回环接收
- **UART1→UART2 交叉测试**：UART1 发送，UART2 接收
- **UART2→UART1 交叉测试**：UART2 发送，UART1 接收

测试共运行 100 组，每组包含上述 3 个测试。

### 测试输出示例

```
====== CH9434 SPI->4xUART TEST START ======
Test configuration:
  - UART0: TX0 <-> RX0 (loopback)
  - UART1: TX1 -> UART2: RX2 (cross-link)
  - UART2: TX2 -> UART1: RX1 (cross-link reverse)
  - Baud rate: 115200 8N1
  - SPI clock: 200kHz
  - Test groups: 100
  - Random data length: 1-255 bytes per test
---------------------------------------------
====== TEST COMPLETE ======
Summary:
  Total groups:    100
  Passed:          100
  Failed:            0
  Pass rate:       100%
---------------------------------------------
RESULT: ALL TESTS PASSED ✓
```

## 编译与烧录

### 环境要求

- ESP-IDF 6.0.2
- Python 3.8+
- esptool.py

### 编译

```bash
cd sample_project_ch9434
source ~/.espressif/tools/activate_idf_v6.0.2.sh
idf.py build
```

### 烧录

```bash
idf.py -p /dev/tty.usbmodem14A01 flash
```

### 串口监视

```bash
idf.py -p /dev/tty.usbmodem14A01 monitor
```

按 `Ctrl+]` 退出串口监视。

## API 参考

### SPI 层（ch9434_spi.h）

| 函数 | 说明 |
|------|------|
| `ch9434_spi_bus_init()` | 初始化 SPI 总线 |
| `ch9434_spi_bus_deinit()` | 反初始化 SPI 总线 |
| `ch9434_spi_write_reg(reg, val)` | 写入单个寄存器 |
| `ch9434_spi_read_reg(reg, val)` | 读取单个寄存器 |
| `ch9434_spi_write_bytes(reg, data, len)` | 批量写入 |
| `ch9434_spi_read_bytes(reg, data, len)` | 批量读取 |

### UART 层（ch9434_uart.h）

| 函数 | 说明 |
|------|------|
| `ch9434_chip_init()` | 初始化 CH9434 芯片 |
| `ch9434_uart_set_config(uart, cfg)` | 配置指定 UART |
| `ch9434_uart_write(uart, data, len)` | 发送数据 |
| `ch9434_uart_read(uart, data, max_len, out_len)` | 读取数据 |
| `ch9434_uart_available(uart, count)` | 查询可用数据量 |
| `ch9434_uart_dump_lsr(uart, lsr)` | 打印 LSR 寄存器（调试用） |

### 寄存器层（ch9434_drv.h）

| 函数 | 说明 |
|------|------|
| `ch9434_write_reg(addr, val)` | 底层寄存器写入 |
| `ch9434_read_reg(addr, val)` | 底层寄存器读取 |
| `ch9434_modify_reg(addr, clear_mask, set_mask)` | 寄存器位修改 |
| `ch9434_uart_write_fifo(uart, data, len)` | 写入 TX FIFO |
| `ch9434_uart_read_fifo(uart, data, len)` | 读取 RX FIFO |

## 技术参数

- **目标芯片**：ESP32-S3
- **CH9434 时钟**：32MHz（默认配置）
- **SPI 时钟**：200kHz
- **UART FIFO 大小**：256 字节（每通道）
- **支持波特率**：1200 ~ 921600 bps

## 许可证

本项目基于 ESP-IDF 开源框架，遵循 Apache 2.0 许可证。