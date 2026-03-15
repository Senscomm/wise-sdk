
#include <hal/kernel.h>
#include <hal/console.h>

#include <cmsis_os.h>



#include "scm_uart.h"

#define MS_TO_TICKS(ms) ((uint32_t)(((uint32_t)(ms) * osKernelGetTickFreq()) / (uint32_t)1000))


#define TEST_TIMEOUT	1000

#define TEST_MSG_SIZE	5


static struct scm_uart_cfg uart_tx_cfg = {
	.baudrate = SCM_UART_BDR_115200,
	.data_bits = SCM_UART_DATA_BITS_8,
	.parity = SCM_UART_NO_PARITY,
	.stop_bits = SCM_UART_STOP_BIT_1,
	.dma_en = 0,
};

static struct scm_uart_cfg uart_rx_cfg = {
	.baudrate = SCM_UART_BDR_115200,
	.data_bits = SCM_UART_DATA_BITS_8,
	.parity = SCM_UART_NO_PARITY,
	.stop_bits = SCM_UART_STOP_BIT_1,
	.dma_en = 0,
};

static void hex_dump(char *title, uint8_t *buf, int len)
{
	int i;

	printf("%s\n", title);

	for (i = 0; i < len; i++) {
		if (i != 0 && (i % 16) == 0) {
			printf("\n%02x ", buf[i]);
		} else {
			printf("%02x ", buf[i]);
		}
	}

	printf("\n");
}

static uint8_t rx_buf[1024];

void iotalink_uart_process(void*argv)
{
	int ret;

	while (1)
    {
    
    	//const char test_msg[] = "UART Sanity Test";		
		//scm_uart_tx(SCM_UART_IDX_1, test_msg, TEST_MSG_SIZE, TEST_TIMEOUT);

		//printf("iotalink_uart_process \n");
		memset(rx_buf, 0, TEST_MSG_SIZE + 1);
		//scm_uart_rx
		ret = scm_uart_rx_async(SCM_UART_IDX_1, rx_buf, TEST_MSG_SIZE, NULL, NULL);
		if (ret==0)
		{
			 scm_uart_tx(SCM_UART_IDX_1, rx_buf, TEST_MSG_SIZE, TEST_TIMEOUT);
		}
	
		osDelay(MS_TO_TICKS(20));

    }

}


void wlt_uart_init(void)
{

#ifdef CONFIG_USE_UART1
		/* UART1 */
		///pinmap( 0, "atcuart.1", "rxd", 0),
		//	pinmap( 1, "atcuart.1", "txd", 0),		

	    uart_tx_cfg.dma_en = 0;
		uart_rx_cfg.dma_en = 0;		
		int ret = scm_uart_init(SCM_UART_IDX_1, &uart_tx_cfg);
		if (ret)
		{
			printf("---------->UART%d initialize failed %x\n", SCM_UART_IDX_1, ret);
			return -1;
		}
		
		printf(" ---- uart_init ----- \n");	
#endif

osThreadAttr_t attr = {
	.name		= "uart_task",
	.stack_size = 1024,
	.priority	= osPriorityNormal,    //osPriorityNormal		 = 24,osPriorityRealtime,//osPriorityLowcon
};
/* run the demo in a new thread to allow further CLI */
osThreadNew(iotalink_uart_process, NULL, &attr);



/*
	tx_buf = dma_malloc(TEST_MSG_SIZE);
	rx_buf = dma_malloc(TEST_MSG_SIZE + 1);

	if (uart_test(1, SCM_UART_IDX_1, SCM_UART_IDX_2) < 0) {
		assert(0);
	}

	if (uart_test(1, SCM_UART_IDX_2, SCM_UART_IDX_1) < 0) {
		assert(0);
	}

	if (uart_test(0, SCM_UART_IDX_1, SCM_UART_IDX_2) < 0) {
		assert(0);
	}

	if (uart_test(0, SCM_UART_IDX_2, SCM_UART_IDX_1) < 0) {
		assert(0);
	}

	dma_free(tx_buf);
	dma_free(rx_buf);
*/

}




