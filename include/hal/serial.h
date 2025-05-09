/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __SERIAL_H__
#define __SERIAL_H__

#include <hal/kernel.h>
#include <hal/device.h>

#include <sys/cdefs.h>
#include <termios.h>

#include <FreeRTOS/FreeRTOS.h>
#include <FreeRTOS/xqueue.h>

struct serial_port;

struct serial_port_ops {
	int (*init)(struct serial_port *);
	void (*start_tx)(struct serial_port *);
	int (*ioctl)(struct serial_port *, unsigned, void *);
	int (*putc)(struct serial_port *, char);
	int (*con_write)(struct serial_port *, char *, int);
};

typedef QueueHandle_t queue_t;
typedef enum { TXD = 0, RXD = 1, CTS = 2, RTS = 3 } uart_pin_t;
enum { TX = 0, RX = 1 };

struct serial_port {
	int id;
	struct device *device;
	struct serial_port_ops *ops;
	struct list_head list;
	struct termios oldtermios;
	queue_t queue[2];
	int initialized;
};

extern int register_serial_port(struct serial_port *);
int uart_get_baudrate(struct termios *);

#endif
