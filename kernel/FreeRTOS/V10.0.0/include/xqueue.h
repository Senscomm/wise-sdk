/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * xqueue.h - FreeRTOS queue extension
 */

#ifndef __XQUEUE_H__
#define __XQUEUE_H__

#include <FreeRTOS/queue.h>

UBaseType_t uxQueueSpacesAvailableFromISR(const QueueHandle_t xQueue);

BaseType_t queue_insert_set(QueueSetMemberHandle_t xQueueOrSemaphore,
			    QueueSetHandle_t xQueueSet);

BaseType_t queue_remove_set(QueueSetMemberHandle_t xQueueOrSemaphore,
			    QueueSetHandle_t xQueueSet);
#endif /* __XQUEUE_H__ */
