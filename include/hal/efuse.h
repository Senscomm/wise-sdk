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

#ifndef __EFUSE_H__
#define __EFUSE_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * High-level device driver
 */

#define IOCTL_EFUSE_READ_ROW		(1)
#define IOCTL_EFUSE_WRITE_ROW		(2)

#define IOCTL_EFUSE_SET_MODE		(3)
#define IOCTL_EFUSE_GET_MODE		(4)
#define IOCTL_EFUSE_SYNC			(5)
#define IOCTL_EFUSE_LOAD			(6)

enum efuse_mode {
	EFUSE_MODE_RAW 			= 0,
	EFUSE_MODE_RAM_BUFFER	= 1,
	EFUSE_MODE_FLASH_BUFFER	= 2,
};

struct efuse_rw_data
{
	int row;
	u32 *val;
};

/*
 * Low-level device driver
 */

struct efuse_ops {
    int (*read_row)(struct device *dev, int row, u32 *val);
    int (*write_row)(struct device *dev, int row, u32 val);
#ifdef CONFIG_EFUSE_BUFFER_MODE
	int (*set_mode)(struct device *dev, u8 mode);
	int (*get_mode)(struct device *dev, u8 *mode);
	int (*sync)(struct device *dev);
	int (*load)(struct device *dev);
#endif
};

/* Access the operations for an eFuse device */
#define efuse_get_ops(dev)	((struct efuse_ops *)(dev)->driver->ops)

static __inline__ u32 efuse_read(struct device *dev,
        int row, u32 *val)
{
	if (!dev)
		return -ENODEV;

	if (!efuse_get_ops(dev)->read_row)
		return -ENOSYS;

	return efuse_get_ops(dev)->read_row(dev, row, val);

}

static __inline__ int efuse_write(struct device *dev,
		int row, u32 val)
{
	if (!dev)
		return -ENODEV;

	if (!efuse_get_ops(dev)->write_row)
		return -ENOSYS;

	return efuse_get_ops(dev)->write_row(dev, row, val);
}

#ifdef CONFIG_EFUSE_BUFFER_MODE
static __inline__ int efuse_set_mode(struct device *dev, u8 mode)
{
	if (!dev)
		return -ENODEV;

	if (!efuse_get_ops(dev)->set_mode)
		return -ENOSYS;

	return efuse_get_ops(dev)->set_mode(dev, mode);
}

static __inline__ int efuse_get_mode(struct device *dev, u8 *mode)
{
	if (!dev)
		return -ENODEV;

	if (!efuse_get_ops(dev)->get_mode)
		return -ENOSYS;

	return efuse_get_ops(dev)->get_mode(dev, mode);
}

static __inline__ int efuse_sync(struct device *dev, u8 mode)
{
	if (!dev)
		return -ENODEV;

	if (!efuse_get_ops(dev)->sync)
		return -ENOSYS;

	return efuse_get_ops(dev)->sync(dev);
}

static __inline__ int efuse_load(struct device *dev)
{
	if (!dev)
		return -ENODEV;

	if (!efuse_get_ops(dev)->load)
		return -ENOSYS;

	return efuse_get_ops(dev)->load(dev);
}
#endif

#ifdef __cplusplus
}
#endif

#endif
