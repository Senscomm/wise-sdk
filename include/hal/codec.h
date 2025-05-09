/*
 * Copyright 2024-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/*
 * Copyright (c) 2018 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public API header file for Audio Codec
 *
 * This file contains the Audio Codec APIs
 */

#ifndef __CODEC_H__
#define __CODEC_H__

#include <hal/i2s.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * High-level device driver
 */

#define IOCTL_CODEC_CONFIGURE       (1)
#define IOCTL_CODEC_START_OUTPUT    (2)
#define IOCTL_CODEC_STOP_OUTPUT     (3)
#define IOCTL_CODEC_START_INPUT     (4)
#define IOCTL_CODEC_STOP_INPUT      (5)
#define IOCTL_CODEC_SET_PROPERTY    (6)
#define IOCTL_CODEC_APPLY_PROPERTY  (7)
#define IOCTL_CODEC_GET_PROPERTY    (8)
#define IOCTL_CODEC_CLEAR_ERRORS    (9)
#define IOCTL_CODEC_REG_ERR_CB     (10)
#define IOCTL_CODEC_DUMP           (11)

/**
 * PCM audio sample rates
 */
typedef enum {
	AUDIO_PCM_RATE_8K	= 8000,		/**< 8 kHz sample rate */
	AUDIO_PCM_RATE_16K	= 16000,	/**< 16 kHz sample rate */
	AUDIO_PCM_RATE_24K	= 24000,	/**< 24 kHz sample rate */
	AUDIO_PCM_RATE_32K	= 32000,	/**< 32 kHz sample rate */
	AUDIO_PCM_RATE_44P1K	= 44100,	/**< 44.1 kHz sample rate */
	AUDIO_PCM_RATE_48K	= 48000,	/**< 48 kHz sample rate */
	AUDIO_PCM_RATE_96K	= 96000,	/**< 96 kHz sample rate */
	AUDIO_PCM_RATE_192K	= 192000,	/**< 192 kHz sample rate */
} audio_pcm_rate_t;

/**
 * PCM audio sample bit widths
 */
typedef enum {
	AUDIO_PCM_WIDTH_16_BITS	= 16,	/**< 16-bit sample width */
	AUDIO_PCM_WIDTH_20_BITS	= 20,	/**< 20-bit sample width */
	AUDIO_PCM_WIDTH_24_BITS	= 24,	/**< 24-bit sample width */
	AUDIO_PCM_WIDTH_32_BITS	= 32,	/**< 32-bit sample width */
} audio_pcm_width_t;

/**
 * Digital Audio Interface (DAI) type
 */
typedef enum {
	AUDIO_DAI_TYPE_I2S,	/**< I2S Interface */
	AUDIO_DAI_TYPE_INVALID,	/**< Other interfaces can be added here */
} audio_dai_type_t;

/**
 * Codec properties that can be set by audio_codec_set_property().
 */
typedef enum {
	AUDIO_PROPERTY_OUTPUT_VOLUME,	/**< Output volume */
	AUDIO_PROPERTY_OUTPUT_MUTE,	/**< Output mute/unmute */
	AUDIO_PROPERTY_INPUT_MUTE,	/**< Input mute/unmute */
} audio_property_t;

/**
 * Audio channel identifiers to use in audio_codec_set_property().
 */
typedef enum {
	AUDIO_CHANNEL_FRONT_LEFT,	/**< Front left channel */
	AUDIO_CHANNEL_FRONT_RIGHT,	/**< Front right channel */
	AUDIO_CHANNEL_LFE,		/**< Low frequency effect channel */
	AUDIO_CHANNEL_FRONT_CENTER,	/**< Front center channel */
	AUDIO_CHANNEL_REAR_LEFT,	/**< Rear left channel */
	AUDIO_CHANNEL_REAR_RIGHT,	/**< Rear right channel */
	AUDIO_CHANNEL_REAR_CENTER,	/**< Rear center channel */
	AUDIO_CHANNEL_SIDE_LEFT,	/**< Side left channel */
	AUDIO_CHANNEL_SIDE_RIGHT,	/**< Side right channel */
	AUDIO_CHANNEL_ALL,		/**< All channels */
} audio_channel_t;

/**
 * @brief Digital Audio Interface Configuration.
 *
 * Configuration is dependent on DAI type
 */
typedef union {
	struct i2s_config i2s;	/**< I2S configuration */
				/* Other DAI types go here */
} audio_dai_cfg_t;

/**
 * Codec configuration parameters
 */
struct audio_codec_cfg {
	uint32_t		mclk_freq;	/**< MCLK input frequency in Hz */
	audio_dai_type_t	dai_type;	/**< Digital interface type */
	audio_dai_cfg_t		dai_cfg;	/**< DAI configuration info */
};

/**
 * Codec (output) volume values
 */

typedef struct {
    float min;
    float cur;   /**< Volume level in 0.5dB resolution */
    float max;
} audio_codec_volume_t;

/**
 * Codec property values
 */
typedef union {
	audio_codec_volume_t	vol;
	bool	mute;	/**< Mute if @a true, unmute if @a false */
} audio_property_value_t;

/**
 * @brief Codec error type
 */
enum audio_codec_error_type {
	/** Output over-current */
	AUDIO_CODEC_ERROR_OVERCURRENT = BIT(0),

	/** Codec over-temperature */
	AUDIO_CODEC_ERROR_OVERTEMPERATURE = BIT(1),

	/** Power low voltage */
	AUDIO_CODEC_ERROR_UNDERVOLTAGE = BIT(2),

	/** Power high voltage */
	AUDIO_CODEC_ERROR_OVERVOLTAGE = BIT(3),

	/** Output direct-current */
	AUDIO_CODEC_ERROR_DC = BIT(4),
};

/**
 * @typedef audio_codec_error_callback_t
 * @brief Callback for error interrupt
 *
 * @param dev Pointer to the codec device
 * @param errors Device errors (bitmask of @ref audio_codec_error_type values)
 */
typedef void (*audio_codec_error_callback_t)(struct device *dev, uint32_t errors);

struct codec_configure_arg {
	struct audio_codec_cfg *cfg;
};

struct codec_property_arg {
	audio_property_t property;
	audio_channel_t channel;
	audio_property_value_t val;
};

struct codec_error_cb_arg {
	audio_codec_error_callback_t cb;
};

/* struct fops */
int codec_ioctl(struct file *file, unsigned int cmd, void *arg);

/*
 * Low-level device driver
 */

struct audio_codec_ops {
	int (*configure)(struct device *dev, struct audio_codec_cfg *cfg);
	int (*start_output)(struct device *dev);
	int (*stop_output)(struct device *dev);
	int (*start_input)(struct device *dev);
	int (*stop_input)(struct device *dev);
	int (*set_property)(struct device *dev,
			    audio_property_t property,
			    audio_channel_t channel,
			    audio_property_value_t val);
	int (*apply_properties)(struct device *dev);
	int (*get_property)(struct device *dev,
			    audio_property_t property,
			    audio_channel_t channel,
			    audio_property_value_t *val);
	int (*clear_errors)(struct device *dev);
	int (*register_error_callback)(struct device *dev,
			 audio_codec_error_callback_t cb);
    int (*dump)(struct device *dev);
};

#define codec_ops(x)  ((struct audio_codec_ops *)(x)->driver->ops)

/**
 * @brief Configure the audio codec
 *
 * Configure the audio codec device according to the configuration
 * parameters provided as input
 *
 * @param dev Pointer to the device structure for codec driver instance.
 * @param cfg Pointer to the structure containing the codec configuration.
 *
 * @return 0 on success, negative error code on failure
 */
static __inline__ int audio_codec_configure(struct device *dev,
					struct audio_codec_cfg *cfg)
{
	if (!dev)
		return -ENODEV;

	return codec_ops(dev)->configure(dev, cfg);
}

/**
 * @brief Set codec to start output audio playback
 *
 * Setup the audio codec device to start the audio playback
 *
 * @param dev Pointer to the device structure for codec driver instance.
 *
 * @return 0 on success, negative error code on failure
 */
static inline int audio_codec_start_output(struct device *dev)
{
	if (!dev)
		return -ENODEV;

	return codec_ops(dev)->start_output(dev);
}

/**
 * @brief Set codec to stop output audio playback
 *
 * Setup the audio codec device to stop the audio playback
 *
 * @param dev Pointer to the device structure for codec driver instance.
 *
 * @return 0 on success, negative error code on failure
 */
static inline int audio_codec_stop_output(struct device *dev)
{
	if (!dev)
		return -ENODEV;

	return codec_ops(dev)->stop_output(dev);
}

/**
 * @brief Set codec to start input audio record
 *
 * Setup the audio codec device to start the audio record
 *
 * @param dev Pointer to the device structure for codec driver instance.
 *
 * @return 0 on success, negative error code on failure
 */
static inline int audio_codec_start_input(struct device *dev)
{
	if (!dev)
		return -ENODEV;

    if (codec_ops(dev)->start_input == NULL) {
        return -ENOSYS;
    }

	return codec_ops(dev)->start_input(dev);
}

/**
 * @brief Set codec to stop input audio record
 *
 * Setup the audio codec device to stop the audio record
 *
 * @param dev Pointer to the device structure for codec driver instance.
 *
 * @return 0 on success, negative error code on failure
 */
static inline int audio_codec_stop_input(struct device *dev)
{
	if (!dev)
		return -ENODEV;

    if (codec_ops(dev)->stop_output == NULL) {
        return -ENOSYS;
    }

	return codec_ops(dev)->stop_input(dev);
}


/**
 * @brief Set a codec property defined by audio_property_t
 *
 * Set a property such as volume level, clock configuration etc.
 *
 * @param dev Pointer to the device structure for codec driver instance.
 * @param property The codec property to set
 * @param channel The audio channel for which the property has to be set
 * @param val pointer to a property value of type audio_codec_property_value_t
 *
 * @return 0 on success, negative error code on failure
 */
static inline int audio_codec_set_property(struct device *dev,
					   audio_property_t property,
					   audio_channel_t channel,
					   audio_property_value_t val)
{
	if (!dev)
		return -ENODEV;

	return codec_ops(dev)->set_property(dev, property, channel, val);
}

/**
 * @brief Atomically apply any cached properties
 *
 * Following one or more invocations of audio_codec_set_property, that may have
 * been cached by the driver, audio_codec_apply_properties can be invoked to
 * apply all the properties as atomic as possible
 *
 * @param dev Pointer to the device structure for codec driver instance.
 *
 * @return 0 on success, negative error code on failure
 */
static inline int audio_codec_apply_properties(struct device *dev)
{
	if (!dev)
		return -ENODEV;

	return codec_ops(dev)->apply_properties(dev);
}

/**
 * @brief Get a codec property defined by audio_property_t
 *
 * Get a property such as volume level, clock configuration etc.
 *
 * @param dev Pointer to the device structure for codec driver instance.
 * @param property The codec property to get
 * @param channel The audio channel for which the property has to be get
 * @param val pointer to a property value of type audio_codec_property_value_t
 *
 * @return 0 on success, negative error code on failure
 */
static inline int audio_codec_get_property(struct device *dev,
					   audio_property_t property,
					   audio_channel_t channel,
					   audio_property_value_t *val)
{
	if (!dev)
		return -ENODEV;

	return codec_ops(dev)->get_property(dev, property, channel, val);
}

/**
 * @brief Clear any codec errors
 *
 * Clear all codec errors.
 * If an error interrupt exists, it will be de-asserted.
 *
 * @param dev Pointer to the device structure for codec driver instance.
 *
 * @return 0 on success, negative error code on failure
 */
static inline int audio_codec_clear_errors(struct device *dev)
{
	if (!dev)
		return -ENODEV;

	if (codec_ops(dev)->clear_errors == NULL)
		return -ENOSYS;

	return codec_ops(dev)->clear_errors(dev);
}

/**
 * @brief Register a callback function for codec error
 *
 * The callback will be called from a thread, so I2C or SPI operations are
 * safe.  However, the thread's stack is limited and defined by the
 * driver.  It is currently up to the caller to ensure that the callback
 * does not overflow the stack.
 *
 * @param dev Pointer to the audio codec device
 * @param cb The function that should be called when an error is detected
 * fires
 *
 * @return 0 if successful, negative errno code if failure.
 */
static inline int audio_codec_register_error_callback(struct device *dev,
				     audio_codec_error_callback_t cb)
{
	if (!dev)
		return -ENODEV;

	if (codec_ops(dev)->register_error_callback == NULL)
		return -ENOSYS;

	return codec_ops(dev)->register_error_callback(dev, cb);
}

/**
 * @brief Dump registers
 *
 * @param dev Pointer to the audio codec device
 *
 * @return 0 if successful, negative errno code if failure.
 */
static inline int audio_codec_dump(struct device *dev)
{
	if (!dev)
		return -ENODEV;

	if (codec_ops(dev)->dump == NULL)
		return -ENOSYS;

	return codec_ops(dev)->dump(dev);
}

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* __CODEC_H__ */
