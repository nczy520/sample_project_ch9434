# ch9434 - WCH CH9434 SPI 转 4 路 UART 桥接芯片 ESP-IDF 驱动

[![Component](https://img.shields.io/badge/ESP--IDF-6.0%2B-blue)](https://docs.espressif.com/projects/esp-idf/)
[![License](https://img.shields.io/badge/license-Apache--2.0-green)](LICENSE)
[![Target](https://img.shields.io/badge/target-ESP32--S3-orange)](https://www.espressif.com/en/products/socs/esp32-s3)

WCH（沁恒微电子）**CH9434** 芯片的 ESP-IDF 生产级驱动。该芯片通过一路 SPI 总线扩展出
**4 路独立 UART 通道**。驱动完全线程安全：由专用的 SPI 服务任务消费请求队列，任意数量的
FreeRTOS 任务可并发调用 API 而不会争用 SPI 外设。

- 支持芯片：**CH9434A** / **CH9434M** / **CH9434D**（协议层功能一致）
- 目标 MCU：**ESP32-S3**（使用 SPI2 或 SPI3 主机）
- SPI 时钟：最高 1 MHz（默认 200 kHz，保证 MISO 时序安全）
- UART 波特率：1200 - 921600 bps
- 每路 UART FIFO：256 字节（RX + TX）
- 并发模型：队列 + 服务任务（无互斥锁，无锁竞争）

## 目录

- [安装方式](#安装方式)
  - [方式 A：Git 子模块（本地项目推荐）](#方式-agit-子模块本地项目推荐)
  - [方式 B：ESP Component Manager（idf.py add-dependency）](#方式-besp-component-manageridf-py-add-dependency)
  - [方式 C：手动拷贝](#方式-c手动拷贝)
- [硬件连接](#硬件连接)
- [配置选项](#配置选项)
- [快速入门](#快速入门)
- [API 参考](#api-参考)
- [并发模型](#并发模型)
- [许可证](#许可证)

## 安装方式

### 方式 A：Git 子模块（本地项目推荐）

在你的 ESP-IDF 项目根目录执行：

```bash
git submodule add https://github.com/nczy520/esp-idf-ch9434.git components/ch9434
```

ESP-IDF 会自动发现 `components/` 目录下的组件，无需额外配置。在你的
`main/CMakeLists.txt` 中声明依赖：

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES ch9434
)
```

### 方式 B：ESP Component Manager（idf.py add-dependency）

在你需要的组件（如 `main/`）的 `idf_component.yml` 中添加：

```yaml
dependencies:
  ch9434:
    git: https://github.com/nczy520/esp-idf-ch9434.git
    path: components/ch9434
    version: main   # 也可指定 tag，如 v1.0.0
```

执行 `idf.py reconfigure`，组件管理器会自动将组件克隆到
`managed_components/ch9434/` 目录。

### 方式 C：手动拷贝

将本目录拷贝到 `<你的项目>/components/ch9434/`，并在你的组件的
`REQUIRES` 列表中添加 `ch9434`。

## 硬件连接

### ESP32-S3 <-> CH9434 SPI 接线

| ESP32-S3 GPIO | CH9434 引脚 | 功能         | 默认值（Kconfig）       |
|---------------|-------------|--------------|-------------------------|
| 11            | MOSI        | 主出从入     | `CONFIG_CH9434_PIN_MOSI` |
| 13            | MISO        | 主入从出     | `CONFIG_CH9434_PIN_MISO` |
| 12            | SCK         | SPI 时钟     | `CONFIG_CH9434_PIN_SCK`  |
| 10            | CS          | 片选（低有效）| `CONFIG_CH9434_PIN_CS`   |
| 3V3           | VCC         | 电源（3.3V） | -                       |
| GND           | GND         | 地线         | -                       |

所有引脚分配可通过 `idf.py menuconfig` -> **CH9434 Configuration** 修改。

### CH9434 UART 引脚

芯片对外提供 4 路独立 UART，每路包含 TX/RX/RTS/CTS：

| UART 编号 | TX  | RX  | RTS | CTS |
|-----------|-----|-----|-----|-----|
| 0         | TX0 | RX0 | RTS0| CTS0|
| 1         | TX1 | RX1 | RTS1| CTS1|
| 2         | TX2 | RX2 | RTS2| CTS2|
| 3         | TX3 | RX3 | RTS3| CTS3|

## 配置选项

所有选项位于 `menuconfig` 的 **Component config -> CH9434 Configuration** 菜单下：

| Kconfig 选项                  | 默认值 | 说明                                |
|------------------------------|--------|-------------------------------------|
| `CONFIG_CH9434_SPI_HOST`     | 2      | SPI 外设（SPI2_HOST / SPI3_HOST）   |
| `CONFIG_CH9434_PIN_MOSI`     | 11     | MOSI GPIO                           |
| `CONFIG_CH9434_PIN_MISO`     | 13     | MISO GPIO                           |
| `CONFIG_CH9434_PIN_SCK`      | 12     | SCK GPIO                            |
| `CONFIG_CH9434_PIN_CS`       | 10     | CS GPIO                             |
| `CONFIG_CH9434_SPI_CLOCK_HZ` | 200000 | SPI 时钟（Hz，50k-1M）              |
| `CONFIG_CH9434_SPI_QUEUE_SIZE`| 16     | SPI 请求队列深度                    |
| `CONFIG_CH9434_SPI_TASK_STACK`| 3072   | SPI 服务任务栈大小（字节）          |
| `CONFIG_CH9434_SPI_TASK_PRIO`| 10     | SPI 服务任务优先级（1-24）          |
| `CONFIG_CH9434_TX_CHUNK_SIZE`| 128    | 每次 TX FIFO 写入字节数             |

## 快速入门

```c
#include "ch9434_uart.h"

void app_main(void)
{
    /* 初始化 SPI 总线并通过 SCR 回环校验芯片通信。 */
    esp_err_t ret = ch9434_chip_init();
    if (ret != ESP_OK) {
        ESP_LOGE("app", "CH9434 init failed: %s", esp_err_to_name(ret));
        return;
    }

    /* 将 4 路 UART 全部配置为 115200 8N1 并启用 FIFO。 */
    const ch9434_uart_config_t cfg = {
        .baud         = CH9434_BAUD_115200,
        .data_bits    = CH9434_DATABITS_8,
        .stop_bits    = CH9434_STOPBITS_1,
        .parity       = CH9434_PARITY_NONE,
        .use_fifo     = true,
        .hw_flow_ctrl = false,
    };
    for (uint8_t u = 0; u < CH9434_UART_COUNT; u++) {
        ch9434_uart_set_config(u, &cfg);
    }

    /* 在 UART0 发送数据（测试时把 TX0 接到 RX0 形成回环）。 */
    const uint8_t msg[] = "Hello from CH9434!\r\n";
    ch9434_uart_write(CH9434_UART0, msg, sizeof(msg));

    /* 读取 RX FIFO 中到达的数据。 */
    uint8_t rx_buf[128];
    uint16_t rx_len = 0;
    vTaskDelay(pdMS_TO_TICKS(100));
    ch9434_uart_read(CH9434_UART0, rx_buf, sizeof(rx_buf), &rx_len);
    ESP_LOGI("app", "RX %u bytes", rx_len);
}
```

完整的端到端测试程序（在多 FreeRTOS 任务中同时使用 4 路 UART）位于父仓库的
`main/` 目录，可作为多 UART 并发访问的参考实现。

## API 参考

### 高层 UART API（`ch9434_uart.h`）

| 函数 | 说明 |
|------|------|
| `ch9434_chip_init()` | 初始化 SPI 总线 + 通过 SCR 回环测试校验芯片通信 |
| `ch9434_uart_set_config(uart, cfg)` | 配置波特率/数据位/停止位/校验/FIFO/流控 |
| `ch9434_uart_write(uart, data, len)` | 发送字节（自动分块适配 FIFO 大小） |
| `ch9434_uart_read(uart, data, max, out)` | 从 RX FIFO 读取最多 `max` 字节 |
| `ch9434_uart_available(uart, count)` | 查询 RX FIFO 中等待读取的字节数 |
| `ch9434_uart_dump_lsr(uart, lsr)` | 人可读的 LSR 寄存器转储（调试用） |

### SPI 层（`ch9434_spi.h`）

| 函数 | 说明 |
|------|------|
| `ch9434_spi_bus_init()` | 初始化 SPI 总线（`ch9434_chip_init` 会自动调用） |
| `ch9434_spi_bus_deinit()` | 释放 SPI 总线 |
| `ch9434_spi_write_reg(reg, val)` | 写入单个寄存器字节 |
| `ch9434_spi_read_reg(reg, val)` | 读取单个寄存器字节 |
| `ch9434_spi_write_bytes(reg, data, len)` | 批量 FIFO 写入 |
| `ch9434_spi_read_bytes(reg, data, len)` | 批量 FIFO 读取 |

### 寄存器层（`ch9434_drv.h`）

按 UART 命名的寄存器访问辅助函数（LCR/MCR/LSR/IER/IIR/DLL/DLM/SCR/FIFO），
基于 SPI 原语构建。当高层 UART API 没有暴露你需要的功能时，可使用这些函数
进行底层寄存器操作。

## 并发模型

所有公共 API 调用都经过相同的流程：

1. 调用方构造 `spi_req_t` 并推入 FreeRTOS 队列。
2. 调用方在 `ulTaskNotifyTake` 上阻塞（轻量同步，无互斥锁）。
3. 单一专用的 **SPI 服务任务**（`spi_svc`，默认优先级 10）从队列取出请求
   并原子地执行。
4. 服务任务完成后通过 `xTaskNotifyGive` 通知调用方。

由于只有这一个任务会接触 SPI 硬件，因此永远不会出现并发
`spi_device_transmit` 调用 —— 早期基于互斥锁方案中出现的断言失败在结构上
即不可能发生。批量 FIFO 传输作为单个队列条目提交，因此执行过程不会与
其他 UART 的数据流交错。

## 许可证

Apache License 2.0，详见 [LICENSE](LICENSE)。
