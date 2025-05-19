/*
 * Copyright 2023-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __SCM_PTA_H__
#define __SCM_PTA_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * PTA force mode
 */
enum scm_pta_force_mode {
	SCM_PTA_FORCE_MODE_OFF,
	SCM_PTA_FORCE_MODE_WIFI,
	SCM_PTA_FORCE_MODE_BLE,
};

/**
 * @brief Get current PTA force mode value
 *
 * @param[out] force_mode current PTA force mode
 */
int scm_pta_get_force_mode(enum scm_pta_force_mode *force_mode);

/**
 * @brief Set PTA force mode value
 *
 * @param[in] force_mode PTA force mode
 */
int scm_pta_set_force_mode(enum scm_pta_force_mode force_mode);

#ifdef __cplusplus
}
#endif

#endif //__SCM_PTA_H__
