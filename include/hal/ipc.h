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
#ifndef __IPC_H__
#define __IPC_H__

#include <hal/kernel.h>
#include <hal/device.h>

#include <hal/sw-irq.h>
#include <freebsd/errors.h>

#define IPC_GET_MODULE(f)           ((f & 0x000000f0) >> 4)
#define IPC_GET_WLAN(f)       ((f & 0x00000400) >> 10)
#define IPC_SET_WLAN(f, t)  do {\
    f = ((f & ~0x00000400) | (t << 10)); \
} while (0);

/* Enum for Module definitions */

typedef enum ipc_module_e
{
    IPC_MODULE_UNUSED = 0,
    IPC_MODULE_WLAN   = 1,
    IPC_MODULE_BLE    = 2,
    IPC_MODULE_SYS    = 3,
    IPC_MODULE_NUM    = IPC_MODULE_SYS
}   ipc_module_t;

/* Enum for Channel definitions */

typedef enum ipc_channel_e
{
    IPC_CHAN_UNUSED  = 0,
    IPC_CHAN_CONTROL = 1,
    IPC_CHAN_DATA    = 2,
    IPC_CHAN_EVENT   = 3,
    IPC_CHAN_NUM     = IPC_CHAN_EVENT
}   ipc_channel_t;

/* Enum for Type definitions */

typedef enum ipc_type_e
{
    IPC_TYPE_UNUSED   = 0,
    IPC_TYPE_REQUEST  = 1,
    IPC_TYPE_RESPONSE = 2,
    IPC_TYPE_ZLP      = 3,
    IPC_TYPE_NUM      = IPC_TYPE_ZLP
}   ipc_type_t;

/* Struct to contain payload information carried in a message */

typedef struct ipc_payload_s
{
    uint32_t      flag; /* Flag specifying channel, module, etc. */
    uint32_t       idx; /* Index (0 - ) assigned to every payload. */
    uint32_t       seq; /* Sequence number. */
    size_t        size; /* Size of actual payload starting at data. */
    size_t       size1; /* Size of wrapped payload. */
    uint8_t      *data; /* Start of actual payload. */
    uint8_t     *data1; /* Start of wrapped payload. */
}   ipc_payload_t;

/* IPC handler */

typedef int (*ipc_handler)(ipc_payload_t *payload, void *priv);

/* Struct to hold a listener's information */

typedef struct ipc_listener_s
{
    ipc_handler              cb; /* IPC callback */

    /* Control channel only */

    uint32_t                seq; /* Sequence number */
    ipc_type_t             type; /* Control packet type */

    void                  *priv; /* Private data for a listener */
    struct list_head list_entry;
}   ipc_listener_t;

struct ipc_ops {

    /* Allocate a buffer space to hold payload data to transmit. */

    ipc_payload_t
        *(*alloc_payload)(struct device *dev, size_t size, bool linear, bool block,
                 int module, int channel, int type);

    /* Transmit a buffer. */

    int  (*transmit)(struct device *dev, ipc_payload_t *payload);

    /* Call to enable or disable RX interrupts */

    int  (*rxint)(struct device *dev, bool enable);

    /* Return true if received data is available. */

    bool (*rxavailable)(struct device *dev);

    /* Receive a buffer. */

    ipc_payload_t
        *(*receive)(struct device *);

    /* Release a previously received buffer. */

    int  (*free_payload)(struct device *dev, ipc_payload_t *payload);

    /* Add a listener to handle received buffer. */

    int  (*addcb)(struct device *dev, ipc_module_t module,
                  ipc_channel_t channel, ipc_listener_t *listener);

    /* Remove a listener. */

    int  (*delcb)(struct device *dev, ipc_module_t module,
                  ipc_channel_t channel, ipc_listener_t *listener);

    /* Util functions. */

    /* Performs copy of the IPC payload into a supplied linear buffer. */

    int  (*copyfrom)(struct device *dev,
                     uint8_t *buffer, uint32_t buflen,
                     ipc_payload_t *payload, uint32_t offset);

    /* Performs copy of the supplied linear buffer into a IPC payload. */

    int  (*copyto)(struct device *dev,
                   uint8_t *buffer, uint32_t buflen,
                   ipc_payload_t *payload, uint32_t offset);

	/* Get synchronized with the host. */

	int (*sync)(struct device *dev);
};


#define ipc_ops(x)	((struct ipc_ops *)(x)->driver->ops)

__ilm__
static __inline__ ipc_payload_t
                      *ipc_alloc(struct device *dev, size_t size, bool linear, bool block,
                                 int module, int channel, int type)
{
	if (!dev)
		return NULL;

	return ipc_ops(dev)->alloc_payload(dev, size, linear, block, module, channel, type);
}

__ilm__
static __inline__ int  ipc_transmit(struct device *dev,
                                    ipc_payload_t *payload)
{
	if (!dev)
		return -ENODEV;

	return ipc_ops(dev)->transmit(dev, payload);
}

static __inline__ int  ipc_rxint(struct device *dev,
                                 bool enable)
{
	if (!dev)
		return -ENODEV;

	return ipc_ops(dev)->rxint(dev, enable);
}

static __inline__ bool ipc_rxavailable(struct device *dev)
{
	if (!dev)
		return false;

	return ipc_ops(dev)->rxavailable(dev);
}

__ilm__
static __inline__ ipc_payload_t
                      *ipc_receive(struct device *dev)
{
	if (!dev)
		return NULL;

	return ipc_ops(dev)->receive(dev);
}

__ilm__
static __inline__ int ipc_free(struct device *dev,
                                ipc_payload_t *payload)
{
	if (!dev)
		return -ENODEV;

	return ipc_ops(dev)->free_payload(dev, payload);
}

static __inline__ int ipc_addcb(struct device *dev,
                                ipc_module_t module,
                                ipc_channel_t channel,
                                ipc_listener_t *listener)
{
	if (!dev)
		return -ENODEV;

	return ipc_ops(dev)->addcb(dev, module, channel, listener);
}

static __inline__ int ipc_delcb(struct device *dev,
                                ipc_module_t module,
                                ipc_channel_t channel,
                                ipc_listener_t *listener)
{
	if (!dev)
		return -ENODEV;

	return ipc_ops(dev)->delcb(dev, module, channel, listener);
}

static __inline__ int ipc_copyfrom(struct device *dev,
                                   uint8_t *buffer, uint32_t buflen,
                                   ipc_payload_t *payload,
                                   uint32_t offset)
{
	if (!dev)
		return -ENODEV;

	return ipc_ops(dev)->copyfrom(dev, buffer, buflen, payload, offset);
}

static __inline__ int ipc_copyto(struct device *dev,
                                 uint8_t *buffer, uint32_t buflen,
                                 ipc_payload_t *payload,
                                 uint32_t offset)
{
	if (!dev)
		return -ENODEV;

	return ipc_ops(dev)->copyto(dev, buffer, buflen, payload, offset);
}

static __inline__ int ipc_sync(struct device *dev)
{
	if (!dev)
		return -ENODEV;

	return ipc_ops(dev)->sync(dev);
}

#endif
