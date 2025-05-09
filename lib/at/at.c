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

#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>

#include <cli.h>
#include <at.h>
#include <at-wifi.h>

#ifdef DEBUG
#undef DEBUG
#endif

void at_print_args(int argc, char *argv[])
{
#ifdef DEBUG
    int i;
    for (i = 0; i < argc; i++)
        printf("[%d] %s\n", i, argv[i]);
#endif
}

/* to strip out prefixing and suffixing "" */
char *at_strip_args(char *args)
{
    char *c = args;
    if (*c != '"')
        return args;

    c++;
    while (!isblank((int)*c) && *c != '"')
        c++;
    if (*c == '"')
        *c = '\0';

    return (args + 1);
}

const struct at_cmd *at_find_cmd(char *cmd, const struct at_cmd *table, int nr)
{
    const struct at_cmd *t;

    for (t = table; t < table + nr; t++) {
        if (strcmp(cmd, t->name) == 0 &&
                strlen(t->name) == strlen(cmd))
            return t;
    }
    return NULL;
}

static int at_result(int rc)
{
    int ret;
    switch (rc) {
        case AT_RESULT_CODE_ERROR:
        case AT_RESULT_CODE_FAIL:
        case AT_RESULT_CODE_SEND_FAIL:
        case AT_RESULT_CODE_IGNORE:
            ret = CMD_RET_FAILURE;
            break;
        case AT_RESULT_CODE_OK:
        case AT_RESULT_CODE_SEND_OK:
        case AT_RESULT_CODE_PROCESS_DONE:
            ret = CMD_RET_SUCCESS;
            break;
        default:
            ret = CMD_RET_FAILURE;
            break;
    }

    return ret;
}

int at_process_cmd(int argc, char *argv[], AT_CAT type)
{
    const struct at_cmd *start, *end, *cmd;
    int rc = CMD_RET_FAILURE;

    start = at_cmd_start();
    end = at_cmd_end();

    if (!argc)
        goto exit;

    if (type != CAT_SET && argc > 2)
        goto exit;

    cmd = at_find_cmd(argv[AT_CMD_NAME], start, end - start);
    if (!cmd)
        goto exit;

    optind = 0;
    if (cmd->handler[type])
        rc = at_result(cmd->handler[type](argc, argv));
    else
        rc = CMD_RET_FAILURE;

exit:
    if (rc == CMD_RET_SUCCESS)
        at_printf("\r\nOK\r\n");
    else
        at_printf("\r\nERROR\r\n");

    return rc;
}

/**
 * at_parse_line() - break arguments into argv
 *
 * @s: command line string after the command
 * @argv: argv array
 *
 * Return: # of arguments
 *
 * This function changes the content of @s.
 */
int at_parse_line(char **s, char *argv[], AT_CAT *type)
{
    int argc = 0;
    char *tmp = *s;
    char *ifce = NULL;
    bool ifound = false;

    /* search for "AT+" prefix */
    *s = strstr(*s, "AT+");
    if (*s == 0) {
        *s = tmp;
        while (isblank((int)**s))
            (*s)++;

        if (**s == '\0')
            goto out;

        /* for commands without prefix "AT+",
         * argv[AT_CMD_NAME] will have the command word */
        argc = AT_CMD_NAME;
        argv[argc++] = *s;
        *type = CAT_EXE;
        goto out;
    }

    /* search for "WL0+" prefix */
    ifce = strstr(*s, "WL0+");
    if(ifce) {
        argv[argc++] = "0";
        ifound = true;
    }

#ifdef CONFIG_SUPPORT_DUAL_VIF
    ifce = strstr(*s, "WL1+");
    if(ifce) {
        argv[argc++] = "1";
        ifound = true;
    }
#endif

    if (!ifound)
        argv[argc++] = "0";

    /* for commands such as "AT+<x>...",
     * argv[0] will have "+<x>", which can be
     * used creating a response message */
    *s += (2 + (ifound ? 4 : 0));

    /* determine the type of AT command */
    tmp = *s;
    if ((tmp = strstr(*s, "=?"))) {
        *type = CAT_TST;
        *tmp++ = ' ';
        *tmp = ' ';
    }
    else if ((tmp = strstr(*s, "?"))) {
        *type = CAT_QRY;
        *tmp = ' ';
    }
    else if ((tmp = strstr(*s, "="))) {
        *type = CAT_SET;
        *tmp = ' ';
    }
    else
        *type = CAT_EXE;

    while (argc < AT_MAX_ARGV) {
        /* Eat up white spaces */
        while (isblank((int)**s))
            (*s)++;

        if (**s == '\0')
            goto out;

        argv[argc++] = *s;
        while (**s && !isblank((int)**s) && **s != ',')
            (*s)++;

        if (**s == '\0')
            goto out;

        **s = '\0';
        (*s)++;
    }

out:
    argv[argc] = NULL;
    return argc;
}

static int echo_on = 1;
void at_echo(int on)
{
    echo_on = on;
}

#define xstr(s) str(s)
#define str(s) #s
#define AT_CONSOLE "/dev/ttyS" xstr(CONFIG_AT_UART_PORT)

static FILE *term = NULL;
static int tfd = 0;

int at_term_fd(void)
{
    return tfd;
}

int at_getchar_timeout(unsigned timeout)
{
    int ret;
    struct pollfd fd = {tfd, POLLIN, 0};
    ret = poll(&fd, 1, timeout);
    if (ret <= 0)
        return -1;
    return getc(term);
}

int at_getchar(void)
{
    return getc(term);
}

static int at_readline(char *buffer, int blen)
{
    char *p = buffer;
    char *p_buf = p;
    int n = 0, plen = 0, col, c;

    col = plen;

    buffer[0] = '\0';

    while (p < buffer + blen) {
        while ((c = at_getchar()) < 0);

        switch (c) {
            case '\r':
                continue;
            case '\n':
                *p = '\0';
                if (echo_on)
                    fputs("\r\n", term);
                return p - p_buf;
            case '\0':
                continue;
            default:
                /*
                 * Must be a normal character then
                 */
                if (n < blen-2) {
                    if (echo_on)
                        fputc(c, term);
                    *p++ = c;
                    ++n;
                    ++col;
                } else {			/* Buffer full */
                    fputc('\a', term);
                }
        }
    }

    return n;
}

int at_printf(const char *fmt, ...)
{
    va_list va;
    int ret;

    va_start(va, fmt);
    ret = vfprintf(term, fmt, va);
    va_end(va);

    return ret;
}


#ifdef CONFIG_AT_OVER_IPC
ipc_listener_t g_wlan_ctrl_listener;
ipc_listener_t g_sys_ctrl_listener;

#define GET_LISTENER_BY_MODULE(m) \
    ((m) == IPC_MODULE_WLAN ? &g_wlan_ctrl_listener : \
     (m) == IPC_MODULE_SYS ? &g_sys_ctrl_listener : NULL)

int at_ipc_send_event (uint8_t *event, size_t size)
{
    struct device * ipc_dev = device_get_by_name("scm2010-ipc");
    ipc_payload_t *payload;

    /* Allocate payload to carry the control request. */

    payload = ipc_alloc(ipc_dev, size, false, true, IPC_MODULE_WLAN, IPC_CHAN_EVENT,
            IPC_TYPE_REQUEST);
    if (!payload)
        goto error;

    payload->seq = 0;
    /* Send out the event. */
    ipc_copyto(ipc_dev, event, size, payload, 0);
    ipc_transmit(ipc_dev, payload);

    return 0;

error:

    return -ENOBUFS;
}

int at_ipc_ctrl_resp(uint8_t *result, size_t size, uint8_t module)
{
    struct device * ipc_dev = device_get_by_name("scm2010-ipc");
    ipc_payload_t *ctrl_resp;
    ipc_listener_t *listener = GET_LISTENER_BY_MODULE(module);

    if (!listener)
        assert(0);

    ctrl_resp = ipc_alloc(ipc_dev, size, false, true, module, IPC_CHAN_CONTROL,
            IPC_TYPE_RESPONSE);

    if (!ctrl_resp)
        return CMD_RET_FAILURE;

    ctrl_resp->seq = listener->seq;
    listener->seq = 0;

    ipc_copyto(ipc_dev, result, size, ctrl_resp, 0);

    return ipc_transmit(ipc_dev, ctrl_resp);
}

int  at_ipc_ctrl_recvd(ipc_payload_t *payload, void *priv)
{
    struct device *dev = (struct device *)priv;
    int argc;
    char *argv[AT_MAX_ARGV];
    uint8_t cmdbuf[AT_MAX_CMDLINE_SIZE] = {0};
    int readlen;
    AT_CAT type;
    char *tmp;
    int resp_result = CMD_RET_SUCCESS;
    size_t size = sizeof(resp_result);
    ipc_listener_t *listener = NULL;
    uint8_t module = IPC_GET_MODULE(payload->flag);;

    listener = GET_LISTENER_BY_MODULE(module);
    if (!listener || !module)
        assert(0);
    readlen = ipc_copyfrom(dev, cmdbuf, sizeof(cmdbuf) - 1, payload, 0);
    listener->seq = payload->seq;

    if(readlen > 0) {
        tmp = (char *) cmdbuf;
        argc = at_parse_line(&tmp, argv, &type);
        resp_result = at_process_cmd(argc, argv, type);
    }
    else
        resp_result = CMD_RET_UNHANDLED;

    ipc_free(dev, payload);

#ifdef DEBUG
    printf("module:%d seq:%d resp_result:%d tmp:%s name:%s type:%d\n", module, payload->seq,
            resp_result, cmdbuf, argv[AT_CMD_NAME], type);
#endif

    if (listener->seq &&
            ((type == CAT_EXE) || (type == CAT_SET))) {
        resp_result = at_ipc_ctrl_resp((uint8_t *) &resp_result,    size, module);
    }

    return resp_result;
}

static void at_ipc_wlan_register(void)
{
    struct device * ipc_dev = device_get_by_name("scm2010-ipc");

    if(g_wlan_ctrl_listener.cb != NULL && g_wlan_ctrl_listener.type != IPC_TYPE_UNUSED)
        return;

    g_wlan_ctrl_listener.cb = at_ipc_ctrl_recvd;
    g_wlan_ctrl_listener.type = IPC_TYPE_REQUEST;
    g_wlan_ctrl_listener.priv = ipc_dev;
    if (ipc_addcb(ipc_dev, IPC_MODULE_WLAN, IPC_CHAN_CONTROL, &g_wlan_ctrl_listener))
        printf("%s not success\n", __func__);
}

void at_ipc_sys_register(void)
{
    struct device * ipc_dev = device_get_by_name("scm2010-ipc");

    if(g_sys_ctrl_listener.cb != NULL && g_sys_ctrl_listener.type != IPC_TYPE_UNUSED)
        return;

    g_sys_ctrl_listener.cb = at_ipc_ctrl_recvd;
    g_sys_ctrl_listener.type = IPC_TYPE_REQUEST;
    g_sys_ctrl_listener.priv = ipc_dev;
    if (ipc_addcb(ipc_dev, IPC_MODULE_SYS, IPC_CHAN_CONTROL, &g_sys_ctrl_listener))
        printf("%s not success\n", __func__);
}

void at_ipc_register(void)
{
    at_ipc_wlan_register();

    at_ipc_sys_register();

}
#endif

int at_run_command(char *line)
{
    int len, argc;
    char *argv[AT_MAX_ARGV];
    char *cmdline = malloc(AT_MAX_CMDLINE_SIZE);
    char *tmp;
    AT_CAT type;
    int ret __maybe_unused;

    if (cmdline == NULL)
        return -1;

    printf("ready\n");

    tfd = open(AT_CONSOLE, O_RDWR, 0);
    term = fdopen(tfd, "r+"); /* rw */

    /* flush any remaining CR/LF */
    while (at_getchar_timeout(0) > 0);

    while (1) {
        len = at_readline(cmdline, AT_MAX_CMDLINE_SIZE);
        if (len <= 0)
            continue;
        tmp = cmdline;
        argc = at_parse_line(&tmp, argv, &type);
        if (argc == 2 && strcmp(argv[AT_CMD_NAME], "quit") == 0)
            break;
        at_process_cmd(argc, argv, type);
    }

    free(cmdline);
    return CMD_RET_SUCCESS;
}

int do_at(int argc, char *argv[])
{
    if (argc > 1)
        return CMD_RET_USAGE;

    return at_run_command(argv[0]);
}

CMD(at, do_at, "AT command handler", NULL);
