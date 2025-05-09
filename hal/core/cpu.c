/*
 * Copyright (c) 2020-2024 Senscomm Semiconductor Co., Ltd. All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <hal/cpu.h>
#include <hal/compiler.h>
#include <hal/types.h>

__weak void dcache_enable(void)
{
	return;
}

__weak void dcache_disable(void)
{
	return;
}

__weak void dcache_flush_all(void)
{
	return;
}

__weak void dcache_flush_range(unsigned long start, unsigned long stop)
{
	return;
}

__weak void dcache_invalidate_range(unsigned long start, unsigned long stop)
{
	return;
}

__weak void dcache_invalidate_all(void)
{
	return;
}

__weak void invalidate_icache_all(void)
{
	return;
}
