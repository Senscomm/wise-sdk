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

#ifndef __I2C_EEPROM_H__

/* Total memory size */
#define EEPROM_MEMORY_SIZE		320

/* I2C slave address of the EEPROM */
#define EEPROM_DEVICE_ADDR      0x50

/* Memory address length of the EEPROM */
#define EEPROM_ADDR_LEN         2


int eeprom_master_init(void);
int eeprom_clear(void);
int eeprom_write(uint16_t addr, uint8_t *buf, uint8_t len);
int eeprom_read(uint16_t addr, uint8_t *buf, uint32_t len);
int eeprom_set_addr(uint16_t addr);
int eeprom_readon(uint8_t *buf, uint32_t len);

int eeprom_slave_init(void);


#endif /* __I2C_EEPROM_H__ */
