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

#ifndef _NET80211_IEEE80211_CRYPTO_RIJNDAEL_H_
#define _NET80211_IEEE80211_CRYPTO_RIJNDAEL_H_

#include <crypto/rijndael-alg-fst.h>

#define RIJNDAEL_MAXKC	(256/32)
#define RIJNDAEL_MAXKB	(256/8)
#define RIJNDAEL_MAXNR	14

typedef struct {
	int	decrypt;
	int	Nr;		/* key-length-dependent number of rounds */
	u32 ek[4 * (RIJNDAEL_MAXNR + 1)];	/* encrypt key schedule */
	u32 dk[4 * (RIJNDAEL_MAXNR + 1)];	/* decrypt key schedule */
} rijndael_ctx;

static __inline void
rijndael_set_key(rijndael_ctx *ctx, const u_char *key, int bits)
{
	ctx->Nr = rijndaelKeySetupEnc(ctx->ek, key, bits);
	rijndaelKeySetupDec(ctx->dk, key, bits);
}

static __inline void
rijndael_decrypt(const rijndael_ctx *ctx, const u_char *src, u_char *dst)
{

	rijndaelDecrypt(ctx->dk, ctx->Nr, src, dst);
}

static __inline void
rijndael_encrypt(const rijndael_ctx *ctx, const u_char *src, u_char *dst)
{
	rijndaelEncrypt(ctx->ek, ctx->Nr, src, dst);
}

#endif /* _NET80211_IEEE80211_CRYPTO_RIJNDAEL_H_ */
