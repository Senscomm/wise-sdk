#include <hal/kernel.h>
#include <hal/console.h>
#include <cmsis_os.h>
#include <string.h>
#include <stdint.h>

#include "iotalink_uart.h"
#include "scm_uart.h"

#define MS_TO_TICKS(ms) ((uint32_t)(((uint32_t)(ms) * osKernelGetTickFreq()) / (uint32_t)1000))
#define TEST_TIMEOUT	300

// ====================== 雷达协议核心定义（补全缺失部分，修正解析） ======================
#define UART_RX_BUF_SIZE        512     // 接收缓存
#define MAX_FRAME_LEN           256     // 最大帧长
#define SYNC_HEAD_H             0x55    // 同步头
#define SYNC_HEAD_L             0xAA    
#define CMD_RT_DATA             0x30    // 实时数据命令字
#define RT_DATA_LEN             32      // 0x30固定数据长度

static RadarRTData_t s_radar_data = {0};


static uint8_t  s_uart_rx_buf[UART_RX_BUF_SIZE] = {0};
static uint16_t s_uart_rx_len = 0;


// -------------------------- 串口配置 --------------------------
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

// -------------------------- HEX打印工具（排查专用） --------------------------
void hex_dump(char *title, uint8_t *buf, int len)
{
	int i;
	printf("%s [%d字节]:\n", title, len);
	for (i = 0; i < len; i++) {
		printf("%02x ", buf[i]);
		if ((i+1) % 16 == 0) printf("\n");
	}
	printf("\n");
}

// -------------------------- 大端转16位 --------------------------
static uint16_t big2short(const uint8_t *buf)
{
    return (uint16_t)((buf[0] << 8) | buf[1]);
}
// -------------------------- 串口发送 --------------------------
int iotalink_send_data( uint8_t * data, int len)
{
    return scm_uart_tx(SCM_UART_IDX_1, data, len, TEST_TIMEOUT);
}
int iotalink_uart2_send_data( uint8_t * data, int len)
{
	return scm_uart_tx(SCM_UART_IDX_2, data, len, TEST_TIMEOUT);
}

/**
 * @brief 串口2控制音乐模组
 */
void wlt_control_music(uint8_t          volume , uint8_t  music_id ,uint8_t  play_ctrl)
{
	
	u8 music_cmd[9]={0x1D,	0x18,0x01, 0, 0, 1,	0xff,	0xff,	0xD1};
	// 	// 音量 0~10
	music_cmd[3]=volume;
	// 音乐编号 0~9
	music_cmd[4]=music_id	;	
	// 播放控制：0=暂停 1=播放	
	music_cmd[5]=play_ctrl; 		

	iotalink_uart2_send_data(music_cmd ,9);

	hex_dump("wlt_control_music",music_cmd,9);

}
// -------------------------- 累加和计算（收发通用） --------------------------
static uint16_t check_sum_calc(const uint8_t *frame, uint16_t frame_len)
{
    uint16_t sum = 0;
    for(uint8_t i=0; i<frame_len-2; i++)
    {
        sum += frame[i];
    }
    return sum;
}

void radar_send_cmd(uint8_t cmd, uint8_t *data, uint8_t data_len)
{
    uint8_t tx_buf[512] = {0};
    uint16_t tx_index = 0;
    uint16_t check_sum = 0;

    // 拼接帧头
    tx_buf[tx_index++] = SYNC_HEAD_H;
    tx_buf[tx_index++] = SYNC_HEAD_L;
    tx_buf[tx_index++] = cmd;
    tx_buf[tx_index++] = data_len;

    // 先判断指针非空，再判断长度
    if(data != NULL && data_len > 0)
    {
        memcpy(&tx_buf[tx_index], data, data_len);
        tx_index += data_len;
    }

    // 协议标准：tx_index = 同步头+命令+长度+数据，正好是累加区间
    check_sum = check_sum_calc(tx_buf, tx_index+2);

    // 大端写入16位累加和
    tx_buf[tx_index++] = (check_sum >> 8) & 0xFF;
    tx_buf[tx_index++] = check_sum & 0xFF;

    hex_dump("send ", tx_buf, tx_index);
    iotalink_send_data(tx_buf, tx_index);
}


// ====================== 核心：0x30 官方32字节协议解析 ======================
void radar_frame_parse(uint8_t cmd, const uint8_t *data, uint16_t data_len)
{
	
    if(cmd != CMD_RT_DATA) return;
    // 协议固定32字节，0x20
    if(data_len != RT_DATA_LEN)
	{
        printf(" 0x30长度错误！协议要求32，实际：%d\n", data_len);
        return;
    }

    // ====================== 严格按照表格Byte下标逐字段解析 ======================
    memcpy(s_radar_data.dev_id, &data[0], 6);        // Byte[0-5] 设备ID
    s_radar_data.heart          = data[6];           // Byte[6] 心率
    s_radar_data.resp            = data[7];           // Byte[7] 呼吸
    s_radar_data.exist_state     = data[8];           // Byte[8] 有人/无人
    s_radar_data.move_state      = data[9];           // Byte[9] 体动状态
    s_radar_data.distance_cm     = big2short(&data[10]);// Byte[10-11] 距离cm
    s_radar_data.abnormal_code    = data[12];         // Byte[12] 异常码
    // 4字节IEEE754浮点数 大端模式
 //   s_radar_data.signal_db       = bytes2float(&data[13]);// Byte[13-16] 信号dB
    s_radar_data.bed_time_min    = big2short(&data[17]);// Byte[17-18] 在床时长
    s_radar_data.out_time_min    = big2short(&data[19]);// Byte[19-20] 离床时长
    s_radar_data.reserve1        = data[21];
    s_radar_data.sleep_state     = data[22];         // Byte[22] 睡眠状态
    s_radar_data.reserve2        = data[23];
    s_radar_data.time_hour       = data[24];         // 时
    s_radar_data.time_min        = data[25];         // 分
    s_radar_data.time_sec         = data[26];         // 秒

    // 解析结果可视化打印
    printf("=========================================\n");
    printf(" 0x30 32字节数据解析成功\n");
    printf("设备ID: %02X%02X%02X%02X%02X%02X\n",
           s_radar_data.dev_id[0],s_radar_data.dev_id[1],s_radar_data.dev_id[2],
           s_radar_data.dev_id[3],s_radar_data.dev_id[4],s_radar_data.dev_id[5]);
    printf("心率：%d 次/分 | 呼吸：%d 次/分\n", s_radar_data.heart, s_radar_data.resp);
    printf("距离：%.2f 米 | 信号强度：%.2f dB\n", 
           s_radar_data.distance_cm / 100.0f, s_radar_data.signal_db);
    printf("存在状态：%d | 体动状态：%d\n", s_radar_data.exist_state, s_radar_data.move_state);
    printf("在床时长：%d分钟 | 离床时长：%d分钟\n", s_radar_data.bed_time_min, s_radar_data.out_time_min);
    printf("睡眠状态：%d | 设备时间：%02d:%02d:%02d\n",
           s_radar_data.sleep_state, s_radar_data.time_hour, s_radar_data.time_min, s_radar_data.time_sec);
    printf("异常码：%d\n", s_radar_data.abnormal_code);
    printf("=========================================\n");
}
extern void wlt_radar_notify( u8 * msg, u8 len);

// --------------------------  帧解析  --------------------------
static int8_t frame_decode(const uint8_t *frame, uint8_t frame_len)
{
    if(frame_len < 6) 
	{
        printf(" 帧长度过短：%d\n", frame_len);
        return -1;
    }

    uint8_t cmd      = frame[2];
    uint8_t data_len = frame[3];
    uint16_t rx_sum  = (frame[frame_len-2] << 8) | frame[frame_len-1];
    uint16_t calc_sum = check_sum_calc(frame, frame_len);

    // 调试打印
//    printf("?? 解析帧：命令字=0x%02X, 数据长度=%d, 帧总长度=%d\n", cmd, data_len, frame_len);
//    hex_dump("帧原始数据", (uint8_t*)frame, frame_len);

    if((6 + data_len) != frame_len || data_len > 250) {
        printf(" 帧长度不匹配\n");
        return -2;
    }

    if(calc_sum != rx_sum) {
        printf(" 校验错误！计算：0x%04X，接收：0x%04X\n", calc_sum, rx_sum);
        return -3;
    }

    // 校验成功
//    printf("? 校验通过\n");

	switch(cmd)
	{
		case 0x0://设备信息
			
		
			radar_set_report_1s();

			break;
		case 0x30:
			
			wlt_radar_notify(frame,frame_len);
			
		//	radar_frame_parse(cmd, &frame[4], data_len);

			
			//u8 spo2[]={0x1D ,0x1b, 0x02, 95, 0xff ,0xff, 0xff ,0xff ,0xD1};
		  //  spo2[3] = rand()%5+90;
			
			//wlt_radar_notify(spo2,9);
			
			break;
		case 0x62://报告生成成功0
		case 0x63://睡眠报告

			wlt_radar_notify(frame,frame_len);
		
			break;
		default ://都透传上报 03(时间同步)6263..睡眠报告
			
			wlt_radar_notify(frame,frame_len);
			
			break;

	}

    return 0;
}

// -------------------------- 接收回调（处理粘包分包） --------------------------
void iotalink_rx_callbak(uint8_t *rx_buf, uint16_t len)
{
    // 原始数据打印
//    hex_dump("原始接收数据", rx_buf, len);

    if((s_uart_rx_len + len) > UART_RX_BUF_SIZE)
    {
        memset(s_uart_rx_buf, 0, UART_RX_BUF_SIZE);
        s_uart_rx_len = 0;
        return;
    }

    memcpy(&s_uart_rx_buf[s_uart_rx_len], rx_buf, len);
    s_uart_rx_len += len;

    while(s_uart_rx_len >= 6)
    {
        uint16_t frame_offset = 0xFFFF;

        // 查找帧头
        for(uint16_t i=0; i<s_uart_rx_len-1; i++)
        {
            if(s_uart_rx_buf[i] == SYNC_HEAD_H && s_uart_rx_buf[i+1] == SYNC_HEAD_L)
            {
                frame_offset = i;
                break;
            }
        }

        if(frame_offset == 0xFFFF)
        {
            s_uart_rx_len = 0;
            memset(s_uart_rx_buf, 0, UART_RX_BUF_SIZE);
            return;
        }

        if(frame_offset > 0)
        {
            s_uart_rx_len -= frame_offset;
            memmove(s_uart_rx_buf, &s_uart_rx_buf[frame_offset], s_uart_rx_len);
            continue;
        }

        uint8_t data_len = s_uart_rx_buf[3];
        uint8_t frame_total_len = 6 + data_len;

        if(frame_total_len > MAX_FRAME_LEN)
        {
            memmove(s_uart_rx_buf, &s_uart_rx_buf[2], s_uart_rx_len-2);
            s_uart_rx_len -= 2;
            continue;
        }

        if(s_uart_rx_len < frame_total_len)
        {
            return;
        }

        // 解析帧
        frame_decode(s_uart_rx_buf, frame_total_len);

	

        // 移除已解析数据
        memmove(s_uart_rx_buf, &s_uart_rx_buf[frame_total_len], s_uart_rx_len - frame_total_len);
        s_uart_rx_len -= frame_total_len;
    }
}

// -------------------------- 雷达配置指令 --------------------------
void radar_set_report_1s(void)
{
    uint8_t data[] = {0x1, 0x1};
    radar_send_cmd(0xF9, data, 2);
}

void radar_set_report_always(void)
{
    uint8_t data[] = {0x00};
    radar_send_cmd(0xFA, data, 1);
}




//在线11:59
void radar_send_time_online(void)
{
	uint8_t data[]={0x55,0xaa,0x03,0x09,0x01,0x1a,0x04,0x16,0x0b,0x3b,0x29,0x03,0x5a,0x02,0x0c};
    iotalink_send_data(data, 15);
}

// -------------------------- 雷达初始化 --------------------------
void radar_start(void)
{
    printf("雷达初始化开始...\n");
	osDelay(MS_TO_TICKS(200));
    radar_set_report_1s();
    radar_set_report_always();
    radar_send_time_online();
    printf("雷达初始化完成，等待0x30数据...\n");
}

// -------------------------- 串口任务（修复逻辑bug） --------------------------
void iotalink_uart_process(void*argv)
{
	int ret;
	int len;
	static uint8_t rx_buf[1024];
	while (1)
    {
		int len =512;
		ret = scm_uart_rx(SCM_UART_IDX_1, rx_buf, &len, TEST_TIMEOUT );
		if (ret)
		{
			if (ret == WISE_ERR_TIMEOUT)
			{
				if(len>0)
				{
					printf("uart rx: %d bytes received.\n", len);
					//scm_uart_tx(SCM_UART_IDX_1, rx_buf, len , TEST_TIMEOUT);

					iotalink_rx_callbak(rx_buf,len);
				}

			
			} 
			else
			{
				printf("uart rx: error %x\n", ret);
				goto exit;
			}
		}

		if (len == 512)
		{
			printf("WARN: uart rx buffer full, reset FIFO but data received might get lost\n");
			scm_uart_reset(SCM_UART_IDX_1);
		}
		osDelay(MS_TO_TICKS(20));

    }
exit:
	scm_uart_deinit(SCM_UART_IDX_1);
}

// -------------------------- UART初始化 --------------------------
void wlt_uart_init(void)
{
#ifdef CONFIG_USE_UART1
	int ret = scm_uart_init(SCM_UART_IDX_1, &uart_tx_cfg);
	if (ret)
	{
		printf("UART1初始化失败 %x\n", ret);
		return;
	}
	printf("UART1初始化成功\n");	
#endif

#ifdef CONFIG_USE_UART2
		 ret = scm_uart_init(SCM_UART_IDX_2, &uart_tx_cfg);
		if (ret)
		{
			printf("UART2初始化失败 %x\n", ret);
			return;
		}
		printf("UART2初始化成功\n");	
#endif

	iotalink_uart2_send_data("uart_test",10);

	osThreadAttr_t attr = {
		.name		= "uart_task",
		.stack_size = 1024,
		.priority	= osPriorityNormal,
	};
	osThreadNew(iotalink_uart_process, NULL, &attr);

	// 启动雷达配置
	radar_start();
}
