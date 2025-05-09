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

#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <poll.h>

#include <hal/kernel.h>
#include <hal/device.h>
#include <hal/serial.h>
#include <hal/console.h>

#include "vfs.h"

int g_serial_console_port = CONFIG_SERIAL_CONSOLE_PORT;

static LIST_HEAD_DEF(serial_port);

int serial_open(struct file *file)
{
	struct serial_port *port = file->f_priv;
	int i, ret = 0;

	if (port->initialized)
		return 0;

	for (i = 0; i < 2; i++) {
		port->queue[i] = xQueueCreate(CONFIG_SERIAL_BUFFER_SIZE, sizeof(char));
		if (port->queue[i] == NULL) {
			ret = -ENOMEM;
			goto error;
		}
	}

	if (port->ops->init)
		ret = port->ops->init(port);

	if (ret)
		goto error;

	port->initialized = 1;
 	return 0;

 error:
	for (i = 0; i < 2; i++)
		if (port->queue[i])
			vQueueDelete(port->queue[i]);

	return ret;
}

ssize_t serial_write(struct file *file, void *buf, size_t size, off_t *pos)
{
	struct serial_port *port = file->f_priv;
	void *ptr = buf;
	size_t len, space;

	space = uxQueueSpacesAvailable(port->queue[TX]);
	len = min(size, space);
try_again:
	if (len == 0) {
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;

		xQueueSend(port->queue[TX], ptr++, -1);
		size--;
		space = uxQueueSpacesAvailable(port->queue[TX]);
		len = min(size, space);
	}
	/* @len: number of writable bytes without blocking */
	while (len > 0) {
		/* Interrupted, then try again in case missing letters */
		if (xQueueSend(port->queue[TX], ptr, 0) != pdPASS) {
			len = 0;
			goto try_again;
		}
		ptr++;
		len--;
	}
	if (ptr != buf)
		port->ops->start_tx(port);

	return (ssize_t)(ptr - buf);
}

ssize_t serial_read(struct file *file, void *buf, size_t size, off_t *pos)
{
	struct serial_port *port = file->f_priv;
	void *ptr = buf;

	if (file->f_flags & O_NONBLOCK &&
			uxQueueMessagesWaiting(port->queue[RX]) == 0)
		return -EAGAIN;

	while (size > 0 &&
	       xQueueReceive(port->queue[RX], ptr, -1) == pdPASS) {
		ptr++;
		size--;
	}

	return (ssize_t)(ptr - buf);
}

unsigned serial_poll(struct file *file, struct poll_table *pt, struct pollfd *pfd)
{
	struct serial_port *port = file->f_priv;
	unsigned mask = 0;

	/*
	 * NB: because of FreeRTOS queue set implementation limitation,
	 * we drop the tx polling for the serial device.
	 *
	 * Alternatively, we can have another flags to file_op:poll() to
	 * indicate which events (RD|WR|ERR) the caller is expecting
	 * from this @file. This would be better.
	 */
	if (pfd->events & POLLOUT)
		poll_add_wait(file, port->queue[TX], pt);
	if (pfd->events & POLLIN)
		poll_add_wait(file, port->queue[RX], pt);

	mask |= uxQueueSpacesAvailable(port->queue[TX]) ? POLLOUT : 0;
	mask |= uxQueueMessagesWaiting(port->queue[RX]) ? POLLIN : 0;
	return mask;
}

int serial_ioctl(struct file *file, unsigned int cmd, void *arg)
{
	struct serial_port *port = file->f_priv;
	struct termios *termios = arg;
	int ret = 0;

	switch (cmd) {
	case TCGETS:
		memcpy(termios, &port->oldtermios, sizeof(*termios));
		break;
	default:
		ret = port->ops->ioctl(port, cmd, arg);
		break;
	}
	return ret;
}

struct fops serial_fops;

#ifdef CONFIG_SERIAL_CONSOLE

static int serial_console_write(struct console *con, char *s, int len)
{
	int write_len = len;
	struct serial_port *port = con->private;

	if (!port)
		return 0;
	if (port->ops->con_write)
		return port->ops->con_write(port, s, len);

	if (port->ops->putc == NULL)
		return 0;

	while (len-- > 0) {
		if (*s == '\n') {
			while (port->ops->putc(port, '\r') < 0);
		}
		while (port->ops->putc(port, *s) < 0);
	}
	return write_len;
}

static struct console serial_console = {
	.write = serial_console_write,
};

#endif /* SERIAL_CONSOLE */

int register_serial_port(struct serial_port *port)
{
	char buf[32];
	struct file *file;

	if (port == NULL || port->ops == NULL)
		return -EINVAL;

	INIT_LIST_HEAD(&port->list);

	sprintf(buf, "/dev/ttyS%d", port->id);

	/* assign for future patch function */
	serial_fops.ioctl = serial_ioctl;
	serial_fops.open = serial_open;
	serial_fops.poll = serial_poll;
	serial_fops.read = serial_read;
	serial_fops.write = serial_write;

	file = vfs_register_device_file(buf, &serial_fops, port);
	if (!file)
		return -ENOMEM;

	printk("UART: %s registered as %s\n", port->device->name, buf);

#if CONFIG_SERIAL_CONSOLE
	if (port->id == g_serial_console_port) {
		serial_console.name = file->f_path;
		serial_console.private = port;
		register_console(&serial_console);
	}
#endif
	return 0;
}



int uart_get_baudrate(struct termios *termios)
{
	/* BSD termios */
	return termios->c_ispeed;
}

#ifdef CONFIG_FREERTOS_SERIAL_API

#include "serial.h"

/*
 * FreeRTOS serial APIs
 *
 * NB: WISE recommends using standard I/O functions to perform serial I/O.
 *
 */

static int csize_map[] = {
	[serBITS_5] = CS5,
	[serBITS_6] = CS6,
	[serBITS_7] = CS7,
	[serBITS_8] = CS8,
};

#define baud(x) [ser##x] = B##x
static int baud_map[] = {
	baud(50),
	baud(75),
	baud(110),
	baud(134),
	baud(150),
	baud(200),
	baud(300),
	baud(600),
	baud(1200),
	baud(1800),
	baud(2400),
	baud(4800),
	baud(9600),
	baud(19200),
	baud(38400),
	baud(57600),
	baud(115200),
};

static void encode_termios(struct termios *termios, eBaud baud,
			   eParity parity, eDataBits bits, eStopBits stop)
{
	switch (parity) {
	case serODD_PARITY:
		termios->c_cflag |= PARODD;
		break;
	case serEVEN_PARITY:
		termios->c_cflag |= PARENB;
		break;
	case serNO_PARITY:
	default:
		termios->c_cflag &= ~PARENB;
		break;
	}

	if (stop == serSTOP_2)
		termios->c_cflag |= CSTOPB;
	else
		termios->c_cflag &= ~CSTOPB;

	termios->c_cflag &= ~CSIZE;
	termios->c_cflag |= csize_map[bits];

	termios->c_ispeed = baud_map[baud];
}


xComPortHandle xSerialPortInit(eCOMPort port, eBaud baud, eParity parity,
			       eDataBits bits, eStopBits stop,
			       unsigned portBASE_TYPE blen)
{
	return NULL;
}

xComPortHandle
xSerialPortInitMinimal(unsigned long baud, unsigned portBASE_TYPE qlen)
{
	return xSerialPortInit(CONFIG_SERIAL_CONSOLE_PORT,
			       ser115200, serNO_PARITY, serBITS_8, serSTOP_1,
			       128);
}

signed portBASE_TYPE
xSerialPutChar(xComPortHandle port, signed char c, TickType_t timeout)
{
	return 0;
}

void
vSerialPutString(xComPortHandle port, const signed char * const s, u16 len)
{
	return;
}

signed portBASE_TYPE
xSerialGetChar(xComPortHandle port, signed char *c, TickType_t timeout)
{
	return 0;
}

portBASE_TYPE xSerialWaitForSemaphore(xComPortHandle xPort)
{
	return false;
}

void vSerialClose(xComPortHandle xPort)
{
	return;
}

#endif /* CONFIG_FREERTOS_SERIAL_API */
