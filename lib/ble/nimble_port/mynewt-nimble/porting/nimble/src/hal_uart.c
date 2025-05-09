
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>

#include "sys/termios.h"
#include "sys/ioctl.h"

#include "hal/cmsis/cmsis_os2.h"
#include "hal/console.h"

#include "hal/hal_uart.h"

#include <hal/kernel.h>
#include <hal/device.h>
#include <hal/clk.h>
#include <hal/pinctrl.h>
#include <hal/serial.h>
#include <hal/console.h>
#include <hal/kmem.h>
#include <hal/uart.h>
#include <hal/device.h>
#include "mmap.h"

#define MAX_TX_BUF                    8

struct hal_uart {
    int fd;
    uint8_t u_open:1;
    hal_uart_rx_char u_rx_func;
    hal_uart_tx_char u_tx_func;
    hal_uart_tx_done u_tx_done;
    volatile uint8_t tx_cmpl;
    void *u_func_arg;

    struct device *dev;
};

static uint8_t u_rx_ch __attribute__((section(".dma_buffer")));
static uint8_t u_tx_ch[MAX_TX_BUF] __attribute__((section(".dma_buffer")));
static struct hal_uart uart;

static void
hal_uart_cb(struct uart_event *evt, void *ctx)
{
    struct hal_uart *u = ctx;

    if (evt->type == UART_EVENT_RX_CMPL) {
        u->u_rx_func(u->u_func_arg, u_rx_ch);
        uart_rx(u->dev, &u_rx_ch, 1);
    } else {
        u->tx_cmpl = 1;
    }
}

int
hal_uart_init_cbs(int port, hal_uart_tx_char tx_func, hal_uart_tx_done tx_done,
  hal_uart_rx_char rx_func, void *arg)
{
    struct hal_uart *u;

    if (port != 0) {
        return -1;
    }
    u = &uart;

    if (u->u_open) {
        return -1;
    }
    u->u_rx_func = rx_func;
    u->u_tx_func = tx_func;
    u->u_tx_done = tx_done;
    u->u_func_arg = arg;
    return 0;
}

void
hal_uart_start_tx(int port)
{
    struct hal_uart *u;
    int data;
    int oft = 0;

    u = &uart;

    while (1) {
        data = u->u_tx_func(u->u_func_arg);
        if (data < 0) {
            if (oft > 0) {
                u->tx_cmpl = 0;
                uart_tx(u->dev, u_tx_ch, oft);
                while(!u->tx_cmpl) {
                }
            }

            break;
        }

        u_tx_ch[oft++] = (uint8_t)data;
        if (oft == MAX_TX_BUF) {
            u->tx_cmpl = 0;
            uart_tx(u->dev, u_tx_ch, MAX_TX_BUF);
            while(!u->tx_cmpl) {
            }
            oft = 0;
        }
    }
}

void
hal_uart_start_rx(int port)
{
}

int
hal_uart_init(int port, void *arg)
{
    struct device *dev;
    char devname[32];

    sprintf(devname, "atcuart.%d", CONFIG_BLE_NIMBLE_HCI_TRANSPORT_UART_PORT);

    dev = device_get_by_name(devname);
    if (!dev) {
        printk("%s not found\n", devname);
        return -1;
    }

    uart.dev = dev;

    return 0;
}

int
hal_uart_config(int port, int32_t baudrate, uint8_t databits, uint8_t stopbits,
  enum hal_uart_parity parity, enum hal_uart_flow_ctl flow_ctl)
{
    struct hal_uart *u;
    struct uart_cfg cfg;
    int ret;

    u = &uart;

    if (u->u_open) {
        return -1;
    }

    if (databits != 8) {
        return -1;
    }
    if (stopbits != 1) {
        return -1;
    }

    cfg.baudrate = (enum uart_baudrate)baudrate;
    cfg.data_bits = UART_DATA_BITS_8;
    cfg.stop_bits = UART_STOP_BIT_1;
    cfg.parity = UART_NO_PARITY;
    cfg.dma_en = 0;
    (void)flow_ctl;

    ret = uart_init(u->dev, &cfg, hal_uart_cb, (void *)u);
    if (ret) {
        printk("atcuart.%d initialize failed\n", dev_id(u->dev));
        return -1;
    }

    u->u_open = 1;

    uart_rx(u->dev, &u_rx_ch, 1);

    return 0;
}

int
hal_uart_close(int port)
{
    volatile struct hal_uart *u;

    if (port != 0) {
        return -1;
    }

    u = &uart;
    u->u_open = 0;

    return 0;
}
