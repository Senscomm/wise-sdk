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

#ifndef _CRYPTO_H_
#define _CRYPTO_H_

#include <stdint.h>
#include <hal/types.h>
#include <hal/device.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * High-level device driver
 */

#define IOCTL_TRNG_READ				(1)

struct trng_read_value
{
	int len;
	uint8_t *val;
};

/*
 * Low-level device driver
 */

struct pke_ops {
    int (*pke_eccp_point_mul)(struct device *dev, uint32_t *k, uint32_t *px, uint32_t *py,
        uint32_t *qx, uint32_t *qy);
    int (*pke_eccp_point_add)(struct device *dev, uint32_t *p1x, uint32_t *p1y,
        uint32_t *p2x, uint32_t *p2y, uint32_t *qx, uint32_t *qy);
    int (*pke_eccp_point_verify)(struct device *dev, uint32_t *px, uint32_t *py);
};

struct trng_ops {
    int (*get_rand_fast)(struct device *dev, uint8_t *rand, uint32_t bytes);
};

#define crypto_pke_get_ops(dev)		((struct pke_ops *)(dev)->driver->ops)
#define crypto_trng_get_ops(dev)	((struct trng_ops *)(dev)->driver->ops)

static __inline__ int crypto_eccp_point_mul(struct device *dev, uint32_t *k, uint32_t *px, uint32_t *py,
        uint32_t *qx, uint32_t *qy)
{
	if (!dev)
		return -ENODEV;
	if (!crypto_pke_get_ops(dev)->pke_eccp_point_mul)
		return -ENOSYS;

	return crypto_pke_get_ops(dev)->pke_eccp_point_mul(dev, k, px, py, qx, qy);
}

static __inline__ int crypto_eccp_point_add(struct device *dev, uint32_t *p1x, uint32_t *p1y,
        uint32_t *p2x, uint32_t *p2y, uint32_t *qx, uint32_t *qy)
{
	if (!dev)
		return -ENODEV;
	if (!crypto_pke_get_ops(dev)->pke_eccp_point_add)
		return -ENOSYS;

	return crypto_pke_get_ops(dev)->pke_eccp_point_add(dev, p1x, p1y, p2x, p2y, qx, qy);
}

static __inline__ int crypto_eccp_point_verify(struct device *dev, uint32_t *px, uint32_t *py)
{
	if (!dev)
		return -ENODEV;
	if (!crypto_pke_get_ops(dev)->pke_eccp_point_verify)
		return -ENOSYS;

	return crypto_pke_get_ops(dev)->pke_eccp_point_verify(dev, px, py);
}

static __inline__ int crypto_trng_get_rand(struct device *dev, uint8_t *rand, uint32_t bytes)
{
	if (!dev)
		return -ENODEV;
	if (!crypto_trng_get_ops(dev)->get_rand_fast)
		return -ENOSYS;

	return crypto_trng_get_ops(dev)->get_rand_fast(dev, rand, bytes);
}

#ifdef __cplusplus
}
#endif

#endif //_CRYPTO_H_
