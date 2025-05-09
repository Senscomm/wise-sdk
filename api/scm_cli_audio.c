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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

#include "hal/kernel.h"
#include "hal/console.h"
#include <hal/kmem.h>
#include "mem.h"
#include "cli.h"
#include "scm_audio.h"

static struct scm_audio_cfg audio_cli_cfg;

static int scm_cli_audio_init(int argc, char *argv[])
{
    int err;

    err = scm_audio_init();
    if (err) {
        printf("audio init error %x\n", err);
        return CMD_RET_FAILURE;
    }

    return CMD_RET_SUCCESS;
}

static int scm_cli_audio_deinit(int argc, char *argv[])
{
    int err;

    err = scm_audio_deinit();
    if (err) {
        printf("audio deinit error %x\n", err);
        return CMD_RET_FAILURE;
    }

    return CMD_RET_SUCCESS;
}

static int scm_cli_audio_config(int argc, char *argv[])
{
    int err;

    argc--;
    argv++;

    if (argc < 5) {
        return CMD_RET_USAGE;
    }

    audio_cli_cfg.mclk_freq = atoi(argv[0]);

    switch (atoi(argv[1])) {
        case 16:
            audio_cli_cfg.word_length = SCM_I2S_WL_16;
            break;
        case 20:
            audio_cli_cfg.word_length = SCM_I2S_WL_20;
            break;
        case 24:
            audio_cli_cfg.word_length = SCM_I2S_WL_24;
            break;
        default:
            return CMD_RET_USAGE;
    }

    switch (atoi(argv[2])) {
        case 0:
            audio_cli_cfg.format = SCM_I2S_FMT_I2S;
            break;
        case 1:
            audio_cli_cfg.format = SCM_I2S_FMT_LJ;
            break;
        case 2:
            audio_cli_cfg.format = SCM_I2S_FMT_RJ;
            break;
        default:
            return CMD_RET_USAGE;
    }

    audio_cli_cfg.role = (atoi(argv[3]) != 0) ? SCM_I2S_ROLE_MASTER \
                         : SCM_I2S_ROLE_SLAVE;
    audio_cli_cfg.fs = atoi(argv[4]);

    if ((err = scm_audio_configure(&audio_cli_cfg)) != WISE_OK) {
        return CMD_RET_FAILURE;
    }

    return CMD_RET_SUCCESS;
}

static int scm_cli_audio_start(int argc, char *argv[])
{
    enum scm_audio_if intf;
    int err;

    argc--;
    argv++;

    if (argc < 1) {
        return CMD_RET_USAGE;
    }

    if (!strcmp(argv[0], "mic")) {
        intf = SCM_AUDIO_INPUT;
    } else if (!strcmp(argv[0], "spk") || !strcmp(argv[0], "hp")) {
        intf = SCM_AUDIO_OUTPUT;
    } else {
        return CMD_RET_USAGE;
    }

    err = scm_audio_start(intf);
    if (err) {
        return CMD_RET_FAILURE;
    }

    return CMD_RET_SUCCESS;
}

static int scm_cli_audio_stop(int argc, char *argv[])
{
    enum scm_audio_if intf;
    int err;

    argc--;
    argv++;

    if (argc < 1) {
        return CMD_RET_USAGE;
    }

    if (!strcmp(argv[0], "mic")) {
        intf = SCM_AUDIO_INPUT;
    } else if (!strcmp(argv[0], "spk") || !strcmp(argv[0], "hp")) {
        intf = SCM_AUDIO_OUTPUT;
    } else {
        return CMD_RET_USAGE;
    }

    err = scm_audio_stop(intf);
    if (err) {
        return CMD_RET_FAILURE;
    }

    return CMD_RET_SUCCESS;
}

static int scm_cli_audio_mute(int argc, char *argv[])
{
    enum scm_audio_if intf;
    int err;

    argc--;
    argv++;

    if (argc < 1) {
        return CMD_RET_USAGE;
    }

    if (!strcmp(argv[0], "mic")) {
        intf = SCM_AUDIO_INPUT;
    } else if (!strcmp(argv[0], "spk") || !strcmp(argv[0], "hp")) {
        intf = SCM_AUDIO_OUTPUT;
    } else {
        return CMD_RET_USAGE;
    }

    err = scm_audio_mute(intf);
    if (err) {
        return CMD_RET_FAILURE;
    }

    return CMD_RET_SUCCESS;
}

static int scm_cli_audio_unmute(int argc, char *argv[])
{
    enum scm_audio_if intf;
    int err;

    argc--;
    argv++;

    if (argc < 1) {
        return CMD_RET_USAGE;
    }

    if (!strcmp(argv[0], "mic")) {
        intf = SCM_AUDIO_INPUT;
    } else if (!strcmp(argv[0], "spk") || !strcmp(argv[0], "hp")) {
        intf = SCM_AUDIO_OUTPUT;
    } else {
        return CMD_RET_USAGE;
    }

    err = scm_audio_unmute(intf);
    if (err) {
        return CMD_RET_FAILURE;
    }

    return CMD_RET_SUCCESS;
}

static int scm_cli_audio_volume(int argc, char *argv[])
{
    struct scm_audio_volume volume;
    float min, cur, max;
    float vol, adj;
    int err;

    argc--;
    argv++;

    if (argc < 1) {
        return CMD_RET_USAGE;
    }

    if (!strcmp(argv[0], "+")) {
        adj = 0.5;
    } else if (!strcmp(argv[0], "-")) {
        adj = -0.5;
    } else {
        return CMD_RET_USAGE;
    }

    err = scm_audio_get_volume(&volume);
    if (err) {
        return CMD_RET_FAILURE;
    }

    min = volume.min;
    cur = volume.cur;
    max = volume.max;

#if 0
    printf("\n[Cur: %*f dB]\n", 7, cur);
    printf("[Min: %*f dB]\n", 7, min);
    printf("[Max: %*f dB]\n", 7, max);
#endif

    vol = min(max(cur + adj, min), max);

    err = scm_audio_set_volume(vol);
    if (err) {
        return CMD_RET_FAILURE;
    }

    return CMD_RET_SUCCESS;
}

static const struct cli_cmd scm_cli_audio_cmd[] = {
    CMDENTRY(init, scm_cli_audio_init, "", ""),
    CMDENTRY(deinit, scm_cli_audio_deinit, "", ""),
    CMDENTRY(config, scm_cli_audio_config, "", ""),
    CMDENTRY(start, scm_cli_audio_start, "", ""),
    CMDENTRY(stop, scm_cli_audio_stop, "", ""),
    CMDENTRY(mute, scm_cli_audio_mute, "", ""),
    CMDENTRY(unmute, scm_cli_audio_unmute, "", ""),
    CMDENTRY(volume, scm_cli_audio_volume, "", ""),
};

static int do_scm_cli_audio(int argc, char *argv[])
{
    const struct cli_cmd *cmd;

    argc--;
    argv++;

    cmd = cli_find_cmd(argv[0], scm_cli_audio_cmd, ARRAY_SIZE(scm_cli_audio_cmd));
    if (cmd == NULL)
        return CMD_RET_USAGE;

    return cmd->handler(argc, argv);
}

CMD(audio, do_scm_cli_audio,
        "CLI commands for Audio CODEC API",
        "audio init" OR
        "audio deinit" OR
        "audio config (mclk freq.) (wl:16|20|24)\n\t(fmt:0(I2S)|1(LJ)|2(RJ)) (master:0|1) (sample freq.)" OR
        "audio start (mic|spk|hp)" OR
        "audio stop (mic|spk|hp)" OR
        "audio mute (mic|spk|hp)" OR
        "audio unmute (mic|spk|hp)" OR
        "audio volume (+|-)"
   );
