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

#define _GNU_SOURCE
#define __USE_NATIVE_HEADER__

#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include <FreeRTOS/FreeRTOS.h>
#include <FreeRTOS/stream_buffer.h>
#include <FreeRTOS/serial.h> /* Demo/Common/include/ */

#include <hal/kernel.h>
#include <hal/device.h>
#include <hal/console.h>
#include <hal/serial.h>
#include <hal/kmem.h>

struct sandbox_serial_port {
	struct serial_port port;
	bool initialized;
};

#define to_sandbox_serial_port(port) \
	container_of(port, struct sandbox_serial_port, port)


/* Virtual interrupt handler */
static int sandbox_serial_irq(int fd, void *data)
{
	struct sandbox_serial_port *sport = data;
	char buf[256], *ptr = buf; /* don't worry */
	ssize_t len;
	portBASE_TYPE resched = pdFALSE;

	len = min(sizeof(buf),
		  uxQueueSpacesAvailableFromISR(sport->port.queue[RX]));
 restart:
	len = read(fd, buf, len);
	if (len == -1) {
		if (errno == EINTR)
			goto restart;
		else
			printf("read: %s\n", strerror(errno));
		goto out;
	}
	while (len-- > 0)
		xQueueSendFromISR(sport->port.queue[RX], ptr++, &resched);

 out:
	portEND_SWITCHING_ISR(resched);

	return 0;
}

static int sandbox_uart_init(struct serial_port *port)
{
	struct sandbox_serial_port *sport = to_sandbox_serial_port(port);
	struct termios term;
	struct sigaction sa __maybe_unused;
	int fd, flags __maybe_unused;

	fd = open("/dev/tty", O_RDWR);
	if (fd < 0)
		return fd;

	/* Raw mode */
	tcgetattr(fd, &term /*&port->oldtermios*/);
	//term = port->oldtermios;
	cfmakeraw(&term);
	term.c_oflag |= OPOST | ONLCR;
	term.c_lflag |= ISIG;
	term.c_cc[VMIN] = 0;
	term.c_cc[VTIME] = 1;
	tcsetattr(fd, TCSANOW, &term);

	request_irq(fd, sandbox_serial_irq, "sandbox-serial", 0, sport);

	return 0;
}

/* FIXME: reetrancy? */
static void sandbox_uart_start_tx(struct serial_port *port)
{
	int ret, len = uxQueueMessagesWaiting(port->queue[TX]);
	char ch;

	while (len > 0) {
		xQueueReceive(port->queue[TX], &ch, 0);
	retry:
		ret = write(1, &ch, 1);
		if (ret < 0) {
			if (errno == EINTR)
				goto retry;
			else
				continue;
		}
		len--;
	}
}

static int sandbox_uart_putc(struct serial_port *port, char ch)
{
	int ret;

 retry:
	ret = write(1, &ch, 1);
	if (ret < 0 && errno == EINTR)
	    goto retry;

	return ret;
}

static struct serial_port_ops sandbox_serial_ops = {
	.init 		= sandbox_uart_init,
	.start_tx 	= sandbox_uart_start_tx,
	.putc 		= sandbox_uart_putc,
};

static int sandbox_uart_probe(struct device *dev)
{
	struct sandbox_serial_port *sport;
	int ret;

	sport = kmalloc(sizeof(*sport));
	if (!sport)
		return -ENOMEM;

	sport->port.device = dev;
	sport->port.ops = &sandbox_serial_ops;
	sport->port.id = dev_id(dev);

	ret = register_serial_port(&sport->port);
	return ret;
}

static declare_driver(sandbox_uart) = {
	.name = "sandbox-uart",
	.probe = sandbox_uart_probe,
};
