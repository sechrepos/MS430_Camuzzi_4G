#ifndef __MAIN_H__
#define __MAIN_H__

#include "def.h"

void Delay(unsigned long);
void bin_16_a_bcd_5(unsigned short, char *);
void binary_to_ascii(unsigned short, char * , char);
unsigned short bin_to_bcd(char);
char bcd_to_bin(char);
char two_ascii_to_bin(char *);
char nibble_to_ascii(char data);
char ascii_to_nibble(char data);
void digit_to_ascii(char c, char * buffer);

// Main Flags
#define Flag_Main_Monitor_LP_T10ms_off	Flags.Bit._0
#define Flag_Main_Modbus_LP_T10ms_off	Flags.Bit._1
#define Flag_Main_Uart0_LP_T10ms_off	Flags.Bit._2
#define Flag_Main_ADC_LP_T10ms_off		Flags.Bit._3
#define Flag_Main_Sender_LP_T10ms_off	Flags.Bit._4
#define FLAG_MAIN_LP_MASK				0x001F
//--------------------------------------------------
#define Flag_Main_RTC_Alarm				Flags.Bit._5
#define Flag_Main_Power_Fail			Flags.Bit._6
//--------------------------------------------------
#define Flag_Main_Ui_Var_Changed		Flags.Bit._7
#define Flag_Main_Logged_In				Flags.Bit._8
#define Flag_Main_Config				Flags.Bit._9
#define Flag_Main_Master_Pass			Flags.Bit._10



// Display sates
#define DISP_NORMAL			0
#define DISP_LOWPOWER		1

extern volatile tREG16 Flags;
extern const unsigned short num_to_bit[];
extern char Global_Date[];

#endif /* __MAIN_H__ */
