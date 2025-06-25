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

#ifndef __SCM_FS_H__
#define __SCM_FS_H__

#ifdef __cplusplus
extern "C" {
#endif

int scm_fs_load(const char *filename);
int scm_fs_write(const char *filename, const char *buf, int len);
int scm_fs_read(const char *filename, char *buf, int len);
int scm_fs_rm(const char *filename);
long int scm_fs_size(const char *filename);
int scm_fs_unmount(const char *pathname);
int scm_fs_format(const char *pathname);

int scm_fs_read_config_value(const char *ns, const char *key, char *buf, int len);
int scm_fs_write_config_value(const char *ns, const char *key, const char *buf, int len);
int scm_fs_remove_config_value(const char *ns, const char *key);
int scm_fs_exists_config_value(const char *ns, const char *key);

#ifdef __cplusplus
}
#endif

#endif //__SCM_FS_H__
