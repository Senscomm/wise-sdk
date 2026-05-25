#ifndef _IOTALINK_UART_H
#define _IOTALINK_UART_H


// 状态枚举（按表格定义）
typedef enum {
    STATE_NO_PEOPLE    = 0,    // 无人
    STATE_HAS_PEOPLE   = 1,    // 有人
    STATE_WEAK_SIGNAL  = 2     // 生命体征信号弱
} ExistStateType;

typedef enum {
    MOVE_NO_PEOPLE     = 0,    // 无人
    MOVE_STATIC        = 1,    // 静息
    MOVE_QUIET         = 2,    // 安静
    MOVE_ACTION        = 3,    // 动作
    MOVE_KEEP_ACTION   = 4     // 持续动作
} MoveStateType;

typedef enum {
    NORMAL             = 0,    // 正常
    NO_SIGNAL          = 5,    // 未检测到生命体征
    RESP_HIGH          = 12,   // 呼吸过高
    RESP_LOW           = 13    // 呼吸过低
} AbnormalStateType;

typedef enum {
    SLEEP_WAKE         = 1,    // 清醒
    SLEEP_REM          = 2,    // 快速眼动
    SLEEP_LIGHT        = 3,    // 浅睡
    SLEEP_DEEP         = 4,    // 深睡
    SLEEP_OUT_BED      = 7     // 离床
} SleepStateType;

// 0x30 32字节完整数据结构体
typedef struct {
    uint8_t dev_id[6];         // Byte[0-5] 产品唯一ID号
    uint8_t heart;             // Byte[6] 心率 次/分 0~150
    uint8_t resp;              // Byte[7] 呼吸率 次/分 0~40
    uint8_t exist_state;       // Byte[8] 人体存在状态
    uint8_t move_state;        // Byte[9] 人体活动状态
    uint16_t distance_cm;      // Byte[10-11] 目标距离 单位cm
    uint8_t abnormal_code;     // Byte[12] 体征异常状态
    float signal_db;           // Byte[13-16] 信号强度 4字节浮点 -100~100 dB
    uint16_t bed_time_min;     // Byte[17-18] 在床时长 分钟
    uint16_t out_time_min;     // Byte[19-20] 离床时长 分钟
    uint8_t reserve1;          // Byte[21] 预留
    uint8_t sleep_state;       // Byte[22] 睡眠状态
    uint8_t reserve2;          // Byte[23] 预留
    uint8_t time_hour;         // Byte[24] 系统时间 时
    uint8_t time_min;          // Byte[25] 系统时间 分
    uint8_t time_sec;          // Byte[26] 系统时间 秒
    uint8_t reserve3[5];       // Byte[27-31] 预留扩展
} RadarRTData_t;

void hex_dump(char *title, uint8_t *buf, int len);


void wlt_uart_init(void);


void radar_set_report_1s(void);

int iotalink_uart2_send_data( uint8_t * data, int len);

/**
 * @brief 串口2控制音乐模组
 */
void wlt_control_music(uint8_t   volume , uint8_t  music_id ,uint8_t  play_ctrl);


#endif // iotalink_uart.h

