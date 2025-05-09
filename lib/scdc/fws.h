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


#ifndef FWS_H_
#define FWS_H_

#include <hal/types.h>

#ifdef CONFIG_SUPPORT_FWS
  #define CFG_SUPPORT_FWS 1

  #ifdef CONFIG_SUPPORT_FWS_CREDIT_MGMT
  #define CFG_SUPPORT_CREDIT_MGMT        CONFIG_SUPPORT_FWS_CREDIT_MGMT
  #else
  #define CFG_SUPPORT_CREDIT_MGMT        0
  #endif

  #ifdef CONFIG_SUPPORT_FWS_CREDIT_BYTE
  #define CFG_SUPPORT_CREDIT_BYTE        CONFIG_SUPPORT_FWS_CREDIT_BYTE
  #else
  #define CFG_SUPPORT_CREDIT_BYTE        0
  #endif

  #define CFG_SUPPORT_CREDIT_PACKET !CFG_SUPPORT_CREDIT_BYTE
  #define CFG_FWS_DBG 0
#else
  #define CFG_SUPPORT_FWS 0
  #define CFG_SUPPORT_CREDIT_MGMT 0
  #define CFG_FWS_DBG 0
#endif

#define FWS_COMP_TXSTATUS_LEN 8

#if CFG_SUPPORT_FWS && CFG_FWS_DBG
void    fws_update_dbg_info (void* tlv, uint32_t seqno, int outidx);
#else
#define fws_update_dbg_info(tlv, seqno, outidx)
#endif

#if CFG_SUPPORT_FWS
void    fws_init(void);
void    fws_set_features(uint32_t flags);
#else
#define fws_init()
#define fws_set_features(flags)
#endif

#if CFG_SUPPORT_CREDIT_MGMT
uint8_t fws_piggyback_txstatus(uint8_t *buf);
uint8_t fws_comp_txstatus_len(void);
void    fws_update_done_hslot(uint8_t* _wlh);
uint8_t fws_report_txstatus(bool force_report, bool event_mode, uint8_t *piggy_buf);
void    fws_enable_credit_mgmt(void);
#else
#define fws_piggyback_txstatus(buf) 0
#define fws_comp_txstatus_len() 0
#define fws_update_done_hslot(_wlh)
#define fws_report_txstatus(force_report, event_mode, piggy_buf)
#define fws_enable_credit_mgmt()
#endif

#endif /* FWS_H_ */
