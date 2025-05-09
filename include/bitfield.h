/*
 * bitfield.h - bitfield manipulation
 *
 * Copyright (c) 2018-2024 Senscomm Semiconductor Co., Ltd. All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#define bitmask(l, h) (((1 << ((h) - (l) + 1)) - 1) << (l))
#define bitmask64(l, h) ((u64)((u64)(1 << ((h) - (l) + 1)) - 1) << (l))
#define bf_get(v, l, h) ((v & bitmask(l, h)) >> (l))
#define bf_set(v, l, h, f) \
	{ v = (((v) & ~bitmask(l, h)) | (((f) << (l)) & bitmask(l, h))); }

#define bf(l, h, name) u32 name:((h) - (l) + 1)
#define bf64(l, h, name) u64 name:((h) - (l) + 1)


#define SM(v, field) (((v) << field##_S) & field)
#define MS(v, field) (((v) & field) >> field##_S)
