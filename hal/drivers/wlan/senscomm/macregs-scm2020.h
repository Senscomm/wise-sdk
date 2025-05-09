#ifndef _MACREGS_H_
#define _MACREGS_H_
/*
 * Registers for MAC
 */

#define REG_INTR_ENABLE                                                          0x000
/* Interrupt enable for TX done of each queue */
/* [0] TX done interrupt for queue0 */
/* ... */
/* [9] TX done interrupt for queue9 */
#define     INTR_ENABLE_TX_DONE_MASK                                             0x3ff
#define     INTR_ENABLE_TX_DONE_SHIFT                                                0
/* Interrupt enable from MAC MCU (TBD) */
#define     INTR_ENABLE_MCU_MASK                                                0xfc00
#define     INTR_ENABLE_MCU_SHIFT                                                   10
/* Interrupt enable for RX done (MPDU is received with no error) */
#define     INTR_ENABLE_RX_DONE_MASK                                           0x10000
#define     INTR_ENABLE_RX_DONE_SHIFT                                               16
/* Interrupt enable for rx data doesn’t need response, */
/* this interrupt is only enabled when REG_VIF0{/1}_ESP.sp_check_en is set to 1 */
#define     INTR_ENABLE_RX_ESP_MASK                                            0x20000
#define     INTR_ENABLE_RX_ESP_SHIFT                                                17
/* Interrupt enable for rx data needs response, */
/* the data is  qos null or qos data with eosp is 1, */
/* more data is 0 for mgmt frame or date frame that is not qos_data and qos null. */
/* the interrupt asserted after response frame send out. */
/* this interrupt is only enabled when REG_VIF0{/1}_ESP.sp_check_en is set to 1 */
#define     INTR_ENABLE_RESP_ESP_MASK                                          0x40000
#define     INTR_ENABLE_RESP_ESP_SHIFT                                              18
/* Interrupt enable for rx bssid matched trigger frame with more TF is 0, */
/* this time our aid is not matched. */
/* this interrupt is only enabled when REG_VIF0{/1}_ESP.sp_check_en is set to 1 */
#define     INTR_ENABLE_NOMORE_TF_ESP_MASK                                     0x80000
#define     INTR_ENABLE_NOMORE_TF_ESP_SHIFT                                         19
/* Interrupt enable for rx bssid matched trigger frame with MORE RARU bit is 0, */
/* this time we don’t have pkt queued for aid matched trigger */
/* this interrupt is only enabled when REG_VIF0{/1}_ESP.sp_check_en is set to 1 */
#define     INTR_ENABLE_NOMORE_RARU_ESP_MASK                                  0x100000
#define     INTR_ENABLE_NOMORE_RARU_ESP_SHIFT                                       20
/* Interrupt enable for send qos null in hetb ppdu when receving basic trigger */
/* need to check register REG_TX_HETB_QNULL about vif/first_ac field for qos null sending */
/* [21] is for vif0 */
/* [22] is for vif1 */
#define     INTR_ENABLE_BASIC_TRIG_QNULL_VIF_MASK                             0x600000
#define     INTR_ENABLE_BASIC_TRIG_QNULL_VIF_SHIFT                                  21
/* Interrupt enable for TSF from VIF0 */
/* [24] TSF interrupt for REG_VIF0_TSF_TIME0 */
/* [25] TSF interrupt for REG_VIF0_TSF_TIME1 */
#define     INTR_ENABLE_TSF0_MASK                                            0x3000000
#define     INTR_ENABLE_TSF0_SHIFT                                                  24
/* Interrupt enable for TSF from VIF1 */
/* [26] TSF interrupt for REG_VIF1_TSF_TIME0 */
/* [27] TSF interrupt for REG_VIF1_TSF_TIME1 */
#define     INTR_ENABLE_TSF1_MASK                                            0xc000000
#define     INTR_ENABLE_TSF1_SHIFT                                                  26
/* Interrupt enable for RX buffer available */
/* This interrupt remains high if REG_RX_BUF_PTR0.read_ptr */
/* is not equal to RX_RX_BUF_PTR1.write_ptr */
#define     INTR_ENABLE_RX_BUF_MASK                                         0x10000000
#define     INTR_ENABLE_RX_BUF_SHIFT                                                28

#define REG_INTR_CLEAR                                                           0x004
/* Interrupt clear for TX done of each queue */
/* [0] TX done interrupt for queue0 */
/* ... */
/* [9] TX done interrupt for queue9 */
#define     INTR_CLEAR_TX_DONE_MASK                                              0x3ff
#define     INTR_CLEAR_TX_DONE_SHIFT                                                 0
/* Interrupt clear from MAC MCU (TBD) */
#define     INTR_CLEAR_MCU_MASK                                                 0xfc00
#define     INTR_CLEAR_MCU_SHIFT                                                    10
/* Interrupt clear for RX done (MPDU is received with no error) */
#define     INTR_CLEAR_RX_DONE_MASK                                            0x10000
#define     INTR_CLEAR_RX_DONE_SHIFT                                                16
/* Interrupt clear for rx data doesn’t need response, */
/* the data is  qos null or qos data with eosp is 1, */
/* more data is 0 for mgmt frame or date frame that is not qos_data and qos null. */
/* this interrupt asserted after rx dma is finished. */
#define     INTR_CLEAR_RX_ESP_MASK                                             0x20000
#define     INTR_CLEAR_RX_ESP_SHIFT                                                 17
/* Interrupt clear for rx data needs response, */
/* the data is  qos null or qos data with eosp is 1, */
/* more data is 0 for mgmt frame or date frame that is not qos_data and qos null. */
/* the interrupt asserted after response frame send out. */
#define     INTR_CLEAR_RESP_ESP_MASK                                           0x40000
#define     INTR_CLEAR_RESP_ESP_SHIFT                                               18
/* Interrupt clear for rx bssid matched trigger frame with more TF is 0, */
/* this time our aid is not matched. */
#define     INTR_CLEAR_NOMORE_TF_ESP_MASK                                      0x80000
#define     INTR_CLEAR_NOMORE_TF_ESP_SHIFT                                          19
/* Interrupt clear for rx bssid matched trigger frame with MORE RARU bit is 0, */
/* this time we don’t have pkt queued for aid matched trigger */
#define     INTR_CLEAR_NOMORE_RARU_ESP_MASK                                   0x100000
#define     INTR_CLEAR_NOMORE_RARU_ESP_SHIFT                                        20
/* Interrupt enable for send qos null in hetb ppdu when receving basic trigger */
/* need to check register REG_TX_HETB_QNULL about vif/first_ac field for qos null sending */
/* [21] is for vif0 */
/* [22] is for vif1 */
#define     INTR_CLEAR_BASIC_TRIG_QNULL_VIF_MASK                              0x600000
#define     INTR_CLEAR_BASIC_TRIG_QNULL_VIF_SHIFT                                   21
/* Interrupt clear for TSF from VIF0 */
/* [24] TSF interrupt for REG_VIF0_TSF_TIME0 */
/* [25] TSF interrupt for REG_VIF0_TSF_TIME1 */
#define     INTR_CLEAR_TSF0_MASK                                             0x3000000
#define     INTR_CLEAR_TSF0_SHIFT                                                   24
/* Interrupt clear for TSF from VIF1 */
/* [26] TSF interrupt for REG_VIF1_TSF_TIME0 */
/* [27] TSF interrupt for REG_VIF1_TSF_TIME1 */
#define     INTR_CLEAR_TSF1_MASK                                             0xc000000
#define     INTR_CLEAR_TSF1_SHIFT                                                   26
/* Interrupt clear for RX buffer available */
/* This interrupt remains high if REG_RX_BUF_PTR0.read_ptr */
/* is not equal to RX_RX_BUF_PTR1.write_ptr */
#define     INTR_CLEAR_RX_BUF_MASK                                          0x10000000
#define     INTR_CLEAR_RX_BUF_SHIFT                                                 28

#define REG_INTR_STATUS                                                          0x004
/* Interrupt status for TX done of each queue */
/* [0] TX done interrupt for queue0 */
/* ... */
/* [9] TX done interrupt for queue9 */
#define     INTR_STATUS_TX_DONE_MASK                                             0x3ff
#define     INTR_STATUS_TX_DONE_SHIFT                                                0
/* Interrupt status from MAC MCU (TBD) */
#define     INTR_STATUS_MCU_MASK                                                0xfc00
#define     INTR_STATUS_MCU_SHIFT                                                   10
/* Interrupt status for RX done (MPDU is received with no error) */
#define     INTR_STATUS_RX_DONE_MASK                                           0x10000
#define     INTR_STATUS_RX_DONE_SHIFT                                               16
/* Interrupt status for rx data doesn’t need response, */
/* the data is  qos null or qos data with eosp is 1, */
/* more data is 0 for mgmt frame or date frame that is not qos_data and qos null. */
/* this interrupt asserted after rx dma is finished. */
/* sw can also check */
/* REG_VIF0{/1}_ESP_STATUS.frame_type/ */
/* REG_VIF0{/1}_ESP_STATUS.frame_subtype/ */
/* REG_VIF0{/1}_ESP_STATUS.more_date/ */
/* REG_VIF0{/1}_ESP_STATUS.qos_eosp */
/* to check the receiving frame type */
/* this interrupt is only enabled when REG_VIF0{/1}_ESP.sp_check_en is set to 1 */
#define     INTR_STATUS_RX_ESP_MASK                                            0x20000
#define     INTR_STATUS_RX_ESP_SHIFT                                                17
/* Interrupt status for rx data needs response, */
/* the data is  qos null or qos data with eosp is 1, */
/* more data is 0 for mgmt frame or date frame that is not qos_data and qos null. */
/* the interrupt asserted after response frame send out. */
/* sw can also check REG_VIF0{/1}_ESP_STATUS.frame_type/ */
/* REG_VIF0{/1}_ESP_STATUS.frame_subtype/ */
/* REG_VIF0{/1}_ESP_STATUS.more_date/ */
/* REG_VIF0{/1}_ESP_STATUS.qos_eosp to check the receiving frame type */
/* this interrupt is only enabled when REG_VIF0{/1}_ESP.sp_check_en is set to 1 */
#define     INTR_STATUS_RESP_ESP_MASK                                          0x40000
#define     INTR_STATUS_RESP_ESP_SHIFT                                              18
/* Interrupt status for rx bssid matched trigger frame with more TF is 0, */
/* this time our aid is not matched. */
/* this interrupt is only enabled when REG_VIF0{/1}_ESP.sp_check_en is set to 1 */
#define     INTR_STATUS_NOMORE_TF_ESP_MASK                                     0x80000
#define     INTR_STATUS_NOMORE_TF_ESP_SHIFT                                         19
/* Interrupt status for rx bssid matched trigger frame with MORE RARU bit is 0, */
/* this time we don’t have pkt queued for aid matched trigger */
/* this interrupt is only enabled when REG_VIF0{/1}_ESP.sp_check_en is set to 1 */
#define     INTR_STATUS_NOMORE_RARU_ESP_MASK                                  0x100000
#define     INTR_STATUS_NOMORE_RARU_ESP_SHIFT                                       20
/* Interrupt enable for send qos null in hetb ppdu when receving basic trigger */
/* need to check register REG_TX_HETB_QNULL about vif/first_ac field for qos null sending */
/* [21] is for vif0 */
/* [22] is for vif1 */
#define     INTR_STATUS_BASIC_TRIG_QNULL_VIF_MASK                             0x600000
#define     INTR_STATUS_BASIC_TRIG_QNULL_VIF_SHIFT                                  21
/* Interrupt status for TSF from VIF0 */
/* [24] TSF interrupt for REG_VIF0_TSF_TIME0 */
/* [25] TSF interrupt for REG_VIF0_TSF_TIME1 */
#define     INTR_STATUS_TSF0_MASK                                            0x3000000
#define     INTR_STATUS_TSF0_SHIFT                                                  24
/* Interrupt status for TSF from VIF1 */
/* [26] TSF interrupt for REG_VIF1_TSF_TIME0 */
/* [27] TSF interrupt for REG_VIF1_TSF_TIME1 */
#define     INTR_STATUS_TSF1_MASK                                            0xc000000
#define     INTR_STATUS_TSF1_SHIFT                                                  26
/* Interrupt status for RX buffer available */
/* This interrupt remains high if REG_RX_BUF_PTR0.read_ptr */
/* is not equal to RX_RX_BUF_PTR1.write_ptr */
#define     INTR_STATUS_RX_BUF_MASK                                         0x10000000
#define     INTR_STATUS_RX_BUF_SHIFT                                                28

#define REG_MAC_CFG                                                              0x008
/* Reset MAC & PHY */
/* 1 : S/W Reset MAC(except register map & DMA)  & PHY simultaneously */
/* The value will be reset to 0 automatically */
#define     MAC_CFG_RST_MASK                                                       0x1
#define     MAC_CFG_RST_SHIFT                                                        0
/* Enable MAC */
/* 0 : Disable MAC RX operation */
/* 1 : Enable MAC RX operation */
#define     MAC_CFG_EN_MASK                                                      0x100
#define     MAC_CFG_EN_SHIFT                                                         8

#define REG_MAC_STATUS                                                           0x00C
/* DMA status */
/* 0 : MAC DMA is in idle state */
/* 1 : MAC DMA is in transaction */
#define     MAC_STATUS_DMA_BUSY_MASK                                               0x1
#define     MAC_STATUS_DMA_BUSY_SHIFT                                                0
/* RX Engine status */
/* 0 : MAC RX Engine is in idle state */
/* 1 : MAC RX Engine is in busy state */
#define     MAC_STATUS_RXE_BUSY_MASK                                               0x2
#define     MAC_STATUS_RXE_BUSY_SHIFT                                                1

#define REG_MCU_CFG                                                              0x010
/* MCU Configuration */
/* 0 : Reset MCU */
/* 1 : Run MCU */
#define     MCU_CFG_RSTB_MASK                                                      0x1
#define     MCU_CFG_RSTB_SHIFT                                                       0

#define REG_MCU_STATUS                                                           0x014
/* Indicate that MCU initialization is done (READ only) */
#define     MCU_STATUS_DONE_MASK                                                   0x1
#define     MCU_STATUS_DONE_SHIFT                                                    0

#define REG_VER_INFO                                                             0x018
/* MAC version */
/* [31:16] product code */
/* [15:12] major version */
/* [11: 4] minor version */
/* [ 3: 0] F(FPGA), C(CHIP) */
#define     VER_INFO_MAC_VER_INFO_MASK                                      0xffffffff
#define     VER_INFO_MAC_VER_INFO_SHIFT                                              0

#define REG_GIT_VER                                                              0x01C
/* MAC git commit number */
#define     GIT_VER_MAC_GIT_VER_MASK                                        0xffffffff
#define     GIT_VER_MAC_GIT_VER_SHIFT                                                0

#define REG_DEV_CFG                                                              0x020
/* 0: 2.4GHz, 1: 5GHz */
/* SIFS time is determined by this value (16 us for 5GHz, 10 us for 2.4GHz) */
#define     DEV_CFG_BAND_MASK                                                      0x1
#define     DEV_CFG_BAND_SHIFT                                                       0
/* update basic nav in case that for HE MU/VHT MU ppdu that directed to us */
/* but doesn't need response */
#define     DEV_CFG_ONAV_MU_OPT_MASK                                               0x2
#define     DEV_CFG_ONAV_MU_OPT_SHIFT                                                1
/* Number of RX chain */
/* 0: The number of RX chain (= Number of spatial stream) is 1 */
/* 1: The number of RX chain (= Number of spatial stream) is 2 */
#define     DEV_CFG_N_RX_MASK                                                    0x100
#define     DEV_CFG_N_RX_SHIFT                                                       8
/* Used for EOF field value of MPDU delimiter of Compressed Beamforming Feedback frame */
#define     DEV_CFG_CBF_EOF_MASK                                               0x10000
#define     DEV_CFG_CBF_EOF_SHIFT                                                   16
/* RU Check for HETB Compressed Beamforming frame */
/* Enable function to check RU size for Compressed Beamforming Feedback frame */
/* when receiving BFRP trigger frame */
#define     DEV_CFG_HETB_CBF_RU_CHK_MASK                                       0x20000
#define     DEV_CFG_HETB_CBF_RU_CHK_SHIFT                                           17
/* abort receving ppdu if mpdu is filter out */
#define     DEV_CFG_RX_FILTER_OUT_ABORT_EN_MASK                                0x40000
#define     DEV_CFG_RX_FILTER_OUT_ABORT_EN_SHIFT                                    18
/* abort receiving ppdu if EOF delimitor is detected */
#define     DEV_CFG_RX_EOF_DEL_ABORT_EN_MASK                                   0x80000
#define     DEV_CFG_RX_EOF_DEL_ABORT_EN_SHIFT                                       19
/* bsrp/bqrp contention option for uora */
/* if this bit is set to 1 and REG_VIF0{/1}_MIN_MPDU_SPACING.hetb_date_dis is set to 1, */
/* for bqrp/bsrq uora case, */
/* not consider the 4 ac queues(vo/vi/be/bk) status for UORA contention. */
#define     DEV_CFG_NON_BASIC_TRIG_CONT_OPT_MASK                              0x100000
#define     DEV_CFG_NON_BASIC_TRIG_CONT_OPT_SHIFT                                   20
/* allow to aggerate mdpu with eof is 1 in hetb ppdu */
/* in case of ack/ba + date case for hetb ppdu, */
/* allow to aggerate mdpu with eof is 1 in hetb ppdu */
/* This register mainly used not to send out ack/ba + mgmt frame in hetb ppdu. */
#define     DEV_CFG_ACK_ENABLED_AMDPU_IN_HETB_MASK                            0x200000
#define     DEV_CFG_ACK_ENABLED_AMDPU_IN_HETB_SHIFT                                 21
/* if REG_VIF0{/1}_MIN_MPDU_SPACING.hetb_date_dis is 1, */
/* avoid to send out qos null in hetb ppdu(not for bsrp/bqrp case). */
/* This register mainly used to not send out ba + qosnull frame, */
/* which qos null is automaticly switched. */
#define     DEV_CFG_ULMU_DATA_DIS_QOSNULL_EN_MASK                             0x400000
#define     DEV_CFG_ULMU_DATA_DIS_QOSNULL_EN_SHIFT                                  22
/* receiving bsrp trigger, select from the queues */
/* which is enabled for ULMU(aid matched or asso uora) for qos null */
#define     DEV_CFG_BSRP_UL_MU_DATA_CHK_EN_MASK                               0x800000
#define     DEV_CFG_BSRP_UL_MU_DATA_CHK_EN_SHIFT                                    23
/* for bqrp and bsrp, check whether there are queues for AID matched trigger and */
/* associated uora when do uora contention */
/* for basic trigger, check whether there are queues for */
/* assoicated uora when do contention */
#define     DEV_CFG_UORA_CONT_DATA_CHK_EN_MASK                               0x1000000
#define     DEV_CFG_UORA_CONT_DATA_CHK_EN_SHIFT                                     24
/* if our device only configured as 20Mhz operation STA, */
/* we support 484/996/2x996 tone ru */
#define     DEV_CFG_WIDE_RU_SUPPORT_20_MASK                                  0x2000000
#define     DEV_CFG_WIDE_RU_SUPPORT_20_SHIFT                                        25
/* if our device is configured not 20Mhz operation STA, */
/* we support 996/2x996 tone ru */
#define     DEV_CFG_WIDE_RU_SUPPORT_MASK                                     0x4000000
#define     DEV_CFG_WIDE_RU_SUPPORT_SHIFT                                           26
/* enable update ba bitmap for last fragmented qos data */
/* set this bit to 1, we support to rx level1 fragmentated frame. */
#define     DEV_CFG_EN_LAST_FRAG_BITMAP_UPD_MASK                             0x8000000
#define     DEV_CFG_EN_LAST_FRAG_BITMAP_UPD_SHIFT                                   27
/* if this bit is set to 1, when sending mpdu from MT for hetb ppdu, */
/* hw will not take the tid 0~7 mpdu which is noack in to tid limitation check */
/* this can help sw agg qosnull with noack in MT to send */
/* notice:if sw wants to send out qos data with noack, */
/* please doesn't use the aid_match/uora method for this que in que cmd */
#define     DEV_CFG_HETB_TID_CHK_IG_NOACK_MASK                              0x10000000
#define     DEV_CFG_HETB_TID_CHK_IG_NOACK_SHIFT                                     28
/* if sta is 40Mhz, can do uora for 80Mhz UL BW RA-RU */
/* this is not explicite descriped in the spec, we don't have such test */
/* default set this value to 0 */
#define     DEV_CFG_UORA_STA40_PRI_UL80_160_MASK                            0x20000000
#define     DEV_CFG_UORA_STA40_PRI_UL80_160_SHIFT                                   29
/* if sta is 40Mhz, can do uora for secondary 80Mhz RA-RU */
/* this is not descripted in the spec */
/* default set this value to 0 */
#define     DEV_CFG_UORA_STA40_SEC80_MASK                                   0x40000000
#define     DEV_CFG_UORA_STA40_SEC80_SHIFT                                          30
/* in the case of sending cbf for rx brpoll/brpoll-trigger case, need to check the whether had received vht/he ndp or not */
#define     DEV_CFG_CBF_RESP_NDP_CHK_EN_MASK                                0x80000000
#define     DEV_CFG_CBF_RESP_NDP_CHK_EN_SHIFT                                       31

#define REG_DEV_OPT1                                                             0x024
/* set to 1'b1 to support BQRP trigger */
#define     DEV_OPT1_BQRP_EN_MASK                                                  0x1
#define     DEV_OPT1_BQRP_EN_SHIFT                                                   0
/* set to 1'b1 to support NFRP trigger */
#define     DEV_OPT1_NFRP_EN_MASK                                                  0x2
#define     DEV_OPT1_NFRP_EN_SHIFT                                                   1
/* delay p2m_rx_td_end in mac, mainly for non-ht ofdma 1 symbol */
#define     DEV_OPT1_PHYRX_TD_DELAY_MASK                                         0x700
#define     DEV_OPT1_PHYRX_TD_DELAY_SHIFT                                            8
/* non-ht ppdu only contains 1 symble pkt delay for us, now simulation is 3.35us */
#define     DEV_OPT1_NONHT_1OFDM_US_DELAY_MASK                                 0x70000
#define     DEV_OPT1_NONHT_1OFDM_US_DELAY_SHIFT                                     16
/* non-ht ppdu only contains 1 symble pkt delay for cycle, */
/* now simulation is 3.35u(mac frequency is 80Mhz) */
#define     DEV_OPT1_NONHT_1OFDM_CYCLE_DELAY_MASK                            0x3f80000
#define     DEV_OPT1_NONHT_1OFDM_CYCLE_DELAY_SHIFT                                  19

#define REG_DEV_CS                                                               0x028
/* Ignore CCA signal from PHY for CS(carrier sense) mechanism */
#define     DEV_CS_IGNORE_CCA_MASK                                                 0x1
#define     DEV_CS_IGNORE_CCA_SHIFT                                                  0
/* Ignore RX busy state for CS mechanism */
#define     DEV_CS_IGNORE_RX_MASK                                                  0x2
#define     DEV_CS_IGNORE_RX_SHIFT                                                   1
/* Regard CS as busy when receiving address-filtered frame */
#define     DEV_CS_BUSY_FLT_RX_MASK                                                0x4
#define     DEV_CS_BUSY_FLT_RX_SHIFT                                                 2
/* Regard CS as busy when rx_sync is asserted */
#define     DEV_CS_IGNORE_RX_SYNC_MASK                                             0x8
#define     DEV_CS_IGNORE_RX_SYNC_SHIFT                                              3
/* ignore RX busy state for CS mechanism if no tx que is waiting response */
#define     DEV_CS_IGNORE_RX_TX_WAIT_RESP_MASK                                    0x10
#define     DEV_CS_IGNORE_RX_TX_WAIT_RESP_SHIFT                                      4

#define REG_DEV_CH_COEX                                                          0x02C
/* device operation channel used for bt coex(for 2.4G) */
/* it is for the wlan primary channel center frequency */
/* 12~84Mhz */
#define     DEV_CH_COEX_WLAN_CHANNEL_FREQ_MASK                                    0x7f
#define     DEV_CH_COEX_WLAN_CHANNEL_FREQ_SHIFT                                      0
/* device operation channel used for bt coex(for 2.4G) */
/* wlan secondary channel offset */
/* 1'b0 for lower;1'b1 for upper */
#define     DEV_CH_COEX_WLAN_CHANNEL_OFFSET_MASK                                 0x100
#define     DEV_CH_COEX_WLAN_CHANNEL_OFFSET_SHIFT                                    8
/* define the response pkt priority */
/* when response frame contains ack/ba or hetb ppdu for bsrq/bqrp/mu-bar */
#define     DEV_CH_COEX_RESP_PRI0_MASK                                         0xf0000
#define     DEV_CH_COEX_RESP_PRI0_SHIFT                                             16
/* define the response pkt priority */
/* when response frame does not contain */
/* ack/ba or hetb ppdu for bsrq/bqrp/mu-bar */
#define     DEV_CH_COEX_RESP_PRI1_MASK                                        0xf00000
#define     DEV_CH_COEX_RESP_PRI1_SHIFT                                             20

#define REG_DEV_PMU                                                              0x030
/* 0 : diable SM power saving */
/* 1 : enable SM power saving */
#define     DEV_PMU_SMPS_EN_MASK                                                   0x1
#define     DEV_PMU_SMPS_EN_SHIFT                                                    0
/* 0 : static SM power save mode */
/* 1 : dynamic SM power save mode */
#define     DEV_PMU_SMPS_MODE_MASK                                                 0x2
#define     DEV_PMU_SMPS_MODE_SHIFT                                                  1

#define REG_DEV_FORCE_CLR                                                        0x034
/* clear vector */
/* 4'h0~4'h9 : clear tx que 0~9 status */
/* 4'hA : clear rxe fsm */
/* 4'hB : clear txe fsm */
/* 4'hC : clear MCU state to IDLE */
/* 4'hD : clear BFI state to IDLE */
#define     DEV_FORCE_CLR_CLR_FROCE_VEC_MASK                                       0xf
#define     DEV_FORCE_CLR_CLR_FROCE_VEC_SHIFT                                        0
/* clr_force */
#define     DEV_FORCE_CLR_CLR_FORCE_EN_MASK                                      0x100
#define     DEV_FORCE_CLR_CLR_FORCE_EN_SHIFT                                         8

#define REG_DEV_OPT2                                                             0x038
/* once cca_fail/rx_busy/tx_prepare_cancel happens for the queue in last slot */
/* would makes the txe_fsm goes to idle */
#define     DEV_OPT2_CONT_TX_TERMI_FSMIDLE_EN_MASK                                 0x1
#define     DEV_OPT2_CONT_TX_TERMI_FSMIDLE_EN_SHIFT                                  0
/* txphy_to_en timeout enable to clear tx_busy */
/* use the same TX_TO value as txe_fsm */
#define     DEV_OPT2_TXPHY_TO_EN_MASK                                              0x2
#define     DEV_OPT2_TXPHY_TO_EN_SHIFT                                               1
/* if rto counter already turns to 1 and we are waiting response, */
/* assert rto_expire */
#define     DEV_OPT2_RTO_TO_CHECK_EN_MASK                                          0x4
#define     DEV_OPT2_RTO_TO_CHECK_EN_SHIFT                                           2
/* for edca case check tx vec generated before contention win */
#define     DEV_OPT2_EDCA_TXVEC_PROT_EN_MASK                                       0x8
#define     DEV_OPT2_EDCA_TXVEC_PROT_EN_SHIFT                                        3
/* assert tx_busy if phy_en is 1 to protect contention */
#define     DEV_OPT2_TXBUSY_PHY_EN_MASK                                           0x10
#define     DEV_OPT2_TXBUSY_PHY_EN_SHIFT                                             4
/* check rx state busy when rx_start comes. */
#define     DEV_OPT2_RXBUSY_CHK_EN_MASK                                           0x20
#define     DEV_OPT2_RXBUSY_CHK_EN_SHIFT                                             5
/* check rx state busy when rx_confiliction happens. */
#define     DEV_OPT2_RXBUSY_CONF_CHK_EN_MASK                                      0x40
#define     DEV_OPT2_RXBUSY_CONF_CHK_EN_SHIFT                                        6
/* if sifs lost happens, txe fsm goes to idle */
#define     DEV_OPT2_SIFS_LOST_STOP_TX_EN_MASK                                    0x80
#define     DEV_OPT2_SIFS_LOST_STOP_TX_EN_SHIFT                                      7
/* when sending queue cancel command, */
/* immediately abort current TX PPDU */
#define     DEV_OPT2_QUE_CANCEL_ABORT_TX_MASK                                    0x100
#define     DEV_OPT2_QUE_CANCEL_ABORT_TX_SHIFT                                       8
/* when sending only ba or ack in hetb ppdu for basic trigger frame, assert interrupt for host */
#define     DEV_OPT2_HETB_INTR_ACKBA_EN_MASK                                     0x200
#define     DEV_OPT2_HETB_INTR_ACKBA_EN_SHIFT                                        9
/* send CTS/ACK/BA frame even if sifs lost happens */
#define     DEV_OPT2_SIFS_LOST_SEND_TX_EN_MASK                                   0x400
#define     DEV_OPT2_SIFS_LOST_SEND_TX_EN_SHIFT                                     10
/* check queue status in rx ppdu end for aid matched basic trigger */
#define     DEV_OPT2_AID_MATCH_BASIC_TRIG_END_EN_MASK                            0x800
#define     DEV_OPT2_AID_MATCH_BASIC_TRIG_END_EN_SHIFT                              11
/* check queue status in rx ppdu end for aid matched bsrp trigger */
#define     DEV_OPT2_AID_MATCH_BSRP_TRIG_END_EN_MASK                            0x1000
#define     DEV_OPT2_AID_MATCH_BSRP_TRIG_END_EN_SHIFT                               12
/* disable to add htc field in cbf responding BRP trigger */
#define     DEV_OPT2_BRP_CBF_HTC_DIS_MASK                                       0x2000
#define     DEV_OPT2_BRP_CBF_HTC_DIS_SHIFT                                          13
/* always disable htc append or enable htc append based on brp_cbf_htc_dis */
#define     DEV_OPT2_BRP_CBF_HTC_STATIC_MASK                                    0x4000
#define     DEV_OPT2_BRP_CBF_HTC_STATIC_SHIFT                                       14
/* enable seqnumber in sequence control field in CBF frame */
/* if this register is 0, sequnce control field in cbf frame is 0 */
#define     DEV_OPT2_CBF_SEQ_EN_MASK                                            0x8000
#define     DEV_OPT2_CBF_SEQ_EN_SHIFT                                               15
/* Send CBF frame with all angle values to zero */
#define     DEV_OPT2_CBF_ANGLE_ZERO_MASK                                       0x10000
#define     DEV_OPT2_CBF_ANGLE_ZERO_SHIFT                                           16
/* Reset sounding sequence when TXOP is finished */
#define     DEV_OPT2_SND_SEQ_RST_EN_MASK                                       0x20000
#define     DEV_OPT2_SND_SEQ_RST_EN_SHIFT                                           17
/* use wlan_tx_abort from pta */
#define     DEV_OPT2_COEX_WLAN_ABORT_EN_MASK                                   0x40000
#define     DEV_OPT2_COEX_WLAN_ABORT_EN_SHIFT                                       18
/* check qos data normal ackp(2'b00) with out trigger/trs then response */
#define     DEV_OPT2_ACKP0_CHECK_EN_MASK                                       0x80000
#define     DEV_OPT2_ACKP0_CHECK_EN_SHIFT                                           19
/* enable obss ppdu condition check for rx vht with group id 0 */
#define     DEV_OPT2_CHECK_VHT_GROUPID0_MASK                                  0x100000
#define     DEV_OPT2_CHECK_VHT_GROUPID0_SHIFT                                       20
/* enable obss ppdu condition check for rx vht with group id 63 */
#define     DEV_OPT2_CHECK_VHT_GROUPID63_MASK                                 0x200000
#define     DEV_OPT2_CHECK_VHT_GROUPID63_SHIFT                                      21
/* when tx que cancel happens after last_slot interrupt, */
/* hw checks whether the canceled que is the one gets last slot. */
#define     DEV_OPT2_TX_CANCEL_QUE_CHK_EN_MASK                                0x400000
#define     DEV_OPT2_TX_CANCEL_QUE_CHK_EN_SHIFT                                     22
/* in the contention win time, check the que get contetion is same with the que get last slot */
#define     DEV_OPT2_BKEND_QUE_CHK_EN_MASK                                    0x800000
#define     DEV_OPT2_BKEND_QUE_CHK_EN_SHIFT                                         23

#define REG_DEV_FORCE_QUE_CLR                                                    0x03C
/* clear queue status */
#define     DEV_FORCE_QUE_CLR_CLR_FROCE_VEC_MASK                                 0x3ff
#define     DEV_FORCE_QUE_CLR_CLR_FROCE_VEC_SHIFT                                    0

#define REG_DEV_CLK_CFG                                                          0x040
/* Clock gating enable for HCLK */
#define     DEV_CLK_CFG_HCLK_GATE_EN_MASK                                          0x1
#define     DEV_CLK_CFG_HCLK_GATE_EN_SHIFT                                           0
/* Clock gating enable for HCLKX0 (IMEM) */
#define     DEV_CLK_CFG_HCLKX0_GATE_EN_MASK                                        0x2
#define     DEV_CLK_CFG_HCLKX0_GATE_EN_SHIFT                                         1
/* Clock gating enable for HCLKX1 (REG) */
#define     DEV_CLK_CFG_HCLKX1_GATE_EN_MASK                                        0x4
#define     DEV_CLK_CFG_HCLKX1_GATE_EN_SHIFT                                         2
/* Clock gating enable for HCLKX2 (DMEM, FIFO, TXE, RXE, etc.) */
#define     DEV_CLK_CFG_HCLKX2_GATE_EN_MASK                                        0x8
#define     DEV_CLK_CFG_HCLKX2_GATE_EN_SHIFT                                         3

#define REG_INTR_SELECT                                                          0x050
/* 0 : Interrupt is generated on wlan_irq0 */
/* 1 : Interrupt is generated on wlan_irq1 */
#define     INTR_SELECT_INTR_SEL_MASK                                       0xffffffff
#define     INTR_SELECT_INTR_SEL_SHIFT                                               0

#define REG_VIF0_CFG0                                                            0x100
/* Virtual interface 0 enable */
#define     VIF0_CFG0_EN_MASK                                                      0x1
#define     VIF0_CFG0_EN_SHIFT                                                       0
/* 11b Present for Interface 0 */
/* 0 : There is no 11b station present in BSS for interface 0 */
/* 1 : 11b station is present in BSS for interface 0 */
#define     VIF0_CFG0_BP_MASK                                                    0x100
#define     VIF0_CFG0_BP_SHIFT                                                       8
/* Preamble Type for Interface 0 */
/* Preamble type in case of transmiting non-HT frame for interface 0 */
/* (0 : long preamble, 1 : short preamble) */
#define     VIF0_CFG0_PT_MASK                                                    0x200
#define     VIF0_CFG0_PT_SHIFT                                                       9
/* Slot time duration for virtual interface 0 */
#define     VIF0_CFG0_SLOT_TIME_MASK                                          0xff0000
#define     VIF0_CFG0_SLOT_TIME_SHIFT                                               16
/* Update TSF value (new TSF0 value = TSF0 + TSF0_OFFSET when top is 0) */
/* Update TSF value (new TSF0 value = TSF0 - TSF0_OFFSET when top is 1) */
#define     VIF0_CFG0_TU_MASK                                                0x1000000
#define     VIF0_CFG0_TU_SHIFT                                                      24
/* Update TSF value with add or minus operation */
/* (new TSF0 value = TSF0 +/- TSF0_OFFSET) */
/* 1'b0 add; 1'b1 minus */
#define     VIF0_CFG0_TOP_MASK                                               0x2000000
#define     VIF0_CFG0_TOP_SHIFT                                                     25
/* TSF0 value clear to 0 */
/* if TSF0 value clear and value update assert same time, clear the TSF0 value */
#define     VIF0_CFG0_TC_MASK                                                0x4000000
#define     VIF0_CFG0_TC_SHIFT                                                      26

#define REG_VIF0_CFG1                                                            0x104
/* Basic HT-MCS Set Bitmap for interface 0 */
/* LS 16 bits of Rx MCS Bitmask subfield of Basic HT-MCS Set field of the HT Operation parameter */
#define     VIF0_CFG1_BASIC_HT_MCS_SET_MASK                                     0xffff
#define     VIF0_CFG1_BASIC_HT_MCS_SET_SHIFT                                         0
/* Basic Rate Set Bitmap for Interface 0 */
/* [0]  11B, 1Mb/s */
/* [1]  11B, 2Mb/s */
/* [2]  11B, 5.5Mb/s */
/* [3]  11B, 11Mb/s */
/* [4]  Legacy, 6Mb/s */
/* [5]  Legacy, 9Mb/s */
/* [6]  Legacy, 12Mb/s */
/* [7]  Legacy, 18Mb/s */
/* [8]  Legacy, 24Mb/s */
/* [9]  Legacy, 36Mb/s */
/* [10] Legacy, 48Mb/s */
/* [11] Legacy, 54Mb/s */
#define     VIF0_CFG1_BASIC_RATE_BITMAP_MASK                                 0xfff0000
#define     VIF0_CFG1_BASIC_RATE_BITMAP_SHIFT                                       16

#define REG_VIF0_RX_FILTER0                                                      0x108
/* MAC Address Enable */
/* Enable MAC address (enable RA matching for address filtering in case of unicast frame) */
#define     VIF0_RX_FILTER0_MAC_EN_MASK                                            0x3
#define     VIF0_RX_FILTER0_MAC_EN_SHIFT                                             0
/* BSSID Enable */
/* Enable BSSID (enable matching for address filtering in case of broadcast frame) */
#define     VIF0_RX_FILTER0_BSS_EN_MASK                                          0x300
#define     VIF0_RX_FILTER0_BSS_EN_SHIFT                                             8
/* BSSID Filtering Enable */
/* [0] All broadcast frame are transferred to S/W, [1] Enable BSSID address filtering */
#define     VIF0_RX_FILTER0_BFE_MASK                                           0x10000
#define     VIF0_RX_FILTER0_BFE_SHIFT                                               16
/* this vif is in ap mode */
#define     VIF0_RX_FILTER0_AP_MODE_MASK                                       0x20000
#define     VIF0_RX_FILTER0_AP_MODE_SHIFT                                           17

#define REG_VIF0_RX_FILTER1                                                      0x10C
/* Control Frame Filtering Bitmap */
/* [0] not transferred to S/W, [1] transferred to S/W */
#define     VIF0_RX_FILTER1_CTRL_FRAME_FILTER_BITMAP_MASK                       0xffff
#define     VIF0_RX_FILTER1_CTRL_FRAME_FILTER_BITMAP_SHIFT                           0
/* Management Frame Filtering Bitmap */
/* [0] not transferred to S/W, [1] transferred to S/W */
#define     VIF0_RX_FILTER1_MGNT_FRAME_FILTER_BITMAP_MASK                   0xffff0000
#define     VIF0_RX_FILTER1_MGNT_FRAME_FILTER_BITMAP_SHIFT                          16

#define REG_VIF0_MAC0_L                                                          0x110
/* MAC Address 0 low 32 bit */
#define     VIF0_MAC0_L_MAC_ADDR0_31_0_MASK                                 0xffffffff
#define     VIF0_MAC0_L_MAC_ADDR0_31_0_SHIFT                                         0

#define REG_VIF0_MAC0_H                                                          0x114
/* MAC Address 0 high 16 bit */
#define     VIF0_MAC0_H_MAC_ADDR0_47_36_MASK                                    0xffff
#define     VIF0_MAC0_H_MAC_ADDR0_47_36_SHIFT                                        0

#define REG_VIF0_MAC1_L                                                          0x118
/* MAC Address 1 low 32 bit */
#define     VIF0_MAC1_L_MAC_ADDR1_31_0_MASK                                 0xffffffff
#define     VIF0_MAC1_L_MAC_ADDR1_31_0_SHIFT                                         0

#define REG_VIF0_MAC1_H                                                          0x11C
/* MAC Address 1 high 16 bit */
#define     VIF0_MAC1_H_MAC_ADDR1_47_16_MASK                                    0xffff
#define     VIF0_MAC1_H_MAC_ADDR1_47_16_SHIFT                                        0

#define REG_VIF0_BSS0_L                                                          0x120
/* BSSID 0 low 32 bit */
#define     VIF0_BSS0_L_BSSID0_31_0_MASK                                    0xffffffff
#define     VIF0_BSS0_L_BSSID0_31_0_SHIFT                                            0

#define REG_VIF0_BSS0_H                                                          0x124
/* BSSID 0 high 16 bit */
#define     VIF0_BSS0_H_BSSID0_47_16_MASK                                       0xffff
#define     VIF0_BSS0_H_BSSID0_47_16_SHIFT                                           0

#define REG_VIF0_BSS1_L                                                          0x128
/* BSSID 1 low 32 bit */
#define     VIF0_BSS1_L_BSSID1_31_0_MASK                                    0xffffffff
#define     VIF0_BSS1_L_BSSID1_31_0_SHIFT                                            0

#define REG_VIF0_BSS1_H                                                          0x12C
/* BSSID 1 high 16 bit */
#define     VIF0_BSS1_H_BSSID1_47_16_MASK                                       0xffff
#define     VIF0_BSS1_H_BSSID1_47_16_SHIFT                                           0

#define REG_VIF0_AID                                                             0x130
/* Association ID for interface 0 */
#define     VIF0_AID_AID_MASK                                                    0xfff
#define     VIF0_AID_AID_SHIFT                                                       0

#define REG_VIF0_BFM0                                                            0x134
/* Grouping value for Compressed Beamforming frame in response to HT NDP Announcement */
/* 0 : Ng = 1 */
/* 1 : Ng = 2 */
/* 2 : Ng = 4 */
#define     VIF0_BFM0_HT_NG_MASK                                                   0x3
#define     VIF0_BFM0_HT_NG_SHIFT                                                    0
/* Cookbook Information value for Compressed Beamforming frame */
/* in response to HT NDP Announcement */
/* 0 : 1 bits for psi, 3 bits for phi */
/* 1 : 2 bits for psi, 4 bits for phi */
/* 2 : 3 bits for psi, 5 bits for phi */
/* 3 : 4 bits for psi, 6 bits for phi */
#define     VIF0_BFM0_HT_CB_MASK                                                   0xc
#define     VIF0_BFM0_HT_CB_SHIFT                                                    2
/* Format for Compressed Beamforming frame used in HT sounding */
#define     VIF0_BFM0_HT_FORMAT_MASK                                              0x70
#define     VIF0_BFM0_HT_FORMAT_SHIFT                                                4
/* MCS for Compressed Beamforming frame used in HT sounding */
#define     VIF0_BFM0_HT_MCS_MASK                                               0x7f00
#define     VIF0_BFM0_HT_MCS_SHIFT                                                   8
/* Grouping value for Compressed Beamforming frame used in VHT sounding */
/* 0 : Ng = 1 */
/* 1 : Ng = 2 */
/* 2 : Ng = 4 */
#define     VIF0_BFM0_VHT_NG_MASK                                              0x30000
#define     VIF0_BFM0_VHT_NG_SHIFT                                                  16
/* Codebook Information value for Compressed Beamforming frame */
/* used when Feedback Type is SU in VHT sounding */
/* 0 : 2 bits for psi, 4 bits for phi */
/* 1 : 4 bits for psi, 6 bits for phi */
#define     VIF0_BFM0_VHT_SU_CB_MASK                                           0x40000
#define     VIF0_BFM0_VHT_SU_CB_SHIFT                                               18
/* Codebook Information value for Compressed Beamforming frame */
/* used when Feedback Type is MU in VHT sounding */
/* 0 : 5 bits for psi, 7 bits for phi */
/* 1 : 7 bits for psi, 9 bits for phi */
#define     VIF0_BFM0_VHT_MU_CB_MASK                                           0x80000
#define     VIF0_BFM0_VHT_MU_CB_SHIFT                                               19
/* Format for Compressed Beamforming frame used in VHT sounding */
#define     VIF0_BFM0_VHT_FORMAT_MASK                                         0x700000
#define     VIF0_BFM0_VHT_FORMAT_SHIFT                                              20
/* MCS for Compressed Beamforming frame used in VHT sounding */
#define     VIF0_BFM0_VHT_MCS_MASK                                          0x7f000000
#define     VIF0_BFM0_VHT_MCS_SHIFT                                                 24

#define REG_VIF0_BFM1                                                            0x138
/* Grouping value for Compressed Beamforming frame */
/* used when Feedback Type is SU in HE non-TB sounding */
/* 0 : Ng = 4 */
/* 1 : Ng = 16 */
#define     VIF0_BFM1_HE_NG_MASK                                                   0x1
#define     VIF0_BFM1_HE_NG_SHIFT                                                    0
/* Codebook Information value for Compressed Beamforming frame */
/* used when Feedback Type is SU in HE non-TB sounding */
/* 0 : 4 bits for phi, 2 bits for psi */
/* 1 : 6 bits for phi, 4 bits for psi */
#define     VIF0_BFM1_HE_CB_MASK                                                   0x4
#define     VIF0_BFM1_HE_CB_SHIFT                                                    2
/* Format for Compressed Beamforming frame used in HE sounding */
#define     VIF0_BFM1_HE_FORMAT_MASK                                              0x70
#define     VIF0_BFM1_HE_FORMAT_SHIFT                                                4
/* MCS for Compressed Beamforming frame used in HE sounding */
#define     VIF0_BFM1_HE_MCS_MASK                                               0x7f00
#define     VIF0_BFM1_HE_MCS_SHIFT                                                   8

#define REG_VIF0_BFM2                                                            0x13C
/* Enable function to limit maximum vaule for reported SNR0 */
#define     VIF0_BFM2_SNR0_LIMIT_EN_MASK                                           0x1
#define     VIF0_BFM2_SNR0_LIMIT_EN_SHIFT                                            0
/* Maximum value for SNR0 in signed 2's complement */
/* used for limiting maximum reported SNR0 value */
#define     VIF0_BFM2_SNR0_MAX_MASK                                             0xff00
#define     VIF0_BFM2_SNR0_MAX_SHIFT                                                 8
/* Enable function to limit maximum vaule for reported SNR1 */
#define     VIF0_BFM2_SNR1_LIMIT_EN_MASK                                       0x10000
#define     VIF0_BFM2_SNR1_LIMIT_EN_SHIFT                                           16
/* Maximum value for SNR1 in signed 2's complement */
/* used for limiting maximum reported SNR0 value */
#define     VIF0_BFM2_SNR1_MAX_MASK                                         0xff000000
#define     VIF0_BFM2_SNR1_MAX_SHIFT                                                24

#define REG_VIF0_TSF_L                                                           0x140
/* tsf [31:0] */
/* 64-bit timestamp timer for interface 0 */
#define     VIF0_TSF_L_TSF_31_0_MASK                                        0xffffffff
#define     VIF0_TSF_L_TSF_31_0_SHIFT                                                0

#define REG_VIF0_TSF_H                                                           0x144
/* tsf [63:32] */
/* 64-bit timestamp timer for interface 0 */
#define     VIF0_TSF_H_TSF_63_32_MASK                                       0xffffffff
#define     VIF0_TSF_H_TSF_63_32_SHIFT                                               0

#define REG_VIF0_TSF_OFFSET_L                                                    0x148
/* tsf offset [31:0] */
/* 64-bit timestamp offset value to update for interface 0 */
/* (new TSF = current TSF + TSF_OFFSET) */
#define     VIF0_TSF_OFFSET_L_TSF_OFFSET_31_0_MASK                          0xffffffff
#define     VIF0_TSF_OFFSET_L_TSF_OFFSET_31_0_SHIFT                                  0

#define REG_VIF0_TSF_OFFSET_H                                                    0x14C
/* tsf offset [63:32] */
/* 64-bit timestamp offset value to update for interface 0 */
/* (new TSF = current TSF + TSF_OFFSET) */
#define     VIF0_TSF_OFFSET_H_TSF_OFFSET_63_32_MASK                         0xffffffff
#define     VIF0_TSF_OFFSET_H_TSF_OFFSET_63_32_SHIFT                                 0

#define REG_VIF0_TSF_TIME0                                                       0x150
/* TSF value for TIME0 interrupt */
/* Timestamp value for interface 0 to generate TIME0 interrupt */
#define     VIF0_TSF_TIME0_TSF_TIME0_MASK                                   0xffffffff
#define     VIF0_TSF_TIME0_TSF_TIME0_SHIFT                                           0

#define REG_VIF0_TSF_TIME1                                                       0x154
/* TSF value for TIME1 interrupt */
/* Timestamp value for interface 0 to generate TIME1 interrupt */
#define     VIF0_TSF_TIME1_TSF_TIME0_MASK                                   0xffffffff
#define     VIF0_TSF_TIME1_TSF_TIME0_SHIFT                                           0

#define REG_VIF0_TRIG_RESP_QUEUE_EN                                              0x158
/* reserved field */
#define     VIF0_TRIG_RESP_QUEUE_EN_RSVD_MASK                                   0x7fff
#define     VIF0_TRIG_RESP_QUEUE_EN_RSVD_SHIFT                                       0

#define REG_VIF0_BSR                                                             0x15C
/* BSR control subfield content for for HTC he varaint, */
/* please refering 11ax D4.2 9.2.4.6a.4 for details */
#define     VIF0_BSR_BSR_CONTENT_MASK                                        0x3ffffff
#define     VIF0_BSR_BSR_CONTENT_SHIFT                                               0

#define REG_VIF0_MIN_MPDU_SPACING                                                0x160
/* minimal mpdu start spacing */
#define     VIF0_MIN_MPDU_SPACING_MMSS_MASK                                        0x7
#define     VIF0_MIN_MPDU_SPACING_MMSS_SHIFT                                         0
/* STA default PE duration, valid value is 0~4 */
#define     VIF0_MIN_MPDU_SPACING_DEFAULT_PE_DURATION_MASK                        0x38
#define     VIF0_MIN_MPDU_SPACING_DEFAULT_PE_DURATION_SHIFT                          3
/* he bss color for this interface, once COLOR_EN is 1, need to set BSS_COLOR to non-zero */
/* before association, can also fill bss_color field to the bss color value we want to assoicate. */
/* this time keep the COLOR_EN to 0 */
#define     VIF0_MIN_MPDU_SPACING_BSS_COLOR_MASK                                0x3f00
#define     VIF0_MIN_MPDU_SPACING_BSS_COLOR_SHIFT                                    8
/* BSS_COLOR enable */
#define     VIF0_MIN_MPDU_SPACING_COLOR_EN_MASK                                 0x4000
#define     VIF0_MIN_MPDU_SPACING_COLOR_EN_SHIFT                                    14
/* BSS_COLOR is disabled */
#define     VIF0_MIN_MPDU_SPACING_COLOR_DIS_MASK                                0x8000
#define     VIF0_MIN_MPDU_SPACING_COLOR_DIS_SHIFT                                   15
/* enable to receive trigger frame */
/* only support basic trigger/murts/mubrpoll/mubar/bsrp/bqrp/nfrp */
#define     VIF0_MIN_MPDU_SPACING_TRIG_EN_MASK                                 0x10000
#define     VIF0_MIN_MPDU_SPACING_TRIG_EN_SHIFT                                     16
/* nable to support associated AP UORA */
#define     VIF0_MIN_MPDU_SPACING_TRIG_ASSO_UORA_EN_MASK                       0x20000
#define     VIF0_MIN_MPDU_SPACING_TRIG_ASSO_UORA_EN_SHIFT                           17
/* enable to supported unassoicated AP UORA */
/* notice TRIG_UNASSO_UORA and TRIG_ASSO_UORA is excluded */
#define     VIF0_MIN_MPDU_SPACING_TRIG_UNASSO_UORA_EN_MASK                     0x40000
#define     VIF0_MIN_MPDU_SPACING_TRIG_UNASSO_UORA_EN_SHIFT                         18
/* support TRS in HTC */
#define     VIF0_MIN_MPDU_SPACING_TRS_EN_MASK                                  0x80000
#define     VIF0_MIN_MPDU_SPACING_TRS_EN_SHIFT                                      19
/* support partial bss color feature */
#define     VIF0_MIN_MPDU_SPACING_PARTIAL_BSS_COLOR_EN_MASK                   0x100000
#define     VIF0_MIN_MPDU_SPACING_PARTIAL_BSS_COLOR_EN_SHIFT                        20
/* only support basic nav that is basic nav, */
/* notice this feature is only applied when this vif is AP mode */
#define     VIF0_MIN_MPDU_SPACING_BASIC_NAV_EN_MASK                           0x200000
#define     VIF0_MIN_MPDU_SPACING_BASIC_NAV_EN_SHIFT                                21
/* disable sending data frame in hetb ppdu for basic trigger frame, */
/* only send out ctrl frame(response frame) frame, or mgmt frame in hetb ppdu */
/* this bit is related to ULMU DATA DIS in OM field */
#define     VIF0_MIN_MPDU_SPACING_HETB_DATA_DIS_MASK                          0x400000
#define     VIF0_MIN_MPDU_SPACING_HETB_DATA_DIS_SHIFT                               22
/* in response to BSRP trigger frame, send qos null mpdu with BSR */
/* (if enable this, only send out 1 qosnull frame with bsr subfield in HTC) */
#define     VIF0_MIN_MPDU_SPACING_BSRP_BSR_EN_MASK                            0x800000
#define     VIF0_MIN_MPDU_SPACING_BSRP_BSR_EN_SHIFT                                 23
/* in response to BSRP trigger frame, send several AC's qosnull mpdu(queues have pkt to send). */
/* function is only valid when REG_VIF0_MIN_MPDU_SPACING.bsrp_bsr_en is 1'b0 */
#define     VIF0_MIN_MPDU_SPACING_BSRP_MORE_QOSNULL_EN_MASK                  0x1000000
#define     VIF0_MIN_MPDU_SPACING_BSRP_MORE_QOSNULL_EN_SHIFT                        24

#define REG_VIF0_QNULL_CTRL_AC0                                                  0x164
/* be queue sequece control field for qosnull frame in HETB, map to queue0 */
#define     VIF0_QNULL_CTRL_AC0_SEQUNECE_CTRL_MASK                              0xffff
#define     VIF0_QNULL_CTRL_AC0_SEQUNECE_CTRL_SHIFT                                  0
/* be queue qos control field for qosnull frame in HETB, map to queue0 */
/* qos null mpdu ack policy field is set to NO_ACK(2'b01) */
#define     VIF0_QNULL_CTRL_AC0_QOS_CTRL_MASK                               0xffff0000
#define     VIF0_QNULL_CTRL_AC0_QOS_CTRL_SHIFT                                      16

#define REG_VIF0_QNULL_CTRL_AC1                                                  0x168
/* bk queue sequece control field for qosnull frame in HETB, map to queue1 */
#define     VIF0_QNULL_CTRL_AC1_SEQUNECE_CTRL_MASK                              0xffff
#define     VIF0_QNULL_CTRL_AC1_SEQUNECE_CTRL_SHIFT                                  0
/* bk queue qos control field for qosnull frame in HETB, map to queue1 */
/* qos null mpdu ack policy field is set to NO_ACK(2'b01) */
#define     VIF0_QNULL_CTRL_AC1_QOS_CTRL_MASK                               0xffff0000
#define     VIF0_QNULL_CTRL_AC1_QOS_CTRL_SHIFT                                      16

#define REG_VIF0_QNULL_CTRL_AC2                                                  0x16C
/* vi queue sequece control field for qosnull frame in HETB, map to queue2 */
#define     VIF0_QNULL_CTRL_AC2_SEQUNECE_CTRL_MASK                              0xffff
#define     VIF0_QNULL_CTRL_AC2_SEQUNECE_CTRL_SHIFT                                  0
/* vi queue qos control field for qosnull frame in HETB, map to queue2 */
/* qos null mpdu ack policy field is set to NO_ACK(2'b01) */
#define     VIF0_QNULL_CTRL_AC2_QOS_CTRL_MASK                               0xffff0000
#define     VIF0_QNULL_CTRL_AC2_QOS_CTRL_SHIFT                                      16

#define REG_VIF0_QNULL_CTRL_AC3                                                  0x170
/* vo queue sequece control field for qosnull frame in HETB, map to queue3 */
#define     VIF0_QNULL_CTRL_AC3_SEQUNECE_CTRL_MASK                              0xffff
#define     VIF0_QNULL_CTRL_AC3_SEQUNECE_CTRL_SHIFT                                  0
/* vo queue qos control field for qosnull frame in HETB, map to queue3 */
/* qos null mpdu ack policy field is set to NO_ACK(2'b01) */
#define     VIF0_QNULL_CTRL_AC3_QOS_CTRL_MASK                               0xffff0000
#define     VIF0_QNULL_CTRL_AC3_QOS_CTRL_SHIFT                                      16

#define REG_VIF0_UORA_CFG                                                        0x174
/* OBO initial value */
/* use this register for obo counter if obo counter is 0 and SW_OBO is set to 1 */
#define     VIF0_UORA_CFG_OBO_INIT_VALUE_MASK                                     0x7f
#define     VIF0_UORA_CFG_OBO_INIT_VALUE_SHIFT                                       0
/* use OBO_INIT_SW value for obo contention */
/* if SW_OBO_VALUE_EN is 1'b1, uses OBO_INIT_VALUE value for initial obo counter */
/* if SW_OBO_VALUE_EN is 1'b0, use lfsr to random gen initial obo counter */
#define     VIF0_UORA_CFG_SW_OBO_VALUE_EN_MASK                                    0x80
#define     VIF0_UORA_CFG_SW_OBO_VALUE_EN_SHIFT                                      7
/* init obo contention window */
/* sw need to write this register to 1 to initiate CW once want to do UORA */
/* (hw would latch CW_MIN to CURR_CW) */
#define     VIF0_UORA_CFG_CW_INIT_MASK                                          0x8000
#define     VIF0_UORA_CFG_CW_INIT_SHIFT                                             15
/* min obo contention window, (2 power cw_min)-1 */
#define     VIF0_UORA_CFG_CW_MIN_MASK                                          0x70000
#define     VIF0_UORA_CFG_CW_MIN_SHIFT                                              16
/* max obo contention window, (2 power cw_max)-1 */
#define     VIF0_UORA_CFG_CW_MAX_MASK                                         0x380000
#define     VIF0_UORA_CFG_CW_MAX_SHIFT                                              19

#define REG_VIF0_UORA_CFG_STATUS                                                 0x174
/* OBO initial value */
/* use this register for obo counter if obo counter is 0 and SW_OBO is set to 1 */
#define     VIF0_UORA_CFG_STATUS_OBO_INIT_VALUE_MASK                              0x7f
#define     VIF0_UORA_CFG_STATUS_OBO_INIT_VALUE_SHIFT                                0
/* use OBO_INIT_SW value for obo contention */
/* if SW_OBO_VALUE_EN is 1'b1, uses OBO_INIT_VALUE value for initial obo counter */
/* if SW_OBO_VALUE_EN is 1'b0, use lfsr to random gen initial obo counter */
#define     VIF0_UORA_CFG_STATUS_SW_OBO_VALUE_EN_MASK                             0x80
#define     VIF0_UORA_CFG_STATUS_SW_OBO_VALUE_EN_SHIFT                               7
/* currently used obo value for contention(read_only) */
#define     VIF0_UORA_CFG_STATUS_OBO_VALUE_MASK                                 0x7f00
#define     VIF0_UORA_CFG_STATUS_OBO_VALUE_SHIFT                                     8
/* init obo contention window */
/* sw need to write this register to 1 to initiate CW once want to do UORA */
/* (hw would latch CW_MIN to CURR_CW) */
#define     VIF0_UORA_CFG_STATUS_CW_INIT_MASK                                   0x8000
#define     VIF0_UORA_CFG_STATUS_CW_INIT_SHIFT                                      15
/* min obo contention window, (2 power cw_min)-1 */
#define     VIF0_UORA_CFG_STATUS_CW_MIN_MASK                                   0x70000
#define     VIF0_UORA_CFG_STATUS_CW_MIN_SHIFT                                       16
/* max obo contention window, (2 power cw_max)-1 */
#define     VIF0_UORA_CFG_STATUS_CW_MAX_MASK                                  0x380000
#define     VIF0_UORA_CFG_STATUS_CW_MAX_SHIFT                                       19
/* currently used obo contetion window, (2 power curr_cw)-1 (read only) */
#define     VIF0_UORA_CFG_STATUS_CURR_CW_MASK                                0x1c00000
#define     VIF0_UORA_CFG_STATUS_CURR_CW_SHIFT                                      22

#define REG_VIF0_UORA_CFG1                                                       0x178
/* each 20Mhz channel enable */
/* each bit is 1 means that 20Mhz subchannel is enabled; */
/* bit 0 is the lowest 20Mhz in 80Mhz channel, and bit 3 this the highest 20Mhz in 80Mhz channel */
/* bit [3:0] for for primay 80Mhz, bit [7:4] is for secondary 80Mhz */
/* only one bit is1 in CHANNEL_EN[7:0] is 1, that is channel switches to that sub 20Mhz channel */
/* CHANNEL_EN[7:4] is 4'hF, that is operation channel switches to secondary 80Mhz */
#define     VIF0_UORA_CFG1_CHANNEL_EN_MASK                                        0xff
#define     VIF0_UORA_CFG1_CHANNEL_EN_SHIFT                                          0
/* uora supported HE LTF MODE */
/* if bit0 is 1'b1, supports single pilot HE-LTF mode */
/* if bit1 is 1'b1, supports masked HE-LTF sequnce mode */
/* bit 1 is only used for 2x HE-LTF + 1.6us GI or 4x HE-LTF + 3.2us GI hetb ppdu */
#define     VIF0_UORA_CFG1_LTF_MODE_EN_MASK                                      0x300
#define     VIF0_UORA_CFG1_LTF_MODE_EN_SHIFT                                         8
/* 1'b0 means 160Mhz, 1'b1 means 80+80Mhz */
#define     VIF0_UORA_CFG1_80P80_MODE_MASK                                       0x400
#define     VIF0_UORA_CFG1_80P80_MODE_SHIFT                                         10
/* primary channel location */
/* 3'b000, primary in lowest 20Mhz; 3'b001, primary in second lowest 20Mhz; */
/* 3'b010, primary in third lowest 20Mhz; 3'b011, primary in fourth lowest20Mhz; */
/* 3'b100, primary in fifth lowest 20Mhz; 3'b101, primary in sixth lowest 20Mhz */
/* 3'b110, primary in seventh lowest 20Mhz, 3'b111, primary in highest 20Mhz; */
/* 4~7 is used for associated with an AP supporting 80+80 or 160Mhz */
#define     VIF0_UORA_CFG1_PRI_CH_MASK                                          0x3800
#define     VIF0_UORA_CFG1_PRI_CH_SHIFT                                             11

#define REG_VIF0_NON_SRG_OBSS_PD                                                 0x17C
/* non_SRG obss pd min value, default value -82dbm */
#define     VIF0_NON_SRG_OBSS_PD_MIN_VALUE_MASK                                   0xff
#define     VIF0_NON_SRG_OBSS_PD_MIN_VALUE_SHIFT                                     0
/* non_SRG obss pd max value, default value -62dbm */
#define     VIF0_NON_SRG_OBSS_PD_MAX_VALUE_MASK                                 0xff00
#define     VIF0_NON_SRG_OBSS_PD_MAX_VALUE_SHIFT                                     8

#define REG_VIF0_SRG_OBSS_PD                                                     0x180
/* SRG obss pd min value, default value -82dbm */
#define     VIF0_SRG_OBSS_PD_MIN_VALUE_MASK                                       0xff
#define     VIF0_SRG_OBSS_PD_MIN_VALUE_SHIFT                                         0
/* SRG obss pd max value, default value -82dbm */
#define     VIF0_SRG_OBSS_PD_MAX_VALUE_MASK                                     0xff00
#define     VIF0_SRG_OBSS_PD_MAX_VALUE_SHIFT                                         8

#define REG_VIF0_TX_PWR                                                          0x184
/* tx ref power used for SR operation, default value 21dbm */
#define     VIF0_TX_PWR_REF_PWR_MASK                                              0xff
#define     VIF0_TX_PWR_REF_PWR_SHIFT                                                0
/* tx power used for frame implicitly sent by MAC H/W */
/* (RTS/CTS/ACK/BA/CBF/CF-End) */
#define     VIF0_TX_PWR_HW_PWR_MASK                                             0xff00
#define     VIF0_TX_PWR_HW_PWR_SHIFT                                                 8

#define REG_VIF0_CFG_INFO                                                        0x188
/* que contention enable */
/* REG_VIF0_CFG_INFO.que_contention_en[4:0] is for que[4:0]; */
/* REG_VIF1_CFG_INFO.que_contention_en[4:0] is for que[9:5] */
#define     VIF0_CFG_INFO_QUE_CONTENTION_EN_MASK                                  0x1f
#define     VIF0_CFG_INFO_QUE_CONTENTION_EN_SHIFT                                    0
/* currently maintained NAV stores to the backup one, */
/* and re_latch the back up nav for nav calculation. */
/* This is used for this interface channel switch */
#define     VIF0_CFG_INFO_NAV_SWITCH_MASK                                         0x20
#define     VIF0_CFG_INFO_NAV_SWITCH_SHIFT                                           5
/* qos null send with UPH field */
/* if this register is 1, Qos null send with UPH field in HE TB ppdu, */
/* else send with BSR field(except BSRP/BQRP) */
#define     VIF0_CFG_INFO_QOS_NULL_UPH_EN_MASK                                    0x40
#define     VIF0_CFG_INFO_QOS_NULL_UPH_EN_SHIFT                                      6
/* replace/append HTC field with UPH field */
/* if this register is 1, send queue pkt(from queues 0~4) */
/* to replace the HTC with HE HTC UPH field in HE TB ppdu */
/* if this register is 0, send queue pkt(from queues 0~4) */
/* to replace the HTC with HE HTC BSR field in HE TB ppdu */
#define     VIF0_CFG_INFO_QUEUE_UPH_EN_MASK                                       0x80
#define     VIF0_CFG_INFO_QUEUE_UPH_EN_SHIFT                                         7
/* add HTC field for HE TB PPDU(only for data send from MT) */
/* if QUEUE_HTC_APPEND_EN is 1 and QUEUE_UPH_EN is 1, append uph field; */
/* if QUEUE_HTC_APPEND_EN is 1 and QUEUE_UPH_EN is 0, append BSR field; */
/* hw also would check the no_htc bit in MT is 1 and then append */
/* (no_htc bit is only set to 1 for qos data/qos null/mgmt frame) */
#define     VIF0_CFG_INFO_QUEUE_HTC_APPEND_EN_MASK                               0x100
#define     VIF0_CFG_INFO_QUEUE_HTC_APPEND_EN_SHIFT                                  8
/* disable nav check to response cts */
#define     VIF0_CFG_INFO_CTS_NAV_CHK_DIS_MASK                                   0x200
#define     VIF0_CFG_INFO_CTS_NAV_CHK_DIS_SHIFT                                      9
/* disable nav check for asso sta trig resp */
#define     VIF0_CFG_INFO_TRIG_NAV_ASSO_CHK_DIS_MASK                             0x400
#define     VIF0_CFG_INFO_TRIG_NAV_ASSO_CHK_DIS_SHIFT                               10
/* disable nav check for unasso sta trig resp */
#define     VIF0_CFG_INFO_TRIG_NAV_UNASSO_CHK_DIS_MASK                           0x800
#define     VIF0_CFG_INFO_TRIG_NAV_UNASSO_CHK_DIS_SHIFT                             11
/* disable cs check for asso sta trig resp */
#define     VIF0_CFG_INFO_TRIG_CS_ASSO_CHK_DIS_MASK                             0x1000
#define     VIF0_CFG_INFO_TRIG_CS_ASSO_CHK_DIS_SHIFT                                12
/* disable cs check for unasso sta trig resp */
#define     VIF0_CFG_INFO_TRIG_CS_UNASSO_CHK_DIS_MASK                           0x2000
#define     VIF0_CFG_INFO_TRIG_CS_UNASSO_CHK_DIS_SHIFT                              13
/* each sub 20Mhz channel mappging for cca */
/* bit [1:0] is the mappging for lowest 20Mhz channel in whole 80Mhz, */
/* bit [3:2] is the mappding for the second lower 20Mhz channel in 80Mhze */
/* bit [5:4] is the mappding for third lowest 20Mhz channel in whole 80Mhz */
/* bit [7:6] is the mappding for higest 20Mhze channel in whole 80Mhz */
#define     VIF0_CFG_INFO_SUBCHANNEL_20_MAPPING_MASK                          0xff0000
#define     VIF0_CFG_INFO_SUBCHANNEL_20_MAPPING_SHIFT                               16
/* don't get OBSS Narrow Bandwidth RU In OFDMA Tolerance Support field  from OBSS beacon */
#define     VIF0_CFG_INFO_OBSS_NARROW_BW_RU_IN_OFDMA_LOST_MASK               0x1000000
#define     VIF0_CFG_INFO_OBSS_NARROW_BW_RU_IN_OFDMA_LOST_SHIFT                     24
/* phy operation channle is DFS-50-100. */
/* if DFS_50_100_CON and OBSS_NARROW_BW_RU_IN_OFDMA_LOST registers are both 1, */
/* and the RU get in TRS/trigger is 26 tone, we would not response HETB ppdu, and update nav */
#define     VIF0_CFG_INFO_DFS_50_100_CON_MASK                                0x2000000
#define     VIF0_CFG_INFO_DFS_50_100_CON_SHIFT                                      25
/* convert HTC field for HE TB PPDU(only for data send from MT) */
/* if QUEUE_UPH_CONVERT_EN is 1 and QUEUE_UPH_EN is 1, convert to uph field; */
/* if if QUEUE_UPH_CONVERT_EN is 1 and QUEUE_UPH_EN is 0, convert BSR field; */
/* hw would check the qos data/qos null/mgmt frame htc + field and do convert */
#define     VIF0_CFG_INFO_QUEUE_HTC_CONVERT_EN_MASK                          0x4000000
#define     VIF0_CFG_INFO_QUEUE_HTC_CONVERT_EN_SHIFT                                26
/* disable sequence number increase if send qos null automaticly in hetb ppdu */
#define     VIF0_CFG_INFO_QOSNULL_SEQ_INC_DIS_MASK                           0x8000000
#define     VIF0_CFG_INFO_QOSNULL_SEQ_INC_DIS_SHIFT                                 27
/* receiving he ppdu with color 0 but doesn't have fcs passed mpdu, */
/* update inbss nav for this he ppdu */
/* notice: can set this register to 1'b1 after association with AP, */
/* and can help dev response trigger in noisy condition */
#define     VIF0_CFG_INFO_COLOR_INBSS_MASK                                  0x20000000
#define     VIF0_CFG_INFO_COLOR_INBSS_SHIFT                                         29
/* interface operation mode */
/* 2'b00 : 20Mhz; 2'b01 : 40Mhz; 2'b10 : 80Mhz; 2'b11 :160Mhz or 80+80Mhz */
#define     VIF0_CFG_INFO_OP_MODE_MASK                                      0xc0000000
#define     VIF0_CFG_INFO_OP_MODE_SHIFT                                             30

#define REG_VIF0_MBSSID_MASK_L32                                                 0x18C
/* mbssid mask low 32 bit */
#define     VIF0_MBSSID_MASK_L32_MBSSID_MASK_31_0_MASK                      0xffffffff
#define     VIF0_MBSSID_MASK_L32_MBSSID_MASK_31_0_SHIFT                              0

#define REG_VIF0_MBSSID_MASK_H16                                                 0x190
/* mbssid mask high 16 bit */
#define     VIF0_MBSSID_MASK_H16_MBSSID_MASK_47_32_MASK                         0xffff
#define     VIF0_MBSSID_MASK_H16_MBSSID_MASK_47_32_SHIFT                             0

#define REG_VIF0_TRIG                                                            0x194
/* resoure request buffer threshold exponent for nfrp */
#define     VIF0_TRIG_RESOURCE_THR_EXP_MASK                                       0xff
#define     VIF0_TRIG_RESOURCE_THR_EXP_SHIFT                                         0
/* resoure request buffer exponent to compare with */
/* resoure request buffer threshold exponent to fill ndp_feedback_status */
#define     VIF0_TRIG_RESOURCE_EXP_MASK                                         0xff00
#define     VIF0_TRIG_RESOURCE_EXP_SHIFT                                             8
/* if set to 1, interface is asleep and not response nfrp trigger */
#define     VIF0_TRIG_INTF_ASLEEP_MASK                                         0x10000
#define     VIF0_TRIG_INTF_ASLEEP_SHIFT                                             16
/* other channels are busy */
/* set to 1 would feedback another 80Mhz bandwidth is busy in BQR field */
#define     VIF0_TRIG_O_80_BUSY_MASK                                           0x20000
#define     VIF0_TRIG_O_80_BUSY_SHIFT                                               17
/* other 80Mhz channel status location */
/* if this bit is 0, another 80Mhz channel would be in [7:4] in BQR field, */
/* our working 80Mhz is in [3:0] */
#define     VIF0_TRIG_O_80_LOC_MASK                                            0x40000
#define     VIF0_TRIG_O_80_LOC_SHIFT                                                18
/* mask 4bit channel_avail for BQR field. */
/* Each bit set to 1, use the monitered one; */
/* Each bit set to 0 the channel is avail(O_80_BUSY is 0) or not avial(O_80_BUSY is 1) */
#define     VIF0_TRIG_BQR_CH_MASK_MASK                                        0xf00000
#define     VIF0_TRIG_BQR_CH_MASK_SHIFT                                             20
/* each 20Mhz channle locaiton in bqr field */
/* bit [1:0] is the mapping for cca[0] */
/* bit [3:2] is the mapding for cca[1] */
/* bit [5:4] is the mapping for cca[2] */
/* bit [7:6] is the mapping for cca[3] */
#define     VIF0_TRIG_BQRP_CH20_MAPPING_MASK                                0xff000000
#define     VIF0_TRIG_BQRP_CH20_MAPPING_SHIFT                                       24

#define REG_VIF0_ESP                                                             0x198
/* Service period check enable for twt */
/* If this this bit is 1, would enable to generate sp terminiation interrupt. */
/* There would have dedicated intr enable bit for each type of interrupt. */
#define     VIF0_ESP_SP_CHECK_EN_MASK                                              0x1
#define     VIF0_ESP_SP_CHECK_EN_SHIFT                                               0
/* reserved bit to use */
/* enable treat more data field in ba/ack is 0 as twt sp end. */
/* More Data Ack subfield of the QoS Info field */
/* Currnelty, SW keeps this bit to 0. */
/* this bit is active when sp_check_en is set to 1 */
#define     VIF0_ESP_RSVD_ACK_BA_MORE_DATA_CHK_MASK                                0x2
#define     VIF0_ESP_RSVD_ACK_BA_MORE_DATA_CHK_SHIFT                                 1
/* enable monitor more data field in control frame(exclude ack/ba/trigger) is 0, */
/* treat this as twt sp end */
/* this bit is active when sp_check_en is set to 1 */
#define     VIF0_ESP_CTRL_MORE_DATA_CHK_MASK                                       0x4
#define     VIF0_ESP_CTRL_MORE_DATA_CHK_SHIFT                                        2

#define REG_VIF0_ESP_STATUS                                                      0x198
/* Service period check enable for twt */
/* If this this bit is 1, would enable to generate sp terminiation interrupt. */
/* There would have dedicated intr enable bit for  each type of interrupt. */
#define     VIF0_ESP_STATUS_SP_CHECK_EN_MASK                                       0x1
#define     VIF0_ESP_STATUS_SP_CHECK_EN_SHIFT                                        0
/* reserved bit to use */
/* enable treat more data field in ba/ack is 0 as twt sp end. */
/* More Data Ack subfield of the QoS Info field */
/* Currnelty, SW keeps this bit to 0. */
/* this bit is active when sp_check_en is set to 1 */
#define     VIF0_ESP_STATUS_RSVD_ACK_BA_MORE_DATA_CHK_MASK                         0x2
#define     VIF0_ESP_STATUS_RSVD_ACK_BA_MORE_DATA_CHK_SHIFT                          1
/* enable monitor more data field in control frame(exclude ack/ba/trigger) is 0, */
/* treat this as twt sp end */
/* this bit is active when sp_check_en is set to 1 */
#define     VIF0_ESP_STATUS_CTRL_MORE_DATA_CHK_MASK                                0x4
#define     VIF0_ESP_STATUS_CTRL_MORE_DATA_CHK_SHIFT                                 2
/* rx frame frame type field */
#define     VIF0_ESP_STATUS_FRAME_TYPE_MASK                                    0x30000
#define     VIF0_ESP_STATUS_FRAME_TYPE_SHIFT                                        16
/* rx frame frame subtype field when rx_esp or resp_esp happens */
#define     VIF0_ESP_STATUS_FRAME_SUBTYPE_MASK                                0x3c0000
#define     VIF0_ESP_STATUS_FRAME_SUBTYPE_SHIFT                                     18
/* rx frame more data field in frame control when rx_esp or resp_esp happens */
#define     VIF0_ESP_STATUS_MORE_DATE_MASK                                    0x400000
#define     VIF0_ESP_STATUS_MORE_DATE_SHIFT                                         22
/* qos data or qos null EOSP field when rx_esp or resp_esp happens */
#define     VIF0_ESP_STATUS_QOS_EOSP_MASK                                     0x800000
#define     VIF0_ESP_STATUS_QOS_EOSP_SHIFT                                          23

#define REG_VIF0_HTC_OMI                                                         0x19C
/* enable send HE OMI control field in HETB ppdu in Qos-null frame for basic trigger */
#define     VIF0_HTC_OMI_HETB_QOSNULL_OMI_EN_MASK                                  0x1
#define     VIF0_HTC_OMI_HETB_QOSNULL_OMI_EN_SHIFT                                   0
/* enable send HE OMI control field in HETB ppdu in MT DATA for basic trigger */
#define     VIF0_HTC_OMI_HETB_DATA_OMI_EN_MASK                                     0x2
#define     VIF0_HTC_OMI_HETB_DATA_OMI_EN_SHIFT                                      1
/* OMI control field send in HETB ppdu together with UPH */
#define     VIF0_HTC_OMI_HE_OMI_MASK                                           0xfff00
#define     VIF0_HTC_OMI_HE_OMI_SHIFT                                                8

#define REG_VIF0_BASIC_TRIG_USER_INFO                                            0x1A0
/* rx_basic_trigger_user_vif0, start from ru_allocation field */
#define     VIF0_BASIC_TRIG_USER_INFO_RX_BASIC_TRIGGER_USER_VIF0_MASK       0xffffffff
#define     VIF0_BASIC_TRIG_USER_INFO_RX_BASIC_TRIGGER_USER_VIF0_SHIFT               0

#define REG_VIF0_BASIC_TRIG_PSDU_LEN                                             0x1A4
/* rx_basic_trigger_hetb_psdu_len_vif0 */
#define     VIF0_BASIC_TRIG_PSDU_LEN_RX_BASIC_TRIGGER_HETB_PSDU_LEN_VIF0_MASK   0xfffff
#define     VIF0_BASIC_TRIG_PSDU_LEN_RX_BASIC_TRIGGER_HETB_PSDU_LEN_VIF0_SHIFT         0

#define REG_VIF0_RSVD                                                            0x1FC
/* reserved bit for ECO */
#define     VIF0_RSVD_RESERVED_MASK                                         0xffffffff
#define     VIF0_RSVD_RESERVED_SHIFT                                                 0

#define REG_VIF1_CFG0                                                            0x200
/* Virtual interface 1 enable */
#define     VIF1_CFG0_EN_MASK                                                      0x1
#define     VIF1_CFG0_EN_SHIFT                                                       0
/* 11b Present for Interface 1 */
/* 0 : There is no 11b station present in BSS for interface 1 */
/* 1 : 11b station is present in BSS for interface 1 */
#define     VIF1_CFG0_BP_MASK                                                    0x100
#define     VIF1_CFG0_BP_SHIFT                                                       8
/* Preamble Type for Interface 1 */
/* Preamble type in case of transmiting non-HT frame for interface 1 */
/* (0 : long preamble, 1 : short preamble) */
#define     VIF1_CFG0_PT_MASK                                                    0x200
#define     VIF1_CFG0_PT_SHIFT                                                       9
/* Slot time duration for virtual interface 1 */
#define     VIF1_CFG0_SLOT_TIME_MASK                                          0xff0000
#define     VIF1_CFG0_SLOT_TIME_SHIFT                                               16
/* Update TSF value (new TSF0 value = TSF0 + TSF0_OFFSET when top is 0) */
/* Update TSF value (new TSF0 value = TSF0 - TSF0_OFFSET when top is 1) */
#define     VIF1_CFG0_TU_MASK                                                0x1000000
#define     VIF1_CFG0_TU_SHIFT                                                      24
/* Update TSF value with add or minus operation */
/* (new TSF0 value = TSF0 +/- TSF0_OFFSET) */
/* 1'b0 add; 1'b1 minus */
#define     VIF1_CFG0_TOP_MASK                                               0x2000000
#define     VIF1_CFG0_TOP_SHIFT                                                     25
/* TSF0 value clear to 0 */
/* if TSF0 value clear and value update assert same time, clear the TSF0 value */
#define     VIF1_CFG0_TC_MASK                                                0x4000000
#define     VIF1_CFG0_TC_SHIFT                                                      26

#define REG_VIF1_CFG1                                                            0x204
/* Basic HT-MCS Set Bitmap for interface 1 */
/* LS 16 bits of Rx MCS Bitmask subfield of Basic HT-MCS Set field of the HT Operation parameter */
#define     VIF1_CFG1_BASIC_HT_MCS_SET_MASK                                     0xffff
#define     VIF1_CFG1_BASIC_HT_MCS_SET_SHIFT                                         0
/* Basic Rate Set Bitmap for Interface 1 */
/* [0]  11B, 1Mb/s */
/* [1]  11B, 2Mb/s */
/* [2]  11B, 5.5Mb/s */
/* [3]  11B, 11Mb/s */
/* [4]  Legacy, 6Mb/s */
/* [5]  Legacy, 9Mb/s */
/* [6]  Legacy, 12Mb/s */
/* [7]  Legacy, 18Mb/s */
/* [8]  Legacy, 24Mb/s */
/* [9]  Legacy, 36Mb/s */
/* [10] Legacy, 48Mb/s */
/* [11] Legacy, 54Mb/s */
#define     VIF1_CFG1_BASIC_RATE_BITMAP_MASK                                 0xfff0000
#define     VIF1_CFG1_BASIC_RATE_BITMAP_SHIFT                                       16

#define REG_VIF1_RX_FILTER0                                                      0x208
/* MAC Address Enable */
/* Enable MAC address (enable RA matching for address filtering in case of unicast frame) */
#define     VIF1_RX_FILTER0_MAC_EN_MASK                                            0x3
#define     VIF1_RX_FILTER0_MAC_EN_SHIFT                                             0
/* BSSID Enable */
/* Enable BSSID (enable matching for address filtering in case of broadcast frame) */
#define     VIF1_RX_FILTER0_BSS_EN_MASK                                          0x300
#define     VIF1_RX_FILTER0_BSS_EN_SHIFT                                             8
/* BSSID Filtering Enable */
/* [0] All broadcast frame are transferred to S/W, [1] Enable BSSID address filtering */
#define     VIF1_RX_FILTER0_BFE_MASK                                           0x10000
#define     VIF1_RX_FILTER0_BFE_SHIFT                                               16
/* this vif is in ap mode */
#define     VIF1_RX_FILTER0_AP_MODE_MASK                                       0x20000
#define     VIF1_RX_FILTER0_AP_MODE_SHIFT                                           17

#define REG_VIF1_RX_FILTER1                                                      0x20C
/* Control Frame Filtering Bitmap */
/* [0] not transferred to S/W, [1] transferred to S/W */
#define     VIF1_RX_FILTER1_CTRL_FRAME_FILTER_BITMAP_MASK                       0xffff
#define     VIF1_RX_FILTER1_CTRL_FRAME_FILTER_BITMAP_SHIFT                           0
/* Management Frame Filtering Bitmap */
/* [0] not transferred to S/W, [1] transferred to S/W */
#define     VIF1_RX_FILTER1_MGNT_FRAME_FILTER_BITMAP_MASK                   0xffff0000
#define     VIF1_RX_FILTER1_MGNT_FRAME_FILTER_BITMAP_SHIFT                          16

#define REG_VIF1_MAC0_L                                                          0x210
/* MAC Address 0 low 32 bit */
#define     VIF1_MAC0_L_MAC_ADDR0_31_0_MASK                                 0xffffffff
#define     VIF1_MAC0_L_MAC_ADDR0_31_0_SHIFT                                         0

#define REG_VIF1_MAC0_H                                                          0x214
/* MAC Address 0 high 16 bit */
#define     VIF1_MAC0_H_MAC_ADDR0_47_36_MASK                                    0xffff
#define     VIF1_MAC0_H_MAC_ADDR0_47_36_SHIFT                                        0

#define REG_VIF1_MAC1_L                                                          0x218
/* MAC Address 1 low 32 bit */
#define     VIF1_MAC1_L_MAC_ADDR1_31_0_MASK                                 0xffffffff
#define     VIF1_MAC1_L_MAC_ADDR1_31_0_SHIFT                                         0

#define REG_VIF1_MAC1_H                                                          0x21C
/* MAC Address 1 high 16 bit */
#define     VIF1_MAC1_H_MAC_ADDR1_47_16_MASK                                    0xffff
#define     VIF1_MAC1_H_MAC_ADDR1_47_16_SHIFT                                        0

#define REG_VIF1_BSS0_L                                                          0x220
/* BSSID 0 low 32 bit */
#define     VIF1_BSS0_L_BSSID0_31_0_MASK                                    0xffffffff
#define     VIF1_BSS0_L_BSSID0_31_0_SHIFT                                            0

#define REG_VIF1_BSS0_H                                                          0x224
/* BSSID 0 high 16 bit */
#define     VIF1_BSS0_H_BSSID0_47_16_MASK                                       0xffff
#define     VIF1_BSS0_H_BSSID0_47_16_SHIFT                                           0

#define REG_VIF1_BSS1_L                                                          0x228
/* BSSID 1 low 32 bit */
#define     VIF1_BSS1_L_BSSID1_31_0_MASK                                    0xffffffff
#define     VIF1_BSS1_L_BSSID1_31_0_SHIFT                                            0

#define REG_VIF1_BSS1_H                                                          0x22C
/* BSSID 1 high 16 bit */
#define     VIF1_BSS1_H_BSSID1_47_16_MASK                                       0xffff
#define     VIF1_BSS1_H_BSSID1_47_16_SHIFT                                           0

#define REG_VIF1_AID                                                             0x230
/* Association ID for interface 0 */
#define     VIF1_AID_AID_MASK                                                    0xfff
#define     VIF1_AID_AID_SHIFT                                                       0

#define REG_VIF1_BFM0                                                            0x234
/* Grouping value for Compressed Beamforming frame in response to HT NDP Announcement */
#define     VIF1_BFM0_HT_NG_MASK                                                   0x3
#define     VIF1_BFM0_HT_NG_SHIFT                                                    0
/* Cookbook Information value for Compressed Beamforming frame in response to HT NDP Announcement */
#define     VIF1_BFM0_HT_CB_MASK                                                   0xc
#define     VIF1_BFM0_HT_CB_SHIFT                                                    2
/* Format for Compressed Beamforming frame in response to HT NDP Announcement */
#define     VIF1_BFM0_HT_FORMAT_MASK                                              0x70
#define     VIF1_BFM0_HT_FORMAT_SHIFT                                                4
/* MCS for Compressed Beamforming frame in response to HT NDP Announcement */
#define     VIF1_BFM0_HT_MCS_MASK                                               0x7f00
#define     VIF1_BFM0_HT_MCS_SHIFT                                                   8
/* Grouping value for Compressed Beamforming frame in response to VHT NDP Announcement */
#define     VIF1_BFM0_VHT_NG_MASK                                              0x30000
#define     VIF1_BFM0_VHT_NG_SHIFT                                                  16
/* Codebook Information value for Compressed Beamforming frame in response to VHT NDP Announcemen */
#define     VIF1_BFM0_VHT_CB_MASK                                              0xc0000
#define     VIF1_BFM0_VHT_CB_SHIFT                                                  18
/* Format for Compressed Beamforming frame in response to VHT N`DP Announcement */
#define     VIF1_BFM0_VHT_FORMAT_MASK                                         0x700000
#define     VIF1_BFM0_VHT_FORMAT_SHIFT                                              20
/* MCS for Compressed Beamforming frame in response to VHT NDP Announcement */
#define     VIF1_BFM0_VHT_MCS_MASK                                          0x7f000000
#define     VIF1_BFM0_VHT_MCS_SHIFT                                                 24

#define REG_VIF1_BFM1                                                            0x238
/* Grouping value for Compressed Beamforming frame in response to HE NDP Announcement */
#define     VIF1_BFM1_HE_NG_MASK                                                   0x3
#define     VIF1_BFM1_HE_NG_SHIFT                                                    0
/* Codebook Information value for Compressed Beamforming frame */
/* in response to HE NDP Announcement */
#define     VIF1_BFM1_HE_CB_MASK                                                   0xc
#define     VIF1_BFM1_HE_CB_SHIFT                                                    2
/* Format for Compressed Beamforming frame in response to HE NDP Announcement */
#define     VIF1_BFM1_HE_FORMAT_MASK                                              0x70
#define     VIF1_BFM1_HE_FORMAT_SHIFT                                                4
/* MCS for Compressed Beamforming frame in response to HE NDP Announcement */
#define     VIF1_BFM1_HE_MCS_MASK                                               0x7f00
#define     VIF1_BFM1_HE_MCS_SHIFT                                                   8

#define REG_VIF1_BFM2                                                            0x23C
/* Enable function to limit maximum vaule for reported SNR0 */
#define     VIF1_BFM2_SNR0_LIMIT_EN_MASK                                           0x1
#define     VIF1_BFM2_SNR0_LIMIT_EN_SHIFT                                            0
/* Maximum value for SNR0 in signed 2's complement */
/* used for limiting maximum reported SNR0 value */
#define     VIF1_BFM2_SNR0_MAX_MASK                                             0xff00
#define     VIF1_BFM2_SNR0_MAX_SHIFT                                                 8
/* Enable function to limit maximum vaule for reported SNR1 */
#define     VIF1_BFM2_SNR1_LIMIT_EN_MASK                                       0x10000
#define     VIF1_BFM2_SNR1_LIMIT_EN_SHIFT                                           16
/* Maximum value for SNR1 in signed 2's complement */
/* used for limiting maximum reported SNR0 value */
#define     VIF1_BFM2_SNR1_MAX_MASK                                         0xff000000
#define     VIF1_BFM2_SNR1_MAX_SHIFT                                                24

#define REG_VIF1_TSF_L                                                           0x240
/* tsf [31:0] */
/* 64-bit timestamp timer for interface 0 */
#define     VIF1_TSF_L_TSF_31_0_MASK                                        0xffffffff
#define     VIF1_TSF_L_TSF_31_0_SHIFT                                                0

#define REG_VIF1_TSF_H                                                           0x244
/* tsf [63:32] */
/* 64-bit timestamp timer for interface 0 */
#define     VIF1_TSF_H_TSF_63_32_MASK                                       0xffffffff
#define     VIF1_TSF_H_TSF_63_32_SHIFT                                               0

#define REG_VIF1_TSF_OFFSET_L                                                    0x248
/* tsf offset [31:0] */
/* 64-bit timestamp offset value to update for interface 0 */
/* (new TSF = current TSF + TSF_OFFSET) */
#define     VIF1_TSF_OFFSET_L_TSF_OFFSET_31_0_MASK                          0xffffffff
#define     VIF1_TSF_OFFSET_L_TSF_OFFSET_31_0_SHIFT                                  0

#define REG_VIF1_TSF_OFFSET_H                                                    0x24C
/* tsf offset [63:32] */
/* 64-bit timestamp offset value to update for interface 0 */
/* (new TSF = current TSF + TSF_OFFSET) */
#define     VIF1_TSF_OFFSET_H_TSF_OFFSET_63_32_MASK                         0xffffffff
#define     VIF1_TSF_OFFSET_H_TSF_OFFSET_63_32_SHIFT                                 0

#define REG_VIF1_TSF_TIME0                                                       0x250
/* TSF value for TIME0 interrupt */
/* Timestamp value for interface 0 to generate TIME0 interrupt */
#define     VIF1_TSF_TIME0_TSF_TIME0_MASK                                   0xffffffff
#define     VIF1_TSF_TIME0_TSF_TIME0_SHIFT                                           0

#define REG_VIF1_TSF_TIME1                                                       0x254
/* TSF value for TIME1 interrupt */
/* Timestamp value for interface 0 to generate TIME1 interrupt */
#define     VIF1_TSF_TIME1_TSF_TIME0_MASK                                   0xffffffff
#define     VIF1_TSF_TIME1_TSF_TIME0_SHIFT                                           0

#define REG_VIF1_TRIG_RESP_QUEUE_EN                                              0x258
/* reserved field */
#define     VIF1_TRIG_RESP_QUEUE_EN_RSVD_MASK                                   0x7fff
#define     VIF1_TRIG_RESP_QUEUE_EN_RSVD_SHIFT                                       0

#define REG_VIF1_BSR                                                             0x25C
/* BSR control subfield content for for HTC he varaint, */
/* please refering 11ax D4.2 9.2.4.6a.4 for details */
#define     VIF1_BSR_BSR_CONTENT_MASK                                        0x3ffffff
#define     VIF1_BSR_BSR_CONTENT_SHIFT                                               0

#define REG_VIF1_MIN_MPDU_SPACING                                                0x260
/* minimal mpdu start spacing */
#define     VIF1_MIN_MPDU_SPACING_MMSS_MASK                                        0x7
#define     VIF1_MIN_MPDU_SPACING_MMSS_SHIFT                                         0
/* STA default PE duration, valid value is 0~4 */
#define     VIF1_MIN_MPDU_SPACING_DEFAULT_PE_DURATION_MASK                        0x38
#define     VIF1_MIN_MPDU_SPACING_DEFAULT_PE_DURATION_SHIFT                          3
/* he bss color for this interface, once COLOR_EN is 1, */
/* need to set BSS_COLOR to non-zero */
/* before association, can also fill bss_color field to the bss color value */
/* we want to assoicate. this time keep the COLOR_EN to 0 */
#define     VIF1_MIN_MPDU_SPACING_BSS_COLOR_MASK                                0x3f00
#define     VIF1_MIN_MPDU_SPACING_BSS_COLOR_SHIFT                                    8
/* BSS_COLOR enable */
#define     VIF1_MIN_MPDU_SPACING_COLOR_EN_MASK                                 0x4000
#define     VIF1_MIN_MPDU_SPACING_COLOR_EN_SHIFT                                    14
/* BSS_COLOR is disabled */
#define     VIF1_MIN_MPDU_SPACING_COLOR_DIS_MASK                                0x8000
#define     VIF1_MIN_MPDU_SPACING_COLOR_DIS_SHIFT                                   15
/* enable to receive trigger frame */
/* only support basic trigger/murts/mubrpoll/mubar/bsrp/bqrp/nfrp */
#define     VIF1_MIN_MPDU_SPACING_TRIG_EN_MASK                                 0x10000
#define     VIF1_MIN_MPDU_SPACING_TRIG_EN_SHIFT                                     16
/* nable to support associated AP UORA */
#define     VIF1_MIN_MPDU_SPACING_TRIG_ASSO_UORA_EN_MASK                       0x20000
#define     VIF1_MIN_MPDU_SPACING_TRIG_ASSO_UORA_EN_SHIFT                           17
/* enable to supported unassoicated AP UORA */
/* notice TRIG_UNASSO_UORA and TRIG_ASSO_UORA is excluded */
#define     VIF1_MIN_MPDU_SPACING_TRIG_UNASSO_UORA_EN_MASK                     0x40000
#define     VIF1_MIN_MPDU_SPACING_TRIG_UNASSO_UORA_EN_SHIFT                         18
/* support TRS in HTC */
#define     VIF1_MIN_MPDU_SPACING_TRS_EN_MASK                                  0x80000
#define     VIF1_MIN_MPDU_SPACING_TRS_EN_SHIFT                                      19
/* support partial bss color feature */
#define     VIF1_MIN_MPDU_SPACING_PARTIAL_BSS_COLOR_EN_MASK                   0x100000
#define     VIF1_MIN_MPDU_SPACING_PARTIAL_BSS_COLOR_EN_SHIFT                        20
/* only support basic nav that is basic nav, */
/* notice this feature is only applied when this vif is AP mode */
#define     VIF1_MIN_MPDU_SPACING_BASIC_NAV_EN_MASK                           0x200000
#define     VIF1_MIN_MPDU_SPACING_BASIC_NAV_EN_SHIFT                                21
/* disable sending data frame in hetb ppdu for basic trigger frame, */
/* only send out ctrl frame(response frame) frame, or mgmt frame in hetb ppdu */
#define     VIF1_MIN_MPDU_SPACING_HETB_DATA_DIS_MASK                          0x400000
#define     VIF1_MIN_MPDU_SPACING_HETB_DATA_DIS_SHIFT                               22
/* in response to BSRP trigger frame, send qos null mpdu with BSR */
/* (if enable this, only send out 1 qosnull frame with bsr subfield in HTC) */
#define     VIF1_MIN_MPDU_SPACING_BSRP_BSR_EN_MASK                            0x800000
#define     VIF1_MIN_MPDU_SPACING_BSRP_BSR_EN_SHIFT                                 23
/* in response to BSRP trigger frame, send several AC's qosnull mpdu(queues have pkt to send) */
/* function is only valid when REG_VIF1_MIN_MPDU_SPACING.bsrp_bsr_en is 1'b0 */
#define     VIF1_MIN_MPDU_SPACING_BSRP_MORE_QOSNULL_EN_MASK                  0x1000000
#define     VIF1_MIN_MPDU_SPACING_BSRP_MORE_QOSNULL_EN_SHIFT                        24

#define REG_VIF1_QNULL_CTRL_AC0                                                  0x264
/* be queue sequece control field for qosnull frame in HETB, map to queue5 */
#define     VIF1_QNULL_CTRL_AC0_SEQUNECE_CTRL_MASK                              0xffff
#define     VIF1_QNULL_CTRL_AC0_SEQUNECE_CTRL_SHIFT                                  0
/* be queue qos control field for qosnull frame in HETB */
/* qos null mpdu ack policy field is set to NO_ACK(2'b01), map to queue5 */
#define     VIF1_QNULL_CTRL_AC0_QOS_CTRL_MASK                               0xffff0000
#define     VIF1_QNULL_CTRL_AC0_QOS_CTRL_SHIFT                                      16

#define REG_VIF1_QNULL_CTRL_AC1                                                  0x268
/* bk queue sequece control field for qosnull frame in HETB, map to queue6 */
#define     VIF1_QNULL_CTRL_AC1_SEQUNECE_CTRL_MASK                              0xffff
#define     VIF1_QNULL_CTRL_AC1_SEQUNECE_CTRL_SHIFT                                  0
/* bk queue qos control field for qosnull frame in HETB, map to queue6 */
/* qos null mpdu ack policy field is set to NO_ACK(2'b01) */
#define     VIF1_QNULL_CTRL_AC1_QOS_CTRL_MASK                               0xffff0000
#define     VIF1_QNULL_CTRL_AC1_QOS_CTRL_SHIFT                                      16

#define REG_VIF1_QNULL_CTRL_AC2                                                  0x26C
/* vi queue sequece control field for qosnull frame in HETB, map to queue7 */
#define     VIF1_QNULL_CTRL_AC2_SEQUNECE_CTRL_MASK                              0xffff
#define     VIF1_QNULL_CTRL_AC2_SEQUNECE_CTRL_SHIFT                                  0
/* vi queue qos control field for qosnull frame in HETB, map to queue7 */
/* qos null mpdu ack policy field is set to NO_ACK(2'b01) */
#define     VIF1_QNULL_CTRL_AC2_QOS_CTRL_MASK                               0xffff0000
#define     VIF1_QNULL_CTRL_AC2_QOS_CTRL_SHIFT                                      16

#define REG_VIF1_QNULL_CTRL_AC3                                                  0x270
/* vo queue sequece control field for qosnull frame in HETB, map to queue8 */
#define     VIF1_QNULL_CTRL_AC3_SEQUNECE_CTRL_MASK                              0xffff
#define     VIF1_QNULL_CTRL_AC3_SEQUNECE_CTRL_SHIFT                                  0
/* vo queue qos control field for qosnull frame in HETB, map to queue8 */
/* qos null mpdu ack policy field is set to NO_ACK(2'b01) */
#define     VIF1_QNULL_CTRL_AC3_QOS_CTRL_MASK                               0xffff0000
#define     VIF1_QNULL_CTRL_AC3_QOS_CTRL_SHIFT                                      16

#define REG_VIF1_UORA_CFG                                                        0x274
/* OBO initial value */
/* use this register for obo counter if obo counter is 0 and SW_OBO is set to 1 */
#define     VIF1_UORA_CFG_OBO_INIT_VALUE_MASK                                     0x7f
#define     VIF1_UORA_CFG_OBO_INIT_VALUE_SHIFT                                       0
/* use OBO_INIT_SW value for obo contention */
/* if SW_OBO_VALUE_EN is 1'b1, uses OBO_INIT_VALUE value for initial obo counter */
/* if SW_OBO_VALUE_EN is 1'b0, use lfsr to random gen initial obo counter */
#define     VIF1_UORA_CFG_SW_OBO_VALUE_EN_MASK                                    0x80
#define     VIF1_UORA_CFG_SW_OBO_VALUE_EN_SHIFT                                      7
/* init obo contention windo */
/* sw need to write this register to 1 to initiate CW once want to do UORA */
/* (hw would latch CW_MIN to CURR_CW) */
#define     VIF1_UORA_CFG_CW_INIT_MASK                                          0x8000
#define     VIF1_UORA_CFG_CW_INIT_SHIFT                                             15
/* min obo contention window, (2 power cw_min)-1 */
#define     VIF1_UORA_CFG_CW_MIN_MASK                                          0x70000
#define     VIF1_UORA_CFG_CW_MIN_SHIFT                                              16
/* max obo contention window, (2 power cw_max)-1 */
#define     VIF1_UORA_CFG_CW_MAX_MASK                                         0x380000
#define     VIF1_UORA_CFG_CW_MAX_SHIFT                                              19

#define REG_VIF1_UORA_CFG_STATUS                                                 0x274
/* OBO initial value */
/* use this register for obo counter if obo counter is 0 and SW_OBO is set to 1 */
#define     VIF1_UORA_CFG_STATUS_OBO_INIT_VALUE_MASK                              0x7f
#define     VIF1_UORA_CFG_STATUS_OBO_INIT_VALUE_SHIFT                                0
/* use OBO_INIT_SW value for obo contention */
/* if SW_OBO_VALUE_EN is 1'b1, uses OBO_INIT_VALUE value for initial obo counter */
/* if SW_OBO_VALUE_EN is 1'b0, use lfsr to random gen initial obo counter */
#define     VIF1_UORA_CFG_STATUS_SW_OBO_VALUE_EN_MASK                             0x80
#define     VIF1_UORA_CFG_STATUS_SW_OBO_VALUE_EN_SHIFT                               7
/* currently used obo value for contention(read_only) */
#define     VIF1_UORA_CFG_STATUS_OBO_VALUE_MASK                                 0x7f00
#define     VIF1_UORA_CFG_STATUS_OBO_VALUE_SHIFT                                     8
/* init obo contention window */
/* sw need to write this register to 1 to initiate CW once want to do UORA */
/* (hw would latch CW_MIN to CURR_CW) */
#define     VIF1_UORA_CFG_STATUS_CW_INIT_MASK                                   0x8000
#define     VIF1_UORA_CFG_STATUS_CW_INIT_SHIFT                                      15
/* min obo contention window, (2 power cw_min)-1 */
#define     VIF1_UORA_CFG_STATUS_CW_MIN_MASK                                   0x70000
#define     VIF1_UORA_CFG_STATUS_CW_MIN_SHIFT                                       16
/* max obo contention window, (2 power cw_max)-1 */
#define     VIF1_UORA_CFG_STATUS_CW_MAX_MASK                                  0x380000
#define     VIF1_UORA_CFG_STATUS_CW_MAX_SHIFT                                       19
/* currently used obo contetion window, (2 power curr_cw)-1 (read only) */
#define     VIF1_UORA_CFG_STATUS_CURR_CW_MASK                                0x1c00000
#define     VIF1_UORA_CFG_STATUS_CURR_CW_SHIFT                                      22

#define REG_VIF1_UORA_CFG1                                                       0x278
/* each 20Mhz channel enable */
/* each bit is1 means that 20Mhz subchannel is enabled; */
/* bit 0 is the lowest 20Mhz in 80Mhz channel, and bit 3 this the highest 20Mhz in 80Mhz channel */
/* bit [3:0] for for primay 80Mhz, bit [7:4] is for secondary 80Mhz */
/* only one bit is1 in CHANNEL_EN[7:0] is 1, that is channel switches to that sub 20Mhz channel */
/* CHANNEL_EN[7:4] is 4'hF, that is operation channel switches to secondary 80Mhz */
#define     VIF1_UORA_CFG1_CHANNEL_EN_MASK                                        0xff
#define     VIF1_UORA_CFG1_CHANNEL_EN_SHIFT                                          0
/* uora supported HE LTF MODE */
/* if bit0 is 1'b1, supports single pilot HE-LTF mode */
/* if bit1 is 1'b1, supports masked HE-LTF sequnce mode */
/* bit 1 is only used for 2x HE-LTF + 1.6us GI or 4x HE-LTF + 3.2us GI hetb ppdu */
#define     VIF1_UORA_CFG1_LTF_MODE_EN_MASK                                      0x300
#define     VIF1_UORA_CFG1_LTF_MODE_EN_SHIFT                                         8
/* 1'b0 means 160Mhz, 1'b1 means 80+80Mhz */
#define     VIF1_UORA_CFG1_80P80_MODE_MASK                                       0x400
#define     VIF1_UORA_CFG1_80P80_MODE_SHIFT                                         10
/* primary channel location */
/* 3'b000, primary in lowest 20Mhz; 3'b001, primary in second lowest 20Mhz; */
/* 3'b010, primary in third lowest 20Mhz; 3'b011, primary in fourth lowest20Mhz; */
/* 3'b100, primary in fifth lowest 20Mhz; 3'b101, primary in sixth lowest 20Mhz */
/* 3'b110, primary in seventh lowest 20Mhz, 3'b111, primary in highest 20Mhz; */
/* 4~7 is used for associated with an AP supporting 80+80 or 160Mhz */
#define     VIF1_UORA_CFG1_PRI_CH_MASK                                          0x3800
#define     VIF1_UORA_CFG1_PRI_CH_SHIFT                                             11

#define REG_VIF1_NON_SRG_OBSS_PD                                                 0x27C
/* non_SRG obss pd min value, default value -82dbm */
#define     VIF1_NON_SRG_OBSS_PD_MIN_VALUE_MASK                                   0xff
#define     VIF1_NON_SRG_OBSS_PD_MIN_VALUE_SHIFT                                     0
/* non_SRG obss pd max value, default value -62dbm */
#define     VIF1_NON_SRG_OBSS_PD_MAX_VALUE_MASK                                 0xff00
#define     VIF1_NON_SRG_OBSS_PD_MAX_VALUE_SHIFT                                     8

#define REG_VIF1_SRG_OBSS_PD                                                     0x280
/* SRG obss pd min value, default value -82dbm */
#define     VIF1_SRG_OBSS_PD_MIN_VALUE_MASK                                       0xff
#define     VIF1_SRG_OBSS_PD_MIN_VALUE_SHIFT                                         0
/* SRG obss pd max value, default value -82dbm */
#define     VIF1_SRG_OBSS_PD_MAX_VALUE_MASK                                     0xff00
#define     VIF1_SRG_OBSS_PD_MAX_VALUE_SHIFT                                         8

#define REG_VIF1_TX_PWR                                                          0x284
/* tx ref power used for SR operation, default value 21dbm */
#define     VIF1_TX_PWR_REF_PWR_MASK                                              0xff
#define     VIF1_TX_PWR_REF_PWR_SHIFT                                                0
/* tx power used for frame implicitly sent by MAC H/W */
/* (RTS/CTS/ACK/BA/CBF/CF-End) */
#define     VIF1_TX_PWR_HW_PWR_MASK                                             0xff00
#define     VIF1_TX_PWR_HW_PWR_SHIFT                                                 8

#define REG_VIF1_CFG_INFO                                                        0x288
/* que contention enable */
/* REG_VIF0_CFG_INFO.que_contention_en[4:0] is for que[4:0]; */
/* REG_VIF1_CFG_INFO.que_contention_en[4:0] is for que[9:5] */
#define     VIF1_CFG_INFO_QUE_CONTENTION_EN_MASK                                  0x1f
#define     VIF1_CFG_INFO_QUE_CONTENTION_EN_SHIFT                                    0
/* currently maintained NAV stores to the backup one, */
/* and re_latch the back up nav for nav calculation. */
/* This is used for this interface channel switch */
#define     VIF1_CFG_INFO_NAV_SWITCH_MASK                                         0x20
#define     VIF1_CFG_INFO_NAV_SWITCH_SHIFT                                           5
/* qos null send with UPH field */
/* if this register is 1, Qos null send with UPH field in HE TB ppdu, */
/* else send with BSR field(except BSRP/BQRP) */
#define     VIF1_CFG_INFO_QOS_NULL_UPH_EN_MASK                                    0x40
#define     VIF1_CFG_INFO_QOS_NULL_UPH_EN_SHIFT                                      6
/* replace/append HTC field with UPH field */
/* if this register is 1, send queue pkt(from queues 5~9) to */
/* replace the HTC with HE HTC UPH field in HE TB ppdu */
/* if this register is 0, send queue pkt(from queues 5~9) to */
/* replace the HTC with HE HTC BSR field in HE TB ppdu */
#define     VIF1_CFG_INFO_QUEUE_UPH_EN_MASK                                       0x80
#define     VIF1_CFG_INFO_QUEUE_UPH_EN_SHIFT                                         7
/* add HTC field for HE TB PPDU(only for data send from MT) */
/* if QUEUE_HTC_APPEND_EN is 1 and QUEUE_UPH_EN is 1, append uph field; */
/* if QUEUE_HTC_APPEND_EN is 1 and QUEUE_UPH_EN is 0, append BSR field; */
/* hw also would check the no_htc bit in MT is 1 and then append */
/* (no_htc bit is only set to 1 for qos data/qos null/mgmt frame) */
#define     VIF1_CFG_INFO_QUEUE_HTC_APPEND_EN_MASK                               0x100
#define     VIF1_CFG_INFO_QUEUE_HTC_APPEND_EN_SHIFT                                  8
/* disable nav check to response cts */
#define     VIF1_CFG_INFO_CTS_NAV_CHK_DIS_MASK                                   0x200
#define     VIF1_CFG_INFO_CTS_NAV_CHK_DIS_SHIFT                                      9
/* disable nav check for asso sta trig resp */
#define     VIF1_CFG_INFO_TRIG_NAV_ASSO_CHK_DIS_MASK                             0x400
#define     VIF1_CFG_INFO_TRIG_NAV_ASSO_CHK_DIS_SHIFT                               10
/* disable nav check for unasso sta trig resp */
#define     VIF1_CFG_INFO_TRIG_NAV_UNASSO_CHK_DIS_MASK                           0x800
#define     VIF1_CFG_INFO_TRIG_NAV_UNASSO_CHK_DIS_SHIFT                             11
/* disable cs check for asso sta trig resp */
#define     VIF1_CFG_INFO_TRIG_CS_ASSO_CHK_DIS_MASK                             0x1000
#define     VIF1_CFG_INFO_TRIG_CS_ASSO_CHK_DIS_SHIFT                                12
/* disable cs check for unasso sta trig resp */
#define     VIF1_CFG_INFO_TRIG_CS_UNASSO_CHK_DIS_MASK                           0x2000
#define     VIF1_CFG_INFO_TRIG_CS_UNASSO_CHK_DIS_SHIFT                              13
/* each sub 20Mhz channel mappging for cca */
/* bit [1:0] is the mappging for lowest 20Mhz channel in whole 80Mhz, */
/* bit [3:2] is the mappding for the second lower 20Mhz channel in 80Mhze */
/* bit [5:4] is the mappding for third lowest 20Mhz channel in whole 80Mhz */
/* bit [7:6] is the mappding for higest 20Mhze channel in whole 80Mhz */
#define     VIF1_CFG_INFO_SUBCHANNEL_20_MAPPING_MASK                          0xff0000
#define     VIF1_CFG_INFO_SUBCHANNEL_20_MAPPING_SHIFT                               16
/* don't get OBSS Narrow Bandwidth RU In OFDMA Tolerance Support field  from OBSS beacon */
#define     VIF1_CFG_INFO_OBSS_NARROW_BW_RU_IN_OFDMA_LOST_MASK               0x1000000
#define     VIF1_CFG_INFO_OBSS_NARROW_BW_RU_IN_OFDMA_LOST_SHIFT                     24
/* phy operation channle is DFS-50-100. */
/* if DFS_50_100_CON and OBSS_NARROW_BW_RU_IN_OFDMA_LOST registers are both 1, */
/* and the RU get in TRS/trigger is 26 tone, we would not response HETB ppdu, and update nav */
#define     VIF1_CFG_INFO_DFS_50_100_CON_MASK                                0x2000000
#define     VIF1_CFG_INFO_DFS_50_100_CON_SHIFT                                      25
/* convert HTC field for HE TB PPDU(only for data send from MT) */
/* if QUEUE_UPH_CONVERT_EN is 1 and QUEUE_UPH_EN is 1, convert to uph field; */
/* if if QUEUE_UPH_CONVERT_EN is 1 and QUEUE_UPH_EN is 0, convert BSR field; */
/* hw would check the qos data/qos null/mgmt frame htc + field and do convert */
#define     VIF1_CFG_INFO_QUEUE_HTC_CONVERT_EN_MASK                          0x4000000
#define     VIF1_CFG_INFO_QUEUE_HTC_CONVERT_EN_SHIFT                                26
/* disable sequence number increase if send qos null automaticly in hetb ppdu */
#define     VIF1_CFG_INFO_QOSNULL_SEQ_INC_DIS_MASK                           0x8000000
#define     VIF1_CFG_INFO_QOSNULL_SEQ_INC_DIS_SHIFT                                 27
/* receiving he ppdu with color 0 but doesn't have fcs passed mpdu, */
/* update inbss nav for this he ppdu */
/* notice: can set this register to 1'b1 after association with AP, */
/* and can help dev response trigger in noisy condition */
#define     VIF1_CFG_INFO_COLOR_INBSS_MASK                                  0x20000000
#define     VIF1_CFG_INFO_COLOR_INBSS_SHIFT                                         29
/* interface operation mode */
/* 2'b00 : 20Mhz; 2'b01 : 40Mhz; 2'b10 : 80Mhz; 2'b11 :160Mhz or 80+80Mhz */
#define     VIF1_CFG_INFO_OP_MODE_MASK                                      0xc0000000
#define     VIF1_CFG_INFO_OP_MODE_SHIFT                                             30

#define REG_VIF1_MBSSID_MASK_L32                                                 0x28C
/* mbssid mask low 32 bit */
#define     VIF1_MBSSID_MASK_L32_MBSSID_MASK_31_0_MASK                      0xffffffff
#define     VIF1_MBSSID_MASK_L32_MBSSID_MASK_31_0_SHIFT                              0

#define REG_VIF1_MBSSID_MASK_H16                                                 0x290
/* mbssid mask high 16 bit */
#define     VIF1_MBSSID_MASK_H16_MBSSID_MASK_47_32_MASK                         0xffff
#define     VIF1_MBSSID_MASK_H16_MBSSID_MASK_47_32_SHIFT                             0

#define REG_VIF1_TRIG                                                            0x294
/* resoure request buffer threshold exponent for nfrp */
#define     VIF1_TRIG_RESOURCE_THR_EXP_MASK                                       0xff
#define     VIF1_TRIG_RESOURCE_THR_EXP_SHIFT                                         0
/* resoure request buffer exponent to compare with */
/* resoure request buffer threshold exponent to fill ndp_feedback_status */
#define     VIF1_TRIG_RESOURCE_EXP_MASK                                         0xff00
#define     VIF1_TRIG_RESOURCE_EXP_SHIFT                                             8
/* if set to 1, interface is asleep and not response nfrp trigger */
#define     VIF1_TRIG_INTF_ASLEEP_MASK                                         0x10000
#define     VIF1_TRIG_INTF_ASLEEP_SHIFT                                             16
/* other channels are busy */
/* set to 1 would feedback another 80Mhz bandwidth is busy in BQR field */
#define     VIF1_TRIG_O_80_BUSY_MASK                                           0x20000
#define     VIF1_TRIG_O_80_BUSY_SHIFT                                               17
/* other 80Mhz channel status location */
/* if this bit is 0, another 80Mhz channel would be in [7:4] in BQR field, */
/* our working 80Mhz is in [3:0] */
#define     VIF1_TRIG_O_80_LOC_MASK                                            0x40000
#define     VIF1_TRIG_O_80_LOC_SHIFT                                                18
/* mask 4bit channel_avail for BQR field. */
/* Each bit set to 1, use the monitered one; */
/* Each bit set to 0 the channel is avail(O_80_BUSY is 0) or not avial(O_80_BUSY is 1) */
#define     VIF1_TRIG_BQR_CH_MASK_MASK                                        0xf00000
#define     VIF1_TRIG_BQR_CH_MASK_SHIFT                                             20
/* each 20Mhz channle locaiton in bqr field */
/* bit [1:0] is the mapping for cca[0] */
/* bit [3:2] is the mapding for cca[1] */
/* bit [5:4] is the mapping for cca[2] */
/* bit [7:6] is the mapping for cca[3] */
#define     VIF1_TRIG_BQRP_CH20_MAPPING_MASK                                0xff000000
#define     VIF1_TRIG_BQRP_CH20_MAPPING_SHIFT                                       24

#define REG_VIF1_ESP                                                             0x298
/* Service period check enable for twt */
/* If this this bit is 1, would enable to generate sp terminiation interrupt. */
/* There would have dedicated intr enable bit for each type of interrupt. */
#define     VIF1_ESP_SP_CHECK_EN_MASK                                              0x1
#define     VIF1_ESP_SP_CHECK_EN_SHIFT                                               0
/* reserved bit to use */
/* enable treat more data field in ba/ack is 0 as twt sp end. */
/* More Data Ack subfield of the QoS Info field */
/* Currnelty, SW keeps this bit to 0. */
/* this bit is active when sp_check_en is set to 1 */
#define     VIF1_ESP_RSVD_ACK_BA_MORE_DATA_CHK_MASK                                0x2
#define     VIF1_ESP_RSVD_ACK_BA_MORE_DATA_CHK_SHIFT                                 1
/* enable monitor more data field in control frame(exclude ack/ba/trigger) is 0, */
/* treat this as as twt sp end */
/* this bit is active when sp_check_en is set to 1 */
#define     VIF1_ESP_CTRL_MORE_DATA_CHK_MASK                                       0x4
#define     VIF1_ESP_CTRL_MORE_DATA_CHK_SHIFT                                        2

#define REG_VIF1_ESP_STATUS                                                      0x298
/* Service period check enable for twt */
/* If this this bit is 1, would enable to generate sp terminiation interrupt. */
/* There would have dedicated intr enable bit for  each type of interrupt. */
#define     VIF1_ESP_STATUS_SP_CHECK_EN_MASK                                       0x1
#define     VIF1_ESP_STATUS_SP_CHECK_EN_SHIFT                                        0
/* reserved bit to use */
/* enable treat more data field in ba/ack is 0 as twt sp end. */
/* More Data Ack subfield of the QoS Info field */
/* Currnelty, SW keeps this bit to 0. */
/* this bit is active when sp_check_en is set to 1 */
#define     VIF1_ESP_STATUS_RSVD_ACK_BA_MORE_DATA_CHK_MASK                         0x2
#define     VIF1_ESP_STATUS_RSVD_ACK_BA_MORE_DATA_CHK_SHIFT                          1
/* enable monitor more data field in control frame(exclude ack/ba/trigger) is 0, */
/* treat this as twt sp end */
/* this bit is active when sp_check_en is set to 1 */
#define     VIF1_ESP_STATUS_CTRL_MORE_DATA_CHK_MASK                                0x4
#define     VIF1_ESP_STATUS_CTRL_MORE_DATA_CHK_SHIFT                                 2
/* rx frame frame type field when rx_esp or resp_esp happens */
#define     VIF1_ESP_STATUS_FRAME_TYPE_MASK                                    0x30000
#define     VIF1_ESP_STATUS_FRAME_TYPE_SHIFT                                        16
/* rx frame frame subtype field when rx_esp or resp_esp happens */
#define     VIF1_ESP_STATUS_FRAME_SUBTYPE_MASK                                0x3c0000
#define     VIF1_ESP_STATUS_FRAME_SUBTYPE_SHIFT                                     18
/* rx frame more data field in frame control when rx_esp or resp_esp happens */
#define     VIF1_ESP_STATUS_MORE_DATE_MASK                                    0x400000
#define     VIF1_ESP_STATUS_MORE_DATE_SHIFT                                         22
/* qos data or qos null EOSP field when rx_esp or resp_esp happens */
#define     VIF1_ESP_STATUS_QOS_EOSP_MASK                                     0x800000
#define     VIF1_ESP_STATUS_QOS_EOSP_SHIFT                                          23

#define REG_VIF1_HTC_OMI                                                         0x29C
/* enable send HE OMI control field in HETB ppdu in Qos-null frame for basic trigger */
#define     VIF1_HTC_OMI_HETB_QOSNULL_OMI_EN_MASK                                  0x1
#define     VIF1_HTC_OMI_HETB_QOSNULL_OMI_EN_SHIFT                                   0
/* enable send HE OMI control field in HETB ppdu in MT DATA for basic trigger */
#define     VIF1_HTC_OMI_HETB_DATA_OMI_EN_MASK                                     0x2
#define     VIF1_HTC_OMI_HETB_DATA_OMI_EN_SHIFT                                      1
/* OMI control field send in HETB ppdu together with UPH */
#define     VIF1_HTC_OMI_HE_OMI_MASK                                           0xfff00
#define     VIF1_HTC_OMI_HE_OMI_SHIFT                                                8

#define REG_VIF1_BASIC_TRIG_USER_INFO                                            0x2A0
/* rx_basic_trigger_user_vif1, start from ru_allocation field */
#define     VIF1_BASIC_TRIG_USER_INFO_RX_BASIC_TRIGGER_USER_VIF1_MASK       0xffffffff
#define     VIF1_BASIC_TRIG_USER_INFO_RX_BASIC_TRIGGER_USER_VIF1_SHIFT               0

#define REG_VIF1_BASIC_TRIG_PSDU_LEN                                             0x2A4
/* rx_basic_trigger_hetb_psdu_len_vif1 */
#define     VIF1_BASIC_TRIG_PSDU_LEN_RX_BASIC_TRIGGER_HETB_PSDU_LEN_VIF1_MASK   0xfffff
#define     VIF1_BASIC_TRIG_PSDU_LEN_RX_BASIC_TRIGGER_HETB_PSDU_LEN_VIF1_SHIFT         0

#define REG_VIF1_RSVD                                                            0x2FC
/* reserved bit for ECO */
#define     VIF1_RSVD_RESERVED_MASK                                         0xffffffff
#define     VIF1_RSVD_RESERVED_SHIFT                                                 0

#define REG_TX_QUEN_CMD0                                                         0x300
/* TX Start Command for ACI0(AC_BE) queue of VIF0 */
/* EDCA queue priority : 4 > 9 > 3 > 8 > 2 > 7 > 0 > 5 > 1 > 6 */
/* bit 0 : queue enable for EDCA */
/* bit 1 : queue enable for aid matched case to send HE TB response(basic trigger) */
/* bit 2 : queue enable for associated UORA case to send HE TB response */
/* bit 3 : queue enable for unassoicated UORA case to send HE TB response */
/* When bit [3:0] written as 0, */
/* all the commands are cancelled and queue is disabled, including EDCA and Trigger-based UL MU transmission. */
#define     TX_QUEN_CMD0_CMD_EDCA_MASK                                             0xf
#define     TX_QUEN_CMD0_CMD_EDCA_SHIFT                                              0
/* AIFSN value */
#define     TX_QUEN_CMD0_AIFSN_MASK                                              0xf00
#define     TX_QUEN_CMD0_AIFSN_SHIFT                                                 8
/* Backoff count value used for backoff */
/* (set by software, and decremented in every slot boundaries if CCA is idle) */
#define     TX_QUEN_CMD0_BOFF_COUNT_MASK                                     0x3ff0000
#define     TX_QUEN_CMD0_BOFF_COUNT_SHIFT                                           16

#define REG_TX_QUEN_TXOP0                                                        0x304
/* TXOP Limit value specified in EDCA Parameter Set element in unit of 1 us */
#define     TX_QUEN_TXOP0_CMD_MASK                                            0x1fffff
#define     TX_QUEN_TXOP0_CMD_SHIFT                                                  0
/* cf_end send in signaling TA. */
#define     TX_QUEN_TXOP0_CFE_BS_TA_MASK                                     0x1000000
#define     TX_QUEN_TXOP0_CFE_BS_TA_SHIFT                                           24
/* tx priority for this queue */
/* used for bt coex priority arb */
#define     TX_QUEN_TXOP0_TX_PRI_MASK                                       0xf0000000
#define     TX_QUEN_TXOP0_TX_PRI_SHIFT                                              28

#define REG_TX_QUEN_STATE0                                                       0x308
/* TX queue state (0: idle, 1: wait, 2: backoff, 3: in transmission) */
#define     TX_QUEN_STATE0_TX_QUEUE_STATE_MASK                                     0x3
#define     TX_QUEN_STATE0_TX_QUEUE_STATE_SHIFT                                      0

#define REG_TX_QUEN_TIMER0                                                       0x30C
/* TXOP timer value remained in unit of 1 us. */
/* TXOP timer is set to TXOP limit value when queue start transmission and decreased by one for every 1us. */
/* If queue is not in transmission, this value is read as 0. */
#define     TX_QUEN_TIMER0_TXOP_TIMER_MASK                                    0x1fffff
#define     TX_QUEN_TIMER0_TXOP_TIMER_SHIFT                                          0

#define REG_TX_QUEN_CMD1                                                         0x310
/* TX Start Command for ACI1(AC_BK) queue of VIF0 */
/* EDCA queue priority : 4 > 9 > 3 > 8 > 2 > 7 > 0 > 5 > 1 > 6 */
/* bit 0 : queue enable for EDCA */
/* bit 1 : queue enable for aid matched case to send HE TB response(basic trigger) */
/* bit 2 : queue enable for associated UORA case to send HE TB response */
/* bit 3 : queue enable for unassoicated UORA case to send HE TB response */
/* When bit [3:0] written as 0, */
/* all the commands are cancelled and queue is disabled, including EDCA and Trigger-based UL MU transmission. */
#define     TX_QUEN_CMD1_CMD_EDCA_MASK                                             0xf
#define     TX_QUEN_CMD1_CMD_EDCA_SHIFT                                              0
/* AIFSN value */
#define     TX_QUEN_CMD1_AIFSN_MASK                                              0xf00
#define     TX_QUEN_CMD1_AIFSN_SHIFT                                                 8
/* Backoff count value used for backoff */
/* (set by software, and decremented in every slot boundaries if CCA is idle) */
#define     TX_QUEN_CMD1_BOFF_COUNT_MASK                                     0x3ff0000
#define     TX_QUEN_CMD1_BOFF_COUNT_SHIFT                                           16

#define REG_TX_QUEN_TXOP1                                                        0x314
/* TXOP Limit value specified in EDCA Parameter Set element in unit of 1 us */
#define     TX_QUEN_TXOP1_CMD_MASK                                            0x1fffff
#define     TX_QUEN_TXOP1_CMD_SHIFT                                                  0
/* cf_end send in signaling TA. */
#define     TX_QUEN_TXOP1_CFE_BS_TA_MASK                                     0x1000000
#define     TX_QUEN_TXOP1_CFE_BS_TA_SHIFT                                           24
/* tx priority for this queue */
/* used for bt coex priority arb */
#define     TX_QUEN_TXOP1_TX_PRI_MASK                                       0xf0000000
#define     TX_QUEN_TXOP1_TX_PRI_SHIFT                                              28

#define REG_TX_QUEN_STATE1                                                       0x318
/* TX queue state (0: idle, 1: wait, 2: backoff, 3: in transmission) */
#define     TX_QUEN_STATE1_TX_QUEUE_STATE_MASK                                     0x3
#define     TX_QUEN_STATE1_TX_QUEUE_STATE_SHIFT                                      0

#define REG_TX_QUEN_TIMER1                                                       0x31C
/* TXOP timer value remained in unit of 1 us. */
/* TXOP timer is set to TXOP limit value when queue start transmission and decreased by one for every 1us. */
/* If queue is not in transmission, this value is read as 0. */
#define     TX_QUEN_TIMER1_TXOP_TIMER_MASK                                    0x1fffff
#define     TX_QUEN_TIMER1_TXOP_TIMER_SHIFT                                          0

#define REG_TX_QUEN_CMD2                                                         0x320
/* TX Start Command for ACI2(AC_VI) queue of VIF0 */
/* EDCA queue priority : 4 > 9 > 3 > 8 > 2 > 7 > 0 > 5 > 1 > 6 */
/* bit 0 : queue enable for EDCA */
/* bit 1 : queue enable for aid matched case to send HE TB response(basic trigger) */
/* bit 2 : queue enable for associated UORA case to send HE TB response */
/* bit 3 : queue enable for unassoicated UORA case to send HE TB response */
/* When bit [3:0] written as 0, */
/* all the commands are cancelled and queue is disabled, including EDCA and Trigger-based UL MU transmission. */
#define     TX_QUEN_CMD2_CMD_EDCA_MASK                                             0xf
#define     TX_QUEN_CMD2_CMD_EDCA_SHIFT                                              0
/* AIFSN value */
#define     TX_QUEN_CMD2_AIFSN_MASK                                              0xf00
#define     TX_QUEN_CMD2_AIFSN_SHIFT                                                 8
/* Backoff count value used for backoff */
/* (set by software, and decremented in every slot boundaries if CCA is idle) */
#define     TX_QUEN_CMD2_BOFF_COUNT_MASK                                     0x3ff0000
#define     TX_QUEN_CMD2_BOFF_COUNT_SHIFT                                           16

#define REG_TX_QUEN_TXOP2                                                        0x324
/* TXOP Limit value specified in EDCA Parameter Set element in unit of 1 us */
#define     TX_QUEN_TXOP2_CMD_MASK                                            0x1fffff
#define     TX_QUEN_TXOP2_CMD_SHIFT                                                  0
/* cf_end send in signaling TA. */
#define     TX_QUEN_TXOP2_CFE_BS_TA_MASK                                     0x1000000
#define     TX_QUEN_TXOP2_CFE_BS_TA_SHIFT                                           24
/* tx priority for this queue */
/* used for bt coex priority arb */
#define     TX_QUEN_TXOP2_TX_PRI_MASK                                       0xf0000000
#define     TX_QUEN_TXOP2_TX_PRI_SHIFT                                              28

#define REG_TX_QUEN_STATE2                                                       0x328
/* TX queue state (0: idle, 1: wait, 2: backoff, 3: in transmission) */
#define     TX_QUEN_STATE2_TX_QUEUE_STATE_MASK                                     0x3
#define     TX_QUEN_STATE2_TX_QUEUE_STATE_SHIFT                                      0

#define REG_TX_QUEN_TIMER2                                                       0x32C
/* TXOP timer value remained in unit of 1 us. */
/* TXOP timer is set to TXOP limit value when queue start transmission and decreased by one for every 1us. */
/* If queue is not in transmission, this value is read as 0. */
#define     TX_QUEN_TIMER2_TXOP_TIMER_MASK                                    0x1fffff
#define     TX_QUEN_TIMER2_TXOP_TIMER_SHIFT                                          0

#define REG_TX_QUEN_CMD3                                                         0x330
/* TX Start Command for ACI3(AC_VO) queue of VIF0 */
/* EDCA queue priority : 4 > 9 > 3 > 8 > 2 > 7 > 0 > 5 > 1 > 6 */
/* bit 0 : queue enable for EDCA */
/* bit 1 : queue enable for aid matched case to send HE TB response(basic trigger) */
/* bit 2 : queue enable for associated UORA case to send HE TB response */
/* bit 3 : queue enable for unassoicated UORA case to send HE TB response */
/* When bit [3:0] written as 0, */
/* all the commands are cancelled and queue is disabled, including EDCA and Trigger-based UL MU transmission. */
#define     TX_QUEN_CMD3_CMD_EDCA_MASK                                             0xf
#define     TX_QUEN_CMD3_CMD_EDCA_SHIFT                                              0
/* AIFSN value */
#define     TX_QUEN_CMD3_AIFSN_MASK                                              0xf00
#define     TX_QUEN_CMD3_AIFSN_SHIFT                                                 8
/* Backoff count value used for backoff */
/* (set by software, and decremented in every slot boundaries if CCA is idle) */
#define     TX_QUEN_CMD3_BOFF_COUNT_MASK                                     0x3ff0000
#define     TX_QUEN_CMD3_BOFF_COUNT_SHIFT                                           16

#define REG_TX_QUEN_TXOP3                                                        0x334
/* TXOP Limit value specified in EDCA Parameter Set element in unit of 1 us */
#define     TX_QUEN_TXOP3_CMD_MASK                                            0x1fffff
#define     TX_QUEN_TXOP3_CMD_SHIFT                                                  0
/* cf_end send in signaling TA. */
#define     TX_QUEN_TXOP3_CFE_BS_TA_MASK                                     0x1000000
#define     TX_QUEN_TXOP3_CFE_BS_TA_SHIFT                                           24
/* tx priority for this queue */
/* used for bt coex priority arb */
#define     TX_QUEN_TXOP3_TX_PRI_MASK                                       0xf0000000
#define     TX_QUEN_TXOP3_TX_PRI_SHIFT                                              28

#define REG_TX_QUEN_STATE3                                                       0x338
/* TX queue state (0: idle, 1: wait, 2: backoff, 3: in transmission) */
#define     TX_QUEN_STATE3_TX_QUEUE_STATE_MASK                                     0x3
#define     TX_QUEN_STATE3_TX_QUEUE_STATE_SHIFT                                      0

#define REG_TX_QUEN_TIMER3                                                       0x33C
/* TXOP timer value remained in unit of 1 us. */
/* TXOP timer is set to TXOP limit value when queue start transmission and decreased by one for every 1us. */
/* If queue is not in transmission, this value is read as 0. */
#define     TX_QUEN_TIMER3_TXOP_TIMER_MASK                                    0x1fffff
#define     TX_QUEN_TIMER3_TXOP_TIMER_SHIFT                                          0

#define REG_TX_QUEN_CMD4                                                         0x340
/* TX Start Command for highest priority queue of VIF0 */
/* EDCA queue priority : 4 > 9 > 3 > 8 > 2 > 7 > 0 > 5 > 1 > 6 */
/* bit 0 : queue enable for EDCA */
/* bit 1 : queue enable for aid matched case to send HE TB response(basic trigger) */
/* bit 2 : queue enable for associated UORA case to send HE TB response */
/* bit 3 : queue enable for unassoicated UORA case to send HE TB response */
/* When bit [3:0] written as 0, */
/* all the commands are cancelled and queue is disabled, including EDCA and Trigger-based UL MU transmission. */
#define     TX_QUEN_CMD4_CMD_EDCA_MASK                                             0xf
#define     TX_QUEN_CMD4_CMD_EDCA_SHIFT                                              0
/* AIFSN value */
#define     TX_QUEN_CMD4_AIFSN_MASK                                              0xf00
#define     TX_QUEN_CMD4_AIFSN_SHIFT                                                 8
/* Backoff count value used for backoff */
/* (set by software, and decremented in every slot boundaries if CCA is idle) */
#define     TX_QUEN_CMD4_BOFF_COUNT_MASK                                     0x3ff0000
#define     TX_QUEN_CMD4_BOFF_COUNT_SHIFT                                           16

#define REG_TX_QUEN_TXOP4                                                        0x344
/* TXOP Limit value specified in EDCA Parameter Set element in unit of 1 us */
#define     TX_QUEN_TXOP4_CMD_MASK                                            0x1fffff
#define     TX_QUEN_TXOP4_CMD_SHIFT                                                  0
/* cf_end send in signaling TA. */
#define     TX_QUEN_TXOP4_CFE_BS_TA_MASK                                     0x1000000
#define     TX_QUEN_TXOP4_CFE_BS_TA_SHIFT                                           24
/* tx priority for this queue */
/* used for bt coex priority arb */
#define     TX_QUEN_TXOP4_TX_PRI_MASK                                       0xf0000000
#define     TX_QUEN_TXOP4_TX_PRI_SHIFT                                              28

#define REG_TX_QUEN_STATE4                                                       0x348
/* TX queue state (0: idle, 1: wait, 2: backoff, 3: in transmission) */
#define     TX_QUEN_STATE4_TX_QUEUE_STATE_MASK                                     0x3
#define     TX_QUEN_STATE4_TX_QUEUE_STATE_SHIFT                                      0

#define REG_TX_QUEN_TIMER4                                                       0x34C
/* TXOP timer value remained in unit of 1 us. */
/* TXOP timer is set to TXOP limit value when queue start transmission and decreased by one for every 1us. */
/* If queue is not in transmission, this value is read as 0. */
#define     TX_QUEN_TIMER4_TXOP_TIMER_MASK                                    0x1fffff
#define     TX_QUEN_TIMER4_TXOP_TIMER_SHIFT                                          0

#define REG_TX_QUEN_CMD5                                                         0x350
/* TX Start Command for ACI0(AC_BE) queue of VIF1 */
/* EDCA queue priority : 4 > 9 > 3 > 8 > 2 > 7 > 0 > 5 > 1 > 6 */
/* bit 0 : queue enable for EDCA */
/* bit 1 : queue enable for aid matched case to send HE TB response(basic trigger) */
/* bit 2 : queue enable for associated UORA case to send HE TB response */
/* bit 3 : queue enable for unassoicated UORA case to send HE TB response */
/* When bit [3:0] written as 0, */
/* all the commands are cancelled and queue is disabled, including EDCA and Trigger-based UL MU transmission. */
#define     TX_QUEN_CMD5_CMD_EDCA_MASK                                             0xf
#define     TX_QUEN_CMD5_CMD_EDCA_SHIFT                                              0
/* AIFSN value */
#define     TX_QUEN_CMD5_AIFSN_MASK                                              0xf00
#define     TX_QUEN_CMD5_AIFSN_SHIFT                                                 8
/* Backoff count value used for backoff */
/* (set by software, and decremented in every slot boundaries if CCA is idle) */
#define     TX_QUEN_CMD5_BOFF_COUNT_MASK                                     0x3ff0000
#define     TX_QUEN_CMD5_BOFF_COUNT_SHIFT                                           16

#define REG_TX_QUEN_TXOP5                                                        0x354
/* TXOP Limit value specified in EDCA Parameter Set element in unit of 1 us */
#define     TX_QUEN_TXOP5_CMD_MASK                                            0x1fffff
#define     TX_QUEN_TXOP5_CMD_SHIFT                                                  0
/* cf_end send in signaling TA. */
#define     TX_QUEN_TXOP5_CFE_BS_TA_MASK                                     0x1000000
#define     TX_QUEN_TXOP5_CFE_BS_TA_SHIFT                                           24
/* tx priority for this queue */
/* used for bt coex priority arb */
#define     TX_QUEN_TXOP5_TX_PRI_MASK                                       0xf0000000
#define     TX_QUEN_TXOP5_TX_PRI_SHIFT                                              28

#define REG_TX_QUEN_STATE5                                                       0x358
/* TX queue state (0: idle, 1: wait, 2: backoff, 3: in transmission) */
#define     TX_QUEN_STATE5_TX_QUEUE_STATE_MASK                                     0x3
#define     TX_QUEN_STATE5_TX_QUEUE_STATE_SHIFT                                      0

#define REG_TX_QUEN_TIMER5                                                       0x35C
/* TXOP timer value remained in unit of 1 us. */
/* TXOP timer is set to TXOP limit value when queue start transmission and decreased by one for every 1us. */
/* If queue is not in transmission, this value is read as 0. */
#define     TX_QUEN_TIMER5_TXOP_TIMER_MASK                                    0x1fffff
#define     TX_QUEN_TIMER5_TXOP_TIMER_SHIFT                                          0

#define REG_TX_QUEN_CMD6                                                         0x360
/* TX Start Command for ACI1(AC_BK) queue of VIF1 */
/* EDCA queue priority : 4 > 9 > 3 > 8 > 2 > 7 > 0 > 5 > 1 > 6 */
/* bit 0 : queue enable for EDCA */
/* bit 1 : queue enable for aid matched case to send HE TB response(basic trigger) */
/* bit 2 : queue enable for associated UORA case to send HE TB response */
/* bit 3 : queue enable for unassoicated UORA case to send HE TB response */
/* When bit [3:0] written as 0, */
/* all the commands are cancelled and queue is disabled, including EDCA and Trigger-based UL MU transmission. */
#define     TX_QUEN_CMD6_CMD_EDCA_MASK                                             0xf
#define     TX_QUEN_CMD6_CMD_EDCA_SHIFT                                              0
/* AIFSN value */
#define     TX_QUEN_CMD6_AIFSN_MASK                                              0xf00
#define     TX_QUEN_CMD6_AIFSN_SHIFT                                                 8
/* Backoff count value used for backoff */
/* (set by software, and decremented in every slot boundaries if CCA is idle) */
#define     TX_QUEN_CMD6_BOFF_COUNT_MASK                                     0x3ff0000
#define     TX_QUEN_CMD6_BOFF_COUNT_SHIFT                                           16

#define REG_TX_QUEN_TXOP6                                                        0x364
/* TXOP Limit value specified in EDCA Parameter Set element in unit of 1 us */
#define     TX_QUEN_TXOP6_CMD_MASK                                            0x1fffff
#define     TX_QUEN_TXOP6_CMD_SHIFT                                                  0
/* cf_end send in signaling TA. */
#define     TX_QUEN_TXOP6_CFE_BS_TA_MASK                                     0x1000000
#define     TX_QUEN_TXOP6_CFE_BS_TA_SHIFT                                           24
/* tx priority for this queue */
/* used for bt coex priority arb */
#define     TX_QUEN_TXOP6_TX_PRI_MASK                                       0xf0000000
#define     TX_QUEN_TXOP6_TX_PRI_SHIFT                                              28

#define REG_TX_QUEN_STATE6                                                       0x368
/* TX queue state (0: idle, 1: wait, 2: backoff, 3: in transmission) */
#define     TX_QUEN_STATE6_TX_QUEUE_STATE_MASK                                     0x3
#define     TX_QUEN_STATE6_TX_QUEUE_STATE_SHIFT                                      0

#define REG_TX_QUEN_TIMER6                                                       0x36C
/* TXOP timer value remained in unit of 1 us. */
/* TXOP timer is set to TXOP limit value when queue start transmission and decreased by one for every 1us. */
/* If queue is not in transmission, this value is read as 0. */
#define     TX_QUEN_TIMER6_TXOP_TIMER_MASK                                    0x1fffff
#define     TX_QUEN_TIMER6_TXOP_TIMER_SHIFT                                          0

#define REG_TX_QUEN_CMD7                                                         0x370
/* TX Start Command for ACI2(AC_VI) queue of VIF1 */
/* EDCA queue priority : 4 > 9 > 3 > 8 > 2 > 7 > 0 > 5 > 1 > 6 */
/* bit 0 : queue enable for EDCA */
/* bit 1 : queue enable for aid matched case to send HE TB response(basic trigger) */
/* bit 2 : queue enable for associated UORA case to send HE TB response */
/* bit 3 : queue enable for unassoicated UORA case to send HE TB response */
/* When bit [3:0] written as 0, */
/* all the commands are cancelled and queue is disabled, including EDCA and Trigger-based UL MU transmission. */
#define     TX_QUEN_CMD7_CMD_EDCA_MASK                                             0xf
#define     TX_QUEN_CMD7_CMD_EDCA_SHIFT                                              0
/* AIFSN value */
#define     TX_QUEN_CMD7_AIFSN_MASK                                              0xf00
#define     TX_QUEN_CMD7_AIFSN_SHIFT                                                 8
/* Backoff count value used for backoff */
/* (set by software, and decremented in every slot boundaries if CCA is idle) */
#define     TX_QUEN_CMD7_BOFF_COUNT_MASK                                     0x3ff0000
#define     TX_QUEN_CMD7_BOFF_COUNT_SHIFT                                           16

#define REG_TX_QUEN_TXOP7                                                        0x374
/* TXOP Limit value specified in EDCA Parameter Set element in unit of 1 us */
#define     TX_QUEN_TXOP7_CMD_MASK                                            0x1fffff
#define     TX_QUEN_TXOP7_CMD_SHIFT                                                  0
/* cf_end send in signaling TA. */
#define     TX_QUEN_TXOP7_CFE_BS_TA_MASK                                     0x1000000
#define     TX_QUEN_TXOP7_CFE_BS_TA_SHIFT                                           24
/* tx priority for this queue */
/* used for bt coex priority arb */
#define     TX_QUEN_TXOP7_TX_PRI_MASK                                       0xf0000000
#define     TX_QUEN_TXOP7_TX_PRI_SHIFT                                              28

#define REG_TX_QUEN_STATE7                                                       0x378
/* TX queue state (0: idle, 1: wait, 2: backoff, 3: in transmission) */
#define     TX_QUEN_STATE7_TX_QUEUE_STATE_MASK                                     0x3
#define     TX_QUEN_STATE7_TX_QUEUE_STATE_SHIFT                                      0

#define REG_TX_QUEN_TIMER7                                                       0x37C
/* TXOP timer value remained in unit of 1 us. */
/* TXOP timer is set to TXOP limit value when queue start transmission and decreased by one for every 1us. */
/* If queue is not in transmission, this value is read as 0. */
#define     TX_QUEN_TIMER7_TXOP_TIMER_MASK                                    0x1fffff
#define     TX_QUEN_TIMER7_TXOP_TIMER_SHIFT                                          0

#define REG_TX_QUEN_CMD8                                                         0x380
/* TX Start Command for ACI2(AC_VO) queue of VIF1 */
/* EDCA queue priority : 4 > 9 > 3 > 8 > 2 > 7 > 0 > 5 > 1 > 6 */
/* bit 0 : queue enable for EDCA */
/* bit 1 : queue enable for aid matched case to send HE TB response(basic trigger) */
/* bit 2 : queue enable for associated UORA case to send HE TB response */
/* bit 3 : queue enable for unassoicated UORA case to send HE TB response */
/* When bit [3:0] written as 0, */
/* all the commands are cancelled and queue is disabled, including EDCA and Trigger-based UL MU transmission. */
#define     TX_QUEN_CMD8_CMD_EDCA_MASK                                             0xf
#define     TX_QUEN_CMD8_CMD_EDCA_SHIFT                                              0
/* AIFSN value */
#define     TX_QUEN_CMD8_AIFSN_MASK                                              0xf00
#define     TX_QUEN_CMD8_AIFSN_SHIFT                                                 8
/* Backoff count value used for backoff */
/* (set by software, and decremented in every slot boundaries if CCA is idle) */
#define     TX_QUEN_CMD8_BOFF_COUNT_MASK                                     0x3ff0000
#define     TX_QUEN_CMD8_BOFF_COUNT_SHIFT                                           16

#define REG_TX_QUEN_TXOP8                                                        0x384
/* TXOP Limit value specified in EDCA Parameter Set element in unit of 1 us */
#define     TX_QUEN_TXOP8_CMD_MASK                                            0x1fffff
#define     TX_QUEN_TXOP8_CMD_SHIFT                                                  0
/* cf_end send in signaling TA. */
#define     TX_QUEN_TXOP8_CFE_BS_TA_MASK                                     0x1000000
#define     TX_QUEN_TXOP8_CFE_BS_TA_SHIFT                                           24
/* tx priority for this queue */
/* used for bt coex priority arb */
#define     TX_QUEN_TXOP8_TX_PRI_MASK                                       0xf0000000
#define     TX_QUEN_TXOP8_TX_PRI_SHIFT                                              28

#define REG_TX_QUEN_STATE8                                                       0x388
/* TX queue state (0: idle, 1: wait, 2: backoff, 3: in transmission) */
#define     TX_QUEN_STATE8_TX_QUEUE_STATE_MASK                                     0x3
#define     TX_QUEN_STATE8_TX_QUEUE_STATE_SHIFT                                      0

#define REG_TX_QUEN_TIMER8                                                       0x38C
/* TXOP timer value remained in unit of 1 us. */
/* TXOP timer is set to TXOP limit value when queue start transmission and decreased by one for every 1us. */
/* If queue is not in transmission, this value is read as 0. */
#define     TX_QUEN_TIMER8_TXOP_TIMER_MASK                                    0x1fffff
#define     TX_QUEN_TIMER8_TXOP_TIMER_SHIFT                                          0

#define REG_TX_QUEN_CMD9                                                         0x390
/* TX Start Command for highest priority queue of VIF1 */
/* EDCA queue priority : 4 > 9 > 3 > 8 > 2 > 7 > 0 > 5 > 1 > 6 */
/* bit 0 : queue enable for EDCA */
/* bit 1 : queue enable for aid matched case to send HE TB response(basic trigger) */
/* bit 2 : queue enable for associated UORA case to send HE TB response */
/* bit 3 : queue enable for unassoicated UORA case to send HE TB response */
/* When bit [3:0] written as 0, */
/* all the commands are cancelled and queue is disabled, including EDCA and Trigger-based UL MU transmission. */
#define     TX_QUEN_CMD9_CMD_EDCA_MASK                                             0xf
#define     TX_QUEN_CMD9_CMD_EDCA_SHIFT                                              0
/* AIFSN value */
#define     TX_QUEN_CMD9_AIFSN_MASK                                              0xf00
#define     TX_QUEN_CMD9_AIFSN_SHIFT                                                 8
/* Backoff count value used for backoff */
/* (set by software, and decremented in every slot boundaries if CCA is idle) */
#define     TX_QUEN_CMD9_BOFF_COUNT_MASK                                     0x3ff0000
#define     TX_QUEN_CMD9_BOFF_COUNT_SHIFT                                           16

#define REG_TX_QUEN_TXOP9                                                        0x394
/* TXOP Limit value specified in EDCA Parameter Set element in unit of 1 us */
#define     TX_QUEN_TXOP9_CMD_MASK                                            0x1fffff
#define     TX_QUEN_TXOP9_CMD_SHIFT                                                  0
/* cf_end send in signaling TA. */
#define     TX_QUEN_TXOP9_CFE_BS_TA_MASK                                     0x1000000
#define     TX_QUEN_TXOP9_CFE_BS_TA_SHIFT                                           24
/* tx priority for this queue */
/* used for bt coex priority arb */
#define     TX_QUEN_TXOP9_TX_PRI_MASK                                       0xf0000000
#define     TX_QUEN_TXOP9_TX_PRI_SHIFT                                              28

#define REG_TX_QUEN_STATE9                                                       0x398
/* TX queue state (0: idle, 1: wait, 2: backoff, 3: in transmission) */
#define     TX_QUEN_STATE9_TX_QUEUE_STATE_MASK                                     0x3
#define     TX_QUEN_STATE9_TX_QUEUE_STATE_SHIFT                                      0

#define REG_TX_QUEN_TIMER9                                                       0x39C
/* TXOP timer value remained in unit of 1 us. */
/* TXOP timer is set to TXOP limit value when queue start transmission and decreased by one for every 1us. */
/* If queue is not in transmission, this value is read as 0. */
#define     TX_QUEN_TIMER9_TXOP_TIMER_MASK                                    0x1fffff
#define     TX_QUEN_TIMER9_TXOP_TIMER_SHIFT                                          0

#define REG_TX_TXTIME_CMD                                                        0x3A0
/* TXTIME calculation start command */
/* After calculation is done, this bit will be automatically cleared */
#define     TX_TXTIME_CMD_CMD_MASK                                                 0x1
#define     TX_TXTIME_CMD_CMD_SHIFT                                                  0

#define REG_TX_TXTIME_RESULT                                                     0x3A4
/* Result value of TXTIME calculation in unit of 0.1us */
#define     TX_TXTIME_RESULT_TXTIME_MASK                                       0xfffff
#define     TX_TXTIME_RESULT_TXTIME_SHIFT                                            0
/* This bit is set when an error occurred during TXTIME calculation */
#define     TX_TXTIME_RESULT_ERR_MASK                                        0x1000000
#define     TX_TXTIME_RESULT_ERR_SHIFT                                              24

#define REG_TX_TXTIME_TV0                                                        0x3A8
/* word 0 of TXVECTOR used for TXTIME calculation */
#define     TX_TXTIME_TV0_TXVECTOR0_MASK                                    0xffffffff
#define     TX_TXTIME_TV0_TXVECTOR0_SHIFT                                            0

#define REG_TX_TXTIME_TV1                                                        0x3AC
/* word 1 of TXVECTOR used for TXTIME calculation */
#define     TX_TXTIME_TV1_TXVECTOR1_MASK                                    0xffffffff
#define     TX_TXTIME_TV1_TXVECTOR1_SHIFT                                            0

#define REG_TX_TXTIME_TV2                                                        0x3B0
/* word 2 of TXVECTOR used for TXTIME calculation */
#define     TX_TXTIME_TV2_TXVECTOR2_MASK                                    0xffffffff
#define     TX_TXTIME_TV2_TXVECTOR2_SHIFT                                            0

#define REG_TX_TXTIME_TV3                                                        0x3B4
/* word 3 of TXVECTOR used for TXTIME calculation */
#define     TX_TXTIME_TV3_TXVECTOR3_MASK                                    0xffffffff
#define     TX_TXTIME_TV3_TXVECTOR3_SHIFT                                            0

#define REG_TX_TXTIME_TV4                                                        0x3B8
/* word 4 of TXVECTOR used for TXTIME calculation */
#define     TX_TXTIME_TV4_TXVECTOR4_MASK                                    0xffffffff
#define     TX_TXTIME_TV4_TXVECTOR4_SHIFT                                            0

#define REG_TX_TXTIME_TV5                                                        0x3BC
/* word 5 of TXVECTOR used for TXTIME calculation */
#define     TX_TXTIME_TV5_TXVECTOR5_MASK                                    0xffffffff
#define     TX_TXTIME_TV5_TXVECTOR5_SHIFT                                            0

#define REG_TX_TXTIME_TV6                                                        0x3C0
/* word 6 of TXVECTOR used for TXTIME calculation */
#define     TX_TXTIME_TV6_TXVECTOR6_MASK                                    0xffffffff
#define     TX_TXTIME_TV6_TXVECTOR6_SHIFT                                            0

#define REG_TX_TO_CFG                                                            0x3D0
/* TX timeout enable */
#define     TX_TO_CFG_TO_EN_MASK                                                   0x1
#define     TX_TO_CFG_TO_EN_SHIFT                                                    0
/* TX timeout value in us unit */
#define     TX_TO_CFG_TO_VALUE_MASK                                         0x1fff0000
#define     TX_TO_CFG_TO_VALUE_SHIFT                                                16

#define REG_TX_TO_11B_CFG                                                        0x3D4
/* TX timeout value for 11B frame in us unit */
#define     TX_TO_11B_CFG_TO_11B_VALUE_MASK                                     0xffff
#define     TX_TO_11B_CFG_TO_11B_VALUE_SHIFT                                         0

#define REG_TX_CFG                                                               0x3E0
/* 0 : Prepare TX 1 slot before contention win */
/* 1 : Prepare TX 2 slots before contention win */
#define     TX_CFG_EARLY_PREP_MASK                                                 0x1
#define     TX_CFG_EARLY_PREP_SHIFT                                                  0

#define REG_TX_HETB_QNULL                                                        0x3E4
/* rx basic trigger first AC select(from 2'b00 ~ 2'b11) */
/* this register need to use with */
#define     TX_HETB_QNULL_RX_BASIC_TRIGGER_FIRST_AC_VIF0_MASK                      0x3
#define     TX_HETB_QNULL_RX_BASIC_TRIGGER_FIRST_AC_VIF0_SHIFT                       0
/* rx basic trigger mpdu_mu_spacing_factor */
#define     TX_HETB_QNULL_RX_BASIC_TRIGGER_MU_SPACING_FACTOR_VIF0_MASK             0xc
#define     TX_HETB_QNULL_RX_BASIC_TRIGGER_MU_SPACING_FACTOR_VIF0_SHIFT              2
/* rx basic trigger queue final select(que0 to que9) */
/* some times prefered_ac in basic trigger doesn't have pkt to send */
/* hw chooses to send mpdu from other queue but psdu length doesn't enough */
#define     TX_HETB_QNULL_RX_BASIC_TRIGGER_QUE_SEL_VIF0_MASK                      0xf0
#define     TX_HETB_QNULL_RX_BASIC_TRIGGER_QUE_SEL_VIF0_SHIFT                        4
/* rx basic trigger tid limit */
#define     TX_HETB_QNULL_RX_BASIC_TRIGGER_TID_LIMIT_VIF0_MASK                   0x700
#define     TX_HETB_QNULL_RX_BASIC_TRIGGER_TID_LIMIT_VIF0_SHIFT                      8
/* rx basic trigger first AC select(from 2'b00 ~ 2'b11) */
/* this register need to use with */
#define     TX_HETB_QNULL_RX_BASIC_TRIGGER_FIRST_AC_VIF1_MASK                  0x30000
#define     TX_HETB_QNULL_RX_BASIC_TRIGGER_FIRST_AC_VIF1_SHIFT                      16
/* rx basic trigger mpdu_mu_spacing_factor */
#define     TX_HETB_QNULL_RX_BASIC_TRIGGER_MU_SPACING_FACTOR_VIF1_MASK         0xc0000
#define     TX_HETB_QNULL_RX_BASIC_TRIGGER_MU_SPACING_FACTOR_VIF1_SHIFT             18
/* rx basic trigger queue final select(que0 to que9) */
/* some times prefered_ac in basic trigger doesn't have pkt to send */
/* hw chooses to send mpdu from other queue but psdu length doesn't enough */
#define     TX_HETB_QNULL_RX_BASIC_TRIGGER_QUE_SEL_VIF1_MASK                  0xf00000
#define     TX_HETB_QNULL_RX_BASIC_TRIGGER_QUE_SEL_VIF1_SHIFT                       20
/* rx basic trigger tid limit */
#define     TX_HETB_QNULL_RX_BASIC_TRIGGER_TID_LIMIT_VIF1_MASK               0x7000000
#define     TX_HETB_QNULL_RX_BASIC_TRIGGER_TID_LIMIT_VIF1_SHIFT                     24

#define REG_RX_CFG                                                               0x400
/* Promiscuous Mode */
/* RX DMA transfers MPDUs to S/W regardless of their RA/BSSID and integrity of received MPDUs */
#define     RX_CFG_PRO_MASK                                                        0x1
#define     RX_CFG_PRO_SHIFT                                                         0

#define REG_RX_BUF_CFG                                                           0x404
/* Enable RX DMA, */
/* 0 : disable RX DMA operation immediately, 1 : enable RX DMA operation */
/* When this field is enabled, */
/* REG_RX_BUF_PTR1.write_ptr is automatically set to zero */
#define     RX_BUF_CFG_EN_MASK                                                     0x1
#define     RX_BUF_CFG_EN_SHIFT                                                      0
/* Number of RX buffer as a power of 2 */
/* [1] 2, [2] 4, [3] 8, [4] 16, [5] 32, [6] 64, [7] 128, [8] 256 */
#define     RX_BUF_CFG_BUF_NUM_MASK                                              0xf00
#define     RX_BUF_CFG_BUF_NUM_SHIFT                                                 8

#define REG_RX_BUF_PTR0                                                          0x408
/* Ready pointer */
/* RX buffer position where S/W will make new RX Buffer ready */
#define     RX_BUF_PTR0_READY_PTR_MASK                                           0x1ff
#define     RX_BUF_PTR0_READY_PTR_SHIFT                                              0
/* Read pointer */
/* RX buffer position where S/W will read received MPDU */
#define     RX_BUF_PTR0_READ_PTR_MASK                                        0x1ff0000
#define     RX_BUF_PTR0_READ_PTR_SHIFT                                              16

#define REG_RX_BUF_PTR1                                                          0x40C
/* Write pointer */
/* RX buffer position where H/W will write received MPDU */
/* When REG_RX_BUF_CFG.en is enabled, */
/* this field is automatically set to zero */
#define     RX_BUF_PTR1_WRITE_PTR_MASK                                           0x1ff
#define     RX_BUF_PTR1_WRITE_PTR_SHIFT                                              0

#define REG_RX_SCB_CTRL                                                          0x410
/* Scoreboard Reset */
/* Reset RX scoreboard (each bit corresponds to each TID) */
#define     RX_SCB_CTRL_SCB_RESET_MASK                                            0xff
#define     RX_SCB_CTRL_SCB_RESET_SHIFT                                              0
/* Scoreboard Window Size */
/* Window size of Scoreboard - 1 */
#define     RX_SCB_CTRL_SCB_WINDOW_MASK                                         0x3f00
#define     RX_SCB_CTRL_SCB_WINDOW_SHIFT                                             8
/* Update scoreboard even if buffer overrun occurs */
#define     RX_SCB_CTRL_SCB_IGNORE_OVERRUN_MASK                                0x10000
#define     RX_SCB_CTRL_SCB_IGNORE_OVERRUN_SHIFT                                    16

#define REG_RX_TO_CFG                                                            0x414
/* TX timeout value unit in us */
/* if rx_to_value is set to 0, rx timeout is disabled */
/* if rx_to_value reach, would clear rx_busy signal so that tx can continue to contention */
/* rx_to_value doesn't impact rx fsm itself. */
/* rxe fsm can continue when new rx ppdu comes. */
#define     RX_TO_CFG_RX_TO_VALUE_MASK                                          0x3fff
#define     RX_TO_CFG_RX_TO_VALUE_SHIFT                                              0
/* if enable this register, would make rxe_fsm goes to idle if rx_to happens */
/* and generate rx ppdu end intr for fw */
#define     RX_TO_CFG_RX_TO_PPDU_END_EN_MASK                                   0x10000
#define     RX_TO_CFG_RX_TO_PPDU_END_EN_SHIFT                                       16

#define REG_RX_TO_11B_CFG                                                        0x418
/* TX timeout value unit in 1us, max 11b ppdu is about 33ms */
/* if rx_to_11b_value is set to 0, rx timeout is disabled */
/* if rx_to_11b_value reach, */
/* would clear rx_busy signal so that tx can continue to contention */
/* rx_to_value doesn't impact rx fsm itself. */
/* rxe fsm can continue when new rx ppdu comes. */
#define     RX_TO_11B_CFG_RX_TO_11B_VALUE_MASK                                  0xffff
#define     RX_TO_11B_CFG_RX_TO_11B_VALUE_SHIFT                                      0

#define REG_SEC_KEY_CMD                                                          0x500
/* Key Command */
/* 0 : No command */
/* 1 : Add key */
/* 2 : Delete key by tag */
/* 3 : Delete all keys */
/* 4 : Read key by index */
/* 5 : Delete key by index */
/* After command operation is done, H/W automatically sets this value to zero */
#define     SEC_KEY_CMD_KCMD_MASK                                                  0x7
#define     SEC_KEY_CMD_KCMD_SHIFT                                                   0
/* Key ID */
#define     SEC_KEY_CMD_KID_MASK                                                 0x300
#define     SEC_KEY_CMD_KID_SHIFT                                                    8
/* Key Type */
/* 0 : PTK, 1 : GTK */
#define     SEC_KEY_CMD_KTYPE_MASK                                               0x400
#define     SEC_KEY_CMD_KTYPE_SHIFT                                                 10
/* Cipher Type */
/* 0 : WEP-40, 1 : WEP-104, 2 : TKIP, */
/* 4 : CCMP128, 5 : CCMP256, 6 : GCMP128, 7 : GCMP256 */
#define     SEC_KEY_CMD_CTYPE_MASK                                             0x70000
#define     SEC_KEY_CMD_CTYPE_SHIFT                                                 16
/* SPP Enable */
/* Indicate that both the STA and its peer have their SPP A-MSDU Capable fields equal to 1 */
#define     SEC_KEY_CMD_SPP_MASK                                               0x80000
#define     SEC_KEY_CMD_SPP_SHIFT                                                   19
/* Index of key memory used for Read key by index & Delete key by index command */
#define     SEC_KEY_CMD_IDX_MASK                                             0xf000000
#define     SEC_KEY_CMD_IDX_SHIFT                                                   24

#define REG_SEC_KEY_STATUS                                                       0x504
/* Key Valid */
/* Each bit Indicates that each key memory entry has a valid cipher key */
#define     SEC_KEY_STATUS_KEY_VALID_MASK                                       0xffff
#define     SEC_KEY_STATUS_KEY_VALID_SHIFT                                           0
/* Index of key memory where key is added by Add key by tag command */
#define     SEC_KEY_STATUS_ADD_IDX_MASK                                      0xf000000
#define     SEC_KEY_STATUS_ADD_IDX_SHIFT                                            24

#define REG_SEC_MAC_ADDR_L32                                                     0x508
/* MAC Address */
/* peer STA's MAC address [31:0] */
#define     SEC_MAC_ADDR_L32_MAC_ADDR_32_0_MASK                             0xffffffff
#define     SEC_MAC_ADDR_L32_MAC_ADDR_32_0_SHIFT                                     0

#define REG_SEC_MAC_ADDR_H16                                                     0x50C
/* MAC Address */
/* peer STA's MAC address [47:32] */
#define     SEC_MAC_ADDR_H16_MAC_ADDR_47_32_MASK                                0xffff
#define     SEC_MAC_ADDR_H16_MAC_ADDR_47_32_SHIFT                                    0

#define REG_SEC_TEMP_KEY0                                                        0x510
/* Temporal Key[31:0] */
#define     SEC_TEMP_KEY0_TEMP_KEY_31_0_MASK                                0xffffffff
#define     SEC_TEMP_KEY0_TEMP_KEY_31_0_SHIFT                                        0

#define REG_SEC_TEMP_KEY1                                                        0x514
/* Temporal Key[63:32] */
#define     SEC_TEMP_KEY1_TEMP_KEY_63_32_MASK                               0xffffffff
#define     SEC_TEMP_KEY1_TEMP_KEY_63_32_SHIFT                                       0

#define REG_SEC_TEMP_KEY2                                                        0x518
/* Temporal Key[95:64] */
#define     SEC_TEMP_KEY2_TEMP_KEY_95_64_MASK                               0xffffffff
#define     SEC_TEMP_KEY2_TEMP_KEY_95_64_SHIFT                                       0

#define REG_SEC_TEMP_KEY3                                                        0x51C
/* Temporal Key[127:96] */
#define     SEC_TEMP_KEY3_TEMP_KEY_127_96_MASK                              0xffffffff
#define     SEC_TEMP_KEY3_TEMP_KEY_127_96_SHIFT                                      0

#define REG_SEC_TEMP_KEY4                                                        0x520
/* Temporal Key[159:128] for CCMP256/GCMP256 */
/* TX MIC Key [31:0] for TKIP */
#define     SEC_TEMP_KEY4_TEMP_KEY_159_128_MASK                             0xffffffff
#define     SEC_TEMP_KEY4_TEMP_KEY_159_128_SHIFT                                     0

#define REG_SEC_TEMP_KEY5                                                        0x524
/* Temporal Key[191:160] for CCMP256/GCMP256 */
/* TX MIC Key [63:32] for TKIP */
#define     SEC_TEMP_KEY5_TEMP_KEY_191_160_MASK                             0xffffffff
#define     SEC_TEMP_KEY5_TEMP_KEY_191_160_SHIFT                                     0

#define REG_SEC_TEMP_KEY6                                                        0x528
/* Temporal Key[223:192] for CCMP256/GCMP256 */
/* RX MIC Key [31:0] for TKIP */
#define     SEC_TEMP_KEY6_TEMP_KEY_223_192_MASK                             0xffffffff
#define     SEC_TEMP_KEY6_TEMP_KEY_223_192_SHIFT                                     0

#define REG_SEC_TEMP_KEY7                                                        0x52C
/* Temporal Key[255:224] for CCMP256/GCMP256 */
/* RX MIC Key [63:32] for TKIP */
#define     SEC_TEMP_KEY7_TEMP_KEY_255_224_MASK                             0xffffffff
#define     SEC_TEMP_KEY7_TEMP_KEY_255_224_SHIFT                                     0

#define REG_SEC_RSC0                                                             0x530
/* Receive Sequence Count [31:0] */
#define     SEC_RSC0_RSC_31_0_MASK                                          0xffffffff
#define     SEC_RSC0_RSC_31_0_SHIFT                                                  0

#define REG_SEC_RSC1                                                             0x534
/* Receive Sequence Count [63:32] */
#define     SEC_RSC1_RSC_63_32_MASK                                         0xffffffff
#define     SEC_RSC1_RSC_63_32_SHIFT                                                 0

#define REG_SEC_RDATA_TAG0                                                       0x538
/* MAC_ADDR[31:0] */
#define     SEC_RDATA_TAG0_MAC_ADDR_31_0_MASK                               0xffffffff
#define     SEC_RDATA_TAG0_MAC_ADDR_31_0_SHIFT                                       0

#define REG_SEC_RDATA_TAG1                                                       0x53C
/* MAC_ADDR[47:32] */
#define     SEC_RDATA_TAG1_MAC_ADDR_47_32_MASK                                  0xffff
#define     SEC_RDATA_TAG1_MAC_ADDR_47_32_SHIFT                                      0
/* Key ID[1:0] */
#define     SEC_RDATA_TAG1_KID_MASK                                            0x30000
#define     SEC_RDATA_TAG1_KID_SHIFT                                                16
/* Key Type */
#define     SEC_RDATA_TAG1_KTYPE_MASK                                          0x40000
#define     SEC_RDATA_TAG1_KTYPE_SHIFT                                              18

#define REG_SEC_RDATA_INFO0                                                      0x540
/* TEMP_KEY[31:0] */
#define     SEC_RDATA_INFO0_TEMP_KEY_31_0_MASK                              0xffffffff
#define     SEC_RDATA_INFO0_TEMP_KEY_31_0_SHIFT                                      0

#define REG_SEC_RDATA_INFO1                                                      0x544
/* TEMP_KEY[63:32] */
#define     SEC_RDATA_INFO1_TEMP_KEY_63_32_MASK                             0xffffffff
#define     SEC_RDATA_INFO1_TEMP_KEY_63_32_SHIFT                                     0

#define REG_SEC_RDATA_INFO2                                                      0x548
/* TEMP_KEY[95:64] */
#define     SEC_RDATA_INFO2_TEMP_KEY_95_64_MASK                             0xffffffff
#define     SEC_RDATA_INFO2_TEMP_KEY_95_64_SHIFT                                     0

#define REG_SEC_RDATA_INFO3                                                      0x54C
/* TEMP_KEY[127:96] */
#define     SEC_RDATA_INFO3_TEMP_KEY_127_96_MASK                            0xffffffff
#define     SEC_RDATA_INFO3_TEMP_KEY_127_96_SHIFT                                    0

#define REG_SEC_RDATA_INFO4                                                      0x550
/* TEMP_KEY[159:128] */
#define     SEC_RDATA_INFO4_TEMP_KEY_159_128_MASK                           0xffffffff
#define     SEC_RDATA_INFO4_TEMP_KEY_159_128_SHIFT                                   0

#define REG_SEC_RDATA_INFO5                                                      0x554
/* TEMP_KEY[191:160] */
#define     SEC_RDATA_INFO5_TEMP_KEY_191_160_MASK                           0xffffffff
#define     SEC_RDATA_INFO5_TEMP_KEY_191_160_SHIFT                                   0

#define REG_SEC_RDATA_INFO6                                                      0x558
/* TEMP_KEY[223:192] */
#define     SEC_RDATA_INFO6_TEMP_KEY_223_192_MASK                           0xffffffff
#define     SEC_RDATA_INFO6_TEMP_KEY_223_192_SHIFT                                   0

#define REG_SEC_RDATA_INFO7                                                      0x55C
/* TEMP_KEY[255:224] */
#define     SEC_RDATA_INFO7_TEMP_KEY_255_224_MASK                           0xffffffff
#define     SEC_RDATA_INFO7_TEMP_KEY_255_224_SHIFT                                   0

#define REG_SEC_RDATA_INFO8                                                      0x560
/* RSC[31:0] */
#define     SEC_RDATA_INFO8_RSC_31_0_MASK                                   0xffffffff
#define     SEC_RDATA_INFO8_RSC_31_0_SHIFT                                           0

#define REG_SEC_RDATA_INFO9                                                      0x564
/* RSC[63:32] */
#define     SEC_RDATA_INFO9_RSC_63_32_MASK                                  0xffffffff
#define     SEC_RDATA_INFO9_RSC_63_32_SHIFT                                          0

#define REG_SEC_RDATA_INFO10                                                     0x568
/* ctype */
#define     SEC_RDATA_INFO10_CTYPE_MASK                                            0x7
#define     SEC_RDATA_INFO10_CTYPE_SHIFT                                             0
/* spp */
#define     SEC_RDATA_INFO10_SPP_MASK                                              0x8
#define     SEC_RDATA_INFO10_SPP_SHIFT                                               3

#define REG_HE_TB_PWR01                                                          0x600
/* min power for mcs0 */
/* he tb min power for mcs0, signed value from -128 to 127, please make sure max - min <= 31 */
#define     HE_TB_PWR01_MCS0_MIN_PWR_MASK                                         0xff
#define     HE_TB_PWR01_MCS0_MIN_PWR_SHIFT                                           0
/* max power for mcs0 */
/* he tb max power for mcs0, signed value from -128 to 127, please make sure max - min <= 31 */
#define     HE_TB_PWR01_MCS0_MAX_PWR_MASK                                       0xff00
#define     HE_TB_PWR01_MCS0_MAX_PWR_SHIFT                                           8
/* min power for mcs1 */
/* he tb min power for mcs1, signed value from -128 to 127, please make sure max - min <= 31 */
#define     HE_TB_PWR01_MCS1_MIN_PWR_MASK                                     0xff0000
#define     HE_TB_PWR01_MCS1_MIN_PWR_SHIFT                                          16
/* max power for mcs1 */
/* he tb max power for mcs1, signed value from -128 to 127, please make sure max - min <= 31 */
#define     HE_TB_PWR01_MCS1_MAX_PWR_MASK                                   0xff000000
#define     HE_TB_PWR01_MCS1_MAX_PWR_SHIFT                                          24

#define REG_HE_TB_PWR23                                                          0x604
/* min power for mcs2 */
/* he tb min power for mcs2, signed value from -128 to 127, please make sure max - min <= 31 */
#define     HE_TB_PWR23_MCS2_MIN_PWR_MASK                                         0xff
#define     HE_TB_PWR23_MCS2_MIN_PWR_SHIFT                                           0
/* max power for mcs2 */
/* he tb max power for mcs2, signed value from -128 to 127, please make sure max - min <= 31 */
#define     HE_TB_PWR23_MCS2_MAX_PWR_MASK                                       0xff00
#define     HE_TB_PWR23_MCS2_MAX_PWR_SHIFT                                           8
/* min power for mcs3 */
/* he tb min power for mcs3, signed value from -128 to 127, please make sure max - min <= 31 */
#define     HE_TB_PWR23_MCS3_MIN_PWR_MASK                                     0xff0000
#define     HE_TB_PWR23_MCS3_MIN_PWR_SHIFT                                          16
/* max power for mcs3 */
/* he tb max power for mcs3, signed value from -128 to 127, please make sure max - min <= 31 */
#define     HE_TB_PWR23_MCS3_MAX_PWR_MASK                                   0xff000000
#define     HE_TB_PWR23_MCS3_MAX_PWR_SHIFT                                          24

#define REG_HE_TB_PWR45                                                          0x608
/* min power for mcs4 */
/* he tb min power for mcs4, signed value from -128 to 127, please make sure max - min <= 31 */
#define     HE_TB_PWR45_MCS4_MIN_PWR_MASK                                         0xff
#define     HE_TB_PWR45_MCS4_MIN_PWR_SHIFT                                           0
/* max power for mcs4 */
/* he tb max power for mcs4, signed value from -128 to 127, please make sure max - min <= 31 */
#define     HE_TB_PWR45_MCS4_MAX_PWR_MASK                                       0xff00
#define     HE_TB_PWR45_MCS4_MAX_PWR_SHIFT                                           8
/* min power for mcs5 */
/* he tb min power for mcs5, signed value from -128 to 127, please make sure max - min <= 31 */
#define     HE_TB_PWR45_MCS5_MIN_PWR_MASK                                     0xff0000
#define     HE_TB_PWR45_MCS5_MIN_PWR_SHIFT                                          16
/* max power for mcs5 */
/* he tb max power for mcs5, signed value from -128 to 127, please make sure max - min <= 31 */
#define     HE_TB_PWR45_MCS5_MAX_PWR_MASK                                   0xff000000
#define     HE_TB_PWR45_MCS5_MAX_PWR_SHIFT                                          24

#define REG_HE_TB_PWR67                                                          0x60C
/* min power for mcs6 */
/* he tb min power for mcs6, signed value from -128 to 127, please make sure max - min <= 31 */
#define     HE_TB_PWR67_MCS6_MIN_PWR_MASK                                         0xff
#define     HE_TB_PWR67_MCS6_MIN_PWR_SHIFT                                           0
/* max power for mcs6 */
/* he tb max power for mcs6, signed value from -128 to 127, please make sure max - min <= 31 */
#define     HE_TB_PWR67_MCS6_MAX_PWR_MASK                                       0xff00
#define     HE_TB_PWR67_MCS6_MAX_PWR_SHIFT                                           8
/* min power for mcs7 */
/* he tb min power for mcs7, signed value from -128 to 127, please make sure max - min <= 31 */
#define     HE_TB_PWR67_MCS7_MIN_PWR_MASK                                     0xff0000
#define     HE_TB_PWR67_MCS7_MIN_PWR_SHIFT                                          16
/* max power for mcs7 */
/* he tb max power for mcs7, signed value from -128 to 127, please make sure max - min <= 31 */
#define     HE_TB_PWR67_MCS7_MAX_PWR_MASK                                   0xff000000
#define     HE_TB_PWR67_MCS7_MAX_PWR_SHIFT                                          24

#define REG_HE_TB_PWR89                                                          0x610
/* min power for mcs8 */
/* he tb min power for mcs8, signed value from -128 to 127, please make sure max - min <= 31 */
#define     HE_TB_PWR89_MCS8_MIN_PWR_MASK                                         0xff
#define     HE_TB_PWR89_MCS8_MIN_PWR_SHIFT                                           0
/* max power for mcs8 */
/* he tb max power for mcs8, signed value from -128 to 127, please make sure max - min <= 31 */
#define     HE_TB_PWR89_MCS8_MAX_PWR_MASK                                       0xff00
#define     HE_TB_PWR89_MCS8_MAX_PWR_SHIFT                                           8
/* min power for mcs9 */
/* he tb min power for mcs9, signed value from -128 to 127, please make sure max - min <= 31 */
#define     HE_TB_PWR89_MCS9_MIN_PWR_MASK                                     0xff0000
#define     HE_TB_PWR89_MCS9_MIN_PWR_SHIFT                                          16
/* max power for mcs9 */
/* he tb max power for mcs9, signed value from -128 to 127, please make sure max - min <= 31 */
#define     HE_TB_PWR89_MCS9_MAX_PWR_MASK                                   0xff000000
#define     HE_TB_PWR89_MCS9_MAX_PWR_SHIFT                                          24

#define REG_HE_TB_PWR1011                                                        0x614
/* min power for mcs10 */
/* he tb min power for mcs10, signed value from -128 to 127, please make sure max - min <= 31 */
#define     HE_TB_PWR1011_MCS10_MIN_PWR_MASK                                      0xff
#define     HE_TB_PWR1011_MCS10_MIN_PWR_SHIFT                                        0
/* max power for mcs10 */
/* he tb max power for mcs10, signed value from -128 to 127, please make sure max - min <= 31 */
#define     HE_TB_PWR1011_MCS10_MAX_PWR_MASK                                    0xff00
#define     HE_TB_PWR1011_MCS10_MAX_PWR_SHIFT                                        8
/* min power for mcs11 */
/* he tb min power for mcs11, signed value from -128 to 127, please make sure max - min <= 31 */
#define     HE_TB_PWR1011_MCS11_MIN_PWR_MASK                                  0xff0000
#define     HE_TB_PWR1011_MCS11_MIN_PWR_SHIFT                                       16
/* max power for mcs11 */
/* he tb max power for mcs11, signed value from -128 to 127, please make sure max - min <= 31 */
#define     HE_TB_PWR1011_MCS11_MAX_PWR_MASK                                0xff000000
#define     HE_TB_PWR1011_MCS11_MAX_PWR_SHIFT                                       24

#define REG_OBSS_PD_TX_PWR_OPT                                                   0x618
/* CCA IDLE slot count for TX PWR limit txop clear */
/* a queue gets contention through the OBSS PD, and starts the TX PWR limit txop. */
/* If in the time it gets contention,  and idle slot cnt >= cca_idle_slot_cnt, */
/* would pkt would send out without tx pwr limitation. */
/* a value != 0 would enable this feature */
#define     OBSS_PD_TX_PWR_OPT_CCA_IDLE_SLOT_CNT_MASK                             0xff
#define     OBSS_PD_TX_PWR_OPT_CCA_IDLE_SLOT_CNT_SHIFT                               0

#define REG_PHYRX_DELAY                                                          0x61C
/* phyrx_delay_value */
#define     PHYRX_DELAY_PHYRX_DELAY_VALUE_MASK                                  0xffff
#define     PHYRX_DELAY_PHYRX_DELAY_VALUE_SHIFT                                      0
/* WN slot time for obss ppdu protocal back trace */
#define     PHYRX_DELAY_WN_SLOT_TIME_MASK                                     0xff0000
#define     PHYRX_DELAY_WN_SLOT_TIME_SHIFT                                          16
/* hetb ppdu send timeout monitor */
/* hetb ppdu send timeout value: if hetb ppdu command sent but in the duraiton of timeout there is no sifs, */
/* would cancel the hetb ppdu sending */
/* (hetb ppdu sending miss the sifs time), default time is 20us */
#define     PHYRX_DELAY_HETB_SEND_TM_MASK                                   0xff000000
#define     PHYRX_DELAY_HETB_SEND_TM_SHIFT                                          24

#define REG_AID_CMD                                                              0x620
/* AID Command */
/* 0 : No command */
/* 1 : Add AID (using REG_AID_MAC_ADDR0/1 and REG_AID_MAC_AID register) */
/* 2 : Delete AID (using REG_AID_MAC_ADDR0/1 and REG_AID_MAC_AID register) */
/* 3 : Delete all AIDs */
/* 4 : Read AID by tag_idx, the read out value is latched in REG_AID_INFO0/1 registers */
/* After command operation is done, H/W automatically sets this value to zero */
/* REG_AID_* related registers are only used for AP mode */
#define     AID_CMD_AID_CMD_MASK                                                   0x7
#define     AID_CMD_AID_CMD_SHIFT                                                    0
/* set this bit to 1 would monitor rx aid search status in aid info0/1 register */
#define     AID_CMD_RX_AID_MONITOR_MASK                                            0x8
#define     AID_CMD_RX_AID_MONITOR_SHIFT                                             3
/* Tag Index used for Read AID command */
#define     AID_CMD_TAG_IDX_MASK                                             0x3000000
#define     AID_CMD_TAG_IDX_SHIFT                                                   24

#define REG_AID_STATUS                                                           0x624
/* AID Valid */
/* Each bit Indicates that each AID entry has a valid AID (read only) */
#define     AID_STATUS_AID_VALID_MASK                                              0xf
#define     AID_STATUS_AID_VALID_SHIFT                                               0

#define REG_AID_MAC_ADDR0                                                        0x628
/* MAC_ADDR[31:0] */
#define     AID_MAC_ADDR0_MAC_ADDR_31_0_MASK                                0xffffffff
#define     AID_MAC_ADDR0_MAC_ADDR_31_0_SHIFT                                        0

#define REG_AID_MAC_ADDR1                                                        0x62C
/* MAC_ADDR[47:32] */
#define     AID_MAC_ADDR1_MAC_ADDR_47_32_MASK                                   0xffff
#define     AID_MAC_ADDR1_MAC_ADDR_47_32_SHIFT                                       0

#define REG_AID_MAC_AID                                                          0x630
/* peer STA's AID */
#define     AID_MAC_AID_AID_MASK                                                 0xfff
#define     AID_MAC_AID_AID_SHIFT                                                    0

#define REG_AID_INFO0                                                            0x634
/* MAC_ADDR[31:0] */
#define     AID_INFO0_MAC_ADDR_31_0_MASK                                    0xffffffff
#define     AID_INFO0_MAC_ADDR_31_0_SHIFT                                            0

#define REG_AID_INFO1                                                            0x638
/* MAC_ADDR[47:32] */
#define     AID_INFO1_MAC_ADDR_47_32_MASK                                       0xffff
#define     AID_INFO1_MAC_ADDR_47_32_SHIFT                                           0
/* peer MAC's aid */
#define     AID_INFO1_MAC_AID_MASK                                           0xfff0000
#define     AID_INFO1_MAC_AID_SHIFT                                                 16

#define REG_OBSS_SR_NON_SRG_CFG                                                  0x63C
/* obss pd threshold for non_SRG */
#define     OBSS_SR_NON_SRG_CFG_NON_SRG_THR_MASK                                  0xff
#define     OBSS_SR_NON_SRG_CFG_NON_SRG_THR_SHIFT                                    0
/* obss pd threshold for non_SRG in case of HE ER ppdu */
#define     OBSS_SR_NON_SRG_CFG_NON_SRG_ER_THR_MASK                             0xff00
#define     OBSS_SR_NON_SRG_CFG_NON_SRG_ER_THR_SHIFT                                 8
/* obss pd sr enable for non_SRG */
#define     OBSS_SR_NON_SRG_CFG_NON_SRG_OBSS_PD_EN_MASK                        0x10000
#define     OBSS_SR_NON_SRG_CFG_NON_SRG_OBSS_PD_EN_SHIFT                            16
/* obss pd sr enabled for ppdu doesn't contain bssid in case of non_SRG */
#define     OBSS_SR_NON_SRG_CFG_NON_SRG_NOBSSID_EN_MASK                        0x20000
#define     OBSS_SR_NON_SRG_CFG_NON_SRG_NOBSSID_EN_SHIFT                            17
/* the ap in this BSS disables non-SRG obss pd(this bit is both for our device is STA and AP) */
/* this bit is related to Non-SRG OBSS PD SR Disallowed subfield equal to 1 */
#define     OBSS_SR_NON_SRG_CFG_NON_SRG_AP_DIS_MASK                            0x40000
#define     OBSS_SR_NON_SRG_CFG_NON_SRG_AP_DIS_SHIFT                                18
/* if our sta tx HE PPDU with PSR_AND_NON_SRG_OBSS_PD_PROHIBITED in tx vector spatial_reuse field, */
/* sw will do regsiter shift in each BEACON time liking OBSS_PD_TX_SRP_NON_SRG_DIS[1:0] <= {OBSS_PD_TX_SRP_NON_SRG_DIS[0],1'b0}. */
/* when hw sends out out HE ppdu with PSR_AND_NON_SRG_OBSS_PD_PROHIBITED in spatial_reuse field, hw would auto set OBSS_PD_TX_SRP_NON_SRG_DIS[0] to 1'b1. */
#define     OBSS_SR_NON_SRG_CFG_NON_SRG_TX_SRP_NON_SRG_DIS_MASK              0x3000000
#define     OBSS_SR_NON_SRG_CFG_NON_SRG_TX_SRP_NON_SRG_DIS_SHIFT                    24

#define REG_OBSS_SR_NON_SRG_COLOR_EN0                                            0x640
/* non-srg obss sr color enable bitmap low 32 bit */
#define     OBSS_SR_NON_SRG_COLOR_EN0_COLOR_EN_L_MASK                       0xffffffff
#define     OBSS_SR_NON_SRG_COLOR_EN0_COLOR_EN_L_SHIFT                               0

#define REG_OBSS_SR_NON_SRG_COLOR_EN1                                            0x644
/* non-srg obss sr color enable bitmap high 32 bit */
#define     OBSS_SR_NON_SRG_COLOR_EN1_COLOR_EN_H_MASK                       0xffffffff
#define     OBSS_SR_NON_SRG_COLOR_EN1_COLOR_EN_H_SHIFT                               0

#define REG_OBSS_SR_NON_SRG_BSSID_EN0                                            0x648
/* non-srg obss sr bssid enable bitmap low 32 bit */
#define     OBSS_SR_NON_SRG_BSSID_EN0_BSSID_EN_L_MASK                       0xffffffff
#define     OBSS_SR_NON_SRG_BSSID_EN0_BSSID_EN_L_SHIFT                               0

#define REG_OBSS_SR_NON_SRG_BSSID_EN1                                            0x64C
/* non-srg obss sr bssid enable bitmap high 32 bit */
#define     OBSS_SR_NON_SRG_BSSID_EN1_BSSID_EN_H_MASK                       0xffffffff
#define     OBSS_SR_NON_SRG_BSSID_EN1_BSSID_EN_H_SHIFT                               0

#define REG_SRG_CFG                                                              0x650
/* srg obss pd threshold */
#define     SRG_CFG_SRG_THR_MASK                                                  0xff
#define     SRG_CFG_SRG_THR_SHIFT                                                    0
/* srg obss pd threshold in case of HE ER ppdu */
#define     SRG_CFG_SRG_ER_THR_MASK                                             0xff00
#define     SRG_CFG_SRG_ER_THR_SHIFT                                                 8
/* SRG obsspd sr enable */
#define     SRG_CFG_SRG_EN_MASK                                                0x10000
#define     SRG_CFG_SRG_EN_SHIFT                                                    16

#define REG_SRG_MAP_COLOR0                                                       0x654
/* SRG ppdu identify based on color bitmap low 32 bit */
#define     SRG_MAP_COLOR0_COLOR_MAP_L_MASK                                 0xffffffff
#define     SRG_MAP_COLOR0_COLOR_MAP_L_SHIFT                                         0

#define REG_SRG_MAP_COLOR1                                                       0x658
/* SRG ppdu identify based on color bitmap high 32 bit */
#define     SRG_MAP_COLOR1_COLOR_MAP_H_MASK                                 0xffffffff
#define     SRG_MAP_COLOR1_COLOR_MAP_H_SHIFT                                         0

#define REG_SRG_MAP_BSSID0                                                       0x65C
/* SRG ppdu identify based on bssid bitmap low 32 bit */
#define     SRG_MAP_BSSID0_BSSID_MAP_L_MASK                                 0xffffffff
#define     SRG_MAP_BSSID0_BSSID_MAP_L_SHIFT                                         0

#define REG_SRG_MAP_BSSID1                                                       0x660
/* SRG ppdu identify based on bssid bitmap high 32 bit */
#define     SRG_MAP_BSSID1_BSSID_MAP_H_MASK                                 0xffffffff
#define     SRG_MAP_BSSID1_BSSID_MAP_H_SHIFT                                         0

#define REG_SRG_SR_COLOR_EN0                                                     0x664
/* srg obss sr color enable bitmap low 32 bit */
#define     SRG_SR_COLOR_EN0_COLOR_EN_L_MASK                                0xffffffff
#define     SRG_SR_COLOR_EN0_COLOR_EN_L_SHIFT                                        0

#define REG_SRG_SR_COLOR_EN1                                                     0x668
/* srg obss sr color enable bitmap high 32 bit */
#define     SRG_SR_COLOR_EN1_COLOR_EN_H_MASK                                0xffffffff
#define     SRG_SR_COLOR_EN1_COLOR_EN_H_SHIFT                                        0

#define REG_SRG_SR_BSSID_EN0                                                     0x66C
/* srg obss sr bssid enable bitmap low 32 bit */
#define     SRG_SR_BSSID_EN0_BSSID_EN_L_MASK                                0xffffffff
#define     SRG_SR_BSSID_EN0_BSSID_EN_L_SHIFT                                        0

#define REG_SRG_SR_BSSID_EN1                                                     0x670
/* srg obss sr bssid enable bitmap high 32 bit */
#define     SRG_SR_BSSID_EN1_BSSID_EN_H_MASK                                0xffffffff
#define     SRG_SR_BSSID_EN1_BSSID_EN_H_SHIFT                                        0

#define REG_SR_CFG                                                               0x674
/* if get rx vector and SR condition match, enable SR function */
#define     SR_CFG_SR_PLCP_HEADER_EN_MASK                                          0x1
#define     SR_CFG_SR_PLCP_HEADER_EN_SHIFT                                           0
/* if get fcs passed mpdu and SR condition match, enable SR function */
#define     SR_CFG_SR_MPDU_EN_MASK                                                 0x2
#define     SR_CFG_SR_MPDU_EN_SHIFT                                                  1

#define REG_CTS_TX_PWR                                                           0x678
/* TX power index for CTS in response of MU-RTS */
#define     CTS_TX_PWR_CTS_TX_PWR_MASK                                            0xff
#define     CTS_TX_PWR_CTS_TX_PWR_SHIFT                                              0

#define REG_DBG_TX_SIFS_RESP_CNT                                                 0x6D8
/* tx ppdu in sifs with control response */
#define     DBG_TX_SIFS_RESP_CNT_TX_SIFS_CTRL_RESP_CNT_MASK                     0xffff
#define     DBG_TX_SIFS_RESP_CNT_TX_SIFS_CTRL_RESP_CNT_SHIFT                         0
/* tx ppdu in sifs with hetb ppdu */
#define     DBG_TX_SIFS_RESP_CNT_TX_SIFS_HETB_CNT_MASK                      0xffff0000
#define     DBG_TX_SIFS_RESP_CNT_TX_SIFS_HETB_CNT_SHIFT                             16

#define REG_DBG_SIG                                                              0x6DC
/* mac debug signal */
#define     DBG_SIG_MAC_DBG_SIG_MASK                                        0xffffffff
#define     DBG_SIG_MAC_DBG_SIG_SHIFT                                                0

#define REG_DBG_MPDU_FLOW                                                        0x6E0
/* mpdu_flow_pass */
#define     DBG_MPDU_FLOW_MPDU_FLOW_PASS_MASK                                   0xffff
#define     DBG_MPDU_FLOW_MPDU_FLOW_PASS_SHIFT                                       0
/* mpdu_flow_fail */
#define     DBG_MPDU_FLOW_MPDU_FLOW_FAIL_MASK                               0xffff0000
#define     DBG_MPDU_FLOW_MPDU_FLOW_FAIL_SHIFT                                      16

#define REG_DBG_PPDU_TYPE0                                                       0x6E4
/* rcvd_agg_mpdu_cnt */
#define     DBG_PPDU_TYPE0_RCVD_AGG_MPDU_CNT_MASK                               0xffff
#define     DBG_PPDU_TYPE0_RCVD_AGG_MPDU_CNT_SHIFT                                   0
/* rcvd_eof1_mpdu_cnt */
#define     DBG_PPDU_TYPE0_RCVD_EOF1_MPDU_CNT_MASK                          0xffff0000
#define     DBG_PPDU_TYPE0_RCVD_EOF1_MPDU_CNT_SHIFT                                 16

#define REG_DBG_PPDU_TYPE1                                                       0x6E8
/* rcvd_nonagg_mpdu_cnt */
#define     DBG_PPDU_TYPE1_RCVD_NONAGG_MPDU_CNT_MASK                            0xffff
#define     DBG_PPDU_TYPE1_RCVD_NONAGG_MPDU_CNT_SHIFT                                0
/* trigger_cca_fail_cnt */
#define     DBG_PPDU_TYPE1_TRIGGER_CCA_FAIL_CNT_MASK                          0xff0000
#define     DBG_PPDU_TYPE1_TRIGGER_CCA_FAIL_CNT_SHIFT                               16
/* trigger_nav_fail_cnt */
#define     DBG_PPDU_TYPE1_TRIGGER_NAV_FAIL_CNT_MASK                        0xff000000
#define     DBG_PPDU_TYPE1_TRIGGER_NAV_FAIL_CNT_SHIFT                               24

#define REG_DBG_PPDU_TYPE2                                                       0x6EC
/* hetb_sifs_lost_cnt */
#define     DBG_PPDU_TYPE2_HETB_SIFS_LOST_CNT_MASK                                0xff
#define     DBG_PPDU_TYPE2_HETB_SIFS_LOST_CNT_SHIFT                                  0
/* tim_sifs_lost_cnt */
#define     DBG_PPDU_TYPE2_TIM_SIFS_LOST_CNT_MASK                                0xf00
#define     DBG_PPDU_TYPE2_TIM_SIFS_LOST_CNT_SHIFT                                   8
/* tim_win_lost_cnt */
#define     DBG_PPDU_TYPE2_TIM_WIN_LOST_CNT_MASK                                0xf000
#define     DBG_PPDU_TYPE2_TIM_WIN_LOST_CNT_SHIFT                                   12
/* tx_ppdu_trunc_cnt */
#define     DBG_PPDU_TYPE2_TX_PPDU_TRUNC_CNT_MASK                             0xff0000
#define     DBG_PPDU_TYPE2_TX_PPDU_TRUNC_CNT_SHIFT                                  16
/* counter is increased when sifs lost happens but response frame is sent */
#define     DBG_PPDU_TYPE2_TX_SIFS_LOST_SEND_CNT_MASK                       0xff000000
#define     DBG_PPDU_TYPE2_TX_SIFS_LOST_SEND_CNT_SHIFT                              24

#define REG_OPT_RSVD                                                             0x6F8
/* reserved bit for eco */
#define     OPT_RSVD_RESERVED_MASK                                          0xffffffff
#define     OPT_RSVD_RESERVED_SHIFT                                                  0

#define REG_OPT_RSVD1                                                            0x6FC
/* reserved bit for eco */
#define     OPT_RSVD1_RESERVED1_MASK                                        0xffffffff
#define     OPT_RSVD1_RESERVED1_SHIFT                                                0

#define REG_DBG_SIM_CFG_OPT                                                      0x700
/* response timeout disable */
/* this is only for simulation purpose */
#define     DBG_SIM_CFG_OPT_RTO_DIS_MASK                                           0x1
#define     DBG_SIM_CFG_OPT_RTO_DIS_SHIFT                                            0
/* write 1 to clear the debug counter */
#define     DBG_SIM_CFG_OPT_DBG_CNT_CLR_MASK                                     0x100
#define     DBG_SIM_CFG_OPT_DBG_CNT_CLR_SHIFT                                        8
/* mac debug signal selection */
#define     DBG_SIM_CFG_OPT_MAC_DBG_SIG_SEL_MASK                              0xff0000
#define     DBG_SIM_CFG_OPT_MAC_DBG_SIG_SEL_SHIFT                                   16
/* mac debug signal internal bus selection */
/* 3'b000 : select [ 7: 0] */
/* 3'b001 : select [15: 8] */
/* 3'b010 : select [23:16] */
/* 3'b011 : select [31:24] */
/* 3'b100 : select [39:32] */
/* 3'b101 : select [47:40] */
/* 3'b110 : select [55:48] */
/* 3'b111 : select [63:56] */
#define     DBG_SIM_CFG_OPT_MAC_DBG_BUS_DATA_SEL_MASK                        0x7000000
#define     DBG_SIM_CFG_OPT_MAC_DBG_BUS_DATA_SEL_SHIFT                              24
/* ppdu debug counter don't take account of non_ht ppdu */
#define     DBG_SIM_CFG_OPT_IGNORE_NON_HT_PPDU_RECEIVING_MASK               0x80000000
#define     DBG_SIM_CFG_OPT_IGNORE_NON_HT_PPDU_RECEIVING_SHIFT                      31

#define REG_DBG_RXE_STATE                                                        0x704
/* rxe_bag fsm state */
#define     DBG_RXE_STATE_RXE_BAG_STATE_MASK                                      0x1f
#define     DBG_RXE_STATE_RXE_BAG_STATE_SHIFT                                        0
/* rxe_buf_bd fsm state */
#define     DBG_RXE_STATE_RXE_BUF_BD_STATE_MASK                                   0xe0
#define     DBG_RXE_STATE_RXE_BUF_BD_STATE_SHIFT                                     5
/* rxe_buf fsm state */
#define     DBG_RXE_STATE_RXE_BUF_STATE_MASK                                     0xf00
#define     DBG_RXE_STATE_RXE_BUF_STATE_SHIFT                                        8
/* rxe_frm fsm state */
#define     DBG_RXE_STATE_RXE_FRM_STATE_MASK                                   0x1f000
#define     DBG_RXE_STATE_RXE_FRM_STATE_SHIFT                                       12
/* rxe_fsm fsm state */
#define     DBG_RXE_STATE_RXE_FSM_STATE_MASK                                  0x3e0000
#define     DBG_RXE_STATE_RXE_FSM_STATE_SHIFT                                       17
/* rxe_hetb_vec fsm state */
#define     DBG_RXE_STATE_RXE_HETB_VEC_STATE_MASK                             0x400000
#define     DBG_RXE_STATE_RXE_HETB_VEC_STATE_SHIFT                                  22
/* rxe_tfm fsm state */
#define     DBG_RXE_STATE_RXE_TFM_STATE_MASK                                 0x7800000
#define     DBG_RXE_STATE_RXE_TFM_STATE_SHIFT                                       23
/* rx busy in air */
#define     DBG_RXE_STATE_RX_BUSY_MASK                                       0x8000000
#define     DBG_RXE_STATE_RX_BUSY_SHIFT                                             27

#define REG_DBG_RXE_TID_UPD_STATE                                                0x708
/* rxe_tid7 bitmap update state */
#define     DBG_RXE_TID_UPD_STATE_RXE_TID7_BITMAP_STATE_MASK                       0xf
#define     DBG_RXE_TID_UPD_STATE_RXE_TID7_BITMAP_STATE_SHIFT                        0
/* rxe_tid6 bitmap update state */
#define     DBG_RXE_TID_UPD_STATE_RXE_TID6_BITMAP_STATE_MASK                      0xf0
#define     DBG_RXE_TID_UPD_STATE_RXE_TID6_BITMAP_STATE_SHIFT                        4
/* rxe_tid5 bitmap update state */
#define     DBG_RXE_TID_UPD_STATE_RXE_TID5_BITMAP_STATE_MASK                     0xf00
#define     DBG_RXE_TID_UPD_STATE_RXE_TID5_BITMAP_STATE_SHIFT                        8
/* rxe_tid4 bitmap update state */
#define     DBG_RXE_TID_UPD_STATE_RXE_TID4_BITMAP_STATE_MASK                    0xf000
#define     DBG_RXE_TID_UPD_STATE_RXE_TID4_BITMAP_STATE_SHIFT                       12
/* rxe_tid3 bitmap update state */
#define     DBG_RXE_TID_UPD_STATE_RXE_TID3_BITMAP_STATE_MASK                   0xf0000
#define     DBG_RXE_TID_UPD_STATE_RXE_TID3_BITMAP_STATE_SHIFT                       16
/* rxe_tid2 bitmap update state */
#define     DBG_RXE_TID_UPD_STATE_RXE_TID2_BITMAP_STATE_MASK                  0xf00000
#define     DBG_RXE_TID_UPD_STATE_RXE_TID2_BITMAP_STATE_SHIFT                       20
/* rxe_tid1 bitmap update state */
#define     DBG_RXE_TID_UPD_STATE_RXE_TID1_BITMAP_STATE_MASK                 0xf000000
#define     DBG_RXE_TID_UPD_STATE_RXE_TID1_BITMAP_STATE_SHIFT                       24
/* rxe_tid0 bitmap update state */
#define     DBG_RXE_TID_UPD_STATE_RXE_TID0_BITMAP_STATE_MASK                0xf0000000
#define     DBG_RXE_TID_UPD_STATE_RXE_TID0_BITMAP_STATE_SHIFT                       28

#define REG_DBG_TXE_STATE                                                        0x70C
/* txe_bfm fsm state */
#define     DBG_TXE_STATE_TXE_BFM_STATE_MASK                                       0xf
#define     DBG_TXE_STATE_TXE_BFM_STATE_SHIFT                                        0
/* txe buf id */
#define     DBG_TXE_STATE_TXE_BUF_ID_MASK                                         0x70
#define     DBG_TXE_STATE_TXE_BUF_ID_SHIFT                                           4
/* txe_frm fsm state */
#define     DBG_TXE_STATE_TXE_FRM_STATE_MASK                                     0xf80
#define     DBG_TXE_STATE_TXE_FRM_STATE_SHIFT                                        7
/* txe hetb state */
#define     DBG_TXE_STATE_TXE_HETB_STATE_MASK                                   0xf000
#define     DBG_TXE_STATE_TXE_HETB_STATE_SHIFT                                      12
/* txe main fsm state */
#define     DBG_TXE_STATE_TXE_FSM_STATE_MASK                                  0x1f0000
#define     DBG_TXE_STATE_TXE_FSM_STATE_SHIFT                                       16
/* txe mtb fsm state */
#define     DBG_TXE_STATE_TXE_MTB_STATE_MASK                                  0x600000
#define     DBG_TXE_STATE_TXE_MTB_STATE_SHIFT                                       21
/* txe ndp state */
#define     DBG_TXE_STATE_TXE_NDP_STATE_MASK                                 0x1800000
#define     DBG_TXE_STATE_TXE_NDP_STATE_SHIFT                                       23
/* txe busy */
#define     DBG_TXE_STATE_TX_BUSY_MASK                                       0x2000000
#define     DBG_TXE_STATE_TX_BUSY_SHIFT                                             25
/* bfm counter */
#define     DBG_TXE_STATE_BFM_COUNTER_MASK                                  0xfc000000
#define     DBG_TXE_STATE_BFM_COUNTER_SHIFT                                         26

#define REG_DBG_DMA_STATE                                                        0x710
/* dma bfi fsm state */
#define     DBG_DMA_STATE_DMA_BFI_STATE_MASK                                       0x7
#define     DBG_DMA_STATE_DMA_BFI_STATE_SHIFT                                        0
/* dma write bus fsm state */
#define     DBG_DMA_STATE_DMAW_BUS_STATE_MASK                                     0x38
#define     DBG_DMA_STATE_DMAW_BUS_STATE_SHIFT                                       3
/* dma read bus fsm state */
#define     DBG_DMA_STATE_DMAR_BUS_STATE_MASK                                    0x1c0
#define     DBG_DMA_STATE_DMAR_BUS_STATE_SHIFT                                       6
/* dma write cmd fsm state */
#define     DBG_DMA_STATE_DMAW_CMD_STATE_MASK                                    0x200
#define     DBG_DMA_STATE_DMAW_CMD_STATE_SHIFT                                       9
/* dma fbo fsm state */
#define     DBG_DMA_STATE_DMA_FBO_STATE_MASK                                     0xc00
#define     DBG_DMA_STATE_DMA_FBO_STATE_SHIFT                                       10
/* dma write fifo fsm state */
#define     DBG_DMA_STATE_DMAW_FIFO_STATE_MASK                                  0x3000
#define     DBG_DMA_STATE_DMAW_FIFO_STATE_SHIFT                                     12
/* dma read fifo fsm state */
#define     DBG_DMA_STATE_DMAR_FIFO_STATE_MASK                                  0xc000
#define     DBG_DMA_STATE_DMAR_FIFO_STATE_SHIFT                                     14
/* tx mpdu with security, but key not found */
#define     DBG_DMA_STATE_TX_SEC_ERR_CNT_MASK                                 0xff0000
#define     DBG_DMA_STATE_TX_SEC_ERR_CNT_SHIFT                                      16
/* tx mpdu with mpdu_length in mt mismatch with real txe processed */
#define     DBG_DMA_STATE_TX_LEN_ERR_CNT_MASK                               0xff000000
#define     DBG_DMA_STATE_TX_LEN_ERR_CNT_SHIFT                                      24

#define REG_DBG_SEC_STATE                                                        0x714
/* ccm_auth_ready */
#define     DBG_SEC_STATE_CCM_AUTH_READY_MASK                                      0x1
#define     DBG_SEC_STATE_CCM_AUTH_READY_SHIFT                                       0
/* end_ready */
#define     DBG_SEC_STATE_ENC_READY_MASK                                           0x2
#define     DBG_SEC_STATE_ENC_READY_SHIFT                                            1
/* gcm_auth_ready */
#define     DBG_SEC_STATE_GCM_AUTH_READY_MASK                                      0x4
#define     DBG_SEC_STATE_GCM_AUTH_READY_SHIFT                                       2
/* sec_ccm fsm state */
#define     DBG_SEC_STATE_SEC_CCM_STATE_MASK                                      0x18
#define     DBG_SEC_STATE_SEC_CCM_STATE_SHIFT                                        3
/* sec_gcm fsm state */
#define     DBG_SEC_STATE_SEC_GCM_STATE_MASK                                      0xe0
#define     DBG_SEC_STATE_SEC_GCM_STATE_SHIFT                                        5
/* sec_key fsm state */
#define     DBG_SEC_STATE_SEC_KEY_STATE_MASK                                     0xf00
#define     DBG_SEC_STATE_SEC_KEY_STATE_SHIFT                                        8
/* sec_skip fsm state */
#define     DBG_SEC_STATE_SEC_TKIP_STATE_MASK                                   0x7000
#define     DBG_SEC_STATE_SEC_TKIP_STATE_SHIFT                                      12
/* sec_wep fsm state */
#define     DBG_SEC_STATE_SEC_WEP_STATE_MASK                                   0x18000
#define     DBG_SEC_STATE_SEC_WEP_STATE_SHIFT                                       15
/* dma_intr_cnt for que write back in tx que release */
#define     DBG_SEC_STATE_DMA_INTR_CNT_MASK                                 0xff000000
#define     DBG_SEC_STATE_DMA_INTR_CNT_SHIFT                                        24

#define REG_DBG_QUE_READ01                                                       0x718
/* mpdus stores in fifo to send for queue0 */
#define     DBG_QUE_READ01_QUE_READ_CNT0_MASK                                   0xffff
#define     DBG_QUE_READ01_QUE_READ_CNT0_SHIFT                                       0
/* mpdus stores in fifo to send for queue2 */
#define     DBG_QUE_READ01_QUE_READ_CNT1_MASK                               0xffff0000
#define     DBG_QUE_READ01_QUE_READ_CNT1_SHIFT                                      16

#define REG_DBG_QUE_READ23                                                       0x71C
/* mpdus stores in fifo to send for queue2 */
#define     DBG_QUE_READ23_QUE_READ_CNT2_MASK                                   0xffff
#define     DBG_QUE_READ23_QUE_READ_CNT2_SHIFT                                       0
/* mpdus stores in fifo to send for queue3 */
#define     DBG_QUE_READ23_QUE_READ_CNT3_MASK                               0xffff0000
#define     DBG_QUE_READ23_QUE_READ_CNT3_SHIFT                                      16

#define REG_DBG_QUE_READ45                                                       0x720
/* mpdus stores in fifo to send for queue4 */
#define     DBG_QUE_READ45_QUE_READ_CNT4_MASK                                   0xffff
#define     DBG_QUE_READ45_QUE_READ_CNT4_SHIFT                                       0
/* mpdus stores in fifo to send for queue5 */
#define     DBG_QUE_READ45_QUE_READ_CNT5_MASK                               0xffff0000
#define     DBG_QUE_READ45_QUE_READ_CNT5_SHIFT                                      16

#define REG_DBG_QUE_READ67                                                       0x724
/* mpdus stores in fifo to send for queue6 */
#define     DBG_QUE_READ67_QUE_READ_CNT6_MASK                                   0xffff
#define     DBG_QUE_READ67_QUE_READ_CNT6_SHIFT                                       0
/* mpdus stores in fifo to send for queue7 */
#define     DBG_QUE_READ67_QUE_READ_CNT7_MASK                               0xffff0000
#define     DBG_QUE_READ67_QUE_READ_CNT7_SHIFT                                      16

#define REG_DBG_QUE_READ89                                                       0x728
/* mpdus stores in fifo to send for queue8 */
#define     DBG_QUE_READ89_QUE_READ_CNT8_MASK                                   0xffff
#define     DBG_QUE_READ89_QUE_READ_CNT8_SHIFT                                       0
/* mpdus stores in fifo to send for queue9 */
#define     DBG_QUE_READ89_QUE_READ_CNT9_MASK                               0xffff0000
#define     DBG_QUE_READ89_QUE_READ_CNT9_SHIFT                                      16

#define REG_DBG_TX_PPDU_CNT01                                                    0x72C
/* ppdu send for queue0 */
#define     DBG_TX_PPDU_CNT01_PPDU_SEND_QUE0_MASK                               0xffff
#define     DBG_TX_PPDU_CNT01_PPDU_SEND_QUE0_SHIFT                                   0
/* ppdu send for queue1 */
#define     DBG_TX_PPDU_CNT01_PPDU_SEND_QUE1_MASK                           0xffff0000
#define     DBG_TX_PPDU_CNT01_PPDU_SEND_QUE1_SHIFT                                  16

#define REG_DBG_TX_PPDU_CNT23                                                    0x730
/* ppdu send for queue2 */
#define     DBG_TX_PPDU_CNT23_PPDU_SEND_QUE2_MASK                               0xffff
#define     DBG_TX_PPDU_CNT23_PPDU_SEND_QUE2_SHIFT                                   0
/* ppdu send for queue3 */
#define     DBG_TX_PPDU_CNT23_PPDU_SEND_QUE3_MASK                           0xffff0000
#define     DBG_TX_PPDU_CNT23_PPDU_SEND_QUE3_SHIFT                                  16

#define REG_DBG_TX_PPDU_CNT45                                                    0x734
/* ppdu send for queue4 */
#define     DBG_TX_PPDU_CNT45_PPDU_SEND_QUE4_MASK                               0xffff
#define     DBG_TX_PPDU_CNT45_PPDU_SEND_QUE4_SHIFT                                   0
/* ppdu send for queue5 */
#define     DBG_TX_PPDU_CNT45_PPDU_SEND_QUE5_MASK                           0xffff0000
#define     DBG_TX_PPDU_CNT45_PPDU_SEND_QUE5_SHIFT                                  16

#define REG_DBG_TX_PPDU_CNT67                                                    0x738
/* ppdu send for queue6 */
#define     DBG_TX_PPDU_CNT67_PPDU_SEND_QUE6_MASK                               0xffff
#define     DBG_TX_PPDU_CNT67_PPDU_SEND_QUE6_SHIFT                                   0
/* ppdu send for queue7 */
#define     DBG_TX_PPDU_CNT67_PPDU_SEND_QUE7_MASK                           0xffff0000
#define     DBG_TX_PPDU_CNT67_PPDU_SEND_QUE7_SHIFT                                  16

#define REG_DBG_TX_PPDU_CNT89                                                    0x73C
/* ppdu send for queue8 */
#define     DBG_TX_PPDU_CNT89_PPDU_SEND_QUE8_MASK                               0xffff
#define     DBG_TX_PPDU_CNT89_PPDU_SEND_QUE8_SHIFT                                   0
/* ppdu send for queue9 */
#define     DBG_TX_PPDU_CNT89_PPDU_SEND_QUE9_MASK                           0xffff0000
#define     DBG_TX_PPDU_CNT89_PPDU_SEND_QUE9_SHIFT                                  16

#define REG_DBG_TX_MPDU_SEC01                                                    0x740
/* mpdu send for queue0 with security */
#define     DBG_TX_MPDU_SEC01_MPDU_SEND_SEC_QUE0_MASK                           0xffff
#define     DBG_TX_MPDU_SEC01_MPDU_SEND_SEC_QUE0_SHIFT                               0
/* mpdu send for queue1 with security */
#define     DBG_TX_MPDU_SEC01_MPDU_SEND_SEC_QUE1_MASK                       0xffff0000
#define     DBG_TX_MPDU_SEC01_MPDU_SEND_SEC_QUE1_SHIFT                              16

#define REG_DBG_TX_MPDU_SEC23                                                    0x744
/* mpdu send for queue2 with security */
#define     DBG_TX_MPDU_SEC23_MPDU_SEND_SEC_QUE2_MASK                           0xffff
#define     DBG_TX_MPDU_SEC23_MPDU_SEND_SEC_QUE2_SHIFT                               0
/* mpdu send for queue3 with security */
#define     DBG_TX_MPDU_SEC23_MPDU_SEND_SEC_QUE3_MASK                       0xffff0000
#define     DBG_TX_MPDU_SEC23_MPDU_SEND_SEC_QUE3_SHIFT                              16

#define REG_DBG_TX_MPDU_SEC45                                                    0x748
/* mpdu send for queue4 with security */
#define     DBG_TX_MPDU_SEC45_MPDU_SEND_SEC_QUE4_MASK                           0xffff
#define     DBG_TX_MPDU_SEC45_MPDU_SEND_SEC_QUE4_SHIFT                               0
/* mpdu send for queue5 with security */
#define     DBG_TX_MPDU_SEC45_MPDU_SEND_SEC_QUE5_MASK                       0xffff0000
#define     DBG_TX_MPDU_SEC45_MPDU_SEND_SEC_QUE5_SHIFT                              16

#define REG_DBG_TX_MPDU_SEC67                                                    0x74C
/* mpdu send for queue6 with security */
#define     DBG_TX_MPDU_SEC67_MPDU_SEND_SEC_QUE6_MASK                           0xffff
#define     DBG_TX_MPDU_SEC67_MPDU_SEND_SEC_QUE6_SHIFT                               0
/* mpdu send for queue7 with security */
#define     DBG_TX_MPDU_SEC67_MPDU_SEND_SEC_QUE7_MASK                       0xffff0000
#define     DBG_TX_MPDU_SEC67_MPDU_SEND_SEC_QUE7_SHIFT                              16

#define REG_DBG_TX_MPDU_SEC89                                                    0x750
/* mpdu send for queue8 with security */
#define     DBG_TX_MPDU_SEC89_MPDU_SEND_SEC_QUE8_MASK                           0xffff
#define     DBG_TX_MPDU_SEC89_MPDU_SEND_SEC_QUE8_SHIFT                               0
/* mpdu send for queue9 with security */
#define     DBG_TX_MPDU_SEC89_MPDU_SEND_SEC_QUE9_MASK                       0xffff0000
#define     DBG_TX_MPDU_SEC89_MPDU_SEND_SEC_QUE9_SHIFT                              16

#define REG_DBG_TX_MPDU_NOSEC01                                                  0x754
/* mpdu send for queue0 without security */
#define     DBG_TX_MPDU_NOSEC01_MPDU_SEND_NOSEC_QUE0_MASK                       0xffff
#define     DBG_TX_MPDU_NOSEC01_MPDU_SEND_NOSEC_QUE0_SHIFT                           0
/* mpdu send for queue1 without security */
#define     DBG_TX_MPDU_NOSEC01_MPDU_SEND_NOSEC_QUE1_MASK                   0xffff0000
#define     DBG_TX_MPDU_NOSEC01_MPDU_SEND_NOSEC_QUE1_SHIFT                          16

#define REG_DBG_TX_MPDU_NOSEC23                                                  0x758
/* mpdu send for queue2 without security */
#define     DBG_TX_MPDU_NOSEC23_MPDU_SEND_NOSEC_QUE2_MASK                       0xffff
#define     DBG_TX_MPDU_NOSEC23_MPDU_SEND_NOSEC_QUE2_SHIFT                           0
/* mpdu send for queue3 without security */
#define     DBG_TX_MPDU_NOSEC23_MPDU_SEND_NOSEC_QUE3_MASK                   0xffff0000
#define     DBG_TX_MPDU_NOSEC23_MPDU_SEND_NOSEC_QUE3_SHIFT                          16

#define REG_DBG_TX_MPDU_NOSEC45                                                  0x75C
/* mpdu send for queue4 without security */
#define     DBG_TX_MPDU_NOSEC45_MPDU_SEND_NOSEC_QUE4_MASK                       0xffff
#define     DBG_TX_MPDU_NOSEC45_MPDU_SEND_NOSEC_QUE4_SHIFT                           0
/* mpdu send for queue5 without security */
#define     DBG_TX_MPDU_NOSEC45_MPDU_SEND_NOSEC_QUE5_MASK                   0xffff0000
#define     DBG_TX_MPDU_NOSEC45_MPDU_SEND_NOSEC_QUE5_SHIFT                          16

#define REG_DBG_TX_MPDU_NOSEC67                                                  0x760
/* mpdu send for queue6 without security */
#define     DBG_TX_MPDU_NOSEC67_MPDU_SEND_NOSEC_QUE6_MASK                       0xffff
#define     DBG_TX_MPDU_NOSEC67_MPDU_SEND_NOSEC_QUE6_SHIFT                           0
/* mpdu send for queue7 without security */
#define     DBG_TX_MPDU_NOSEC67_MPDU_SEND_NOSEC_QUE7_MASK                   0xffff0000
#define     DBG_TX_MPDU_NOSEC67_MPDU_SEND_NOSEC_QUE7_SHIFT                          16

#define REG_DBG_TX_MPDU_NOSEC89                                                  0x764
/* mpdu send for queue8 without security */
#define     DBG_TX_MPDU_NOSEC89_MPDU_SEND_NOSEC_QUE8_MASK                       0xffff
#define     DBG_TX_MPDU_NOSEC89_MPDU_SEND_NOSEC_QUE8_SHIFT                           0
/* mpdu send for queue9 without security */
#define     DBG_TX_MPDU_NOSEC89_MPDU_SEND_NOSEC_QUE9_MASK                   0xffff0000
#define     DBG_TX_MPDU_NOSEC89_MPDU_SEND_NOSEC_QUE9_SHIFT                          16

#define REG_DBG_ERR_CNT0                                                         0x768
/* tx rts flow error count for bandwidth signaling TA, bw > 20Mhz */
#define     DBG_ERR_CNT0_RTS_BS_FLOW_ERR_CNT_MASK                                 0xff
#define     DBG_ERR_CNT0_RTS_BS_FLOW_ERR_CNT_SHIFT                                   0
/* tx rts and wait ctsflow timeout count */
#define     DBG_ERR_CNT0_RTS_FLOW_TM_CNT_MASK                                   0xff00
#define     DBG_ERR_CNT0_RTS_FLOW_TM_CNT_SHIFT                                       8
/* tx data and wait ack/ba flow timeout count */
#define     DBG_ERR_CNT0_ACK_FLOW_TM_CNT_MASK                                 0xff0000
#define     DBG_ERR_CNT0_ACK_FLOW_TM_CNT_SHIFT                                      16
/* count for edca prepare to send pkt in last slot */
/* but find cca before final contention, stop tx */
#define     DBG_ERR_CNT0_EDCA_CCA_STOP_CNT_MASK                             0xff000000
#define     DBG_ERR_CNT0_EDCA_CCA_STOP_CNT_SHIFT                                    24

#define REG_DBG_PPDU_FLOW                                                        0x76C
/* ppdu counter of SHM_TX_RESULTN.OK == 1 cases */
#define     DBG_PPDU_FLOW_TX_PPDU_FLOW_PASS_MASK                                  0xff
#define     DBG_PPDU_FLOW_TX_PPDU_FLOW_PASS_SHIFT                                    0
/* ppdu counter of SHM_TX_RESULTN.OK == 0 cases */
#define     DBG_PPDU_FLOW_TX_PPDU_FLOW_FAIL_MASK                                0xff00
#define     DBG_PPDU_FLOW_TX_PPDU_FLOW_FAIL_SHIFT                                    8
/* tx dma request count */
#define     DBG_PPDU_FLOW_TX_DMA_REQ_CNT_MASK                               0xffff0000
#define     DBG_PPDU_FLOW_TX_DMA_REQ_CNT_SHIFT                                      16

#define REG_DBG_RX_PPDU_CNT0                                                     0x770
/* receive ppdu count */
#define     DBG_RX_PPDU_CNT0_RX_PPDU_CNT0_MASK                                  0xffff
#define     DBG_RX_PPDU_CNT0_RX_PPDU_CNT0_SHIFT                                      0
/* rx data type mpdu count */
#define     DBG_RX_PPDU_CNT0_RX_MPDU_DATA_CNT_MASK                          0xffff0000
#define     DBG_RX_PPDU_CNT0_RX_MPDU_DATA_CNT_SHIFT                                 16

#define REG_DBG_RX_PPDU_CNT1                                                     0x774
/* rx mgmt type mpdu count */
#define     DBG_RX_PPDU_CNT1_RX_MPDU_MGMT_CNT_MASK                              0xffff
#define     DBG_RX_PPDU_CNT1_RX_MPDU_MGMT_CNT_SHIFT                                  0
/* rx ctrl type mpdu count */
#define     DBG_RX_PPDU_CNT1_RX_MPDU_CTRL_CNT_MASK                          0xffff0000
#define     DBG_RX_PPDU_CNT1_RX_MPDU_CTRL_CNT_SHIFT                                 16

#define REG_DBG_RX_PPDU_CNT2                                                     0x778
/* rx ndp ppdu count */
#define     DBG_RX_PPDU_CNT2_RX_NDP_CNT_MASK                                    0xffff
#define     DBG_RX_PPDU_CNT2_RX_NDP_CNT_SHIFT                                        0
/* rx mpdu key search error */
#define     DBG_RX_PPDU_CNT2_RX_MPDU_KEY_ERR_CNT_MASK                       0xffff0000
#define     DBG_RX_PPDU_CNT2_RX_MPDU_KEY_ERR_CNT_SHIFT                              16

#define REG_DBG_RX_PPDU_CNT3                                                     0x77C
/* rx mpdu mic error count */
#define     DBG_RX_PPDU_CNT3_RX_MPDU_MIC_ERR_CNT_MASK                           0xffff
#define     DBG_RX_PPDU_CNT3_RX_MPDU_MIC_ERR_CNT_SHIFT                               0
/* rx mpdu icv error count */
#define     DBG_RX_PPDU_CNT3_RX_MPDU_ICV_ERR_CNT_MASK                       0xffff0000
#define     DBG_RX_PPDU_CNT3_RX_MPDU_ICV_ERR_CNT_SHIFT                              16

#define REG_DBG_RX_PPDU_CNT4                                                     0x780
/* rx mpdu fcs error count(buf overflow is also treated as fcs err) */
#define     DBG_RX_PPDU_CNT4_RX_MPDU_FCS_ERR_CNT_MASK                           0xffff
#define     DBG_RX_PPDU_CNT4_RX_MPDU_FCS_ERR_CNT_SHIFT                               0
/* rx mpdu and find buf overflow count */
#define     DBG_RX_PPDU_CNT4_RX_MPDU_BUF_OVERFLOW_CNT_MASK                  0xffff0000
#define     DBG_RX_PPDU_CNT4_RX_MPDU_BUF_OVERFLOW_CNT_SHIFT                         16

#define REG_DBG_RX_PPDU_CNT5                                                     0x784
/* rx mpdu filter in count */
#define     DBG_RX_PPDU_CNT5_RX_MPDU_FILTER_IN_CNT_MASK                         0xffff
#define     DBG_RX_PPDU_CNT5_RX_MPDU_FILTER_IN_CNT_SHIFT                             0
/* rx mpdu filter out count */
#define     DBG_RX_PPDU_CNT5_RX_MPDU_FILTER_OUT_CNT_MASK                    0xffff0000
#define     DBG_RX_PPDU_CNT5_RX_MPDU_FILTER_OUT_CNT_SHIFT                           16

#define REG_DBG_RX_PPDU_CNT6                                                     0x788
/* rx mpdu need to be decrypted count */
#define     DBG_RX_PPDU_CNT6_RX_MPDU_DECRYPT_CNT_MASK                           0xffff
#define     DBG_RX_PPDU_CNT6_RX_MPDU_DECRYPT_CNT_SHIFT                               0
/* rxdma count */
#define     DBG_RX_PPDU_CNT6_RXDMA_CNT_MASK                                 0xffff0000
#define     DBG_RX_PPDU_CNT6_RXDMA_CNT_SHIFT                                        16

#define REG_DBG_INTR_CNT0                                                        0x78C
/* tx_done interrupt count for queue0 */
#define     DBG_INTR_CNT0_TX_DONE0_CNT_MASK                                       0xff
#define     DBG_INTR_CNT0_TX_DONE0_CNT_SHIFT                                         0
/* tx_done interrupt count for queue1 */
#define     DBG_INTR_CNT0_TX_DONE1_CNT_MASK                                     0xff00
#define     DBG_INTR_CNT0_TX_DONE1_CNT_SHIFT                                         8
/* tx_done interrupt count for queue2 */
#define     DBG_INTR_CNT0_TX_DONE2_CNT_MASK                                   0xff0000
#define     DBG_INTR_CNT0_TX_DONE2_CNT_SHIFT                                        16
/* tx_done interrupt count for queue3 */
#define     DBG_INTR_CNT0_TX_DONE3_CNT_MASK                                 0xff000000
#define     DBG_INTR_CNT0_TX_DONE3_CNT_SHIFT                                        24

#define REG_DBG_INTR_CNT1                                                        0x790
/* tx_done interrupt count for queue4 */
#define     DBG_INTR_CNT1_TX_DONE4_CNT_MASK                                       0xff
#define     DBG_INTR_CNT1_TX_DONE4_CNT_SHIFT                                         0
/* tx_done interrupt count for queue5 */
#define     DBG_INTR_CNT1_TX_DONE5_CNT_MASK                                     0xff00
#define     DBG_INTR_CNT1_TX_DONE5_CNT_SHIFT                                         8
/* tx_done interrupt count for queue6 */
#define     DBG_INTR_CNT1_TX_DONE6_CNT_MASK                                   0xff0000
#define     DBG_INTR_CNT1_TX_DONE6_CNT_SHIFT                                        16
/* tx_done interrupt count for queue7 */
#define     DBG_INTR_CNT1_TX_DONE7_CNT_MASK                                 0xff000000
#define     DBG_INTR_CNT1_TX_DONE7_CNT_SHIFT                                        24

#define REG_DBG_INTR_CNT2                                                        0x794
/* tx_done interrupt count for queue8 */
#define     DBG_INTR_CNT2_TX_DONE8_CNT_MASK                                       0xff
#define     DBG_INTR_CNT2_TX_DONE8_CNT_SHIFT                                         0
/* tx_done interrupt count for queue9 */
#define     DBG_INTR_CNT2_TX_DONE9_CNT_MASK                                     0xff00
#define     DBG_INTR_CNT2_TX_DONE9_CNT_SHIFT                                         8
/* mcu interrupt0 count */
#define     DBG_INTR_CNT2_MCU_INTR0_CNT_MASK                                  0xff0000
#define     DBG_INTR_CNT2_MCU_INTR0_CNT_SHIFT                                       16
/* mcu interrupt1 count */
#define     DBG_INTR_CNT2_MCU_INTR1_CNT_MASK                                0xff000000
#define     DBG_INTR_CNT2_MCU_INTR1_CNT_SHIFT                                       24

#define REG_DBG_INTR_CNT3                                                        0x798
/* mcu interrupt2 count */
#define     DBG_INTR_CNT3_MCU_INTR2_CNT_MASK                                      0xff
#define     DBG_INTR_CNT3_MCU_INTR2_CNT_SHIFT                                        0
/* mcu interrupt3 count */
#define     DBG_INTR_CNT3_MCU_INTR3_CNT_MASK                                    0xff00
#define     DBG_INTR_CNT3_MCU_INTR3_CNT_SHIFT                                        8
/* mcu interrupt4 count */
#define     DBG_INTR_CNT3_MCU_INTR4_CNT_MASK                                  0xff0000
#define     DBG_INTR_CNT3_MCU_INTR4_CNT_SHIFT                                       16
/* mcu interrupt5 count */
#define     DBG_INTR_CNT3_MCU_INTR5_CNT_MASK                                0xff000000
#define     DBG_INTR_CNT3_MCU_INTR5_CNT_SHIFT                                       24

#define REG_DBG_INTR_CNT4                                                        0x79C
/* rx done interrupt count */
#define     DBG_INTR_CNT4_RX_DONE_CNT_MASK                                      0xffff
#define     DBG_INTR_CNT4_RX_DONE_CNT_SHIFT                                          0
/* because of DFS, trigger doesn't send out response for 26 tone ru counter */
#define     DBG_INTR_CNT4_TRIGGER_DFS_CNT_MASK                                0xff0000
#define     DBG_INTR_CNT4_TRIGGER_DFS_CNT_SHIFT                                     16
/* because of DFS, trs mpdu doesn't send out response for 26 tone ru counter */
#define     DBG_INTR_CNT4_TRS_DFS_CNT_MASK                                  0xff000000
#define     DBG_INTR_CNT4_TRS_DFS_CNT_SHIFT                                         24

#define REG_DBG_INTR_CNT5                                                        0x7A0
/* interface 0 tsf 0 interrupt count */
#define     DBG_INTR_CNT5_INTF0_TSF0_INTR_CNT_MASK                                0xff
#define     DBG_INTR_CNT5_INTF0_TSF0_INTR_CNT_SHIFT                                  0
/* interface 0 tsf 1 interrupt count */
#define     DBG_INTR_CNT5_INTF0_TSF1_INTR_CNT_MASK                              0xff00
#define     DBG_INTR_CNT5_INTF0_TSF1_INTR_CNT_SHIFT                                  8
/* interface 1 tsf 0 interrupt count */
#define     DBG_INTR_CNT5_INTF1_TSF0_INTR_CNT_MASK                            0xff0000
#define     DBG_INTR_CNT5_INTF1_TSF0_INTR_CNT_SHIFT                                 16
/* interface 1 tsf 1 interrupt count */
#define     DBG_INTR_CNT5_INTF1_TSF1_INTR_CNT_MASK                          0xff000000
#define     DBG_INTR_CNT5_INTF1_TSF1_INTR_CNT_SHIFT                                 24

#define REG_DBG_INTR_CNT6                                                        0x7A4
/* interrupt count for rx_esp */
#define     DBG_INTR_CNT6_RX_ESP_INTR_CNT_MASK                                    0xff
#define     DBG_INTR_CNT6_RX_ESP_INTR_CNT_SHIFT                                      0
/* interrupt count for resp_esp */
#define     DBG_INTR_CNT6_RESP_ESP_INTR_CNT_MASK                                0xff00
#define     DBG_INTR_CNT6_RESP_ESP_INTR_CNT_SHIFT                                    8
/* interrupt count for nomore_tf_esp */
#define     DBG_INTR_CNT6_RX_NOMORE_TF_ESP_CNT_MASK                           0xff0000
#define     DBG_INTR_CNT6_RX_NOMORE_TF_ESP_CNT_SHIFT                                16
/* interrupt count for nomore_raru_esp */
#define     DBG_INTR_CNT6_RX_NOMORE_RARU_ESP_CNT_MASK                       0xff000000
#define     DBG_INTR_CNT6_RX_NOMORE_RARU_ESP_CNT_SHIFT                              24

#define REG_DBG_LAST_TXVEC0                                                      0x7A8
/* tx mcs */
#define     DBG_LAST_TXVEC0_TX_MCS_MASK                                           0x7f
#define     DBG_LAST_TXVEC0_TX_MCS_SHIFT                                             0
/* tx preamble_type */
#define     DBG_LAST_TXVEC0_PREAMBLE_TYPE_MASK                                    0x80
#define     DBG_LAST_TXVEC0_PREAMBLE_TYPE_SHIFT                                      7
/* tx l_length */
#define     DBG_LAST_TXVEC0_L_LENGTH_MASK                                      0xfff00
#define     DBG_LAST_TXVEC0_L_LENGTH_SHIFT                                           8
/* tx format */
#define     DBG_LAST_TXVEC0_FORMAT_MASK                                       0x700000
#define     DBG_LAST_TXVEC0_FORMAT_SHIFT                                            20
/* tx fec_coding */
#define     DBG_LAST_TXVEC0_FEC_CODING_MASK                                   0x800000
#define     DBG_LAST_TXVEC0_FEC_CODING_SHIFT                                        23
/* tx txpwr_level_index */
#define     DBG_LAST_TXVEC0_TXPWR_LEVEL_INDEX_MASK                          0xff000000
#define     DBG_LAST_TXVEC0_TXPWR_LEVEL_INDEX_SHIFT                                 24

#define REG_DBG_LAST_TXVEC1                                                      0x7AC
/* tx length */
#define     DBG_LAST_TXVEC1_LENGTH_MASK                                         0xffff
#define     DBG_LAST_TXVEC1_LENGTH_SHIFT                                             0
/* tx spatial_reuse */
#define     DBG_LAST_TXVEC1_SPATIAL_REUSE_MASK                              0xffff0000
#define     DBG_LAST_TXVEC1_SPATIAL_REUSE_SHIFT                                     16

#define REG_DBG_LAST_TXVEC2                                                      0x7B0
/* tx apep_length */
#define     DBG_LAST_TXVEC2_APEP_LENGTH_MASK                                   0xfffff
#define     DBG_LAST_TXVEC2_APEP_LENGTH_SHIFT                                        0
/* tx ch_bandwidth */
#define     DBG_LAST_TXVEC2_CH_BANDWIDTH_MASK                                 0x700000
#define     DBG_LAST_TXVEC2_CH_BANDWIDTH_SHIFT                                      20
/* tx stbc */
#define     DBG_LAST_TXVEC2_STBC_MASK                                         0x800000
#define     DBG_LAST_TXVEC2_STBC_SHIFT                                              23
/* tx ru_allocation */
#define     DBG_LAST_TXVEC2_RU_ALLOCATION_MASK                              0xff000000
#define     DBG_LAST_TXVEC2_RU_ALLOCATION_SHIFT                                     24

#define REG_DBG_LAST_TXVEC3                                                      0x7B4
/* tx he_sig_a2_reserved */
#define     DBG_LAST_TXVEC3_HE_SIG_A2_RESERVED_MASK                              0x1ff
#define     DBG_LAST_TXVEC3_HE_SIG_A2_RESERVED_SHIFT                                 0
/* tx txop_duration */
#define     DBG_LAST_TXVEC3_TXOP_DURATION_MASK                                  0xfe00
#define     DBG_LAST_TXVEC3_TXOP_DURATION_SHIFT                                      9
/* tx partial_aid */
#define     DBG_LAST_TXVEC3_PARTIAL_AID_MASK                                 0x1ff0000
#define     DBG_LAST_TXVEC3_PARTIAL_AID_SHIFT                                       16
/* tx rsvd */
#define     DBG_LAST_TXVEC3_RSVD_MASK                                        0xe000000
#define     DBG_LAST_TXVEC3_RSVD_SHIFT                                              25
/* tx dyn_bandwidth_in_non_ht */
#define     DBG_LAST_TXVEC3_DYN_BANDWIDTH_IN_NON_HT_MASK                    0x10000000
#define     DBG_LAST_TXVEC3_DYN_BANDWIDTH_IN_NON_HT_SHIFT                           28
/* tx ch_bandwidth_in_non_ht */
#define     DBG_LAST_TXVEC3_CH_BANDWIDTH_IN_NON_HT_MASK                     0x60000000
#define     DBG_LAST_TXVEC3_CH_BANDWIDTH_IN_NON_HT_SHIFT                            29
/* tx midamble_periodicity */
#define     DBG_LAST_TXVEC3_MIDAMBLE_PERIODICITY_MASK                       0x80000000
#define     DBG_LAST_TXVEC3_MIDAMBLE_PERIODICITY_SHIFT                              31

#define REG_DBG_LAST_TXVEC4                                                      0x7B8
/* reserved field */
#define     DBG_LAST_TXVEC4_SERVICE_RSVD_MASK                                   0xffff
#define     DBG_LAST_TXVEC4_SERVICE_RSVD_SHIFT                                       0
/* tx group_id */
#define     DBG_LAST_TXVEC4_GROUP_ID_MASK                                     0x3f0000
#define     DBG_LAST_TXVEC4_GROUP_ID_SHIFT                                          16
/* tx gi_type */
#define     DBG_LAST_TXVEC4_GI_TYPE_MASK                                      0xc00000
#define     DBG_LAST_TXVEC4_GI_TYPE_SHIFT                                           22
/* tx ru_tone_set_index */
#define     DBG_LAST_TXVEC4_RU_TONE_SET_INDEX_MASK                          0xff000000
#define     DBG_LAST_TXVEC4_RU_TONE_SET_INDEX_SHIFT                                 24

#define REG_DBG_LAST_TXVEC5                                                      0x7BC
/* tx scrambler_initial_value */
#define     DBG_LAST_TXVEC5_SCRAMBLER_INITIAL_VALUE_MASK                          0x7f
#define     DBG_LAST_TXVEC5_SCRAMBLER_INITIAL_VALUE_SHIFT                            0
/* tx ldpc_extra_symbol */
#define     DBG_LAST_TXVEC5_LDPC_EXTRA_SYMBOL_MASK                                0x80
#define     DBG_LAST_TXVEC5_LDPC_EXTRA_SYMBOL_SHIFT                                  7
/* tx bss_color */
#define     DBG_LAST_TXVEC5_BSS_COLOR_MASK                                      0x3f00
#define     DBG_LAST_TXVEC5_BSS_COLOR_SHIFT                                          8
/* tx he_ltf_type */
#define     DBG_LAST_TXVEC5_HE_LTF_TYPE_MASK                                    0xc000
#define     DBG_LAST_TXVEC5_HE_LTF_TYPE_SHIFT                                       14
/* tx total_num_sts */
#define     DBG_LAST_TXVEC5_TOTAL_NUM_STS_MASK                                 0x70000
#define     DBG_LAST_TXVEC5_TOTAL_NUM_STS_SHIFT                                     16
/* tx num_he_ltf */
#define     DBG_LAST_TXVEC5_NUM_HE_LTF_MASK                                   0x380000
#define     DBG_LAST_TXVEC5_NUM_HE_LTF_SHIFT                                        19
/* tx nominal_packet_padding */
#define     DBG_LAST_TXVEC5_NOMINAL_PACKET_PADDING_MASK                       0xc00000
#define     DBG_LAST_TXVEC5_NOMINAL_PACKET_PADDING_SHIFT                            22
/* tx default_pe_duration */
#define     DBG_LAST_TXVEC5_DEFAULT_PE_DURATION_MASK                         0x7000000
#define     DBG_LAST_TXVEC5_DEFAULT_PE_DURATION_SHIFT                               24
/* tx starting_sts_num */
#define     DBG_LAST_TXVEC5_STARTING_STS_NUM_MASK                           0x38000000
#define     DBG_LAST_TXVEC5_STARTING_STS_NUM_SHIFT                                  27
/* tx smoothing */
#define     DBG_LAST_TXVEC5_SMOOTHING_MASK                                  0x40000000
#define     DBG_LAST_TXVEC5_SMOOTHING_SHIFT                                         30
/* tx aggregation */
#define     DBG_LAST_TXVEC5_AGGREGATION_MASK                                0x80000000
#define     DBG_LAST_TXVEC5_AGGREGATION_SHIFT                                       31

#define REG_DBG_LAST_TXVEC6                                                      0x7C0
/* tx dcm */
#define     DBG_LAST_TXVEC6_DCM_MASK                                               0x1
#define     DBG_LAST_TXVEC6_DCM_SHIFT                                                0
/* tx num_sts */
#define     DBG_LAST_TXVEC6_NUM_STS_MASK                                           0x2
#define     DBG_LAST_TXVEC6_NUM_STS_SHIFT                                            1
/* tx doppler */
#define     DBG_LAST_TXVEC6_DOPPLER_MASK                                           0x4
#define     DBG_LAST_TXVEC6_DOPPLER_SHIFT                                            2
/* he_ltf_mode */
#define     DBG_LAST_TXVEC6_HE_LTF_MODE_MASK                                       0x8
#define     DBG_LAST_TXVEC6_HE_LTF_MODE_SHIFT                                        3
/* txop_ps_not_allowed */
#define     DBG_LAST_TXVEC6_TXOP_PS_NOT_ALLOWED_MASK                              0x10
#define     DBG_LAST_TXVEC6_TXOP_PS_NOT_ALLOWED_SHIFT                                4
/* trigger_method */
#define     DBG_LAST_TXVEC6_TRIGGER_METHOD_MASK                                   0x20
#define     DBG_LAST_TXVEC6_TRIGGER_METHOD_SHIFT                                     5
/* tx uplink_flag */
#define     DBG_LAST_TXVEC6_UPLINK_FLAG_MASK                                      0x40
#define     DBG_LAST_TXVEC6_UPLINK_FLAG_SHIFT                                        6
/* trigger_responding */
#define     DBG_LAST_TXVEC6_TRIGGER_RESPONDING_MASK                               0x80
#define     DBG_LAST_TXVEC6_TRIGGER_RESPONDING_SHIFT                                 7
/* tx he_tb_data_symbols */
#define     DBG_LAST_TXVEC6_HE_TB_DATA_SYMBOLS_MASK                             0x1f00
#define     DBG_LAST_TXVEC6_HE_TB_DATA_SYMBOLS_SHIFT                                 8
/* tx he_tb_pre_fec_factor */
#define     DBG_LAST_TXVEC6_HE_TB_PRE_FEC_FACTOR_MASK                           0x6000
#define     DBG_LAST_TXVEC6_HE_TB_PRE_FEC_FACTOR_SHIFT                              13
/* tx feedback_status */
#define     DBG_LAST_TXVEC6_FEEDBACK_STATUS_MASK                                0x8000
#define     DBG_LAST_TXVEC6_FEEDBACK_STATUS_SHIFT                                   15
/* tx he_tb_pe_disambiguity */
#define     DBG_LAST_TXVEC6_HE_TB_PE_DISAMBIGUITY_MASK                         0x10000
#define     DBG_LAST_TXVEC6_HE_TB_PE_DISAMBIGUITY_SHIFT                             16
/* tx err cnt from phy */
#define     DBG_LAST_TXVEC6_TX_ERR_CNT_MASK                                  0xf000000
#define     DBG_LAST_TXVEC6_TX_ERR_CNT_SHIFT                                        24
/* tx_phy_type */
#define     DBG_LAST_TXVEC6_TX_PHY_TYPE_LAT_MASK                            0x80000000
#define     DBG_LAST_TXVEC6_TX_PHY_TYPE_LAT_SHIFT                                   31

#define REG_DBG_LAST_RXVEC0                                                      0x7C4
/* rx mcs */
#define     DBG_LAST_RXVEC0_MCS_MASK                                              0x7f
#define     DBG_LAST_RXVEC0_MCS_SHIFT                                                0
/* rx preamble_type */
#define     DBG_LAST_RXVEC0_PREAMBLE_TYPE_MASK                                    0x80
#define     DBG_LAST_RXVEC0_PREAMBLE_TYPE_SHIFT                                      7
/* rx l_length */
#define     DBG_LAST_RXVEC0_L_LENGTH_MASK                                      0xfff00
#define     DBG_LAST_RXVEC0_L_LENGTH_SHIFT                                           8
/* rx format */
#define     DBG_LAST_RXVEC0_FORMAT_MASK                                       0x700000
#define     DBG_LAST_RXVEC0_FORMAT_SHIFT                                            20
/* rx fec_coding */
#define     DBG_LAST_RXVEC0_FEC_CODING_MASK                                   0x800000
#define     DBG_LAST_RXVEC0_FEC_CODING_SHIFT                                        23
/* rx rssi_legacy */
#define     DBG_LAST_RXVEC0_RSSI_LEGACY_MASK                                0xff000000
#define     DBG_LAST_RXVEC0_RSSI_LEGACY_SHIFT                                       24

#define REG_DBG_LAST_RXVEC1                                                      0x7C8
/* rx length */
#define     DBG_LAST_RXVEC1_LENGTH_MASK                                         0xffff
#define     DBG_LAST_RXVEC1_LENGTH_SHIFT                                             0
/* rx spatial_reuse */
#define     DBG_LAST_RXVEC1_SPATIAL_REUSE_MASK                                 0xf0000
#define     DBG_LAST_RXVEC1_SPATIAL_REUSE_SHIFT                                     16
/* rx mcs_sig_b */
#define     DBG_LAST_RXVEC1_MCS_SIG_B_MASK                                    0x700000
#define     DBG_LAST_RXVEC1_MCS_SIG_B_SHIFT                                         20
/* rx dcm */
#define     DBG_LAST_RXVEC1_DCM_SIG_B_MASK                                    0x800000
#define     DBG_LAST_RXVEC1_DCM_SIG_B_SHIFT                                         23
/* rx rssi */
#define     DBG_LAST_RXVEC1_RSSI_MASK                                       0xff000000
#define     DBG_LAST_RXVEC1_RSSI_SHIFT                                              24

#define REG_DBG_LAST_RXVEC2                                                      0x7CC
/* rx psdu_length */
#define     DBG_LAST_RXVEC2_PSDU_LENGTH_MASK                                   0xfffff
#define     DBG_LAST_RXVEC2_PSDU_LENGTH_SHIFT                                        0
/* rx ch_bandwidth */
#define     DBG_LAST_RXVEC2_CH_BANDWIDTH_MASK                                 0x700000
#define     DBG_LAST_RXVEC2_CH_BANDWIDTH_SHIFT                                      20
/* rx stbc */
#define     DBG_LAST_RXVEC2_STBC_MASK                                         0x800000
#define     DBG_LAST_RXVEC2_STBC_SHIFT                                              23
/* rx txop_duration */
#define     DBG_LAST_RXVEC2_TXOP_DURATION_MASK                              0x7f000000
#define     DBG_LAST_RXVEC2_TXOP_DURATION_SHIFT                                     24
/* rx dcm */
#define     DBG_LAST_RXVEC2_DCM_MASK                                        0x80000000
#define     DBG_LAST_RXVEC2_DCM_SHIFT                                               31

#define REG_DBG_LAST_RXVEC3                                                      0x7D0
/* rx ru_allocation0 */
#define     DBG_LAST_RXVEC3_RU_ALLOCATION0_MASK                                   0xff
#define     DBG_LAST_RXVEC3_RU_ALLOCATION0_SHIFT                                     0
/* rx ru_allocation1 */
#define     DBG_LAST_RXVEC3_RU_ALLOCATION1_MASK                                 0xff00
#define     DBG_LAST_RXVEC3_RU_ALLOCATION1_SHIFT                                     8
/* rx ru_allocation2 */
#define     DBG_LAST_RXVEC3_RU_ALLOCATION2_MASK                               0xff0000
#define     DBG_LAST_RXVEC3_RU_ALLOCATION2_SHIFT                                    16
/* rx ru_allocation3 */
#define     DBG_LAST_RXVEC3_RU_ALLOCATION3_MASK                             0xff000000
#define     DBG_LAST_RXVEC3_RU_ALLOCATION3_SHIFT                                    24

#define REG_DBG_LAST_RXVEC4                                                      0x7D4
/* rx ru_allocation4 */
#define     DBG_LAST_RXVEC4_RU_ALLOCATION4_MASK                                   0xff
#define     DBG_LAST_RXVEC4_RU_ALLOCATION4_SHIFT                                     0
/* rx ru_allocation5 */
#define     DBG_LAST_RXVEC4_RU_ALLOCATION5_MASK                                 0xff00
#define     DBG_LAST_RXVEC4_RU_ALLOCATION5_SHIFT                                     8
/* rx ru_allocation6 */
#define     DBG_LAST_RXVEC4_RU_ALLOCATION6_MASK                               0xff0000
#define     DBG_LAST_RXVEC4_RU_ALLOCATION6_SHIFT                                    16
/* rx ru_allocation7 */
#define     DBG_LAST_RXVEC4_RU_ALLOCATION7_MASK                             0xff000000
#define     DBG_LAST_RXVEC4_RU_ALLOCATION7_SHIFT                                    24

#define REG_DBG_LAST_RXVEC5                                                      0x7D8
/* rx sta_id_list */
#define     DBG_LAST_RXVEC5_STA_ID_LIST_MASK                                     0x7ff
#define     DBG_LAST_RXVEC5_STA_ID_LIST_SHIFT                                        0
/* rx pe_duration */
#define     DBG_LAST_RXVEC5_PE_DURATION_MASK                                    0x3800
#define     DBG_LAST_RXVEC5_PE_DURATION_SHIFT                                       11
/* rx gi_type */
#define     DBG_LAST_RXVEC5_GI_TYPE_MASK                                        0xc000
#define     DBG_LAST_RXVEC5_GI_TYPE_SHIFT                                           14
/* rx partial_aid */
#define     DBG_LAST_RXVEC5_PARTIAL_AID_MASK                                 0x1ff0000
#define     DBG_LAST_RXVEC5_PARTIAL_AID_SHIFT                                       16
/* rx num_sts */
#define     DBG_LAST_RXVEC5_NUM_STS_MASK                                     0xe000000
#define     DBG_LAST_RXVEC5_NUM_STS_SHIFT                                           25
/* rx dyn_bandwidth_in_non_ht */
#define     DBG_LAST_RXVEC5_DYN_BANDWIDTH_IN_NON_HT_MASK                    0x10000000
#define     DBG_LAST_RXVEC5_DYN_BANDWIDTH_IN_NON_HT_SHIFT                           28
/* rx ch_bandwidth_in_non_ht */
#define     DBG_LAST_RXVEC5_CH_BANDWIDTH_IN_NON_HT_MASK                     0x60000000
#define     DBG_LAST_RXVEC5_CH_BANDWIDTH_IN_NON_HT_SHIFT                            29
/* rx doppler */
#define     DBG_LAST_RXVEC5_DOPPLER_MASK                                    0x80000000
#define     DBG_LAST_RXVEC5_DOPPLER_SHIFT                                           31

#define REG_DBG_LAST_RXVEC6                                                      0x7DC
/* rx scrambler_initial_value */
#define     DBG_LAST_RXVEC6_SCRAMBLER_INITIAL_VALUE_MASK                          0x7f
#define     DBG_LAST_RXVEC6_SCRAMBLER_INITIAL_VALUE_SHIFT                            0
/* rx uplink_flag */
#define     DBG_LAST_RXVEC6_UPLINK_FLAG_MASK                                      0x80
#define     DBG_LAST_RXVEC6_UPLINK_FLAG_SHIFT                                        7
/* rx bss_color */
#define     DBG_LAST_RXVEC6_BSS_COLOR_MASK                                      0x3f00
#define     DBG_LAST_RXVEC6_BSS_COLOR_SHIFT                                          8
/* rx he_ltf_type */
#define     DBG_LAST_RXVEC6_HE_LTF_TYPE_MASK                                    0xc000
#define     DBG_LAST_RXVEC6_HE_LTF_TYPE_SHIFT                                       14
/* rx group_id */
#define     DBG_LAST_RXVEC6_GROUP_ID_MASK                                     0x3f0000
#define     DBG_LAST_RXVEC6_GROUP_ID_SHIFT                                          16
/* rx user_position */
#define     DBG_LAST_RXVEC6_USER_POSITION_MASK                                0xc00000
#define     DBG_LAST_RXVEC6_USER_POSITION_SHIFT                                     22
/* rx beam_change */
#define     DBG_LAST_RXVEC6_BEAM_CHANGE_MASK                                 0x1000000
#define     DBG_LAST_RXVEC6_BEAM_CHANGE_SHIFT                                       24
/* rx beamformed */
#define     DBG_LAST_RXVEC6_BEAMFORMED_MASK                                  0x2000000
#define     DBG_LAST_RXVEC6_BEAMFORMED_SHIFT                                        25
/* rx txop_ps_not_allowed */
#define     DBG_LAST_RXVEC6_TXOP_PS_NOT_ALLOWED_MASK                         0x4000000
#define     DBG_LAST_RXVEC6_TXOP_PS_NOT_ALLOWED_SHIFT                               26
/* rx non_ht_modulation */
#define     DBG_LAST_RXVEC6_NON_HT_MODULATION_MASK                           0x8000000
#define     DBG_LAST_RXVEC6_NON_HT_MODULATION_SHIFT                                 27
/* rx aggregation */
#define     DBG_LAST_RXVEC6_AGGREGATION_MASK                                0x10000000
#define     DBG_LAST_RXVEC6_AGGREGATION_SHIFT                                       28
/* rx sounding */
#define     DBG_LAST_RXVEC6_SOUNDING_MASK                                   0x20000000
#define     DBG_LAST_RXVEC6_SOUNDING_SHIFT                                          29
/* rx smoothing */
#define     DBG_LAST_RXVEC6_SMOOTHING_MASK                                  0x40000000
#define     DBG_LAST_RXVEC6_SMOOTHING_SHIFT                                         30
/* rx lsigvalid */
#define     DBG_LAST_RXVEC6_LSIGVALID_MASK                                  0x80000000
#define     DBG_LAST_RXVEC6_LSIGVALID_SHIFT                                         31

#define REG_DBG_LAST_RXVEC7                                                      0x7E0
/* rx spatial_reuse2 */
#define     DBG_LAST_RXVEC7_SPATIAL_REUSE2_MASK                                    0xf
#define     DBG_LAST_RXVEC7_SPATIAL_REUSE2_SHIFT                                     0
/* rx spatial_reuse3 */
#define     DBG_LAST_RXVEC7_SPATIAL_REUSE3_MASK                                   0xf0
#define     DBG_LAST_RXVEC7_SPATIAL_REUSE3_SHIFT                                     4
/* rx spatial_reuse4 */
#define     DBG_LAST_RXVEC7_SPATIAL_REUSE4_MASK                                  0xf00
#define     DBG_LAST_RXVEC7_SPATIAL_REUSE4_SHIFT                                     8
/* rx rsvd */
#define     DBG_LAST_RXVEC7_RX_RSVD_MASK                                        0x7000
#define     DBG_LAST_RXVEC7_RX_RSVD_SHIFT                                           12
/* rx phy_type */
#define     DBG_LAST_RXVEC7_RX_PHY_TYPE_MASK                                    0x8000
#define     DBG_LAST_RXVEC7_RX_PHY_TYPE_SHIFT                                       15
/* rx remain_time */
#define     DBG_LAST_RXVEC7_REMAIN_TIME_MASK                                0xffff0000
#define     DBG_LAST_RXVEC7_REMAIN_TIME_SHIFT                                       16

#define REG_DBG_LAST_RXVEC8                                                      0x7E4
/* rx rcpi */
#define     DBG_LAST_RXVEC8_RCPI_MASK                                             0xff
#define     DBG_LAST_RXVEC8_RCPI_SHIFT                                               0
/* rx cfo_est */
#define     DBG_LAST_RXVEC8_CFO_EST_MASK                                      0x1fff00
#define     DBG_LAST_RXVEC8_CFO_EST_SHIFT                                            8
/* rx_vec_tail_rsvd */
#define     DBG_LAST_RXVEC8_RX_VEC_TAIL_RSVD_MASK                           0xffe00000
#define     DBG_LAST_RXVEC8_RX_VEC_TAIL_RSVD_SHIFT                                  21

#define REG_DBG_NAV0                                                             0x7E8
/* intf0 inbss nav */
#define     DBG_NAV0_VIF0_INBSS_NAV_MASK                                        0xffff
#define     DBG_NAV0_VIF0_INBSS_NAV_SHIFT                                            0
/* intf0 nav */
#define     DBG_NAV0_VIF0_NAV_MASK                                          0xffff0000
#define     DBG_NAV0_VIF0_NAV_SHIFT                                                 16

#define REG_DBG_NAV1                                                             0x7EC
/* intf1 inbss nav */
#define     DBG_NAV1_VIF1_INBSS_NAV_MASK                                        0xffff
#define     DBG_NAV1_VIF1_INBSS_NAV_SHIFT                                            0
/* intf1 nav */
#define     DBG_NAV1_VIF1_NAV_MASK                                          0xffff0000
#define     DBG_NAV1_VIF1_NAV_SHIFT                                                 16

#define REG_DBG_SR0                                                              0x7F0
/* srg_sr_abort count */
#define     DBG_SR0_SRG_SR_ABORT_MASK                                             0xff
#define     DBG_SR0_SRG_SR_ABORT_SHIFT                                               0
/* nonsrg_sr_abort count */
#define     DBG_SR0_NON_SRG_SR_ABORT_MASK                                       0xff00
#define     DBG_SR0_NON_SRG_SR_ABORT_SHIFT                                           8
/* srg_cnt count */
#define     DBG_SR0_SRG_CNT_MASK                                              0xff0000
#define     DBG_SR0_SRG_CNT_SHIFT                                                   16
/* non_srg_cnt count */
#define     DBG_SR0_NONSRG_CNT_MASK                                         0xff000000
#define     DBG_SR0_NONSRG_CNT_SHIFT                                                24

#define REG_DBG_SR1                                                              0x7F4
/* abort_ppdu_eof_deli count */
#define     DBG_SR1_ABORT_PPDU_EOF_DELI_MASK                                      0xff
#define     DBG_SR1_ABORT_PPDU_EOF_DELI_SHIFT                                        0
/* abort_ppdu_filter_out count */
#define     DBG_SR1_ABORT_PPDU_FILTER_OUT_MASK                                  0xff00
#define     DBG_SR1_ABORT_PPDU_FILTER_OUT_SHIFT                                      8
/* sr_payload_abort_ppdu count */
#define     DBG_SR1_SR_PAYLOAD_ABORT_PPDU_MASK                                0xff0000
#define     DBG_SR1_SR_PAYLOAD_ABORT_PPDU_SHIFT                                     16
/* sr_head_abort_ppdu count */
#define     DBG_SR1_SR_HEAD_ABORT_PPDU_MASK                                 0xff000000
#define     DBG_SR1_SR_HEAD_ABORT_PPDU_SHIFT                                        24

#define REG_DBG_LAT_NAV0                                                         0x7F8
/* vif0_last_inbss_nav_lat */
#define     DBG_LAT_NAV0_VIF0_LAST_INBSS_NAV_LAT_MASK                           0xffff
#define     DBG_LAT_NAV0_VIF0_LAST_INBSS_NAV_LAT_SHIFT                               0
/* vif0_last_nav_lat */
#define     DBG_LAT_NAV0_VIF0_LAST_NAV_LAT_MASK                             0xffff0000
#define     DBG_LAT_NAV0_VIF0_LAST_NAV_LAT_SHIFT                                    16

#define REG_DBG_LAT_NAV1                                                         0x7FC
/* vif1_last_inbss_nav_lat */
#define     DBG_LAT_NAV1_VIF1_LAST_INBSS_NAV_LAT_MASK                           0xffff
#define     DBG_LAT_NAV1_VIF1_LAST_INBSS_NAV_LAT_SHIFT                               0
/* vif1_last_nav_lat */
#define     DBG_LAT_NAV1_VIF1_LAST_NAV_LAT_MASK                             0xffff0000
#define     DBG_LAT_NAV1_VIF1_LAST_NAV_LAT_SHIFT                                    16

#define REG_DBG_STATE_CFG                                                        0x800
/* state timeout counter, if configure to 0, disable state timeout */
/* unit is ms */
#define     DBG_STATE_CFG_STATE_TO_CNT_MASK                                       0xff
#define     DBG_STATE_CFG_STATE_TO_CNT_SHIFT                                         0
/* select which que to minitor in mac_sys_debug */
/* 0~9 to select que0 to que9 */
/* 10 to select rx pointer interrupt */
#define     DBG_STATE_CFG_QUE_DEBUG_MONITOR_SEL_MASK                             0xf00
#define     DBG_STATE_CFG_QUE_DEBUG_MONITOR_SEL_SHIFT                                8

#define REG_DBG_STATE_CHK                                                        0x804
/* select the timeout counter apply for which state */
/* bit0 ~ bit9 : for que 0 to que 9 in tx state */
/* bit10 : txe fsm not in idle state */
/* bit11 : txe mtb not in idle state */
/* bit12 : txe hetb not in idle state */
/* bit13 : txe bfm not in idle state */
/* bit14 : rxe fsm not in idle state */
/* bit15 : rxe hetb_vec_state not in idle state */
/* bit16 : rxe frm not in idle state */
/* bit17 : rxe buf not in idle state */
/* bit18 : rxe buf_db not in idle state */
/* bit19 : MCU state not in idle state */
/* bit20 : MCU sounding state not in idle state */
/* bit21 : dma bfi state is not in idle state */
/* bit22 : dma fbo state is not in idle state */
#define     DBG_STATE_CHK_STATE_CHK_BIT_EN_MASK                             0xffffffff
#define     DBG_STATE_CHK_STATE_CHK_BIT_EN_SHIFT                                     0

#define REG_DBG_LAST_TX_LEN                                                      0x808
/* last tx psdu length */
#define     DBG_LAST_TX_LEN_LAST_TX_LEN_MASK                                   0xfffff
#define     DBG_LAST_TX_LEN_LAST_TX_LEN_SHIFT                                        0

#define REG_DBG_TXDMA_DELAY                                                      0x80C
/* txdma_max_delay, unit is 8 clock cycles */
#define     DBG_TXDMA_DELAY_TXDMA_MAX_DELAY_MASK                                0xffff
#define     DBG_TXDMA_DELAY_TXDMA_MAX_DELAY_SHIFT                                    0
/* txdma_average_delay, unit is 8 clock cycles */
#define     DBG_TXDMA_DELAY_TXDMA_AVE_DELAY_MASK                            0xffff0000
#define     DBG_TXDMA_DELAY_TXDMA_AVE_DELAY_SHIFT                                   16

#define REG_DBG_LAST_CBF                                                         0x810
/* Average SNR of Space-Time Stream 1 */
#define     DBG_LAST_CBF_SNR0_MASK                                                0xff
#define     DBG_LAST_CBF_SNR0_SHIFT                                                  0
/* Average SNR of Space-Time Stream 2 */
#define     DBG_LAST_CBF_SNR1_MASK                                              0xff00
#define     DBG_LAST_CBF_SNR1_SHIFT                                                  8
/* Number of columns (set to Nc - 1) */
#define     DBG_LAST_CBF_NC_INDEX_MASK                                         0x10000
#define     DBG_LAST_CBF_NC_INDEX_SHIFT                                             16
/* Number of rows (set to Nr - 1) */
#define     DBG_LAST_CBF_NR_INDEX_MASK                                         0xe0000
#define     DBG_LAST_CBF_NR_INDEX_SHIFT                                             17
/* Channel width */
/* 0 : 20 MHz, 1 : 40 MHz, 2 : 80 MHz */
#define     DBG_LAST_CBF_BW_MASK                                              0x300000
#define     DBG_LAST_CBF_BW_SHIFT                                                   20
/* Number of rows (set to Nr - 1) */
#define     DBG_LAST_CBF_FEEDBACK_TYPE_MASK                                   0xc00000
#define     DBG_LAST_CBF_FEEDBACK_TYPE_SHIFT                                        22
/* Sounding Dialog Token Number field */
/* in the corresponding NDPA frame */
#define     DBG_LAST_CBF_DIALOG_TOKEN_NUMBER_MASK                           0x3f000000
#define     DBG_LAST_CBF_DIALOG_TOKEN_NUMBER_SHIFT                                  24
/* 0 : HT, 1 : VHT, 2 : HE non-TB, 3 : HE TB */
#define     DBG_LAST_CBF_MODE_MASK                                          0xc0000000
#define     DBG_LAST_CBF_MODE_SHIFT                                                 30
#endif /*_MACREGS_H_*/