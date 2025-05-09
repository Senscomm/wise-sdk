/*
 * Copyright 2023-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
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
#include <cli.h>

int debug_exception(void)
{
	printf("Exception demo.\n");

	return 0;
}

/* Exceptions to be illustrated
 * - Illegal instruction, e.g.,'jump 0xc0000000'
 * - Instruction access fault, e.g., 'jump 0x12345678'
 * - Breakpoint (ebreak) by 'jump 0xabc' where 0xabc holds 'ebreak' instruction
 * - More...
 */

typedef void (*func_to_jump)(void);

void jump_to_func(func_to_jump func)
{
	(*func)();
	printf("Jump to %p was successful.\n", func);
}	

int do_jump_to_func(int argc, char *argv[])
{
	func_to_jump g_func_to_jump;

	argc--, argv++;

	if (argc == 1) {
		g_func_to_jump = (func_to_jump)strtoul(argv[0], NULL, 16);
	} else {
		return CMD_RET_USAGE;
	}

	jump_to_func(g_func_to_jump);

	return CMD_RET_SUCCESS;
}

CMD(jump_to_func, do_jump_to_func,
	"Jump to a function address",
	"jump_to_func <address of the function in hex.>"
);

