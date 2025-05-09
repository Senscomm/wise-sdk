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
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <cli.h>
#include <hal/rom.h>

#ifndef CONFIG_MEM_HEAP_DEBUG
#error "Select Kernel -> FreeRTOS kernel -> Memory option -> Debug enabled malloc/free and Record memory allocated function."
#endif
#ifdef CONFIG_CLI_HISTORY
#error "Unselect Command line interface -> Command history."
#endif

int debug_heap_leak(void)
{
	printf("Heap leak demo.\n");

	return 0;
}

/* Demonstrate how to use the 'heap list' command.
 */

static char *saved_string[10] = {0,};

static int save_string(const char *whatusave)
{
	char *str;
	int i;

	str = zalloc(strlen(whatusave) + 1);
	strncpy(str, whatusave, strlen(whatusave));

	for (i = 0; i < 10; i++) {
		if (saved_string[i] == NULL) {
			saved_string[i] = str;
			break;
		}
	}

	if (i == 10) {
		/* Oops, we forgot to free str.
		 */
		return -1;
	}

	return i;
}

static int purge_string(int idx)
{
	if (idx >= 0 && idx < 10) {
		if (saved_string[idx] != NULL) {
			free(saved_string[idx]);
			saved_string[idx] = NULL;
			return 0;
		}
	}

	return -1;
}

int do_save_string(int argc, char *argv[])
{
	const char *str_to_save;
	int index;

	argc--, argv++;

	if (argc == 1) {
		str_to_save = argv[0];
		index = save_string(str_to_save);
	} else {
		return CMD_RET_USAGE;
	}

	if (index >= 0) {
		printf("Saved %s to %d.\n", str_to_save, index);
	}
	else {
		printf("Couldn't save %s because there is no empty slot.\n", str_to_save);
	}

	return CMD_RET_SUCCESS;
}

CMD(save_string, do_save_string,
	"Save a string",
	"save_string <string you want to save>"
);

int do_purge_string(int argc, char *argv[])
{
	int index;

	argc--, argv++;

	if (argc == 1) {
		index = strtoul(argv[0], NULL, 10);
	} else {
		return CMD_RET_USAGE;
	}

	purge_string(index);

	return CMD_RET_SUCCESS;
}

CMD(purge_string, do_purge_string,
	"Purge a string",
	"purge_string <index returned from save_string>"
);

int do_list_string(int argc, char *argv[])
{
	int i, index = -1;

	argc--, argv++;

	if (argc == 1) {
		index = strtoul(argv[0], NULL, 10);
	}

	for (i = 0; i < 10; i++) {
		if ((index == -1 || index == i) && saved_string[i]) {
			printf("[%02d] %s\n", i, saved_string[i]);
		}
	}

	return CMD_RET_SUCCESS;
}

CMD(list_string, do_list_string,
	"List saved string(s)",
	"list_string (<index>)"
);
