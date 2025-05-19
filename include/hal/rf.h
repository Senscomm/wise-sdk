/*
 * Copyright 2022-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _RF_H_
#define _RF_H_

#include <hal/kernel.h>
#include <hal/device.h>
#include <hal/clk.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Generic RF interface.
 */

typedef int (*reg_write)(u32 addr, u32 data);
typedef u32 (*reg_read)(u32 addr);

struct rf_cal_data {
	int tx_i_offset[2];
	int tx_q_offset[2];
	int tx_i_dc_offset[2];
	int tx_q_dc_offset[2];
	int rx_i_offset[2];
	int rx_q_offset[2];
	int rx_i_dc_offset[2];
	int rx_q_dc_offset[2];
};

struct rf_ops {
	int (*config)(struct device *dev, reg_write wrfn, reg_read rdfn);
	int (*init)(struct device *dev, u8 path);
	int (*set_channel)(struct device *dev, u8 path, u32 freq, u8 bw);
	int (*get_temp)(struct device *dev, u8 print_en);
	int (*cfo_adj)(struct device *dev, int ppm);
	int (*cfo_ofs)(struct device *dev, int ofs);
	int (*get_cal_data)(struct device *dev, u8 bd_no, struct rf_cal_data *data);
	int (*rf_control)(struct device *dev, u8 flag);
};

/* Access the operations for an RF device */
#define rf_get_ops(dev)	((struct rf_ops *)(dev)->driver->ops)

static __inline__ int rf_onoff_cntrl(struct device *dev,	u8 flag)
{
	if (!dev)
		return -ENODEV;
	if (!rf_get_ops(dev)->rf_control)
		return -ENOSYS;

	return rf_get_ops(dev)->rf_control(dev, flag);
}

static __inline__ int rf_config(struct device *dev,
		reg_write wrfn, reg_read rdfn)
{
	if (!dev)
		return -ENODEV;
	if (!rf_get_ops(dev)->config)
		return -ENOSYS;

	return rf_get_ops(dev)->config(dev, wrfn, rdfn);

}

static __inline__ int rf_init(struct device *dev, u8 path)
{
	if (!dev)
		return -ENODEV;
	if (!rf_get_ops(dev)->init)
		return -ENOSYS;

	return rf_get_ops(dev)->init(dev, path);

}

static __inline__ int rf_set_channel(struct device *dev, u8 path, u32 freq, u8 bw)
{
	if (!dev)
		return -ENODEV;
	if (!rf_get_ops(dev)->set_channel)
		return -ENOSYS;

	return rf_get_ops(dev)->set_channel(dev, path, freq, bw);

}

static __inline__ int rf_get_temp(struct device *dev, u8 print_en)
{
	if (!dev)
		return -ENODEV;
	if (!rf_get_ops(dev)->get_temp)
		return -ENOSYS;

	return rf_get_ops(dev)->get_temp(dev, print_en);

}

static __inline__ int rf_cfo_adj(struct device *dev, int ppm)
{
	if (!dev)
		return -ENODEV;
	if (!rf_get_ops(dev)->cfo_adj)
		return -ENOSYS;

	return rf_get_ops(dev)->cfo_adj(dev, ppm);

}

static __inline__ int rf_cfo_ofs(struct device *dev, int ofs)
{
        if (!dev)
                return -ENODEV;
        if (!rf_get_ops(dev)->cfo_ofs)
                return -ENOSYS;

        return rf_get_ops(dev)->cfo_ofs(dev, ofs);
}

static __inline__ int rf_get_cal_data(struct device *dev, u8 bd_no, struct rf_cal_data *data)
{
	if (!dev)
		return -ENODEV;
	if (!rf_get_ops(dev)->get_cal_data)
		return -ENOSYS;

	return rf_get_ops(dev)->get_cal_data(dev, bd_no, data);

}

#ifdef __cplusplus
}
#endif

#endif  /* _RF_H_ */
