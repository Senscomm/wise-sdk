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

#include <cmsis_os.h>
#include <hal/kernel.h>
#include <hal/device.h>
#include <hal/clk.h>
#include <hal/pinctrl.h>
#include <hal/serial.h>
#include <hal/console.h>
#include <hal/kmem.h>
#include <hal/crypto.h>

/***************** PKE microcode ******************/
#define MICROCODE_PDBL                        (0x04)
#define MICROCODE_PADD                        (0x08)
#define MICROCODE_PVER                        (0x0C)
#define MICROCODE_PMUL                        (0x10)
#define MICROCODE_MODEXP                      (0x14)
#define MICROCODE_MODMUL                      (0x18)
#define MICROCODE_MODINV                      (0x1C)
#define MICROCODE_MODADD                      (0x20)
#define MICROCODE_MODSUB                      (0x24)
#define MICROCODE_MGMR_PRE                    (0x28)
#define MICROCODE_INTMUL                      (0x2C)
#define MICROCODE_Ed25519_PMUL                (0x30)
#define MICROCODE_Ed25519_PADD                (0x34)
#define MICROCODE_C25519_PMUL                 (0x38)

/*********** PKE register action offset ************/
#define PKE_EXE_OUTPUT_AFFINE                 (0x10)
#define PKE_EXE_R1_MONT_R0_AFFINE             (0x09)
#define PKE_EXE_R1_MONT_R0_MONT               (0x0A)
#define PKE_EXE_R1_AFFINE_R0_AFFINE           (0x05)
#define PKE_EXE_R1_AFFINE_R0_MONT             (0x06)
#define PKE_EXE_ECCP_POINT_MUL                (PKE_EXE_OUTPUT_AFFINE + PKE_EXE_R1_AFFINE_R0_MONT)
#define PKE_EXE_ECCP_POINT_ADD                (PKE_EXE_OUTPUT_AFFINE + PKE_EXE_R1_AFFINE_R0_AFFINE)
#define PKE_EXE_ECCP_POINT_DBL                (PKE_EXE_OUTPUT_AFFINE + PKE_EXE_R1_MONT_R0_AFFINE)
#define PKE_EXE_ECCP_POINT_VER                (PKE_EXE_OUTPUT_AFFINE + PKE_EXE_R1_AFFINE_R0_MONT)

#define PKE_EXE_CFG_ALL_MONT                  (0x002A)
#define PKE_EXE_CFG_ALL_NON_MONT              (0x0000)
#define PKE_EXE_CFG_MOD_EXP                   (0x0316)

#define GET_WORD_LEN(bitLen)                  (((bitLen)+31)/32)
#define GET_BYTE_LEN(bitLen)                  (((bitLen)+7)/8)

/*********** some PKE algorithm operand length ************/
#define OPERAND_MAX_BIT_LEN                   (4096)
#define OPERAND_MAX_WORD_LEN                  (GET_WORD_LEN(OPERAND_MAX_BIT_LEN))

#define ECCP_MAX_BIT_LEN                      (521)  //ECC521
#define ECCP_MAX_BYTE_LEN                     (GET_BYTE_LEN(ECCP_MAX_BIT_LEN))
#define ECCP_MAX_WORD_LEN                     (GET_WORD_LEN(ECCP_MAX_BIT_LEN))

#define C25519_BYTE_LEN                       (256/8)
#define C25519_WORD_LEN                       (256/32)

#define Ed25519_BYTE_LEN                      C25519_BYTE_LEN
#define Ed25519_WORD_LEN                      C25519_WORD_LEN

#define MAX_RSA_WORD_LEN                      OPERAND_MAX_WORD_LEN
#define MAX_RSA_BIT_LEN                       (MAX_RSA_WORD_LEN<<5)
#define MIN_RSA_BIT_LEN                       (512)

/***************** PKE register *******************/
#define PKE_CTRL           (*((volatile uint32_t *)(PKE_BASE_ADDR)))
#define PKE_CFG            (*((volatile uint32_t *)(PKE_BASE_ADDR+0x04)))
#define PKE_MC_PTR         (*((volatile uint32_t *)(PKE_BASE_ADDR+0x08)))
#define PKE_RISR           (*((volatile uint32_t *)(PKE_BASE_ADDR+0x0C)))
#define PKE_IMCR           (*((volatile uint32_t *)(PKE_BASE_ADDR+0x10)))
#define PKE_MISR           (*((volatile uint32_t *)(PKE_BASE_ADDR+0x14)))
#define PKE_RT_CODE        (*((volatile uint32_t *)(PKE_BASE_ADDR+0x24)))
#define PKE_EXE_CONF       (*((volatile uint32_t *)(PKE_BASE_ADDR+0x50)))
#define PKE_VERSION        (*((volatile uint32_t *)(PKE_BASE_ADDR+0xFC)))
#define PKE_A(a, step)     ((volatile uint32_t *)(PKE_BASE_ADDR+0x0400+(a)*(step)))
#define PKE_B(a, step)     ((volatile uint32_t *)(PKE_BASE_ADDR+0x1000+(a)*(step)))

/*********** PKE register action offset ************/
#define PKE_INT_ENABLE_OFFSET                 (8)
#define PKE_START_CALC                        (1)

typedef struct
{
	uint32_t eccp_p_bitLen;        //bit length of prime p
	uint32_t eccp_n_bitLen;        //bit length of order n
	uint32_t *eccp_p;              //prime p
	uint32_t *eccp_p_h;
	uint32_t *eccp_p_n1;
	uint32_t *eccp_a;
	uint32_t *eccp_b;
	uint32_t *eccp_Gx;
	uint32_t *eccp_Gy;
	uint32_t *eccp_n;              //order of curve or point(Gx,Gy)
	uint32_t *eccp_n_h;
	uint32_t *eccp_n_n1;
} eccp_curve_t;

enum PKE_RET_CODE
{
	PKE_SUCCESS = 0,
};

uint32_t const secp256r1_p[8]        = {0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0x00000000,0x00000000,0x00000000,0x00000001,0xFFFFFFFF};
uint32_t const secp256r1_a[8]        = {0xFFFFFFFC,0xFFFFFFFF,0xFFFFFFFF,0x00000000,0x00000000,0x00000000,0x00000001,0xFFFFFFFF};
uint32_t const secp256r1_b[8]        = {0x27D2604B,0x3BCE3C3E,0xCC53B0F6,0x651D06B0,0x769886BC,0xB3EBBD55,0xAA3A93E7,0x5AC635D8};
uint32_t const secp256r1_gx[8]       = {0xD898C296,0xF4A13945,0x2DEB33A0,0x77037D81,0x63A440F2,0xF8BCE6E5,0xE12C4247,0x6B17D1F2};
uint32_t const secp256r1_gy[8]       = {0x37BF51F5,0xCBB64068,0x6B315ECE,0x2BCE3357,0x7C0F9E16,0x8EE7EB4A,0xFE1A7F9B,0x4FE342E2};
uint32_t const secp256r1_n[8]        = {0xFC632551,0xF3B9CAC2,0xA7179E84,0xBCE6FAAD,0xFFFFFFFF,0xFFFFFFFF,0x00000000,0xFFFFFFFF};
uint32_t const secp256r1_p_h[8]      = {0x00000003,0x00000000,0xFFFFFFFF,0xFFFFFFFB,0xFFFFFFFE,0xFFFFFFFF,0xFFFFFFFD,0x00000004};
uint32_t const secp256r1_p_n1[1]     = {1};
uint32_t const secp256r1_n_h[8]      = {0xBE79EEA2,0x83244C95,0x49BD6FA6,0x4699799C,0x2B6BEC59,0x2845B239,0xF3D95620,0x66E12D94};
uint32_t const secp256r1_n_n1[1]     = {0xEE00BC4F};

static const eccp_curve_t secp256r1 = {
    256,
    256,
    (uint32_t *)secp256r1_p,
    (uint32_t *)secp256r1_p_h,
    (uint32_t *)secp256r1_p_n1,
    (uint32_t *)secp256r1_a,
    (uint32_t *)secp256r1_b,
    (uint32_t *)secp256r1_gx,
    (uint32_t *)secp256r1_gy,
    (uint32_t *)secp256r1_n,
    (uint32_t *)secp256r1_n_h,
    (uint32_t *)secp256r1_n_n1,
};

static uint32_t step;
osMutexId_t cyprto_pke_mutex; /* Lock for PKE */

/*
 * function: get pke IP version
 * return: pke IP version
 */
#ifdef CONFIG_CMD_DMESG
static uint32_t pke_get_version(void)
{
	return PKE_VERSION;
}
#endif

int cyprto_pke_probe(struct device *dev)
{
#ifdef CONFIG_CMD_DMESG
	printk("pke version : 0x%x\n", pke_get_version());
#endif
	if ((cyprto_pke_mutex = osMutexNew(NULL)) == NULL) {
		return -EINVAL;
	}
	return 0;
}

static void pke_mutex_acquire(void)
{
	if (osMutexAcquire(cyprto_pke_mutex, osWaitForever) != osOK) {
		assert(false);
	}
}

static void pke_mutex_release(void)
{
	osMutexRelease(cyprto_pke_mutex);
}

/*
 * Enable PKE APB clock
 */
static void pke_clock_enable(void)
{
	volatile uint32_t v = *SYS(CRYPTO_CFG);
	*SYS(CRYPTO_CFG) = v | (1 << 16);
}

/*
 * Disable PKE APB clock
 */
static void pke_clock_disable(void)
{
	volatile uint32_t v = *SYS(CRYPTO_CFG);
	*SYS(CRYPTO_CFG) = v & ~(1 << 16);
}

/*
 * set operand width
 * parameters:
 *     bitLen --------------------- input, bit length of operand
 * return: none
 * caution: please make sure 0 < bitLen <= OPERAND_MAX_BIT_LEN
 */
static void pke_set_operand_width(uint32_t bitLen)
{
	volatile uint32_t mask = ~(0x07FFFF);
	uint32_t cfg = 0, len;

	len = (bitLen + 255) / 256;

	if (1 == len) {
		cfg = 2;
		step = 0x24;
	} else if (2 == len) {
		cfg = 3;
		step = 0x44;
	} else if (len <= 4) {
		cfg = 4;
		step = 0x84;
	} else if (len <= 8) {
		cfg = 5;
		step = 0x104;
	} else if (len <= 16) {
		cfg = 6;
		step = 0x204;
	}

	cfg = (cfg << 16) | (bitLen);

	PKE_CFG &= mask;
	PKE_CFG |= cfg;
}

/*
 * load input operand to baseaddr
 * parameters:
 *     baseaddr ------------------- output, destination data
 *     data ----------------------- input, source data
 *     wordLen -------------------- input, word length of data
 * return: none
 */
static void pke_load_operand(uint32_t *baseaddr, uint32_t *data, uint32_t wordLen)
{
	uint32_t i;

	if (baseaddr != data) {
		for (i = 0; i < wordLen; i++) {
			*((volatile uint32_t *)baseaddr+i) = data[i];
		}
	}
}

/*
 * clear uint32 buffer
 * parameters:
 *     a -------------------------- input&output, word buffer a
 *     aWordLen ------------------- input, word length of buffer a
 * return: none
 */
static void uint32_clear(uint32_t *a, uint32_t wordLen)
{
	volatile uint32_t i = wordLen;

	while(i) {
		a[--i] = 0;
	}
}

/*
 * set operation micro code
 * parameters:
 *     addr ----------------------- input, specific micro code
 * return: none
 */
static void pke_set_microcode(uint32_t addr)
{
	PKE_MC_PTR = addr;
}

/*
 * clear finished and interrupt tag
 * parameters: none
 * return: none
 */
static void pke_clear_interrupt(void)
{
	volatile uint32_t mask = ~1;

	PKE_RISR &= mask;      //write 0 to clear
}

/*
 * start pke calc
 */
static void pke_start(void)
{
	volatile uint32_t flag = PKE_START_CALC;

	PKE_CTRL |= flag;
}

/*
 * wait till done
 */
static void pke_wait_till_done(void)
{
	volatile uint32_t flag = 1;

	while(!(PKE_RISR & flag))
	{;}
}

/*
 * return calc return code
 * parameters: none
 * return 0(success), other(error)
 */
static uint32_t pke_check_rt_code(void)
{
	return (uint8_t)(PKE_RT_CODE & 0x07);
}

/*
 * set operation micro code, start hardware, wait till done, and return code
 * parameters:
 *     micro_code ----------------- input, specific micro code
 * return: PKE_SUCCESS(success), other(inverse not exists or error)
 */
static uint32_t pke_set_micro_code_start_wait_return_code(uint32_t micro_code)
{
	pke_set_microcode(micro_code);

	pke_clear_interrupt();

	pke_start();

	pke_wait_till_done();

	return pke_check_rt_code();
}

/*
 * get result operand from baseaddr
 * parameters:
 *     baseaddr ------------------- input, source data
 *     data ----------------------- output, destination data
 *     wordLen -------------------- input, word length of data
 * return: none
 */
static void pke_read_operand(uint32_t *baseaddr, uint32_t *data, uint32_t wordLen)
{
	uint32_t i;

	if (baseaddr != data) {
		for (i = 0; i < wordLen; i++) {
			data[i] = *((volatile uint32_t *)baseaddr+i);
		}
	}
}

/*
 * calc H(R^2 mod modulus) and n1( - modulus ^(-1) mod 2^w ) for modMul,modExp, and pointMul. etc.
 *           here w is bit width of word, i,e. 32.
 * parameters:
 *     modulus -------------------- input, modulus
 *     bitLen --------------------- input, bit length of modulus
 *     H -------------------------- output, R^2 mod modulus
 *     n1 ------------------------- output,  - modulus ^(-1) mod 2^w, here w is 32 actually
 * return: PKE_SUCCESS(success), other(error)
 * caution:
 *     1. modulus must be odd
 *     2. please make sure word length of buffer H is equal to wordLen(word length of modulus),
 *        and n1 only need one word.
 *     3. bitLen must not be bigger than OPERAND_MAX_BIT_LEN
 */
static uint32_t pke_pre_calc_mont(const uint32_t *modulus, uint32_t bitLen, uint32_t *H, uint32_t *n1)
{
	uint32_t wordLen = GET_WORD_LEN(bitLen);
	uint32_t ret;

	pke_set_operand_width(bitLen);  //pke_set_operand_width(wordLen<<5);

	pke_load_operand((uint32_t *)(PKE_B(3,step)), (uint32_t *)modulus, wordLen);    //B3 modulus

	if ((step/4) > wordLen) {
		uint32_clear((uint32_t *)(PKE_B(3,step))+wordLen, (step/4)-wordLen);
		uint32_clear((uint32_t *)(PKE_A(3,step))+wordLen, (step/4)-wordLen);
	}

	ret = pke_set_micro_code_start_wait_return_code(MICROCODE_MGMR_PRE);
	if (PKE_SUCCESS != ret) {
		return ret;
	}

	if (NULL != H) {
		pke_read_operand((uint32_t *)(PKE_A(3,step)), H, wordLen);                  //A3 H
	}

	if (NULL != n1) {
		pke_read_operand((uint32_t *)(PKE_B(4,step)), n1, 1);                       //B4 n1
	}

	return PKE_SUCCESS;
}

/*
 * load the pre-calculated mont parameters H(R^2 mod modulus)
 * parameters:
 *     H -------------------------- input, R^2 mod modulus
 *     n1 ------------------------- input,  - modulus ^(-1) mod 2^w, here w is 32 actually
 *     wordLen -------------------- input, word length of modulus or H
 * return: none
 * caution:
 *     1. please make sure the 2 input parameters are both valid
 *     2. wordLen must not be bigger than OPERAND_MAX_WORD_LEN
 */
static void pke_load_pre_calc_mont(uint32_t *H, uint32_t *n1, uint32_t wordLen)
{
	pke_set_operand_width(wordLen<<5);

	pke_load_operand((uint32_t *)(PKE_A(3,step)), H, wordLen);
	if ((step/4) > wordLen) {
		uint32_clear((uint32_t *)(PKE_A(3,step))+wordLen, (step/4)-wordLen);
	}

	pke_load_operand((uint32_t *)(PKE_B(4,step)), n1, 1);
}

/*
 * set modulus and pre-calculated mont parameters H(R^2 mod modulus) and n0'(- modulus ^(-1) mod 2^w) for hardware operation
 * parameters:
 *     modulus -------------------- input, modulus
 *     modulus_h ------------------ input, R^2 mod modulus
 *     modulus_n1 ----------------- input, - modulus ^(-1) mod 2^w, here w is 32 actually
 *     bitLen --------------------- input, bit length of modulus
 * return: PKE_SUCCESS(success), other(error)
 * caution:
 *     1. modulus must be odd
 *     2. bitLen must not be bigger than OPERAND_MAX_BIT_LEN
 */
static uint32_t pke_set_modulus_and_pre_mont(uint32_t *modulus, uint32_t *modulus_h,
	uint32_t *modulus_n1, uint32_t bitLen)
{
	uint32_t wordLen = GET_WORD_LEN(bitLen);

	if ((NULL == modulus_h) || (NULL == modulus_n1)) {
		return pke_pre_calc_mont(modulus, bitLen, NULL, NULL);
	} else {
		pke_load_pre_calc_mont(modulus_h, modulus_n1, wordLen);

		pke_load_operand((uint32_t *)(PKE_B(3,step)), modulus, wordLen);          //B3 p
		if ((step/4) > wordLen) {
			uint32_clear((uint32_t *)(PKE_B(3,step))+wordLen, (step/4)-wordLen);
		}

		return PKE_SUCCESS;
	}
}

/*
 * set exe config
 * parameters:
 *    cfg ------------------------ input, specific config value
 * return: none
 */
static void pke_set_exe_cfg(uint32_t cfg)
{
	PKE_EXE_CONF = cfg;
}

/*
 * ECCP curve point mul(random point), Q=[k]P
 * parameters:
 *     curve ---------------------- input, eccp_curve_t curve struct pointer
 *     k -------------------------- input, scalar
 *     Px ------------------------- input, x coordinate of point P
 *     Py ------------------------- input, y coordinate of point P
 *     Qx ------------------------- output, x coordinate of point Q
 *     Qy ------------------------- output, y coordinate of point Q
 * return: PKE_SUCCESS(success), other(error)
 * caution:
 *     1. please make sure k in [1,n-1], n is order of ECCP curve
 *     2. please make sure input point P is on the curve
 *     3. please make sure bit length of the curve is not bigger than ECCP_MAX_BIT_LEN
 *     4. even if the input point P is valid, the output may be infinite point, in this case
 *        it will return error.
 */
static uint32_t crypto_pke_eccp_point_mul(const eccp_curve_t *curve, uint32_t *k, uint32_t *Px, uint32_t *Py,
	uint32_t *Qx, uint32_t *Qy)
{
	uint32_t wordLen = GET_WORD_LEN(curve->eccp_p_bitLen);
	uint32_t ret;

	//set ecc_p, ecc_p_h, ecc_p_n1, etc.
	ret = pke_set_modulus_and_pre_mont(curve->eccp_p, curve->eccp_p_h, curve->eccp_p_n1, curve->eccp_p_bitLen);
	if (PKE_SUCCESS != ret) {
		return ret;
	}

	pke_load_operand((uint32_t *)(PKE_B(0,step)), Px, wordLen);                     //B0 Px
	pke_load_operand((uint32_t *)(PKE_B(1,step)), Py, wordLen);                     //B1 Py
	pke_load_operand((uint32_t *)(PKE_A(5,step)), curve->eccp_a, wordLen);          //A5 a
	pke_load_operand((uint32_t *)(PKE_A(4,step)), k, wordLen);                      //A4 k

	if ((step/4) > wordLen) {
		uint32_clear((uint32_t *)(PKE_B(0,step))+wordLen, (step/4)-wordLen);
		uint32_clear((uint32_t *)(PKE_B(1,step))+wordLen, (step/4)-wordLen);
		uint32_clear((uint32_t *)(PKE_A(5,step))+wordLen, (step/4)-wordLen);
		uint32_clear((uint32_t *)(PKE_A(4,step))+wordLen, (step/4)-wordLen);
	}

	pke_set_exe_cfg(PKE_EXE_ECCP_POINT_MUL);

	ret = pke_set_micro_code_start_wait_return_code(MICROCODE_PMUL);
	if (PKE_SUCCESS != ret) {
		return ret;
	}

	pke_read_operand((uint32_t *)(PKE_A(0,step)), Qx, wordLen);                     //A0 Qx
	if (NULL != Qy) {
		pke_read_operand((uint32_t *)(PKE_A(1,step)), Qy, wordLen);                 //A1 Qy
	}

	return PKE_SUCCESS;
}

static int crypto_pke_point_mul(struct device *dev, uint32_t *k, uint32_t *px, uint32_t *py,
	uint32_t *qx, uint32_t *qy)
{
	int ret;

	pke_mutex_acquire();
	pke_clock_enable();

	ret = crypto_pke_eccp_point_mul(&secp256r1, k, px, py, qx, qy);

	pke_clock_disable();
	pke_mutex_release();

	return ret;
}

/*
 * ECCP curve point add, Q=P1+P2
 * parameters:
 *     curve ---------------------- input, eccp_curve_t curve struct pointer
 *     P1x ------------------------ input, x coordinate of point P1
 *     P1y ------------------------ input, y coordinate of point P1
 *     P2x ------------------------ input, x coordinate of point P2
 *     P2y ------------------------ input, y coordinate of point P2
 *     Qx ------------------------- output, x coordinate of point Q=P1+P2
 *     Qy ------------------------- output, y coordinate of point Q=P1+P2
 * return: PKE_SUCCESS(success), other(error)
 * caution:
 *     1. please make sure input point P1 and P2 are both on the curve
 *     2. please make sure bit length of the curve is not bigger than ECCP_MAX_BIT_LEN
 *     3. even if the input point P1 and P2 is valid, the output may be infinite point,
 *        in this case it will return error.
 */
static uint32_t crypto_pke_eccp_point_add(const eccp_curve_t *curve, uint32_t *P1x, uint32_t *P1y,
	uint32_t *P2x, uint32_t *P2y, uint32_t *Qx, uint32_t *Qy)
{
	uint32_t wordLen = GET_WORD_LEN(curve->eccp_p_bitLen);
	uint32_t ret;

	//set ecc_p, ecc_p_h, ecc_p_n1, etc.
	ret = pke_set_modulus_and_pre_mont(curve->eccp_p, curve->eccp_p_h, curve->eccp_p_n1, curve->eccp_p_bitLen);
	if (PKE_SUCCESS != ret) {
		return ret;
	}

	//pke_pre_calc_mont() may cover A1, so load A1(P1x) here
	pke_load_operand((uint32_t *)(PKE_A(0,step)), P1x, wordLen);                    //A0 P1x
	pke_load_operand((uint32_t *)(PKE_A(1,step)), P1y, wordLen);                    //A1 P1y
	pke_load_operand((uint32_t *)(PKE_B(0,step)), P2x, wordLen);                    //B0 P2x
	pke_load_operand((uint32_t *)(PKE_B(1,step)), P2y, wordLen);                    //B1 P2y
	pke_load_operand((uint32_t *)(PKE_A(5,step)), curve->eccp_a, wordLen);          //A5 a

	if ((step/4) > wordLen) {
		uint32_clear((uint32_t *)(PKE_A(0,step))+wordLen, (step/4)-wordLen);
		uint32_clear((uint32_t *)(PKE_A(1,step))+wordLen, (step/4)-wordLen);
		uint32_clear((uint32_t *)(PKE_B(0,step))+wordLen, (step/4)-wordLen);
		uint32_clear((uint32_t *)(PKE_B(1,step))+wordLen, (step/4)-wordLen);
		uint32_clear((uint32_t *)(PKE_A(5,step))+wordLen, (step/4)-wordLen);
	}

	pke_set_exe_cfg(PKE_EXE_ECCP_POINT_ADD);

	ret = pke_set_micro_code_start_wait_return_code(MICROCODE_PADD);
	if (PKE_SUCCESS != ret) {
		return ret;
	}
	pke_read_operand((uint32_t *)(PKE_A(0,step)), Qx, wordLen);                     //A0 Qx
	if (NULL != Qy) {
		pke_read_operand((uint32_t *)(PKE_A(1,step)), Qy, wordLen);                 //A1 Qy
	}

	return PKE_SUCCESS;
}

static int crypto_pke_point_add(struct device *dev, uint32_t *p1x, uint32_t *p1y,
	uint32_t *p2x, uint32_t *p2y, uint32_t *qx, uint32_t *qy)
{
	int ret;

	pke_mutex_acquire();
	pke_clock_enable();

	ret = crypto_pke_eccp_point_add(&secp256r1, p1x, p1y, p2x, p2y, qx, qy);

	pke_clock_disable();
	pke_mutex_release();

	return ret;
}


/*
 * check whether the input point P is on ECCP curve or not
 * parameters:
 *     curve ---------------------- input, eccp_curve_t curve struct pointer
 *     Px ------------------------- input, x coordinate of point P
 *     Py ------------------------- input, y coordinate of point P
 * return: PKE_SUCCESS(success, on the curve), other(error or not on the curve)
 * caution:
 *     1. please make sure bit length of the curve is not bigger than ECCP_MAX_BIT_LEN
 *     2. after calculation, A1 and A2 will be changed!
 */
static uint32_t crypto_pke_eccp_point_verify(const eccp_curve_t *curve, uint32_t *Px, uint32_t *Py)
{
	uint32_t wordLen = GET_WORD_LEN(curve->eccp_p_bitLen);
	uint32_t ret;

	//set ecc_p, ecc_p_h, ecc_p_n1, etc.
	ret = pke_set_modulus_and_pre_mont(curve->eccp_p, curve->eccp_p_h, curve->eccp_p_n1, curve->eccp_p_bitLen);
	if (PKE_SUCCESS != ret) {
		return ret;
	}

	pke_load_operand((uint32_t *)(PKE_B(0, step)), Px, wordLen);                       //B0 Px
	pke_load_operand((uint32_t *)(PKE_B(1, step)), Py, wordLen);                       //B1 Py
	pke_load_operand((uint32_t *)(PKE_A(5, step)), (uint32_t *)secp256r1_a, wordLen);  //A5 a
	pke_load_operand((uint32_t *)(PKE_A(4, step)), (uint32_t *)secp256r1_b, wordLen);  //A4 b

	if ((step/4) > wordLen) {
		uint32_clear((uint32_t *)(PKE_B(0,step)) + wordLen, (step/4) - wordLen);
		uint32_clear((uint32_t *)(PKE_B(1,step)) + wordLen, (step/4) - wordLen);
		uint32_clear((uint32_t *)(PKE_A(5,step)) + wordLen, (step/4) - wordLen);
		uint32_clear((uint32_t *)(PKE_A(4,step)) + wordLen, (step/4) - wordLen);
	}

	pke_set_exe_cfg(PKE_EXE_ECCP_POINT_VER);

	ret = pke_set_micro_code_start_wait_return_code(MICROCODE_PVER);
	if (PKE_SUCCESS != ret) {
		return ret;
	}

	return PKE_SUCCESS;
}

static int crypto_pke_point_veirfy(struct device *dev, uint32_t *px, uint32_t *py)
{
	int ret;

	pke_mutex_acquire();
	pke_clock_enable();

	ret = crypto_pke_eccp_point_verify(&secp256r1, px, py);

	pke_clock_disable();
	pke_mutex_release();

	return ret;
}

struct pke_ops crypto_pke_ops = {
    .pke_eccp_point_mul = crypto_pke_point_mul,
    .pke_eccp_point_add = crypto_pke_point_add,
    .pke_eccp_point_verify = crypto_pke_point_veirfy,
};

static declare_driver(crypto_pke) = {
    .name = "pke",
    .probe = cyprto_pke_probe,
    .ops   = &crypto_pke_ops
};
