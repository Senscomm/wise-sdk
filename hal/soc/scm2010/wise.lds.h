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
#ifndef __WISE_LDS_H__
#define __WISE_LDS_H__

#define ARRAY(x)	    \
	__ ##x##_start = .; \
	KEEP(*(SORT(.x*))); \
	__##x##_end = .;

#define EXPORT_SECTION_INFO(name, section) 			\
	__##name##_start = ADDR(section); 			\
	__##name##_end  = ADDR(section) + SIZEOF(section); 	\
	__##name##_size = SIZEOF(section);			\
	__##name##_lma = LOADADDR(section)

#endif /* __WISE_LDS_H__ */
