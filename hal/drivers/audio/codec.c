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

#include <stdio.h>
#include <hal/init.h>
#include <hal/kernel.h>
#include <hal/pinctrl.h>
#include <hal/device.h>
#include <hal/console.h>
#include <hal/kmem.h>
#include <hal/codec.h>

#include "sys/ioctl.h"
#include "vfs.h"

int codec_ioctl(struct file *file, unsigned int cmd, void *arg)
{
	struct device *dev = file->f_priv;
	int ret = 0;

	switch (cmd) {
	case IOCTL_CODEC_CONFIGURE: {
		struct codec_configure_arg *cfg_arg = arg;
		ret = audio_codec_configure(dev, cfg_arg->cfg);
		break;
	}
	case IOCTL_CODEC_START_OUTPUT: {
		ret = audio_codec_start_output(dev);
		break;
	}
	case IOCTL_CODEC_STOP_OUTPUT: {
		ret = audio_codec_stop_output(dev);
		break;
	}
	case IOCTL_CODEC_START_INPUT: {
		ret = audio_codec_start_input(dev);
		break;
	}
	case IOCTL_CODEC_STOP_INPUT: {
		ret = audio_codec_stop_input(dev);
		break;
	}
	case IOCTL_CODEC_SET_PROPERTY: {
		struct codec_property_arg *prop_arg = arg;
		ret = audio_codec_set_property(dev, prop_arg->property,
				prop_arg->channel, prop_arg->val);
		break;
	}
	case IOCTL_CODEC_APPLY_PROPERTY: {
		ret = audio_codec_apply_properties(dev);
		break;
    }
	case IOCTL_CODEC_GET_PROPERTY: {
		struct codec_property_arg *prop_arg = arg;
		ret = audio_codec_get_property(dev, prop_arg->property,
				prop_arg->channel, &prop_arg->val);
		break;
	}
	case IOCTL_CODEC_CLEAR_ERRORS: {
		ret = audio_codec_clear_errors(dev);
		break;
    }
	case IOCTL_CODEC_REG_ERR_CB: {
		struct codec_error_cb_arg *error_cb_arg = arg;
		ret = audio_codec_register_error_callback(dev, error_cb_arg->cb);
		break;
    }
	case IOCTL_CODEC_DUMP: {
		ret = audio_codec_dump(dev);
		break;
    }
	default:
		ret = -EINVAL;
	}

	return ret;
}

#ifdef CONFIG_AUDIO_CLI

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "cli.h"

static struct audio_codec_cfg audio_cli_config;

static int cli_codec_configure(int argc, char *argv[])
{
	int fd = -1;
	struct codec_configure_arg cfg_arg;
	struct i2s_config *i2s_cfg;
	u32 mclk_freq;
	u8 channels, wordlength;
	i2s_fmt_t format;
	bool master;
	u32 frame_clk_freq;
	int ret = CMD_RET_SUCCESS;

	if (argc < 6) {
		ret = CMD_RET_USAGE;
		goto exit;
	}

	mclk_freq = atoi(argv[0]);
	channels = atoi(argv[1]);
	if (channels > 2) {
		ret = CMD_RET_USAGE;
		goto exit;
	}
	wordlength = atoi(argv[2]);
	if (wordlength != 16 && wordlength != 20 && wordlength != 24) {
		ret = CMD_RET_USAGE;
		goto exit;
	}
	switch (atoi(argv[3])) {
	case 0:
		format = I2S_FMT_DATA_FORMAT_I2S;
		break;
	case 1:
		format = I2S_FMT_DATA_FORMAT_LEFT_JUSTIFIED;
		break;
	case 2:
		format = I2S_FMT_DATA_FORMAT_RIGHT_JUSTIFIED;
		break;
	default:
		ret = CMD_RET_USAGE;
		goto exit;
	}
	master = (atoi(argv[4]) != 0);
	frame_clk_freq = atoi(argv[5]);

	cfg_arg.cfg = &audio_cli_config;
	cfg_arg.cfg->mclk_freq = mclk_freq;
	cfg_arg.cfg->dai_type = AUDIO_DAI_TYPE_I2S;

	i2s_cfg = &cfg_arg.cfg->dai_cfg.i2s;

	i2s_cfg->word_size = wordlength;
	i2s_cfg->channels = channels;
	i2s_cfg->format = format;
	if (master) {
		i2s_cfg->options = I2S_OPT_BIT_CLK_MASTER | I2S_OPT_FRAME_CLK_MASTER;
	} else {
		i2s_cfg->options = I2S_OPT_BIT_CLK_SLAVE | I2S_OPT_FRAME_CLK_SLAVE;
	}
	i2s_cfg->frame_clk_freq = frame_clk_freq;

	fd = open("/dev/audio", 0, 0);
	if (fd < 0) {
		printf("[%s, %d] Cannot open /dev/codec, fd: %d\n", __func__, __LINE__, fd);
		ret = CMD_RET_FAILURE;
		goto exit;
	}

	if (ioctl(fd, IOCTL_CODEC_CONFIGURE, &cfg_arg) < 0) {
		printf("[%s, %d] IOCTL_CODEC_CONFIGURE failed.\n", __func__, __LINE__);
		ret = CMD_RET_FAILURE;
		goto exit;
	}

exit:
	if (fd >= 0)
		close(fd);

	return ret;
}

static int cli_codec_start(int argc, char *argv[])
{
	int fd = -1;
	unsigned cmd;
	int ret = CMD_RET_SUCCESS;

	if (argc < 1) {
		ret = CMD_RET_USAGE;
		goto exit;
	}

	if (!strcmp(argv[0], "mic")) {
		cmd = IOCTL_CODEC_START_INPUT;
	} else if (!strcmp(argv[0], "spk") || !strcmp(argv[0], "hp")) {
		cmd = IOCTL_CODEC_START_OUTPUT;
	} else {
		ret = CMD_RET_USAGE;
		goto exit;
	}

	fd = open("/dev/audio", O_RDWR, 0);
	if (fd < 0) {
		printf("[%s, %d] Cannot open /dev/audio, fd: %d\n", __func__, __LINE__, fd);
		ret = CMD_RET_FAILURE;
		goto exit;
	}

	ret = ioctl(fd, cmd, NULL);
	if (ret < 0) {
		ret = CMD_RET_FAILURE;
		goto exit;
	}

exit:

	if (fd >= 0)
		close(fd);

	return ret;
}

static int cli_codec_stop(int argc, char *argv[])
{
	int fd = -1;
	unsigned cmd;
	int ret = CMD_RET_SUCCESS;

	if (argc < 1) {
		ret = CMD_RET_USAGE;
		goto exit;
	}

	if (!strcmp(argv[0], "mic")) {
		cmd = IOCTL_CODEC_STOP_INPUT;
	} else if (!strcmp(argv[0], "spk") || !strcmp(argv[0], "hp")) {
		cmd = IOCTL_CODEC_STOP_OUTPUT;
	} else {
		ret = CMD_RET_USAGE;
		goto exit;
	}

	fd = open("/dev/audio", O_RDWR, 0);
	if (fd < 0) {
		printf("[%s, %d] Cannot open /dev/audio, fd: %d\n", __func__, __LINE__, fd);
		ret = CMD_RET_FAILURE;
		goto exit;
	}

	ret = ioctl(fd, cmd, NULL);
	if (ret < 0) {
		ret = CMD_RET_FAILURE;
		goto exit;
	}

exit:

	if (fd >= 0)
		close(fd);

	return ret;
}

static int cli_codec_mute(int argc, char *argv[])
{
	int fd = -1;
	struct codec_property_arg prop_arg;
	int ret = CMD_RET_SUCCESS;

	if (argc < 1) {
		ret = CMD_RET_USAGE;
		goto exit;
	}

	if (!strcmp(argv[0], "mic")) {
	    prop_arg.property = AUDIO_PROPERTY_INPUT_MUTE;
	} else if (!strcmp(argv[0], "spk") || !strcmp(argv[0], "hp")) {
	    prop_arg.property = AUDIO_PROPERTY_OUTPUT_MUTE;
    } else {
		ret = CMD_RET_USAGE;
		goto exit;
    }
	prop_arg.channel = AUDIO_CHANNEL_ALL;
	prop_arg.val.mute = true;

	fd = open("/dev/audio", O_RDWR, 0);
	if (fd < 0) {
		printf("[%s, %d] Cannot open /dev/audio, fd: %d\n", __func__, __LINE__, fd);
		ret = CMD_RET_FAILURE;
		goto exit;
	}

	ret = ioctl(fd, IOCTL_CODEC_SET_PROPERTY, &prop_arg);
	if (ret < 0) {
		ret = CMD_RET_FAILURE;
		goto exit;
	}

exit:

	if (fd >= 0)
		close(fd);

	return ret;
}

static int cli_codec_unmute(int argc, char *argv[])
{
	int fd = -1;
	struct codec_property_arg prop_arg;
	int ret = CMD_RET_SUCCESS;

	if (argc < 1) {
		ret = CMD_RET_USAGE;
		goto exit;
	}

	if (!strcmp(argv[0], "mic")) {
	    prop_arg.property = AUDIO_PROPERTY_INPUT_MUTE;
	} else if (!strcmp(argv[0], "spk") || !strcmp(argv[0], "hp")) {
	    prop_arg.property = AUDIO_PROPERTY_OUTPUT_MUTE;
    } else {
		ret = CMD_RET_USAGE;
		goto exit;
    }
	prop_arg.channel = AUDIO_CHANNEL_ALL;
	prop_arg.val.mute = false;

	fd = open("/dev/audio", O_RDWR, 0);
	if (fd < 0) {
		printf("[%s, %d] Cannot open /dev/audio, fd: %d\n", __func__, __LINE__, fd);
		ret = CMD_RET_FAILURE;
		goto exit;
	}

	ret = ioctl(fd, IOCTL_CODEC_SET_PROPERTY, &prop_arg);
	if (ret < 0) {
		ret = CMD_RET_FAILURE;
		goto exit;
	}

exit:

	if (fd >= 0)
		close(fd);

	return ret;
}

static int cli_codec_vol(int argc, char *argv[])
{
    int fd = -1;
    struct codec_property_arg prop_arg;
    int ret = CMD_RET_SUCCESS;
    float min, cur, max;
    float vol, adj;

	if (argc < 1) {
		ret = CMD_RET_USAGE;
		goto exit;
	}

	if (!strcmp(argv[0], "+")) {
        adj = 0.5;
    } else if (!strcmp(argv[0], "-")) {
        adj = -0.5;
    } else {
		ret = CMD_RET_USAGE;
		goto exit;
    }

	fd = open("/dev/audio", O_RDWR, 0);
	if (fd < 0) {
		printf("[%s, %d] Cannot open /dev/audio, fd: %d\n", __func__, __LINE__, fd);
		ret = CMD_RET_FAILURE;
		goto exit;
	}

	prop_arg.property = AUDIO_PROPERTY_OUTPUT_VOLUME;
	prop_arg.channel = AUDIO_CHANNEL_ALL;

	ret = ioctl(fd, IOCTL_CODEC_GET_PROPERTY, &prop_arg);
	if (ret < 0) {
		printf("[%s, %d] IOCTL_CODEC_GET_PROPERTY failed. %d\n", __func__, __LINE__, ret);
		ret = CMD_RET_FAILURE;
		goto exit;
	}

    min = prop_arg.val.vol.min;
    cur = prop_arg.val.vol.cur;
    max = prop_arg.val.vol.max;

    printf("\n[Cur: %*f dB]\n", 7, cur);
    printf("[Min: %*f dB]\n", 7, min);
    printf("[Max: %*f dB]\n", 7, max);

    vol = prop_arg.val.vol.cur;

    vol += adj;
    if (vol > max) {
        vol = max;
    }
    if (vol < min) {
        vol = min;
    }

    printf("\n[New: %*f dB]\n\n", 7, vol);

	prop_arg.val.vol.cur = vol;

	ret = ioctl(fd, IOCTL_CODEC_SET_PROPERTY, &prop_arg);
	if (ret < 0) {
		printf("[%s, %d] IOCTL_CODEC_SET_PROPERTY failed. %d\n", __func__, __LINE__, ret);
		ret = CMD_RET_FAILURE;
		goto exit;
	}

exit:

	if (fd >= 0)
		close(fd);

	return ret;
}

static int cli_codec_dump(int argc, char *argv[])
{
	int fd = -1;
	int ret = CMD_RET_SUCCESS;

	fd = open("/dev/audio", O_RDWR, 0);
	if (fd < 0) {
		printf("[%s, %d] Cannot open /dev/audio, fd: %d\n", __func__, __LINE__, fd);
		ret = CMD_RET_FAILURE;
		goto exit;
	}

	ret = ioctl(fd, IOCTL_CODEC_DUMP, NULL);
	if (ret < 0) {
		printf("[%s, %d] IOCTL_CODEC_DUMP failed. %d\n", __func__, __LINE__, ret);
		ret = CMD_RET_FAILURE;
		goto exit;
	}

exit:

	if (fd >= 0)
		close(fd);

	return ret;
}

static int do_codec(int argc, char *argv[])
{
	argc--; argv++;

	if (!strcmp("config", argv[0])) {
		argc--; argv++;
		if (!argc) {
			goto usage;
		}
		return cli_codec_configure(argc, argv);
	} else if (!strcmp("start", argv[0])) {
		argc--; argv++;
		if (!argc) {
			goto usage;
		}
		return cli_codec_start(argc, argv);
	} else if (!strcmp("stop", argv[0])) {
		argc--; argv++;
		if (!argc) {
			goto usage;
		}
		return cli_codec_stop(argc, argv);
	} else if (!strcmp("mute", argv[0])) {
		argc--; argv++;
		return cli_codec_mute(argc, argv);
	} else if (!strcmp("unmute", argv[0])) {
		argc--; argv++;
		return cli_codec_unmute(argc, argv);
	} else if (!strcmp("vol", argv[0])) {
		argc--; argv++;
		if (!argc) {
			goto usage;
		}
		return cli_codec_vol(argc, argv);
	} else if (!strcmp("dump", argv[0])) {
        return cli_codec_dump(argc, argv);
    }

usage:
	return CMD_RET_USAGE;
}

CMD(codec, do_codec,
	"CLI commands for Audio CODEC driver",
	"codec config (mclk freq.) (channels: 1|2) (wl:16|20|24)\n\t(fmt:0(I2S)|1(LJ)|2(RJ)) (master:0|1) (sample freq.)" OR
    "codec start (mic|spk|hp)" OR
    "codec stop (mic|spk|hp)" OR
    "codec mute (mic|spk|hp)" OR
    "codec unmute (mic|spk|hp)" OR
	"codec vol (+|-)" OR
    "codec dump"
);

#endif
