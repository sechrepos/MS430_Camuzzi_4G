#include "def.h"

void monitor(void);
void monitor_1s_tick(void);

#define Flag_Monitor_Alarm_Pending_In_H				Flags_Monitor_Alarm.Bit._0
#define Flag_Monitor_Alarm_Pending_In_L				Flags_Monitor_Alarm.Bit._1
#define Flag_Monitor_Alarm_Pending_Out_H			Flags_Monitor_Alarm.Bit._2
#define Flag_Monitor_Alarm_Pending_Out_L			Flags_Monitor_Alarm.Bit._3
#define Flag_Monitor_Alarm_Pending_Dif				Flags_Monitor_Alarm.Bit._4
#define Flag_Monitor_Alarm_Pending_Low_Batt			Flags_Monitor_Alarm.Bit._5
#define MASK_ALARM_PEND		0x7F

#define Flag_Monitor_Alarm_Status_In_H		Flags_Monitor_Status.Bit._0
#define Flag_Monitor_Alarm_Status_In_L		Flags_Monitor_Status.Bit._1
#define Flag_Monitor_Alarm_Status_Out_H		Flags_Monitor_Status.Bit._2
#define Flag_Monitor_Alarm_Status_Out_L		Flags_Monitor_Status.Bit._3
#define Flag_Monitor_Alarm_Status_Dif		Flags_Monitor_Status.Bit._4
#define MASK_ALARM_STATUS	0x1F

enum{
MONITOR_INIT,
MONITOR_WAIT,
MONITOR_P_SAMPLE,
MONITOR_B_SAMPLE,
};

#define MONITOR_P_SAMP_T		2
#define MONITOR_B_SAMP_T		2

#define SIZE_DIF_AVG_BUF		4
#define LOG2_SIZE_DIF_AVG_BUF	2

#define MON_BUF_SIZE			4

#define FIVE_BATTERIES			          	    // Five batteries pack
#define LOW_BAT_LEVEL_HIST		(128*1)		// 4096=32V (128/V)
#ifdef FIVE_BATTERIES	
#define LOW_BAT_LEVEL			(128*17.5)	// 4096=32V (128/V)
#else
#define LOW_BAT_LEVEL			(128*21)	  // 4096=32V (128/V)
#endif



//Timer_Alarm[] index
enum{IN_HIGH, IN_LOW, OUT_HIGH, OUT_LOW};

extern tREG08 Flags_Monitor_Status;
extern tREG08 Flags_Monitor_Alarm;


