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

#ifndef __SDIOIF_H__
#define __SDIOIF_H__

#define RX_DESC_INVALID -1

/*
|  frag_len   | checksum | frag bit & total_len | channel & rsvd | offset |
|  2 bytes    |  2 bytes |        2 byte        |      1 byte    | 1 byte |
*/
#define SDIO_DMA_ALIGNMENT 4  /* SDIO DMA start address alignment is 4 bytes */
#define SDPCM_HWHDR_LEN 4
#define SDPCM_SWHDR_LEN 4
/* SDPCM_HDRLEN must be SDIO_DMA_ALIGNMENT[4] alignment */
#define SDPCM_HDRLEN (SDPCM_HWHDR_LEN + SDPCM_SWHDR_LEN)
#define SDPCM_TLEN_MASK 0x00007fff
#define SDPCM_TLEN_SHIFT 0
#define SDPCM_FRAG_MASK 0x00008000
#define SDPCM_FRAG_SHIFT 15
#define SDPCM_CHANNEL_MASK 0x000f0000
#define SDPCM_CHANNEL_SHIFT 16
#define SDPCM_CONTROL_CHANNEL 0 /* Control */
#define SDPCM_EVENT_CHANNEL 1	/* Asyc Event Indication */
#define SDPCM_DATA_CHANNEL 2	/* Data Xmit/Recv */
#define SDPCM_DOFFSET_MASK 0xff000000
#define SDPCM_DOFFSET_SHIFT 24

#define SCDC_REQUEST_SET (0)
#define SCDC_REQUEST_GET (1)

int sdio_netif_init(void);

#ifdef CONFIG_SDIO_PM
int sdio_notify_host_reenum(void);
void sdio_fifo_resume(void);
#else
__inline__ void sdio_notify_host_reenum(void) { }
#endif

#endif /* __SDIOIF_H__ */
