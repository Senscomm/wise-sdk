/*
 * Copyright 2022-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef _scdc_h_
#define _scdc_h_

#include <hal/types.h>
#include <u-boot/list.h>
#include <freebsd/if_var.h>

#define SCDC_ATTR_WEAK				  __attribute__ ((weak))

// Log Level 1: Error
//#define SCDC_LOG1               printk
#define SCDC_LOG1(...)

// Log Level 2: Warn
#if CONFIG_SCDC_DEBUG >= 2
#define SCDC_LOG2             SCDC_LOG1
#else
#define SCDC_LOG2(...)
#endif

// Log Level 3: Info
#if CONFIG_SCDC_DEBUG >= 3
#define SCDC_LOG3             SCDC_LOG1
#else
#define SCDC_LOG3(...)
#endif

#define ASSERT_1(_cond) assert(_cond)
#define ASSERT_2(_cond, _ret) assert(_cond)
#define GET_3RD_ARG(arg1, arg2, arg3, ...) arg3
#define SCDC_ASSERT(...) GET_3RD_ARG(__VA_ARGS__, ASSERT_2, ASSERT_1,UNUSED)(__VA_ARGS__)

/* bRequest */

#define SCDC_REQUEST_SET	(0)
#define SCDC_REQUEST_GET	(1)

/* For supporting multiple interfaces */
#define SNCMF_MAX_IFS	2

#ifndef BIT
#define BIT(x)  (0x1UL << (x))
#endif

#define SCDC_BUF_DESC BIT(0)
#define SCDC_CACHE_BUF BIT(1)

/*
 * hardware header, 802.11 header, cipher header, and A-MSDU header.
 * 28 (HW HDR) + 26 (QoS HDR) + 8 (Cipher) + 14 (A-MSDU HDR) + (CONFIG_MAX_AMSDU_NUM * Buffer descriptor * CONFIG_BDS_HDR_NUM) (BDS head)
 */
#define SCM_BDS_HDR_SIZE (CONFIG_MAX_AMSDU_NUM * (12/*sizeof(struct bdesc)*/ * CONFIG_BDS_HDR_NUM))

#ifdef CONFIG_SUPPORT_BDS_ADJ
#define SCM_BDS_ADJ_SIZE SCM_BDS_HDR_SIZE
#else
#define SCM_BDS_ADJ_SIZE 0
#endif
#define SNCMF_FW_MAC_HDR_LEN				(76 + SCM_BDS_HDR_SIZE + SCM_BDS_ADJ_SIZE)

struct scdc_ops {
  void (*write)(void* buffer, uint32_t bufsize, bool isevent);
  void (*chainwrite)(struct ifqueue *data_queue);
  void (*read_info)(uint8_t itf, uint8_t bufidx, void *pinfo);
  void (*array_linearize)(uint8_t itf, uint8_t bufidx, void *pinfo);
  void (*advance_outptr)(uint8_t itf, uint8_t bufidx, uint16_t len);
  void (*prep_out)(uint8_t itf, uint8_t bufidx, bool async, void *pinfo);
  uint8_t (*get_out_pipe_num)(void);
  uint8_t (*get_extra_hdr_len)(void);
  uint32_t (*get_out_size)(bool cnt_in_pkts);
  int (*prep_in)(uint8_t itf, uint8_t bufidx, void *pinfo);
  uint8_t* (*get_buf)(uint8_t itf, int outidx, uint8_t op);
  void (*set_buf)(uint8_t itf, int outidx, uint8_t op, uint8_t *value);
  int (*awake_host)(void);
};

struct scdc_buffer {
  uint8_t *start;
  struct sncmf_proto_scdc_header *hdr;
  uint32_t len;
  uint8_t *body;
  struct list_head list;
};

typedef struct
{
    uint16_t len_lin;   ///< linear length in item size
    uint16_t len_wrap;  ///< wrapped length in item size
    void    *ptr_lin;   ///< linear part start pointer
    void    *ptr_wrap;  ///< wrapped part start pointer
    void    *priv_data; ///< private data
    uint8_t  pre_len;   ///< pre length in item size
    uint8_t  pad_len;   ///< padded length in item size
} scdc_buf_ptr_info_t;

struct scdc_buffer *scdc_alloc_buffer(uint32_t payloadlen);
int scdc_event(int ifidx, struct scdc_buffer *scdc_buf, uint8_t data_offset);
int scdc_init(void);
uint32_t scdc_get_out_size(bool cnt_in_pkts);

//--------------------------------------------------------------------+
// Application Callback API (weak is optional)
//--------------------------------------------------------------------+

SCDC_ATTR_WEAK int scdc_vendor_dump_out_pkts(void);

// Invoked when vendor class driver is opened
SCDC_ATTR_WEAK  void scdc_vendor_reg(struct scdc_ops *ops);

// Invoked when vendor class driver is opened
SCDC_ATTR_WEAK void scdc_vendor_open(uint8_t itf);

// Invoked when received control request with VENDOR TYPE
SCDC_ATTR_WEAK bool scdc_vendor_control_xfer_cb(uint8_t itf, uint8_t request, void** buffer, uint16_t len);

// Invoked when received new data
SCDC_ATTR_WEAK void scdc_vendor_rx_cb(uint8_t itf, int outidx, uint16_t len, void *priv);

SCDC_ATTR_WEAK bool scdc_vendor_out_pkts_empty(int outidx);

SCDC_ATTR_WEAK int scdc_vendor_get_buf(int size, uint32_t *offset, uint8_t **buf, uint8_t **buf_desc, uint8_t op);


#endif /* _scdc_h_ */
