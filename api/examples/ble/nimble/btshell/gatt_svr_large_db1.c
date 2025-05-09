/*
 * Copyright (c) 2024-2025 Senscomm, Inc. All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "services/gatt/ble_svc_gatt.h"
#include "services/gap/ble_svc_gap.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/ble_gatt.h"
#include "btshell.h"


#define PTS_UUID_128(uuid16) 										\
	((const ble_uuid_t *) (&(ble_uuid128_t) BLE_UUID128_INIT(   	\
			0xef, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23, 0x01,         \
			0x00, 0x00, 0x00, 0x00, (uint8_t)uuid16, (uint8_t)(uuid16 >> 8), 0x00, 0x00 \
			)))


enum large_db1_attr_idx {
	/* Service D */
	IDX_D_CHR1, 		// [READ|READ_AUTHEN]
	IDX_D_CHR2, 		// [READ|READ_AUTHOR]

	/* Service ATT */
	IDX_ATT_CHR1, 		// [INDICATION]
	IDX_ATT_CHR2, 		// [READ]
	IDX_ATT_CHR3, 		// [READ]
	IDX_ATT_CHR4, 		// [READ]

	/* Service A */
	IDX_A_CHR1,			// [READ|READ_ENC]
	IDX_A_CHR2,			// [READ|WRITE]
	IDX_A_CHR3,			// [WRITE]
	IDX_A_CHR4,			// [WRITE]

	/* Service B.4 */
	IDX_B4_CHR1,		// [WRITE]

	/* Service GAP */
	IDX_GAP_CHR1,		// [READ]
	IDX_GAP_CHR2,		// [READ]
	IDX_GAP_CHR3,		// [READ]
	IDX_GAP_CHR4,		// [READ]

	/* Service B.3 */
	IDX_B3_CHR1,		// [READ|WRITE_NO_RSP|WRITE|NOTIFY]

	/* Service B.1 */
	IDX_B1_CHR1,		// [READ|WRITE|WRITE_AUTHEN]
	IDX_B1_CHR2,		// [READ|WRITE]
	IDX_B1_CHR2_DSC1,	// [READ|WRITE]
	IDX_B1_CHR3,		// []
	IDX_B1_CHR3_DSC1,	// []
	IDX_B1_CHR4,		// [READ]
	IDX_B1_CHR4_DSC1,	// [READ]

	/* Service B.2 */
	IDX_B2_CHR1,		// [READ|WRITE|RELIABLE_WRITE|AUX_WRITE|WRITE_AUTHOR]
	IDX_B2_CHR1_DSC1,	// [READ]
	IDX_B2_CHR1_DSC2,	// [READ|WRITE]
	IDX_B2_CHR1_DSC3,	// [READ]
	IDX_B2_CHR1_DSC4,	// [READ]

	/* Service B.5 */
	IDX_B5_CHR1,		// [READ|WRITE|WRITE_ENC]
	IDX_B5_CHR1_DSC1, 	// [READ|WRITE|READ_AUTHEN|WRITE_AUTHEN]
	IDX_B5_CHR1_DSC2,	// [READ|WRITE|READ_AUTHOR|WRITE_AUTHOR]
	IDX_B5_CHR1_DSC3,	// [READ|WRITE|READ_ENC|WRITE_ENC]

	/* Service C.1 */
	IDX_C1_CHR1,		// [READ|WRITE|RELIABLE_WRITE]
	IDX_C1_CHR1_DSC1,	// [READ]
	IDX_C1_CHR1_DSC2,	// [READ|WRITE]
	IDX_C1_CHR1_DSC3,	// [WRITE]

	/* Service F */
	IDX_F_CHR1,			// [READ]
	IDX_F_CHR1_DSC1,	// [READ]
	IDX_F_CHR2,			// [READ|WRITE]
	IDX_F_CHR2_DSC1,	// [READ]
	IDX_F_CHR3,			// [READ|WRITE]
	IDX_F_CHR3_DSC1,	// [READ]
	IDX_F_CHR4,			// [READ|WRITE]
	IDX_F_CHR4_DSC1,	// [READ]
	IDX_F_CHR5,			// [READ]
	IDX_F_CHR5_DSC1,	// [READ]
	IDX_F_CHR6,			// [READ|WRITE]
	IDX_F_CHR7,			// [READ]
	IDX_F_CHR7_DSC1,	// [READ]
	IDX_F_CHR8,			// [READ]
	IDX_F_CHR8_DSC1,	// [READ]
	IDX_F_CHR9,			// [READ]
	IDX_F_CHR9_DSC1,	// [READ]
	IDX_F_CHR10,		// [READ]
	IDX_F_CHR10_DSC1,	// [READ]

	/* Service C.2 */
	IDX_C2_CHAR1,		// [READ]
	IDX_C2_CHAR2,		// [READ|WRITE]
	IDX_C2_CHAR2_DSC1,	// [READ|WRITE]
	IDX_C2_CHAR3,		// [READ|WRITE]
	IDX_C2_CHAR3_DSC1,	// [READ|WRITE]
	IDX_C2_CHAR4,		// [READ|WRITE]
	IDX_C2_CHAR4_DSC1,	// [READ|WRITE]
	IDX_C2_CHAR5,		// [READ|WRITE]
	IDX_C2_CHAR5_DSC1,	// [READ|WRITE]
	IDX_C2_CHAR6,		// [READ|WRITE]
	IDX_C2_CHAR6_DSC1,	// [READ|WRITE]
	IDX_C2_CHAR7,		// [READ|WRITE]
	IDX_C2_CHAR7_DSC1,	// [READ|WRITE]
	IDX_C2_CHAR8,		// [READ|WRITE|READ_AUTHEN|WRITE_AUTHEN]
	IDX_C2_CHAR8_DSC1,	// [READ|WRITE|READ_AUTHEN|WRITE_AUTHEN]
	IDX_C2_CHAR9,		// [READ|WRITE|READ_AUTHOR|WRITE_AUTHOR]
	IDX_C2_CHAR9_DSC1,	// [READ|WRITE|READ_AUTHOR|WRITE_AUTHOR]
	IDX_C2_CHAR10,		// [READ|WRITE|READ_ENC|WRITE_ENC]
	IDX_C2_CHAR10_DSC1, // [READ|WRITE|READ_ENC|WRITE_ENC]
	IDX_C2_CHAR11,		// [READ|WRITE]
	IDX_C2_CHAR11_DSC1, // [READ|WRITE]
	IDX_C2_CHAR12,		// [READ|WRITE]
	IDX_C2_CHAR12_DSC1, // [READ|WRITE]
	IDX_C2_CHAR13,		// [READ|WRITE]
	IDX_C2_CHAR13_DSC1, // [READ|WRITE]
	IDX_C2_CHAR14,		// [READ|WRITE]
	IDX_C2_CHAR14_DSC1, // [READ|WRITE]
	IDX_C2_CHAR15,		// [READ|WRITE|READ_AUTHEN|WRITE_AUTHEN]
	IDX_C2_CHAR15_DSC1,	// [READ|WRITE|READ_AUTHEN|WRITE_AUTHEN]
	IDX_C2_CHAR16,		// [READ|WRITE|READ_AUTHOR|WRITE_AUTHOR]
	IDX_C2_CHAR16_DSC1,	// [READ|WRITE|READ_AUTHOR|WRITE_AUTHOR]
	IDX_C2_CHAR17,		// [READ|WRITE|READ_ENC|WRITE_ENC]
	IDX_C2_CHAR17_DSC1, // [READ|WRITE|READ_ENC|WRITE_ENC]
	IDX_C2_CHAR18,		// [READ|WRITE]
	IDX_C2_CHAR18_DSC1, // [READ|WRITE]
	IDX_C2_CHAR19,		// [READ|WRITE]
	IDX_C2_CHAR19_DSC1, // [READ|WRITE]
	IDX_C2_CHAR20,		// [READ|WRITE]
	IDX_C2_CHAR20_DSC1, // [READ|WRITE]
	IDX_C2_CHAR21,		// [READ|WRITE]
	IDX_C2_CHAR21_DSC1, // [READ|WRITE]
	IDX_C2_CHAR22,		// [READ|WRITE]
	IDX_C2_CHAR22_DSC1, // [READ|WRITE]
};

struct large_db1_data {
	uint8_t authorization;

	uint8_t srv_d_chr1;				// [READ|READ_AUTHEN]
	uint8_t srv_d_chr2;             // [READ|READ_AUTHOR]

	uint8_t srv_att_chr1[4];		// [INDICATION]
	uint8_t srv_att_chr2;			// [READ]
	uint8_t srv_att_chr3;			// [READ]
	uint8_t srv_att_chr4[16];		// [READ]

	uint8_t srv_a_chr1;				// [READ|READ_ENC]
	uint8_t srv_a_chr2[512];        // [READ|WRITE]
	uint8_t srv_a_chr3[50];         // [WRITE]
	uint8_t srv_a_chr4;             // [WRITE]

	uint8_t srv_b4_chr1;			// [WRITE]

	uint8_t srv_gap_chr1[13];		// [READ]
	uint8_t dev_name_len;
	uint16_t srv_gap_chr2;          // [READ]
	uint16_t srv_gap_chr3[4];       // [READ]
	uint8_t srv_gap_chr4;           // [READ]

	uint8_t srv_b3_chr1;			// [READ|WRITE_NO_RSP|WRITE|NOTIFY]

	uint8_t srv_b1_chr1;			// [READ|WRITE|WRITE_AUTHEN]
	uint8_t srv_b1_chr2;            // [READ|WRITE]
	uint16_t srv_b1_chr2_dsc1;      // [READ|WRITE]
	uint8_t srv_b1_chr3[49];        // []
	uint8_t srv_b1_chr3_dsc1[49];   // []
	uint8_t srv_b1_chr4[43];        // [READ]
	uint8_t srv_b1_chr4_dsc1[43];   // [READ]

	uint8_t srv_b2_chr1;			// [READ|WRITE|RELIABLE_WRITE|AUX_WRITE|WRITE_AUTHOR]
	uint16_t srv_b2_chr1_dsc1;      // [READ]
	uint8_t srv_b2_chr1_dsc2[26];   // [READ|WRITE]
	uint8_t srv_b2_chr1_dsc3[7];    // [READ]
	uint8_t srv_b2_chr1_dsc4;       // [READ]

	uint8_t srv_b5_chr1;			// [READ|WRITE|WRITE_ENC]
	uint8_t srv_b5_chr1_dsc1;       // [READ|WRITE|READ_AUTHEN|WRITE_AUTHEN]
	uint8_t srv_b5_chr1_dsc2;       // [READ|WRITE|READ_AUTHOR|WRITE_AUTHOR]
	uint8_t srv_b5_chr1_dsc3;       // [READ|WRITE|READ_ENC|WRITE_ENC]

	uint8_t srv_c1_chr1;			// [READ|WRITE|RELIABLE_WRITE]
	uint16_t srv_c1_chr1_dsc1;      // [READ]
	uint8_t srv_c1_chr1_dsc2;       // [READ|WRITE]
	uint8_t srv_c1_chr1_dsc3;       // [WRITE]

	uint8_t srv_f_chr1[10];			// [READ]
	uint8_t srv_f_chr1_dsc1[7];     // [READ]
	uint8_t srv_f_chr2;             // [READ|WRITE]
	uint8_t srv_f_chr2_dsc1[7];     // [READ]
	uint16_t srv_f_chr3;            // [READ|WRITE]
	uint8_t srv_f_chr3_dsc1[7];     // [READ]
	uint32_t srv_f_chr4;            // [READ|WRITE]
	uint8_t srv_f_chr4_dsc1[7];     // [READ]
	uint8_t srv_f_chr5[7];          // [READ]
	uint16_t srv_f_chr5_dsc1[3];    // [READ]
	uint8_t srv_f_chr6;             // [READ|WRITE]
	uint8_t srv_f_chr7[6];			// [READ]
	uint16_t srv_f_chr7_dsc1[2];    // [READ]
	uint8_t srv_f_chr8[2];			// [READ]
	uint16_t srv_f_chr8_dsc1[2];    // [READ]
	uint8_t srv_f_chr9[5];			// [READ]
	uint16_t srv_f_chr9_dsc1[2];    // [READ]
	uint8_t srv_f_chr10[3];			// [READ]
	uint16_t srv_f_chr10_dsc1[2];   // [READ]

	uint8_t srv_c2_chr1;			// [READ]
	uint8_t srv_c2_chr2[21];        // [READ|WRITE]
	uint8_t srv_c2_chr2_dsc1[21];   // [READ|WRITE]
	uint8_t srv_c2_chr3[22];        // [READ|WRITE]
	uint8_t srv_c2_chr3_dsc1[22];   // [READ|WRITE]
	uint8_t srv_c2_chr4[23];        // [READ|WRITE]
	uint8_t srv_c2_chr4_dsc1[23];   // [READ|WRITE]
	uint8_t srv_c2_chr5[43];        // [READ|WRITE]
	uint8_t srv_c2_chr5_dsc1[43];   // [READ|WRITE]
	uint8_t srv_c2_chr6[44];        // [READ|WRITE]
	uint8_t srv_c2_chr6_dsc1[44];   // [READ|WRITE]
	uint8_t srv_c2_chr7[45];        // [READ|WRITE]
	uint8_t srv_c2_chr7_dsc1[46];   // [READ|WRITE]
	uint8_t srv_c2_chr8[43];        // [READ|WRITE|READ_AUTHEN|WRITE_AUTHEN]
	uint8_t srv_c2_chr8_dsc1[43];   // [READ|WRITE|READ_AUTHEN|WRITE_AUTHEN]
	uint8_t srv_c2_chr9[44];        // [READ|WRITE|READ_AUTHOR|WRITE_AUTHOR]
	uint8_t srv_c2_chr9_dsc1[44];   // [READ|WRITE|READ_AUTHOR|WRITE_AUTHOR]
	uint8_t srv_c2_chr10[45];       // [READ|WRITE|READ_ENC|WRITE_ENC]
	uint8_t srv_c2_chr10_dsc1[45];  // [READ|WRITE|READ_ENC|WRITE_ENC]
	uint8_t srv_c2_chr11[46];       // [READ|WRITE]
	uint8_t srv_c2_chr11_dsc1[46];  // [READ|WRITE]
	uint8_t srv_c2_chr12[47];       // [READ|WRITE]
	uint8_t srv_c2_chr12_dsc1[48];  // [READ|WRITE]
	uint8_t srv_c2_chr13[48];       // [READ|WRITE]
	uint8_t srv_c2_chr13_dsc1[48];  // [READ|WRITE]
	uint8_t srv_c2_chr14[95];       // [READ|WRITE]
	uint8_t srv_c2_chr14_dsc1[95];  // [READ|WRITE]
	uint8_t srv_c2_chr15[49];       // [READ|WRITE|READ_AUTHEN|WRITE_AUTHEN]
	uint8_t srv_c2_chr15_dsc1[49];  // [READ|WRITE|READ_AUTHEN|WRITE_AUTHEN]
	uint8_t srv_c2_chr16[50];       // [READ|WRITE|READ_AUTHOR|WRITE_AUTHOR]
	uint8_t srv_c2_chr16_dsc1[50];  // [READ|WRITE|READ_AUTHOR|WRITE_AUTHOR]
	uint8_t srv_c2_chr17[51];       // [READ|WRITE|READ_ENC|WRITE_ENC]
	uint8_t srv_c2_chr17_dsc1[51];  // [READ|WRITE|READ_ENC|WRITE_ENC]
	uint8_t srv_c2_chr18[64];       // [READ|WRITE]
	uint8_t srv_c2_chr18_dsc1[64];  // [READ|WRITE]
	uint8_t srv_c2_chr19[64];       // [READ|WRITE]
	uint8_t srv_c2_chr19_dsc1[64];  // [READ|WRITE]
	uint8_t srv_c2_chr20[128];      // [READ|WRITE]
	uint8_t srv_c2_chr20_dsc1[128]; // [READ|WRITE]
	uint8_t srv_c2_chr21[94];       // [READ|WRITE]
	uint8_t srv_c2_chr21_dsc1[94];  // [READ|WRITE]
	uint8_t srv_c2_chr22[127];      // [READ|WRITE]
	uint8_t srv_c2_chr22_dsc1[127]; // [READ|WRITE]
};

struct large_db1_data large_db1 = {
	/* Authorization enable after encryption changed */
	.authorization = 0,

	/* Service D data */
	.srv_d_chr1 = 0x0c,
	.srv_d_chr2 = 0x0b,

	/* Service ATT Profile data */
	.srv_att_chr1 = { 0x01, 0x00, 0xff, 0xff },
	.srv_att_chr2 = 0,
	.srv_att_chr3 = 0,
	.srv_att_chr4 = {
		0xF9, 0x6C, 0x4E, 0x91, 0x5E, 0x25, 0x6C, 0xB0, 0xFB, 0x89, 0x6A, 0xB0, 0xB1, 0x9B, 0x6D, 0x14,
	},

	/* Service A data */
	.srv_a_chr1 = 0x01,
	.srv_a_chr2 = {
		0x31, 0x31, 0x31, 0x31, 0x31, 0x32, 0x32, 0x32, 0x32, 0x32, 0x33, 0x33, 0x33, 0x33, 0x33, 0x34,
		0x34, 0x34, 0x34, 0x34, 0x35, 0x35, 0x35, 0x35, 0x35, 0x36, 0x36, 0x36, 0x36, 0x36, 0x37, 0x37,
		0x37, 0x37, 0x37, 0x38, 0x38, 0x38, 0x38, 0x38, 0x39, 0x39, 0x39, 0x39, 0x39, 0x30, 0x30, 0x30,
		0x30, 0x30, 0x31, 0x31, 0x31, 0x31, 0x31, 0x32, 0x32, 0x32, 0x32, 0x32, 0x33, 0x33, 0x33, 0x33,
		0x33, 0x34, 0x34, 0x34, 0x34, 0x34, 0x35, 0x35, 0x35, 0x35, 0x35, 0x36, 0x36, 0x36, 0x36, 0x36,
		0x37, 0x37, 0x37, 0x37, 0x37, 0x38, 0x38, 0x38, 0x38, 0x38, 0x39, 0x39, 0x39, 0x39, 0x39, 0x30,
		0x30, 0x30, 0x30, 0x30, 0x31, 0x31, 0x31, 0x31, 0x31, 0x32, 0x32, 0x32, 0x32, 0x32, 0x33, 0x33,
		0x33, 0x33, 0x33, 0x34, 0x34, 0x34, 0x34, 0x34, 0x35, 0x35, 0x35, 0x35, 0x35, 0x36, 0x36, 0x36,
		0x36, 0x36, 0x37, 0x37, 0x37, 0x37, 0x37, 0x38, 0x38, 0x38, 0x38, 0x38, 0x39, 0x39, 0x39, 0x39,
		0x39, 0x30, 0x30, 0x30, 0x30, 0x30, 0x31, 0x31, 0x31, 0x31, 0x31, 0x32, 0x32, 0x32, 0x32, 0x32,
		0x33, 0x33, 0x33, 0x33, 0x33, 0x34, 0x34, 0x34, 0x34, 0x34, 0x35, 0x35, 0x35, 0x35, 0x35, 0x36,
		0x36, 0x36, 0x36, 0x36, 0x37, 0x37, 0x37, 0x37, 0x37, 0x38, 0x38, 0x38, 0x38, 0x38, 0x39, 0x39,
		0x39, 0x39, 0x39, 0x30, 0x30, 0x30, 0x30, 0x30, 0x31, 0x31, 0x31, 0x31, 0x31, 0x32, 0x32, 0x32,
		0x32, 0x32, 0x33, 0x33, 0x33, 0x33, 0x33, 0x34, 0x34, 0x34, 0x34, 0x34, 0x35, 0x35, 0x35, 0x35,
		0x35, 0x36, 0x36, 0x36, 0x36, 0x36, 0x37, 0x37, 0x37, 0x37, 0x37, 0x38, 0x38, 0x38, 0x38, 0x38,
		0x39, 0x39, 0x39, 0x39, 0x39, 0x30, 0x30, 0x30, 0x30, 0x30, 0x31, 0x31, 0x31, 0x31, 0x31, 0x32,
		0x32, 0x32, 0x32, 0x32, 0x33, 0x33, 0x33, 0x33, 0x33, 0x34, 0x34, 0x34, 0x34, 0x34, 0x35, 0x35,
		0x35, 0x35, 0x35, 0x36, 0x36, 0x36, 0x36, 0x36, 0x37, 0x37, 0x37, 0x37, 0x37, 0x38, 0x38, 0x38,
		0x38, 0x38, 0x39, 0x39, 0x39, 0x39, 0x39, 0x30, 0x30, 0x30, 0x30, 0x30, 0x31, 0x31, 0x31, 0x31,
		0x31, 0x32, 0x32, 0x32, 0x32, 0x32, 0x33, 0x33, 0x33, 0x33, 0x33, 0x34, 0x34, 0x34, 0x34, 0x34,
		0x35, 0x35, 0x35, 0x35, 0x35, 0x36, 0x36, 0x36, 0x36, 0x36, 0x37, 0x37, 0x37, 0x37, 0x37, 0x38,
		0x38, 0x38, 0x38, 0x38, 0x39, 0x39, 0x39, 0x39, 0x39, 0x30, 0x30, 0x30, 0x30, 0x30, 0x31, 0x31,
		0x31, 0x31, 0x31, 0x32, 0x32, 0x32, 0x32, 0x32, 0x33, 0x33, 0x33, 0x33, 0x33, 0x34, 0x34, 0x34,
		0x34, 0x34, 0x35, 0x35, 0x35, 0x35, 0x35, 0x36, 0x36, 0x36, 0x36, 0x36, 0x37, 0x37, 0x37, 0x37,
		0x37, 0x38, 0x38, 0x38, 0x38, 0x38, 0x39, 0x39, 0x39, 0x39, 0x39, 0x30, 0x30, 0x30, 0x30, 0x30,
		0x31, 0x31, 0x31, 0x31, 0x31, 0x32, 0x32, 0x32, 0x32, 0x32, 0x33, 0x33, 0x33, 0x33, 0x33, 0x34,
		0x34, 0x34, 0x34, 0x34, 0x35, 0x35, 0x35, 0x35, 0x35, 0x36, 0x36, 0x36, 0x36, 0x36, 0x37, 0x37,
		0x37, 0x37, 0x37, 0x38, 0x38, 0x38, 0x38, 0x38, 0x39, 0x39, 0x39, 0x39, 0x39, 0x30, 0x30, 0x30,
		0x30, 0x30, 0x31, 0x31, 0x31, 0x31, 0x31, 0x32, 0x32, 0x32, 0x32, 0x32, 0x33, 0x33, 0x33, 0x33,
		0x33, 0x34, 0x34, 0x34, 0x34, 0x34, 0x35, 0x35, 0x35, 0x35, 0x35, 0x36, 0x36, 0x36, 0x36, 0x36,
		0x37, 0x37, 0x37, 0x37, 0x37, 0x38, 0x38, 0x38, 0x38, 0x38, 0x39, 0x39, 0x39, 0x39, 0x39, 0x30,
		0x30, 0x30, 0x30, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31, 0x32,
	},
	.srv_a_chr3 = {
		0x31, 0x31, 0x31, 0x31, 0x31, 0x32, 0x32, 0x32, 0x32, 0x32, 0x33, 0x33, 0x33, 0x33, 0x33, 0x34,
		0x34, 0x34, 0x34, 0x34, 0x35, 0x35, 0x35, 0x35, 0x35, 0x36, 0x36, 0x36, 0x36, 0x36, 0x37, 0x37,
		0x37, 0x37, 0x37, 0x38, 0x38, 0x38, 0x38, 0x38, 0x39, 0x39, 0x39, 0x39, 0x39, 0x30, 0x30, 0x30,
		0x30, 0x30,
	},

	/* Service B.4 data */
	.srv_b4_chr1 = 0x07,

	/* Service Generic Access Profile data */
	.srv_gap_chr1 = {
		0x54, 0x65, 0x73, 0x74, 0x20, 0x44, 0x61, 0x74, 0x61, 0x62, 0x61, 0x73, 0x65,
	},
	.dev_name_len = 13,
	.srv_gap_chr2 = 17,
	.srv_gap_chr3 = {
		100, 200, 0, 2000,
	},
	.srv_gap_chr4 = 1,

	/* Service B.3 data */
	.srv_b3_chr1 = 0x06,

	/* Service B.1 data */
	.srv_b1_chr1 = 0x04,
	.srv_b1_chr2 = 0x04,
	.srv_b1_chr2_dsc1 = 0,
	.srv_b1_chr3 = {
		0x31, 0x31, 0x31, 0x31, 0x31, 0x32, 0x32, 0x32, 0x32, 0x32, 0x33, 0x33, 0x33, 0x33, 0x33, 0x34,
		0x34, 0x34, 0x34, 0x34, 0x35, 0x35, 0x35, 0x35, 0x35, 0x36, 0x36, 0x36, 0x36, 0x36, 0x37, 0x37,
		0x37, 0x37, 0x37, 0x38, 0x38, 0x38, 0x38, 0x38, 0x39, 0x39, 0x39, 0x39, 0x39, 0x30, 0x30, 0x30,
		0x30,
	},
	.srv_b1_chr3_dsc1 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x11, 0x22,
		0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
		0x99,
	},
	.srv_b1_chr4 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x11, 0x22,
		0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33,
	},
	.srv_b1_chr4_dsc1 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x11, 0x22,
		0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33,
	},

	/* Service B.2 data */
	.srv_b2_chr1 = 0x05,
	.srv_b2_chr1_dsc1 = 0x0002,
	.srv_b2_chr1_dsc2 = {
		0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
		0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a,
	},
	.srv_b2_chr1_dsc3 = {
		0x04, 0x00, 0x01, 0x30, 0x01, 0x11, 0x31,
	},
	.srv_b2_chr1_dsc4 = 0x44,

	/* Service B.5 data */
	.srv_b5_chr1 = 0x08,
	.srv_b5_chr1_dsc1 = 0x01,
	.srv_b5_chr1_dsc2 = 0x02,
	.srv_b5_chr1_dsc3 = 0x03,

	/* Service C.1 data */
	.srv_c1_chr1 = 0x09,
	.srv_c1_chr1_dsc1 = 0x0001,
	.srv_c1_chr1_dsc2 = 0x22,
	.srv_c1_chr1_dsc3 = 0x33,

	/* Service F data */
	.srv_f_chr1 = {
		0x4c, 0x65, 0x6e, 0x67, 0x74, 0x68, 0x20, 0x69, 0x73, 0x20,
	},
	.srv_f_chr1_dsc1 = {
		0x19, 0x00, 0x00, 0x30, 0x01, 0x00, 0x00,
	},
	.srv_f_chr2 = 0x65,
	.srv_f_chr2_dsc1 = {
		0x04, 0x00, 0x01, 0x27, 0x01, 0x01, 0x00,
	},
	.srv_f_chr3 = 0x1234,
	.srv_f_chr3_dsc1 = {
		0x06, 0x00, 0x10, 0x27, 0x01, 0x02, 0x00,
	},
	.srv_f_chr4 = 0x01020304,
	.srv_f_chr4_dsc1 = {
		0x08, 0x00, 0x17, 0x27, 0x01, 0x03, 0x00,
	},
	.srv_f_chr5 = {
		0x65, 0x34, 0x12, 0x04, 0x03, 0x02, 0x01,
	},
	.srv_f_chr5_dsc1 = {
		0x00a6, 0x00a9, 0x00ac,
	},
	.srv_f_chr6 = 0x12,
	.srv_f_chr7 = {
		0x04, 0x03, 0x02, 0x01, 0x34, 0x12,
	},
	.srv_f_chr7_dsc1 = {
		0x00ac, 0x00a9,
	},
	.srv_f_chr8 = {
		0x65, 0x65,
	},
	.srv_f_chr8_dsc1 = {
		0x00a6, 0x00a6,
	},
	.srv_f_chr9 = {
		0x65, 0x04, 0x03, 0x02, 0x01,
	},
	.srv_f_chr9_dsc1 = {
		0x00a6, 0x00ac,
	},
	.srv_f_chr10 = {
		0x65, 0x34, 0x12,
	},
	.srv_f_chr10_dsc1 = {
		0x00a6, 0x00a9,
	},

	/* Service C.2 data */
	.srv_c2_chr1 = 0x0a,
	.srv_c2_chr2 = {
		0x31, 0x31, 0x31, 0x31, 0x31, 0x32, 0x32, 0x32, 0x32, 0x32, 0x33, 0x33, 0x33, 0x33, 0x33, 0x34,
		0x34, 0x34, 0x34, 0x34, 0x35,
	},
	.srv_c2_chr2_dsc1 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x11,
	},
	.srv_c2_chr3 = {
		0x32, 0x32, 0x32, 0x32, 0x32, 0x33, 0x33, 0x33, 0x33, 0x33, 0x34, 0x34, 0x34, 0x34, 0x34, 0x35,
		0x35, 0x35, 0x35, 0x35, 0x36, 0x36,
	},
	.srv_c2_chr3_dsc1 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x11, 0x22,
	},
	.srv_c2_chr4 = {
		0x33, 0x33, 0x33, 0x33, 0x33, 0x34, 0x34, 0x34, 0x34, 0x34, 0x35, 0x35, 0x35, 0x35, 0x35, 0x36,
		0x36, 0x36, 0x36, 0x36, 0x37, 0x37, 0x37,
	},
	.srv_c2_chr4_dsc1 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x11, 0x22, 0x33,
	},
	.srv_c2_chr5 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x11, 0x22,
		0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33,
	},
	.srv_c2_chr5_dsc1 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x11, 0x22,
		0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33,
	},
	.srv_c2_chr6 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x11, 0x22,
		0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44,
	},
	.srv_c2_chr6_dsc1 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x11, 0x22,
		0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44,
	},
	.srv_c2_chr7 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x11, 0x22,
		0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
	},
	.srv_c2_chr7_dsc1 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x11, 0x22,
		0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
	},
	.srv_c2_chr8 = {
		0x31, 0x31, 0x31, 0x31, 0x31, 0x32, 0x32, 0x32, 0x32, 0x32, 0x33, 0x33, 0x33, 0x33, 0x33, 0x34,
		0x34, 0x34, 0x34, 0x34, 0x35, 0x35, 0x35, 0x35, 0x35, 0x36, 0x36, 0x36, 0x36, 0x36, 0x37, 0x37,
		0x37, 0x37, 0x37, 0x38, 0x38, 0x38, 0x38, 0x38, 0x39, 0x39, 0x39,
	},
	.srv_c2_chr8_dsc1 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x11, 0x22,
		0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33,
	},
	.srv_c2_chr9 = {
		0x32, 0x32, 0x32, 0x32, 0x32, 0x33, 0x33, 0x33, 0x33, 0x33, 0x34, 0x34, 0x34, 0x34, 0x34, 0x35,
		0x35, 0x35, 0x35, 0x35, 0x36, 0x36, 0x36, 0x36, 0x36, 0x37, 0x37, 0x37, 0x37, 0x37, 0x38, 0x38,
		0x38, 0x38, 0x38, 0x39, 0x39, 0x39, 0x39, 0x39, 0x30, 0x30, 0x30, 0x30,
	},
	.srv_c2_chr9_dsc1 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x11, 0x22,
		0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44,
	},
	.srv_c2_chr10 = {
		0x33, 0x33, 0x33, 0x33, 0x33, 0x34, 0x34, 0x34, 0x34, 0x34, 0x35, 0x35, 0x35, 0x35, 0x35, 0x36,
		0x36, 0x36, 0x36, 0x36, 0x37, 0x37, 0x37, 0x37, 0x37, 0x38, 0x38, 0x38, 0x38, 0x38, 0x39, 0x39,
		0x39, 0x39, 0x39, 0x30, 0x30, 0x30, 0x30, 0x30, 0x31, 0x31, 0x31, 0x31, 0x31,
	},
	.srv_c2_chr10_dsc1 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x11, 0x22,
		0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
	},
	.srv_c2_chr11 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x11, 0x22,
		0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	},
	.srv_c2_chr11_dsc1 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x11, 0x22,
		0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
	},
	.srv_c2_chr12 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x11, 0x22,
		0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77
	},
	.srv_c2_chr12_dsc1 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x11, 0x22,
		0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77
	},
	.srv_c2_chr13 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x11, 0x22,
		0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
	},
	.srv_c2_chr13_dsc1 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x11, 0x22,
		0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
	},
	.srv_c2_chr14 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x11, 0x22,
		0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
		0x99, 0x01, 0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78, 0x89, 0x90, 0x01, 0x23, 0x45, 0x67, 0x89,
		0x01, 0x23, 0x45, 0x67, 0x89, 0x01, 0x23, 0x45, 0x67, 0x89, 0x01, 0x23, 0x45, 0x67, 0x89, 0x01,
		0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78, 0x89, 0x90, 0x01, 0x12, 0x23, 0x34, 0x45, 0x56,
	},
	.srv_c2_chr14_dsc1 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x11, 0x22,
		0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
		0x99, 0x01, 0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78, 0x89, 0x90, 0x01, 0x23, 0x45, 0x67, 0x89,
		0x01, 0x23, 0x45, 0x67, 0x89, 0x01, 0x23, 0x45, 0x67, 0x89, 0x01, 0x23, 0x45, 0x67, 0x89, 0x01,
		0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78, 0x89, 0x90, 0x01, 0x12, 0x23, 0x34, 0x45, 0x56,
	},
	.srv_c2_chr15 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x11, 0x22,
		0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
		0x99,
	},
	.srv_c2_chr15_dsc1 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x11, 0x22,
		0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
		0x99,
	},
	.srv_c2_chr16 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x11, 0x22,
		0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
		0x99, 0x00,
	},
	.srv_c2_chr16_dsc1 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x11, 0x22,
		0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
		0x99, 0x00,
	},
	.srv_c2_chr17 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x11, 0x22,
		0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
		0x99, 0x00, 0x11,
	},
	.srv_c2_chr17_dsc1 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x11, 0x22,
		0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
		0x99, 0x00, 0x11,
	},
	.srv_c2_chr18 = {
		0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
		0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
		0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
		0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
	},
	.srv_c2_chr18_dsc1 = {
		0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
		0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
		0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
		0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
	},
	.srv_c2_chr19 = {
		0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
		0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11,
		0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
		0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33,
	},
	.srv_c2_chr19_dsc1 = {
		0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
		0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11,
		0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
		0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33,
	},
	.srv_c2_chr20 = {
		0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
		0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11,
		0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
		0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33,
		0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
		0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
		0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11,
		0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
	},
	.srv_c2_chr20_dsc1 = {
		0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
		0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11,
		0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
		0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33,
		0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
		0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
		0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11,
		0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
	},
	.srv_c2_chr21 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x11, 0x22,
		0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
		0x99, 0x01, 0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78, 0x89, 0x90, 0x01, 0x23, 0x45, 0x67, 0x89,
		0x01, 0x23, 0x45, 0x67, 0x89, 0x01, 0x23, 0x45, 0x67, 0x89, 0x01, 0x23, 0x45, 0x67, 0x89, 0x01,
		0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78, 0x89, 0x90, 0x01, 0x12, 0x23, 0x34, 0x45,
	},
	.srv_c2_chr21_dsc1 = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12,
		0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 0x11, 0x22,
		0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
		0x99, 0x01, 0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78, 0x89, 0x90, 0x01, 0x23, 0x45, 0x67, 0x89,
		0x01, 0x23, 0x45, 0x67, 0x89, 0x01, 0x23, 0x45, 0x67, 0x89, 0x01, 0x23, 0x45, 0x67, 0x89, 0x01,
		0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78, 0x89, 0x90, 0x01, 0x12, 0x23, 0x34, 0x45,
	},
	.srv_c2_chr22 = {
		0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
		0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
		0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
		0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
		0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
		0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
		0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
		0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
	},
	.srv_c2_chr22_dsc1 = {
		0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
		0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
		0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
		0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
		0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
		0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
		0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
		0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
	},
};

static int
gatt_svr_access_test(uint16_t conn_handle, uint16_t attr_handle,
		struct ble_gatt_access_ctxt *ctxt,
		void *arg);

static const struct ble_gatt_svc_def gatt_svr_svc_b5[] = {
	{
		/* Service B.5 */
		.type = BLE_GATT_SVC_TYPE_PRIMARY,
		.uuid = BLE_UUID16_DECLARE(0xA00B),
		.start_handle = 0x0080,
        .characteristics = (struct ble_gatt_chr_def[]) { {
			.uuid = BLE_UUID16_DECLARE(0xB008),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
				BLE_GATT_CHR_F_WRITE_ENC,
			.arg = (void *)IDX_B5_CHR1,
			.min_key_size = 16,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0xB015),
				.att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE |
					BLE_ATT_F_READ_AUTHEN | BLE_ATT_F_WRITE_AUTHEN,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_B5_CHR1_DSC1,
				.min_key_size = 16,
			}, {
				.uuid = BLE_UUID16_DECLARE(0xB016),
				.att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE |
					BLE_ATT_F_READ_AUTHOR | BLE_ATT_F_WRITE_AUTHOR,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_B5_CHR1_DSC2,
			}, {
				.uuid = BLE_UUID16_DECLARE(0xB017),
				.att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE |
					BLE_ATT_F_READ_ENC | BLE_ATT_F_WRITE_ENC,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_B5_CHR1_DSC3,
				.min_key_size = 16,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			0, /* No more characteristics in this service. */
		} },
	},

	{
		0, /* No more services. */
	},
};

static const struct ble_gatt_svc_def *inc_svcs_b5[] = {
	&gatt_svr_svc_b5[0],
	NULL,
};

static const struct ble_gatt_svc_def gatt_svr_svc_d[] = {
	{
		/* Service D */
		.type = BLE_GATT_SVC_TYPE_SECONDARY,
		.uuid = BLE_UUID16_DECLARE(0xA00D),
		.includes = inc_svcs_b5,
		.start_handle = 0x0001,
        .characteristics = (struct ble_gatt_chr_def[]) { {
			.uuid = BLE_UUID16_DECLARE(0xB00C),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_AUTHEN,
			.arg = (void *)IDX_D_CHR1,
			.min_key_size = 16,
		}, {
			.uuid = PTS_UUID_128(0xB00B),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_AUTHOR,
			.arg = (void *)IDX_D_CHR2,
		}, {
			0, /* No more characteristics in this service. */
		} },
	},

	{
		0, /* No more services. */
	},
};

static const struct ble_gatt_svc_def gatt_svr_svc_att[] = {
	{
		/* Service Attribute Profile */
		.type = BLE_GATT_SVC_TYPE_PRIMARY,
		.uuid = BLE_UUID16_DECLARE(0x1801),
		.start_handle = 0x0010,
        .characteristics = (struct ble_gatt_chr_def[]) { {
			.uuid = BLE_UUID16_DECLARE(0x2A05),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_INDICATE,
			.arg = (void *)IDX_ATT_CHR1,
		}, {
			.uuid = BLE_UUID16_DECLARE(0x2B29),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
			.arg = (void *)IDX_ATT_CHR2,
		}, {
			.uuid = BLE_UUID16_DECLARE(0x2B3A),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ,
			.arg = (void *)IDX_ATT_CHR3,
		}, {
			.uuid = BLE_UUID16_DECLARE(0x2B2A),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ,
			.arg = (void *)IDX_ATT_CHR4,
		}, {
			0, /* No more characteristics in this service. */
		} },
	},

	{
		0, /* No more services. */
	},
};

static const struct ble_gatt_svc_def *inc_svcs_d[] = {
	&gatt_svr_svc_d[0],
	NULL,
};

static const struct ble_gatt_svc_def gatt_svr_svc_a[] = {
	{
		/* Service A */
		.type = BLE_GATT_SVC_TYPE_PRIMARY,
		.uuid = BLE_UUID16_DECLARE(0xA00A),
		.includes = inc_svcs_d,
		.start_handle = 0x0020,
        .characteristics = (struct ble_gatt_chr_def[]) { {
			.uuid = BLE_UUID16_DECLARE(0xB001),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC,
			.arg = (void *)IDX_A_CHR1,
			.min_key_size = 16,
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB002),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
			.arg = (void *)IDX_A_CHR2,
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB002),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_WRITE,
			.arg = (void *)IDX_A_CHR3,
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB003),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_WRITE,
			.arg = (void *)IDX_A_CHR4,
		}, {
			0, /* No more characteristics in this service. */
		} },
	},

	{
		0, /* No more services. */
	},
};


static const struct ble_gatt_svc_def gatt_svr_svc_b4[] = {
	{
		/* Service B.4 */
		.type = BLE_GATT_SVC_TYPE_PRIMARY,
		.uuid = BLE_UUID16_DECLARE(0xA00B),
		.start_handle = 0x0030,
        .characteristics = (struct ble_gatt_chr_def[]) { {
			.uuid = BLE_UUID16_DECLARE(0xB007),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_WRITE,
			.arg = (void *)IDX_B4_CHR1,
		}, {
			0, /* No more characteristics in this service. */
		} },
	},

	{
		0, /* No more services. */
	},
};

static const struct ble_gatt_svc_def gatt_svr_svc_gap[] = {
	{
		/* Service Generic Access Profile */
		.type = BLE_GATT_SVC_TYPE_PRIMARY,
		.uuid = BLE_UUID16_DECLARE(0x1800),
		.start_handle = 0x0040,
        .characteristics = (struct ble_gatt_chr_def[]) { {
			.uuid = BLE_UUID16_DECLARE(0x2A00),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
			.arg = (void *)IDX_GAP_CHR1,
		}, {
			.uuid = BLE_UUID16_DECLARE(0x2A01),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
			.arg = (void *)IDX_GAP_CHR2,
		}, {
			.uuid = BLE_UUID16_DECLARE(0x2A04),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ,
			.arg = (void *)IDX_GAP_CHR3,
		}, {
#if 0 /* central address resolution */
			.uuid = BLE_UUID16_DECLARE(0x2AA6),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ,
			.arg = (void *)IDX_GAP_CHR4,
		}, {
#endif
			0, /* No more characteristics in this service. */
		} },
	},

	{
		0, /* No more services. */
	},
};

static const struct ble_gatt_svc_def gatt_svr_svc_b3[] = {
	{
		/* Service B.3 */
		.type = BLE_GATT_SVC_TYPE_PRIMARY,
		.uuid = BLE_UUID16_DECLARE(0xA00B),
		.start_handle = 0x0050,
        .characteristics = (struct ble_gatt_chr_def[]) { {
			.uuid = BLE_UUID16_DECLARE(0xB006),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
				BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_NOTIFY |
				BLE_GATT_CHR_F_INDICATE,
			.arg = (void *)IDX_B3_CHR1,
		}, {
			0, /* No more characteristics in this service. */
		} },
	},

	{
		0, /* No more services. */
	},
};

static const struct ble_gatt_svc_def gatt_svr_svc_b1[] = {
	{
		/* Service B.1 */
		.type = BLE_GATT_SVC_TYPE_PRIMARY,
		.uuid = BLE_UUID16_DECLARE(0xA00B),
		.start_handle = 0x0060,
        .characteristics = (struct ble_gatt_chr_def[]) { {
			.uuid = BLE_UUID16_DECLARE(0xB004),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
				BLE_GATT_CHR_F_WRITE_AUTHEN,
			.arg = (void *)IDX_B1_CHR1,
			.min_key_size = 16,
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB004),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
			.arg = (void *)IDX_B1_CHR2,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0x2903),
				.att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_B1_CHR2_DSC1,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB004),
            .access_cb = gatt_svr_access_test,
			.flags = 0,
			.arg = (void *)IDX_B1_CHR3,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0xB012),
				.access_cb = gatt_svr_access_test,
				.att_flags = 0,
				.arg = (void *)IDX_B1_CHR3_DSC1,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB004),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ,
			.arg = (void *)IDX_B1_CHR4,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0xB012),
				.access_cb = gatt_svr_access_test,
				.att_flags = BLE_ATT_F_READ,
				.arg = (void *)IDX_B1_CHR4_DSC1,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			0, /* No more characteristics in this service. */
		} },
	},

	{
		0, /* No more services. */
	},
};

static const struct ble_gatt_svc_def gatt_svr_svc_b2[] = {
	{
		/* Service B.2 */
		.type = BLE_GATT_SVC_TYPE_PRIMARY,
		.uuid = BLE_UUID16_DECLARE(0xA00B),
		.start_handle = 0x0070,
        .characteristics = (struct ble_gatt_chr_def[]) { {
			.uuid = BLE_UUID16_DECLARE(0xB005),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
				BLE_GATT_CHR_F_RELIABLE_WRITE | BLE_GATT_CHR_F_AUX_WRITE |
				BLE_GATT_CHR_F_WRITE_AUTHOR,
			.arg = (void *)IDX_B2_CHR1,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0x2900),
				.att_flags = BLE_ATT_F_READ,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_B2_CHR1_DSC1,
			}, {
				.uuid = BLE_UUID16_DECLARE(0x2901),
				.att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_B2_CHR1_DSC2,
			}, {
				.uuid = BLE_UUID16_DECLARE(0x2904),
				.att_flags = BLE_ATT_F_READ,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_B2_CHR1_DSC3,
			}, {
				.uuid = PTS_UUID_128(0xD5D4),
				.att_flags = BLE_ATT_F_READ,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_B2_CHR1_DSC4,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			0, /* No more characteristics in this service. */
		} },
	},

	{
		0, /* No more services. */
	},
};

static const struct ble_gatt_svc_def gatt_svr_svc_c1[] = {
	{
		/* Service C.1*/
		.type = BLE_GATT_SVC_TYPE_PRIMARY,
		.uuid = PTS_UUID_128(0xA00C),
		.includes = inc_svcs_d,
		.start_handle = 0x0090,
        .characteristics = (struct ble_gatt_chr_def[]) { {
			.uuid = PTS_UUID_128(0xB009),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
				BLE_GATT_CHR_F_RELIABLE_WRITE,
			.arg = (void *)IDX_C1_CHR1,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0x2900),
				.att_flags = BLE_ATT_F_READ,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_C1_CHR1_DSC1,
			}, {
				.uuid = PTS_UUID_128(0xD9D2),
				.att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_C1_CHR1_DSC2,
			}, {
				.uuid = PTS_UUID_128(0xD9D3),
				.att_flags = BLE_ATT_F_WRITE,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_C1_CHR1_DSC3,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			0, /* No more characteristics in this service. */
		} },
	},

	{
		0, /* No more services. */
	},
};

static const struct ble_gatt_svc_def gatt_svr_svc_f[] = {
	{
		/* Service F */
		.type = BLE_GATT_SVC_TYPE_PRIMARY,
		.uuid = BLE_UUID16_DECLARE(0xA00F),
		.start_handle = 0x00a0,
        .characteristics = (struct ble_gatt_chr_def[]) { {
			.uuid = BLE_UUID16_DECLARE(0xB00E),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ,
			.arg = (void *)IDX_F_CHR1,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0x2904),
				.att_flags = BLE_ATT_F_READ,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_F_CHR1_DSC1,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB00F),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
			.arg = (void *)IDX_F_CHR2,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0x2904),
				.att_flags = BLE_ATT_F_READ,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_F_CHR2_DSC1,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB006),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
			.arg = (void *)IDX_F_CHR3,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0x2904),
				.att_flags = BLE_ATT_F_READ,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_F_CHR3_DSC1,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB007),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
			.arg = (void *)IDX_F_CHR4,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0x2904),
				.att_flags = BLE_ATT_F_READ,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_F_CHR4_DSC1,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB010),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ,
			.arg = (void *)IDX_F_CHR5,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0x2905),
				.att_flags = BLE_ATT_F_READ,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_F_CHR5_DSC1,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB011),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
				BLE_GATT_CHR_F_AUTH_SIGN_WRITE,
			.arg = (void *)IDX_F_CHR6,
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB010),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ,
			.arg = (void *)IDX_F_CHR7,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0x2905),
				.att_flags = BLE_ATT_F_READ,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_F_CHR7_DSC1,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB010),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ,
			.arg = (void *)IDX_F_CHR8,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0x2905),
				.att_flags = BLE_ATT_F_READ,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_F_CHR8_DSC1,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB010),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ,
			.arg = (void *)IDX_F_CHR9,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0x2905),
				.att_flags = BLE_ATT_F_READ,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_F_CHR9_DSC1,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB010),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ,
			.arg = (void *)IDX_F_CHR10,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0x2905),
				.att_flags = BLE_ATT_F_READ,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_F_CHR10_DSC1,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			0, /* No more characteristics in this service. */
		} },
	},

	{
		0, /* No more services. */
	},
};

static const struct ble_gatt_svc_def gatt_svr_svc_c2[] = {
	{
		/* Service C.2 */
		.type = BLE_GATT_SVC_TYPE_PRIMARY,
		.uuid = PTS_UUID_128(0xA00C),
		.start_handle = 0x00c0,
        .characteristics = (struct ble_gatt_chr_def[]) { {
			.uuid = BLE_UUID16_DECLARE(0xB00A),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ,
			.arg = (void *)IDX_C2_CHAR1,
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB002),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
			.arg = (void *)IDX_C2_CHAR2,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0xB012),
				.att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_C2_CHAR2_DSC1,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB002),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
			.arg = (void *)IDX_C2_CHAR3,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0xB013),
				.att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_C2_CHAR3_DSC1,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB002),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
			.arg = (void *)IDX_C2_CHAR4,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0xB014),
				.att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_C2_CHAR4_DSC1,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB002),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
			.arg = (void *)IDX_C2_CHAR5,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0xB012),
				.att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_C2_CHAR5_DSC1,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB002),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
			.arg = (void *)IDX_C2_CHAR6,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0xB013),
				.att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_C2_CHAR6_DSC1,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB002),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
			.arg = (void *)IDX_C2_CHAR7,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0xB014),
				.att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_C2_CHAR7_DSC1,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB002),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
				BLE_GATT_CHR_F_READ_AUTHEN | BLE_GATT_CHR_F_WRITE_AUTHEN,
			.arg = (void *)IDX_C2_CHAR8,
			.min_key_size = 16,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0xB012),
				.att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE |
					BLE_ATT_F_READ_AUTHEN | BLE_ATT_F_WRITE_AUTHEN,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_C2_CHAR8_DSC1,
				.min_key_size = 16,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB002),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
				BLE_GATT_CHR_F_READ_AUTHOR | BLE_GATT_CHR_F_WRITE_AUTHOR,
			.arg = (void *)IDX_C2_CHAR9,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0xB013),
				.att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE |
					BLE_ATT_F_READ_AUTHOR | BLE_ATT_F_WRITE_AUTHOR,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_C2_CHAR9_DSC1,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB002),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
				BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_WRITE_ENC,
			.arg = (void *)IDX_C2_CHAR10,
			.min_key_size = 16,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0xB014),
				.att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE |
					BLE_ATT_F_READ_ENC | BLE_ATT_F_WRITE_ENC,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_C2_CHAR10_DSC1,
				.min_key_size = 16,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB004),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
			.arg = (void *)IDX_C2_CHAR11,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0xB016),
				.att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_C2_CHAR11_DSC1,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB004),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
			.arg = (void *)IDX_C2_CHAR12,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0xB016),
				.att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_C2_CHAR12_DSC1,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB004),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
			.arg = (void *)IDX_C2_CHAR13,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0xB017),
				.att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_C2_CHAR13_DSC1,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB001),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
			.arg = (void *)IDX_C2_CHAR14,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0xB012),
				.att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_C2_CHAR14_DSC1,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB002),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
				BLE_GATT_CHR_F_READ_AUTHEN | BLE_GATT_CHR_F_WRITE_AUTHEN,
			.arg = (void *)IDX_C2_CHAR15,
			.min_key_size = 16,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0xB012),
				.att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE |
					BLE_ATT_F_READ_AUTHEN | BLE_ATT_F_WRITE_AUTHEN,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_C2_CHAR15_DSC1,
				.min_key_size = 16,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB002),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
				BLE_GATT_CHR_F_READ_AUTHOR | BLE_GATT_CHR_F_WRITE_AUTHOR,
			.arg = (void *)IDX_C2_CHAR16,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0xB013),
				.att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE |
					BLE_ATT_F_READ_AUTHOR | BLE_ATT_F_WRITE_AUTHOR,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_C2_CHAR16_DSC1,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB002),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
				BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_WRITE_ENC,
			.arg = (void *)IDX_C2_CHAR17,
			.min_key_size = 16,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0xB014),
				.att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE |
					BLE_ATT_F_READ_ENC | BLE_ATT_F_WRITE_ENC,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_C2_CHAR17_DSC1,
				.min_key_size = 16,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB018),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
			.arg = (void *)IDX_C2_CHAR18,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0xB01A),
				.att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_C2_CHAR18_DSC1,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB019),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
			.arg = (void *)IDX_C2_CHAR19,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0xB01B),
				.att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_C2_CHAR19_DSC1,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB020),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
			.arg = (void *)IDX_C2_CHAR20,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0xB01C),
				.att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_C2_CHAR20_DSC1,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB001),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
			.arg = (void *)IDX_C2_CHAR21,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0xB012),
				.att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_C2_CHAR21_DSC1,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			.uuid = BLE_UUID16_DECLARE(0xB018),
            .access_cb = gatt_svr_access_test,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
			.arg = (void *)IDX_C2_CHAR22,
			.descriptors = (struct ble_gatt_dsc_def[]){ {
				.uuid = BLE_UUID16_DECLARE(0xB01A),
				.att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
				.access_cb = gatt_svr_access_test,
				.arg = (void *)IDX_C2_CHAR22_DSC1,
			}, {
				0, /* No more descriptors in this characteristic. */
			} }
		}, {
			0, /* No more characteristics in this service. */
		} },
	},

	{
		0, /* No more services. */
	},
};

static int
gatt_svr_chr_write(struct os_mbuf *om, uint16_t min_len, uint16_t max_len,
		void *dst, uint16_t *len)
{
	uint16_t om_len;
	int rc;

	om_len = OS_MBUF_PKTLEN(om);
	if (om_len < min_len || om_len > max_len) {
		return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
	}

	rc = ble_hs_mbuf_to_flat(om, dst, max_len, len);
	if (rc != 0) {
		return BLE_ATT_ERR_UNLIKELY;
	}

	return 0;
}

static int
gatt_svr_access_read(uint16_t conn_handle, int attr_idx, uint16_t attr_handle,
		struct ble_gatt_access_ctxt *ctxt)
{
	int rc;
	void *data;
	uint16_t len;

	if (ctxt->om == NULL) {
		printf("read authorization check\n");
		if (!large_db1.authorization) {
			return BLE_ATT_ERR_INSUFFICIENT_AUTHOR;
		}
		return 0;
	}

	printf("[PTS] chr read: idx=%d handle=0x%04x\n", attr_idx, attr_handle);

	switch (attr_idx) {
	case IDX_D_CHR1:
		data = &large_db1.srv_d_chr1;
		len = sizeof (large_db1.srv_d_chr1);
		break;
	case IDX_D_CHR2:
		if (!large_db1.authorization) {
			return BLE_ATT_ERR_INSUFFICIENT_AUTHOR;
		}

		data = &large_db1.srv_d_chr2;
		len = sizeof (large_db1.srv_d_chr2);
		break;
	case IDX_ATT_CHR1:
		data = &large_db1.srv_att_chr1;
		len = sizeof (large_db1.srv_att_chr1);
		break;
	case IDX_ATT_CHR2:
		large_db1.srv_att_chr2 &= ~0xF8;
		data = &large_db1.srv_att_chr2;
		len = sizeof (large_db1.srv_att_chr2);
		break;
	case IDX_ATT_CHR3:
		data = &large_db1.srv_att_chr3;
		len = sizeof (large_db1.srv_att_chr3);
		break;
	case IDX_ATT_CHR4:
		data = large_db1.srv_att_chr4;
		len = sizeof (large_db1.srv_att_chr4);
		break;
	case IDX_A_CHR1:
		data = &large_db1.srv_a_chr1;
		len = sizeof (large_db1.srv_a_chr1);
		break;
	case IDX_A_CHR2:
		data = large_db1.srv_a_chr2;
		len = sizeof (large_db1.srv_a_chr2);
		break;
	case IDX_GAP_CHR1:
		data = large_db1.srv_gap_chr1;
		len = large_db1.dev_name_len;
		break;
	case IDX_GAP_CHR2:
		data = &large_db1.srv_gap_chr2;
		len = sizeof (large_db1.srv_gap_chr2);
		break;
	case IDX_GAP_CHR3:
		data = large_db1.srv_gap_chr3;
		len = sizeof (large_db1.srv_gap_chr3);
		break;
	case IDX_GAP_CHR4:
		data = &large_db1.srv_gap_chr4;
		len = sizeof (large_db1.srv_gap_chr4);
		break;
	case IDX_B3_CHR1:
		data = &large_db1.srv_b3_chr1;
		len = sizeof (large_db1.srv_b3_chr1);
		break;
	case IDX_B1_CHR1:
		data = &large_db1.srv_b1_chr1;
		len = sizeof (large_db1.srv_b1_chr1);
		break;
	case IDX_B1_CHR2:
		data = &large_db1.srv_b1_chr2;
		len = sizeof (large_db1.srv_b1_chr2);
		break;
	case IDX_B1_CHR4:
		data = large_db1.srv_b1_chr4;
		len = sizeof (large_db1.srv_b1_chr4);
		break;
	case IDX_B2_CHR1:
		data = &large_db1.srv_b2_chr1;
		len = sizeof (large_db1.srv_b2_chr1);
		break;
	case IDX_B5_CHR1:
		data = &large_db1.srv_b5_chr1;
		len = sizeof (large_db1.srv_b5_chr1);
		break;
	case IDX_C1_CHR1:
		data = &large_db1.srv_c1_chr1;
		len = sizeof (large_db1.srv_c1_chr1);
		break;
	case IDX_F_CHR1:
		data = large_db1.srv_f_chr1;
		len = sizeof (large_db1.srv_f_chr1);
		break;
	case IDX_F_CHR2:
		data = &large_db1.srv_f_chr2;
		len = sizeof (large_db1.srv_f_chr2);
		break;
	case IDX_F_CHR3:
		data = &large_db1.srv_f_chr3;
		len = sizeof (large_db1.srv_f_chr3);
		break;
	case IDX_F_CHR4:
		data = &large_db1.srv_f_chr4;
		len = sizeof (large_db1.srv_f_chr4);
		break;
	case IDX_F_CHR5:
		data = large_db1.srv_f_chr5;
		len = sizeof (large_db1.srv_f_chr5);
		break;
	case IDX_F_CHR6:
		data = &large_db1.srv_f_chr6;
		len = sizeof (large_db1.srv_f_chr6);
		break;
	case IDX_F_CHR7:
		data = large_db1.srv_f_chr7;
		len = sizeof (large_db1.srv_f_chr7);
		break;
	case IDX_F_CHR8:
		data = large_db1.srv_f_chr8;
		len = sizeof (large_db1.srv_f_chr8);
		break;
	case IDX_F_CHR9:
		data = large_db1.srv_f_chr9;
		len = sizeof (large_db1.srv_f_chr9);
		break;
	case IDX_F_CHR10:
		data = large_db1.srv_f_chr10;
		len = sizeof (large_db1.srv_f_chr10);
		break;
	case IDX_C2_CHAR1:
		data = &large_db1.srv_c2_chr1;
		len = sizeof (large_db1.srv_c2_chr1);
		break;
	case IDX_C2_CHAR2:
		data = large_db1.srv_c2_chr2;
		len = sizeof (large_db1.srv_c2_chr2);
		break;
	case IDX_C2_CHAR3:
		data = large_db1.srv_c2_chr3;
		len = sizeof (large_db1.srv_c2_chr3);
		break;
	case IDX_C2_CHAR4:
		data = large_db1.srv_c2_chr4;
		len = sizeof (large_db1.srv_c2_chr4);
		break;
	case IDX_C2_CHAR5:
		data = large_db1.srv_c2_chr5;
		len = sizeof (large_db1.srv_c2_chr5);
		break;
	case IDX_C2_CHAR6:
		data = large_db1.srv_c2_chr6;
		len = sizeof (large_db1.srv_c2_chr6);
		break;
	case IDX_C2_CHAR7:
		data = large_db1.srv_c2_chr7;
		len = sizeof (large_db1.srv_c2_chr7);
		break;
	case IDX_C2_CHAR8:
		data = large_db1.srv_c2_chr8;
		len = sizeof (large_db1.srv_c2_chr8);
		break;
	case IDX_C2_CHAR9:
		if (!large_db1.authorization) {
			return BLE_ATT_ERR_INSUFFICIENT_AUTHOR;
		}

		data = large_db1.srv_c2_chr9;
		len = sizeof (large_db1.srv_c2_chr9);
		break;
	case IDX_C2_CHAR10:
		data = large_db1.srv_c2_chr10;
		len = sizeof (large_db1.srv_c2_chr10);
		break;
	case IDX_C2_CHAR11:
		data = large_db1.srv_c2_chr11;
		len = sizeof (large_db1.srv_c2_chr11);
		break;
	case IDX_C2_CHAR12:
		data = large_db1.srv_c2_chr12;
		len = sizeof (large_db1.srv_c2_chr12);
		break;
	case IDX_C2_CHAR13:
		data = large_db1.srv_c2_chr13;
		len = sizeof (large_db1.srv_c2_chr13);
		break;
	case IDX_C2_CHAR14:
		data = large_db1.srv_c2_chr14;
		len = sizeof (large_db1.srv_c2_chr14);
		break;
	case IDX_C2_CHAR15:
		data = large_db1.srv_c2_chr15;
		len = sizeof (large_db1.srv_c2_chr15);
		break;
	case IDX_C2_CHAR16:
		data = large_db1.srv_c2_chr16;
		len = sizeof (large_db1.srv_c2_chr16);
		break;
	case IDX_C2_CHAR17:
		data = large_db1.srv_c2_chr17;
		len = sizeof (large_db1.srv_c2_chr17);
		break;
	case IDX_C2_CHAR18:
		data = large_db1.srv_c2_chr18;
		len = sizeof (large_db1.srv_c2_chr18);
		break;
	case IDX_C2_CHAR19:
		data = large_db1.srv_c2_chr19;
		len = sizeof (large_db1.srv_c2_chr19);
		break;
	case IDX_C2_CHAR20:
		data = large_db1.srv_c2_chr20;
		len = sizeof (large_db1.srv_c2_chr20);
		break;
	case IDX_C2_CHAR21:
		data = large_db1.srv_c2_chr21;
		len = sizeof (large_db1.srv_c2_chr21);
		break;
	case IDX_C2_CHAR22:
		data = large_db1.srv_c2_chr22;
		len = sizeof (large_db1.srv_c2_chr22);
		break;
	default:
		data = NULL;
		len = 0;
		break;
	};

	if (data && len > 0) {

		print_bytes(data, len);
		printf("\n");

		rc = os_mbuf_append(ctxt->om, data, len);
		return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
	} else {
		printf("This is not included READ property (%d)\n", attr_idx);
		return BLE_ATT_ERR_READ_NOT_PERMITTED;
	}
}

static int
gatt_svr_access_write(uint16_t conn_handle, int attr_idx, uint16_t attr_handle,
		struct ble_gatt_access_ctxt *ctxt)
{
	int rc;
	void *data;
	uint16_t len;

	if (ctxt->om == NULL) {
		printf("write authorization check\n");
		if (!large_db1.authorization) {
			return BLE_ATT_ERR_INSUFFICIENT_AUTHOR;
		}
		return 0;
	}

	printf("[PTS] chr write: idx=%d handle=0x%04x\n", attr_idx, attr_handle);

	switch (attr_idx) {
	case IDX_ATT_CHR2:
		data = &large_db1.srv_att_chr2;
		len = sizeof (large_db1.srv_att_chr2);
		break;
	case IDX_A_CHR2:
		data = large_db1.srv_a_chr2;
		len = sizeof (large_db1.srv_a_chr2);
		break;
	case IDX_A_CHR3:
		data = large_db1.srv_a_chr3;
		len = sizeof (large_db1.srv_a_chr3);
		break;
	case IDX_A_CHR4:
		data = &large_db1.srv_a_chr4;
		len = sizeof (large_db1.srv_a_chr4);
		break;
	case IDX_GAP_CHR1:
		data = large_db1.srv_gap_chr1;
		len = sizeof (large_db1.srv_gap_chr1);
		break;
	case IDX_GAP_CHR2:
		data = &large_db1.srv_gap_chr2;
		len = sizeof (large_db1.srv_gap_chr2);
		break;
	case IDX_B4_CHR1:
		data = &large_db1.srv_a_chr4;
		len = sizeof (large_db1.srv_a_chr4);
		break;
	case IDX_B3_CHR1:
		data = &large_db1.srv_b3_chr1;
		len = sizeof (large_db1.srv_b3_chr1);
		break;
	case IDX_B1_CHR1:
		data = &large_db1.srv_b1_chr1;
		len = sizeof (large_db1.srv_b1_chr1);
		break;
	case IDX_B1_CHR2:
		data = &large_db1.srv_b1_chr2;
		len = sizeof (large_db1.srv_b1_chr2);
		break;
	case IDX_B2_CHR1:
		if (!large_db1.authorization) {
			return BLE_ATT_ERR_INSUFFICIENT_AUTHOR;
		}

		data = &large_db1.srv_b2_chr1;
		len = sizeof (large_db1.srv_b2_chr1);
		break;
	case IDX_B5_CHR1:
		data = &large_db1.srv_b5_chr1;
		len = sizeof (large_db1.srv_b5_chr1);
		break;
	case IDX_C1_CHR1:
		data = &large_db1.srv_c1_chr1;
		len = sizeof (large_db1.srv_c1_chr1);
		break;
	case IDX_F_CHR2:
		data = &large_db1.srv_f_chr2;
		len = sizeof (large_db1.srv_f_chr2);
		break;
	case IDX_F_CHR3:
		data = &large_db1.srv_f_chr3;
		len = sizeof (large_db1.srv_f_chr3);
		break;
	case IDX_F_CHR4:
		data = &large_db1.srv_f_chr4;
		len = sizeof (large_db1.srv_f_chr4);
		break;
	case IDX_F_CHR6:
		data = &large_db1.srv_f_chr6;
		len = sizeof (large_db1.srv_f_chr6);
		break;
	case IDX_C2_CHAR2:
		data = large_db1.srv_c2_chr2;
		len = sizeof (large_db1.srv_c2_chr2);
		break;
	case IDX_C2_CHAR3:
		data = large_db1.srv_c2_chr3;
		len = sizeof (large_db1.srv_c2_chr3);
		break;
	case IDX_C2_CHAR4:
		data = large_db1.srv_c2_chr4;
		len = sizeof (large_db1.srv_c2_chr4);
		break;
	case IDX_C2_CHAR5:
		data = large_db1.srv_c2_chr5;
		len = sizeof (large_db1.srv_c2_chr5);
		break;
	case IDX_C2_CHAR6:
		data = large_db1.srv_c2_chr6;
		len = sizeof (large_db1.srv_c2_chr6);
		break;
	case IDX_C2_CHAR7:
		data = large_db1.srv_c2_chr7;
		len = sizeof (large_db1.srv_c2_chr7);
		break;
	case IDX_C2_CHAR8:
		data = large_db1.srv_c2_chr8;
		len = sizeof (large_db1.srv_c2_chr8);
		break;
	case IDX_C2_CHAR9:
		if (!large_db1.authorization) {
			return BLE_ATT_ERR_INSUFFICIENT_AUTHOR;
		}

		data = large_db1.srv_c2_chr9;
		len = sizeof (large_db1.srv_c2_chr9);
		break;
	case IDX_C2_CHAR10:
		data = large_db1.srv_c2_chr10;
		len = sizeof (large_db1.srv_c2_chr10);
		break;
	case IDX_C2_CHAR11:
		data = large_db1.srv_c2_chr11;
		len = sizeof (large_db1.srv_c2_chr11);
		break;
	case IDX_C2_CHAR12:
		data = large_db1.srv_c2_chr12;
		len = sizeof (large_db1.srv_c2_chr12);
		break;
	case IDX_C2_CHAR13:
		data = large_db1.srv_c2_chr13;
		len = sizeof (large_db1.srv_c2_chr13);
		break;
	case IDX_C2_CHAR14:
		data = large_db1.srv_c2_chr14;
		len = sizeof (large_db1.srv_c2_chr14);
		break;
	case IDX_C2_CHAR15:
		data = large_db1.srv_c2_chr15;
		len = sizeof (large_db1.srv_c2_chr15);
		break;
	case IDX_C2_CHAR16:
		data = large_db1.srv_c2_chr16;
		len = sizeof (large_db1.srv_c2_chr16);
		break;
	case IDX_C2_CHAR17:
		data = large_db1.srv_c2_chr17;
		len = sizeof (large_db1.srv_c2_chr17);
		break;
	case IDX_C2_CHAR18:
		data = large_db1.srv_c2_chr18;
		len = sizeof (large_db1.srv_c2_chr18);
		break;
	case IDX_C2_CHAR19:
		data = large_db1.srv_c2_chr19;
		len = sizeof (large_db1.srv_c2_chr19);
		break;
	case IDX_C2_CHAR20:
		data = large_db1.srv_c2_chr20;
		len = sizeof (large_db1.srv_c2_chr20);
		break;
	case IDX_C2_CHAR21:
		data = large_db1.srv_c2_chr21;
		len = sizeof (large_db1.srv_c2_chr21);
		break;
	case IDX_C2_CHAR22:
		data = large_db1.srv_c2_chr22;
		len = sizeof (large_db1.srv_c2_chr22);
		break;
	default:
		data = NULL;
		len = 0;
		break;
	}

	if (data && len > 0) {
		uint16_t write_len;

		rc = gatt_svr_chr_write(ctxt->om, 0, len,
				data, &write_len);

		print_bytes(data, len);
		printf("\n");

		if (rc == 0 && IDX_GAP_CHR1) {
			large_db1.dev_name_len = write_len;
		}

		return rc == 0 ? 0 : rc;
	} else {
		printf("This is not included WRITE property %d\n", attr_idx);
		return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
	}
}

static int
gatt_svr_access_read_dsc(uint16_t conn_handle, int attr_idx, uint16_t attr_handle,
		struct ble_gatt_access_ctxt *ctxt)
{
	int rc;
	void *data;
	uint16_t len;

	printf("[PTS] dsc read: idx=%d handle=0x%04x\n", attr_idx, attr_handle);

	switch (attr_idx) {
	case IDX_B1_CHR2_DSC1:
		data = &large_db1.srv_b1_chr2_dsc1;
		len  = sizeof (large_db1.srv_b1_chr2_dsc1);
		break;
	case IDX_B1_CHR4_DSC1:
		data = large_db1.srv_b1_chr4_dsc1;
		len  = sizeof (large_db1.srv_b1_chr4_dsc1);
		break;
	case IDX_B2_CHR1_DSC1:
		data = &large_db1.srv_b2_chr1_dsc1;
		len  = sizeof (large_db1.srv_b2_chr1_dsc1);
		break;
	case IDX_B2_CHR1_DSC2:
		data = large_db1.srv_b2_chr1_dsc2;
		len  = sizeof (large_db1.srv_b2_chr1_dsc2);
		break;
	case IDX_B2_CHR1_DSC3:
		data = large_db1.srv_b2_chr1_dsc3;
		len  = sizeof (large_db1.srv_b2_chr1_dsc3);
		break;
	case IDX_B2_CHR1_DSC4:
		data = &large_db1.srv_b2_chr1_dsc4;
		len  = sizeof (large_db1.srv_b2_chr1_dsc4);
		break;
	case IDX_B5_CHR1_DSC1:
		data = &large_db1.srv_b5_chr1_dsc1;
		len  = sizeof (large_db1.srv_b5_chr1_dsc1);
		break;
	case IDX_B5_CHR1_DSC2:
		if (!large_db1.authorization) {
			return BLE_ATT_ERR_INSUFFICIENT_AUTHOR;
		}

		data = &large_db1.srv_b5_chr1_dsc2;
		len  = sizeof (large_db1.srv_b5_chr1_dsc2);
		break;
	case IDX_B5_CHR1_DSC3:
		data = &large_db1.srv_b5_chr1_dsc3;
		len  = sizeof (large_db1.srv_b5_chr1_dsc3);
		break;
	case IDX_C1_CHR1_DSC1:
		data = &large_db1.srv_c1_chr1_dsc1;
		len  = sizeof (large_db1.srv_c1_chr1_dsc1);
		break;
	case IDX_C1_CHR1_DSC2:
		data = &large_db1.srv_c1_chr1_dsc2;
		len  = sizeof (large_db1.srv_c1_chr1_dsc2);
		break;
	case IDX_F_CHR1_DSC1:
		data = large_db1.srv_f_chr1_dsc1;
		len  = sizeof (large_db1.srv_f_chr1_dsc1);
		break;
	case IDX_F_CHR2_DSC1:
		data = large_db1.srv_f_chr2_dsc1;
		len  = sizeof (large_db1.srv_f_chr2_dsc1);
		break;
	case IDX_F_CHR3_DSC1:
		data = large_db1.srv_f_chr3_dsc1;
		len  = sizeof (large_db1.srv_f_chr3_dsc1);
		break;
	case IDX_F_CHR4_DSC1:
		data = large_db1.srv_f_chr4_dsc1;
		len  = sizeof (large_db1.srv_f_chr4_dsc1);
		break;
	case IDX_F_CHR5_DSC1:
		data = large_db1.srv_f_chr5_dsc1;
		len  = sizeof (large_db1.srv_f_chr5_dsc1);
		break;
	case IDX_F_CHR7_DSC1:
		data = large_db1.srv_f_chr7_dsc1;
		len  = sizeof (large_db1.srv_f_chr7_dsc1);
		break;
	case IDX_F_CHR8_DSC1:
		data = large_db1.srv_f_chr8_dsc1;
		len  = sizeof (large_db1.srv_f_chr8_dsc1);
		break;
	case IDX_F_CHR9_DSC1:
		data = large_db1.srv_f_chr9_dsc1;
		len  = sizeof (large_db1.srv_f_chr9_dsc1);
		break;
	case IDX_F_CHR10_DSC1:
		data = large_db1.srv_f_chr9_dsc1;
		len  = sizeof (large_db1.srv_f_chr9_dsc1);
		break;
	case IDX_C2_CHAR2_DSC1:
		data = large_db1.srv_c2_chr2_dsc1;
		len  = sizeof (large_db1.srv_c2_chr2_dsc1);
		break;
	case IDX_C2_CHAR3_DSC1:
		data = large_db1.srv_c2_chr3_dsc1;
		len  = sizeof (large_db1.srv_c2_chr3_dsc1);
		break;
	case IDX_C2_CHAR4_DSC1:
		data = large_db1.srv_c2_chr4_dsc1;
		len  = sizeof (large_db1.srv_c2_chr4_dsc1);
		break;
	case IDX_C2_CHAR5_DSC1:
		data = large_db1.srv_c2_chr5_dsc1;
		len  = sizeof (large_db1.srv_c2_chr5_dsc1);
		break;
	case IDX_C2_CHAR6_DSC1:
		data = large_db1.srv_c2_chr6_dsc1;
		len  = sizeof (large_db1.srv_c2_chr6_dsc1);
		break;
	case IDX_C2_CHAR7_DSC1:
		data = large_db1.srv_c2_chr7_dsc1;
		len  = sizeof (large_db1.srv_c2_chr7_dsc1);
		break;
	case IDX_C2_CHAR8_DSC1:
		data = large_db1.srv_c2_chr8_dsc1;
		len  = sizeof (large_db1.srv_c2_chr8_dsc1);
		break;
	case IDX_C2_CHAR9_DSC1:
		if (!large_db1.authorization) {
			return BLE_ATT_ERR_INSUFFICIENT_AUTHOR;
		}

		data = large_db1.srv_c2_chr9_dsc1;
		len  = sizeof (large_db1.srv_c2_chr9_dsc1);
		break;
	case IDX_C2_CHAR10_DSC1:
		data = large_db1.srv_c2_chr10_dsc1;
		len  = sizeof (large_db1.srv_c2_chr10_dsc1);
		break;
	case IDX_C2_CHAR11_DSC1:
		data = large_db1.srv_c2_chr11_dsc1;
		len  = sizeof (large_db1.srv_c2_chr11_dsc1);
		break;
	case IDX_C2_CHAR12_DSC1:
		data = large_db1.srv_c2_chr12_dsc1;
		len  = sizeof (large_db1.srv_c2_chr12_dsc1);
		break;
	case IDX_C2_CHAR13_DSC1:
		data = large_db1.srv_c2_chr13_dsc1;
		len  = sizeof (large_db1.srv_c2_chr13_dsc1);
		break;
	case IDX_C2_CHAR14_DSC1:
		data = large_db1.srv_c2_chr14_dsc1;
		len  = sizeof (large_db1.srv_c2_chr14_dsc1);
		break;
	case IDX_C2_CHAR15_DSC1:
		data = large_db1.srv_c2_chr15_dsc1;
		len  = sizeof (large_db1.srv_c2_chr15_dsc1);
		break;
	case IDX_C2_CHAR16_DSC1:
		data = large_db1.srv_c2_chr16_dsc1;
		len  = sizeof (large_db1.srv_c2_chr16_dsc1);
		break;
	case IDX_C2_CHAR17_DSC1:
		data = large_db1.srv_c2_chr17_dsc1;
		len  = sizeof (large_db1.srv_c2_chr17_dsc1);
		break;
	case IDX_C2_CHAR18_DSC1:
		data = large_db1.srv_c2_chr18_dsc1;
		len  = sizeof (large_db1.srv_c2_chr18_dsc1);
		break;
	case IDX_C2_CHAR19_DSC1:
		data = large_db1.srv_c2_chr19_dsc1;
		len  = sizeof (large_db1.srv_c2_chr19_dsc1);
		break;
	case IDX_C2_CHAR20_DSC1:
		data = large_db1.srv_c2_chr20_dsc1;
		len  = sizeof (large_db1.srv_c2_chr20_dsc1);
		break;
	case IDX_C2_CHAR21_DSC1:
		data = large_db1.srv_c2_chr21_dsc1;
		len  = sizeof (large_db1.srv_c2_chr21_dsc1);
		break;
	case IDX_C2_CHAR22_DSC1:
		data = large_db1.srv_c2_chr22_dsc1;
		len  = sizeof (large_db1.srv_c2_chr22_dsc1);
		break;
	default:
		data = NULL;
		len = 0;
		break;
	}

	if (data && len > 0) {

		print_bytes(data, len);
		printf("\n");

		rc = os_mbuf_append(ctxt->om, data, len);
		return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
	} else {
		printf("This is not included READ property (%d)\n", attr_idx);
		return BLE_ATT_ERR_READ_NOT_PERMITTED;
	}
}

static int
gatt_svr_access_write_dsc(uint16_t conn_handle, int attr_idx, uint16_t attr_handle,
		struct ble_gatt_access_ctxt *ctxt)
{
	int rc;
	void *data;
	uint16_t len;

	printf("[PTS] dsc write: idx=%d handle=0x%04x\n", attr_idx, attr_handle);

	switch (attr_idx) {
	case IDX_B1_CHR2_DSC1:
		data = &large_db1.srv_b1_chr2_dsc1;
		len  = sizeof (large_db1.srv_b1_chr2_dsc1);
		break;
	case IDX_B2_CHR1_DSC2:
		data = large_db1.srv_b2_chr1_dsc2;
		len  = sizeof (large_db1.srv_b2_chr1_dsc2);
		break;
	case IDX_B5_CHR1_DSC1:
		data = &large_db1.srv_b5_chr1_dsc1;
		len  = sizeof (large_db1.srv_b5_chr1_dsc1);
		break;
	case IDX_B5_CHR1_DSC2:
		if (!large_db1.authorization) {
			return BLE_ATT_ERR_INSUFFICIENT_AUTHOR;
		}

		data = &large_db1.srv_b5_chr1_dsc2;
		len  = sizeof (large_db1.srv_b5_chr1_dsc2);
		break;
	case IDX_B5_CHR1_DSC3:
		data = &large_db1.srv_b5_chr1_dsc3;
		len  = sizeof (large_db1.srv_b5_chr1_dsc3);
		break;
	case IDX_C1_CHR1_DSC2:
		data = &large_db1.srv_c1_chr1_dsc2;
		len  = sizeof (large_db1.srv_c1_chr1_dsc2);
		break;
	case IDX_C1_CHR1_DSC3:
		data = &large_db1.srv_c1_chr1_dsc3;
		len  = sizeof (large_db1.srv_c1_chr1_dsc3);
		break;
	case IDX_C2_CHAR2_DSC1:
		data = large_db1.srv_c2_chr2_dsc1;
		len  = sizeof (large_db1.srv_c2_chr2_dsc1);
		break;
	case IDX_C2_CHAR3_DSC1:
		data = large_db1.srv_c2_chr3_dsc1;
		len  = sizeof (large_db1.srv_c2_chr3_dsc1);
		break;
	case IDX_C2_CHAR4_DSC1:
		data = large_db1.srv_c2_chr4_dsc1;
		len  = sizeof (large_db1.srv_c2_chr4_dsc1);
		break;
	case IDX_C2_CHAR5_DSC1:
		data = large_db1.srv_c2_chr5_dsc1;
		len  = sizeof (large_db1.srv_c2_chr5_dsc1);
		break;
	case IDX_C2_CHAR6_DSC1:
		data = large_db1.srv_c2_chr6_dsc1;
		len  = sizeof (large_db1.srv_c2_chr6_dsc1);
		break;
	case IDX_C2_CHAR7_DSC1:
		data = large_db1.srv_c2_chr7_dsc1;
		len  = sizeof (large_db1.srv_c2_chr7_dsc1);
		break;
	case IDX_C2_CHAR8_DSC1:
		data = large_db1.srv_c2_chr8_dsc1;
		len  = sizeof (large_db1.srv_c2_chr8_dsc1);
		break;
	case IDX_C2_CHAR9_DSC1:
		if (!large_db1.authorization) {
			return BLE_ATT_ERR_INSUFFICIENT_AUTHOR;
		}

		data = large_db1.srv_c2_chr9_dsc1;
		len  = sizeof (large_db1.srv_c2_chr9_dsc1);
		break;
	case IDX_C2_CHAR10_DSC1:
		data = large_db1.srv_c2_chr10_dsc1;
		len  = sizeof (large_db1.srv_c2_chr10_dsc1);
		break;
	case IDX_C2_CHAR11_DSC1:
		data = large_db1.srv_c2_chr11_dsc1;
		len  = sizeof (large_db1.srv_c2_chr11_dsc1);
		break;
	case IDX_C2_CHAR12_DSC1:
		data = large_db1.srv_c2_chr12_dsc1;
		len  = sizeof (large_db1.srv_c2_chr12_dsc1);
		break;
	case IDX_C2_CHAR13_DSC1:
		data = large_db1.srv_c2_chr13_dsc1;
		len  = sizeof (large_db1.srv_c2_chr13_dsc1);
		break;
	case IDX_C2_CHAR14_DSC1:
		data = large_db1.srv_c2_chr14_dsc1;
		len  = sizeof (large_db1.srv_c2_chr14_dsc1);
		break;
	case IDX_C2_CHAR15_DSC1:
		data = large_db1.srv_c2_chr15_dsc1;
		len  = sizeof (large_db1.srv_c2_chr15_dsc1);
		break;
	case IDX_C2_CHAR16_DSC1:
		data = large_db1.srv_c2_chr16_dsc1;
		len  = sizeof (large_db1.srv_c2_chr16_dsc1);
		break;
	case IDX_C2_CHAR17_DSC1:
		data = large_db1.srv_c2_chr17_dsc1;
		len  = sizeof (large_db1.srv_c2_chr17_dsc1);
		break;
	case IDX_C2_CHAR18_DSC1:
		data = large_db1.srv_c2_chr18_dsc1;
		len  = sizeof (large_db1.srv_c2_chr18_dsc1);
		break;
	case IDX_C2_CHAR19_DSC1:
		data = large_db1.srv_c2_chr19_dsc1;
		len  = sizeof (large_db1.srv_c2_chr19_dsc1);
		break;
	case IDX_C2_CHAR20_DSC1:
		data = large_db1.srv_c2_chr20_dsc1;
		len  = sizeof (large_db1.srv_c2_chr20_dsc1);
		break;
	case IDX_C2_CHAR21_DSC1:
		data = large_db1.srv_c2_chr21_dsc1;
		len  = sizeof (large_db1.srv_c2_chr21_dsc1);
		break;
	case IDX_C2_CHAR22_DSC1:
		data = large_db1.srv_c2_chr22_dsc1;
		len  = sizeof (large_db1.srv_c2_chr22_dsc1);
		break;
	default:
		data = NULL;
		len = 0;
		break;
	}

	if (data && len > 0) {
		rc = gatt_svr_chr_write(ctxt->om, 0, len,
				data, NULL);

		print_bytes(data, len);
		printf("\n");

		return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
	} else {
		printf("This is not included WRITE property %d\n", attr_idx);
		return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
	}
}

static int
gatt_svr_access_test(uint16_t conn_handle, uint16_t attr_handle,
		struct ble_gatt_access_ctxt *ctxt,
		void *arg)
{
	int attr_idx = (int)arg;
	int rc = 0;

	if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
		rc = gatt_svr_access_read(conn_handle, attr_idx, attr_handle, ctxt);
	} else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
		rc = gatt_svr_access_write(conn_handle, attr_idx, attr_handle, ctxt);
	} else if (ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC) {
		rc = gatt_svr_access_read_dsc(conn_handle, attr_idx, attr_handle, ctxt);
	} else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_DSC) {
		rc = gatt_svr_access_write_dsc(conn_handle, attr_idx, attr_handle, ctxt);
	} else {
		printf("Unknown operation code : %d\n", ctxt->op);
		return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
	}

	return rc;
}

static int
gatt_svr_db_add(const struct ble_gatt_svc_def *defs)
{
	int rc;

	rc = ble_gatts_count_cfg(defs);
	if (rc != 0) {
		return rc;
	}

	rc = ble_gatts_add_svcs(defs);
	if (rc != 0) {
		return rc;
	}

	return 0;
}

void
gatt_svr_large_db1_author(uint8_t enable)
{
	large_db1.authorization = enable;
}

int
gatt_svr_init_large_db1(void)
{
	int rc;

	rc = ble_gatts_reset();
	if (rc != 0) {
		return rc;
	}

	rc = gatt_svr_db_add(gatt_svr_svc_b5);
	if (rc != 0) {
		return rc;
	}

	rc = gatt_svr_db_add(gatt_svr_svc_d);
	if (rc != 0) {
		return rc;
	}

	rc = gatt_svr_db_add(gatt_svr_svc_att);
	if (rc != 0) {
		return rc;
	}

	rc = gatt_svr_db_add(gatt_svr_svc_a);
	if (rc != 0) {
		return rc;
	}

	rc = gatt_svr_db_add(gatt_svr_svc_b4);
	if (rc != 0) {
		return rc;
	}

	rc = gatt_svr_db_add(gatt_svr_svc_gap);
	if (rc != 0) {
		return rc;
	}

	rc = gatt_svr_db_add(gatt_svr_svc_b3);
	if (rc != 0) {
		return rc;
	}

	rc = gatt_svr_db_add(gatt_svr_svc_b1);
	if (rc != 0) {
		return rc;
	}

	rc = gatt_svr_db_add(gatt_svr_svc_b2);
	if (rc != 0) {
		return rc;
	}

	rc = gatt_svr_db_add(gatt_svr_svc_c1);
	if (rc != 0) {
		return rc;
	}

	rc = gatt_svr_db_add(gatt_svr_svc_f);
	if (rc != 0) {
		return rc;
	}

	rc = gatt_svr_db_add(gatt_svr_svc_c2);
	if (rc != 0) {
		return rc;
	}

	rc = ble_gatts_start();
	if (rc != 0) {
		return rc;
	}

	return 0;
}
