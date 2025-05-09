#ifndef __SPI_DEMO__
#define __SPI_DEMO__

int spi_master_init(void);
int spi_master_config(enum scm_spi_data_io_format io_format, scm_spi_notify notify);
int spi_master_tx(uint32_t count, uint32_t timeout);
int spi_master_rx(uint32_t timeout);
void spi_master_recv_msg_print(void);

int spi_slave_init(void);
int spi_slave_config(enum scm_spi_data_io_format io_format, scm_spi_notify notify);
int spi_slave_tx(uint32_t count);
int spi_slave_rx(void);
void spi_slave_recv_msg_print(int len);

#endif //__SPI_DEMO__
