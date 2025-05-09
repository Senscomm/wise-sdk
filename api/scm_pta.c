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
#include "scm_pta.h"
#include "sys/ioctl.h"
#include "hal/pta.h"

#include "wise_err.h"

int scm_pta_get_force_mode(enum scm_pta_force_mode *force_mode)
{
	struct pta_force_mode_val force_mode_val;
	int ret;
	int fd;

	fd = open("/dev/pta", 0, 0);
	if (fd < 0) {
		return WISE_ERR_NOT_FOUND;
	}

	ret = ioctl(fd, IOCTL_PTA_GET_FORCE_MODE, &force_mode_val);

	close(fd);

	if (ret) {
		return WISE_ERR_IOCTL;
	}

	*force_mode = (enum scm_pta_force_mode)force_mode_val.val;

	return WISE_OK;
}

int scm_pta_set_force_mode(enum scm_pta_force_mode force_mode)
{
	struct pta_force_mode_val force_mode_val;
	int ret;
	int fd;

	fd = open("/dev/pta", 0, 0);
	if (fd < 0) {
		return WISE_ERR_NOT_FOUND;
	}

	force_mode_val.val = force_mode;

	ret = ioctl(fd, IOCTL_PTA_SET_FORCE_MODE, &force_mode_val);

	close(fd);

	if (ret) {
		return WISE_ERR_IOCTL;
	}

	return WISE_OK;
}
