/********************************************************************************************************
 * @file	aes_reg.h
 *
 * @brief	This is the header file for B91
 *
 * @author	Driver Group
 * @date	2019
 *
 * @par     Copyright (c) 2019, Telink Semiconductor (Shanghai) Co., Ltd. ("TELINK")
 *          All rights reserved.
 *
 *          Redistribution and use in source and binary forms, with or without
 *          modification, are permitted provided that the following conditions are met:
 *
 *              1. Redistributions of source code must retain the above copyright
 *              notice, this list of conditions and the following disclaimer.
 *
 *              2. Unless for usage inside a TELINK integrated circuit, redistributions
 *              in binary form must reproduce the above copyright notice, this list of
 *              conditions and the following disclaimer in the documentation and/or other
 *              materials provided with the distribution.
 *
 *              3. Neither the name of TELINK, nor the names of its contributors may be
 *              used to endorse or promote products derived from this software without
 *              specific prior written permission.
 *
 *              4. This software, with or without modification, must only be used with a
 *              TELINK integrated circuit. All other usages are subject to written permission
 *              from TELINK and different commercial license may apply.
 *
 *              5. Licensee shall be solely responsible for any claim to the extent arising out of or
 *              relating to such deletion(s), modification(s) or alteration(s).
 *
 *          THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *          ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *          WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *          DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER BE LIABLE FOR ANY
 *          DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *          (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *          LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *          ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *          (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *          SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *******************************************************************************************************/
#ifndef _AES_REG_H_
#define _AES_REG_H_

#include "../sys.h"


#define	aes_base_addr				(0x160000)

#define	reg_aes_irq_mask			REG_ADDR32(aes_base_addr + 0x0c)

#define reg_aes_irq_status			REG_ADDR32(aes_base_addr + 0x10)

#define reg_aes_clr_irq_status		REG_ADDR32(aes_base_addr + 0x18)

#define reg_aes_mode     			REG_ADDR32(aes_base_addr + 0xb0)
enum{
	FLD_AES_START	=	BIT(0),
	FLD_AES_MODE	=	BIT(1),   /**< 0-ciher  1-deciher */
};

#define reg_aes_key(v)     			REG_ADDR32(aes_base_addr + 0xb4 + (v*4))

#define reg_aes_ptr     			REG_ADDR32(aes_base_addr + 0xc4)

#define reg_aes_RPACE_CNT     	REG_ADDR32(aes_base_addr + 0x288)
enum{
	FLD_AES_PRAND				=	BIT_RNG(0, 23),
	FLD_AES_IRK_NUM				=	BIT_RNG(24, 27),
	FLD_AES_GEN_RES				=	BIT(29),   /**< W1C */
	FLD_AES_RPACE_START			=	BIT(30), /**< 0-idle 1-running 2-finished */
	FLD_AES_RPACE_EN			=	BIT(31),  /**< 0-unmatched 1-matched */
};

#define reg_aes_hash_status     	REG_ADDR32(aes_base_addr + 0x28c)
enum{
	FLD_AES_HASH_STA			=	BIT_RNG(0, 23),
	FLD_AES_IRK_CNT				=	BIT_RNG(24, 27),
	FLD_AES_RPACE_STA_CLR		=	BIT(28),   /**< W1C */
	FLD_AES_RPACE_STA			=	BIT_RNG(29, 30), /**< 0-idle 1-running 2-finished */
	FLD_AES_HASH_MATCH			=	BIT(31),  /**< 0-unmatched 1-matched */
};

#define reg_aes_irk_ptr     	REG_ADDR32(aes_base_addr + 0x290)








#define reg_embase_addr     		REG_ADDR32(0x170304)


/**
 *  @brief  Define AES IRQ
 */
typedef enum{
	FLD_CRYPT_IRQ		= BIT(7),
}aes_irq_e;

#endif /* _AES_REG_H_ */
