/*
 * Copyright 2023-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "errno.h"
#include "scm_crypto.h"
#include "sys/ioctl.h"
#include "hal/crypto.h"

int scm_crypto_trng_read(uint8_t *val, int len)
{
	struct trng_read_value trng_val;
	int ret;
	int fd;

	fd = open("/dev/trng", 0, 0);
	if (fd < 0) {
		return WISE_ERR_NOT_FOUND;
	}

	trng_val.len = len;
	trng_val.val = val;

	ret = ioctl(fd, IOCTL_TRNG_READ, &trng_val);

	close(fd);

	if (ret) {
		return WISE_ERR_IOCTL;
	}
	return WISE_OK;
}
