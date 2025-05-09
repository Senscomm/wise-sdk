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

int debug_stack_ovf(void)
{
	printf("Stack overflow demo.\n");

	return 0;
}

/* Stack overflow happens in 'init' thread, a.k.a. CLI thread.
 */

static int print_string(const char *whatuwant)
{
	char str[4*2048]; /* Oops! */

	/* Clear the content.
	 */
	memset(str, '0', sizeof(str)); /* Oops! */

	sprintf(str, "What you want is \" %s \"\n", whatuwant);

	printf(str);

	return 0;
}

int do_print_string(int argc, char *argv[])
{
	const char *str_to_print;

	argc--, argv++;

	if (argc == 1) {
		str_to_print = argv[0];
		print_string(str_to_print);
	} else {
		return CMD_RET_USAGE;
	}

	return CMD_RET_SUCCESS;
}

CMD(print_string, do_print_string,
	"Print a string",
	"print_string <string you want to print>"
);
