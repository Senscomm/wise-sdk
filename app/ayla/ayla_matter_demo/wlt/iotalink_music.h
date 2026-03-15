//iotalink_music.h

#ifndef _IOTALINK_MUSIC_H_
#define _IOTALINK_MUSIC_H_



#define MY_BUG   1
#if  MY_BUG 

#define my_printf   printf  

#else

#define my_printf(...)

#endif


void iotalink_adc_init(void);


void iotalink_music_process_0_0(void);

void iotalink_music_process_0_1(void);

void iotalink_music_process_0_2(void);


void iotalink_music_process_1(void);
void iotalink_music_process_2(void);
void iotalink_music_process_3(void);
void iotalink_music_process_4(void);

void iotalink_music_process_5(void);



#endif //_IOTALINK_MUSIC_H_

