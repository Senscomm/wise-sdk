#ifndef _PHYREGS_H_
#define _PHYREGS_H_
/*
 * Registers for PHY top
 */

#define FW_PWR_EN                                           0x0000
/* 1'b 1 : PHY power ON & Clock enable */
#define     FW_PWR_EN_MASK                                     0x1
#define     FW_PWR_EN_SHIFT                                      0
/* 1'b 1 : TX clock gating ON */
#define     PHY_TX_CLOCK_GATING_ON_MASK                       0x10
#define     PHY_TX_CLOCK_GATING_ON_SHIFT                         4
/* 1'b 1 : RX clock gating ON */
#define     PHY_RX_CLOCK_GATING_ON_MASK                       0x20
#define     PHY_RX_CLOCK_GATING_ON_SHIFT                         5
/* 1'b 1 : OFDM clock gating ON */
#define     PHY_OFDM_CLOCK_GATING_ON_MASK                     0x40
#define     PHY_OFDM_CLOCK_GATING_ON_SHIFT                       6
/* 1'b 1 : OFDM TX clock gating ON */
#define     PHY_OFDMTX_CLOCK_GATING_ON_MASK                   0x80
#define     PHY_OFDMTX_CLOCK_GATING_ON_SHIFT                     7
/* 1'b 1 : OFDM RX clock gating ON */
#define     PHY_OFDMRX_CLOCK_GATING_ON_MASK                  0x100
#define     PHY_OFDMRX_CLOCK_GATING_ON_SHIFT                     8
/* 1'b 1 : RX LDPC clock gating ON */
#define     PHY_LDPCRX_CLOCK_GATING_ON_MASK                  0x200
#define     PHY_LDPCRX_CLOCK_GATING_ON_SHIFT                     9
/* 1'b 1 : B11 clock gating ON */
#define     PHY_B11_CLOCK_GATING_ON_MASK                     0x400
#define     PHY_B11_CLOCK_GATING_ON_SHIFT                       10
/* 1'b 1 : B11 TX clock gating ON */
#define     PHY_B11TX_CLOCK_GATING_ON_MASK                   0x800
#define     PHY_B11TX_CLOCK_GATING_ON_SHIFT                     11
/* 1'b 1 : B11 RX clock gating ON */
#define     PHY_B11RX_CLOCK_GATING_ON_MASK                  0x1000
#define     PHY_B11RX_CLOCK_GATING_ON_SHIFT                     12
/* [1] enable manual setting [0] manual setting val */
#define     PHY_CLKMODE_TX_MANSET_MASK                  0x30000000
#define     PHY_CLKMODE_TX_MANSET_SHIFT                         28

#define FW_OPBW_PRICH_VINTF0                                0x0004
/* virtual interface 0 */
/* Operating BW (0 : 20MHz, 1: 40MHz) */
#define     FW_OPBW_VINTF0_MASK                                0x1
#define     FW_OPBW_VINTF0_SHIFT                                 0
/* virtual interface 0 */
/* The position of the primary 20MHz channel (0~3) */
#define     FW_PRICH_VINTF0_MASK                              0x30
#define     FW_PRICH_VINTF0_SHIFT                                4
/* virtual interface 0 */
/* 0 : The position of the primary 20MHz channel is on the primary 80MHz channel */
/* 1 : The position of the primary 20MHz channel is on the secondary 80MHz channel */
#define     FW_PRI20_ON_SEC80_VINTF0_MASK                    0x100
#define     FW_PRI20_ON_SEC80_VINTF0_SHIFT                       8

#define FW_OPBW_PRICH_VINTF1                                0x0008
/* virtual interface 1 */
/* Operating BW (0 : 20MHz, 1: 40MHz) */
#define     FW_OPBW_VINTF1_MASK                                0x1
#define     FW_OPBW_VINTF1_SHIFT                                 0
/* virtual interface 1 */
/* The position of the primary 20MHz channel (0~3) */
#define     FW_PRICH_VINTF1_MASK                              0x30
#define     FW_PRICH_VINTF1_SHIFT                                4
/* virtual interface 1 */
/* 0 : The position of the primary 20MHz channel is on the primary 80MHz channel */
/* 1 : The position of the primary 20MHz channel is on the secondary 80MHz channel */
#define     FW_PRI20_ON_SEC80_VINTF1_MASK                    0x100
#define     FW_PRI20_ON_SEC80_VINTF1_SHIFT                       8

#define FW_PHY_SW_RESET                                     0x000C
/* S/W reset for PHY */
#define     FW_PHY_SWRST_MASK                                  0x1
#define     FW_PHY_SWRST_SHIFT                                   0

#define FW_OFDM_ON                                          0x0010
/* OFDM ON. If this value is 0, clocks for OFDM are turned off */
#define     FW_OFDM_ON_MASK                                    0x1
#define     FW_OFDM_ON_SHIFT                                     0

#define FW_SIGEXT_VINTF0                                    0x0018
/* virtual interface 0 */
/* It indicates that the channel is 2.4GHz band */
/* Signal extension (0 : OFF, 1 : ON) */
#define     FW_SIGEXT_VINTF0_MASK                              0x1
#define     FW_SIGEXT_VINTF0_SHIFT                               0

#define FW_SIGEXT_VINTF1                                    0x001C
/* virtual interface 1 */
/* It indicates that the channel is 2.4GHz band */
/* Signal extension (0 : OFF, 1 : ON) */
#define     FW_SIGEXT_VINTF1_MASK                              0x1
#define     FW_SIGEXT_VINTF1_SHIFT                               0

#define PHY_LOOPBACK_ABCROSS                                0x0020
/* Cross loopback */
#define     PHY_AB_CROSS_LOOPBACK_RXEN_MASK                    0x1
#define     PHY_AB_CROSS_LOOPBACK_RXEN_SHIFT                     0

#define PHY_TXWAIT1                                         0x0024
/* The wait time to align the timing that a packet is transmitted regardless of OPBW and TX_CHBW */
#define     PHY_TXWAIT_OP20_CB20_MASK                         0x3f
#define     PHY_TXWAIT_OP20_CB20_SHIFT                           0
/* The wait time to align the timing that a packet is transmitted regardless of OPBW and TX_CHBW */
#define     PHY_TXWAIT_OP40_CB20_MASK                        0xfc0
#define     PHY_TXWAIT_OP40_CB20_SHIFT                           6
/* The wait time to align the timing that a packet is transmitted regardless of OPBW and TX_CHBW */
#define     PHY_TXWAIT_OP40_CB40_MASK                      0x3f000
#define     PHY_TXWAIT_OP40_CB40_SHIFT                          12

#define PHY_TXWAIT2                                         0x0028
/* The wait time to align the timing that a packet is transmitted regardless of OPBW and TX_CHBW */
#define     PHY_TXWAIT_OP80_CB20_MASK                         0x3f
#define     PHY_TXWAIT_OP80_CB20_SHIFT                           0
/* The wait time to align the timing that a packet is transmitted regardless of OPBW and TX_CHBW */
#define     PHY_TXWAIT_OP80_CB40_MASK                        0xfc0
#define     PHY_TXWAIT_OP80_CB40_SHIFT                           6
/* The wait time to align the timing that a packet is transmitted regardless of OPBW and TX_CHBW */
#define     PHY_TXWAIT_OP80_CB80_MASK                      0x3f000
#define     PHY_TXWAIT_OP80_CB80_SHIFT                          12

#define PHY_TRXWAIT                                         0x002C
/* The wait time to align the timing of rx_td_end regardless of OPBW */
#define     PHY_RXWAIT_OP20_MASK                              0x3f
#define     PHY_RXWAIT_OP20_SHIFT                                0
/* The wait time to align the timing of rx_td_end regardless of OPBW */
#define     PHY_RXWAIT_OP40_MASK                             0xfc0
#define     PHY_RXWAIT_OP40_SHIFT                                6
/* The wait time to align the timing of rx_td_end regardless of OPBW */
#define     PHY_RXWAIT_OP80_MASK                           0x3f000
#define     PHY_RXWAIT_OP80_SHIFT                               12
/* The wait time for 11B to align the timing that a packet is transmitted regardless of OPBW and TX_CHBW */
#define     PHY_TXWAIT_OP20_CB20_B11_MASK                 0xfc0000
#define     PHY_TXWAIT_OP20_CB20_B11_SHIFT                      18
/* The wait time for 11B to align the timing that a packet is transmitted regardless of OPBW and TX_CHBW */
#define     PHY_TXWAIT_OP40_CB20_B11_MASK               0x3f000000
#define     PHY_TXWAIT_OP40_CB20_B11_SHIFT                      24

#define PHY_CONFIG0                                         0x0034
/* The same scrambler seed ON */
/* It is used for debugging */
/* If not debugging purpose, should be set to 0 */
#define     PHY_SAME_SCRMB_SEED_MASK                           0x1
#define     PHY_SAME_SCRMB_SEED_SHIFT                            0

#define PHY_CONFIG1                                         0x0038
/* Turn on OFDM RX */
#define     PHY_OFDM_RX_ON_MASK                                0x1
#define     PHY_OFDM_RX_ON_SHIFT                                 0

#define FW_SEL_VINTF                                        0x003C
/* Select a virtual interface */
/* 0 : virtual interface 0 */
/* 1 : virtual interface 1 */
#define     FW_SEL_VINTF_MASK                                  0x1
#define     FW_SEL_VINTF_SHIFT                                   0

#define FW_VHT_CONFIG0_VINTF0                               0x0040
/* virtual interface 0 */
/* VHT filtered out ON */
#define     FW_VHT_FLTRDO_ON_VINTF0_MASK                       0x1
#define     FW_VHT_FLTRDO_ON_VINTF0_SHIFT                        0
/* virtual interface 0 */
/* VHT LISTEN_TO_GID00 */
#define     FW_VHT_LISTEN_TO_GID00_VINTF0_MASK                 0x2
#define     FW_VHT_LISTEN_TO_GID00_VINTF0_SHIFT                  1
/* virtual interface 0 */
/* VHT LISTEN_TO_GID63 */
#define     FW_VHT_LISTEN_TO_GID63_VINTF0_MASK                 0x4
#define     FW_VHT_LISTEN_TO_GID63_VINTF0_SHIFT                  2

#define FW_VHT_CONFIG0_VINTF1                               0x0044
/* virtual interface 1 */
/* VHT filtered out ON */
#define     FW_VHT_FLTRDO_ON_VINTF1_MASK                       0x1
#define     FW_VHT_FLTRDO_ON_VINTF1_SHIFT                        0
/* virtual interface 1 */
/* VHT LISTEN_TO_GID00 */
#define     FW_VHT_LISTEN_TO_GID00_VINTF1_MASK                 0x2
#define     FW_VHT_LISTEN_TO_GID00_VINTF1_SHIFT                  1
/* virtual interface 1 */
/* VHT LISTEN_TO_GID63 */
#define     FW_VHT_LISTEN_TO_GID63_VINTF1_MASK                 0x4
#define     FW_VHT_LISTEN_TO_GID63_VINTF1_SHIFT                  2

#define FW_VHT_CONFIG1_VINTF0                               0x0048
/* virtual interface 0 */
/* VHT PARTIAL_AID_LIST_GID00 */
#define     FW_VHT_PARTIAL_AID_LIST_GID00_VINTF0_MASK        0x1ff
#define     FW_VHT_PARTIAL_AID_LIST_GID00_VINTF0_SHIFT           0

#define FW_VHT_CONFIG1_VINTF1                               0x004C
/* virtual interface 1 */
/* VHT PARTIAL_AID_LIST_GID00 */
#define     FW_VHT_PARTIAL_AID_LIST_GID00_VINTF1_MASK        0x1ff
#define     FW_VHT_PARTIAL_AID_LIST_GID00_VINTF1_SHIFT           0

#define FW_VHT_CONFIG2_VINTF0                               0x0050
/* virtual interface 0 */
/* VHT PARTIAL_AID_LIST_GID63 */
#define     FW_VHT_PARTIAL_AID_LIST_GID63_VINTF0_MASK        0x1ff
#define     FW_VHT_PARTIAL_AID_LIST_GID63_VINTF0_SHIFT           0

#define FW_VHT_CONFIG2_VINTF1                               0x0054
/* virtual interface 1 */
/* VHT PARTIAL_AID_LIST_GID63 */
#define     FW_VHT_PARTIAL_AID_LIST_GID63_VINTF1_MASK        0x1ff
#define     FW_VHT_PARTIAL_AID_LIST_GID63_VINTF1_SHIFT           0

#define FW_VHT_CONFIG3_VINTF0                               0x0058
/* virtual interface 0 */
/* VHT Membership Status Array[31:0] */
#define     FW_VHT_ARRAY_MEMBERSHIP_STATUS0_VINTF0_MASK 0xffffffff
#define     FW_VHT_ARRAY_MEMBERSHIP_STATUS0_VINTF0_SHIFT         0

#define FW_VHT_CONFIG3_VINTF1                               0x005C
/* virtual interface 1 */
/* VHT Membership Status Array[31:0] */
#define     FW_VHT_ARRAY_MEMBERSHIP_STATUS0_VINTF1_MASK 0xffffffff
#define     FW_VHT_ARRAY_MEMBERSHIP_STATUS0_VINTF1_SHIFT         0

#define FW_VHT_CONFIG4_VINTF0                               0x0060
/* virtual interface 0 */
/* VHT Membership Status Array[63:32] */
#define     FW_VHT_ARRAY_MEMBERSHIP_STATUS1_VINTF0_MASK 0xffffffff
#define     FW_VHT_ARRAY_MEMBERSHIP_STATUS1_VINTF0_SHIFT         0

#define FW_VHT_CONFIG4_VINTF1                               0x0064
/* virtual interface 1 */
/* VHT Membership Status Array[63:32] */
#define     FW_VHT_ARRAY_MEMBERSHIP_STATUS1_VINTF1_MASK 0xffffffff
#define     FW_VHT_ARRAY_MEMBERSHIP_STATUS1_VINTF1_SHIFT         0

#define FW_VHT_CONFIG5_VINTF0                               0x0068
/* virtual interface 0 */
/* VHT User Position Array[31:0] */
#define     FW_VHT_ARRAY_USERPOS0_VINTF0_MASK           0xffffffff
#define     FW_VHT_ARRAY_USERPOS0_VINTF0_SHIFT                   0

#define FW_VHT_CONFIG5_VINTF1                               0x006C
/* virtual interface 1 */
/* VHT User Position Array[31:0] */
#define     FW_VHT_ARRAY_USERPOS0_VINTF1_MASK           0xffffffff
#define     FW_VHT_ARRAY_USERPOS0_VINTF1_SHIFT                   0

#define FW_VHT_CONFIG6_VINTF0                               0x0070
/* virtual interface 0 */
/* VHT User Position Array[63:32] */
#define     FW_VHT_ARRAY_USERPOS1_VINTF0_MASK           0xffffffff
#define     FW_VHT_ARRAY_USERPOS1_VINTF0_SHIFT                   0

#define FW_VHT_CONFIG6_VINTF1                               0x0074
/* virtual interface 1 */
/* VHT User Position Array[63:32] */
#define     FW_VHT_ARRAY_USERPOS1_VINTF1_MASK           0xffffffff
#define     FW_VHT_ARRAY_USERPOS1_VINTF1_SHIFT                   0

#define FW_VHT_CONFIG7_VINTF0                               0x0078
/* virtual interface 0 */
/* VHT User Position Array[95:64] */
#define     FW_VHT_ARRAY_USERPOS2_VINTF0_MASK           0xffffffff
#define     FW_VHT_ARRAY_USERPOS2_VINTF0_SHIFT                   0

#define FW_VHT_CONFIG7_VINTF1                               0x007C
/* virtual interface 1 */
/* VHT User Position Array[95:64] */
#define     FW_VHT_ARRAY_USERPOS2_VINTF1_MASK           0xffffffff
#define     FW_VHT_ARRAY_USERPOS2_VINTF1_SHIFT                   0

#define FW_VHT_CONFIG8_VINTF0                               0x0080
/* virtual interface 0 */
/* VHT User Position Array[127:96] */
#define     FW_VHT_ARRAY_USERPOS3_VINTF0_MASK           0xffffffff
#define     FW_VHT_ARRAY_USERPOS3_VINTF0_SHIFT                   0

#define FW_VHT_CONFIG8_VINTF1                               0x0084
/* virtual interface 1 */
/* VHT User Position Array[127:96] */
#define     FW_VHT_ARRAY_USERPOS3_VINTF1_MASK           0xffffffff
#define     FW_VHT_ARRAY_USERPOS3_VINTF1_SHIFT                   0

#define PHY_HE_CONFIG0                                      0x0090
/* TX pre-compensation always ON */
/* 0 : HE TB PPDU or NONHT PPDU with TRIGGER_RESPONDING set to true compensate for CFO error and SFO error */
/* 1 : all PPDU compensates for CFO error and SFO error */
#define     PHY_TXCOMP_ALWAYS_ON_MASK                          0x1
#define     PHY_TXCOMP_ALWAYS_ON_SHIFT                           0
/* TX pre-compensation always OFF */
/* This field's priority is higher than phy_txcomp_always_on */
/* 0 : HE TB PPDU or NONHT PPDU with TRIGGER_RESPONDING set to true compensate for CFO error and SFO error */
/* 1 : TX pre-compensation OFF */
#define     PHY_TXCOMP_ALWAYS_OFF_MASK                        0x10
#define     PHY_TXCOMP_ALWAYS_OFF_SHIFT                          4

#define FW_HE_CONFIG1_VINTF0                                0x0094
/* virtual interface 0 */
/* HE-SIGA filtered out ON */
/* 0 : All BSS Color can be received */
/* 1 : One specific BSS color which is set in fw_hesiga_bsscolor can be received */
#define     FW_HESIGA_FLTRDO_ON_VINTF0_MASK                    0x1
#define     FW_HESIGA_FLTRDO_ON_VINTF0_SHIFT                     0
/* virtual interface 0 */
/* HE-SIGA filtered out ON for UL PPDU */
/* 0 : PPDU which is addressed to an AP can be received */
/* 1 : PPDU which is addressed to an AP cannot be received */
#define     FW_HESIGA_UL_FLTRDO_ON_VINTF0_MASK                 0x2
#define     FW_HESIGA_UL_FLTRDO_ON_VINTF0_SHIFT                  1
/* virtual interface 0 */
/* HE-SIGA filtered out ON for DL PPDU */
/* 0 : PPDU which is addressed to a STA can be received */
/* 1 : PPDU which is addressed to a STA cannot be received */
#define     FW_HESIGA_DL_FLTRDO_ON_VINTF0_MASK                 0x4
#define     FW_HESIGA_DL_FLTRDO_ON_VINTF0_SHIFT                  2

#define FW_HE_CONFIG1_VINTF1                                0x0098
/* virtual interface 1 */
/* HE-SIGA filtered out ON */
/* 0 : All BSS Color can be received */
/* 1 : One specific BSS color which is set in fw_hesiga_bsscolor can be received */
#define     FW_HESIGA_FLTRDO_ON_VINTF1_MASK                    0x1
#define     FW_HESIGA_FLTRDO_ON_VINTF1_SHIFT                     0
/* virtual interface 1 */
/* HE-SIGA filtered out ON for UL PPDU */
/* 0 : PPDU which is addressed to an AP can be received */
/* 1 : PPDU which is addressed to an AP cannot be received */
#define     FW_HESIGA_UL_FLTRDO_ON_VINTF1_MASK                 0x2
#define     FW_HESIGA_UL_FLTRDO_ON_VINTF1_SHIFT                  1
/* virtual interface 1 */
/* HE-SIGA filtered out ON for DL PPDU */
/* 0 : PPDU which is addressed to a STA can be received */
/* 1 : PPDU which is addressed to a STA cannot be received */
#define     FW_HESIGA_DL_FLTRDO_ON_VINTF1_MASK                 0x4
#define     FW_HESIGA_DL_FLTRDO_ON_VINTF1_SHIFT                  2

#define FW_HE_CONFIG2_VINTF0                                0x009C
/* virtual interface 0 */
/* BSS color */
#define     FW_HESIGA_BSSCOLOR_VINTF0_MASK                    0x3f
#define     FW_HESIGA_BSSCOLOR_VINTF0_SHIFT                      0
/* virtual interface 0 */
/* My STA ID */
#define     FW_HESIGB_MYSTAID_VINTF0_MASK                  0x1ffc0
#define     FW_HESIGB_MYSTAID_VINTF0_SHIFT                       6
/* virtual interface 0 */
/* the BSSID Index of the BSSID of the AP */
#define     FW_HESIGB_BSSIDX_VINTF0_MASK                 0xffe0000
#define     FW_HESIGB_BSSIDX_VINTF0_SHIFT                       17

#define FW_HE_CONFIG2_VINTF1                                0x00A0
/* virtual interface 1 */
/* BSS color */
#define     FW_HESIGA_BSSCOLOR_VINTF1_MASK                    0x3f
#define     FW_HESIGA_BSSCOLOR_VINTF1_SHIFT                      0
/* virtual interface 1 */
/* My STA ID */
#define     FW_HESIGB_MYSTAID_VINTF1_MASK                  0x1ffc0
#define     FW_HESIGB_MYSTAID_VINTF1_SHIFT                       6
/* virtual interface 1 */
/* the BSSID Index of the BSSID */
#define     FW_HESIGB_BSSIDX_VINTF1_MASK                 0xffe0000
#define     FW_HESIGB_BSSIDX_VINTF1_SHIFT                       17

#define PHY_LSIG_NOT_FILTERING                              0x00A4
/* [0] : */
#define     PHY_LSIG_NOT_FILTERING_MASK                 0xffffffff
#define     PHY_LSIG_NOT_FILTERING_SHIFT                         0

#define PHY_HTSIG_NOT_FILTERING                             0x00A8
/* [0] : */
#define     PHY_HTSIG_NOT_FILTERING_MASK                0xffffffff
#define     PHY_HTSIG_NOT_FILTERING_SHIFT                        0

#define PHY_VHTSIGA_NOT_FILTERING                           0x00AC
/* [0] : */
#define     PHY_VHTSIGA_NOT_FILTERING_MASK              0xffffffff
#define     PHY_VHTSIGA_NOT_FILTERING_SHIFT                      0

#define PHY_VHTSIGB_NOT_FILTERING                           0x00B0
/* [0] : */
#define     PHY_VHTSIGB_NOT_FILTERING_MASK              0xffffffff
#define     PHY_VHTSIGB_NOT_FILTERING_SHIFT                      0

#define PHY_HESIGA_NOT_FILTERING                            0x00B4
/* [0] : */
#define     PHY_HESIGA_NOT_FILTERING_MASK               0xffffffff
#define     PHY_HESIGA_NOT_FILTERING_SHIFT                       0

#define PHY_INVALID_NOT_FILTERING                           0x00B8
/* [0] : */
#define     PHY_INVALID_NOT_FILTERING_MASK              0xffffffff
#define     PHY_INVALID_NOT_FILTERING_SHIFT                      0

#define PHY_B11_TX_POWER_OFFSET                             0x00C0
/* TX power offset for 11b (The digital power of 11b is greater than OFDM) */
/* The range is -16 ~ 15 dB */
#define     PHY_B11_TX_POWER_OFFSET_MASK                      0x1f
#define     PHY_B11_TX_POWER_OFFSET_SHIFT                        0

#define PHY_MON_CNT_RST                                     0x00FC
/* reset for monitoring registers */
#define     PHY_MON_CNT_RST_MASK                               0x1
#define     PHY_MON_CNT_RST_SHIFT                                0

#define PHY_MON_00                                          0x0100
/* [15: 0] : OFDM TX error counter (including TX vec error, underflow) */
/* [31:16] : B11 TX error counter (including TX vec error, underflow) */
#define     PHY_MON_00_MASK                             0xffffffff
#define     PHY_MON_00_SHIFT                                     0

#define PHY_MON_01                                          0x0104
/* [ 9: 0] : TX FIFO underflow counter */
/* [19:10] : TX FDBUF underflow counter */
/* [29:20] : TX FIFO overflow counter */
#define     PHY_MON_01_MASK                             0xffffffff
#define     PHY_MON_01_SHIFT                                     0

#define PHY_MON_02                                          0x0108
/* CS SAT counter */
#define     PHY_MON_02_MASK                             0xffffffff
#define     PHY_MON_02_SHIFT                                     0

#define PHY_MON_03                                          0x010C
/* CS STF counter */
#define     PHY_MON_03_MASK                             0xffffffff
#define     PHY_MON_03_SHIFT                                     0

#define PHY_MON_04                                          0x0110
/* CS B11 counter */
#define     PHY_MON_04_MASK                             0xffffffff
#define     PHY_MON_04_SHIFT                                     0

#define PHY_MON_05                                          0x0114
/* CS PWR counter */
#define     PHY_MON_05_MASK                             0xffffffff
#define     PHY_MON_05_SHIFT                                     0

#define PHY_MON_06                                          0x0118
/* OFDM SYNC counter */
#define     PHY_MON_06_MASK                             0xffffffff
#define     PHY_MON_06_SHIFT                                     0

#define PHY_MON_07                                          0x011C
/* B11 DET counter */
#define     PHY_MON_07_MASK                             0xffffffff
#define     PHY_MON_07_SHIFT                                     0

#define PHY_MON_08                                          0x0120
/* NONHT DET counter */
#define     PHY_MON_08_MASK                             0xffffffff
#define     PHY_MON_08_SHIFT                                     0

#define PHY_MON_09                                          0x0124
/* HTGF DET counter */
#define     PHY_MON_09_MASK                             0xffffffff
#define     PHY_MON_09_SHIFT                                     0

#define PHY_MON_10                                          0x0128
/* HTMF DET counter */
#define     PHY_MON_10_MASK                             0xffffffff
#define     PHY_MON_10_SHIFT                                     0

#define PHY_MON_11                                          0x012C
/* VHT DET counter */
#define     PHY_MON_11_MASK                             0xffffffff
#define     PHY_MON_11_SHIFT                                     0

#define PHY_MON_12                                          0x0130
/* HESU DET counter */
#define     PHY_MON_12_MASK                             0xffffffff
#define     PHY_MON_12_SHIFT                                     0

#define PHY_MON_13                                          0x0134
/* HEER DET counter */
#define     PHY_MON_13_MASK                             0xffffffff
#define     PHY_MON_13_SHIFT                                     0

#define PHY_MON_14                                          0x0138
/* HEMU DET counter */
#define     PHY_MON_14_MASK                             0xffffffff
#define     PHY_MON_14_SHIFT                                     0

#define PHY_MON_15                                          0x013C
/* HETB DET counter */
#define     PHY_MON_15_MASK                             0xffffffff
#define     PHY_MON_15_SHIFT                                     0

#define PHY_MON_16                                          0x0140
/* REP DET counter */
#define     PHY_MON_16_MASK                             0xffffffff
#define     PHY_MON_16_SHIFT                                     0

#define PHY_MON_17                                          0x0144
/* LSIG OK counter */
#define     PHY_MON_17_MASK                             0xffffffff
#define     PHY_MON_17_SHIFT                                     0

#define PHY_MON_18                                          0x0148
/* RLSIG OK counter */
#define     PHY_MON_18_MASK                             0xffffffff
#define     PHY_MON_18_SHIFT                                     0

#define PHY_MON_19                                          0x014C
/* HTSIG OK counter */
#define     PHY_MON_19_MASK                             0xffffffff
#define     PHY_MON_19_SHIFT                                     0

#define PHY_MON_20                                          0x0150
/* VHTSIGA OK counter */
#define     PHY_MON_20_MASK                             0xffffffff
#define     PHY_MON_20_SHIFT                                     0

#define PHY_MON_21                                          0x0154
/* HESIGA OK counter */
#define     PHY_MON_21_MASK                             0xffffffff
#define     PHY_MON_21_SHIFT                                     0

#define PHY_MON_22                                          0x0158
/* Stronger packet DET counter */
#define     PHY_MON_22_MASK                             0xffffffff
#define     PHY_MON_22_SHIFT                                     0

#define PHY_MON_23                                          0x015C
/* Unexpected ERR counter */
#define     PHY_MON_23_MASK                             0xffffffff
#define     PHY_MON_23_SHIFT                                     0

#define PHY_MON_24                                          0x0160
/* [23:16] : GAIN DB */
/* [15: 8] : RSSI */
/* [ 7: 0] : RCPI */
#define     PHY_MON_24_MASK                             0xffffffff
#define     PHY_MON_24_SHIFT                                     0

#define PHY_MON_25                                          0x0164
/* [12: 0] : Estimated CFO value */
#define     PHY_MON_25_MASK                             0xffffffff
#define     PHY_MON_25_SHIFT                                     0

#define PHY_MON_26                                          0x0168
/* [31:18] : [13:0] of Received HTSIG bits */
/* [17: 0] : Received LSIG bits */
#define     PHY_MON_26_MASK                             0xffffffff
#define     PHY_MON_26_SHIFT                                     0

#define PHY_MON_27                                          0x016C
/* [27: 0] : [41:14] of Received HTSIG bits */
#define     PHY_MON_27_MASK                             0xffffffff
#define     PHY_MON_27_SHIFT                                     0

#define PHY_MON_28                                          0x0170
/* [31: 0] : [31:0] of Received VHTSIGA bits */
#define     PHY_MON_28_MASK                             0xffffffff
#define     PHY_MON_28_SHIFT                                     0

#define PHY_MON_29                                          0x0174
/* [31:10] : [21:0] of Received HESIGA bits */
/* [ 9: 0] : [42:32] of Received VHTSIGA bits */
#define     PHY_MON_29_MASK                             0xffffffff
#define     PHY_MON_29_SHIFT                                     0

#define PHY_MON_30                                          0x0178
/* [23: 0] : [45:22] of Received HESIGA bits */
#define     PHY_MON_30_MASK                             0xffffffff
#define     PHY_MON_30_SHIFT                                     0

#define PHY_MON_31                                          0x017C
/* [31: 0] : FCS OK counter */
#define     PHY_MON_31_MASK                             0xffffffff
#define     PHY_MON_31_SHIFT                                     0

#define PHY_MON_32                                          0x0180
/* [31: 0] : FD end counter */
#define     PHY_MON_32_MASK                             0xffffffff
#define     PHY_MON_32_SHIFT                                     0

#define PHY_MON_33                                          0x0184
/* [31: 0] : TX done counter */
#define     PHY_MON_33_MASK                             0xffffffff
#define     PHY_MON_33_SHIFT                                     0

#define PHY_MON_34                                          0x0188
/* [31: 0] : RX VEC EN counter */
#define     PHY_MON_34_MASK                             0xffffffff
#define     PHY_MON_34_SHIFT                                     0

#define PHY_MON_35                                          0x018C
/* [31: 0] : B11 CRC OK counter */
#define     PHY_MON_35_MASK                             0xffffffff
#define     PHY_MON_35_SHIFT                                     0

#define PHY_MON_36                                          0x0190
/* [31: 0] : Unsupported mode counter */
#define     PHY_MON_36_MASK                             0xffffffff
#define     PHY_MON_36_SHIFT                                     0

#define PHY_MON_37                                          0x0194
/* [31: 0] : RX SIG INFO 0 */
#define     PHY_MON_37_MASK                             0xffffffff
#define     PHY_MON_37_SHIFT                                     0

#define PHY_MON_38                                          0x0198
/* [31: 0] : RX SIG INFO 1 */
#define     PHY_MON_38_MASK                             0xffffffff
#define     PHY_MON_38_SHIFT                                     0

#define PHY_MON_39                                          0x019C
/* [31: 0] : RX SIG INFO 2 */
#define     PHY_MON_39_MASK                             0xffffffff
#define     PHY_MON_39_SHIFT                                     0

#define PHY_MON_40                                          0x01A0
/* [31: 0] : RX SIG INFO 3 */
#define     PHY_MON_40_MASK                             0xffffffff
#define     PHY_MON_40_SHIFT                                     0

#define PHY_MON_41                                          0x01A4
/* [31: 0] : RX SIG INFO 4 */
#define     PHY_MON_41_MASK                             0xffffffff
#define     PHY_MON_41_SHIFT                                     0

#define PHY_MON_42                                          0x01A8
/* [31: 0] : RX SIG INFO 5 */
#define     PHY_MON_42_MASK                             0xffffffff
#define     PHY_MON_42_SHIFT                                     0

#define PHY_MON_43                                          0x01AC
/* [31: 0] : RX SIG INFO 6 */
#define     PHY_MON_43_MASK                             0xffffffff
#define     PHY_MON_43_SHIFT                                     0

#define PHY_MON_44                                          0x01B0
/* [31: 0] : RX SIG INFO 7 */
#define     PHY_MON_44_MASK                             0xffffffff
#define     PHY_MON_44_SHIFT                                     0

#define PHY_MON_45                                          0x01B4
/* [31: 0] : RX SIG INFO 8 */
#define     PHY_MON_45_MASK                             0xffffffff
#define     PHY_MON_45_SHIFT                                     0

#define PHY_MON_46                                          0x01B8
/* [31: 0] : RX SIG INFO 9 */
#define     PHY_MON_46_MASK                             0xffffffff
#define     PHY_MON_46_SHIFT                                     0

#define PHY_MON_47                                          0x01BC
/* [31: 0] : RX SIG INFO 10 */
#define     PHY_MON_47_MASK                             0xffffffff
#define     PHY_MON_47_SHIFT                                     0

#define PHY_MON_48                                          0x01C0
/* [31: 0] : RX SIG INFO 11 */
#define     PHY_MON_48_MASK                             0xffffffff
#define     PHY_MON_48_SHIFT                                     0

#define PHY_MON_49                                          0x01C4
/* [31: 0] : RX SIG INFO 12 */
#define     PHY_MON_49_MASK                             0xffffffff
#define     PHY_MON_49_SHIFT                                     0

#define PHY_MON_50                                          0x01C8
/* [15: 0] : phy_swrst counter */
/* [32:16] : phy_swrst from MAC counter */
#define     PHY_MON_50_MASK                             0xffffffff
#define     PHY_MON_50_SHIFT                                     0

#define PHY_MON_51                                          0x01CC
/* [31: 0] : RESERVED */
#define     PHY_MON_51_MASK                             0xffffffff
#define     PHY_MON_51_SHIFT                                     0

#define PHY_MON_52                                          0x01D0
/* [31: 0] : RESERVED */
#define     PHY_MON_52_MASK                             0xffffffff
#define     PHY_MON_52_SHIFT                                     0

#define PHY_MON_53                                          0x01D4
/* [31: 0] : RESERVED */
#define     PHY_MON_53_MASK                             0xffffffff
#define     PHY_MON_53_SHIFT                                     0

#define PHY_MON_54                                          0x01D8
/* [31: 0] : RESERVED */
#define     PHY_MON_54_MASK                             0xffffffff
#define     PHY_MON_54_SHIFT                                     0

#define PHY_MON_55                                          0x01DC
/* [31: 0] : RESERVED */
#define     PHY_MON_55_MASK                             0xffffffff
#define     PHY_MON_55_SHIFT                                     0

#define PHY_MON_56                                          0x01E0
/* [31: 0] : RESERVED */
#define     PHY_MON_56_MASK                             0xffffffff
#define     PHY_MON_56_SHIFT                                     0

#define PHY_MON_57                                          0x01E4
/* [31: 0] : RESERVED */
#define     PHY_MON_57_MASK                             0xffffffff
#define     PHY_MON_57_SHIFT                                     0

#define PHY_MON_58                                          0x01E8
/* [31: 0] : RESERVED */
#define     PHY_MON_58_MASK                             0xffffffff
#define     PHY_MON_58_SHIFT                                     0

#define PHY_MON_59                                          0x01EC
/* [31: 0] : RESERVED */
#define     PHY_MON_59_MASK                             0xffffffff
#define     PHY_MON_59_SHIFT                                     0

#define PHY_DUMP_SIG_SEL                                    0x01F0
/* Select the signal to dump */
/* Corresponds to [9:0] out of 32bits */
/* 0 : phy_o4x_txd_i */
/* 1 : rsm_o4x_txd_i */
/* 2 : lp2_o4x_txd_i */
/* 3 : ftb_c4x_txd_i */
/* 4 : rfi_o2x_phyin_i */
/* 5 : rsm_o4x_rxd_i */
/* 6 : lp2_o4x_rxd_i */
/* 10 : llr_out */
/* 11 : bf_snr */
/* 12 : H matrix when receiving NDP */
/* 13 : phy_trx_info */
/* 15 : ofdm_td_fsm[9:7], b11_rx_fsm[6:3], b11_tx_fsm[2:0] */
#define     PHY_DUMP_SIG_SEL0_MASK                             0xf
#define     PHY_DUMP_SIG_SEL0_SHIFT                              0
/* Select the signal to dump */
/* Corresponds to [19:10] out of 32bits */
/* 0 : phy_o4x_txd_q */
/* 1 : rsm_o4x_txd_q */
/* 2 : lp2_o4x_txd_q */
/* 3 : ftb_c4x_txd_q */
/* 4 : rfi_o2x_phyin_q */
/* 5 : rsm_o4x_rxd_q */
/* 6 : lp2_o4x_rxd_q */
/* 7 : ofdm_td_fsm[19:16], phy_top_fsm[15:10] */
/* 8 : rx_cca_mode0[16:14], rx_cca_mode2[13:10] */
/* 9 : rxgain_db[16:10] */
/* 10 : llr_out */
/* 11 : bf_snr */
/* 12 : H matrix when receiving NDP */
/* 13 : phy_trx_info */
/* 15 : cca_fsm[19:16], ofdm_rx_fsm[15:11], ofdm_td_fsm[10] */
#define     PHY_DUMP_SIG_SEL1_MASK                            0xf0
#define     PHY_DUMP_SIG_SEL1_SHIFT                              4
/* Select the signal to dump */
/* Corresponds to [31:20] out of 32bits */
/* 0 : ofdm_td_fsm[29:26], phy_top_fsm[25:20] */
/* 1 : agc_fsm[30:26], phy_top_fsm[25:20] */
/* 7 : ofdm_tx_fsm[29:25], ofdm_rx_fsm[24:20] */
/* 8 : cca_fsm[30:27], b11_tx_fsm[26:24], b11_rx_fsm[23:20] */
/* 9 : cca_fsm[29:26], phy_top_fsm[25:20] */
/* 10 : llr_out */
/* 11 : bf_snr */
/* 12 : H matrix when receiving NDP */
/* 13 : phy_trx_info */
/* 15 : ofdm_tx_fsm[30:26], phy_top_fsm[25:20] */
#define     PHY_DUMP_SIG_SEL2_MASK                           0xf00
#define     PHY_DUMP_SIG_SEL2_SHIFT                              8
/* Select the state to dump */
/* 0 : ofdm_td_fsm[9:6], phy_top_fsm[5:0] */
/* 1 : cca_fsm[13:10], ofdm_td_fsm[9:6], phy_top_fsm[5:0] */
/* 2 : ofdm_tx_fsm[10:6], phy_top_fsm[5:0] */
/* 3 : ofdm_rx_fsm[10:6], phy_top_fsm[5:0] */
/* 4 : b11_tx_fsm[8:6], phy_top_fsm[5:0] */
/* 5 : b11_rx_fsm[9:6], phy_top_fsm[5:0] */
/* 6 : cca_fsm[13:10], b11_rx_fsm[9:6], phy_top_fsm[5:0] */
/* 7 : phy_top_fsm[5:0] */
/* 15 : b11_rx_fsm[30:27], b11_tx_fsm[26:24], ofdm_tx_fsm[23:19], cca_fsm[18:15], ofdm_rx_fsm[14:10], ofdm_td_fsm[9:6], phy_top_fsm[5:0] */
#define     PHY_DUMP_STATE_SEL_MASK                         0xf000
#define     PHY_DUMP_STATE_SEL_SHIFT                            12

#define PHY_FD_DUMP_SIG_SEL                                 0x01F4
/* Select the signal in frequency domain to dump */
/* 0 : llr */
/* 1 : ditlv */
/* 2 : viterbi[31:0] */
/* 3 : viterbi[63:32] */
/* 4 : viterbi[93:64] */
/* 5 : dscrm */
/* 6 : tone demapper[31:0] */
/* 7 : tone demapper[63:32] */
/* 8 : tone demapper[95:64] */
/* 9 : tone demapper[99:96] */
/* 10 : ldpc */
#define     PHY_FD_DUMP_SIG_SEL_MASK                           0xf
#define     PHY_FD_DUMP_SIG_SEL_SHIFT                            0

#define PHY_SELF_TX_CONFIG0                                 0x0200
/* reset for self TX */
#define     PHY_SELF_TX_RST_MASK                               0x1
#define     PHY_SELF_TX_RST_SHIFT                                0
/* self TX start */
/* If phy_self_tx_cont is 0, then just one packet is transmitted */
/* If phy_self_tx_cont is 1, then the same packets are transmitted continuously */
#define     PHY_SELF_TX_START_MASK                             0x2
#define     PHY_SELF_TX_START_SHIFT                              1

#define PHY_SELF_TX_CONFIG1                                 0x0204
/* self TX mode ON */
/* Turn on this before starting self TX */
#define     PHY_SELF_TX_ON_MASK                                0x1
#define     PHY_SELF_TX_ON_SHIFT                                 0
/* 0 : Transmit as many packets as set */
/* 1 : Transmit the same packet continuously */
#define     PHY_SELF_TX_CONT_MASK                              0x2
#define     PHY_SELF_TX_CONT_SHIFT                               1
/* Data type */
/* 0 : all 0's */
/* 1 : all 1's */
/* 2 : increment value */
#define     PHY_SELF_TX_DATATYPE_MASK                          0xc
#define     PHY_SELF_TX_DATATYPE_SHIFT                           2
/* 0 : OFDM */
/* 1 : B11 */
#define     PHY_SELF_TX_PHYTYPE_MASK                          0x10
#define     PHY_SELF_TX_PHYTYPE_SHIFT                            4
/* self TX response mode */
/* When any packet is received, It transmits a packet after interval time. */
#define     PHY_SELF_TX_RSP_MODE_MASK                         0x20
#define     PHY_SELF_TX_RSP_MODE_SHIFT                           5
/* Packet interval (us) */
#define     PHY_SELF_TX_ITVL_MASK                           0xffc0
#define     PHY_SELF_TX_ITVL_SHIFT                               6
/* the number of packets to be transmitted */
#define     PHY_SELF_TX_PKT_NUM_MASK                    0xffff0000
#define     PHY_SELF_TX_PKT_NUM_SHIFT                           16

#define PHY_SELF_TX_CONFIG2                                 0x0208
/* TX vector[31:0] */
#define     PHY_SELF_TX_VEC0_MASK                       0xffffffff
#define     PHY_SELF_TX_VEC0_SHIFT                               0

#define PHY_SELF_TX_CONFIG3                                 0x020C
/* TX vector[63:32] */
#define     PHY_SELF_TX_VEC1_MASK                       0xffffffff
#define     PHY_SELF_TX_VEC1_SHIFT                               0

#define PHY_SELF_TX_CONFIG4                                 0x0210
/* TX vector[95:64] */
#define     PHY_SELF_TX_VEC2_MASK                       0xffffffff
#define     PHY_SELF_TX_VEC2_SHIFT                               0

#define PHY_SELF_TX_CONFIG5                                 0x0214
/* TX vector[127:96] */
#define     PHY_SELF_TX_VEC3_MASK                       0xffffffff
#define     PHY_SELF_TX_VEC3_SHIFT                               0

#define PHY_SELF_TX_CONFIG6                                 0x0218
/* TX vector[159:128] */
#define     PHY_SELF_TX_VEC4_MASK                       0xffffffff
#define     PHY_SELF_TX_VEC4_SHIFT                               0

#define PHY_SELF_TX_CONFIG7                                 0x021C
/* TX vector[191:160] */
#define     PHY_SELF_TX_VEC5_MASK                       0xffffffff
#define     PHY_SELF_TX_VEC5_SHIFT                               0

#define PHY_SELF_TX_CONFIG8                                 0x0220
/* TX vector[223:192] */
#define     PHY_SELF_TX_VEC6_MASK                       0xffffffff
#define     PHY_SELF_TX_VEC6_SHIFT                               0

#define PHY_DUMP_MODE                                       0x022C
/* phy dump mode */
#define     PHY_DUMP_MODE_MASK                                 0x7
#define     PHY_DUMP_MODE_SHIFT                                  0

#define PHY_DUMP_FLT0                                       0x0230
/* phy trx dump mode selection */
/* 0 : TX */
/* 1 : RX */
/* 2 : RXED */
#define     PHY_DUMP_FLT_SEL_MASK                              0xf
#define     PHY_DUMP_FLT_SEL_SHIFT                               0
/* tx pkt format */
/* 0 : NONHT */
/* 1 : HTMF */
/* 2 : HTGF */
/* 3 : VHT */
/* 4 : HESU */
/* 5 : HEER */
/* 6 : HEMU */
/* 7 : HETB */
/* 8 : B11 */
#define     PHY_DUMP_FLT_TX_FORMAT_MASK                     0x1ff0
#define     PHY_DUMP_FLT_TX_FORMAT_SHIFT                         4
/* rx pkt format */
/* 0 : NONHT */
/* 1 : HTMF */
/* 2 : HTGF */
/* 3 : VHT */
/* 4 : HESU */
/* 5 : HEER */
/* 6 : HEMU */
/* 7 : HETB */
/* 8 : B11 */
#define     PHY_DUMP_FLT_RX_FORMAT_MASK                  0x1ff0000
#define     PHY_DUMP_FLT_RX_FORMAT_SHIFT                        16

#define PHY_DUMP_FLT1                                       0x0234
/* min length */
#define     PHY_DUMP_FLT_MIN_LENGTH_MASK                   0xfffff
#define     PHY_DUMP_FLT_MIN_LENGTH_SHIFT                        0

#define PHY_DUMP_FLT2                                       0x0238
/* max length */
#define     PHY_DUMP_FLT_MAX_LENGTH_MASK                   0xfffff
#define     PHY_DUMP_FLT_MAX_LENGTH_SHIFT                        0

#define PHY_DUMP_FLT3                                       0x023C
/* min length */
#define     PHY_DUMP_FLT_MIN_RSSI_MASK                        0xff
#define     PHY_DUMP_FLT_MIN_RSSI_SHIFT                          0

#define PHY_DUMP_FLT4                                       0x0240
/* max length */
#define     PHY_DUMP_FLT_MAX_RSSI_MASK                        0xff
#define     PHY_DUMP_FLT_MAX_RSSI_SHIFT                          0

#define PHY_DUMP_STOP_FLT0                                  0x0250
/* stop conditions */
/* 0 : format */
/* 1 : nsts */
/* 2 : mcs */
/* 3 : cbw */
/* 4 : length */
#define     PHY_DUMP_STOP_FLT_SEL_MASK                        0xff
#define     PHY_DUMP_STOP_FLT_SEL_SHIFT                          0
/* FORMAT */
/* 0 : NONHT */
/* 1 : HTMF */
/* 2 : HTGF */
/* 3 : VHT */
/* 4 : HESU */
/* 5 : HEER */
/* 6 : HEMU */
/* 7 : HETB */
/* 8 : B11 */
#define     PHY_DUMP_STOP_FLT_FORMAT_MASK                    0xf00
#define     PHY_DUMP_STOP_FLT_FORMAT_SHIFT                       8
/* NSTS (0~7) */
#define     PHY_DUMP_STOP_FLT_NSTS_MASK                     0x7000
#define     PHY_DUMP_STOP_FLT_NSTS_SHIFT                        12
/* MCS */
#define     PHY_DUMP_STOP_FLT_MCS_MASK                     0xf0000
#define     PHY_DUMP_STOP_FLT_MCS_SHIFT                         16
/* CBW */
#define     PHY_DUMP_STOP_FLT_CBW_MASK                    0x700000
#define     PHY_DUMP_STOP_FLT_CBW_SHIFT                         20

#define PHY_DUMP_STOP_FLT1                                  0x0254
/* min length */
#define     PHY_DUMP_STOP_MIN_LENGTH_MASK                  0xfffff
#define     PHY_DUMP_STOP_MIN_LENGTH_SHIFT                       0

#define PHY_DUMP_STOP_FLT2                                  0x0258
/* max length */
#define     PHY_DUMP_STOP_MAX_LENGTH_MASK                  0xfffff
#define     PHY_DUMP_STOP_MAX_LENGTH_SHIFT                       0

#define PHY_RECV_ABNORMAL_PE                                0x03E4
/* Support to receive HE PPDU with abnormal PE duration which is 20us or 24us */
#define     PHY_RECV_ABNORMAL_PE_MASK                          0x1
#define     PHY_RECV_ABNORMAL_PE_SHIFT                           0

#define PHY_RAND_SIG_TEST_MODE                              0x03E8
/* random signal field test mode */
#define     PHY_RAND_SIG_TEST_MODE_MASK                        0x1
#define     PHY_RAND_SIG_TEST_MODE_SHIFT                         0

#define PHY_COSIM_MODE                                      0x03EC
/* This field should be 1 when doing COSIM */
#define     PHY_COSIM_MODE_MASK                                0x1
#define     PHY_COSIM_MODE_SHIFT                                 0
/* Carrier sensing is operated in ED_STATE for RIFS support */
#define     PHY_EDSTATE_CS_ON_MASK                             0x2
#define     PHY_EDSTATE_CS_ON_SHIFT                              1

#define PHY_GIT_VER                                         0x03F0
/* PHY commit number */
#define     PHY_GIT_VER_MASK                            0xffffffff
#define     PHY_GIT_VER_SHIFT                                    0

#define PHY_VER_INFO                                        0x03F4
/* PHY version */
/* [31:16] : product code */
/* [15:12] : major version */
/* [11: 4] : minor version */
/* [ 3: 0] : F(FPGA), C(CHIP) */
#define     PHY_VER_INFO_MASK                           0xffffffff
#define     PHY_VER_INFO_SHIFT                                   0

#define PHY_DONT_BE_STUCK                                   0x03FC
/* hidden mode */
#define     PHY_DONT_BE_STUCK_MASK                             0x1
#define     PHY_DONT_BE_STUCK_SHIFT                              0
/*
 * Registers for TRX_FE
 */

#define PHY_LPF_CONFIG                                      0x1800
/* 0 : 4x interpolation filter ON */
/* 1 : 4x interpolation filter OFF */
#define     PHY_TXLPF4X_BYPASS_MASK                            0x1
#define     PHY_TXLPF4X_BYPASS_SHIFT                             0
/* 0 : primary channel shift ON */
/* 1 : primary channel shift OFF */
#define     PHY_TXLPF2X_CSFT_OFF_MASK                         0x10
#define     PHY_TXLPF2X_CSFT_OFF_SHIFT                           4
/* Pulse shaping ON OFF */
#define     PHY_TXLPF2X_PSF_ON_MASK                          0x100
#define     PHY_TXLPF2X_PSF_ON_SHIFT                             8
/* RxLPF bypass */
#define     PHY_RXLPF2X_BYPASS_MASK                         0x1000
#define     PHY_RXLPF2X_BYPASS_SHIFT                            12
/* RxLPF output gain ([3:1] shift 0, 1, 2, 3, 4, [0] x 1.5) */
#define     PHY_RXLPF2X_OUTGAIN0_MASK                      0xf0000
#define     PHY_RXLPF2X_OUTGAIN0_SHIFT                          16

#define PHY_STR_PKT_ON                                      0x1804
/* Detection of stronger packet during Rx. 0: OFF, 1:ON */
#define     PHY_STR_PKT_ON_MASK                                0x1
#define     PHY_STR_PKT_ON_SHIFT                                 0
/* Stronger packet detection is asserted when PRI_RSSI is lower than this value */
#define     PHY_STR_PKT_RSSI_THRD_MASK                       0xff0
#define     PHY_STR_PKT_RSSI_THRD_SHIFT                          4

#define PHY_RIFS_WAIT_TIME                                  0x1808
/* RIFS wait time */
#define     PHY_RIFS_WAIT_TIME_MASK                          0x1ff
#define     PHY_RIFS_WAIT_TIME_SHIFT                             0

#define PHY_TDBUF_SYNC_OFFSET                               0x180C
/* unit is 25ns. maximum is 190ns */
#define     PHY_TDBUF_SYNC_OFFSET_OP20_MASK                   0xff
#define     PHY_TDBUF_SYNC_OFFSET_OP20_SHIFT                     0
/* unit is 12.5ns. maximum is 190ns */
#define     PHY_TDBUF_SYNC_OFFSET_OP40_MASK                 0xff00
#define     PHY_TDBUF_SYNC_OFFSET_OP40_SHIFT                     8

#define PHY_FDSHAPE_CFG0                                    0x1820
/* FD shape is enabled if Tx power (dBm) is stronger than this lvl */
#define     PHY_FDSHAPE_THR_CB20_MASK                         0xff
#define     PHY_FDSHAPE_THR_CB20_SHIFT                           0
/* FD shape is enabled if Tx power (dBm) is stronger than this lvl */
#define     PHY_FDSHAPE_THR_CB40_MASK                       0xff00
#define     PHY_FDSHAPE_THR_CB40_SHIFT                           8
/* Tx FD shape ON(1) OFF(0) */
#define     PHY_FDSHAPE_ONOFF_MASK                         0x10000
#define     PHY_FDSHAPE_ONOFF_SHIFT                             16

#define PHY_FDSHAPE_CFG1                                    0x1824
/* Tx FD shape minimum gain */
#define     PHY_FDSHAPE_MINGAIN_MASK                         0xfff
#define     PHY_FDSHAPE_MINGAIN_SHIFT                            0
/* Tx FD shape maximum gain */
#define     PHY_FDSHAPE_MAXGAIN_MASK                     0xfff0000
#define     PHY_FDSHAPE_MAXGAIN_SHIFT                           16
/*
 * Registers for PHY TXBE
 */

#define PHY_TXBE_MANUAL_RESET                               0x1000
/* MANUAL RESET - ACTIVE HIGH */
/* 1'b0 : Diagonal (default) */
/* 1'b1 : programmable - based on stored Q val in memory */
#define     PHY_TXBE_MANUAL_RESET_MASK                         0x1
#define     PHY_TXBE_MANUAL_RESET_SHIFT                          0

#define PHY_TXBE_L_DATA_FETCH_TIME_CONTROL                  0x1004
/* Deferring fetch start time from L-DATA symbol start */
/* value 1 : 25nsec. */
/* Default 240 == 6 usec. */
/* 1.2usec difference btw phy txen and txbe_en */
#define     NONHT_DELAY_TIME_MASK                            0x3ff
#define     NONHT_DELAY_TIME_SHIFT                               0

#define PHY_TXBE_HT_DATA_FETCH_TIME_CONTROL                 0x1008
/* Deferring fetch start time from HT-DATA symbol start */
/* value 1 : 25nsec. */
/* Default 0 == 0 usec. */
#define     HTMF_DELAY_TIME_MASK                             0x3ff
#define     HTMF_DELAY_TIME_SHIFT                                0

#define PHY_TXBE_VHT_DATA_FETCH_TIME_CONTROL                0x100C
/* Deferring fetch start time from VHT-DATA symbol start */
/* value 1 : 25nsec. */
/* Default 0 == 0 usec. */
#define     VHT_DELAY_TIME_MASK                              0x3ff
#define     VHT_DELAY_TIME_SHIFT                                 0

#define PHY_TXBE_HESU_DATA_FETCH_TIME_CONTROL               0x1010
/* Deferring fetch start time from HE-DATA symbol start */
/* value 1 : 25nsec. */
/* default 0 == 0 usec. */
#define     HESU_DELAY_TIME_MASK                             0x3ff
#define     HESU_DELAY_TIME_SHIFT                                0

#define PHY_TXBE_HETB_DATA_FETCH_TIME_CONTROL               0x1014
/* Deferring fetch start time from HE-DATA symbol start */
/* value 1 : 25nsec. */
/* default 0 == 0 usec. */
#define     HETB_DELAY_TIME_MASK                             0x3ff
#define     HETB_DELAY_TIME_SHIFT                                0

#define PHY_TXBE_HEER_DATA_FETCH_TIME_CONTROL               0x1018
/* Deferring fetch start time from HE-DATA symbol start */
/* value 1 : 25nsec. */
/* default 0 == 0 usec. */
#define     HEER_DELAY_TIME_MASK                             0x3ff
#define     HEER_DELAY_TIME_SHIFT                                0
/*
 * Registers for RX TD
 */

#define PHY_CCA_THRD                                        0x3000
/* CCA OFDM threshold */
#define     PHY_CCA_THRD_OFDM_MASK                            0xff
#define     PHY_CCA_THRD_OFDM_SHIFT                              0
/* CCA 11B threshold */
#define     PHY_CCA_THRD_11B_MASK                           0xff00
#define     PHY_CCA_THRD_11B_SHIFT                               8
/* CCA ED threshold */
#define     PHY_CCA_THRD_ED_MASK                          0xff0000
#define     PHY_CCA_THRD_ED_SHIFT                               16
/* CCA digital threshold */
#define     PHY_CCA_THRD_DGTL_MASK                      0x3f000000
#define     PHY_CCA_THRD_DGTL_SHIFT                             24

#define PHY_RSSI_OFFSET                                     0x3004
/* RSSI offset of PHY (signed 7.3 format) */
#define     PHY_RSSI_OFFSET_MASK                             0x7ff
#define     PHY_RSSI_OFFSET_SHIFT                                0

#define FW_OBSS_PD                                          0x3008
/* OBSS PD level of PHY */
#define     FW_OBSS_PD_LVL_MASK                               0xff
#define     FW_OBSS_PD_LVL_SHIFT                                 0

#define PHY_CCA_OPTION                                      0x300C
/* wait time for cs_pwr state (unit is 25ns) */
#define     PHY_CCA_PWR_WAIT_TIME_MASK                       0x3ff
#define     PHY_CCA_PWR_WAIT_TIME_SHIFT                          0
/* wait time for cs_sat state (unit is 25ns) */
#define     PHY_CCA_SAT_WAIT_TIME_MASK                    0x3ff000
#define     PHY_CCA_SAT_WAIT_TIME_SHIFT                         12
/* 0 : agc gain changed */
#define     PHY_CCA_PWR_MSR_INIT_MASK                    0x1000000
#define     PHY_CCA_PWR_MSR_INIT_SHIFT                          24
/* agc_gain offset depending on a filter delay for OP20 */
#define     PHY_CCA_AGC_GAIN_OFFSET_OP20_MASK           0x1c000000
#define     PHY_CCA_AGC_GAIN_OFFSET_OP20_SHIFT                  26
/* agc_gain offset depending on a filter delay for OP40 */
#define     PHY_CCA_AGC_GAIN_OFFSET_OP40_MASK           0xe0000000
#define     PHY_CCA_AGC_GAIN_OFFSET_OP40_SHIFT                  29

#define PHY_11B_CS_CNTCFG                                   0x3010
/* 11b CS start delay. Ignore some samples when cs enabled */
/* (only valid in A path) */
#define     PHY_11B_CS_START_DELAY_MASK                       0xff
#define     PHY_11B_CS_START_DELAY_SHIFT                         0
/* 11b CS valid delay. Internal measurements are valid after this. */
/* (only valid in A path) */
#define     PHY_11B_CS_VALID_DELAY_MASK                     0xff00
#define     PHY_11B_CS_VALID_DELAY_SHIFT                         8
/* 11b CS after some silence period (no 11b detection) */
/* (only valid in A path) */
#define     PHY_11B_CS_NODET_DUR_MASK                     0xff0000
#define     PHY_11B_CS_NODET_DUR_SHIFT                          16
/* 11b power low indication if pwrlow for this duration */
/* (only valid in A path) */
#define     PHY_11B_CS_PWRLOW_DUR_MASK                  0xff000000
#define     PHY_11B_CS_PWRLOW_DUR_SHIFT                         24

#define PHY_11B_CS_MINPWR                                   0x3014
/* Minimum power threshold for 11b CS */
/* (only valid in A path) */
#define     PHY_11B_CS_MINPWR_MASK                       0x1ffffff
#define     PHY_11B_CS_MINPWR_SHIFT                              0

#define PHY_11B_CS_PARAM                                    0x3018
/* Power average parameter ((1-a)Pold + aPnew, x.5 format) */
/* (only valid in A path) */
#define     PHY_11B_CS_AVGPARAM_MASK                          0x3f
#define     PHY_11B_CS_AVGPARAM_SHIFT                            0
/* Carrier sensing threshold (x.6) */
/* (only valid in A path) */
#define     PHY_11B_CS_THRPARAM_MASK                        0xff00
#define     PHY_11B_CS_THRPARAM_SHIFT                            8
/* Minimum RSSI for 11b cs (dB) */
/* (only valid in A path) */
#define     PHY_11B_CS_RSSI_THR_MASK                      0xff0000
#define     PHY_11B_CS_RSSI_THR_SHIFT                           16
/* Disable RSSI check for 11b CS */
/* (only valid in A path) */
#define     PHY_11B_CS_RSSICHK_OFF_MASK                  0x1000000
#define     PHY_11B_CS_RSSICHK_OFF_SHIFT                        24
/* Enable 11b carrier sensing (only valid in A path) */
#define     PHY_11B_CS_ENABLE_MASK                      0x10000000
#define     PHY_11B_CS_ENABLE_SHIFT                             28

#define PHY_SYNC_BDET_CFG                                   0x301C
/* STF acorr boundary detection threshold */
#define     PHY_STF_BDET_THRD_MASK                             0xf
#define     PHY_STF_BDET_THRD_SHIFT                              0
/* STF acorr boundary detection enable */
#define     PHY_STF_BDET_ENA_MASK                             0x10
#define     PHY_STF_BDET_ENA_SHIFT                               4
/* Boundary detection valid latency (tune param for high rssi) */
#define     PHY_STF_BDET_COUNT_MASK                         0xff00
#define     PHY_STF_BDET_COUNT_SHIFT                             8

#define PHY_SYNC_CCFO_CSFO                                  0x3020
/* CCFO to CSFO conversion ratio (set by Fc) */
#define     FW_CSFO_CONV_RATIO_MASK                         0x1fff
#define     FW_CSFO_CONV_RATIO_SHIFT                             0
/* Number of samples for estimiting CCFO */
/* 1 - 32 sample, 0 - 16 sample */
#define     PHY_CCFO_ESTLEN_MASK                            0x8000
#define     PHY_CCFO_ESTLEN_SHIFT                               15
/* Time domain csfo compensation enable */
/* 1 - ON, 0 - OFF */
#define     PHY_CSFO_TDCOMP_MASK                           0x10000
#define     PHY_CSFO_TDCOMP_SHIFT                               16
/* Calculate l20 output gain */
/* 1 - ON, 0 - OFF */
#define     PHY_CSFO_L20GAIN_EN_MASK                      0x100000
#define     PHY_CSFO_L20GAIN_EN_SHIFT                           20

#define PHY_SYNC_LLTF_DET                                   0x3024
/* LLTF detection threshold */
#define     PHY_SYNC_LLTF_THR_MASK                            0x3f
#define     PHY_SYNC_LLTF_THR_SHIFT                              0
/* minimum power for LLTF detection */
#define     PHY_SYNC_LLTF_MINPWR_MASK                      0x7ff00
#define     PHY_SYNC_LLTF_MINPWR_SHIFT                           8
/* FCFO measurement window shift from CP boundary */
#define     PHY_SYNC_FCFO_MSRPOS_MASK                     0xf00000
#define     PHY_SYNC_FCFO_MSRPOS_SHIFT                          20
/* Boundary search counter */
#define     PHY_SYNC_BSEARCH_CNT_MASK                   0xff000000
#define     PHY_SYNC_BSEARCH_CNT_SHIFT                          24

#define PHY_SYNC_REPDET                                     0x3028
/* Lower threshold for repetition detection */
#define     PHY_SYNC_REPTHR_LOWER_MASK                        0x3f
#define     PHY_SYNC_REPTHR_LOWER_SHIFT                          0
/* Upper threshold for repetition detection */
#define     PHY_SYNC_REPTHR_UPPER_MASK                      0x3f00
#define     PHY_SYNC_REPTHR_UPPER_SHIFT                          8
/* IQ ratio for repetition detection */
#define     PHY_SYNC_REPTHR_IQRATIO_MASK                  0x3f0000
#define     PHY_SYNC_REPTHR_IQRATIO_SHIFT                       16
/* HE ER format detection enable */
#define     PHY_SYNC_REPTHR_HEER_EN_MASK                 0x1000000
#define     PHY_SYNC_REPTHR_HEER_EN_SHIFT                       24

#define PHY_SYNC_MIMOCFG                                    0x302C
/* CCFO acorr combining on/off when 2 Rx */
#define     PHY_SYNC_CCFO_COMBEN_MASK                          0x1
#define     PHY_SYNC_CCFO_COMBEN_SHIFT                           0
/* FCFO acorr combining on/off when 2 Rx */
#define     PHY_SYNC_FCFO_COMBEN_MASK                         0x10
#define     PHY_SYNC_FCFO_COMBEN_SHIFT                           4
/* Repetition det acorr combining on/off when 2 Rx */
#define     PHY_SYNC_REPD_COMBEN_MASK                        0x100
#define     PHY_SYNC_REPD_COMBEN_SHIFT                           8
/* Enable wideband position adjustment */
#define     PHY_SYNC_WIDEBAND_ON_MASK                       0x1000
#define     PHY_SYNC_WIDEBAND_ON_SHIFT                          12
/* Sync timeout counter */
#define     PHY_SYNC_TIMER_MAX_MASK                      0x3ff0000
#define     PHY_SYNC_TIMER_MAX_SHIFT                            16

#define PHY_CS_CONFIG_0                                     0x3030
/* Cross-correlation detector ON */
#define     PHY_CS_XCRDET_ON_MASK                              0x1
#define     PHY_CS_XCRDET_ON_SHIFT                               0
/* Auto-correlation detector ON */
#define     PHY_CS_ACRDET_ON_MASK                             0x10
#define     PHY_CS_ACRDET_ON_SHIFT                               4
/* Rising power check ON */
#define     PHY_CS_RISING_PWR_CHECK_ON_MASK                   0x20
#define     PHY_CS_RISING_PWR_CHECK_ON_SHIFT                     5
/* Measurement valid delay (should be >= 64) */
#define     PHY_CS_MSR_VALID_DELAY_MASK                    0x3ff00
#define     PHY_CS_MSR_VALID_DELAY_SHIFT                         8
/* Power-Low detection duration */
#define     PHY_CS_PWRLOW_DUR_MASK                      0x3ff00000
#define     PHY_CS_PWRLOW_DUR_SHIFT                             20

#define PHY_CS_CONFIG_1                                     0x3034
/* Minimum digital power for cross-correlation detection */
#define     PHY_CS_XCR_MINPWR_MASK                         0xfffff
#define     PHY_CS_XCR_MINPWR_SHIFT                              0
/* Minimum RSSI for cross-correlation detection. Signed 8 bit. */
/* (enabled when MSB = 1) */
#define     PHY_CS_XCR_MINRSSI_MASK                      0xff00000
#define     PHY_CS_XCR_MINRSSI_SHIFT                            20

#define PHY_CS_CONFIG_2                                     0x3038
/* Minimum digital power for auto-correlation detection */
#define     PHY_CS_ACR_MINPWR_MASK                         0xfffff
#define     PHY_CS_ACR_MINPWR_SHIFT                              0
/* Minimum RSSI for auto-correlation detection. Signed 8 bit. */
/* (enabled when MSB = 1) */
#define     PHY_CS_ACR_MINRSSI_MASK                      0xff00000
#define     PHY_CS_ACR_MINRSSI_SHIFT                            20

#define PHY_CS_CONFIG_THRD                                  0x303C
/* cross-correlation detector threshold */
#define     PHY_CS_XCR_THR_MASK                               0x3f
#define     PHY_CS_XCR_THR_SHIFT                                 0
/* auto-correlation detector threshold */
#define     PHY_CS_ACR_THR0_MASK                            0x3f00
#define     PHY_CS_ACR_THR0_SHIFT                                8
/* auto-correlation detector threshold (for checking xcorr) */
#define     PHY_CS_ACR_THR1_MASK                          0x3f0000
#define     PHY_CS_ACR_THR1_SHIFT                               16

#define PHY_CSS_STATE                                       0x3040
/* CSS internal state monitor */
#define     PHY_CSS_INTERNAL_ST_A_MASK                         0xf
#define     PHY_CSS_INTERNAL_ST_A_SHIFT                          0
/* CSS internal state monitor */
#define     PHY_CSS_INTERNAL_ST_B_MASK                        0xf0
#define     PHY_CSS_INTERNAL_ST_B_SHIFT                          4
/* CSS internal state monitor */
#define     PHY_CSS_INTERNAL_ST_C_MASK                       0xf00
#define     PHY_CSS_INTERNAL_ST_C_SHIFT                          8

#define PHY_CS_MIMO_CONFIG                                  0x3044
/* LLTF detection bsearch threshold (new ver) */
#define     PHY_SYNC_BSEARCH_THR0_MASK                        0x3f
#define     PHY_SYNC_BSEARCH_THR0_SHIFT                          0
/* LLTF detection bsearch threshold (new ver) */
#define     PHY_SYNC_BSEARCH_THR1_MASK                      0x3f00
#define     PHY_SYNC_BSEARCH_THR1_SHIFT                          8
/* LLTF detection bsearch threshold (new ver) */
#define     PHY_SYNC_BSEARCH_THR2_MASK                    0x3f0000
#define     PHY_SYNC_BSEARCH_THR2_SHIFT                         16
/* Combine sync info if RSSI diff is lower than this value */
/* Or, select stronger value */
#define     PHY_SYNC_MIMOCOMB_THRD_MASK                 0xff000000
#define     PHY_SYNC_MIMOCOMB_THRD_SHIFT                        24

#define PHY_CS_ACISAT_FLT                                   0x3048
/* saturation detection and prich rssi is lower than this level, discard */
#define     PHY_CS_ACISAT_ABSRSSI_MASK                        0xff
#define     PHY_CS_ACISAT_ABSRSSI_SHIFT                          0
/* Saturation detection and pri ch rssi diff is bigger than this, discard. */
#define     PHY_CS_ACISAT_RELRSSI_MASK                      0xff00
#define     PHY_CS_ACISAT_RELRSSI_SHIFT                          8
/* ACI SAT RSSI check enable */
#define     PHY_CS_ACISAT_DROP_EN_MASK                  0x10000000
#define     PHY_CS_ACISAT_DROP_EN_SHIFT                         28

#define PHY_SYNC_REPDET_THRBND                              0x304C
/* Max thr for repdet */
#define     PHY_SYNC_REPDET_THRMAX_MASK                       0x1f
#define     PHY_SYNC_REPDET_THRMAX_SHIFT                         0
/* Max thr for repdet */
#define     PHY_SYNC_REPDET_THRMIN_MASK                     0x1f00
#define     PHY_SYNC_REPDET_THRMIN_SHIFT                         8
/* if RSSI is lower than this value, ignored (unless max RSSI) */
#define     PHY_SYNC_MIMOOFF_RSSI_LMT_MASK              0xff000000
#define     PHY_SYNC_MIMOOFF_RSSI_LMT_SHIFT                     24

#define PHY_AGC_INDEX_CFG                                   0x3050
/* AGC Initial gain index */
#define     PHY_AGC_INITGAIN_MASK                             0xff
#define     PHY_AGC_INITGAIN_SHIFT                               0
/* AGC Max gain index */
#define     PHY_AGC_MAXGAIN_MASK                            0xff00
#define     PHY_AGC_MAXGAIN_SHIFT                                8
/* AGC Min gain index */
#define     PHY_AGC_MINGAIN_MASK                          0xff0000
#define     PHY_AGC_MINGAIN_SHIFT                               16

#define PHY_AGC_GDOWN_CFG                                   0x3054
/* Gain step for initchkpwr saturation */
#define     PHY_AGC_GDOWN_SAT0_MASK                           0xff
#define     PHY_AGC_GDOWN_SAT0_SHIFT                             0
/* Gain step for saturation CS */
#define     PHY_AGC_GDOWN_SAT1_MASK                         0xff00
#define     PHY_AGC_GDOWN_SAT1_SHIFT                             8
/* Gain step for 2nd saturation and FAGC sat */
#define     PHY_AGC_GDOWN_SAT2_MASK                       0xff0000
#define     PHY_AGC_GDOWN_SAT2_SHIFT                            16
/* Gain step for power CS */
#define     PHY_AGC_GDOWN_SAT3_MASK                     0xff000000
#define     PHY_AGC_GDOWN_SAT3_SHIFT                            24

#define PHY_AGC_WAIT_CNT0                                   0x3058
/* Initial state wait counter (40M) */
#define     PHY_AGC_INITWAIT_T_MASK                          0x3ff
#define     PHY_AGC_INITWAIT_T_SHIFT                             0
/* Ready state gain settling wait counter (40M) */
#define     PHY_AGC_READYGAIN_T_MASK                     0x3ff0000
#define     PHY_AGC_READYGAIN_T_SHIFT                           16

#define PHY_AGC_WAIT_CNT1                                   0x305C
/* Initial coarse gain adj wait counter (40M) */
#define     PHY_AGC_INIT_GWAIT_C_T_MASK                      0x3ff
#define     PHY_AGC_INIT_GWAIT_C_T_SHIFT                         0
/* Initial fine gain adj wait counter (40M) */
#define     PHY_AGC_INIT_GWAIT_F_T_MASK                  0x3ff0000
#define     PHY_AGC_INIT_GWAIT_F_T_SHIFT                        16

#define PHY_AGC_WAIT_CNT2                                   0x3060
/* Coarse gain adj wait counter (40M) */
#define     PHY_AGC_GADJ_WAIT_C_T_MASK                       0x3ff
#define     PHY_AGC_GADJ_WAIT_C_T_SHIFT                          0
/* Fine gain adj wait counter (40M) */
#define     PHY_AGC_GADJ_WAIT_F_T_MASK                   0x3ff0000
#define     PHY_AGC_GADJ_WAIT_F_T_SHIFT                         16

#define PHY_AGC_WAIT_CNT3                                   0x3064
/* Coarse gain adj wait counter (40M) */
#define     PHY_AGC_FAGC_WAIT_C_T_MASK                       0x3ff
#define     PHY_AGC_FAGC_WAIT_C_T_SHIFT                          0
/* Fine gain adj wait counter (40M) */
#define     PHY_AGC_FAGC_WAIT_F_T_MASK                   0x3ff0000
#define     PHY_AGC_FAGC_WAIT_F_T_SHIFT                         16

#define PHY_AGC_PWRMSR_CFG                                  0x3068
/* Power low threshold. If bit [9] = 1, disabled. */
#define     PHY_AGC_PWRMSR_LOWTHR_MASK                       0x3ff
#define     PHY_AGC_PWRMSR_LOWTHR_SHIFT                          0
/* DC measurement duration. 0: 2 stf pattern, 1: 4 stf pattern */
#define     PHY_AGC_MSRDC_4STF_MASK                        0x10000
#define     PHY_AGC_MSRDC_4STF_SHIFT                            16
/* Carrier sensing RSSI threshold check enable */
#define     PHY_AGC_RSSI_THRCHK_EN_MASK                   0x100000
#define     PHY_AGC_RSSI_THRCHK_EN_SHIFT                        20

#define PHY_AGC_INITPWR_RANGE                               0x306C
/* Init power lower boundary */
#define     PHY_AGC_INITPWR_MIN_MASK                         0x3ff
#define     PHY_AGC_INITPWR_MIN_SHIFT                            0
/* Init power upper boundary */
#define     PHY_AGC_INITPWR_MAX_MASK                     0x3ff0000
#define     PHY_AGC_INITPWR_MAX_SHIFT                           16

#define PHY_AGC_TARGET_PWR                                  0x3070
/* Signal target power */
#define     PHY_AGC_SIGNALPWR_TGT_MASK                       0x3ff
#define     PHY_AGC_SIGNALPWR_TGT_SHIFT                          0
/* Noise target power */
#define     PHY_AGC_NOISEPWR_TGT_MASK                    0x3ff0000
#define     PHY_AGC_NOISEPWR_TGT_SHIFT                          16

#define PHY_AGC_CS_RSSI_THR                                 0x3074
/* RSSI threshold for power CS */
#define     PHY_AGC_RSSI_THR_PWRCS_MASK                       0xff
#define     PHY_AGC_RSSI_THR_PWRCS_SHIFT                         0

#define PHY_AGC_RSSI_OFFSET                                 0x3078
/* RSSI offset btw digital power and true RSSI */
#define     PHY_AGC_RSSI_OFFSET_MASK                          0xff
#define     PHY_AGC_RSSI_OFFSET_SHIFT                            0

#define PHY_AGC_MODE_CFG                                    0x307C
/* Fix initial gain */
#define     PHY_AGC_FIX_INITGAIN_MASK                          0x1
#define     PHY_AGC_FIX_INITGAIN_SHIFT                           0
/* 1 - Digital loopback mode (no gain change) */
#define     PHY_AGC_DIG_LPBACK_MODE_MASK                      0x10
#define     PHY_AGC_DIG_LPBACK_MODE_SHIFT                        4

#define PHY_FAGC_EN_CFG                                     0x3080
/* FAGC enable when hxstf candidate */
#define     PHY_AGC_FAGC_EN_HXCAND_MASK                        0x1
#define     PHY_AGC_FAGC_EN_HXCAND_SHIFT                         0
/* FAGC enable when vhtstf candidate */
#define     PHY_AGC_FAGC_EN_VHTCAND_MASK                      0x10
#define     PHY_AGC_FAGC_EN_VHTCAND_SHIFT                        4
/* FAGC enable for 2 symbol count after repetition det */
#define     PHY_AGC_FAGC_EN_TIMER_MASK                       0x100
#define     PHY_AGC_FAGC_EN_TIMER_SHIFT                          8

#define PHY_FAGC_TRIG_CFG                                   0x3084
/* FAGC trigger when hxstf confirmed */
#define     PHY_AGC_FAGC_TRIG_CONFIRMED_MASK                   0x1
#define     PHY_AGC_FAGC_TRIG_CONFIRMED_SHIFT                    0
/* FAGC trigger when vhtstf candidate and CS ok */
#define     PHY_AGC_FAGC_TRIG_VHTCS_MASK                      0x10
#define     PHY_AGC_FAGC_TRIG_VHTCS_SHIFT                        4
/* FAGC trigger when hxstf candidate and CS ok */
#define     PHY_AGC_FAGC_TRIG_HXCS_MASK                      0x100
#define     PHY_AGC_FAGC_TRIG_HXCS_SHIFT                         8
/* FAGC trigger when satdet */
#define     PHY_AGC_FAGC_TRIG_SATDET_MASK                   0x1000
#define     PHY_AGC_FAGC_TRIG_SATDET_SHIFT                      12
/* FAGC trigger when timer on and CS ok */
#define     PHY_AGC_FAGC_TRIG_TIMERCS_MASK                 0x10000
#define     PHY_AGC_FAGC_TRIG_TIMERCS_SHIFT                     16

#define PHY_FAGC_PWR_COND                                   0x3088
/* FAGC trigger power condition (lower bound, x.3) */
#define     PHY_AGC_FAGC_PWRCOND_LOW_MASK                     0xff
#define     PHY_AGC_FAGC_PWRCOND_LOW_SHIFT                       0
/* FAGC trigger power condition (upper bound, x.3) */
#define     PHY_AGC_FAGC_PWRCOND_UPP_MASK                   0xff00
#define     PHY_AGC_FAGC_PWRCOND_UPP_SHIFT                       8
/* FAGC trigger power condition enable */
#define     PHY_AGC_FAGC_PWRCOND_ENA_MASK                  0x10000
#define     PHY_AGC_FAGC_PWRCOND_ENA_SHIFT                      16

#define PHY_AGC_STATUS_RD1                                  0x308C
/* AGC ready state gain */
#define     PHY_AGC_READY_GAIN_MASK                           0xff
#define     PHY_AGC_READY_GAIN_SHIFT                             0
/* AGC current gain */
#define     PHY_AGC_CURR_GAIN_MASK                          0xff00
#define     PHY_AGC_CURR_GAIN_SHIFT                              8
/* AGC current state */
#define     PHY_AGC_CURR_ST_MASK                          0x1f0000
#define     PHY_AGC_CURR_ST_SHIFT                               16
/* AGC state when init signal is received (except HOLD) */
#define     PHY_AGC_ST_WHEN_INIT_MASK                   0x1f000000
#define     PHY_AGC_ST_WHEN_INIT_SHIFT                          24

#define PHY_AGC_STATUS_RD2                                  0x3090
/* AGC hold gain */
#define     PHY_AGC_HOLD_GAIN_MASK                            0xff
#define     PHY_AGC_HOLD_GAIN_SHIFT                              0
/* AGC hold power */
#define     PHY_AGC_HOLD_PWR_MASK                          0x3ff00
#define     PHY_AGC_HOLD_PWR_SHIFT                               8

#define PHY_AGC_TEST_OPTION                                 0x3094
/* Extend release agc signal by 1 clock (o2x) */
#define     PHY_AGC_EXTEND_CLEAR_MASK                          0x1
#define     PHY_AGC_EXTEND_CLEAR_SHIFT                           0
/* Escape from any state by agc release */
#define     PHY_AGC_ESCAPE_ANY_STATE_MASK                     0x10
#define     PHY_AGC_ESCAPE_ANY_STATE_SHIFT                       4
/* [23-16] Reserved */
/* [15] separate sat CS and sat det condition (active 0) */
/* [14] MIMO CS ready condition en */
/* [13] Use aux CS ready in AGC */
/* [12] New sat gain update condition */
/* [11] CS ready check in corr CS detection */
/* [10] MIMO WAIT escape enable */
/* [ 9] 11b CS en condition */
/* [ 8] Reserved */
#define     PHY_AGC_TEST_OPTION_ETC_MASK                  0xffff00
#define     PHY_AGC_TEST_OPTION_ETC_SHIFT                        8

#define PHY_AGC_PWRCS_CFG                                   0x3098
/* Power CS threshold */
#define     PHY_AGC_PWRCS_THRD_MASK                          0x3ff
#define     PHY_AGC_PWRCS_THRD_SHIFT                             0
/* Power CS enable */
#define     PHY_AGC_PWRCS_ENA_MASK                         0x10000
#define     PHY_AGC_PWRCS_ENA_SHIFT                             16

#define PHY_SYNC2_PARAM                                     0x30E0
/* STF boundary detection delay */
#define     PHY_SYNC_BDET_DELAY_MASK                          0xff
#define     PHY_SYNC_BDET_DELAY_SHIFT                            0
/* 2nd peak detection window min */
#define     PHY_SYNC_2NDPEAK_DLY_MIN_MASK                   0xff00
#define     PHY_SYNC_2NDPEAK_DLY_MIN_SHIFT                       8
/* 2nd peak detection window max */
#define     PHY_SYNC_2NDPEAK_DLY_MAX_MASK                 0xff0000
#define     PHY_SYNC_2NDPEAK_DLY_MAX_SHIFT                      16
/* Cluster detection window size */
#define     PHY_SYNC_CLUSTER_WIN_MASK                   0xff000000
#define     PHY_SYNC_CLUSTER_WIN_SHIFT                          24

#define PHY_PWRCS2_PARAM0                                   0x30F0
/* enable power cs v2 */
#define     PHY_PWR_CS2_EN_MASK                                0x1
#define     PHY_PWR_CS2_EN_SHIFT                                 0
/* primary power cs v2 for strpkt det */
#define     PHY_PWR_CS2_P20_EN_MASK                            0x2
#define     PHY_PWR_CS2_P20_EN_SHIFT                             1
/* pwr cs mode sel - 0 : full bw, 1: data bw, 2: p20 only */
#define     PHY_PWR_CS2_SEL_MASK                               0xc
#define     PHY_PWR_CS2_SEL_SHIFT                                2
/* discard a few power measurement before pwr cs ready */
#define     PHY_PWR_CS2_DLY_MASK                              0xf0
#define     PHY_PWR_CS2_DLY_SHIFT                                4
/* after power cs or sat cs, no signal flag if */
/* rssi is weaker than < ready_rssi + this threshold */
#define     PHY_PWR_CS2_NOSIGDET_THRD_MASK                  0xff00
#define     PHY_PWR_CS2_NOSIGDET_THRD_SHIFT                      8
/* Power detection threshold (dB) */
#define     PHY_PWR_CS2_THRD_MASK                         0xff0000
#define     PHY_PWR_CS2_THRD_SHIFT                              16
/* Power detection minimum rssi (dB) */
#define     PHY_PWR_CS2_MINRSSI_MASK                    0xff000000
#define     PHY_PWR_CS2_MINRSSI_SHIFT                           24

#define PHY_PWRCS2_PARAM1                                   0x30F4
/* full band power measurement - remove dc power */
#define     PHY_PWR_CS2_FBAND_MSRAC_MASK                       0x1
#define     PHY_PWR_CS2_FBAND_MSRAC_SHIFT                        0
/* half band power measurement - remove dc power */
#define     PHY_PWR_CS2_HBAND_MSRAC_MASK                       0x2
#define     PHY_PWR_CS2_HBAND_MSRAC_SHIFT                        1
/* Pri 20M band power measurement - remove dc power */
#define     PHY_PWR_CS2_20M_MSRAC_MASK                         0x4
#define     PHY_PWR_CS2_20M_MSRAC_SHIFT                          2
/* 0 : measure 1.6us,  1: 0.8us */
#define     PHY_PWR_CS2_MSR_800NS_MASK                         0x8
#define     PHY_PWR_CS2_MSR_800NS_SHIFT                          3
/* full band dc hpf ON */
#define     PHY_PWR_CS2_FBAND_HPF_ON_MASK                     0x10
#define     PHY_PWR_CS2_FBAND_HPF_ON_SHIFT                       4
/* half band dc hpf ON */
#define     PHY_PWR_CS2_HBAND_HPF_ON_MASK                     0x20
#define     PHY_PWR_CS2_HBAND_HPF_ON_SHIFT                       5
/* full band dc hpf weight */
#define     PHY_PWR_CS2_FBAND_HPF_WGT_MASK                   0xf00
#define     PHY_PWR_CS2_FBAND_HPF_WGT_SHIFT                      8
/* half band dc hpf weight */
#define     PHY_PWR_CS2_HBAND_HPF_WGT_MASK                  0xf000
#define     PHY_PWR_CS2_HBAND_HPF_WGT_SHIFT                     12
/* no signal detection digital power thrd */
#define     PHY_PWR_CS2_NOSIG_DGTL_MASK                   0xff0000
#define     PHY_PWR_CS2_NOSIG_DGTL_SHIFT                        16

#define PHY_PRI_SPWR                                        0x3100
/* Signal power of the primary 20MHz */
#define     RX_TD_MON_00_MASK                           0xffffffff
#define     RX_TD_MON_00_SHIFT                                   0

#define PHY_PRI_NPWR                                        0x3104
/* Noise power of the primary 20MHz */
#define     RX_TD_MON_01_MASK                           0xffffffff
#define     RX_TD_MON_01_SHIFT                                   0

#define PHY_LPF20_CONFIG                                    0x3110
/* DC HPF weight for pri20 */
#define     L20_DCHPF_WGT_MASK                                 0xf
#define     L20_DCHPF_WGT_SHIFT                                  0
/* [3] 11b ON [2] always on, [1] csready, [0] agc done */
#define     L20_DCHPF_UPD_MASK                                0xf0
#define     L20_DCHPF_UPD_SHIFT                                  4
/* DC HPF enable */
#define     L20_DCHPF_ON_MASK                                0x100
#define     L20_DCHPF_ON_SHIFT                                   8
/* LPF20 outgain ([3:1] shift 0,1,2,3,4, [0] x1.5) */
#define     L20_LPF_OUTGAIN0_MASK                          0xf0000
#define     L20_LPF_OUTGAIN0_SHIFT                              16

#define PHY_LPF20_TDSPUR_CFG                                0x3114
/* TD spur frequency */
/* spur frequency = rnd(2048 * freq(Hz) * 512 / (2 x opbw (Hz))) */
#define     L20_TD_SPUR_FREQ_MASK                          0xfffff
#define     L20_TD_SPUR_FREQ_SHIFT                               0
/* TD spur filter weight */
#define     L20_TD_SPUR_FLT_WGT_MASK                      0xf00000
#define     L20_TD_SPUR_FLT_WGT_SHIFT                           20
/* TD spur filter update */
/* [2] always on, [1] csready, [0] agc done */
#define     L20_TD_SPUR_FLT_UPD_MASK                     0x7000000
#define     L20_TD_SPUR_FLT_UPD_SHIFT                           24
/* TD spur filter ON */
#define     L20_TD_SPUR_FLT_ON_MASK                     0x10000000
#define     L20_TD_SPUR_FLT_ON_SHIFT                            28

#define PHY_MIDCS_LTF1X                                     0x3130
/* The weight value to detect 3.2us DATA and LTF field */
#define     PHY_MIDCS_WGT_LTF1X_MASK                           0xf
#define     PHY_MIDCS_WGT_LTF1X_SHIFT                            0
/* The threshold value to detect 3.2us DATA and LTF field */
#define     PHY_MIDCS_THRD_LTF1X_MASK                      0xffff0
#define     PHY_MIDCS_THRD_LTF1X_SHIFT                           4

#define PHY_MIDCS_LTF2X                                     0x3134
/* The weight value to detect 6.4us LTF field */
#define     PHY_MIDCS_WGT_LTF2X_MASK                           0xf
#define     PHY_MIDCS_WGT_LTF2X_SHIFT                            0
/* The threshold value to detect 6.4us LTF field */
#define     PHY_MIDCS_THRD_LTF2X_MASK                     0x1ffff0
#define     PHY_MIDCS_THRD_LTF2X_SHIFT                           4

#define PHY_MIDCS_LTF4X                                     0x3138
/* The weight value to detect 12.8us LTF field */
#define     PHY_MIDCS_WGT_LTF4X_MASK                           0xf
#define     PHY_MIDCS_WGT_LTF4X_SHIFT                            0
/* The threshold value to detect 12.8us LTF field */
#define     PHY_MIDCS_THRD_LTF4X_MASK                     0x1ffff0
#define     PHY_MIDCS_THRD_LTF4X_SHIFT                           4

#define PHY_SNRE_LOWSNR_WGT                                 0x3200
/* The weight value for low SNR detection */
/* If the value is less than the set value, SNRE determines as low SNR */
/* 255 means  0 dB. */
/* 241 means -0.26 dB. */
/* 228 means -0.5 dB. */
/* 215 means -0.75 dB. */
/* 203 means -1 dB. */
/* 192 means -1.25 dB. */
/* 181 means -1.5 dB. */
#define     PHY_SNRE_LOWSNR_WGT_MASK                          0xff
#define     PHY_SNRE_LOWSNR_WGT_SHIFT                            0

#define PHY_STR_PKT_SATDET_CFG                              0x3210
/* Saturation detection threshold for stronger packet detection */
#define     PHY_STR_PKT_SATDET_THRD_MASK                     0xfff
#define     PHY_STR_PKT_SATDET_THRD_SHIFT                        0
/* The number of saturation in 400ns for stronger packet detection (max 8) */
#define     PHY_STR_PKT_SATDET_NUM_MASK                     0xf000
#define     PHY_STR_PKT_SATDET_NUM_SHIFT                        12

#define PHY_RADAR_DET_CFG0                                  0x3220
/* Packet drop by ACI or radar detection enable */
#define     PHY_RADAR_DROP_EN_MASK                             0x1
#define     PHY_RADAR_DROP_EN_SHIFT                              0
/* Chirp detector enable */
#define     PHY_CHIRP_DET_EN_MASK                              0x2
#define     PHY_CHIRP_DET_EN_SHIFT                               1
/* Short pulse detector enable */
#define     PHY_SHORTPLS_DET_EN_MASK                           0x4
#define     PHY_SHORTPLS_DET_EN_SHIFT                            2
/* Chirp detector threshold */
#define     PHY_CHIRP_DET_THR_MASK                            0x70
#define     PHY_CHIRP_DET_THR_SHIFT                              4
/* Chirp detector acorr scale */
#define     PHY_CHIRP_ACR_SCL_MASK                           0x300
#define     PHY_CHIRP_ACR_SCL_SHIFT                              8
/* Chirp detector acrr avg cff */
#define     PHY_CHIRP_AVG_CFF_MASK                          0x3000
#define     PHY_CHIRP_AVG_CFF_SHIFT                             12
/* Minimum RSSI for chirp detection */
#define     PHY_CHIRP_DET_MINRSSI_MASK                  0xff000000
#define     PHY_CHIRP_DET_MINRSSI_SHIFT                         24

#define PHY_RADAR_DET_CFG1                                  0x3224
/* Chirp pre-detection threshold */
#define     PHY_CHIRP_DET_CNTTHR_PRE_MASK                    0xfff
#define     PHY_CHIRP_DET_CNTTHR_PRE_SHIFT                       0
/* Chirp detection threshold */
#define     PHY_CHIRP_DET_CNTTHR_MASK                    0xfff0000
#define     PHY_CHIRP_DET_CNTTHR_SHIFT                          16

#define PHY_RADAR_DET_CFG2                                  0x3228
/* Chirp detection minimum digital power */
#define     PHY_CHIRP_DET_MINPWR_MASK                   0xffffffff
#define     PHY_CHIRP_DET_MINPWR_SHIFT                           0

#define PHY_RADAR_DET_CFG3                                  0x322C
/* Chirp pre-detection timer (p2x) */
#define     PHY_CHIRP_PREDET_TIMER_MASK                     0xffff
#define     PHY_CHIRP_PREDET_TIMER_SHIFT                         0
/* Chirp detection minimum timer (p2x) */
#define     PHY_CHIRP_MIN_TIMER_MASK                    0xffff0000
#define     PHY_CHIRP_MIN_TIMER_SHIFT                           16

#define PHY_RADAR_DET_CFG4                                  0x3230
/* Chirp detection maximum timer (p2x) */
#define     PHY_CHIRP_MAX_TIMER_MASK                        0xffff
#define     PHY_CHIRP_MAX_TIMER_SHIFT                            0
/* Short pulse detection max timer (p2x) */
#define     PHY_SHORTPLS_MAX_TIMER_MASK                 0xffff0000
#define     PHY_SHORTPLS_MAX_TIMER_SHIFT                        16

#define PHY_RADAR_DET_CFG5                                  0x3234
/* Short pulse train detection count */
#define     PHY_SHORTPLS_TRN_THR_MASK                         0xff
#define     PHY_SHORTPLS_TRN_THR_SHIFT                           0
/* Next short pulse detection window (p2x x16) */
#define     PHY_SHORTPLS_NXT_TIMER_MASK                 0xffff0000
#define     PHY_SHORTPLS_NXT_TIMER_SHIFT                        16

#define PHY_RADAR_DET_CFG6                                  0x3238
/* Radar detection count clear */
#define     PHY_RADAR_DET_CNTCLR_MASK                          0x1
#define     PHY_RADAR_DET_CNTCLR_SHIFT                           0

#define PHY_RADAR_DET_RPT                                   0x323C
/* Short pulse detection count */
#define     PHY_SHORTPLS_CNT_MASK                           0xffff
#define     PHY_SHORTPLS_CNT_SHIFT                               0
/* Long pulse detection count */
#define     PHY_LONGPLS_CNT_MASK                        0xffff0000
#define     PHY_LONGPLS_CNT_SHIFT                               16

#define PHY_RADAR_DET_CFG7                                  0x3240
/* Chirp in-band frequency test (x.5) */
#define     PHY_CHIRP_FREQ_THR_MASK                           0xff
#define     PHY_CHIRP_FREQ_THR_SHIFT                             0
/* Use frequency check for power cs tone chk */
#define     PHY_CHIRP_FREQCHK_PWRCS_MASK                     0x100
#define     PHY_CHIRP_FREQCHK_PWRCS_SHIFT                        8
/* Use frequency check for long pulse detection */
#define     PHY_CHIRP_FREQCHK_LONGPLS_MASK                   0x200
#define     PHY_CHIRP_FREQCHK_LONGPLS_SHIFT                      9
/* Power CS tone test using chirp det */
#define     PHY_PWRCS_TONE_CHK_EN_MASK                       0x400
#define     PHY_PWRCS_TONE_CHK_EN_SHIFT                         10
/* Power CS tone test threshold */
#define     PHY_PWRCS_TONE_CHK_MAGTHR_MASK                  0x7000
#define     PHY_PWRCS_TONE_CHK_MAGTHR_SHIFT                     12
/* Power CS tone test count */
#define     PHY_PWRCS_TONE_CHK_CNTTHR_MASK                0xff0000
#define     PHY_PWRCS_TONE_CHK_CNTTHR_SHIFT                     16

#define PHY_RADAR_DET_CFG8                                  0x3244
/* Power CS magnitude range test en */
#define     PHY_CHIRP_MAGCHK_EN_CS_MASK                        0x1
#define     PHY_CHIRP_MAGCHK_EN_CS_SHIFT                         0
/* Chirp det magnitude range test en */
#define     PHY_CHIRP_MAGCHK_EN_CHIRP_MASK                    0x10
#define     PHY_CHIRP_MAGCHK_EN_CHIRP_SHIFT                      4
/* Chirp det magnitude average cff */
#define     PHY_CHIRP_MAGCHK_AVGCFF_MASK                     0x300
#define     PHY_CHIRP_MAGCHK_AVGCFF_SHIFT                        8
/* Chirp det magnitude range threshold (x.4), max 7 */
#define     PHY_CHIRP_MAGCHK_THR_MASK                       0xf000
#define     PHY_CHIRP_MAGCHK_THR_SHIFT                          12
/* Hold sync detection if (pwrcs or sat cs) and nosig p20 */
#define     PHY_CHIRP_SYNC_HOLD_MASK                       0x10000
#define     PHY_CHIRP_SYNC_HOLD_SHIFT                           16
/* Start chirp detection even if no inband cs det */
#define     PHY_CHIRP_DET_EN_OOB_MASK                     0x100000
#define     PHY_CHIRP_DET_EN_OOB_SHIFT                          20
/* ignore frequency check for chirp pre-detection */
#define     PHY_CHIRP_PRECHK_OOB_MASK                    0x1000000
#define     PHY_CHIRP_PRECHK_OOB_SHIFT                          24
/* new detection rule for chirp detection */
#define     PHY_CHIRP_NEWDET_RULE_MASK                  0x10000000
#define     PHY_CHIRP_NEWDET_RULE_SHIFT                         28
/*
 * Registers for 11B (Only valid for A path)
 */

#define PHY_TARGET_PWR                                      0x2800
/* Target digital power after AGC (dB) */
#define     PHY_TARGER_PWR_MASK                               0x7f
#define     PHY_TARGER_PWR_SHIFT                                 0

#define PHY_SYMB                                            0x2804
/* The number of symbols to search a symbol boundary */
#define     PHY_SYMB_NUM_MASK                                  0x7
#define     PHY_SYMB_NUM_SHIFT                                   0

#define PHY_CCFOE                                           0x2808
/* The number of symbols to estimate coarse CFO */
#define     PHY_CCFOE_NUM_MASK                                0x1f
#define     PHY_CCFOE_NUM_SHIFT                                  0

#define PHY_11B_DET                                         0x280C
/* The weight value for 11b detection */
#define     PHY_11B_DET_WGT_MASK                              0x1f
#define     PHY_11B_DET_WGT_SHIFT                                0
/* The threshold value for 11b detection */
#define     PHY_11B_DET_THRD_MASK                       0x1fffff00
#define     PHY_11B_DET_THRD_SHIFT                               8

#define FW_11B_CSFO_CONV_VINTF0                             0x2810
/* virtual interface 0 */
/* CFO to SFO value (5 / center freq. * pow(2,20)) */
/* center freq | csfo_conv */
/* 2412    |   2174 */
/* 2417    |   2169 */
/* 2422    |   2165 */
/* 2427    |   2160 */
/* 2432    |   2156 */
/* 2437    |   2151 */
/* 2442    |   2147 */
/* 2447    |   2143 */
/* 2452    |   2138 */
/* 2457    |   2134 */
/* 2462    |   2130 */
/* 2467    |   2125 */
/* 2472    |   2121 */
/* 2484    |   2111 */
#define     FW_11B_CSFO_CONV_VAL_VINTF0_MASK                 0xfff
#define     FW_11B_CSFO_CONV_VAL_VINTF0_SHIFT                    0

#define FW_11B_CSFO_CONV_VINTF1                             0x2814
/* virtual interface 1 */
/* CFO to SFO value (5 / center freq. * pow(2,20)) */
#define     FW_11B_CSFO_CONV_VAL_VINTF1_MASK                 0xfff
#define     FW_11B_CSFO_CONV_VAL_VINTF1_SHIFT                    0

#define FW_11B_PLLADJ_VINTF0                                0x2818
/* virtual interface 0 */
/* PLL adjustment frequency for CFO at RX */
/* (freq(Hz) divided by 11e6) * pow (2, 22) */
#define     FW_11B_PLL_ADJ_VINTF0_MASK                     0x3ffff
#define     FW_11B_PLL_ADJ_VINTF0_SHIFT                          0

#define FW_11B_PLLADJ_VINTF1                                0x281C
/* virtual interface 1 */
/* PLL adjustment frequency for CFO at RX */
/* (freq(Hz) divided by 11e6) * pow (2, 22) */
#define     FW_11B_PLL_ADJ_VINTF1_MASK                     0x3ffff
#define     FW_11B_PLL_ADJ_VINTF1_SHIFT                          0

#define FW_11B_TX_PLLADJ_VINTF0                             0x2820
/* virtual interface 0 */
/* PLL adjustment frequency for CFO at TX */
/* freq(Hz) / 20e6 * pow (2, 20) */
#define     FW_11B_TX_PLL_ADJ_VINTF0_MASK                  0x3ffff
#define     FW_11B_TX_PLL_ADJ_VINTF0_SHIFT                       0

#define FW_11B_TX_PLLADJ_VINTF1                             0x2824
/* virtual interface 1 */
/* PLL adjustment frequency for CFO at TX */
/* freq(Hz) / 20e6 * pow (2, 20) */
#define     FW_11B_TX_PLL_ADJ_VINTF1_MASK                  0x3ffff
#define     FW_11B_TX_PLL_ADJ_VINTF1_SHIFT                       0

#define PHY_11B_CONFIG0                                     0x2828
/* dither ON */
#define     PHY_11B_DITHER_ON_MASK                             0x1
#define     PHY_11B_DITHER_ON_SHIFT                              0
/* PSF ON */
#define     PHY_11B_PSF_ON_MASK                                0x2
#define     PHY_11B_PSF_ON_SHIFT                                 1

#define PHY_11B_CONFIG1                                     0x282C
/* Resampler TX offset */
#define     PHY_11B_RESM_TXOFFSET_MASK                    0x3fffff
#define     PHY_11B_RESM_TXOFFSET_SHIFT                          0

#define PHY_11B_CONFIG2                                     0x2830
/* Resampler RX offset */
#define     PHY_11B_RESM_RXOFFSET_MASK                    0x3fffff
#define     PHY_11B_RESM_RXOFFSET_SHIFT                          0

#define FW_11B_ON                                           0x2840
/* 11B RX ON */
/* When the center frequency is 5GHz, 11b should be turned off */
#define     FW_11B_ON_MASK                                     0x1
#define     FW_11B_ON_SHIFT                                      0

#define PHY_11B_RSSI_OFFSET                                 0x2844
/* RSSI offset of 11B PHY (signed 7.3 format) */
#define     PHY_11B_RSSI_OFFSET_MASK                         0x7ff
#define     PHY_11B_RSSI_OFFSET_SHIFT                            0

#define PHY_B11_MON_00                                      0x2900
/* [14: 0] : LENGTH */
#define     PHY_B11_MON_00_MASK                         0xffffffff
#define     PHY_B11_MON_00_SHIFT                                 0

#define PHY_B11_MON_01                                      0x2904
/* [11: 0] : the number of octets */
#define     PHY_B11_MON_01_MASK                         0xffffffff
#define     PHY_B11_MON_01_SHIFT                                 0
/*
 * Registers for TRX MID
 */

#define PHY_PHAMP_CFG0                                      0x3800
/* Use data subcarriers for phase rotation estimation */
#define     PHY_PHAMP_DATA_AIDED_ON_MASK                       0x1
#define     PHY_PHAMP_DATA_AIDED_ON_SHIFT                        0
/* OFF phase estimation */
#define     PHY_PHAMP_PHASE_EST_OFF_MASK                      0x10
#define     PHY_PHAMP_PHASE_EST_OFF_SHIFT                        4
/* OFF amplitude estimation */
#define     PHY_PHAMP_AMP_EST_OFF_MASK                       0x100
#define     PHY_PHAMP_AMP_EST_OFF_SHIFT                          8
/* Minimum MCS for amplitude tracking */
#define     PHY_AMPTRK_MINMCS_MASK                          0xf000
#define     PHY_AMPTRK_MINMCS_SHIFT                             12

#define PHY_PHAMP_CONV                                      0x3804
/* CFO SFO conversion ratio = (20e6 divided by Fc) * pow(2, 22) */
#define     PHY_PHAMP_CONV_RATIO_MASK                      0x3ffff
#define     PHY_PHAMP_CONV_RATIO_SHIFT                           0

#define PHY_PHAMP_PLLADJ                                    0x3808
/* PLL adjustment frequency for CFO */
/* (freq divided by 20e6) * pow (2, 27) */
#define     PHY_PHAMP_PLL_ADJ_MASK                        0xffffff
#define     PHY_PHAMP_PLL_ADJ_SHIFT                              0

#define PHY_CHESMT_CFG0                                     0x3830
/* [5] use smoothing field to turn off smoothing */
/* [4] use beamformed field to turn off smoothing */
/* [3:2] ONOFF control. 0 - smoothing OFF, 1 - smoothing ON, 2 - auto */
/* [1:0] Smoothing filter selection. 0 - filter 0, 1 - filter 1, 2 - auto */
#define     PHY_CHESMT_CFG0_MASK                        0xffffffff
#define     PHY_CHESMT_CFG0_SHIFT                                0

#define PHY_CHPROC3_PARAMETERS                              0x3900
/* ADP SMT enable */
/* active high */
#define     PHY_ADPSMT_ENA_MASK                                0x1
#define     PHY_ADPSMT_ENA_SHIFT                                 0
/* FD Scaler enable */
/* active high */
#define     PHY_FDSCL2_ENA_MASK                                0x2
#define     PHY_FDSCL2_ENA_SHIFT                                 1
/* FD Scaler offset enable */
/* active high */
#define     PHY_FDSCL2_OFFSET_ENA_MASK                         0x4
#define     PHY_FDSCL2_OFFSET_ENA_SHIFT                          2
/* select external target level */
/* active high */
#define     PHY_FDSCL2_EXTERNAL_MASK                          0x10
#define     PHY_FDSCL2_EXTERNAL_SHIFT                            4
/* FD Scaler for nonht */
/* 0.25 DB resolution */
#define     PHY_FDSCL2_TGT_NONHT_MASK                       0xff00
#define     PHY_FDSCL2_TGT_NONHT_SHIFT                           8
/* unsigned */
/* bf_nvar offset */
#define     PHY_FDSCL2_OFFSET_MASK                       0x7ff0000
#define     PHY_FDSCL2_OFFSET_SHIFT                             16

#define PHY_FDSCL2_MINNPWR                                  0x3904
/* minimal noise power */
/* unsigned */
#define     PHY_FDSCL2_MINNPWR_MASK                       0xffffff
#define     PHY_FDSCL2_MINNPWR_SHIFT                             0

#define PHY_FDSCL2_TGT_SS1_M3_0                             0x3908
/* target offset */
/* ss1 MCS0 */
#define     PHY_FDSCL2_TGT_SS1_M0_MASK                        0xff
#define     PHY_FDSCL2_TGT_SS1_M0_SHIFT                          0
/* target offset */
/* ss1 MCS1 */
#define     PHY_FDSCL2_TGT_SS1_M1_MASK                      0xff00
#define     PHY_FDSCL2_TGT_SS1_M1_SHIFT                          8
/* target offset */
/* ss1 MCS2 */
#define     PHY_FDSCL2_TGT_SS1_M2_MASK                    0xff0000
#define     PHY_FDSCL2_TGT_SS1_M2_SHIFT                         16
/* target offset */
/* ss1 MCS3 */
#define     PHY_FDSCL2_TGT_SS1_M3_MASK                  0xff000000
#define     PHY_FDSCL2_TGT_SS1_M3_SHIFT                         24

#define PHY_FDSCL2_TGT_SS1_M7_4                             0x390C
/* target offset */
/* ss1 MCS4 */
#define     PHY_FDSCL2_TGT_SS1_M4_MASK                        0xff
#define     PHY_FDSCL2_TGT_SS1_M4_SHIFT                          0
/* target offset */
/* ss1 MCS5 */
#define     PHY_FDSCL2_TGT_SS1_M5_MASK                      0xff00
#define     PHY_FDSCL2_TGT_SS1_M5_SHIFT                          8
/* target offset */
/* ss1 MCS6 */
#define     PHY_FDSCL2_TGT_SS1_M6_MASK                    0xff0000
#define     PHY_FDSCL2_TGT_SS1_M6_SHIFT                         16
/* target offset */
/* ss1 MCS7 */
#define     PHY_FDSCL2_TGT_SS1_M7_MASK                  0xff000000
#define     PHY_FDSCL2_TGT_SS1_M7_SHIFT                         24

#define PHY_FDSCL2_TGT_SS1_M11_8                            0x3910
/* target offset */
/* ss1 MCS8 */
#define     PHY_FDSCL2_TGT_SS1_M8_MASK                        0xff
#define     PHY_FDSCL2_TGT_SS1_M8_SHIFT                          0
/* target offset */
/* ss1 MCS9 */
#define     PHY_FDSCL2_TGT_SS1_M9_MASK                      0xff00
#define     PHY_FDSCL2_TGT_SS1_M9_SHIFT                          8
/* target offset */
/* ss1 MCS10 */
#define     PHY_FDSCL2_TGT_SS1_M10_MASK                   0xff0000
#define     PHY_FDSCL2_TGT_SS1_M10_SHIFT                        16
/* target offset */
/* ss1 MCS11 */
#define     PHY_FDSCL2_TGT_SS1_M11_MASK                 0xff000000
#define     PHY_FDSCL2_TGT_SS1_M11_SHIFT                        24

#define PHY_FDSCL2_TGT_SS1_M13_12                           0x3914
/* target offset */
/* ss1 MCS12 */
#define     PHY_FDSCL2_TGT_SS1_M12_MASK                       0xff
#define     PHY_FDSCL2_TGT_SS1_M12_SHIFT                         0
/* target offset */
/* ss1 MCS13 */
#define     PHY_FDSCL2_TGT_SS1_M13_MASK                     0xff00
#define     PHY_FDSCL2_TGT_SS1_M13_SHIFT                         8

#define PHY_FDSCL2_REF_SNR_LVL_MCS3_0                       0x3924
/* ref snr level */
/* MCS0 */
#define     PHY_FDSCL2_REF_SNR_M0_MASK                        0xff
#define     PHY_FDSCL2_REF_SNR_M0_SHIFT                          0
/* ref snr level */
/* MCS1 */
#define     PHY_FDSCL2_REF_SNR_M1_MASK                      0xff00
#define     PHY_FDSCL2_REF_SNR_M1_SHIFT                          8
/* ref snr level */
/* MCS2 */
#define     PHY_FDSCL2_REF_SNR_M2_MASK                    0xff0000
#define     PHY_FDSCL2_REF_SNR_M2_SHIFT                         16
/* ref snr level */
/* MCS3 */
#define     PHY_FDSCL2_REF_SNR_M3_MASK                  0xff000000
#define     PHY_FDSCL2_REF_SNR_M3_SHIFT                         24

#define PHY_FDSCL2_REF_SNR_LVL_MCS7_4                       0x3928
/* ref snr level */
/* MCS4 */
#define     PHY_FDSCL2_REF_SNR_M4_MASK                        0xff
#define     PHY_FDSCL2_REF_SNR_M4_SHIFT                          0
/* ref snr level */
/* MCS5 */
#define     PHY_FDSCL2_REF_SNR_M5_MASK                      0xff00
#define     PHY_FDSCL2_REF_SNR_M5_SHIFT                          8
/* ref snr level */
/* MCS6 */
#define     PHY_FDSCL2_REF_SNR_M6_MASK                    0xff0000
#define     PHY_FDSCL2_REF_SNR_M6_SHIFT                         16
/* ref snr level */
/* MCS7 */
#define     PHY_FDSCL2_REF_SNR_M7_MASK                  0xff000000
#define     PHY_FDSCL2_REF_SNR_M7_SHIFT                         24

#define PHY_FDSCL2_REF_SNR_LVL_MCS11_8                      0x392C
/* ref snr level */
/* MCS8 */
#define     PHY_FDSCL2_REF_SNR_M8_MASK                        0xff
#define     PHY_FDSCL2_REF_SNR_M8_SHIFT                          0
/* ref snr level */
/* MCS9 */
#define     PHY_FDSCL2_REF_SNR_M9_MASK                      0xff00
#define     PHY_FDSCL2_REF_SNR_M9_SHIFT                          8
/* ref snr level */
/* MCS10 */
#define     PHY_FDSCL2_REF_SNR_M10_MASK                   0xff0000
#define     PHY_FDSCL2_REF_SNR_M10_SHIFT                        16
/* ref snr level */
/* MCS11 */
#define     PHY_FDSCL2_REF_SNR_M11_MASK                 0xff000000
#define     PHY_FDSCL2_REF_SNR_M11_SHIFT                        24

#define PHY_FDSCL2_REF_SNR_LVL_MCS13_12                     0x3930
/* ref snr level */
/* MCS12 */
#define     PHY_FDSCL2_REF_SNR_M12_MASK                       0xff
#define     PHY_FDSCL2_REF_SNR_M12_SHIFT                         0
/* ref snr level */
/* MCS13 */
#define     PHY_FDSCL2_REF_SNR_M13_MASK                     0xff00
#define     PHY_FDSCL2_REF_SNR_M13_SHIFT                         8
/* FEC LDPC mode */
/* target level */
#define     PHY_FDSCL2_LDPC_LIMIT_MASK                    0xff0000
#define     PHY_FDSCL2_LDPC_LIMIT_SHIFT                         16

#define PHY_FDSCL2_REF_SNR_BND_OFFSET                       0x3934
/* ref snr offset */
/* 18.0*8 */
#define     PHY_FDSCL2_SNR_OFFSET_MASK                       0x7ff
#define     PHY_FDSCL2_SNR_OFFSET_SHIFT                          0
/* FDSCL BND */
/* dB Limit */
#define     PHY_FDSCL2_BND_LIMIT_MASK                     0xff0000
#define     PHY_FDSCL2_BND_LIMIT_SHIFT                          16
/* FEC BCC mode */
/* target level */
#define     PHY_FDSCL2_BCC_LIMIT_MASK                   0xff000000
#define     PHY_FDSCL2_BCC_LIMIT_SHIFT                          24

#define PHY_ADPSMT_CH_COH_THR_MCS0_3                        0x3940
/* channel coherent threshold */
/* MCS0 */
#define     PHY_CH_COH_THR_M0_MASK                            0xff
#define     PHY_CH_COH_THR_M0_SHIFT                              0
/* channel coherent threshold */
/* MCS1 */
#define     PHY_CH_COH_THR_M1_MASK                          0xff00
#define     PHY_CH_COH_THR_M1_SHIFT                              8
/* channel coherent threshold */
/* MCS2 */
#define     PHY_CH_COH_THR_M2_MASK                        0xff0000
#define     PHY_CH_COH_THR_M2_SHIFT                             16
/* channel coherent threshold */
/* MCS3 */
#define     PHY_CH_COH_THR_M3_MASK                      0xff000000
#define     PHY_CH_COH_THR_M3_SHIFT                             24

#define PHY_ADPSMT_CH_COH_THR_MCS4_7                        0x3944
/* channel coherent threshold */
/* MCS4 */
#define     PHY_CH_COH_THR_M4_MASK                            0xff
#define     PHY_CH_COH_THR_M4_SHIFT                              0
/* channel coherent threshold */
/* MCS5 */
#define     PHY_CH_COH_THR_M5_MASK                          0xff00
#define     PHY_CH_COH_THR_M5_SHIFT                              8
/* channel coherent threshold */
/* MCS6 */
#define     PHY_CH_COH_THR_M6_MASK                        0xff0000
#define     PHY_CH_COH_THR_M6_SHIFT                             16
/* channel coherent threshold */
/* MCS7 */
#define     PHY_CH_COH_THR_M7_MASK                      0xff000000
#define     PHY_CH_COH_THR_M7_SHIFT                             24

#define PHY_ADPSMT_CH_COH_THR_MCS8_11                       0x3948
/* channel coherent threshold */
/* MCS8 */
#define     PHY_CH_COH_THR_M8_MASK                            0xff
#define     PHY_CH_COH_THR_M8_SHIFT                              0
/* channel coherent threshold */
/* MCS9 */
#define     PHY_CH_COH_THR_M9_MASK                          0xff00
#define     PHY_CH_COH_THR_M9_SHIFT                              8
/* channel coherent threshold */
/* MCS10 */
#define     PHY_CH_COH_THR_M10_MASK                       0xff0000
#define     PHY_CH_COH_THR_M10_SHIFT                            16
/* channel coherent threshold */
/* MCS11 */
#define     PHY_CH_COH_THR_M11_MASK                     0xff000000
#define     PHY_CH_COH_THR_M11_SHIFT                            24

#define PHY_ADPSMT_CH_COH_THR_OTHERS                        0x394C
/* channel coherent threshold */
/* MCS12 */
#define     PHY_CH_COH_THR_M12_MASK                           0xff
#define     PHY_CH_COH_THR_M12_SHIFT                             0
/* channel coherent threshold */
/* MCS13 */
#define     PHY_CH_COH_THR_M13_MASK                         0xff00
#define     PHY_CH_COH_THR_M13_SHIFT                             8
/* flat channel coherent threshold */
/* LEGACY */
#define     PHY_FLAT_CH_COH_THR_MASK                      0xff0000
#define     PHY_FLAT_CH_COH_THR_SHIFT                           16
/* smooth on/off threshold */
/* SMT */
#define     PHY_SMOOTH_ONOFF_THR_MASK                   0xff000000
#define     PHY_SMOOTH_ONOFF_THR_SHIFT                          24

#define PHY_ADPSMT_NPWR_THR_OFFSET                          0x3950
/* noise power threshold offset to decide noise channel or not (signed 7.3) */
#define     PHY_ADPSMT_NPWR_THR_OFFSET_MASK                  0x7ff
#define     PHY_ADPSMT_NPWR_THR_OFFSET_SHIFT                     0

#define CHPROC_SPUR_NULL_HE0                                0x3960
/* Spur position (in 4x subcarrier indexing) */
#define     PHY_CHPROC_SPURNULL_HE0_POS_MASK                 0xfff
#define     PHY_CHPROC_SPURNULL_HE0_POS_SHIFT                    0
/* Spur width (in 4x subcarrier indexing) */
#define     PHY_CHPROC_SPURNULL_HE0_WIDTH_MASK             0x3f000
#define     PHY_CHPROC_SPURNULL_HE0_WIDTH_SHIFT                 12
/* Spur nulling onoff */
#define     PHY_CHPROC_SPURNULL_ON_MASK                   0x100000
#define     PHY_CHPROC_SPURNULL_ON_SHIFT                        20

#define CHPROC_SPUR_NULL_HE1                                0x3964
/* Spur position (in 4x subcarrier indexing) */
/* If there is only 1 spur, set same value as spur0 */
#define     PHY_CHPROC_SPURNULL_HE1_POS_MASK                 0xfff
#define     PHY_CHPROC_SPURNULL_HE1_POS_SHIFT                    0
/* Spur width (in 4x subcarrier indexing) */
/* If there is only 1 spur, set same value as spur0 */
#define     PHY_CHPROC_SPURNULL_HE1_WIDTH_MASK             0x3f000
#define     PHY_CHPROC_SPURNULL_HE1_WIDTH_SHIFT                 12

#define CHPROC_SPUR_NULL_NHE0                               0x3968
/* Spur position (in 1x subcarrier indexing) */
#define     PHY_CHPROC_SPURNULL_NHE0_POS_MASK                0xfff
#define     PHY_CHPROC_SPURNULL_NHE0_POS_SHIFT                   0
/* Spur width (in 1x subcarrier indexing) */
#define     PHY_CHPROC_SPURNULL_NHE0_WIDTH_MASK            0x3f000
#define     PHY_CHPROC_SPURNULL_NHE0_WIDTH_SHIFT                12

#define CHPROC_SPUR_NULL_NHE1                               0x396C
/* Spur position (in 1x subcarrier indexing) */
/* If there is only 1 spur, set same value as spur0 */
#define     PHY_CHPROC_SPURNULL_NHE1_POS_MASK                0xfff
#define     PHY_CHPROC_SPURNULL_NHE1_POS_SHIFT                   0
/* Spur width (in 1x subcarrier indexing) */
/* If there is only 1 spur, set same value as spur0 */
#define     PHY_CHPROC_SPURNULL_NHE1_WIDTH_MASK            0x3f000
#define     PHY_CHPROC_SPURNULL_NHE1_WIDTH_SHIFT                12
/*
 * Registers for PHY RXBE
 */

#define PHY_RXBE_MANUAL_RESET                               0x1400
/* MANUAL RESET - ACTIVE HIGH */
/* 1'b0 : Default */
/* 1'b1 : Reset */
#define     PHY_RXBE_MANUAL_RESET_MASK                         0x1
#define     PHY_RXBE_MANUAL_RESET_SHIFT                          0

#define FW_LDPCDEC_ITER                                     0x1404
/* The number of LDPC decoding iteration when CR is 1/2 */
#define     FW_LDPCDEC_ITER_CR12_MASK                         0x1f
#define     FW_LDPCDEC_ITER_CR12_SHIFT                           0
/* The number of LDPC decoding iteration when CR is 2/3 */
#define     FW_LDPCDEC_ITER_CR23_MASK                       0x1f00
#define     FW_LDPCDEC_ITER_CR23_SHIFT                           8
/* The number of LDPC decoding iteration when CR is 3/4 */
#define     FW_LDPCDEC_ITER_CR34_MASK                     0x1f0000
#define     FW_LDPCDEC_ITER_CR34_SHIFT                          16
/* The number of LDPC decoding iteration when CR is 5/6 */
#define     FW_LDPCDEC_ITER_CR56_MASK                   0x1f000000
#define     FW_LDPCDEC_ITER_CR56_SHIFT                          24

#define FW_LDPCDEC_STBC_ITER                                0x1408
/* The number of LDPC decoding iteration when CR is 1/2 and STBC */
#define     FW_LDPCDEC_STBC_ITER_CR12_MASK                    0x1f
#define     FW_LDPCDEC_STBC_ITER_CR12_SHIFT                      0
/* The number of LDPC decoding iteration when CR is 2/3 and STBC */
#define     FW_LDPCDEC_STBC_ITER_CR23_MASK                  0x1f00
#define     FW_LDPCDEC_STBC_ITER_CR23_SHIFT                      8
/* The number of LDPC decoding iteration when CR is 3/4 and STBC */
#define     FW_LDPCDEC_STBC_ITER_CR34_MASK                0x1f0000
#define     FW_LDPCDEC_STBC_ITER_CR34_SHIFT                     16
/* The number of LDPC decoding iteration when CR is 5/6 and STBC */
#define     FW_LDPCDEC_STBC_ITER_CR56_MASK              0x1f000000
#define     FW_LDPCDEC_STBC_ITER_CR56_SHIFT                     24

#define FW_VITERBI_CLK_CTRL                                 0x140C
/* BCC RX CLOCK CONTROL MODE */
/* 2'b00 : FIX MODE during frame reception */
/* : clock control as OPBW only but fixed and clock off when RXBE disabled */
/* : 160MHz @ OP20, 160MHz @ OP40, 320MHz @ OP80 */
/* 2'b01 : DYN MODE clock off after VHT SIGB decoding, clock on when data field starts */
/* : 160MHz @ OP20 */
/* : 160MHz @ OP40 except VHT MCS8/9 */
/* : 320MHz @ OP40 with VHT MCS8/9 */
/* : 320MHz @ OP80 */
/* 2'b10 : ALWAYS ON regardless of rxbe_en */
/* 2'b11 : MANUAL MODE, using bcc_rxclk_sel */
#define     FW_BCCCLK_CTRL_MODE_MASK                           0x3
#define     FW_BCCCLK_CTRL_MODE_SHIFT                            0
/* Manual fixed viterbi clock */
/* 2'b00 : 80MHz */
/* 2'b01 : 160MHz */
/* 2'b10 : 320MHz */
/* 2'b11 : N/A */
#define     FW_MANUAL_BCCCLK_CONFIG_MASK                       0xc
#define     FW_MANUAL_BCCCLK_CONFIG_SHIFT                        2

#define FW_LDPC_CTRL                                        0x1410
/* LDPC RX EARLY TERMINATION CONTROL */
/* 1'b0  : Early Termination Disable */
/* 1'b1  : Early Termination Enable */
#define     FW_LDPC_EARLY_TERM_MASK                            0x1
#define     FW_LDPC_EARLY_TERM_SHIFT                             0
/* LDPC RX CLOCK GATING CONTROL */
/* 1'b0  : Clock gating Disable */
/* 1'b1  : Clock gating Enable */
#define     FW_LDPC_CLK_GATE_MASK                              0x2
#define     FW_LDPC_CLK_GATE_SHIFT                               1
/*
 * Registers for RF interface
 */

#define FW_RFI_EN                                           0x0000
/* [0] RFI enable manual control value */
/* [1] RFI Tx enable manual control value */
/* [2] RFI Rx enable manual control value */
#define     RFI_MANUAL_ONOFF_MASK                              0x7
#define     RFI_MANUAL_ONOFF_SHIFT                               0
/* 1 : Use RFI enable manual control enable */
/* 0 : RFI enabled if PHY en */
/* [0] RFI enable [1] Tx enable [2] Rx enable */
#define     RFI_MANUAL_CTL_EN_MASK                            0x70
#define     RFI_MANUAL_CTL_EN_SHIFT                              4
/* RFI clock gating enable */
#define     RFI_CG_EN_MASK                                 0x10000
#define     RFI_CG_EN_SHIFT                                     16
/* RFI LUT clock enable */
#define     RFI_LUT_CLK_EN_MASK                            0x20000
#define     RFI_LUT_CLK_EN_SHIFT                                17
/* RFI ADC clock boost */
#define     RFI_ADC_CLKBOOST_MASK                          0x40000
#define     RFI_ADC_CLKBOOST_SHIFT                              18
/* RFI ADC clock boost */
#define     RFI_DAC_CLKBOOST_MASK                          0x80000
#define     RFI_DAC_CLKBOOST_SHIFT                              19

#define FW_RFI_ACTT_CFG                                     0x0004
/* [6:0] ACTT RF Channel index */
/* [8] ACTT RF power ON */
/* [12] ACTT RF WiFi Datarate (0:20M, 1:40M) */
/* [30:16] TxLDO Prep counter */
/* [31] TxLDO Prep always ON */
#define     RFI_ACTT_CONFIG_MASK                        0xffffffff
#define     RFI_ACTT_CONFIG_SHIFT                                0

#define FW_RFI_ACTT_CFG2                                    0x0008
/* cal-tx-rx-ready-idle */
#define     RFI_ACTT_CONFIG2_MASK                        0x1ffffff
#define     RFI_ACTT_CONFIG2_SHIFT                               0

#define FW_RFI_LUT_INTFV2_CFG                               0x000C
/* RFI DPMEM new interface enable */
/* (Caution - Dump doesn't work while this interface is enabled.) */
#define     RFI_DPMEM_USE_NEWINTF_MASK                       0x100
#define     RFI_DPMEM_USE_NEWINTF_SHIFT                          8

#define FW_RFI_CTRLSIG_CFG_00                               0x0010
/* RFI control signal generator 0 */
/* [21] Control signal generator EN */
/* [20] Active Low (1), Active High (0) */
/* [19:18] Start signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always ON) */
/* [17:16] End signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always OFF) */
/* [15: 8] Start delay */
/* [ 7: 0] End delay */
#define     RFI_CTRLSIG_CFG_00_MASK                       0x3fffff
#define     RFI_CTRLSIG_CFG_00_SHIFT                             0

#define FW_RFI_CTRLSIG_CFG_01                               0x0014
/* RFI control signal generator 1 */
/* [21] Control signal generator EN */
/* [20] Active Low (1), Active High (0) */
/* [19:18] Start signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always ON) */
/* [17:16] End signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always OFF) */
/* [15: 8] Start delay */
/* [ 7: 0] End delay */
#define     RFI_CTRLSIG_CFG_01_MASK                       0x3fffff
#define     RFI_CTRLSIG_CFG_01_SHIFT                             0

#define FW_RFI_CTRLSIG_CFG_02                               0x0018
/* RFI control signal generator 2 */
/* [21] Control signal generator EN */
/* [20] Active Low (1), Active High (0) */
/* [19:18] Start signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always ON) */
/* [17:16] End signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always OFF) */
/* [15: 8] Start delay */
/* [ 7: 0] End delay */
#define     RFI_CTRLSIG_CFG_02_MASK                       0x3fffff
#define     RFI_CTRLSIG_CFG_02_SHIFT                             0

#define FW_RFI_CTRLSIG_CFG_03                               0x001C
/* RFI control signal generator 3 */
/* [21] Control signal generator EN */
/* [20] Active Low (1), Active High (0) */
/* [19:18] Start signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always ON) */
/* [17:16] End signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always OFF) */
/* [15: 8] Start delay */
/* [ 7: 0] End delay */
#define     RFI_CTRLSIG_CFG_03_MASK                       0x3fffff
#define     RFI_CTRLSIG_CFG_03_SHIFT                             0

#define FW_RFI_CTRLSIG_CFG_04                               0x0020
/* RFI control signal generator 04 */
/* [21] Control signal generator EN */
/* [20] Active Low (1), Active High (0) */
/* [19:18] Start signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always ON) */
/* [17:16] End signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always OFF) */
/* [15: 8] Start delay */
/* [ 7: 0] End delay */
#define     RFI_CTRLSIG_CFG_04_MASK                       0x3fffff
#define     RFI_CTRLSIG_CFG_04_SHIFT                             0

#define FW_RFI_CTRLSIG_CFG_05                               0x0024
/* RFI control signal generator 5 */
/* [21] Control signal generator EN */
/* [20] Active Low (1), Active High (0) */
/* [19:18] Start signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always ON) */
/* [17:16] End signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always OFF) */
/* [15: 8] Start delay */
/* [ 7: 0] End delay */
#define     RFI_CTRLSIG_CFG_05_MASK                       0x3fffff
#define     RFI_CTRLSIG_CFG_05_SHIFT                             0

#define FW_RFI_CTRLSIG_CFG_06                               0x0028
/* RFI control signal generator 6 */
/* [21] Control signal generator EN */
/* [20] Active Low (1), Active High (0) */
/* [19:18] Start signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always ON) */
/* [17:16] End signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always OFF) */
/* [15: 8] Start delay */
/* [ 7: 0] End delay */
#define     RFI_CTRLSIG_CFG_06_MASK                       0x3fffff
#define     RFI_CTRLSIG_CFG_06_SHIFT                             0

#define FW_RFI_CTRLSIG_CFG_07                               0x002C
/* RFI control signal generator 7 */
/* [21] Control signal generator EN */
/* [20] Active Low (1), Active High (0) */
/* [19:18] Start signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always ON) */
/* [17:16] End signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always OFF) */
/* [15: 8] Start delay */
/* [ 7: 0] End delay */
#define     RFI_CTRLSIG_CFG_07_MASK                       0x3fffff
#define     RFI_CTRLSIG_CFG_07_SHIFT                             0

#define FW_RFI_CTRLSIG_CFG_08                               0x0030
/* RFI control signal generator 8 */
/* [21] Control signal generator EN */
/* [20] Active Low (1), Active High (0) */
/* [19:18] Start signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always ON) */
/* [17:16] End signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always OFF) */
/* [15: 8] Start delay */
/* [ 7: 0] End delay */
#define     RFI_CTRLSIG_CFG_08_MASK                       0x3fffff
#define     RFI_CTRLSIG_CFG_08_SHIFT                             0

#define FW_RFI_CTRLSIG_CFG_09                               0x0034
/* RFI control signal generator 9 */
/* [21] Control signal generator EN */
/* [20] Active Low (1), Active High (0) */
/* [19:18] Start signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always ON) */
/* [17:16] End signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always OFF) */
/* [15: 8] Start delay */
/* [ 7: 0] End delay */
#define     RFI_CTRLSIG_CFG_09_MASK                       0x3fffff
#define     RFI_CTRLSIG_CFG_09_SHIFT                             0

#define FW_RFI_CTRLSIG_CFG_10                               0x0038
/* RFI control signal generator 10 */
/* [21] Control signal generator EN */
/* [20] Active Low (1), Active High (0) */
/* [19:18] Start signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always ON) */
/* [17:16] End signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always OFF) */
/* [15: 8] Start delay */
/* [ 7: 0] End delay */
#define     RFI_CTRLSIG_CFG_10_MASK                       0x3fffff
#define     RFI_CTRLSIG_CFG_10_SHIFT                             0

#define FW_RFI_CTRLSIG_CFG_11                               0x003C
/* RFI control signal generator 11 */
/* [21] Control signal generator EN */
/* [20] Active Low (1), Active High (0) */
/* [19:18] Start signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always ON) */
/* [17:16] End signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always OFF) */
/* [15: 8] Start delay */
/* [ 7: 0] End delay */
#define     RFI_CTRLSIG_CFG_11_MASK                       0x3fffff
#define     RFI_CTRLSIG_CFG_11_SHIFT                             0

#define FW_RFI_CTRLSIG_CFG_12                               0x0040
/* RFI control signal generator 12 */
/* [21] Control signal generator EN */
/* [20] Active Low (1), Active High (0) */
/* [19:18] Start signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always ON) */
/* [17:16] End signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always OFF) */
/* [15: 8] Start delay */
/* [ 7: 0] End delay */
#define     RFI_CTRLSIG_CFG_12_MASK                       0x3fffff
#define     RFI_CTRLSIG_CFG_12_SHIFT                             0

#define FW_RFI_CTRLSIG_CFG_13                               0x0044
/* RFI control signal generator 13 */
/* [21] Control signal generator EN */
/* [20] Active Low (1), Active High (0) */
/* [19:18] Start signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always ON) */
/* [17:16] End signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always OFF) */
/* [15: 8] Start delay */
/* [ 7: 0] End delay */
#define     RFI_CTRLSIG_CFG_13_MASK                       0x3fffff
#define     RFI_CTRLSIG_CFG_13_SHIFT                             0

#define FW_RFI_CTRLSIG_CFG_14                               0x0048
/* RFI control signal generator 14 */
/* [21] Control signal generator EN */
/* [20] Active Low (1), Active High (0) */
/* [19:18] Start signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always ON) */
/* [17:16] End signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always OFF) */
/* [15: 8] Start delay */
/* [ 7: 0] End delay */
#define     RFI_CTRLSIG_CFG_14_MASK                       0x3fffff
#define     RFI_CTRLSIG_CFG_14_SHIFT                             0

#define FW_RFI_CTRLSIG_CFG_15                               0x004C
/* RFI control signal generator 15 */
/* [21] Control signal generator EN */
/* [20] Active Low (1), Active High (0) */
/* [19:18] Start signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always ON) */
/* [17:16] End signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always OFF) */
/* [15: 8] Start delay */
/* [ 7: 0] End delay */
#define     RFI_CTRLSIG_CFG_15_MASK                       0x3fffff
#define     RFI_CTRLSIG_CFG_15_SHIFT                             0

#define FW_RFI_CTRLSIG_CFG_16                               0x0050
/* RFI control signal generator 16 */
/* [21] Control signal generator EN */
/* [20] Active Low (1), Active High (0) */
/* [19:18] Start signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always ON) */
/* [17:16] End signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always OFF) */
/* [15: 8] Start delay */
/* [ 7: 0] End delay */
#define     RFI_CTRLSIG_CFG_16_MASK                       0x3fffff
#define     RFI_CTRLSIG_CFG_16_SHIFT                             0

#define FW_RFI_CTRLSIG_CFG_17                               0x0054
/* RFI control signal generator 17 */
/* [21] Control signal generator EN */
/* [20] Active Low (1), Active High (0) */
/* [19:18] Start signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always ON) */
/* [17:16] End signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always OFF) */
/* [15: 8] Start delay */
/* [ 7: 0] End delay */
#define     RFI_CTRLSIG_CFG_17_MASK                       0x3fffff
#define     RFI_CTRLSIG_CFG_17_SHIFT                             0

#define FW_RFI_CTRLSIG_CFG_18                               0x0058
/* RFI control signal generator 18 */
/* [21] Control signal generator EN */
/* [20] Active Low (1), Active High (0) */
/* [19:18] Start signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always ON) */
/* [17:16] End signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always OFF) */
/* [15: 8] Start delay */
/* [ 7: 0] End delay */
#define     RFI_CTRLSIG_CFG_18_MASK                       0x3fffff
#define     RFI_CTRLSIG_CFG_18_SHIFT                             0

#define FW_RFI_CTRLSIG_CFG_19                               0x005C
/* RFI control signal generator 19 */
/* [21] Control signal generator EN */
/* [20] Active Low (1), Active High (0) */
/* [19:18] Start signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always ON) */
/* [17:16] End signal selection */
/* (0 : txen, 1: txvalid, 2: rxen, 3: always OFF) */
/* [15: 8] Start delay */
/* [ 7: 0] End delay */
#define     RFI_CTRLSIG_CFG_19_MASK                       0x3fffff
#define     RFI_CTRLSIG_CFG_19_SHIFT                             0

#define FW_RFI_RXLUTV2_ADDR                                 0x0100
/* RxLUT select */
/* use 1 when selected */
#define     RFI_RXLUT_SELECT_MASK                            0x100
#define     RFI_RXLUT_SELECT_SHIFT                               8
/* RxLUT mem addr */
#define     RFI_RXLUT_ADDR_MASK                               0x7f
#define     RFI_RXLUT_ADDR_SHIFT                                 0

#define FW_RFI_RXLUTV2_WRDATA_DC                            0x0104
/* RxLUT wr data ([23:12] DC I, [11:0] DC Q) */
/* (DC and Gain must be updated at the same time. 40 bits operation) */
#define     RFI_RXLUT_WRDATA_DC_MASK                      0xffffff
#define     RFI_RXLUT_WRDATA_DC_SHIFT                            0

#define FW_RFI_RXLUTV2_WRDATA_GAIN                          0x0108
/* RxLUT wr data ([15:8] RFgain, [7] elna, [6:0] dgain) */
/* (DC and Gain must be updated at the same time. 40 bits operation) */
#define     RFI_RXLUT_WRDATA_GAIN_MASK                      0xffff
#define     RFI_RXLUT_WRDATA_GAIN_SHIFT                          0

#define FW_RFI_RXLUTV2_WRSTART                              0x010C
/* RxLUT wr start */
#define     RFI_RXLUT_WRSTART_MASK                             0x1
#define     RFI_RXLUT_WRSTART_SHIFT                              0

#define FW_RFI_RXLUTV2_RDDATA_DC                            0x0110
/* RxLUT rd data ([23:12] DC I, [11:0] DC Q) */
#define     RFI_RXLUT_RDDATA_DC_MASK                      0xffffff
#define     RFI_RXLUT_RDDATA_DC_SHIFT                            0

#define FW_RFI_RXLUTV2_RDDATA_GAIN                          0x0114
/* RxLUT rd data ([15:8] RFgain, [7] elna, [6:0] dgain) */
#define     RFI_RXLUT_RDDATA_GAIN_MASK                      0xffff
#define     RFI_RXLUT_RDDATA_GAIN_SHIFT                          0

#define FW_RFI_RXLUTV2_RADDR                                0x0118
/* RxLUT select */
#define     RFI_RXLUT_SEL_RD_MASK                            0x100
#define     RFI_RXLUT_SEL_RD_SHIFT                               8
/* RxLUT mem addr */
#define     RFI_RXLUT_ADDR_RD_MASK                            0x7f
#define     RFI_RXLUT_ADDR_RD_SHIFT                              0

#define FW_RFI_TXLUTV2_ADDR                                 0x0120
/* TxLUT select */
/* use 1 when selected */
#define     RFI_TXLUT_SELECT_MASK                            0x100
#define     RFI_TXLUT_SELECT_SHIFT                               8
/* TxLUT mem addr */
#define     RFI_TXLUT_ADDR_MASK                               0x7f
#define     RFI_TXLUT_ADDR_SHIFT                                 0

#define FW_RFI_TXLUTV2_WRDATA_DC                            0x0124
/* TxLUT wr data ([23:12] DC I, [11:0] DC Q) */
/* (DC and Gain must be updated at the same time. 40 bits operation) */
#define     RFI_TXLUT_WRDATA_DC_MASK                      0xffffff
#define     RFI_TXLUT_WRDATA_DC_SHIFT                            0

#define FW_RFI_TXLUTV2_WRDATA_GAIN                          0x0128
/* TxLUT wr data ([15:8] RFgain, [7] elna, [6:0] dgain) */
/* (DC and Gain must be updated at the same time. 40 bits operation) */
#define     RFI_TXLUT_WRDATA_GAIN_MASK                      0xffff
#define     RFI_TXLUT_WRDATA_GAIN_SHIFT                          0

#define FW_RFI_TXLUTV2_WRSTART                              0x012C
/* TxLUT wr start */
#define     RFI_TXLUT_WRSTART_MASK                             0x1
#define     RFI_TXLUT_WRSTART_SHIFT                              0

#define FW_RFI_TXLUTV2_RDDATA_DC                            0x0130
/* TxLUT rd data ([23:12] DC I, [11:0] DC Q) */
#define     RFI_TXLUT_RDDATA_DC_MASK                      0xffffff
#define     RFI_TXLUT_RDDATA_DC_SHIFT                            0

#define FW_RFI_TXLUTV2_RDDATA_GAIN                          0x0134
/* TxLUT rd data ([15:8] RFgain, [7] elna, [6:0] dgain) */
#define     RFI_TXLUT_RDDATA_GAIN_MASK                      0xffff
#define     RFI_TXLUT_RDDATA_GAIN_SHIFT                          0

#define FW_RFI_TXLUTV2_RADDR                                0x0138
/* TxLUT select */
#define     RFI_TXLUT_SEL_RD_MASK                            0x100
#define     RFI_TXLUT_SEL_RD_SHIFT                               8
/* TxLUT mem addr */
#define     RFI_TXLUT_ADDR_RD_MASK                            0x7f
#define     RFI_TXLUT_ADDR_RD_SHIFT                              0

#define FW_RFI_DPMEMV2_ADDR                                 0x0140
/* dpmem mem addr */
#define     RFI_DPMEM_ADDR_MASK                              0x7ff
#define     RFI_DPMEM_ADDR_SHIFT                                 0

#define FW_RFI_DPMEMV2_WRDATA                               0x0144
/* DPMEM wr data */
#define     RFI_DPMEM_WRDATA_MASK                       0xffffffff
#define     RFI_DPMEM_WRDATA_SHIFT                               0

#define FW_RFI_DPMEMV2_WRSTART                              0x0148
/* DPMEM wr start */
#define     RFI_DPMEM_WRSTART_MASK                             0x1
#define     RFI_DPMEM_WRSTART_SHIFT                              0

#define FW_RFI_DPMEMV2_RDDATA                               0x014C
/* DPMEM rd data */
#define     RFI_DPMEM_RDDATA_MASK                       0xffffffff
#define     RFI_DPMEM_RDDATA_SHIFT                               0

#define FW_RFI_DPMEMV2_RADDR                                0x0150
/* dpmem mem addr */
#define     RFI_DPMEM_ADDR_RD_MASK                           0x7ff
#define     RFI_DPMEM_ADDR_RD_SHIFT                              0

#define FW_RFI_TXDAC_INTF                                   0x0200
/* [0] DAC interface ON */
/* [1] I channel MSB inversion */
/* [2] Q channel MSB inversion */
/* [3] I channel non-MSB inversion */
/* [4] Q channel non-MSB inversion */
/* [5] Swap IQ channel */
/* [6] Bypass */
/* [7] Reserved */
#define     TXDAC_INTF_CFG_MASK                               0xff
#define     TXDAC_INTF_CFG_SHIFT                                 0

#define FW_RFI_TX_IQ_MIS_COMP                               0x0214
/* IQ mismatch compensation (x.12 format, Iout = Iin + m0*Qin) */
#define     TXIQ_MUL_0_MASK                                 0xffff
#define     TXIQ_MUL_0_SHIFT                                     0
/* IQ mismatch compensation (x.12 format, Qout = m1*Qin) */
#define     TXIQ_MUL_1_MASK                             0xffff0000
#define     TXIQ_MUL_1_SHIFT                                    16

#define FW_RFI_TX_LO_LEAK_MAN                               0x0218
/* Manually set LO leakage compensation parameter (not from LUT) */
#define     TXLO_COMP_MANUAL_MASK                              0x1
#define     TXLO_COMP_MANUAL_SHIFT                               0

#define FW_RFI_TX_LO_LEAK_PARA                              0x021C
/* Q channel LO leakage compensation */
#define     TXIQ_ADD_Q_MAN_MASK                              0xfff
#define     TXIQ_ADD_Q_MAN_SHIFT                                 0
/* I channel LO leakage compensation */
#define     TXIQ_ADD_I_MAN_MASK                          0xfff0000
#define     TXIQ_ADD_I_MAN_SHIFT                                16

#define PHY_RFI_TX_DIG_GAIN_MAN                             0x0220
/* [  7] 1 - manual gain ON, 0 - gain from LUT */
/* [6:0] Gain in 0.5 dB step. -20~43.5 dB (7'd 40 = 0 dB) */
#define     TX_DGAIN_MANUAL_MASK                              0xff
#define     TX_DGAIN_MANUAL_SHIFT                                0

#define FW_RFI_TX_INTP_FLT                                  0x0230
/* 3x interpolation stage enable */
#define     RFITX_3XINTP_ON_MASK                               0x1
#define     RFITX_3XINTP_ON_SHIFT                                0

#define PHY_RFI_TXPWR_MINMAX                                0x0240
/* Tx minimum power (0.5 dB scale, signed) */
#define     RFI_TXMINPWRX2_MASK                               0xff
#define     RFI_TXMINPWRX2_SHIFT                                 0
/* Tx maximum power (0.5 dB scale, signed) */
#define     RFI_TXMAXPWRX2_MASK                             0xff00
#define     RFI_TXMAXPWRX2_SHIFT                                 8
/* 0:0.5 dB, 1:1 dB, 2:2 dB step */
#define     RFI_TXPWR_IDX_STEP_MASK                        0x30000
#define     RFI_TXPWR_IDX_STEP_SHIFT                            16

#define PHY_RFI_TSSI_CFG                                    0x0250
/* Tx TSSI measurement enable */
#define     RFI_TSSI_MSR_EN_MASK                               0x1
#define     RFI_TSSI_MSR_EN_SHIFT                                0
/* max avg cff (use internal gear shifting for a while) */
/* 0 : 1/1024,  1 : 1/2048, 2: 1/4096,  3: 1/8192 */
/* 4 : 1/16384, 5 : 1/32768 */
#define     RFI_TSSI_AVG_CFF_MASK                             0x70
#define     RFI_TSSI_AVG_CFF_SHIFT                               4
/* 1: use i channel, 0 : use q channel */
#define     RFI_TSSI_USE_ICH_MASK                            0x100
#define     RFI_TSSI_USE_ICH_SHIFT                               8

#define PHY_RFI_TSSI_MSR                                    0x0254
/* Tx TSSI measurement result path */
#define     RFI_TSSI_MSR_RES_MASK                       0xffff0000
#define     RFI_TSSI_MSR_RES_SHIFT                              16
/* Tx TSSI measurement  - Tx power in Tx vct */
#define     RFI_TSSI_TXPWR_VCT_MASK                         0xff00
#define     RFI_TSSI_TXPWR_VCT_SHIFT                             8
/* Tx TSSI measurement  - Tx lut index */
#define     RFI_TSSI_TXPWR_IDX_MASK                           0xff
#define     RFI_TSSI_TXPWR_IDX_SHIFT                             0

#define PHY_RFI_TX_SIGGEN_CFG                               0x0300
/* [0] Enable Tone gen 0 ~ [1] Enable Tone gen 1 */
/* [2] Enable Test wave generation from ROM */
#define     TXTONE_GEN_ENA_MASK                                0x7
#define     TXTONE_GEN_ENA_SHIFT                                 0
/* [1] I channel output enable, [0] Q channel enable */
#define     TX_SIGGEN_IQEN_MASK                            0x30000
#define     TX_SIGGEN_IQEN_SHIFT                                16
/* 0 : normal iq, 1 : iq swap, 2: ii mode, 3: qq mode */
#define     TX_SIGGEN_ETC_MASK                             0xc0000
#define     TX_SIGGEN_ETC_SHIFT                                 18
/* Enable test siggen and use test signal for output */
#define     TX_SIGGEN_ENA_MASK                            0x100000
#define     TX_SIGGEN_ENA_SHIFT                                 20
/* 1 - Tx En and valid from signal gen if siggen is ON, 0 - always from Tx */
#define     TX_CTRL_FROM_SIGGEN_MASK                     0x1000000
#define     TX_CTRL_FROM_SIGGEN_SHIFT                           24
/* 0 - Tx signal from siggen or PHY Tx */
/* 1 - Tx signal from Rx RFI output (override other options) */
#define     TX_SIGNAL_FROM_RX_MASK                      0x10000000
#define     TX_SIGNAL_FROM_RX_SHIFT                             28

#define PHY_RFI_TX_TONEGEN_0                                0x0304
/* Phase increase step N. Fout = N x OPBW x 4 / 128 */
#define     TXTONE0_PHSTEP_MASK                               0x7f
#define     TXTONE0_PHSTEP_SHIFT                                 0
/* Initial phase offset */
#define     TXTONE0_PHINIT_MASK                             0x7f00
#define     TXTONE0_PHINIT_SHIFT                                 8

#define PHY_RFI_TX_TONEGEN_1                                0x0308
/* Phase increase step N. Fout = N x OPBW x 4 / 128 */
#define     TXTONE1_PHSTEP_MASK                               0x7f
#define     TXTONE1_PHSTEP_SHIFT                                 0
/* Initial phase offset */
#define     TXTONE1_PHINIT_MASK                             0x7f00
#define     TXTONE1_PHINIT_SHIFT                                 8

#define PHY_RFI_TX_WAVEGEN_CFG_0                            0x0324
/* Number of iteration. 0 : Inf */
#define     TX_WAVEROM_ITER_MASK                            0xffff
#define     TX_WAVEROM_ITER_SHIFT                                0
/* Number samples in 1 packet. 0 : Inf (only use for tonegen) */
#define     TX_WAVEROM_LEN_MASK                         0xffff0000
#define     TX_WAVEROM_LEN_SHIFT                                16

#define PHY_RFI_TX_WAVEGEN_CFG_1                            0x0328
/* Test packet interval (in samples) */
#define     TX_WAVEROM_INTERVAL_MASK                       0xfffff
#define     TX_WAVEROM_INTERVAL_SHIFT                            0
/* Select waveform stored in ROM (for test) */
#define     TX_WAVEROM_SEL_MASK                           0xf00000
#define     TX_WAVEROM_SEL_SHIFT                                20

#define PHY_RFI_TX_WAVEGEN_CFG_2                            0x032C
/* TxEn to TxValid delay (start of packet) */
#define     TX_WAVEROM_SDELAY_MASK                            0xff
#define     TX_WAVEROM_SDELAY_SHIFT                              0
/* TxValid to TxEn delay (end of packet) */
#define     TX_WAVEROM_EDELAY_MASK                          0xff00
#define     TX_WAVEROM_EDELAY_SHIFT                              8
/* Tx siggen (tone and wave) output gain */
#define     TX_SIGGEN_OGAIN_MASK                          0xff0000
#define     TX_SIGGEN_OGAIN_SHIFT                               16

#define PHY_RFI_TX_WAVEGEN_CFG_3                            0x0330
/* Tx waverom trigger delay */
#define     TX_WAVEROM_TRIG_DELAY_MASK                      0xffff
#define     TX_WAVEROM_TRIG_DELAY_SHIFT                          0
/* Tx waverom trigger signal generation for dump */
#define     TX_WAVEROM_TRIG_MODE_MASK                      0x10000
#define     TX_WAVEROM_TRIG_MODE_SHIFT                          16

#define PHY_RFI_TX_WAVEGEN_CFG_4                            0x0334
/* Tx waverom read addr */
#define     TX_WAVEROM_RDADDR_MASK                          0xffff
#define     TX_WAVEROM_RDADDR_SHIFT                              0
/* Tx waverom read mode */
#define     TX_WAVEROM_RDMODE_MASK                         0x10000
#define     TX_WAVEROM_RDMODE_SHIFT                             16

#define PHY_RFI_TX_WAVEGEN_READ                             0x0338
/* Tx waverom read data */
#define     TX_WAVEROM_RDDATA_I_MASK                        0xffff
#define     TX_WAVEROM_RDDATA_I_SHIFT                            0
/* Tx waverom read data */
#define     TX_WAVEROM_RDDATA_Q_MASK                    0xffff0000
#define     TX_WAVEROM_RDDATA_Q_SHIFT                           16

#define PHY_RFI_TX_POLYNL_CFG                               0x0400
/* Enable polynomial nonlinearity function */
#define     POLYNL_ON_MASK                                     0x1
#define     POLYNL_ON_SHIFT                                      0
/* Bypass polynl */
#define     POLYNL_BYPASS_MASK                                 0x2
#define     POLYNL_BYPASS_SHIFT                                  1
/* Enable clipping */
#define     CLIP_ON_MASK                                       0x4
#define     CLIP_ON_SHIFT                                        2
/* Bypass clipping */
#define     CLIP_BYPASS_MASK                                   0x8
#define     CLIP_BYPASS_SHIFT                                    3
/* 1: one = 4096, 0 : one = 2048 */
#define     POLYNL_CFFONE_4096_MASK                           0x10
#define     POLYNL_CFFONE_4096_SHIFT                             4
/* Saturation threshold */
#define     CLIP_SAT_LVL_MASK                           0xffff0000
#define     CLIP_SAT_LVL_SHIFT                                  16

#define PHY_RFI_DPD_CFG2                                    0x0404
/* Mag output scale. Scale 1 = 8d 78 */
#define     CLIP_MAG_SCL_MASK                                 0xff
#define     CLIP_MAG_SCL_SHIFT                                   0
/* Scaled saturation level */
#define     CLIP_SAT_LVL_SCL_MASK                       0xffff0000
#define     CLIP_SAT_LVL_SCL_SHIFT                              16

#define PHY_RFI_DPD_LUTWRIDX                                0x0408
/* Manual DPD cff registers are copied to this address */
/* when dpdcff_copy_man2lut is set. */
#define     DPDCFF_COPY_ADDR_MASK                              0xf
#define     DPDCFF_COPY_ADDR_SHIFT                               0

#define PHY_RFI_DPD_LUTWRITE                                0x040C
/* if 1, current manual dpd registers are copied to dpd lut */
/* this reg return to zero automatically */
#define     DPDCFF_COPY_MAN2LUT_MASK                           0x1
#define     DPDCFF_COPY_MAN2LUT_SHIFT                            0

#define PHY_RFI_DPD_LUTIDX                                  0x0410
/* Tx power offset (0.5 dB step, for temperature cal) */
#define     POLYNL_PWR_OFFSET_MASK                            0xff
#define     POLYNL_PWR_OFFSET_SHIFT                              0
/* minimum Tx power (dBm) for enabling DPD */
#define     POLYNL_MIN_TXPWR_MASK                           0xff00
#define     POLYNL_MIN_TXPWR_SHIFT                               8
/* DPD coefficient manual mode */
#define     DPDCFF_MAN_MODE_MASK                           0x10000
#define     DPDCFF_MAN_MODE_SHIFT                               16

#define PHY_RFI_DPD_OUTSAT                                  0x0414
/* Enable clipping */
#define     OUTCLIP_ON_MASK                                    0x1
#define     OUTCLIP_ON_SHIFT                                     0
/* Bypass clipping */
#define     OUTCLIP_BYPASS_MASK                               0x10
#define     OUTCLIP_BYPASS_SHIFT                                 4
/* Saturation threshold */
#define     OUTCLIP_SAT_LVL_MASK                        0xffff0000
#define     OUTCLIP_SAT_LVL_SHIFT                               16

#define FW_TXLUT_MANSET_NL_CFF00                            0x0420
/* Tx NLDIST CFF q 00 */
#define     MAN_NL_CFF_Q_00_MASK                            0xffff
#define     MAN_NL_CFF_Q_00_SHIFT                                0
/* Tx NLDIST CFF i 00 */
/* x[n] */
#define     MAN_NL_CFF_I_00_MASK                        0xffff0000
#define     MAN_NL_CFF_I_00_SHIFT                               16

#define FW_TXLUT_MANSET_NL_CFF01                            0x0424
/* Tx NLDIST CFF q 01 */
#define     MAN_NL_CFF_Q_01_MASK                            0xffff
#define     MAN_NL_CFF_Q_01_SHIFT                                0
/* Tx NLDIST CFF i 01 */
/* x[n] * |x[n  ]|^2 */
#define     MAN_NL_CFF_I_01_MASK                        0xffff0000
#define     MAN_NL_CFF_I_01_SHIFT                               16

#define FW_TXLUT_MANSET_NL_CFF02                            0x0428
/* Tx NLDIST CFF q 02 */
#define     MAN_NL_CFF_Q_02_MASK                            0xffff
#define     MAN_NL_CFF_Q_02_SHIFT                                0
/* Tx NLDIST CFF i 02 */
/* x[n] * |x[n  ]|^4 */
#define     MAN_NL_CFF_I_02_MASK                        0xffff0000
#define     MAN_NL_CFF_I_02_SHIFT                               16

#define FW_TXLUT_MANSET_NL_CFF10                            0x042C
/* Tx NLDIST CFF q 10 */
#define     MAN_NL_CFF_Q_10_MASK                            0xffff
#define     MAN_NL_CFF_Q_10_SHIFT                                0
/* Tx NLDIST CFF i 10 */
/* x[n-1] */
#define     MAN_NL_CFF_I_10_MASK                        0xffff0000
#define     MAN_NL_CFF_I_10_SHIFT                               16

#define FW_TXLUT_MANSET_NL_CFF11                            0x0430
/* Tx NLDIST CFF q 11 */
#define     MAN_NL_CFF_Q_11_MASK                            0xffff
#define     MAN_NL_CFF_Q_11_SHIFT                                0
/* Tx NLDIST CFF i 11 */
/* x[n-1] * |x[n-1]|^2 */
#define     MAN_NL_CFF_I_11_MASK                        0xffff0000
#define     MAN_NL_CFF_I_11_SHIFT                               16

#define FW_TXLUT_MANSET_NL_CFF12                            0x0434
/* Tx NLDIST CFF q 12 */
#define     MAN_NL_CFF_Q_12_MASK                            0xffff
#define     MAN_NL_CFF_Q_12_SHIFT                                0
/* Tx NLDIST CFF i 12 */
/* x[n-1] * |x[n-1]|^4 */
#define     MAN_NL_CFF_I_12_MASK                        0xffff0000
#define     MAN_NL_CFF_I_12_SHIFT                               16

#define FW_TXLUT_MANSET_NL_CFF20                            0x0438
/* Tx NLDIST CFF q 20 */
#define     MAN_NL_CFF_Q_20_MASK                            0xffff
#define     MAN_NL_CFF_Q_20_SHIFT                                0
/* Tx NLDIST CFF i 20 */
/* x[n-2] */
#define     MAN_NL_CFF_I_20_MASK                        0xffff0000
#define     MAN_NL_CFF_I_20_SHIFT                               16

#define FW_TXLUT_MANSET_NL_CFF21                            0x043C
/* Tx NLDIST CFF q 21 */
#define     MAN_NL_CFF_Q_21_MASK                            0xffff
#define     MAN_NL_CFF_Q_21_SHIFT                                0
/* Tx NLDIST CFF i 21 */
/* x[n-2] * |x[n-2]|^2 */
#define     MAN_NL_CFF_I_21_MASK                        0xffff0000
#define     MAN_NL_CFF_I_21_SHIFT                               16

#define FW_TXLUT_MANSET_NL_CFF22                            0x0440
/* Tx NLDIST CFF q 22 */
#define     MAN_NL_CFF_Q_22_MASK                            0xffff
#define     MAN_NL_CFF_Q_22_SHIFT                                0
/* Tx NLDIST CFF i 22 */
/* x[n-2] * |x[n-2]|^4 */
#define     MAN_NL_CFF_I_22_MASK                        0xffff0000
#define     MAN_NL_CFF_I_22_SHIFT                               16

#define FW_RFI_RXADC_INTF                                   0x0500
/* [0] ADC interface ON */
/* [1] I channel MSB inversion */
/* [2] Q channel MSB inversion */
/* [3] I channel non-MSB inversion */
/* [4] Q channel non-MSB inversion */
/* [5] Swap IQ channel */
/* [6] Bypass */
/* [7] Reserved */
#define     RXADC_INTF_CFG_MASK                               0xff
#define     RXADC_INTF_CFG_SHIFT                                 0

#define FW_RFI_RXDECI_FLT                                   0x0504
/* Rx decimation stage ONOFF */
#define     RFIRX_2XDECI_ON_MASK                               0x1
#define     RFIRX_2XDECI_ON_SHIFT                                0

#define FW_RFI_RX_IQ_MIS_COMP                               0x0514
/* IQ mismatch compensation (x.12 format, Iout = Iin + m0*Qin) */
#define     RXIQ_MUL_0_MASK                                 0xffff
#define     RXIQ_MUL_0_SHIFT                                     0
/* IQ mismatch compensation (x.12 format, Qout = m1*Qin) */
#define     RXIQ_MUL_1_MASK                             0xffff0000
#define     RXIQ_MUL_1_SHIFT                                    16

#define FW_RFI_RX_DC_MAN                                    0x0518
/* Manually set Rx DC compensation parameter (not from LUT) */
#define     RXDC_COMP_MANUAL_MASK                              0x1
#define     RXDC_COMP_MANUAL_SHIFT                               0

#define FW_RFI_RX_DC_PARA                                   0x051C
/* Q channel DC compensation */
#define     RXIQ_ADD_Q_MAN_MASK                              0xfff
#define     RXIQ_ADD_Q_MAN_SHIFT                                 0
/* I channel DC compensation */
#define     RXIQ_ADD_I_MAN_MASK                          0xfff0000
#define     RXIQ_ADD_I_MAN_SHIFT                                16

#define FW_RFI_RXDC_HPF_CFG                                 0x0520
/* DC HPF bw0 (0: hold, 1: 1/4, 2: 1/8, ... 'd 10: 1/2048, 11: clear) */
#define     RX_DCHPF_WGT0_MASK                                 0xf
#define     RX_DCHPF_WGT0_SHIFT                                  0
/* DC HPF bw1 (0: hold, 1: 1/4, 2: 1/8, ... 'd 10: 1/2048, 11: clear) */
#define     RX_DCHPF_WGT1_MASK                                0xf0
#define     RX_DCHPF_WGT1_SHIFT                                  4
/* DC HPF bw2 (0: hold, 1: 1/4, 2: 1/8, ... 'd 10: 1/2048, 11: clear) */
#define     RX_DCHPF_WGT2_MASK                               0xf00
#define     RX_DCHPF_WGT2_SHIFT                                  8
/* DC HPF bw3 (0: hold, 1: 1/4, 2: 1/8, ... 'd 10: 1/2048, 11: clear) */
#define     RX_DCHPF_WGT3_MASK                              0xf000
#define     RX_DCHPF_WGT3_SHIFT                                 12
/* DC Rejection HPF Enable */
#define     RX_DCHPF_ON_MASK                               0x10000
#define     RX_DCHPF_ON_SHIFT                                   16

#define FW_RFI_RXDC_EST                                     0x0524
/* DC estimated in HPF (Q channel) */
#define     RX_DCEST_OUT_Q_MASK                              0xfff
#define     RX_DCEST_OUT_Q_SHIFT                                 0
/* DC estimated in HPF (I channel) */
#define     RX_DCEST_OUT_I_MASK                          0xfff0000
#define     RX_DCEST_OUT_I_SHIFT                                16

#define PHY_RFI_RX_DIG_GAIN_MAN                             0x0528
/* [  7] 1 - manual gain ON, 0 - gain from LUT */
/* [6:0] Gain in 0.5 dB step. -20~43.5 dB (7'd 40 = 0 dB) */
#define     RX_DGAIN_MANUAL_MASK                              0xff
#define     RX_DGAIN_MANUAL_SHIFT                                0

#define PHY_RFI_SATDET_CFG                                  0x052C
/* Saturation detection threshold */
#define     SATDET_LVL_MASK                                  0xfff
#define     SATDET_LVL_SHIFT                                     0
/* Timer for avoiding late saturation detection */
/* After 1st saturation & while agc running, */
/* if sat is not detected within (this value x 25ns) duration, */
#define     SAT_WAIT_T_MASK                                0xf0000
#define     SAT_WAIT_T_SHIFT                                    16
/* Saturation detector input selection */
/* 0 : ADC interface output, 1: IQ DC comp output */
/* 2 : HPF output, 3: Digital gain output */
#define     SAT_DET_INPUT_MASK                            0x300000
#define     SAT_DET_INPUT_SHIFT                                 20

#define PHY_RFI_SATDETV2_CFG                                0x0530
/* 0 : satdet v1,  1: satdet v2 */
#define     SATDET2_SEL_MASK                                   0x1
#define     SATDET2_SEL_SHIFT                                    0
/* 0: satcs after checking inband, 1: satcs immediately */
#define     SATDET2_EARLY_SATCS_MASK                          0x10
#define     SATDET2_EARLY_SATCS_SHIFT                            4
/* 1: check saturation during freqchk, 0 : only freq test */
#define     SATDET2_CHK_INBANDSAT_MASK                        0x20
#define     SATDET2_CHK_INBANDSAT_SHIFT                          5
/* wait count after 1st sat (1 = 50ns) */
#define     SATDET2_FREQCHK_DLY_MASK                         0xf00
#define     SATDET2_FREQCHK_DLY_SHIFT                            8
/* frequency check window size (1 = 50ns) */
#define     SATDET2_FREQCHK_WIN_MASK                        0xf000
#define     SATDET2_FREQCHK_WIN_SHIFT                           12
/* zero crossing count threshold */
#define     SATDET2_ZEROCROSS_CNTTHR_MASK                 0xff0000
#define     SATDET2_ZEROCROSS_CNTTHR_SHIFT                      16
/* zero crossing hysteresis window */
#define     SATDET2_ZEROCROSS_HISTTHR_MASK              0xff000000
#define     SATDET2_ZEROCROSS_HISTTHR_SHIFT                     24

#define PHY_RFI_TESTMSR_CFG                                 0x0600
/* Tone measurement 0 gain (0-1x, 1-2x, 2-4x, 3-8x) */
#define     TONEMSR0_GAIN_MASK                                 0x7
#define     TONEMSR0_GAIN_SHIFT                                  0
/* Tone measurement 1 gain (0-1x, 1-2x, 2-4x, 3-8x) */
#define     TONEMSR1_GAIN_MASK                                0x70
#define     TONEMSR1_GAIN_SHIFT                                  4
/* Enable tone measurement */
#define     TEST_MSR_EN_MASK                                 0x100
#define     TEST_MSR_EN_SHIFT                                    8
/* 1: Free-running measurement, 0: measure 1 time */
#define     MSR_FREERUN_MASK                                0x1000
#define     MSR_FREERUN_SHIFT                                   12

#define PHY_RFI_TESTMSR_START                               0x0604
/* Start measurement (one time) */
#define     MSR_ONE_STRB_MASK                                  0x1
#define     MSR_ONE_STRB_SHIFT                                   0

#define PHY_RFI_TESTMSR_TONECFG                             0x0608
/* Phase increase step N. Fout = N x OPBW x 4 / 128 */
#define     TONEMSR0_PHSTEP_MASK                              0x7f
#define     TONEMSR0_PHSTEP_SHIFT                                0
/* Initial phase offset */
#define     TONEMSR0_PHINIT_MASK                            0x7f00
#define     TONEMSR0_PHINIT_SHIFT                                8
/* 1: Use Tx test signal, 0: Use Rx RFI internal tone gen */
#define     TONEMSR0_EXTIN_MASK                             0x8000
#define     TONEMSR0_EXTIN_SHIFT                                15
/* Phase increase step N. Fout = N x OPBW x 4 / 128 */
#define     TONEMSR1_PHSTEP_MASK                          0x7f0000
#define     TONEMSR1_PHSTEP_SHIFT                               16
/* Initial phase offset */
#define     TONEMSR1_PHINIT_MASK                        0x7f000000
#define     TONEMSR1_PHINIT_SHIFT                               24
/* 1: Use Tx test signal, 0: Use Rx RFI internal tone gen */
#define     TONEMSR1_EXTIN_MASK                         0x80000000
#define     TONEMSR1_EXTIN_SHIFT                                31

#define PHY_RFI_TESTMSR_IDX                                 0x0610
/* Measurement index used for checking if updated */
#define     TESTMSR_INDEX_MASK                              0xffff
#define     TESTMSR_INDEX_SHIFT                                  0

#define PHY_RFI_RX_DCMSR                                    0x0614
/* DC measurement result Q */
#define     DCMSR_AVG_Q_MASK                                 0xfff
#define     DCMSR_AVG_Q_SHIFT                                    0
/* DC measurement result I */
#define     DCMSR_AVG_I_MASK                             0xfff0000
#define     DCMSR_AVG_I_SHIFT                                   16

#define PHY_RFI_PWRMSR_I                                    0x0618
/* Power measurement of I channel */
#define     TEST_PWRMSR_II_MASK                           0xffffff
#define     TEST_PWRMSR_II_SHIFT                                 0

#define PHY_RFI_PWRMSR_Q                                    0x061C
/* Power measurement of Q channel */
#define     TEST_PWRMSR_QQ_MASK                           0xffffff
#define     TEST_PWRMSR_QQ_SHIFT                                 0

#define PHY_RFI_PWRMSR_IQ                                   0x0620
/* Average of I x Q */
#define     TEST_PWRMSR_IQ_MASK                           0xffffff
#define     TEST_PWRMSR_IQ_SHIFT                                 0

#define PHY_RFI_PWRMSR_SUM                                  0x0624
/* Power measurement of I+Q channel */
#define     TEST_PWRMSR_SUM_MASK                          0xffffff
#define     TEST_PWRMSR_SUM_SHIFT                                0

#define PHY_RFI_TONEMSR0_ICH                                0x0628
/* Average of RxI x sin(Ftone0) */
#define     TEST_TONEMSR0_CORR_IS_MASK                      0xffff
#define     TEST_TONEMSR0_CORR_IS_SHIFT                          0
/* Average of RxI x cos(Ftone0) */
#define     TEST_TONEMSR0_CORR_IC_MASK                  0xffff0000
#define     TEST_TONEMSR0_CORR_IC_SHIFT                         16

#define PHY_RFI_TONEMSR0_QCH                                0x062C
/* Average of RxQ x sin(Ftone0) */
#define     TEST_TONEMSR0_CORR_QS_MASK                      0xffff
#define     TEST_TONEMSR0_CORR_QS_SHIFT                          0
/* Average of RxQ x cos(Ftone0) */
#define     TEST_TONEMSR0_CORR_QC_MASK                  0xffff0000
#define     TEST_TONEMSR0_CORR_QC_SHIFT                         16

#define PHY_RFI_TONEMSR0_SUM                                0x0630
/* Tone crosscorrelation (imag) */
#define     TEST_TONEMSR0_CORR_QSUM_MASK                    0xffff
#define     TEST_TONEMSR0_CORR_QSUM_SHIFT                        0
/* Tone crosscorrelation (real) */
#define     TEST_TONEMSR0_CORR_ISUM_MASK                0xffff0000
#define     TEST_TONEMSR0_CORR_ISUM_SHIFT                       16

#define PHY_RFI_TONEMSR1_ICH                                0x0634
/* Average of RxI x sin(Ftone1) */
#define     TEST_TONEMSR1_CORR_IS_MASK                      0xffff
#define     TEST_TONEMSR1_CORR_IS_SHIFT                          0
/* Average of RxI x cos(Ftone1) */
#define     TEST_TONEMSR1_CORR_IC_MASK                  0xffff0000
#define     TEST_TONEMSR1_CORR_IC_SHIFT                         16

#define PHY_RFI_TONEMSR1_QCH                                0x0638
/* Average of RxQ x sin(Ftone1) */
#define     TEST_TONEMSR1_CORR_QS_MASK                      0xffff
#define     TEST_TONEMSR1_CORR_QS_SHIFT                          0
/* Average of RxQ x cos(Ftone1) */
#define     TEST_TONEMSR1_CORR_QC_MASK                  0xffff0000
#define     TEST_TONEMSR1_CORR_QC_SHIFT                         16

#define PHY_RFI_TONEMSR1_SUM                                0x063C
/* Tone crosscorrelation (imag) */
#define     TEST_TONEMSR1_CORR_QSUM_MASK                    0xffff
#define     TEST_TONEMSR1_CORR_QSUM_SHIFT                        0
/* Tone crosscorrelation (real) */
#define     TEST_TONEMSR1_CORR_ISUM_MASK                0xffff0000
#define     TEST_TONEMSR1_CORR_ISUM_SHIFT                       16

#define PHY_RFI_MEMDP_CFG0                                  0x0700
/* Dump block enable */
#define     MEMDP_ENA_MASK                                     0x1
#define     MEMDP_ENA_SHIFT                                      0
/* 0 : waveform only (continuous dump until triggered, 2048 sample) */
/* 1 : waveform and state (continuous, state, tx vct, rx vct, 1024 sample) */
/* 2 : timestamp and state (event based, state, tx vct and rx vct, 1024 sample) */
/* (state, signal can be configured by PHY register setting) */
#define     MEMDP_MODE_SEL_MASK                               0x30
#define     MEMDP_MODE_SEL_SHIFT                                 4
/* 1 - dump start when triggered */
/* 0 - dump stop when triggered */
#define     MEMDP_START_FROM_TRIG_MASK                        0x40
#define     MEMDP_START_FROM_TRIG_SHIFT                          6
/* Trigger signal selection for waveform dump */
/* 0 : Tx enable (rising edge) */
/* 1 : Rx enable (rising edge) */
/* 2 : Rx Packet start (SAT CS detected) */
/* 3 : Rx Packet start (OFDM Corr CS detected) */
/* 4 : Rx Packet start (11B CS detected) */
/* 5 : Rx Packet start (PWR CS detected) */
/* 6 : Rx Packet start (AGC done) */
/* 7 : Strong signal CS during Rx */
/* 8 : Fine AGC start */
/* 9 : Rx Packet end (normal OFDM Rx end) */
/* 10 : Rx Packet end (normal 11B Rx end) */
/* 11 : Rx Packet end (noise, interference, unsupported) */
/* 12 : CCA rising edge */
/* 13 : CCA falling edge */
/* 14 : PHY s/w reset */
/* 15~29 : Reserved */
/* 30 : Manual trigger (memdp_man_trigger) */
/* 31 : Free running dump (stop after memdp_trig_iter) */
#define     MEMDP_TRIG_SEL_MASK                             0x1f00
#define     MEMDP_TRIG_SEL_SHIFT                                 8
/* The detection method of the trigger signal */
/* 0 : Rising edge */
/* 1 : Falling edge */
/* 2 : Level */
#define     MEMDP_TRIG_EDGE_SEL_MASK                        0x6000
#define     MEMDP_TRIG_EDGE_SEL_SHIFT                           13
/* Free running mode stops after this iteration */
/* if 0, don't stop. */
#define     MEMDP_TRIG_ITER_MASK                           0xf0000
#define     MEMDP_TRIG_ITER_SHIFT                               16
/* Waveform dump length after trigger */
#define     MEMDP_LEN_AFTERTRIG_MASK                    0x7ff00000
#define     MEMDP_LEN_AFTERTRIG_SHIFT                           20

#define PHY_RFI_MEMDP_CFG1                                  0x0704
/* 0 : No decimation (o4x clock) */
/* 1 : 1/2 decimation */
/* 2 : 1/4 decimation */
/* 3 : 1/8 decimation */
/* 4 : 1/16 decimation */
#define     MEMDP_WAVE_DECI_MASK                               0x7
#define     MEMDP_WAVE_DECI_SHIFT                                0
/* 1 : from PHY input, 0 : RF intf Rx output */
#define     MEMDP_SRC_FROM_PHY_MASK                           0x10
#define     MEMDP_SRC_FROM_PHY_SHIFT                             4
/* 0 : adc out, 1 : rfi out, 2 : rssi adc, 3 : adc i and agc gain */
/* 4 : adc q and agc gain, 5 :rfi i and agc gain, 6 : rfi q and agc gain, */
/* 7 : reserved */
#define     MEMDP_SRC_SEL_RFI_MASK                           0x700
#define     MEMDP_SRC_SEL_RFI_SHIFT                              8

#define PHY_RFI_MEMDP_RUN                                   0x0710
/* Dump start (doesn't return to 0. set to 0 before reading mem.) */
#define     MEMDP_RUN_MASK                                     0x1
#define     MEMDP_RUN_SHIFT                                      0

#define PHY_RFI_MEMDP_MANTRIG                               0x0714
/* Manual trigger signal (return to 0) */
#define     MEMDP_MANTRIG_MASK                                 0x1
#define     MEMDP_MANTRIG_SHIFT                                  0

#define PHY_RFI_MEMDP_ENDADDR                               0x0718
/* Last address of current dump operation */
#define     MEMDP_ENDADDR_MASK                               0x7ff
#define     MEMDP_ENDADDR_SHIFT                                  0
/* If 1, DUMP block is running (not triggered yet) */
#define     MEMDP_RUNNING_MASK                              0x1000
#define     MEMDP_RUNNING_SHIFT                                 12

#define PHY_RFI_CTRL_EXT                                    0x0720
/* For future use. */
/* In FPGA, [0] adc clock phase inv, [1] dac clock phase inv. */
#define     RFI_CTRL_EXT_MASK                               0xffff
#define     RFI_CTRL_EXT_SHIFT                                   0

#define FW_RFSPI0_CONFIG                                    0x0730
/* rfspi0 reset (active high) */
#define     FW_RFSPI0_RESET_MASK                               0x1
#define     FW_RFSPI0_RESET_SHIFT                                0
/* rfspi0 enable (active high) */
#define     FW_RFSPI0_EN_MASK                                  0x2
#define     FW_RFSPI0_EN_SHIFT                                   1
/* spi_clk frequency divider */
/* divide rfspi0_clk by pow(2,(clk_div+1)) */
#define     FW_RFSPI0_CLK_DIV_MASK                            0x1c
#define     FW_RFSPI0_CLK_DIV_SHIFT                              2
/* Operation Mode */
/* 1'b0 : AD9963 */
/* 1'b1 : MAX2829 */
#define     FW_RFSPI0_MODE_MASK                               0x20
#define     FW_RFSPI0_MODE_SHIFT                                 5
/* Interrupt Mode */
/* 1'b1(default) : Active-high Interrupt */
/* 1'b0          : Active-low  Interrupt */
#define     FW_RFSPI0_IRQ_MODE_MASK                           0x40
#define     FW_RFSPI0_IRQ_MODE_SHIFT                             6
/* RF SPI master core selection */
/* 1'b1(default) : to use RF Generalized SPI */
/* 1'b0          : to use RF Customized SPI */
#define     FW_RFSPI0_CORE_SEL_MASK                     0x80000000
#define     FW_RFSPI0_CORE_SEL_SHIFT                            31

#define FW_RFSPI0_CMD                                       0x0734
/* READ/WRITE */
/* 1'b0 : WRITE */
/* 1'b1 : READ only for AD9963 */
#define     FW_RFSPI0_RW_MASK                                  0x1
#define     FW_RFSPI0_RW_SHIFT                                   0
/* SPI Register Address */
/* AD9963 : 13-bit Address (5 MSB bits should be 1'b0) */
/* MAX2829 : 4-bit Address */
#define     FW_RFSPI0_ADDR_MASK                             0x3ffe
#define     FW_RFSPI0_ADDR_SHIFT                                 1
/* Number of Bytes ONLY For Burst Write of AD9963 */
/* 0 : A Byte for a written address */
/* 1 : Two Byte for a written address(addr.) and  addr.+1 */
/* 2 : Three Byte for a written address(addr.) and  addr.+1, addr.+2 */
/* 3 : Four Byte for a written address(addr.) and  addr.+1, addr.+2, addr.+3 */
#define     FW_RFSPI0_AFE_WR_NBYTE_MASK                     0xc000
#define     FW_RFSPI0_AFE_WR_NBYTE_SHIFT                        14

#define FW_RFSPI0_AFE_WRITE_DATA                            0x0738
/* AFE, AD9963 Write Data at fw_rfspi0_addr */
#define     FW_RFSPI0_AFE_WR_DATA0_MASK                       0xff
#define     FW_RFSPI0_AFE_WR_DATA0_SHIFT                         0
/* AFE, AD9963 Write Data at fw_rfspi0_addr+1 */
#define     FW_RFSPI0_AFE_WR_DATA1_MASK                     0xff00
#define     FW_RFSPI0_AFE_WR_DATA1_SHIFT                         8
/* AFE, AD9963 Write Data at fw_rfspi0_addr+2 */
#define     FW_RFSPI0_AFE_WR_DATA2_MASK                   0xff0000
#define     FW_RFSPI0_AFE_WR_DATA2_SHIFT                        16
/* AFE, AD9963 Write Data at fw_rfspi0_addr+3 */
#define     FW_RFSPI0_AFE_WR_DATA3_MASK                 0xff000000
#define     FW_RFSPI0_AFE_WR_DATA3_SHIFT                        24

#define FW_RFSPI0_RF_WRITE_DATA                             0x073C
/* RF, MAX2920 Write Data at fw_rfspi0_addr */
#define     FW_RFSPI0_RF_WR_DATA_MASK                       0x3fff
#define     FW_RFSPI0_RF_WR_DATA_SHIFT                           0

#define FW_RFSPI0_RUN                                       0x0740
/* RF SPI ACTIVATION */
/* Active High, autonomously set to 0 after this field is asserted */
#define     FW_RFSPI0_RUN_MASK                                 0x1
#define     FW_RFSPI0_RUN_SHIFT                                  0

#define FW_RFSPI0_AFE_READ_DATA                             0x0744
/* AFE, AD9963 Read Data at fw_rfspi0_addr */
#define     FW_RFSPI0_AFE_RD_DATA_MASK                        0xff
#define     FW_RFSPI0_AFE_RD_DATA_SHIFT                          0

#define FW_RFSPI0_CTRL_RD                                   0x0748
/* Read RF SPI STATUS */
/* 0 : STANDBY */
/* 1 : Running */
#define     FW_RFSPI0_BUSY_MASK                                0x1
#define     FW_RFSPI0_BUSY_SHIFT                                 0

#define FW_RFSPI0_IRQ_CLR                                   0x074C
/* Interrupt Clear (return to default) */
#define     FW_RFSPI0_IRQ_CLEAR_MASK                           0x1
#define     FW_RFSPI0_IRQ_CLEAR_SHIFT                            0

#define FW_RFSPI1_CONFIG                                    0x0750
/* rfspi1 reset (active high) */
#define     FW_RFSPI1_RESET_MASK                               0x1
#define     FW_RFSPI1_RESET_SHIFT                                0
/* rfspi1 enable (active high) */
#define     FW_RFSPI1_EN_MASK                                  0x2
#define     FW_RFSPI1_EN_SHIFT                                   1
/* spi_clk frequency divider */
/* divide rfspi1_clk by pow(2,(clk_div+1)) */
#define     FW_RFSPI1_CLK_DIV_MASK                            0x1c
#define     FW_RFSPI1_CLK_DIV_SHIFT                              2
/* Operation Mode */
/* 1'b0 : AD9963 */
/* 1'b1 : MAX2829 */
#define     FW_RFSPI1_MODE_MASK                               0x20
#define     FW_RFSPI1_MODE_SHIFT                                 5
/* Interrupt Mode */
/* 1'b1(default) : Active-high Interrupt */
/* 1'b0          : Active-low  Interrupt */
#define     FW_RFSPI1_IRQ_MODE_MASK                           0x40
#define     FW_RFSPI1_IRQ_MODE_SHIFT                             6
/* RF SPI master core selection */
/* 1'b1(default) : to use RF Generalized SPI */
/* 1'b0          : to use RF Customized SPI */
#define     FW_RFSPI1_CORE_SEL_MASK                     0x80000000
#define     FW_RFSPI1_CORE_SEL_SHIFT                            31

#define FW_RFSPI1_CMD                                       0x0754
/* READ/WRITE */
/* 1'b0 : WRITE */
/* 1'b1 : READ only for AD9963 */
#define     FW_RFSPI1_RW_MASK                                  0x1
#define     FW_RFSPI1_RW_SHIFT                                   0
/* SPI Register Address */
/* AD9963 : 13-bit Address (5 MSB bits should be 1'b0) */
/* MAX2829 : 4-bit Address */
#define     FW_RFSPI1_ADDR_MASK                             0x3ffe
#define     FW_RFSPI1_ADDR_SHIFT                                 1
/* Number of Bytes ONLY For Burst Write of AD9963 */
/* 0 : A Byte for a written address */
/* 1 : Two Byte for a written address(addr.) and  addr.+1 */
/* 2 : Three Byte for a written address(addr.) and  addr.+1, addr.+2 */
/* 3 : Four Byte for a written address(addr.) and  addr.+1, addr.+2, addr.+3 */
#define     FW_RFSPI1_AFE_WR_NBYTE_MASK                     0xc000
#define     FW_RFSPI1_AFE_WR_NBYTE_SHIFT                        14

#define FW_RFSPI1_AFE_WRITE_DATA                            0x0758
/* AFE, AD9963 Write Data at fw_rfspi1_addr */
#define     FW_RFSPI1_AFE_WR_DATA0_MASK                       0xff
#define     FW_RFSPI1_AFE_WR_DATA0_SHIFT                         0
/* AFE, AD9963 Write Data at fw_rfspi1_addr+1 */
#define     FW_RFSPI1_AFE_WR_DATA1_MASK                     0xff00
#define     FW_RFSPI1_AFE_WR_DATA1_SHIFT                         8
/* AFE, AD9963 Write Data at fw_rfspi1_addr+2 */
#define     FW_RFSPI1_AFE_WR_DATA2_MASK                   0xff0000
#define     FW_RFSPI1_AFE_WR_DATA2_SHIFT                        16
/* AFE, AD9963 Write Data at fw_rfspi1_addr+3 */
#define     FW_RFSPI1_AFE_WR_DATA3_MASK                 0xff000000
#define     FW_RFSPI1_AFE_WR_DATA3_SHIFT                        24

#define FW_RFSPI1_RF_WRITE_DATA                             0x075C
/* RF, MAX2920 Write Data at fw_rfspi1_addr */
#define     FW_RFSPI1_RF_WR_DATA_MASK                       0x3fff
#define     FW_RFSPI1_RF_WR_DATA_SHIFT                           0

#define FW_RFSPI1_RUN                                       0x0760
/* RF SPI ACTIVATION */
/* Active High, autonomously set to 0 after this field is asserted */
#define     FW_RFSPI1_RUN_MASK                                 0x1
#define     FW_RFSPI1_RUN_SHIFT                                  0

#define FW_RFSPI1_AFE_READ_DATA                             0x0764
/* AFE, AD9963 Read Data at fw_rfspi1_addr */
#define     FW_RFSPI1_AFE_RD_DATA_MASK                        0xff
#define     FW_RFSPI1_AFE_RD_DATA_SHIFT                          0

#define FW_RFSPI1_CTRL_RD                                   0x0768
/* Read RF SPI STATUS */
/* 0 : STANDBY */
/* 1 : Running */
#define     FW_RFSPI1_BUSY_MASK                                0x1
#define     FW_RFSPI1_BUSY_SHIFT                                 0

#define FW_RFSPI1_IRQ_CLR                                   0x076C
/* Interrupt Clear (Return to default) */
#define     FW_RFSPI1_IRQ_CLEAR_MASK                           0x1
#define     FW_RFSPI1_IRQ_CLEAR_SHIFT                            0

#define FW_RF_GSPI0_CONFIG                                  0x0780
/* General master SPI reset (active high) */
#define     FW_RF_GSPI0_RESET_MASK                             0x1
#define     FW_RF_GSPI0_RESET_SHIFT                              0
/* General master SPI enable (active high) */
#define     FW_RF_GSPI0_EN_MASK                                0x2
#define     FW_RF_GSPI0_EN_SHIFT                                 1
/* spi_clk frequency divider */
/* divide G-SPI clock  by pow(2,(clk_div+1)) */
#define     FW_RF_GSPI0_CLK_DIV_MASK                          0x1c
#define     FW_RF_GSPI0_CLK_DIV_SHIFT                            2
/* SPI CLOCK MODE */
/* 1'b0 : DATA captured @ Clock Rising edge */
/* 1'b1 : DATA captured @ Clock falling edge */
#define     FW_RF_GSPI0_CLK_MODE_MASK                         0x20
#define     FW_RF_GSPI0_CLK_MODE_SHIFT                           5
/* Interrupt Mode */
/* 1'b1(default) : Active-high Interrupt */
/* 1'b0          : Active-low  Interrupt */
#define     FW_RF_GSPI0_IRQ_MODE_MASK                         0x40
#define     FW_RF_GSPI0_IRQ_MODE_SHIFT                           6
/* SPI Interrupt Enable */
/* 1'b1(default) : Enable interrupt */
/* 1'b0          : Disable interrupt */
#define     FW_RF_GSPI0_IRQ_EN_MASK                           0x80
#define     FW_RF_GSPI0_IRQ_EN_SHIFT                             7
/* TIME INTERVAL From CE assertion To 1st clock rising */
/* T_rfspi_clock * 2^(val) */
/*  */
#define     FW_RF_GSPI0_CE2CLK_T1_CTRL_MASK                 0x7f00
#define     FW_RF_GSPI0_CE2CLK_T1_CTRL_SHIFT                     8
/* Chip Enable Polarity */
/* 0 : Active low */
/* 1 : Active high */
#define     FW_RF_GSPI0_CE_POL_CTRL_MASK                    0x8000
#define     FW_RF_GSPI0_CE_POL_CTRL_SHIFT                       15
/* Number of SPI DATA BITS */
/* default : 32 bits */
/* nbits <= 32 bits */
#define     FW_RF_GSPI0_SPI_NBITS_MASK                    0x3f0000
#define     FW_RF_GSPI0_SPI_NBITS_SHIFT                         16
/* chip selection among 4 option lines */
/* default : LSB B0 */
/* only 1 bit shall be high */
#define     FW_RF_GSPI0_SPI_CE_SEL_MASK                  0x7800000
#define     FW_RF_GSPI0_SPI_CE_SEL_SHIFT                        23

#define FW_RF_GSPI0_RUN                                     0x0784
/* RF GSPI ACTIVATION */
/* Active High, autonomously set to 0 after this field is asserted */
#define     FW_RF_GSPI0_RUN_MASK                               0x1
#define     FW_RF_GSPI0_RUN_SHIFT                                0

#define FW_RF_GSPI0_STATUS                                  0x0788
/* SPI status */
/* HIGH : BUSY */
/* LOW : IDLE/STANDBY */
#define     FW_RF_GSPI0_STATUS_MASK                            0x1
#define     FW_RF_GSPI0_STATUS_SHIFT                             0
/* GSPI FSM STATUS */
/* 3'd0 : IDLE */
/* 3'd1 : STANDBY */
/* 3'd2 : START */
/* 3'd3 : WR_PHASE */
/* 3'd4 : READ_PHASE */
/* 3'd5 : EOF */
#define     FW_RF_GSPI0_FSM_MASK                              0x70
#define     FW_RF_GSPI0_FSM_SHIFT                                4

#define FW_RF_GSPI0_WR_DATA                                 0x078C
/* SPI WRITE DATA */
/* MSB first */
/* MSB -> MSB-1 -> MSB-2 -> ... -> 1 -> 0 */
#define     FW_RF_GSPI0_WR_DATA_MASK                    0xffffffff
#define     FW_RF_GSPI0_WR_DATA_SHIFT                            0

#define FW_RF_GSPI0_RD_DATA                                 0x0790
/* SPI READ DATA */
/* MSB first */
/* clk  |^^^|___|^^|___|^^^|__ */
/* miso MSB--> MSB-1->MSB-2 --> ... */
#define     FW_RF_GSPI0_RD_DATA_MASK                    0xffffffff
#define     FW_RF_GSPI0_RD_DATA_SHIFT                            0

#define FW_RF_GSPI0_IRQ_CLR                                 0x0794
/* Interrupt Clear (return to default) */
#define     FW_RF_GSPI0_IRQ_CLR_MASK                           0x1
#define     FW_RF_GSPI0_IRQ_CLR_SHIFT                            0

#define FW_RF_GSPI0_WR_NBITS                                0x0798
/* SPI_FRAME = WRITE PHASE + READ PHASE */
/* N-bit of WRITE PHASE */
/* Bi-directional IO buffer control */
#define     FW_RF_GSPI0_WR_NBITS_MASK                         0x1f
#define     FW_RF_GSPI0_WR_NBITS_SHIFT                           0

#define FW_RF_GSPI1_CONFIG                                  0x07B0
/* rfspi1 reset (active high) */
#define     FW_RF_GSPI1_RESET_MASK                             0x1
#define     FW_RF_GSPI1_RESET_SHIFT                              0
/* rfspi1 enable (active high) */
#define     FW_RF_GSPI1_EN_MASK                                0x2
#define     FW_RF_GSPI1_EN_SHIFT                                 1
/* spi_clk frequency divider */
/* divide rfspi1_clk by pow(2,(clk_div+1)) */
#define     FW_RF_GSPI1_CLK_DIV_MASK                          0x1c
#define     FW_RF_GSPI1_CLK_DIV_SHIFT                            2
/* SPI CLOCK MODE */
/* 1'b0 : DATA captured @ Clock Rising edge */
/* 1'b1 : DATA captured @ Clock falling edge */
#define     FW_RF_GSPI1_CLK_MODE_MASK                         0x20
#define     FW_RF_GSPI1_CLK_MODE_SHIFT                           5
/* Interrupt Mode */
/* 1'b1(default) : Active-high Interrupt */
/* 1'b0          : Active-low  Interrupt */
#define     FW_RF_GSPI1_IRQ_MODE_MASK                         0x40
#define     FW_RF_GSPI1_IRQ_MODE_SHIFT                           6
/* SPI Interrupt Enable */
/* 1'b1(default) : Enable interrupt */
/* 1'b0          : Disable interrupt */
#define     FW_RF_GSPI1_IRQ_EN_MASK                           0x80
#define     FW_RF_GSPI1_IRQ_EN_SHIFT                             7
/* TIME INTERVAL From CE assertion To 1st clock rising */
/* T_rfspi_clock * 2^(val) */
/*  */
#define     FW_RF_GSPI1_CE2CLK_T1_CTRL_MASK                 0x7f00
#define     FW_RF_GSPI1_CE2CLK_T1_CTRL_SHIFT                     8
/* Chip Enable Polarity */
/* 0 : Active low */
/* 1 : Active high */
#define     FW_RF_GSPI1_CE_POL_CTRL_MASK                    0x8000
#define     FW_RF_GSPI1_CE_POL_CTRL_SHIFT                       15
/* Number of SPI DATA BITS */
/* default : 32 bits */
/* nbits <= 32 bits */
#define     FW_RF_GSPI1_SPI_NBITS_MASK                    0x3f0000
#define     FW_RF_GSPI1_SPI_NBITS_SHIFT                         16
/* chip selection among 4 option lines */
/* default : LSB B0 */
/* only 1 bit shall be high */
#define     FW_RF_GSPI1_SPI_CE_SEL_MASK                  0x7800000
#define     FW_RF_GSPI1_SPI_CE_SEL_SHIFT                        23

#define FW_RF_GSPI1_RUN                                     0x07B4
/* RF GSPI ACTIVATION */
/* Active High, autonomously set to 0 after this field is asserted */
#define     FW_RF_GSPI1_RUN_MASK                               0x1
#define     FW_RF_GSPI1_RUN_SHIFT                                0

#define FW_RF_GSPI1_STATUS                                  0x07B8
/* SPI status */
/* HIGH : BUSY */
/* LOW : IDLE/STANDBY */
#define     FW_RF_GSPI1_STATUS_MASK                            0x1
#define     FW_RF_GSPI1_STATUS_SHIFT                             0
/* GSPI FSM STATUS */
/* 3'd0 : IDLE */
/* 3'd1 : STANDBY */
/* 3'd2 : START */
/* 3'd3 : WR_PHASE */
/* 3'd4 : READ_PHASE */
/* 3'd5 : EOF */
#define     FW_RF_GSPI1_FSM_MASK                              0x70
#define     FW_RF_GSPI1_FSM_SHIFT                                4

#define FW_RFSPI1_REV11_WR_DATA                             0x07BC
/* SPI WRITE DATA */
/* MSB first */
/* MSB -> MSB-1 -> MSB-2 -> ... -> 1 -> 0 */
#define     FW_RF_GSPI1_WR_DATA_MASK                    0xffffffff
#define     FW_RF_GSPI1_WR_DATA_SHIFT                            0

#define FW_RF_GSPI1_RD_DATA                                 0x07C0
/* SPI READ DATA */
/* MSB first */
/* clk   |^^^|___|^^^|___|^^^|__ */
/* miso MSB-->MSB-1-->MSB-2--> ... */
#define     FW_RF_GSPI1_RD_DATA_MASK                    0xffffffff
#define     FW_RF_GSPI1_RD_DATA_SHIFT                            0

#define FW_RF_GSPI1_IRQ_CLR                                 0x07C4
/* Interrupt Clear (return to default) */
#define     FW_RF_GSPI1_IRQ_CLR_MASK                           0x1
#define     FW_RF_GSPI1_IRQ_CLR_SHIFT                            0

#define FW_RF_GSPI1_WR_NBITS                                0x07C8
/* SPI_FRAME = WRITE PHASE + READ PHASE */
/* N-bit of WRITE PHASE */
/* Bi-directional IO buffer control */
#define     FW_RF_GSPI1_WR_NBITS_MASK                         0x1f
#define     FW_RF_GSPI1_WR_NBITS_SHIFT                           0
/*
 * Registers for RF interface - TX LUTs
 */

#define FW_TXLUT_CFG0                                       0x1000
/* TX LUT output manual mode */
#define     TXLUT_OUT_MANUAL_MODE_MASK                         0x1
#define     TXLUT_OUT_MANUAL_MODE_SHIFT                          0
/* TX LUT input (Tx gain) manual mode */
#define     TXLUT_IN_MANUAL_MODE_MASK                         0x10
#define     TXLUT_IN_MANUAL_MODE_SHIFT                           4
/* Tx LUT index (Gain input) for manual input mode */
#define     TXLUT_IN_INDEX_MASK                             0x7f00
#define     TXLUT_IN_INDEX_SHIFT                                 8
/* Tx LUT Read/Write Mode (1: APB, 0: access by Tx gain) */
#define     TXLUT_MEMORY_ACCESS_EN_MASK                    0x10000
#define     TXLUT_MEMORY_ACCESS_EN_SHIFT                        16
/* Tx LUT use even index only */
#define     TXLUT_EVEN_IDX_ONLY_MASK                     0x1000000
#define     TXLUT_EVEN_IDX_ONLY_SHIFT                           24
/* 1: Use manual Tx settings in Rx mode, 0: hold TxLUT values */
#define     USE_MANSET_IN_RX_MASK                       0x10000000
#define     USE_MANSET_IN_RX_SHIFT                              28

#define FW_TXLUT_MANSET_LO                                  0x1004
/* Tx LO compensation Q (manual mode) */
#define     MAN_LO_COMP_Q_MASK                               0xfff
#define     MAN_LO_COMP_Q_SHIFT                                  0
/* Tx LO compensation I (manual mode) */
#define     MAN_LO_COMP_I_MASK                           0xfff0000
#define     MAN_LO_COMP_I_SHIFT                                 16

#define FW_TXLUT_MANSET_TXGAIN_OUT                          0x1008
/* Tx digital gain (manual mode) */
#define     MAN_TX_DIG_GAIN_MASK                              0x7f
#define     MAN_TX_DIG_GAIN_SHIFT                                0
/* Tx ext lna gain (manual mode) */
#define     MAN_TX_ELNA_GAIN_MASK                             0x80
#define     MAN_TX_ELNA_GAIN_SHIFT                               7
/* Tx RF gain control signal (manual mode) */
#define     MAN_TX_RF_GAIN_MASK                             0xff00
#define     MAN_TX_RF_GAIN_SHIFT                                 8
/*
 * Registers for RF interface - RX LUTs
 */

#define FW_RXLUT_CFG0                                       0x0800
/* RX LUT output manual mode */
#define     RXLUT_OUT_MANUAL_MODE_MASK                         0x1
#define     RXLUT_OUT_MANUAL_MODE_SHIFT                          0
/* RX LUT input (Rx gain) manual mode */
#define     RXLUT_IN_MANUAL_MODE_MASK                         0x10
#define     RXLUT_IN_MANUAL_MODE_SHIFT                           4
/* Rx LUT index (Gain input) for manual input mode */
#define     RXLUT_IN_INDEX_MASK                             0x7f00
#define     RXLUT_IN_INDEX_SHIFT                                 8
/* Rx LUT Read/Write Mode (1: APB, 0: access by Rx gain) */
#define     RXLUT_MEMORY_ACCESS_EN_MASK                    0x10000
#define     RXLUT_MEMORY_ACCESS_EN_SHIFT                        16
/* 1: Use gain_updated from PHY top, 0: internal gain_updated */
#define     RXLUT_USE_EXT_UPDATED_MASK                    0x100000
#define     RXLUT_USE_EXT_UPDATED_SHIFT                         20
/* 1: Use even index only, 0: Use all index */
#define     RXLUT_EVEN_IDX_ONLY_MASK                     0x1000000
#define     RXLUT_EVEN_IDX_ONLY_SHIFT                           24
/* 1: Use manual Rx settings in Tx mode, 0: hold RxLUT values */
#define     USE_MANSET_IN_TX_MASK                       0x10000000
#define     USE_MANSET_IN_TX_SHIFT                              28

#define FW_RXLUT_MANSET_DC                                  0x0810
/* Rx DC compensation Q (manual mode, digital) */
#define     MAN_RXDC_DIGCOMP_Q_MASK                          0xfff
#define     MAN_RXDC_DIGCOMP_Q_SHIFT                             0
/* Rx DC compensation I (manual mode, digital) */
#define     MAN_RXDC_DIGCOMP_I_MASK                      0xfff0000
#define     MAN_RXDC_DIGCOMP_I_SHIFT                            16

#define FW_RXLUT_MANSET_RXGAIN_OUT                          0x0814
/* Rx digital gain (manual mode) */
#define     MAN_RX_DIG_GAIN_MASK                              0xff
#define     MAN_RX_DIG_GAIN_SHIFT                                0
/* Rx RF gain control signal (manual mode) */
#define     MAN_RX_RF_GAIN_MASK                             0xff00
#define     MAN_RX_RF_GAIN_SHIFT                                 8

#define FW_RXHPF_CFG1                                       0x0820
/* HPF state 0 duration (N+1 clocks in p2x) */
#define     HPF_ST0_TO_ST1_DURATION_MASK                      0xff
#define     HPF_ST0_TO_ST1_DURATION_SHIFT                        0
/* HPF state 1 duration (N+1 clocks in p2x) */
#define     HPF_ST1_TO_ST2_DURATION_MASK                    0xff00
#define     HPF_ST1_TO_ST2_DURATION_SHIFT                        8

#define FW_RXHPF_CFG2                                       0x0824
/* HPF BW selection for state 0. [3:2] for RF HPF (if any), [1:0] for dig HPF */
#define     HPF_ST0_BW_CTRL_MASK                               0xf
#define     HPF_ST0_BW_CTRL_SHIFT                                0
/* HPF BW selection for state 1. [3:2] for RF HPF (if any), [1:0] for dig HPF */
#define     HPF_ST1_BW_CTRL_MASK                              0xf0
#define     HPF_ST1_BW_CTRL_SHIFT                                4
/* HPF BW selection for state 2. [3:2] for RF HPF (if any), [1:0] for dig HPF */
#define     HPF_ST2_BW_CTRL_MASK                             0xf00
#define     HPF_ST2_BW_CTRL_SHIFT                                8
/* HPF BW selection for state 3. [3:2] for RF HPF (if any), [1:0] for dig HPF */
#define     HPF_ST3_BW_CTRL_MASK                            0xf000
#define     HPF_ST3_BW_CTRL_SHIFT                               12

#define FW_RXGAIN_LNAMASK                                   0x0828
/* Mask LNA control signals. Used for adjusting gain change latency */
#define     RXGAIN_LNA_MASK_MASK                          0xffffff
#define     RXGAIN_LNA_MASK_SHIFT                                0
#endif /*_PHYREGS_H_*/
