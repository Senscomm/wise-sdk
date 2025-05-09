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

#ifndef __WISE_MTSMP_H__
#define __WISE_MTSMP_H__

#include <u-boot/list.h>
#include <hal/types.h>

/*
 * sample code to record mbuf timestamp,
 * it links array by set m_ext.ext_arg2,
 * please check it before use it.
 */

#ifdef CONFIG_SUPPORT_MTSMP
struct m_timestamp_in
{
  void *   m_in;
  uint32_t start_t;
  uint32_t end_t;
};

struct m_timestamp_out
{
  void *   m_out;
  uint8_t  subframes;
  uint32_t start_t;
  uint32_t end_t;
};

#define COUNT_NUM 100
#define PACKET_LEN_THR 1000

struct m_timestamp
{
  int outcount;
  int incount;
  struct m_timestamp_in in[COUNT_NUM];
  struct m_timestamp_out out[COUNT_NUM];
};

extern struct m_timestamp mbuf_timestamp;

#define tsmp_init() memset(&mbuf_timestamp, 0, sizeof(mbuf_timestamp))

#define tsmp_get_count(what) mbuf_timestamp.what
#define tsmp_set_count(what, value) mbuf_timestamp.what = value
#define tsmp_inc_count(what) mbuf_timestamp.what++

#define tsmp_get_idx(m) (m->m_ext.ext_arg2 - 1)
#define tsmp_set_idx(m, i) do { m->m_ext.ext_arg2 = i; } while (0)

#define tsmp_get(what) mbuf_timestamp.what
#define tsmp_set(i, what, value) \
do { \
 if ((uint32_t) i < COUNT_NUM) \
    mbuf_timestamp.what = value; \
} while (0)

#define tsmp_time(i, what) \
do { \
    tsmp_set(i, what, ktime()); \
} while (0)

#define tsmp_diff(st, ed) abs(mbuf_timestamp.ed - mbuf_timestamp.st)
#define tsmp_valid(what) (mbuf_timestamp.what.end_t != 0)
#define tsmp_avg(t,c) do { (t = t/c); } while (0)

#define tsmp_time_out(m, what) tsmp_time(tsmp_get_idx(m), out[tsmp_get_idx(m)].what)
#define tsmp_diff_out(i, st, ed) tsmp_diff(out[i].st, out[i].ed)
#define tsmp_get_out(i, what) tsmp_get(out[i].what)
#define tsmp_set_out(m, what, value) tsmp_set(tsmp_get_idx(m), out[tsmp_get_idx(m)].what, value)

#define tsmp_clean_out(m) \
do { \
  uint8_t i = tsmp_get_idx(m); \
  if (i <= COUNT_NUM) \
    memset(&mbuf_timestamp.out[i], 0, sizeof(mbuf_timestamp.out[i])); \
} while (0)

#define tsmp_diff_in(i, st, ed) tsmp_diff(in[i].st, in[i].ed)
#define tsmp_get_in(i, what) tsmp_get(in[i].what)
#define tsmp_set_in(m, what, value) tsmp_set(tsmp_get_idx(m), in[tsmp_get_idx(m)].what, value)

#define tsmp_clean_in(m) \
do { \
  uint8_t i = tsmp_get_idx(m); \
  if (i <= COUNT_NUM) \
    memset(&mbuf_timestamp.in[i], 0, sizeof(mbuf_timestamp.in[i])); \
} while (0)

#define tsmp_time_in(m, what) tsmp_time(tsmp_get_idx(m), in[tsmp_get_idx(m)].what)

#else
#define tsmp_get_idx(m)
#define tsmp_set_idx(m, i)
#define tsmp_set(i, what, value)
#define tsmp_time(i, what)
#define tsmp_clean_in(m)

#define tsmp_time_out(m, what)
#define tsmp_set_out(m, what, value)

#define tsmp_time_in(m, what)
#define tsmp_set_in(m, what, value)

#endif

#endif /* __WISE_MTSMP_H__ */
