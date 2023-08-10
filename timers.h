#ifndef __TIMERS_H__
#define __TIMERS_H__

#include <stdint.h>

void init_TimerB7(void);
void TimerB_off(void);
void TimerB_on(void);
void init_TimerA3(void);
void dco_freq_adj(void);
void dco_freq_adj_stop(void);
uint16_t timerB_get_tick(void);

#define DCO_FSET	150//=4915200/32768

#endif /* __TIMERS_H__ */
