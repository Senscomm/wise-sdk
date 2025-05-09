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

#ifndef __AT_H__
#define __AT_H__

#include <u-boot/linker-lists.h>

#define AT_MAX_ARGV 		32
#define AT_MAX_CMDLINE_SIZE 	64

typedef enum {
	AT_RESULT_CODE_OK           = 0x00,       /*!< "OK" */
	AT_RESULT_CODE_ERROR        = 0x01,       /*!< "ERROR" */
	AT_RESULT_CODE_FAIL         = 0x02,       /*!< "ERROR" */
	AT_RESULT_CODE_SEND_OK      = 0x03,       /*!< "SEND OK" */
	AT_RESULT_CODE_SEND_FAIL    = 0x04,       /*!< "SEND FAIL" */
	AT_RESULT_CODE_IGNORE       = 0x05,       /*!< response nothing */
	AT_RESULT_CODE_PROCESS_DONE = 0x06,       /*!< response nothing */


	AT_RESULT_CODE_MAX
}	AT_RESULT_CODE;


typedef enum {
	CAT_TST, 	/* AT+<x>=? */
	CAT_QRY,  	/* AT+<x>? */
	CAT_SET,	/* AT+<x>=[...] */
	CAT_EXE,	/* AT+<x> */
	CAT_MAX,
}	AT_CAT;

typedef int (*AT_HNDLR) (int, char *[]);

struct at_cmd {
	const char *name;
	AT_HNDLR handler[CAT_MAX];
};

#define AT_CMD(_id_) \
	ll_entry_declare(struct at_cmd, _id_, at)

#define at_cmd_start() ll_entry_start(struct at_cmd, at)

#define at_cmd_end() ll_entry_end(struct at_cmd, at)

/* define AT command with "AT+" prefix */
#define ATPLUS(cmd, test, query, set, exec)		\
	AT_CMD(cmd) = {					\
		.name = "+"#cmd,			\
		.handler = {test, query, set, exec},	\
	}

/* define AT command without "AT+" prefix */
#define AT(cmd, test, query, set, exec)			\
	AT_CMD(cmd) = {					\
		.name = #cmd,				\
		.handler = {test, query, set, exec},	\
	}

#ifdef CONFIG_AT_OVER_IPC
void at_ipc_register(void);
#endif

#endif // __AT_H__
