
#include "iotalink.h"
#include "scm_i2c.h"

/**********************************************************************
 * 文件名：iotalink_max30102.c
 * 功能：MAX30102 血氧心率传感器完整驱动（硬件I2C + INT中断 + 算法）
 * 平台：基于 scm_i2c.h 硬件I2C接口
 * 输出：心率HR(次/分)、血氧SpO?(%)
 **********************************************************************/

//=============================================================================
//======================= 配置参数 ===========================
//=============================================================================
#define MAX30102_I2C_IDX             SCM_I2C_IDX_1    // I2C端口 19 20
#define MAX30102_INT_GPIO_PIN        0                // INT脚编号
#define MAX30102_ADDR_7BIT           0x57             // 7位地址(0xAE>>1) 

// 采样配置
#define MAX30102_SR_100HZ             0x01
#define MAX30102_LED_PW_18BIT         0x03
#define MAX30102_LED_CURRENT          0x2C

// 算法参数
#define SPO2_SAMPLE_CNT               100
#define HR_BUF_SIZE                   10

//中断引脚电平
u8	MAX30102_INT  =1;

//=============================================================================
//=======================  MAX30102 寄存器定义 ============================
//=============================================================================
#define MAX30102_PART_ID              0x15

#define MAX30102_INT_STATUS1          0x00
#define MAX30102_INT_ENABLE1          0x02
#define MAX30102_FIFO_WR_PTR           0x04
#define MAX30102_FIFO_RD_PTR           0x06
#define MAX30102_FIFO_DATA             0x07
#define MAX30102_FIFO_CONFIG           0x08
#define MAX30102_MODE_CONFIG           0x09
#define MAX30102_SPO2_CONFIG           0x0A
#define MAX30102_LED1_PA               0x0C
#define MAX30102_LED2_PA               0x0D
#define MAX30102_PART_ID_REG           0xFF

//=============================================================================
//======================== 全局变量 =======================================
//=============================================================================
// 中断
volatile uint8_t max30102_int_flag = 0;

//红外光通道原始 18 位 ADC 值 红光通道原始 18 位 ADC 值  
uint32_t max30102_ir = 0, max30102_red = 0;
// 算法
static int32_t ir_buf[SPO2_SAMPLE_CNT];
static int32_t red_buf[SPO2_SAMPLE_CNT];
static uint8_t  sample_idx = 0;
static int32_t  ir_dc = 0, red_dc = 0;
static uint32_t peak_t[HR_BUF_SIZE];
static uint8_t  peak_idx = 0;
static uint32_t last_peak = 0;
static float    hr_val = 0, spo2_val = 0;

//=============================================================================
//========================  I2C 基础读写 ===================================
//=============================================================================

#if 1
int max30102_wr(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return scm_i2c_master_tx(MAX30102_I2C_IDX, MAX30102_ADDR_7BIT, buf, 2, 500);
}

int max30102_rd(uint8_t reg, uint8_t *val)
{
    return scm_i2c_master_tx_rx(MAX30102_I2C_IDX, MAX30102_ADDR_7BIT,&reg, 1, val, 1, 500);
}

int max30102_rd_fifo(uint8_t *buf)
{
    uint8_t reg = MAX30102_FIFO_DATA;
    return scm_i2c_master_tx_rx(MAX30102_I2C_IDX, MAX30102_ADDR_7BIT,&reg, 1, buf, 6, 500);
}
#else 
int max30102_wr(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    int ret = scm_i2c_master_tx(MAX30102_I2C_IDX, MAX30102_ADDR_7BIT, buf, 2, 500);
    // 0: 成功 | -1: 超时 | -2: NACK无应答 | -3: 总线错误 
    printf("[MAX30102 WR] Reg:0x%02X, Val:0x%02X, Ret:%d\n", reg, val, ret); 
    return ret;
}

int max30102_rd(uint8_t reg, uint8_t *val)
{
    int ret = scm_i2c_master_tx_rx(MAX30102_I2C_IDX, MAX30102_ADDR_7BIT,&reg, 1, val, 1, 500);

    if (ret == 0) {
        printf("[MAX30102 RD] Reg:0x%02X, Read Val:0x%02X, Ret:%d\n", reg, *val, ret);
    } else {
        printf("[MAX30102 RD] Reg:0x%02X, Read FAILED, Ret:%d\n", reg, ret);
    }
    
    return ret;
}

int max30102_rd_fifo(uint8_t *buf)
{
    uint8_t reg = MAX30102_FIFO_DATA;
    int ret = scm_i2c_master_tx_rx(MAX30102_I2C_IDX, MAX30102_ADDR_7BIT,&reg, 1, buf, 6, 500);
    
    if (ret == 0) {
        printf("[MAX30102 RD_FIFO] Ret:%d, Data:0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n", 
               ret, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
    } else {
        printf("[MAX30102 RD_FIFO] Read FAILED, Ret:%d\n", ret);
    }
    
    return ret;
}

#endif


//=============================================================================
//======================== （中断）数据处理 ==================================
//=============================================================================
void max30102_process(void)
{
    uint8_t buf[6], sta;
   // if (!max30102_int_flag) return;
	while(MAX30102_INT==1)
	{
		scm_gpio_read(5,&MAX30102_INT);
	} 

    max30102_int_flag = 0;
    max30102_rd(MAX30102_INT_STATUS1, &sta);
    max30102_rd_fifo(buf);

    max30102_red = ((uint32_t)buf[0] << 16) | (buf[1] << 8) | buf[2];
    max30102_ir  = ((uint32_t)buf[3] << 16) | (buf[4] << 8) | buf[5];
    max30102_red &= 0x3FFFF;
    max30102_ir  &= 0x3FFFF;
}

//=============================================================================
//======================== 心率血氧算法 ==================================
//=============================================================================
void dc_remove(int32_t *ir, int32_t *rd)
{
    ir_dc  = ir_dc - (ir_dc >> 5) + *ir;
    *ir    = ir_dc >> 5;
    red_dc = red_dc - (red_dc >> 5) + *rd;
    *rd    = red_dc >> 5;
}

void calc_spo2(void)
{
    float ir_ac = 0, rd_ac = 0;
    float ir_sum = 0, rd_sum = 0;

    for (int i = 1; i < SPO2_SAMPLE_CNT; i++)
	{
        ir_ac = ir_buf[i] - ir_buf[i-1];
        rd_ac = red_buf[i] - red_buf[i-1];
        ir_sum += ir_ac * ir_ac;
        rd_sum += rd_ac * rd_ac;
    }

    float R = (rd_sum / (red_dc + 1)) / (ir_sum / (ir_dc + 1));
    spo2_val = 110.0f - 25.0f * R;

    if (spo2_val > 100) spo2_val = 100;
    if (spo2_val < 70)  spo2_val = 70;
}

uint8_t detect_hr(int32_t ac)
{
    static int32_t max_v = -1000000, min_v = 1000000;
    static uint8_t st = 0;
    uint32_t tick = osKernelGetTickCount();; //系统毫秒时钟

    if (ac > max_v) max_v = ac;
    if (ac < min_v) min_v = ac;

    if (st == 0 && ac < (max_v - (max_v - min_v)/3)) 
	{
        peak_t[peak_idx++] = tick - last_peak;
        last_peak = tick;
        if (peak_idx >= HR_BUF_SIZE) peak_idx = 0;

        uint32_t sum = 0;
        for (int i = 0; i < HR_BUF_SIZE; i++) sum += peak_t[i];
        hr_val = 60000.0f / (sum / HR_BUF_SIZE);

        max_v = -1000000;
        min_v = 1000000;
        st = 1;
        return 1;
    }

    if (st == 1 && ac > (min_v + (max_v - min_v)/3)) st = 0;
    return 0;
}

void max30102_algorithm(void)
{
    int32_t ir = (int32_t)max30102_ir;
    int32_t rd = (int32_t)max30102_red;

    dc_remove(&ir, &rd);
    ir_buf[sample_idx] = ir;
    red_buf[sample_idx] = rd;
    sample_idx++;

    if (sample_idx >= SPO2_SAMPLE_CNT) {
        sample_idx = 0;
        calc_spo2();
    }
    detect_hr(ir - (ir_dc >> 5));
}

//=============================================================================
//======================== 获取结果接口 ==================================
//=============================================================================
void max30102_get(float *hr, float *spo2)
{
    *hr = hr_val;
    *spo2 = spo2_val;
}

#define FS 100
#define BUFFER_SIZE  (FS* 5) 
#define HR_FIFO_SIZE 7
#define MA4_SIZE  4 // DO NOT CHANGE
#define HAMMING_SIZE  5// DO NOT CHANGE
#define min(x,y) ((x) < (y) ? (x) : (y))

const uint16_t auw_hamm[31]={ 41,    276,    512,    276,     41 }; //Hamm=  long16(512* hamming(5)');
//uch_spo2_table is computed as  -45.060*ratioAverage* ratioAverage + 30.354 *ratioAverage + 94.845 ;
const uint8_t uch_spo2_table[184]={ 95, 95, 95, 96, 96, 96, 97, 97, 97, 97, 97, 98, 98, 98, 98, 98, 99, 99, 99, 99, 
                            99, 99, 99, 99, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 
                            100, 100, 100, 100, 99, 99, 99, 99, 99, 99, 99, 99, 98, 98, 98, 98, 98, 98, 97, 97, 
                            97, 97, 96, 96, 96, 96, 95, 95, 95, 94, 94, 94, 93, 93, 93, 92, 92, 92, 91, 91, 
                            90, 90, 89, 89, 89, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 83, 82, 82, 81, 81, 
                            80, 80, 79, 78, 78, 77, 76, 76, 75, 74, 74, 73, 72, 72, 71, 70, 69, 69, 68, 67, 
                            66, 66, 65, 64, 63, 62, 62, 61, 60, 59, 58, 57, 56, 56, 55, 54, 53, 52, 51, 50, 
                            49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 31, 30, 29, 
                            28, 27, 26, 25, 23, 22, 21, 20, 19, 17, 16, 15, 14, 12, 11, 10, 9, 7, 6, 5, 
                            3, 2, 1 } ;
static  int32_t an_dx[ BUFFER_SIZE-MA4_SIZE]; // delta
static  int32_t an_x[ BUFFER_SIZE]; //ir
static  int32_t an_y[ BUFFER_SIZE]; //red

void maxim_heart_rate_and_oxygen_saturation(uint32_t *pun_ir_buffer,  int32_t n_ir_buffer_length, uint32_t *pun_red_buffer, int32_t *pn_spo2, int8_t *pch_spo2_valid, 
                              int32_t *pn_heart_rate, int8_t  *pch_hr_valid)
/**
* \brief        Calculate the heart rate and SpO2 level
* \par          Details
*               By detecting  peaks of PPG cycle and corresponding AC/DC of red/infra-red signal, the ratio for the SPO2 is computed.
*               Since this algorithm is aiming for Arm M0/M3. formaula for SPO2 did not achieve the accuracy due to register overflow.
*               Thus, accurate SPO2 is precalculated and save longo uch_spo2_table[] per each ratio.
*
* \param[in]    *pun_ir_buffer           - IR sensor data buffer
* \param[in]    n_ir_buffer_length      - IR sensor data buffer length
* \param[in]    *pun_red_buffer          - Red sensor data buffer
* \param[out]    *pn_spo2                - Calculated SpO2 value
* \param[out]    *pch_spo2_valid         - 1 if the calculated SpO2 value is valid
* \param[out]    *pn_heart_rate          - Calculated heart rate value
* \param[out]    *pch_hr_valid           - 1 if the calculated heart rate value is valid
*
* \retval       None
*/
{
    uint32_t un_ir_mean ,un_only_once ;
    int32_t k ,n_i_ratio_count;
    int32_t i, s, m, n_exact_ir_valley_locs_count ,n_middle_idx;
    int32_t n_th1, n_npks,n_c_min;      
    int32_t an_ir_valley_locs[15] ;
    int32_t an_exact_ir_valley_locs[15] ;
    int32_t an_dx_peak_locs[15] ;
    int32_t n_peak_interval_sum;
    
    int32_t n_y_ac, n_x_ac;
    int32_t n_spo2_calc; 
    int32_t n_y_dc_max, n_x_dc_max; 
    int32_t n_y_dc_max_idx, n_x_dc_max_idx; 
    int32_t an_ratio[5],n_ratio_average; 
    int32_t n_nume,  n_denom ;
    // remove DC of ir signal    
    un_ir_mean =0; 
    for (k=0 ; k<n_ir_buffer_length ; k++ ) un_ir_mean += pun_ir_buffer[k] ;
    un_ir_mean =un_ir_mean/n_ir_buffer_length ;
    for (k=0 ; k<n_ir_buffer_length ; k++ )  an_x[k] =  pun_ir_buffer[k] - un_ir_mean ; 
    
    // 4 pt Moving Average
    for(k=0; k< BUFFER_SIZE-MA4_SIZE; k++){
        n_denom= ( an_x[k]+an_x[k+1]+ an_x[k+2]+ an_x[k+3]);
        an_x[k]=  n_denom/(int32_t)4; 
    }

    // get difference of smoothed IR signal
    
    for( k=0; k<BUFFER_SIZE-MA4_SIZE-1;  k++)
        an_dx[k]= (an_x[k+1]- an_x[k]);

    // 2-pt Moving Average to an_dx
    for(k=0; k< BUFFER_SIZE-MA4_SIZE-2; k++){
        an_dx[k] =  ( an_dx[k]+an_dx[k+1])/2 ;
    }
    
    // hamming window
    // flip wave form so that we can detect valley with peak detector
    for ( i=0 ; i<BUFFER_SIZE-HAMMING_SIZE-MA4_SIZE-2 ;i++){
        s= 0;
        for( k=i; k<i+ HAMMING_SIZE ;k++){
            s -= an_dx[k] *auw_hamm[k-i] ; 
                     }
        an_dx[i]= s/ (int32_t)1146; // divide by sum of auw_hamm 
    }

 
    n_th1=0; // threshold calculation
    for ( k=0 ; k<BUFFER_SIZE-HAMMING_SIZE ;k++){
        n_th1 += ((an_dx[k]>0)? an_dx[k] : ((int32_t)0-an_dx[k])) ;
    }
    n_th1= n_th1/ ( BUFFER_SIZE-HAMMING_SIZE);
    // peak location is acutally index for sharpest location of raw signal since we flipped the signal         
    maxim_find_peaks( an_dx_peak_locs, &n_npks, an_dx, BUFFER_SIZE-HAMMING_SIZE, n_th1, 8, 5 );//peak_height, peak_distance, max_num_peaks 

    n_peak_interval_sum =0;
    if (n_npks>=2){
        for (k=1; k<n_npks; k++)
            n_peak_interval_sum += (an_dx_peak_locs[k]-an_dx_peak_locs[k -1]);
        n_peak_interval_sum=n_peak_interval_sum/(n_npks-1);
        *pn_heart_rate=(int32_t)(6000/n_peak_interval_sum);// beats per minutes
        *pch_hr_valid  = 1;
    }
    else  {
        *pn_heart_rate = -999;
        *pch_hr_valid  = 0;
    }
            
    for ( k=0 ; k<n_npks ;k++)
        an_ir_valley_locs[k]=an_dx_peak_locs[k]+HAMMING_SIZE/2; 


    // raw value : RED(=y) and IR(=X)
    // we need to assess DC and AC value of ir and red PPG. 
    for (k=0 ; k<n_ir_buffer_length ; k++ )  {
        an_x[k] =  pun_ir_buffer[k] ; 
        an_y[k] =  pun_red_buffer[k] ; 
    }

    // find precise min near an_ir_valley_locs
    n_exact_ir_valley_locs_count =0; 
    for(k=0 ; k<n_npks ;k++){
        un_only_once =1;
        m=an_ir_valley_locs[k];
        n_c_min= 16777216;//2^24;
        if (m+5 <  BUFFER_SIZE-HAMMING_SIZE  && m-5 >0){
            for(i= m-5;i<m+5; i++)
                if (an_x[i]<n_c_min){
                    if (un_only_once >0){
                       un_only_once =0;
                   } 
                   n_c_min= an_x[i] ;
                   an_exact_ir_valley_locs[k]=i;
                }
            if (un_only_once ==0)
                n_exact_ir_valley_locs_count ++ ;
        }
    }
    if (n_exact_ir_valley_locs_count <2 ){
       *pn_spo2 =  -999 ; // do not use SPO2 since signal ratio is out of range
       *pch_spo2_valid  = 0; 
       return;
    }
    // 4 pt MA
    for(k=0; k< BUFFER_SIZE-MA4_SIZE; k++){
        an_x[k]=( an_x[k]+an_x[k+1]+ an_x[k+2]+ an_x[k+3])/(int32_t)4;
        an_y[k]=( an_y[k]+an_y[k+1]+ an_y[k+2]+ an_y[k+3])/(int32_t)4;
    }

    //using an_exact_ir_valley_locs , find ir-red DC andir-red AC for SPO2 calibration ratio
    //finding AC/DC maximum of raw ir * red between two valley locations
    n_ratio_average =0; 
    n_i_ratio_count =0; 
    
    for(k=0; k< 5; k++) an_ratio[k]=0;
    for (k=0; k< n_exact_ir_valley_locs_count; k++){
        if (an_exact_ir_valley_locs[k] > BUFFER_SIZE ){             
            *pn_spo2 =  -999 ; // do not use SPO2 since valley loc is out of range
            *pch_spo2_valid  = 0; 
            return;
        }
    }
    // find max between two valley locations 
    // and use ratio betwen AC compoent of Ir & Red and DC compoent of Ir & Red for SPO2 

    for (k=0; k< n_exact_ir_valley_locs_count-1; k++){
        n_y_dc_max= -16777216 ; 
        n_x_dc_max= - 16777216; 
        if (an_exact_ir_valley_locs[k+1]-an_exact_ir_valley_locs[k] >10){
            for (i=an_exact_ir_valley_locs[k]; i< an_exact_ir_valley_locs[k+1]; i++){
                if (an_x[i]> n_x_dc_max) {n_x_dc_max =an_x[i];n_x_dc_max_idx =i; }
                if (an_y[i]> n_y_dc_max) {n_y_dc_max =an_y[i];n_y_dc_max_idx=i;}
            }
            n_y_ac= (an_y[an_exact_ir_valley_locs[k+1]] - an_y[an_exact_ir_valley_locs[k] ] )*(n_y_dc_max_idx -an_exact_ir_valley_locs[k]); //red
            n_y_ac=  an_y[an_exact_ir_valley_locs[k]] + n_y_ac/ (an_exact_ir_valley_locs[k+1] - an_exact_ir_valley_locs[k])  ; 
        
        
            n_y_ac=  an_y[n_y_dc_max_idx] - n_y_ac;    // subracting linear DC compoenents from raw 
            n_x_ac= (an_x[an_exact_ir_valley_locs[k+1]] - an_x[an_exact_ir_valley_locs[k] ] )*(n_x_dc_max_idx -an_exact_ir_valley_locs[k]); // ir
            n_x_ac=  an_x[an_exact_ir_valley_locs[k]] + n_x_ac/ (an_exact_ir_valley_locs[k+1] - an_exact_ir_valley_locs[k]); 
            n_x_ac=  an_x[n_y_dc_max_idx] - n_x_ac;      // subracting linear DC compoenents from raw 
            n_nume=( n_y_ac *n_x_dc_max)>>7 ; //prepare X100 to preserve floating value
            n_denom= ( n_x_ac *n_y_dc_max)>>7;
            if (n_denom>0  && n_i_ratio_count <5 &&  n_nume != 0)
            {   
                an_ratio[n_i_ratio_count]= (n_nume*20)/n_denom ; //formular is ( n_y_ac *n_x_dc_max) / ( n_x_ac *n_y_dc_max) ;  ///*************************n_nume原来是*100************************//
                n_i_ratio_count++;
            }
        }
    }

    maxim_sort_ascend(an_ratio, n_i_ratio_count);
    n_middle_idx= n_i_ratio_count/2;

    if (n_middle_idx >1)
        n_ratio_average =( an_ratio[n_middle_idx-1] +an_ratio[n_middle_idx])/2; // use median
    else
        n_ratio_average = an_ratio[n_middle_idx ];

    if( n_ratio_average>2 && n_ratio_average <184){
        n_spo2_calc= uch_spo2_table[n_ratio_average] ;
        *pn_spo2 = n_spo2_calc ;
        *pch_spo2_valid  = 1;//  float_SPO2 =  -45.060*n_ratio_average* n_ratio_average/10000 + 30.354 *n_ratio_average/100 + 94.845 ;  // for comparison with table
    }
    else{
        *pn_spo2 =  -999 ; // do not use SPO2 since signal ratio is out of range
        *pch_spo2_valid  = 0; 
    }
}


void maxim_find_peaks(int32_t *pn_locs, int32_t *pn_npks, int32_t *pn_x, int32_t n_size, int32_t n_min_height, int32_t n_min_distance, int32_t n_max_num)
/**
* \brief        Find peaks
* \par          Details
*               Find at most MAX_NUM peaks above MIN_HEIGHT separated by at least MIN_DISTANCE
*
* \retval       None
*/
{
    maxim_peaks_above_min_height( pn_locs, pn_npks, pn_x, n_size, n_min_height );
    maxim_remove_close_peaks( pn_locs, pn_npks, pn_x, n_min_distance );
    *pn_npks = min( *pn_npks, n_max_num );
}

void maxim_peaks_above_min_height(int32_t *pn_locs, int32_t *pn_npks, int32_t  *pn_x, int32_t n_size, int32_t n_min_height)
/**
* \brief        Find peaks above n_min_height
* \par          Details
*               Find all peaks above MIN_HEIGHT
*
* \retval       None
*/
{
    int32_t i = 1, n_width;
    *pn_npks = 0;
    
    while (i < n_size-1){
        if (pn_x[i] > n_min_height && pn_x[i] > pn_x[i-1]){            // find left edge of potential peaks
            n_width = 1;
            while (i+n_width < n_size && pn_x[i] == pn_x[i+n_width])    // find flat peaks
                n_width++;
            if (pn_x[i] > pn_x[i+n_width] && (*pn_npks) < 15 ){                            // find right edge of peaks
                pn_locs[(*pn_npks)++] = i;        
                // for flat peaks, peak location is left edge
                i += n_width+1;
            }
            else
                i += n_width;
        }
        else
            i++;
    }
}


void maxim_remove_close_peaks(int32_t *pn_locs, int32_t *pn_npks, int32_t *pn_x, int32_t n_min_distance)
/**
* \brief        Remove peaks
* \par          Details
*               Remove peaks separated by less than MIN_DISTANCE
*
* \retval       None
*/
{
    
    int32_t i, j, n_old_npks, n_dist;
    
    /* Order peaks from large to small */
    maxim_sort_indices_descend( pn_x, pn_locs, *pn_npks );

    for ( i = -1; i < *pn_npks; i++ ){
        n_old_npks = *pn_npks;
        *pn_npks = i+1;
        for ( j = i+1; j < n_old_npks; j++ ){
            n_dist =  pn_locs[j] - ( i == -1 ? -1 : pn_locs[i] ); // lag-zero peak of autocorr is at index -1
            if ( n_dist > n_min_distance || n_dist < -n_min_distance )
                pn_locs[(*pn_npks)++] = pn_locs[j];
        }
    }

    // Resort indices longo ascending order
    maxim_sort_ascend( pn_locs, *pn_npks );
}

void maxim_sort_ascend(int32_t *pn_x,int32_t n_size) 
/**
* \brief        Sort array
* \par          Details
*               Sort array in ascending order (insertion sort algorithm)
*
* \retval       None
*/
{
    int32_t i, j, n_temp;
    for (i = 1; i < n_size; i++) {
        n_temp = pn_x[i];
        for (j = i; j > 0 && n_temp < pn_x[j-1]; j--)
            pn_x[j] = pn_x[j-1];
        pn_x[j] = n_temp;
    }
}

void maxim_sort_indices_descend(int32_t *pn_x, int32_t *pn_indx, int32_t n_size)
/**
* \brief        Sort indices
* \par          Details
*               Sort indices according to descending order (insertion sort algorithm)
*
* \retval       None
*/ 
{
    int32_t i, j, n_temp;
    for (i = 1; i < n_size; i++) {
        n_temp = pn_indx[i];
        for (j = i; j > 0 && pn_x[n_temp] > pn_x[pn_indx[j-1]]; j--)
            pn_indx[j] = pn_indx[j-1];
        pn_indx[j] = n_temp;
    }
}





uint32_t aun_ir_buffer[500]; 	 //IR LED   红外光数据，用于计算血氧
int32_t n_ir_buffer_length;    //数据长度
uint32_t aun_red_buffer[500];  //Red LED	红光数据，用于计算心率曲线以及计算心率
int32_t n_sp02; //SPO2值
int8_t ch_spo2_valid;   //用于显示SP02计算是否有效的指示符
int32_t n_heart_rate;   //心率值
int8_t  ch_hr_valid;    //用于显示心率计算是否有效的指示符

uint8_t Temp;

uint32_t un_min, un_max, un_prev_data;  
int i;
int32_t n_brightness;
float f_temp;
//u8 temp_num=0;
u8 temp[6];
u8 str[100];
u8 dis_hr=0,dis_spo2=0;
#define MAX_BRIGHTNESS 255
#define INTERRUPT_REG 0X00


//=============================================================================
//======================== 中断初始化 =====================================
//=============================================================================
void max30102_int_init(void)
{
	int ret;
  	ret = scm_gpio_configure(5,SCM_GPIO_PROP_INPUT_PULL_UP);
	if(ret ==0)
	{

		printf(" ------------[max30102_int] scm_gpio_configure ok \n");
	}
	else printf("  ----------------scm_gpio_configure FAILED \n");
}

//=============================================================================



//	MUX("scl", 19, 6),
//	MUX("sda", 20, 6),
//=============================================================================
//======================== 传感器初始化 ==================================
//=============================================================================
int max30102_init(void)
{
    uint8_t dummy;

    uint8_t id;
    int ret; 
    struct scm_i2c_cfg cfg = {0};
    cfg.role = SCM_I2C_ROLE_MASTER;
    cfg.addr_len = SCM_I2C_ADDR_LEN_7BIT;
    cfg.bitrate = 400000;
    cfg.pull_up_en = 1;

    // =====  验证I2C初始化 =====
    ret = scm_i2c_init(MAX30102_I2C_IDX);
    if (ret != 0)
	{
        printf("ERROR: scm_i2c_init failed! ret = 0x%x\n", ret);
        return -2; 
    }
    else printf("===========>  scm_i2c_init OK\n");

    // ===== 验证I2C配置 =====
    ret = scm_i2c_configure(MAX30102_I2C_IDX, &cfg, NULL, NULL);
    if (ret != 0)	
	{
        printf("ERROR: scm_i2c_configure failed! ret = %d\n", ret);
        return -3;
    }
    else printf("===========> scm_i2c_configure OK\n");


    // 复位
    ret = max30102_wr(MAX30102_MODE_CONFIG, 0x40);
	
    osDelay(MS_TO_TICKS(10)); 
    for (uint32_t i = 0; i < 100000; i++); // 复位延时

    // 读ID
    ret = max30102_rd(MAX30102_PART_ID_REG, &id);
    if (ret != 0)
	{
        printf("ERROR: max30102_rd (PART_ID) failed! ret = %d\n", ret);
        return -6;
    }
    printf("\n\n Read MAX30102 PART_ID: 0x%02X (expected: 0x%02X) \n", id, MAX30102_PART_ID);

	if (id != MAX30102_PART_ID)
	{
        printf("MAX30102 NOT FOUND! \n");
        return -1;
    }


#if 0
    // 清空FIFO
    max30102_wr(MAX30102_FIFO_WR_PTR, 0x00);
    max30102_wr(MAX30102_FIFO_RD_PTR, 0x00);

    // 配置
    max30102_wr(MAX30102_FIFO_CONFIG, 0x00);
    max30102_wr(MAX30102_SPO2_CONFIG, (0x01 << 5) | (MAX30102_SR_100HZ << 2) | MAX30102_LED_PW_18BIT);
    max30102_wr(MAX30102_LED1_PA, MAX30102_LED_CURRENT);
    max30102_wr(MAX30102_LED2_PA, MAX30102_LED_CURRENT); 

    // 开启FIFO中断
    max30102_wr(MAX30102_INT_ENABLE1, 0x40);
    max30102_wr(MAX30102_MODE_CONFIG, 0x03); // SpO2模式
#else

#define REG_INTR_STATUS_1 0x00
#define REG_INTR_STATUS_2 0x01
#define REG_INTR_ENABLE_1 0x02
#define REG_INTR_ENABLE_2 0x03
#define REG_FIFO_WR_PTR 0x04
#define REG_OVF_COUNTER 0x05
#define REG_FIFO_RD_PTR 0x06
#define REG_FIFO_DATA 0x07
#define REG_FIFO_CONFIG 0x08
#define REG_MODE_CONFIG 0x09
#define REG_SPO2_CONFIG 0x0A
#define REG_LED1_PA 0x0C
#define REG_LED2_PA 0x0D
#define REG_PILOT_PA 0x10
#define REG_MULTI_LED_CTRL1 0x11
#define REG_MULTI_LED_CTRL2 0x12
#define REG_TEMP_INTR 0x1F
#define REG_TEMP_FRAC 0x20
#define REG_TEMP_CONFIG 0x21
#define REG_PROX_INT_THRESH 0x30
#define REG_REV_ID 0xFE
#define REG_PART_ID 0xFF	
					
/*		max30102_wr(REG_INTR_ENABLE_1,0xc0); // INTR setting
		max30102_wr(REG_INTR_ENABLE_2,0x00);
		max30102_wr(REG_FIFO_WR_PTR,0x00);	//FIFO_WR_PTR[4:0]
		max30102_wr(REG_OVF_COUNTER,0x00);	//OVF_COUNTER[4:0]
		max30102_wr(REG_FIFO_RD_PTR,0x00);	//FIFO_RD_PTR[4:0]



		
		max30102_wr(REG_FIFO_CONFIG,0x0f);	//sample avg = 1, fifo rollover=false, fifo almost full = 17
//		max30102_wr(REG_FIFO_CONFIG,0x4f);		// ? 开启连续采集（滚动模式）
		max30102_wr(REG_MODE_CONFIG,0x03);	//0x02 for Red only, 0x03 for SpO2 mode 0x07 multimode LED
		max30102_wr(REG_SPO2_CONFIG,0x27);	// SPO2_ADC range = 4096nA, SPO2 sample rate (100 Hz), LED pulseWidth (400uS)  

		max30102_wr(REG_LED1_PA,0x24);		//Choose value for ~ 7mA for LED1
		max30102_wr(REG_LED2_PA,0x24);		// Choose value for ~ 7mA for LED2
		max30102_wr(REG_PILOT_PA,0x7f);		// Choose value for ~ 25mA for Pilot LED
*/
    // 先清除所有中断标志
    // ==========================
    max30102_wr(REG_INTR_STATUS_1, &dummy);
    max30102_rd(REG_INTR_STATUS_2, &dummy);

	// ========== 连续采集 + 中断使能 最终配置 ==========

	
	max30102_wr(REG_FIFO_WR_PTR,	0x00);	  // 清空FIFO写指针
	max30102_wr(REG_OVF_COUNTER,   0x00);	 // 清空溢出
	max30102_wr(REG_FIFO_RD_PTR,   0x00);	 // 清空FIFO读指针
	
	max30102_wr(REG_FIFO_CONFIG,   0x4F);	 // ? 关键：开启FIFO滚动 = 连续采集
//	max30102_wr(REG_FIFO_CONFIG,   0x6F);// 波形更平滑、心率更准--->      太慢
	
#if 0
	max30102_wr(REG_SPO2_CONFIG,   0x27);	 // 100Hz采样率 + 18位精度
	
	max30102_wr(REG_LED1_PA,	   0x24);	 // 红光电流
	max30102_wr(REG_LED2_PA,	   0x24);	 // 红外电流
	
//	max30102_wr(REG_PILOT_PA,	   0x7f);	 // 灵敏度
//	max30102_wr(REG_PILOT_PA,	   0x30);//原电流太大 → 容易信号饱和、数据顶满
	max30102_wr(REG_PILOT_PA,	   0x50);
#else 
	max30102_wr(REG_SPO2_CONFIG,   0x27);	 // 100Hz采样率 + 18位精度

		// 100Hz + 正常脉宽 + 不超增益
	//max30102_wr(REG_SPO2_CONFIG, 0x21);

	//  ADC量程缩小→灵敏度翻倍
	///max30102_wr(REG_SPO2_CONFIG,   0x07);	 // 2048nA高灵敏，100Hz+18位不变
	
	//  LED电流拉满（安全范围，提升光功率）
//	max30102_wr(REG_LED1_PA,	   0x40);	 // 红光12.7mA
//	max30102_wr(REG_LED2_PA,	   0x40);	 // 红外12.7mA
	max30102_wr(REG_LED1_PA,  0x28);//0x28（8mA）
	max30102_wr(REG_LED2_PA,  0x28);

	// 3. 引导电流（保持你现在的数值即可）
	max30102_wr(REG_PILOT_PA,	   0x50);

#endif

/*	
	max30102_wr(REG_FIFO_CONFIG,   0x4F);	 // 2倍平均+连续采集
	max30102_wr(REG_SPO2_CONFIG,   0x07);	 // 2048nA量程，高灵敏	
	max30102_wr(REG_LED1_PA,	   0x40);	 // 红光12.7mA，高灵敏
	max30102_wr(REG_LED2_PA,	   0x40);	 // 红外12.7mA，高灵敏
	max30102_wr(REG_PILOT_PA,	   0x50);	 // 引导电流匹配
	*/

//清除后再配置中断
		max30102_wr(REG_INTR_ENABLE_1,	0x40);	  // ? 使能中断：PPG数据就绪就拉低 INT 单点触发
//连续调试灵敏度	
//	max30102_wr(REG_INTR_ENABLE_1,	0xC0); // PPG_RDY + FIFO满

// 中断只开FIFO满中断，关闭PPG_RDY单点触发
//	max30102_wr(REG_INTR_ENABLE_1, 0x80); // 0x80 = 只开 A_FULL（FIFO快满才触发）

	max30102_wr(REG_INTR_ENABLE_2,	0x00);	  // 关闭温度中断



	max30102_wr(REG_MODE_CONFIG,   0x03);	 // ? 启动 SpO2 连续采集模式
	// 
		
#endif
    max30102_int_init();
	
    printf("MAX30102 init OK\n");
    return 0;
}


/****************************************************/
#define FINGER_DETECT_THRESHOLD  4000    // 手指检测阈值（无手指信号<1000，有手指>xxx0，要具体微调）

// 手指检测：1=有手指  0=无手指
uint8_t max30102_check_finger(void)
{
    uint32_t ir_val;
    
    max30102_rd_fifo(temp);
    ir_val  = ((temp[3]<<16)|(temp[4]<<8)|temp[5]) >> 6;

    if(ir_val > FINGER_DETECT_THRESHOLD)
        return 1;
	
    return 0;
}

void max30102_task(void)
{
    while(1)
    {
        // 每次重新测量先初始化
        un_min=0x3FFFF;
        un_max=0;
        n_ir_buffer_length=500; 
        n_brightness = 0;
        uint8_t sample_abort = 0;   // 采样中途松手标志

        // 1. 等待手指按压上来
        printf("请按压手指开始测量...\r\n");
        while(1)
        {
            if(max30102_check_finger())
            {
                printf("检测到手指，开始采样...\r\n");
                break;
            }
            osDelay(MS_TO_TICKS(100));
        }

        // 2. 采集500点，**每采一点都检测手指**，松手立刻终止
        for(i=0;i<n_ir_buffer_length;i++)
        {
            // 中途检测手指，松了就放弃本次采样
            if(!max30102_check_finger())
            {
                printf("采样中途手指松开，本次测量终止\r\n");
                sample_abort = 1;
                break;
            }

            max30102_rd_fifo(temp);

            aun_red_buffer[i] = ((temp[0]<<16)|(temp[1]<<8)|temp[2]) >> 6;
            aun_ir_buffer[i]  = ((temp[3]<<16)|(temp[4]<<8)|temp[5]) >> 6;

            if(un_min>aun_red_buffer[i]) un_min=aun_red_buffer[i];
            if(un_max<aun_red_buffer[i]) un_max=aun_red_buffer[i];
        }

        // 中途松手，直接重新一轮
        if(sample_abort)
        {
            dis_hr = 0;
            dis_spo2 = 0;
            osDelay(MS_TO_TICKS(500));
            continue;
        }

        // 采满500点才进算法
        un_prev_data=aun_red_buffer[n_ir_buffer_length-1];
        maxim_heart_rate_and_oxygen_saturation(aun_ir_buffer, n_ir_buffer_length, 
                                               aun_red_buffer, &n_sp02, 
                                               &ch_spo2_valid, &n_heart_rate, &ch_hr_valid); 
        printf("采样完成，算法计算中...\r\n");

        // 只打印算法有效结果
        if(ch_hr_valid && ch_spo2_valid) 
        {
            dis_hr = n_heart_rate;
            dis_spo2 = n_sp02;
            printf("===== 有效测量值 =====\r\n");
         //   printf("心率: %d BPM | 血氧: %d %% \r\n\r\n", dis_hr, dis_spo2);
			printf(" SPO2 血氧: %d %% \r\n\r\n",  dis_spo2);

			u8 spo2[]={0x1D ,0x1b, 0x02, 95, 0xff ,0xff, 0xff ,0xff ,0xD1};
		  //  spo2[3] = rand()%5+90;
			 spo2[3] = dis_spo2;
			wlt_radar_notify(spo2,9);//上报血氧
        }
        else
        {
            printf("数据无效，请保持手指稳定重新按压\r\n\r\n");
            dis_hr = 0;
            dis_spo2 = 0;
        }

        osDelay(MS_TO_TICKS(100));
    }
}


void iotalink_max30102_init()
{
	printf("------- iotalink_max30102_init -----------------\n");
    // 初始化
    max30102_init();

	osThreadAttr_t attr = {
		.name		= "max30102_task",
		.stack_size = 1024,
		.priority	= osPriorityNormal,    //osPriorityNormal		 = 24,osPriorityRealtime,//osPriorityLowcon
	};
	/* run the demo in a new thread to allow further CLI */
	osThreadNew(max30102_task, NULL, &attr);	


}







