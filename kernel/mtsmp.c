/*
 * Copyright 2021-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
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
#include <hal/kernel.h>
#include <cli.h>

#include "mtsmp.h"

struct m_timestamp mbuf_timestamp = {0};

int mtsmp_clean(int argc, char *argv[])
{
  tsmp_set(0, outcount, 0);
  tsmp_set(0, incount, 0);
  tsmp_init();
  return 0;
}

int mtsmp_out(int argc, char *argv[])
{
  int idx;
  uint8_t count = 0;
  uint32_t avg_start_to_end = 0;

  for(idx = 0; idx < COUNT_NUM; idx++)
  {
    if (tsmp_valid(out[idx]))
    {
      count++;
      avg_start_to_end += tsmp_diff_out(idx, start_t, end_t);

      printf("idx: %d sf: %d m: %p start: %06u end: %06u\n", idx,
              tsmp_get_out(idx, subframes), tsmp_get_out(idx, m_out),
              tsmp_get_out(idx, start_t), tsmp_get_out(idx, end_t));
      printf("star->end: %06u us\n", tsmp_diff_out(idx, start_t, end_t));
    }
  }
  if (count)
    tsmp_avg(avg_start_to_end, count);

  printf("\n\n");
  printf("%d avg start->end: %06u us\n", count, avg_start_to_end);

  return 0;
}

int mtsmp_in(int argc, char *argv[])
{
  int idx;
  uint8_t count = 0;
  uint32_t avg_start_to_end = 0;

  for(idx = 0; idx < COUNT_NUM; idx++)
  {
    if (tsmp_valid(in[idx]))
    {
      count++;
      avg_start_to_end += tsmp_diff_in(idx, start_t, end_t);

      printf("idx: %d m: %p start: %06u end: %06u\n", idx, tsmp_get_in(idx, m_in),
              tsmp_get_in(idx, start_t), tsmp_get_in(idx, end_t));

      printf("start->end: %06u us\n", tsmp_diff_in(idx, start_t, end_t));
    }
  }

  if (count)
    tsmp_avg(avg_start_to_end, count);

  printf("\n\n");
  printf("%d avg start->end: %06u us\n", count, avg_start_to_end);
  return 0;
}

/**
 * mbuf timestamp CLI commands
 */

static const struct cli_cmd mtsmp_cmd[] = {
	CMDENTRY(out, mtsmp_out, "", ""),
	CMDENTRY(in, mtsmp_in, "", ""),
	CMDENTRY(clean, mtsmp_clean, "", ""),
	CMDENTRY(c, mtsmp_clean, "", ""),
};

static int do_mtsmp(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], mtsmp_cmd, ARRAY_SIZE(mtsmp_cmd));
	if (cmd == NULL)
		return CMD_RET_USAGE;

	return cmd->handler(argc, argv);
}

CMD(mtsmp, do_mtsmp,
	"timestamp for out/in path",
	"mtsmp out" OR
	"mtsmp in" OR
	"mtsmp <clean|c>" OR
);
