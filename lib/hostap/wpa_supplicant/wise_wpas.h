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

#ifndef __WISE_WPAS_H
#define __WISE_WPAS_H

#include "includes.h"
#include "common.h"
#include "wpa_supplicant_i.h"
#include "ap/hostapd.h"
#include "ap/sta_info.h"

int wise_wpas_run(const char *iface, int dbglvl);
int wise_wpas_kill(void);
int wise_wpas_cli(int argc, char *argv[]);
struct wpa_supplicant *wise_wpas_get(const char *iface);
struct wpa_ssid * wise_wpas_get_network(struct wpa_supplicant *wpa_s);
int is_wise_wpas_run(wise_interface_t ifc);

#endif
