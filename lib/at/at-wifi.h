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

#ifndef __AT_WIFI_H__
#define __AT_WIFI_H__

#include <hal/types.h>

struct at_wifi_ops_t
{
    void (*at_send_event_hdl)(void *event, size_t size);
    void (*at_get_mac_hdl)(uint8_t **mac_addr);
    void (*at_scan_result_resp)(uint8_t *result, size_t size);
};

void at_wifi_ops_register(struct at_wifi_ops_t *ops);
void at_wifi_init(void);
void at_wifi_deinit(void);

#endif // __AT_WIFI_H__
