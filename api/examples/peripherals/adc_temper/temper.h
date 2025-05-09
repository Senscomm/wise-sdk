#ifndef _TEMPER_H_
#define _TEMPER_H_

int temper_setup(int chan, float series_res, float ref_volt);

int temper_read(float *temperature);

#endif //_TEMPER_H_
