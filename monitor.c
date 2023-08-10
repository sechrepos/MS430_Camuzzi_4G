#include <stddef.h>
#include <io430x16x.h>
#include "def.h"
#include "monitor.h"
#include "flash.h"
#include "main.h"
#include "adc.h"
#include "ui.h"
#include "sender.h"

// Local Variables
tREG08 Flags_Monitor_Status;
tREG08 Flags_Monitor_Alarm;
char Monitor_State;
volatile char Monitor_P_Timer;
volatile char Monitor_B_Timer;
volatile char Alarm_Timer[4];

//******************************************************************************
void monitor_1s_tick(void)
{
	if(Monitor_P_Timer) Monitor_P_Timer--;
	if(Monitor_B_Timer) Monitor_B_Timer--;
	if(Alarm_Timer[IN_HIGH]) Alarm_Timer[IN_HIGH]--;
	if(Alarm_Timer[IN_LOW]) Alarm_Timer[IN_LOW]--;
	if(Alarm_Timer[OUT_HIGH]) Alarm_Timer[OUT_HIGH]--;
	if(Alarm_Timer[OUT_LOW]) Alarm_Timer[OUT_LOW]--;
}
//******************************************************************************
void Clear_In_H(void)
{
	Alarm_Timer[IN_HIGH] = read_flash(ID_T_Mask);
	Flag_Monitor_Alarm_Status_In_H = 0;
	Flag_Monitor_Alarm_Pending_In_H = 0;
}

void Clear_In_L(void)
{
	Alarm_Timer[IN_LOW] = read_flash(ID_T_Mask);
	Flag_Monitor_Alarm_Status_In_L = 0;
	Flag_Monitor_Alarm_Pending_In_L = 0;
}

void Clear_Out_H(void)
{
	Alarm_Timer[OUT_HIGH] = read_flash(ID_T_Mask);
	Flag_Monitor_Alarm_Status_Out_H = 0;
	Flag_Monitor_Alarm_Pending_Out_H = 0;
}

void Clear_Out_L(void)
{
	Alarm_Timer[OUT_LOW] = read_flash(ID_T_Mask);
	Flag_Monitor_Alarm_Status_Out_L = 0;
	Flag_Monitor_Alarm_Pending_Out_L = 0;
}

//******************************************************************************

void monitor(void)
{
	switch(Monitor_State)
	{
		//  --------------------------------------------------------------
		case MONITOR_INIT:
			// Set initial conditions
			Monitor_P_Timer = 2;
			Monitor_B_Timer = 2;
			Alarm_Timer[IN_HIGH] = read_flash(ID_T_Mask);
			Alarm_Timer[IN_LOW] = read_flash(ID_T_Mask);
			Alarm_Timer[OUT_HIGH] = read_flash(ID_T_Mask);
			Alarm_Timer[OUT_LOW] = read_flash(ID_T_Mask);

			Monitor_State++;
			break;

		//  --------------------------------------------------------------
		case MONITOR_WAIT:
			//  --------------------------------------------------------------
			// Get new sample?
			Flag_Main_Monitor_LP_T10ms_off = 1;								// Enable low power
			if(Monitor_P_Timer == 0)										// Sample pressure
			{
				Flag_Main_Monitor_LP_T10ms_off = 0;							// Low power not allowed
				if(!Flag_Main_Config) Monitor_P_Timer = MONITOR_P_SAMP_T;
				else Monitor_P_Timer = 1;
				ADC_New_Sample(PRESSURE);
				Monitor_State=MONITOR_P_SAMPLE;
				break;
			}

			if(Monitor_B_Timer == 0)										// Sample temperature
			{
				Flag_Main_Monitor_LP_T10ms_off = 0;							// Low power not allowed
				if(!Flag_Main_Config) Monitor_B_Timer = MONITOR_B_SAMP_T;
				else Monitor_B_Timer = 1;
				ADC_New_Sample(BATTERY);
				Monitor_State=MONITOR_B_SAMPLE;
				break;
			}

			break;

		//  --------------------------------------------------------------
		case MONITOR_P_SAMPLE:
			//  --------------------------------------------------------------
			if(!ADC_Done(PRESSURE)) break;

			if(Flag_Main_Config)
			{
				Monitor_State = MONITOR_WAIT;
				break;
			}

			//  --------------------------------------------------------------
			// In High Alarm enabled?
			//  --------------------------------------------------------------
			if(read_flash(ID_Alarm_In_H_Mode))
			{
				if(Sensors_Samples[0] >= read_flash(ID_Alarm_In_H))
				{
					Flag_Monitor_Alarm_Status_In_H = 1;

					if(!Flag_Monitor_Alarm_Pending_In_H					// No pending alarm? (one alarm at a time)
					&& (Alarm_Timer[IN_HIGH] == 0))						// Mask expired
					{
						Flag_Monitor_Alarm_Pending_In_H = 1;
						set_alarm(ALARM_IN_HIGH, NULL);
					}
				}
				else
				{
					Alarm_Timer[IN_HIGH] = read_flash(ID_T_Mask);
					if((Sensors_Samples[0] + read_flash(ID_Alarm_In_H_Hist)) <= read_flash(ID_Alarm_In_H))
 						Clear_In_H();
				}
			}
			else Clear_In_H();
		
			//  --------------------------------------------------------------
			// In Low Alarm enabled?
			//  --------------------------------------------------------------
			if(read_flash(ID_Alarm_In_L_Mode))
			{
				if(Sensors_Samples[0] <= read_flash(ID_Alarm_In_L))
				{
					Flag_Monitor_Alarm_Status_In_L = 1;

					if(!Flag_Monitor_Alarm_Pending_In_L					// No pending alarm? (one alarm at a time)
					&& (Alarm_Timer[IN_LOW] == 0))						// Mask expired
					{
						Flag_Monitor_Alarm_Pending_In_L = 1;
						set_alarm(ALARM_IN_LOW, NULL);
					}
				}
				else
				{
					Alarm_Timer[IN_LOW] = read_flash(ID_T_Mask);
					if(Sensors_Samples[0] >= (read_flash(ID_Alarm_In_L) + read_flash(ID_Alarm_In_L_Hist)))
						Clear_In_L();
				}
			}
			else Clear_In_L();

			//  --------------------------------------------------------------
			// Out High Alarm enabled?
			//  --------------------------------------------------------------
			if(read_flash(ID_Alarm_Out_H_Mode))
			{
				if(Sensors_Samples[1] >= read_flash(ID_Alarm_Out_H))
				{
					Flag_Monitor_Alarm_Status_Out_H = 1;

					if(!Flag_Monitor_Alarm_Pending_Out_H				// No pending alarm? (one alarm at a time)
					&& (Alarm_Timer[OUT_HIGH] == 0))						// Mask expired
					{
						Flag_Monitor_Alarm_Pending_Out_H = 1;
						set_alarm(ALARM_OUT_HIGH, NULL);
					}
				}
				else
				{
					Alarm_Timer[OUT_HIGH] = read_flash(ID_T_Mask);
					if((Sensors_Samples[1] + read_flash(ID_Alarm_Out_H_Hist)) <= read_flash(ID_Alarm_Out_H))
 						Clear_Out_H();
				}
			}
			else Clear_Out_H();
		
			//  --------------------------------------------------------------
			// Out Low Alarm enabled?
			//  --------------------------------------------------------------
			if(read_flash(ID_Alarm_Out_L_Mode))
			{
				if(Sensors_Samples[1] <= read_flash(ID_Alarm_Out_L))
				{
					Flag_Monitor_Alarm_Status_Out_L = 1;

					if(!Flag_Monitor_Alarm_Pending_Out_L				// No pending alarm? (one alarm at a time)
					&& (Alarm_Timer[OUT_LOW] == 0))						// Mask expired
					{
						Flag_Monitor_Alarm_Pending_Out_L = 1;
						set_alarm(ALARM_OUT_LOW, NULL);
					}
				}
				else
				{
					Alarm_Timer[OUT_LOW] = read_flash(ID_T_Mask);
					if(Sensors_Samples[1] >= (read_flash(ID_Alarm_Out_L) + read_flash(ID_Alarm_Out_L_Hist)))
						Clear_Out_L();
				}
			}
			else Clear_Out_L();

			Monitor_State = MONITOR_WAIT;
			break;
	
		//  --------------------------------------------------------------
		case MONITOR_B_SAMPLE:
			if(!ADC_Done(BATTERY)) break;

			if(Flag_Main_Config)
			{
				Monitor_State = MONITOR_WAIT;
				break;
			}

			if(Battery < LOW_BAT_LEVEL)
			{
				if(!Flag_Monitor_Alarm_Pending_Low_Batt)
				{				
					set_alarm(ALARM_LOW_BATT, NULL);
					Flag_Monitor_Alarm_Pending_Low_Batt = 1;
				}
			}
			else if(Battery > (LOW_BAT_LEVEL + LOW_BAT_LEVEL_HIST))
				Flag_Monitor_Alarm_Pending_Low_Batt = 0;

			Monitor_State = MONITOR_WAIT;
			break;

		// Default -------------------------------------------------------------
		default:
			Monitor_State = MONITOR_INIT;
			break;
	}
}
//******************************************************************************
