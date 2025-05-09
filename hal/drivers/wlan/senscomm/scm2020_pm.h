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
#ifndef __SCM2020_WLAN_PM_H__
#define __SCM2020_WLAN_PM_H__

#ifdef CONFIG_WATCHER_PORT_FILTER
#define WC_PORT_FILTER 1
#else
#define WC_PORT_FILTER 0
#endif

#ifdef CONFIG_WATCHER_KEEPALIVE_NULL
#define WC_KEEPALIVE_NULL WC_NULL_PKT
#else
#define WC_KEEPALIVE_NULL 0
#endif

#ifdef CONFIG_WATCHER_KEEPALIVE_GARP
#define WC_KEEPALIVE_GARP WC_GARP_PKT
#else
#define WC_KEEPALIVE_GARP 0
#endif

#ifdef CONFIG_WATCHER_KEEPALIVE_MQTT
#define WC_KEEPALIVE_MQTT WC_MQTT_PKT
#else
#define WC_KEEPALIVE_MQTT 0
#endif

#define MINUTE (60 * 1000)

#ifdef CONFIG_KEEPALIVE_PERIODIC_TX_TIME
#define WC_KEEPALIVE_PERIODIC_TX_TIME (CONFIG_KEEPALIVE_PERIODIC_TX_TIME * 1000) /* 60 sec */
#else
#define WC_KEEPALIVE_PERIODIC_TX_TIME (MINUTE) /* 60 sec */
#endif

#ifdef CONFIG_WATCHER_TX_UDPHOLE
#define WC_TX_UDPHOLE WC_UDP_HOLE_PKT
#define WC_UDPHOLE_PERIODIC_TX_TIME (CONFIG_UDPHOLE_PERIODIC_TX_TIME * MINUTE) /* 3 min */
#else
#define WC_TX_UDPHOLE 0
#define WC_UDPHOLE_PERIODIC_TX_TIME (3 * MINUTE) /* 3 min */
#endif

#ifdef CONFIG_WATCHER_TX_TCPHOLE
#define WC_TX_TCPHOLE WC_TCP_HOLE_PKT
#define WC_TCPHOLE_PERIODIC_TX_TIME (CONFIG_TCPHOLE_PERIODIC_TX_TIME * MINUTE) /* 3 min */
#else
#define WC_TX_TCPHOLE 0
#define WC_TCPHOLE_PERIODIC_TX_TIME (3 * MINUTE) /* 3 min */
#endif

#ifdef CONFIG_BCN_LOSS_LAST_CHK
#define WC_BCN_LOSS_LAST_CHECK 1
#else
#define WC_BCN_LOSS_LAST_CHECK 0
#endif

#ifdef CONFIG_BCN_LOSS_TIME
#define WC_BCN_LOSS_TIME_S CONFIG_BCN_LOSS_TIME
#else
#define WC_BCN_LOSS_TIME_S 3
#endif

struct wlan_pm_ops{
	void (*init)(struct sc_softc *sc);
	void (*sta_rx_ps_frame)(struct ieee80211_node *ni, struct ieee80211_frame *mh);
	void (*sta_ps_handler)(struct ieee80211_node *ni, void *arg, int status);
	void (*sta_ps_bmiss_handler)(void *data, int pending);
	void (*sta_doze)(struct ieee80211vap *vap, u64 tsf_gap);
	void (*sta_awake)(void *data);
	void (*sta_tx_ps_pkts)(struct ifnet *ifp);
	void (*sta_update_ps_info)(void);
	int (*wlan_resume)(struct device *dev);
	int (*wlan_suspend)(struct device *dev, u32 *duration);
	void (*wc_keepalive)(u8 mode, u32 intvl);
	void (*wc_bcn_loss_chk)(u8 enable);
	void (*wc_port_filter)(u8 enable);
	void (*wc_tsf_snap)(u32 tsf);
};

#define SCM2020_CALCULATE_DTIM_INTERVAL(ic, ni, n_beacon) \
    do { \
        assert((ni)->ni_intval && (ni)->ni_dtim_period); \
        int n_bcn_li = (ic)->ic_lintval / (ni)->ni_intval; \
        if (n_bcn_li % (ni)->ni_dtim_period) \
            n_bcn_li = ((n_bcn_li + (ni)->ni_dtim_period) / (ni)->ni_dtim_period) * (ni)->ni_dtim_period; \
        else \
            n_bcn_li = (n_bcn_li / (ni)->ni_dtim_period) * (ni)->ni_dtim_period; \
        if (n_bcn_li > (ni)->ni_dtim_period) { \
            n_beacon = n_bcn_li; \
        } else { \
            n_beacon = (ni)->ni_dtim_period; \
        } \
    } while (0)

#ifdef CONFIG_SCM2020_WLAN_PM
struct wlan_pm_ops *get_wlan_pm_ops(void);
void scm2020_wlan_pm_attach(struct sc_softc *sc);

#define WLAN_PM_OP(name, ...) \
do { \
    struct wlan_pm_ops *ops = get_wlan_pm_ops(); \
    if (ops && ops->name) { \
        ops->name(__VA_ARGS__); \
    } \
} while(0)

#define WLAN_PM_OP_SUSPEND(...) \
({ \
    int ret = -EBUSY; \
    struct wlan_pm_ops *ops = get_wlan_pm_ops(); \
    if (ops && ops->wlan_suspend) { \
        ret = ops->wlan_suspend(__VA_ARGS__); \
    } \
    ret; \
})

#define WLAN_PM_OP_RESUME(...) \
({ \
    int ret = 0; \
    struct wlan_pm_ops *ops = get_wlan_pm_ops(); \
    if (ops && ops->wlan_resume) { \
        ret = ops->wlan_resume(__VA_ARGS__); \
    } \
    ret; \
})

__inline__ static void scm2020_wlan_pm_init(struct sc_softc *sc)
{
	WLAN_PM_OP(init,sc);
}

__inline__ static void scm2020_process_rx_ps_frame(struct ieee80211_node *ni, struct ieee80211_frame *mh)
{
	WLAN_PM_OP(sta_rx_ps_frame, ni, mh);
}

__inline__ static void scm2020_sta_ps(struct ieee80211_node *ni, void *arg, int status)
{
	WLAN_PM_OP(sta_ps_handler, ni, arg, status);
}

__inline__ static void scm2020_sta_ps_bmiss(void *data, int pending)
{
	WLAN_PM_OP(sta_ps_bmiss_handler, data, pending);
}

__inline__ static void scm2020_sta_doze(struct ieee80211vap *vap, u64 tsf_gap)
{
	WLAN_PM_OP(sta_doze, vap, tsf_gap);
}

__inline__ static void scm2020_sta_awake(void *data)
{
	WLAN_PM_OP(sta_awake, data);
}

__inline__ static void scm2020_sta_update_ps_info(void)
{
	WLAN_PM_OP(sta_update_ps_info);
}

__inline__ static void scm2020_start_ps_packet(void *data)
{
	WLAN_PM_OP(sta_tx_ps_pkts, data);
}

__inline__ static int scm2020_wlan_resume(struct device *dev)
{
	return WLAN_PM_OP_RESUME(dev);
}

__inline__ static int scm2020_wlan_suspend(struct device *dev, u32 *duration)
{
	return WLAN_PM_OP_SUSPEND(dev, duration);
}

__inline__ static void scm2020_wlan_watcher_keepalive(u8 mode, u32 intvl)
{
	WLAN_PM_OP(wc_keepalive, mode, intvl);
}

__inline__ static void scm2020_wlan_watcher_bcn_loss_chk(u8 enable)
{
	WLAN_PM_OP(wc_bcn_loss_chk, enable);
}

__inline__ static void scm2020_wlan_watcher_port_filter(u8 enable)
{
	WLAN_PM_OP(wc_port_filter, enable);
}

__inline__ static void scm2020_wlan_watcher_tsf_snap(u32 tsf)
{
	WLAN_PM_OP(wc_tsf_snap, tsf);
}

#else
__inline__ static void scm2020_wlan_pm_attach(struct sc_softc *sc){return;}

__inline__ static void scm2020_wlan_pm_init(struct sc_softc *sc) {}

__inline__ static void scm2020_process_rx_ps_frame(struct ieee80211_node *ni, struct ieee80211_frame *mh) {}
__inline__ static void scm2020_sta_ps(struct ieee80211_node *ni, void *arg, int status) {}

__inline__ static void scm2020_sta_ps_bmiss(void *data, int pending) {}

__inline__ static void scm2020_sta_doze(struct ieee80211vap *vap, u64 tsf_gap) {}

__inline__ static void scm2020_sta_awake(void *data) {}

__inline__ static void scm2020_sta_update_ps_info(void) {}

__inline__ static void scm2020_start_ps_packet(void *data) {}

__inline__ static int scm2020_wlan_resume(struct device *dev) { return 0;}

__inline__ static int scm2020_wlan_suspend(struct device *dev, u32 *duration) { return -EBUSY;}

__inline__ static void scm2020_wlan_watcher_keepalive(u8 mode, u32 intvl) {}

__inline__ static void scm2020_wlan_watcher_bcn_loss_chk(u8 enable) {}

__inline__ static void scm2020_wlan_watcher_port_filter(u8 enable) {}

__inline__ static void scm2020_wlan_watcher_tsf_snap(u32 tsf) {}
#endif



#endif /* __SCM2020_WLAN_PM_H__ */
