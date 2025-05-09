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

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include <cli.h>

static const struct cli_cmd *cmd_locked;

/* XXX don't use isblank() from ctype because it will bring
 * in lots of rodata, especially in wise.ram */
static inline char is_blank(int c)
{
	if (c > 0 && c < 0x21)
		return c;
	else
		return 0;
}

/**
 * cli_parse_line() - break arguments into argv
 *
 * @s: command line string after the command
 * @argv: argv array
 *
 * Return: # of arguments
 *
 * This function changes the content of @s.
 */
int cli_parse_line(char **s, char *argv[])
{
	int argc = 0;

	while (argc < CMD_MAX_ARGV) {
		int inquote = 0;

		/* Eat up white spaces */
		while (is_blank((int)**s))
			(*s)++;

		if (**s == '\0')
			goto out;

		if (**s == '"') {
			inquote = 1;
			(*s)++;
		}

		argv[argc++] = *s;
		while (**s && !is_blank((int)**s)) {

			/* three cases to consider  *
			 * 1. skip ; check in quote *
			 * 2. skip ; check if \;    *
			 * 3. \\ \" in quote        *
			 */
			if (inquote) {
				if (**s == '\\' &&
						(*((*s)+1) == '\\' ||
						*((*s)+1) == '"'))
					memmove(*s, (*s)+1, strlen((*s)+1));
				else if (**s == '"')
					break;
			}
			else {
				if (**s == '\\')
					memmove(*s, (*s)+1, strlen((*s)+1));
				else if (**s == ';') {
					**s = '\0';
					(*s)++;
					goto out;
				}
			}
			(*s)++;
		}

		if (**s == '\0')
			goto out;

		**s = '\0';
		(*s)++;
	}

 out:
	argv[argc] = NULL;
	return argc;
}

void cli_lock_cmd(const struct cli_cmd *cmd)
{
	cmd_locked = cmd;
}

const struct cli_cmd *cli_cmd_start(void)
{
	if (cmd_locked)
		return cmd_locked;
	else
		return _cli_cmd_start();
}

const struct cli_cmd *cli_cmd_end(void)
{
	if (cmd_locked)
		return (cmd_locked + 1);
	else
		return _cli_cmd_end();
}

const struct cli_cmd *cli_find_cmd(char *cmd, const struct cli_cmd *table, int nr)
{
	const struct cli_cmd *t;

	for (t = table; t < table + nr; t++) {
		if (strcmp(cmd, t->name) == 0 &&
			strlen(t->name) == strlen(cmd))
			return t;
	}
	return NULL;
}


static int cli_process_cmd(int argc, char *argv[])
{
	const struct cli_cmd *start, *end, *cmd;
	int rc = 0;

	start = cli_cmd_start();
	end = cli_cmd_end();

	cmd = cli_find_cmd(argv[0], start, end - start);
	if (!cmd) {
		printf("Unknown command: %s\n", argv[0]);
		goto ret_fail;
	}
	optind = 0;
	rc = cmd->handler(argc, argv);
	if (rc == CMD_RET_USAGE) {
		if (cmd->usage) {
#if 1
			printf("Usage: %s\n", cmd->usage);
#else
			fputs("Usage: ", stdout);
			fputs(cmd->usage, stdout);
			fputs("\n", stdout);
#endif
		} else {
			argc = 2;
			argv[0] = (char *) cmd->name;
			argv[1] = "help";
			cmd->handler(argc, argv);
			return 0;
		}
	} else if (rc == CMD_RET_FAILURE)
		printf("Error: %s\n", argv[0]);

	return rc;

ret_fail:
	return -1;
}

#ifdef CONFIG_CLI_HISTORY

#ifndef CONFIG_CLI_MAX_HISTORY
#define CONFIG_CLI_MAX_HISTORY 8
#endif

static char *cmd_history[CONFIG_CLI_MAX_HISTORY];
static int cmd_hindex = 0, cmd_cursor = 0;

static inline int hindex(int index)
{
	return (index & (CONFIG_CLI_MAX_HISTORY-1));
}

#ifndef max
#define max(x, y) ((x) > (y))? (x) : (y)
#endif

static inline int cmd_oldest_index(void)
{
	int oldest;

	oldest = cmd_hindex - (CONFIG_CLI_MAX_HISTORY - 1);
	return max(0, oldest);
}

static void cmd_history_add(char *cmd)
{
	char *str, *last;
	int index;

	last = cmd_history[hindex(cmd_hindex - 1)];
	if (last && strcmp(last, cmd) == 0) {
		cmd_cursor = cmd_hindex;
		return;
	}

	str =  strdup(cmd);
	if (str == NULL)
		return;

	cmd_history[hindex(cmd_hindex++)] = str;
	cmd_cursor = cmd_hindex;

	index = hindex(cmd_hindex);
	if (cmd_history[index]) {
		free(cmd_history[index]);
		cmd_history[index] = NULL;
	}
}


/**
 * [max(0, cmd_hindex - (SIZ-1)), cmd_hindex]
 *
 * Previous action can be:
 * 1. new command added: cmd_cursor == cmd_hindex
 * 2. up: cmd_cursor < cmd_hindex
 * 3: down: cmd_cursor ?
 *
 * Pre decrement
 */
static char *cmd_history_search_backward(void)
{
	char *str;
	int oldest = cmd_oldest_index();

	cmd_cursor--;
	str = cmd_history[hindex(cmd_cursor)]; /* previous history */
	cmd_cursor = max(cmd_cursor, oldest);
	return str;
}

static char *cmd_history_search_forward(void)
{
	char *str = NULL;

	if (cmd_cursor == cmd_hindex)
		goto out;

	cmd_cursor++;
	str = cmd_history[hindex(cmd_cursor)]; /* next history */
 out:
	return str;
}

static int show_cmd_history(int argc, char *argv[])
{
	int index;

	if (argc == 1) {
		/* Show the last CONFIG_CLI_MHAX_HISTORY */
		if ((index = cmd_hindex - (CONFIG_CLI_MAX_HISTORY - 1)) < 0)
			index = 0;
		for (; index < cmd_hindex; index++) {
			printf("%4d %s\n", index,
			       cmd_history[hindex(index)]);
		}
		return 0;
	}
	if (argc > 2)
		return CMD_RET_USAGE;

	index = atoi(argv[1]);
	cli_run_command(cmd_history[hindex(index)]);

	return 0;
}

CMD(history, show_cmd_history, "show/get history", "history <index>");

#endif /* CONFIG_CMD_HISTORY */

int cli_run_command(char *line)
{
	/*char cmdbuf[CMD_MAX_CMDLINE_SIZE];*/
	char *argv[CMD_MAX_ARGV] = { NULL, };
	int argc;
	int ret = CMD_RET_SUCCESS;

	if (!line || *line == '\0')
		return -1;

	if (strlen(line) >= CMD_MAX_CMDLINE_SIZE) {
		printf("Command too long\n");
		return -1;
	}

#ifdef CONFIG_CLI_HISTORY
	cmd_history_add(line);
#endif

	do {
		argc = cli_parse_line(&line, argv);
		if (argc == 0)
			break;
		ret = cli_process_cmd(argc, argv);
	} while (ret == CMD_RET_SUCCESS);

	return ret;
}

int os_system(const char * command)
{
	char *line;
	int ret;

	line = os_malloc(strlen(command) + 1);
	if (line == NULL)
		return -1;
	strcpy(line, command);
	ret = cli_run_command(line);
	os_free(line);
	return ret;
}

#ifdef CONFIG_CMD_HELP

static int cmd_help(int argc, char *argv[])
{
	const struct cli_cmd *start, *end, *cmd;

	start = cli_cmd_start();
	end = cli_cmd_end();

	if (argc == 1) {
		for (cmd = start; cmd < end; cmd++)
			printf("%-20s - %s\n", cmd->name, cmd->desc);

		return CMD_RET_SUCCESS;
	}

	if (argc >= 2) {
		cmd = cli_find_cmd(argv[1], start, end - start);
		if (!cmd)
			return CMD_RET_FAILURE;

		if (cmd->usage) {
#if 1
			printf("Usage: %s\n%s\n", cmd->usage, cmd->desc);
#else
			fputs("Usage: ", stdout);
			fputs(cmd->usage, stdout);
			fputs("\n", stdout);
			fputs(cmd->desc, stdout);
			fputs("\n", stdout);
#endif
		} else {
			argc = 2;
			argv[0] = (char *) cmd->name;
			argv[1] = "help",
			cmd->handler(argc, argv);
		}
	}

	return CMD_RET_SUCCESS;
}

CMD(help, cmd_help,
    "print command description and usage",
	"help" OR
    "help [command]"
);

#endif

/* Inspired by UARTCommandConsole.c */

#include <FreeRTOS/FreeRTOS.h>
#include <FreeRTOS/task.h>
#include <FreeRTOS/semphr.h>
#if 0
#include <FreeRTOS/serial.h>
#endif

static const char erase_seq[] = "\b \b";	/* erase sequence */
static const char tab_seq[] = "        ";	/* used to expand TABs */

/*
 * __puts() - use fputs() that does not emit the trailing '\n'
 *
 */
static void __puts(const char *s)
{
	fputs(s, stdout);
}

static char *delete_char(char *buffer, char *p, int *colp, int *np, int plen)
{
	char *s;

	if (*np == 0)
		return p;

	if (*(--p) == '\t') {		/* will retype the whole line */
		while (*colp > plen) {
			__puts(erase_seq);
			(*colp)--;
		}
		for (s = buffer; s < p; ++s) {
			if (*s == '\t') {
				__puts(tab_seq + ((*colp) & 07));
				*colp += 8 - ((*colp) & 07);
			} else {
				++(*colp);
				putchar(*s);
			}
		}
	} else {
		__puts(erase_seq);
		(*colp)--;
	}
	(*np)--;

	return p;
}

#ifdef CONFIG_CLI_HISTORY
/*
 * Control sequence indicator sequence
 * ^](parameters)*(intermediate bytes)*<final byte>,
 * parameters: 0x30-0x3f
 * intermdiate bytes: 0x20-0x2f
 * final byte: 0x40-0x7e
 */

static char *
handle_escape_sequence(char *buf, char *p, int *colp, int *np, int plen)
{
	int c, esclen;
	enum { CSI_INIT, CSI_START, CSI_PARAM, CSI_INTERMEDIATE, CSI_FINAL } state;
	char esc[8];
	char *hist = NULL;

	esc[0] = '\x1b';
	esclen = 1;
	state = CSI_INIT;

	do { c = getchar(); } while (c < 0);
	if (c != '[')
		goto fail;

	esclen++;
	state = CSI_START;

	while (state != CSI_FINAL) {
		do { c = getchar(); }  while (c < 0);

		if (esclen == sizeof(esc))
			goto fail;

		if (c >= 0x20 && c < 0x30) {
			if (state > CSI_PARAM)
				goto fail;
			esc[esclen++] = c;
			state = CSI_PARAM;
		} else if (c >= 0x30 && c < 0x40) {
			if (state > CSI_INTERMEDIATE)
				goto fail;
			esc[esclen++] = c;
			state = CSI_INTERMEDIATE;
		} else if (c >= 0x40 && c < 0x7f) {
			esc[esclen++] = c;
			state = CSI_FINAL;
		} else {
			goto fail;
		}
	}
	if (c == 'A' || c == 'B') {
		while ((*colp) > plen) {
			__puts(erase_seq);
			(*colp)--;
		}
		hist = (c == 'A') ?
			cmd_history_search_backward() :
			cmd_history_search_forward();

		if (hist) {
			printf("%s", hist);
			strcpy(buf, hist);
			*np = strlen(hist);
			*colp = *np + plen;
		} else {
			*np = 0;
			*colp = plen;
		}
		return buf + *np;
	}
 fail:
	return p;
}
#endif

#ifdef CONFIG_CLI_AUTOCOMPLETE
static char *cmd_autocomplete(char *line, char *p, int *colp, int *np, int plen)
{
	char *argv0, *s = line;
	const struct cli_cmd *t, *start = NULL, *end = NULL;
	int c;

	/*
	 * What does the buffer look like at this moment?
	 * What information do I need to carry on auto completion?
	 * Can I put '\0' at the middle of @line? yes
	 */
	*p = '\0';
	while (is_blank((int)*s)) s++;
	if (*s == '\0')
		goto fail;

	argv0 = s;
	while (*s && !is_blank((int) *s)) s++;
	*s = '\0';

	for (t = cli_cmd_start(); t < cli_cmd_end(); t++) {
		if (strncmp(t->name, argv0, strlen(argv0)) == 0) {
			if (start == NULL)
				start = t;
		} else if (start) {
			end = t;
			break;
		}
	}
	if (start == NULL)
		goto fail;

	/* Cycle through start - end */
	t = start;
	do {
		while ((*colp) > plen) {
			__puts(erase_seq);
			(*colp)--;
		}
		printf("%s", t->name);
		strcpy(line, t->name);
		*np = strlen(t->name);
		*colp = *np + plen;
		p = line + *np;

		c = getchar();
		t++;
		if (t == end)
			t = start;
	} while (c =='\t');

	ungetc(c, stdin);
 fail:
	return p;
}

#endif /* CONFIG_CLI_AUTOCOMPLETE */

#ifndef CONFIG_CLI_STAYTIMEOUT
#define CONFIG_CLI_STAYTIMEOUT	0
#else
#include <hal/pm.h>
#endif

int cli_readline(const char *prompt, char *buffer, int blen)
{
	char *p = buffer;
	char *p_buf = p;
	int n = 0, plen = 0, col, c;

	if (prompt) {
		plen = strlen(prompt);
		__puts(prompt);
	}
	col = plen;

	buffer[0] = '\0';

	while (p < buffer + blen) {
		while ((c = getchar()) < 0);

#if (CONFIG_CLI_STAYTIMEOUT > 0)
		pm_staytimeout(CONFIG_CLI_STAYTIMEOUT);
#endif

		switch (c) {
		case '\r':
		case '\n':
			*p = '\0';
			__puts("\r\n");
			return p - p_buf;
		case '\0':
			continue;
		case 0x03: /* ^C - break */
			p_buf[0] = '\0'; /* discard input */
			return -1;
		case 0x15: /* ^U - erase line */
			while (col > plen) {
				__puts(erase_seq);
				--col;
			}
			p = p_buf;
			n = 0;
			continue;

		case 0x17: /* ^W - erase word	*/
			p = delete_char(p_buf, p, &col, &n, plen);
			while ((n > 0) && (*p != ' '))
				p = delete_char(p_buf, p, &col, &n, plen);
			continue;

		case 0x08: /* ^H  - backspace	*/
		case 0x7F: /* DEL - backspace	*/
			p = delete_char(p_buf, p, &col, &n, plen);
			continue;

#ifdef CONFIG_CLI_HISTORY
		case '\x1b':
			/* start of escpe sequence */
			p = handle_escape_sequence(p_buf, p, &col, &n, plen);
			continue;
#endif
#ifdef CONFIG_CLI_AUTOCOMPLETE
		case '\t':
			p = cmd_autocomplete(p_buf, p, &col, &n, plen);
			continue;
#endif
		default:
			/*
			 * Must be a normal character then
			 */
			if (n < blen-2) {
				if (c == '\t') {	/* expand TABs */
					__puts(tab_seq + (col & 07));
					col += 8 - (col & 07);
				} else {
					putchar(c);
					++col;
				}
				*p++ = c;
				++n;
			} else {			/* Buffer full */
				putchar('\a');
			}
		}

	}

	return n;
}

#ifdef CONFIG_CMDLINE
static void cli_task(void *param)
{
	static char input[CMD_MAX_CMDLINE_SIZE];
	int len;

#if 0
	printf("\x1b[2J\x1b[1;1H%s\n", version_string);
#endif

	while (1) {
		len = cli_readline(CONFIG_CMDLINE_PROMPT, input, sizeof(input));
		if (len <= 0)
			continue;

		cli_run_command(input);
	}
}
#endif

void cli_start(void)
{
#ifdef CONFIG_CMDLINE
	/* Create that task that handles the console itself. */
	xTaskCreate(cli_task, "cli", (2 * CONFIG_DEFAULT_STACK_SIZE / sizeof(StackType_t)), NULL, 1, NULL);
#else
	printf("Enable CLI by menuconfig.\n");
#endif
}


void cli_loop(void)
{
	cli_task(NULL);
}
