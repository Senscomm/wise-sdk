/*
 * Copyright 2025-2026 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __SCM_AUDIO_H__
#define __SCM_AUDIO_H__

#include <stdint.h>

#include "wise_err.h"
#include "scm_i2s.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Audio interface type
 */
enum scm_audio_if {
    SCM_AUDIO_INPUT, /* MIC, ADC */
    SCM_AUDIO_OUTPUT /* HP(SPK), DAC */
};

/**
 * Audio volume
 */
struct scm_audio_volume {
    float min;
    float cur;
    float max;
};

/**
 * Audio CODEC device configuration
 */
struct scm_audio_cfg {
    uint32_t mclk_freq;             /* MCLK frequency */
	enum scm_i2s_wl word_length;	/* word length in bits */
	enum scm_i2s_fmt format;		/* I2S data format */
	enum scm_i2s_role role;			/* Role on I2S bus */
	uint32_t fs;					/* Sampling frequency, i.e., LRCLK frequency */
};

/**
 * @brief Initialize Audio CODEC
 */
int scm_audio_init(void);

/**
 * @brief Deinitialize Audio CODEC
 */
int scm_audio_deinit(void);

/**
 * @brief Configure Audio CODEC
 *
 * @param[in] cfg the cfg parameter
 */
int scm_audio_configure(struct scm_audio_cfg *cfg);

/**
 * @brief Start Audio CODEC interface
 */
int scm_audio_start(enum scm_audio_if intf);

/**
 * @brief Stop Audio CODEC interface
 */
int scm_audio_stop(enum scm_audio_if intf);

/**
 * @brief Get Audio (min/cur/max) volume
 */
int scm_audio_get_volume(struct scm_audio_volume *vol);

/**
 * @brief Set Audio current volume
 */
int scm_audio_set_volume(float vol);

/**
 * @brief Mute Audio
 */
int scm_audio_mute(enum scm_audio_if intf);

/**
 * @brief Unmute Audio
 */
int scm_audio_unmute(enum scm_audio_if intf);

#ifdef __cplusplus
}
#endif

#endif //__SCM_AUDIO_H__
