/*
 * Copyright 2021-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include <cmsis_os.h>
#include <hal/kernel.h>
#include <hal/device.h>
#include <hal/clk.h>
#include <hal/pinctrl.h>
#include <hal/serial.h>
#include <hal/console.h>
#include <hal/kmem.h>
#include <hal/crypto.h>

#include "vfs.h"

//TRNG register address
#define TRNG_CR             (*(volatile uint32_t *)(TRNG_BASE_ADDR + 0x0000))
#define TRNG_MSEL           (*(volatile uint32_t *)(TRNG_BASE_ADDR + 0x0004))
#define TRNG_SR             (*(volatile uint32_t *)(TRNG_BASE_ADDR + 0x0008))
#define TRNG_DR             (*(volatile uint32_t *)(TRNG_BASE_ADDR + 0x000C))
#define TRNG_VERSION        (*(volatile uint32_t *)(TRNG_BASE_ADDR + 0x0010))
#define TRNG_RESEED         (*(volatile uint32_t *)(TRNG_BASE_ADDR + 0x0040))
#define TRNG_HT_CR          (*(volatile uint32_t *)(TRNG_BASE_ADDR + 0x0060))
#define TRNG_HT_SR          (*(volatile uint32_t *)(TRNG_BASE_ADDR + 0x0070))
#define RO_SRC_EN1          (*(volatile uint32_t *)(TRNG_BASE_ADDR + 0x0080))
#define RO_SRC_EN2          (*(volatile uint32_t *)(TRNG_BASE_ADDR + 0x0084))
#define SCLK_FREQ           (*(volatile uint32_t *)(TRNG_BASE_ADDR + 0x0088))

#define TERO_CR             (*(volatile uint32_t *)(TRNG_BASE_ADDR + 0x00B0))
#define TERO_THOLD          (*(volatile uint32_t *)(TRNG_BASE_ADDR + 0x00B4))
#define TERO_CNT(i)         (*(volatile uint32_t *)(TRNG_BASE_ADDR + 0x00C0 + 4*i))
#define TERO_SR             (*(volatile uint32_t *)(TRNG_BASE_ADDR + 0x00D0))
#define TERO_DR             (*(volatile uint32_t *)(TRNG_BASE_ADDR + 0x00D4))
#define TERO_RCR(i)         (*(volatile uint32_t *)(TRNG_BASE_ADDR + 0x00E0 + 4*i))

//TRNG action offset
#define TRNG_GLOBAL_INT_OFFSET          (24)
#define TRNG_READ_EMPTY_INT_OFFSET      (17)
#define TRNG_DATA_INT_OFFSET            (16)
#define TRNG_RO_ENTROPY_OFFSET          (4)
#define TRNG_TERO_THRESHOLD_OFFSET      (24)
#define TRNG_TERO_ENTROPY_OFFSET        (8)

struct trng_driver_data {
	struct fops devfs_ops;
	struct device *dev;
} trng_data;

//TRNG return code
enum TRNG_RET_CODE
{
	TRNG_SUCCESS = 0,
	TRNG_BUFFER_NULL,
	TRNG_INVALID_CONFIG,
	TRNG_HT_ERROR,
	TRNG_ERROR
};

typedef uint32_t GET_RAND_WORDS(uint32_t *a, uint32_t words);

/* function: get trng IP version
 * parameters: none
 * return: trng IP version
 */
#ifdef CONFIG_CMD_DMESG
static uint32_t crypto_trng_get_version(void)
{
	return TRNG_VERSION;
}
#endif


/* function: TRNG enable
 * parameters: none
 * return: none
 */
static void crypto_trng_enable(void)
{
	volatile uint32_t flag = 1;
	volatile uint32_t i;

	TRNG_CR |= flag;

	//sleep for a while
	i = 0xFFF;
	while (i--) {
		;
	}
}

/* function: TRNG disable
 * parameters: none
 * return: none
 */
static void crypto_trng_disable(void)
{
	volatile uint32_t mask = ~((uint32_t)1);

	TRNG_CR &= mask;
}

/* function: set TRNG mode
 * parameters:
 *     with_post_processing ------- 0:no,  other:yes
 * return: none
 */
static void crypto_trng_set_mode(uint8_t with_post_processing)
{
	volatile uint32_t mask = ~((uint32_t)1);
	volatile uint32_t flag = 1;
	volatile uint32_t clear_flag = 0x00000007;

	if (with_post_processing) {
		TRNG_MSEL |= flag;
	} else {
		TRNG_MSEL &= mask;
	}

	TRNG_SR |= clear_flag; //write 1 to clear
}

/* function: get some rand words
 * parameters:
 *     a -------------------------- output, random words
 *     words ---------------------- input, word number of output, must be in [1, 8]
 * return: TRNG_SUCCESS(success), other(error)
 * caution:
 *     1. please make sure the two parameters are valid
 */
uint32_t crypto_get_rand_uint32(uint32_t *a, uint32_t words)
{
	volatile uint32_t DT_ready_flag = 2;
	volatile uint32_t HT_error_flag = 1;
	uint32_t i;

	while (0 == (TRNG_SR & DT_ready_flag)) {
		if (TRNG_SR & HT_error_flag) {
			return TRNG_HT_ERROR;
		}
	}

	for (i = 0; i < words; i++) {
		*(a++) = TRNG_DR;
	}

	TRNG_SR |= DT_ready_flag;  //clear

	return TRNG_SUCCESS;
}

/* function: get rand buffer(internal basis interface)
 * parameters:
 *     rand ----------------------- input, byte buffer rand
 *     bytes ---------------------- input, byte length of rand
 *     get_rand_words ------------- input, function pointer to get some random words(at most 8 words)
 * return: TRNG_SUCCESS(success), other(error)
 */
static int crypto_get_rand_buffer(uint8_t *rand, uint32_t bytes,
	GET_RAND_WORDS get_rand_words)
{
	volatile uint32_t enable_flag = 1;
	volatile uint32_t ro_entropy_mask = (0x0000000F<<TRNG_RO_ENTROPY_OFFSET);
	uint32_t i;
	uint32_t tmp, tmp_len, rng_data;
	uint32_t count, ret;
	uint8_t *a = rand;

	//check input parameters
	if (NULL == rand || NULL == get_rand_words) {
		return TRNG_BUFFER_NULL;
	} else if (0 == bytes) {
		return TRNG_SUCCESS;
	}

	//make sure trng and ro are enabled
	if(0 == (TRNG_CR & enable_flag)) {
		return TRNG_INVALID_CONFIG;
	} else if (0 == (TRNG_CR & ro_entropy_mask)) {
		return TRNG_INVALID_CONFIG;
	}

	tmp_len = bytes;

	tmp = ((uint32_t)a) & 3;
	if (tmp) {
		i = 4-tmp;

		ret = get_rand_words(&rng_data, 1);
		if (TRNG_SUCCESS != ret) {
			goto END;
		} else {
			if (tmp_len > i) {
				memcpy(a, &rng_data, i);
				a += i;
				tmp_len -= i;
			} else {
				memcpy(a, &rng_data, tmp_len);
				goto END;
			}
		}
	}

	tmp = tmp_len / 4;
	while (tmp) {
		if (tmp > 8) {
			count = 8;
		} else {
			count = tmp;
		}

		ret = get_rand_words((uint32_t *)a, count);
		if (TRNG_SUCCESS != ret) {
			goto END;
		} else {
			a += count<<2;
			tmp -= count;
		}
	}

	tmp_len = tmp_len & 3;
	if (tmp_len) {
		ret = get_rand_words(&rng_data, 1);
		if(TRNG_SUCCESS != ret) {
			goto END;
		} else {
			memcpy(a, &rng_data, tmp_len);
		}
	}

	ret = TRNG_SUCCESS;

END:
	if (TRNG_SUCCESS != ret) {
		memset(rand, 0, bytes);
	}

	return ret;
}

/* function: get rand with fast speed(with entropy reducing, for such as clearing tmp buffer)
 * parameters:
 *     rand ----------------------- input, byte buffer rand
 *     bytes ---------------------- input, byte length of rand
 * return: TRNG_SUCCESS(success), other(error)
 */
static int crypto_get_rand_fast(struct device *dev, uint8_t *rand, uint32_t bytes)
{
	int ret;
	volatile uint32_t flag = 0;
	uint32_t flags;

	local_irq_save(flags);

	// with post-processing
	if (flag == TRNG_MSEL) {
		crypto_trng_disable();
		crypto_trng_set_mode(1);
		crypto_trng_enable();
	}

	ret = crypto_get_rand_buffer(rand, bytes, crypto_get_rand_uint32);

	local_irq_restore(flags);

	return ret;
}

static int crypto_trng_ioctl(struct file *file, unsigned int cmd, void *arg)
{
	struct trng_driver_data *priv = file->f_priv;
	struct device *dev = priv->dev;
	struct trng_read_value *rd_val;
	int ret = 0;

	switch (cmd) {
	case IOCTL_TRNG_READ:
		rd_val = arg;
		ret = crypto_trng_get_ops(dev)->get_rand_fast(dev, rd_val->val, rd_val->len);
		if (ret != TRNG_SUCCESS) {
			ret = -EIO;
		}
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

int crypto_trng_probe(struct device *dev)
{
	struct trng_driver_data *priv;
	char buf[32];
	struct file *file;

#ifdef CONFIG_CMD_DMESG
	printk("trng version : %02x\n", crypto_trng_get_version());
#endif

	priv = &trng_data;

	priv->dev = dev;
	dev->driver_data = priv;

	sprintf(buf, "/dev/trng");

	priv->devfs_ops.ioctl = crypto_trng_ioctl;

	file = vfs_register_device_file(buf, &priv->devfs_ops, priv);
	if (!file) {
		printk("%s: failed to register as %s\n", dev_name(dev), buf);
		return -1;
	}

	printk("TRNG: %s registered as %s\n", dev_name(dev), buf);

	return 0;
}

struct trng_ops crypto_trng_ops = {
    .get_rand_fast = crypto_get_rand_fast,
};

static declare_driver(crypto_trng) = {
	.name 	= "trng",
	.probe 	= crypto_trng_probe,
	.ops 	= &crypto_trng_ops
};
