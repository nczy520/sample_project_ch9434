/*
 * CH9434 register map (subset used by SPI to 4xUART bridge)
 *
 * Reference: WCH CH9434A/CH9434D register definitions (CH9434.H).
 * CH9434M follows the CH9434A-style timing/protocol.
 *
 * Each sub-UART lives at an offset of 0x10 inside the register space.
 */
#ifndef CH9434_REGS_H
#define CH9434_REGS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- Operation codes (high bit of address byte) ---------- */
#define CH9434_REG_OP_WRITE                 0x80
#define CH9434_REG_OP_READ                  0x00

/* ---------- Sub-UART register offsets (0x10 stride) ---------- */
#define CH9434_UART0_REG_OFFSET             0x00
#define CH9434_UART1_REG_OFFSET             0x10
#define CH9434_UART2_REG_OFFSET             0x20
#define CH9434_UART3_REG_OFFSET             0x30

/* Per-UART register offsets (relative to the UART's own base) */
#define CH9434_UARTx_RBR                    0x00  /* RO - receive buffer (DLAB=0) */
#define CH9434_UARTx_THR                    0x00  /* WO - transmit holding (DLAB=0) */
#define CH9434_UARTx_IER                    0x01  /* R/W - interrupt enable (DLAB=0) */
#define CH9434_UARTx_IIR                    0x02  /* RO - interrupt identification */
#define CH9434_UARTx_FCR                    0x02  /* WO - FIFO control */
#define CH9434_UARTx_LCR                    0x03  /* R/W - line control */
#define CH9434_UARTx_BIT_DLAB               (1 << 7)
#define CH9434_UARTx_MCR                    0x04  /* R/W - modem control */
#define CH9434_UARTx_LSR                    0x05  /* R   - line status */
#define CH9434_UARTx_MSR                    0x06  /* R   - modem status */
#define CH9434_UARTx_SCR                    0x07  /* R/W - scratch (user defined) */
#define CH9434_UARTx_DLL                    0x00  /* R/W - divisor latch L (DLAB=1) */
#define CH9434_UARTx_DLM                    0x01  /* R/W - divisor latch H (DLAB=1) */

/* Line Status Register (LSR) bits */
#define CH9434_LSR_DR                       (1 << 0)  /* data ready */
#define CH9434_LSR_OE                       (1 << 1)  /* overrun error */
#define CH9434_LSR_PE                       (1 << 2)  /* parity error */
#define CH9434_LSR_FE                       (1 << 3)  /* framing error */
#define CH9434_LSR_BI                       (1 << 4)  /* break indicator */
#define CH9434_LSR_THRE                     (1 << 5)  /* transmitter holding empty */
#define CH9434_LSR_TEMT                     (1 << 6)  /* transmitter empty */
#define CH9434_LSR_FERR                     (1 << 7)  /* FIFO data error */

/* Line Control Register (LCR) bits */
#define CH9434_LCR_WLS_5                    0x00
#define CH9434_LCR_WLS_6                    0x01
#define CH9434_LCR_WLS_7                    0x02
#define CH9434_LCR_WLS_8                    0x03
#define CH9434_LCR_STOP                     (1 << 2)
#define CH9434_LCR_PARITY_EN                (1 << 3)
#define CH9434_LCR_PARITY_EVEN              (1 << 4)
#define CH9434_LCR_PARITY_STICK             (1 << 5)
#define CH9434_LCR_BREAK                    (1 << 6)

/* FIFO Control Register (FCR) bits */
#define CH9434_FCR_ENABLE                   (1 << 0)
#define CH9434_FCR_RX_RESET                 (1 << 1)
#define CH9434_FCR_TX_RESET                 (1 << 2)
#define CH9434_FCR_TRIG_1                   (0 << 6)
#define CH9434_FCR_TRIG_4                   (1 << 6)
#define CH9434_FCR_TRIG_8                   (2 << 6)
#define CH9434_FCR_TRIG_14                  (3 << 6)

/* Modem Control Register (MCR) bits */
#define CH9434_MCR_DTR                      (1 << 0)
#define CH9434_MCR_RTS                      (1 << 1)
#define CH9434_MCR_OUT1                     (1 << 2)
#define CH9434_MCR_OUT2                     (1 << 3)
#define CH9434_MCR_LOOPBACK                 (1 << 4)
#define CH9434_MCR_AFE                      (1 << 5)  /* auto flow control enable */

/* Interrupt Enable Register (IER) bits */
#define CH9434_IER_RX                       (1 << 0)
#define CH9434_IER_TX                       (1 << 1)
#define CH9434_IER_LS                       (1 << 2)
#define CH9434_IER_MS                       (1 << 3)

/* Interrupt Identification Register (IIR) bits */
#define CH9434_IIR_PEND                     (1 << 0)  /* 0=pending, 1=no interrupt */
#define CH9434_IIR_ID_MASK                  0x0E
#define CH9434_IIR_ID_MODEM                 0x00
#define CH9434_IIR_ID_TX_EMPTY              0x02
#define CH9434_IIR_ID_RX_AVAIL              0x04
#define CH9434_IIR_ID_LINE_STATUS           0x06
#define CH9434_IIR_ID_TIMEOUT               0x0C

/* ---------- Global registers ---------- */
#define CH9434_TNOW_CTRL_CFG                0x41
#define CH9434_TNOW_POLAR_MASK              0xF0
#define CH9434_TNOW_EN_MASK                 0x0F
#define CH9434_FIFO_CTRL                    0x42
#define CH9434_FIFO_CTRL_TR                 (1 << 4)
#define CH9434_FIFO_CTRL_UART_MASK          0x0F
#define CH9434_FIFO_CTRL_L                  0x43
#define CH9434_FIFO_CTRL_H                  0x44
#define CH9434_IO_SEL_FUN_CFG               0x45
/* Clock control register (0x48) bit layout per WCH CH9434 reference */
#define CH9434_CLK_FREQ_MUL_EN              (3 << 6)  /* enable clock multiplier (1=1x, 3=15x mult) */
#define CH9434_CLK_XT_EN                    (1 << 5)  /* use external crystal  */
#define CH9434_CLK_DIV_MASK                 0x1F
#define CH9434_CLK_CTRL_CFG                 0x48
#define CH9434_SLEEP_MOD_CFG                0x4A

/* UART indices (also used in the FIFO control byte) */
#define CH9434_UART0                        0
#define CH9434_UART1                        1
#define CH9434_UART2                        2
#define CH9434_UART3                        3
#define CH9434_UART_COUNT                   4

/* Helpers: convert uart index to its base address and per-register addr */
#define CH9434_UART_BASE(idx)               ((idx) * 0x10)
#define CH9434_UART_REG(idx, reg)           (CH9434_UART_BASE(idx) + (reg))

/* Per-UART address lookups (full address, no OP bit) */
#define CH9434_ADDR_RBR(u)                  CH9434_UART_REG((u), CH9434_UARTx_RBR)
#define CH9434_ADDR_THR(u)                  CH9434_UART_REG((u), CH9434_UARTx_THR)
#define CH9434_ADDR_IER(u)                  CH9434_UART_REG((u), CH9434_UARTx_IER)
#define CH9434_ADDR_IIR(u)                  CH9434_UART_REG((u), CH9434_UARTx_IIR)
#define CH9434_ADDR_FCR(u)                  CH9434_UART_REG((u), CH9434_UARTx_FCR)
#define CH9434_ADDR_LCR(u)                  CH9434_UART_REG((u), CH9434_UARTx_LCR)
#define CH9434_ADDR_MCR(u)                  CH9434_UART_REG((u), CH9434_UARTx_MCR)
#define CH9434_ADDR_LSR(u)                  CH9434_UART_REG((u), CH9434_UARTx_LSR)
#define CH9434_ADDR_MSR(u)                  CH9434_UART_REG((u), CH9434_UARTx_MSR)
#define CH9434_ADDR_SCR(u)                  CH9434_UART_REG((u), CH9434_UARTx_SCR)
#define CH9434_ADDR_DLL(u)                  CH9434_UART_REG((u), CH9434_UARTx_DLL)
#define CH9434_ADDR_DLM(u)                  CH9434_UART_REG((u), CH9434_UARTx_DLM)

#ifdef __cplusplus
}
#endif

#endif /* CH9434_REGS_H */
