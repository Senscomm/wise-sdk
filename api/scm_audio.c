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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <cmsis_os.h>
#include "hal/codec.h"
#include "hal/kmem.h"

#include "vfs.h"
#include "sys/ioctl.h"
#include "scm_audio.h"

struct scm_audio_data {
    int fd;
    char *dev_name;
    struct audio_codec_cfg cfg;
};

static struct scm_audio_data audio_data = {
    .fd = -1,
    .dev_name = "/dev/audio",
};


int scm_audio_init(void)
{
    int fd;

    if (audio_data.fd > 0) {
        return WISE_ERR_INVALID_STATE;
    }

    fd = open(audio_data.dev_name, 0, 0);
    if (fd < 0) {
        return WISE_ERR_NOT_FOUND;
    }

    audio_data.fd = fd;

    return WISE_OK;
}

int scm_audio_deinit(void)
{
    if (audio_data.fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    close(audio_data.fd);
    audio_data.fd = -1;

    return WISE_OK;
}

int scm_audio_configure(struct scm_audio_cfg *cfg)
{
    struct codec_configure_arg arg;
    struct audio_codec_cfg *codec;
    struct i2s_config *i2s;

    if (audio_data.fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    arg.cfg = codec = &audio_data.cfg;
    i2s = &codec->dai_cfg.i2s;

    codec->mclk_freq = cfg->mclk_freq;
    codec->dai_type = AUDIO_DAI_TYPE_I2S;

    i2s->channels = 2; /* fixed */
    i2s->frame_clk_freq = cfg->fs;

    if (cfg->role == SCM_I2S_ROLE_MASTER) {
        i2s->options = I2S_OPT_BIT_CLK_MASTER | I2S_OPT_FRAME_CLK_MASTER;
    } else {
        i2s->options = I2S_OPT_BIT_CLK_SLAVE | I2S_OPT_FRAME_CLK_SLAVE;
    }

    switch (cfg->word_length) {
    case SCM_I2S_WL_16:
        i2s->word_size = 16;
        break;
    case SCM_I2S_WL_20:
        i2s->word_size = 20;
        break;
    case SCM_I2S_WL_24:
        i2s->word_size = 24;
        break;
    default:
        return WISE_ERR_INVALID_ARG;
    }

    switch (cfg->format) {
    case SCM_I2S_FMT_I2S:
        i2s->format = I2S_FMT_DATA_FORMAT_I2S;
        break;
    case SCM_I2S_FMT_LJ:
        i2s->format = I2S_FMT_DATA_FORMAT_LEFT_JUSTIFIED;
        break;
    case SCM_I2S_FMT_RJ:
        i2s->format = I2S_FMT_DATA_FORMAT_RIGHT_JUSTIFIED;
        break;
    default:
        return WISE_ERR_INVALID_ARG;
    }

    if (ioctl(audio_data.fd, IOCTL_CODEC_CONFIGURE, &arg)) {
        return WISE_ERR_IOCTL;
    }

    return WISE_OK;
}

int scm_audio_start(enum scm_audio_if intf)
{
    uint32_t cmd;

    if (audio_data.fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    if (intf == SCM_AUDIO_INPUT) {
        cmd = IOCTL_CODEC_START_INPUT;
    } else if (intf == SCM_AUDIO_OUTPUT) {
        cmd = IOCTL_CODEC_START_OUTPUT;
    } else {
        return WISE_ERR_INVALID_ARG;
    }

    if (ioctl(audio_data.fd, cmd, NULL)) {
        return WISE_ERR_IOCTL;
    }

    return WISE_OK;
}

int scm_audio_stop(enum scm_audio_if intf)
{
    uint32_t cmd;

    if (audio_data.fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    if (intf == SCM_AUDIO_INPUT) {
        cmd = IOCTL_CODEC_STOP_INPUT;
    } else if (intf == SCM_AUDIO_OUTPUT) {
        cmd = IOCTL_CODEC_STOP_OUTPUT;
    } else {
        return WISE_ERR_INVALID_ARG;
    }

    if (ioctl(audio_data.fd, cmd, NULL)) {
        return WISE_ERR_IOCTL;
    }

    return WISE_OK;
}

int scm_audio_get_volume(struct scm_audio_volume *vol)
{
	struct codec_property_arg prop_arg;

    if (audio_data.fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

	prop_arg.property = AUDIO_PROPERTY_OUTPUT_VOLUME;
	prop_arg.channel = AUDIO_CHANNEL_ALL;

	if (ioctl(audio_data.fd, IOCTL_CODEC_GET_PROPERTY, &prop_arg)) {
        return WISE_ERR_IOCTL;
	}

    vol->min = prop_arg.val.vol.min;
    vol->cur = prop_arg.val.vol.cur;
    vol->max = prop_arg.val.vol.max;

    return WISE_OK;
}

int scm_audio_set_volume(float vol)
{
	struct codec_property_arg prop_arg;

    if (audio_data.fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

	prop_arg.property = AUDIO_PROPERTY_OUTPUT_VOLUME;
	prop_arg.channel = AUDIO_CHANNEL_ALL;
    prop_arg.val.vol.cur = vol; /* min, max doesn't matter. */

	if (ioctl(audio_data.fd, IOCTL_CODEC_SET_PROPERTY, &prop_arg)) {
        return WISE_ERR_IOCTL;
	}

    return WISE_OK;
}

int scm_audio_mute(enum scm_audio_if intf)
{
	struct codec_property_arg prop_arg;

    if (audio_data.fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    if (intf == SCM_AUDIO_INPUT) {
	    prop_arg.property = AUDIO_PROPERTY_INPUT_MUTE;
    } else if (intf == SCM_AUDIO_OUTPUT) {
	    prop_arg.property = AUDIO_PROPERTY_OUTPUT_MUTE;
    } else {
        return WISE_ERR_INVALID_ARG;
    }

	prop_arg.channel = AUDIO_CHANNEL_ALL;
    prop_arg.val.mute = true;

	if (ioctl(audio_data.fd, IOCTL_CODEC_SET_PROPERTY, &prop_arg)) {
        return WISE_ERR_IOCTL;
	}

    return WISE_OK;
}

int scm_audio_unmute(enum scm_audio_if intf)
{
	struct codec_property_arg prop_arg;

    if (audio_data.fd <= 0) {
        return WISE_ERR_INVALID_STATE;
    }

    if (intf == SCM_AUDIO_INPUT) {
	    prop_arg.property = AUDIO_PROPERTY_INPUT_MUTE;
    } else if (intf == SCM_AUDIO_OUTPUT) {
	    prop_arg.property = AUDIO_PROPERTY_OUTPUT_MUTE;
    } else {
        return WISE_ERR_INVALID_ARG;
    }

	prop_arg.channel = AUDIO_CHANNEL_ALL;
    prop_arg.val.mute = false;

	if (ioctl(audio_data.fd, IOCTL_CODEC_SET_PROPERTY, &prop_arg)) {
        return WISE_ERR_IOCTL;
	}

    return WISE_OK;
}
