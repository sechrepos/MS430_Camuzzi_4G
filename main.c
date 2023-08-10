#include <io430x16x.h>
#include <intrinsics.h>
#include "def.h"
#include "main.h"
#include "flash.h"
#include "button.h"
#include "lcd.h"
#include "timers.h"
#include "ui.h"
#include "monitor.h"
#include "rtc.h"
#include "modem.h"
#include "modbus_std_485.h"
#include "uart_0.h"
#include "adc.h"
#include "sender.h"

// Local Variables
volatile tREG16 Flags;
char Global_Date[6];

const char bcd_to_bin_table[] = {0,10,20,30,40,50,60,70,80,90};
const unsigned short num_to_bit[] = {
0x0001,0x0002,0x0004,0x0008,
0x0010,0x0020,0x0040,0x0080,
0x0100,0x0200,0x0400,0x0800,
0x1000,0x2000,0x4000,0x8000
};

// External variables 
extern volatile tREG08 Flags_Botones;
extern volatile tREG08 Flags_LCD;
extern tREG08 Flags_Monitor;
extern volatile char Ui_Sleep;

//******************************************************************************
void Delay(unsigned long a)
{
	while(a-- != 0) WDTCTL = WDTPW + WDTCNTCL;    // Clr WDT
}
//******************************************************************************
void init_hw(void)
{
	// Dir_485_1:	7=B4			(out)
	// NC: 			6=B3			(out)
	// O1=sol0:		5=B2			(out)
	// O0=sol1:		4=B1			(out)
	// Sw_N:		3				(in)
	// Sw_A:		2				(in)
	// Sw_Rearm:	1				(in)
	// Sw_Menu: 	0				(in)
	P1DIR = 0xF0;
	P1OUT = 0x00;

	// RX2:				7			(in)
	// TX2:				6			(out)
	// PIN POWER:	5			(out)
	// Dir_485_2:	4			(out)
	// Power fail:		3			(in)
	// RTC 1s alarm:	2			(in)
	// NC:				1=LED		(out)
	// NC:				0=B5		(out)
	P2DIR = 0x73;
	P2IES_bit.P2 = 1;			// Interrupt on falling edge
	P2IE_bit.P2 = 1;			// Enable interrupt on pin
	P2IES_bit.P3 = 1;			// Interrupt on falling edge

	// RX1				7			(in)  // Using the serial periphereal
	// TX1				6			(in)  // out when uart enable
	// RX0				5			(in)  // Using the serial periphereal
	// TX0				4			(in)  // out when uart enable
	// LCD/RTC ULCK0:	3			(out) // Using soft SPI
	// LCD/RTC SOMI0:	2			(in=RTC,out=LCD)
	// LCD/RTC SIMO0:	1			(out)
	// LCD STE0:		0			(out)
	P3DIR = 0x0B;
	P3OUT = 0;					// Tx low to avoid current leaks, Clock low
	
	// Sen_56_Ena:		7			(out)
	// Ena_485_1:		6			(out)(neg.)
	// Ena_GPRS:		5			(out)
	// Sen_012_Ena:		4			(out)
	// Sen_34_Ena:		3			(out)
	// Ena_BT:			2			(out)(neg.)
	// CTS				1			(in)
	// RTS				0			(out)
	P4DIR = 0xFD;
	P4OUT = 0x44;				// BT & 485_1 off

	// RTC CS:			7			(out)
	// SD CP:			6			(in)(not used)
	// SD WP:			5			(in)(not used)
	// LCD RESET:		4			(out)
	// SD ULCK1:		3			(out) // Using soft SPI
	// SD SOMI1:		2			(in)
	// SD SIMO1:		1			(out)
	// SD CS:			0			(out)
	P5DIR = 0x9B;
	P5OUT = 0x91; 				// LCD reset high, CLK low, RTC & SD disabled

	// ADC Battery:		7			(in)
	// ADC Sensor 6:	6			(out)(not used)
	// ADC Sensor 5:	5			(out)(not used)
	// ADC Sensor 4:	4			(out)(not used)
	// ADC Sensor 3:	3			(out)(not used)
	// ADC Sensor 2:	2			(out)(not used)
	// ADC Sensor 1:	1=temp.		(in)
	// ADC Sensor 0:	0=pressure	(in)
	P6DIR = 0x7C;	// Dir. selection
	P6SEL |= 0x83;	// Analog selection

//------------------------------------------------------------------------------
// WDT (default) from SMCLK (@4915200Hz) expires every 6.5ms
// from ACLK is not possible because it will be in low power mode for one second
// which is the max WDT time
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Clocks
//------------------------------------------------------------------------------
	// ACLK @32768, from LFXT1 (default)
    // XT2 off (default)
    
    // MCLK & SMCLK @4915200Hz, from DCOCLK
	DCOCTL = 0xE0;	// DCO=7, MOD=0
	BCSCTL1 = 0X87;	// RSEL=7
	BCSCTL2 = 0X00;	// DCOR=0
}

//******************************************************************************
void bin_16_a_bcd_5(unsigned short bin, char * bcd)
{
	char m;

	*bcd = 0;
	*(bcd+1) = 0;
	*(bcd+2) = 0;

	for(m = 16; m > 0; m--)
	{
		WDTCTL = WDTPW + WDTCNTCL;							// Clr WDT
		if((*bcd & 0X0F) >= 0X05) *bcd += 0X03; 
//		if((*bcd & 0XF0) >= 0X50) *bcd += 0X30;

		if((*(bcd+1) & 0X0F) >= 0X05) *(bcd+1) += 0X03; 
		if((*(bcd+1) & 0XF0) >= 0X50) *(bcd+1) += 0X30;

		if((*(bcd+2) & 0X0F) >= 0X05) *(bcd+2) += 0X03; 
		if((*(bcd+2) & 0XF0) >= 0X50) *(bcd+2) += 0X30; 

		*bcd <<= 1;
		if(*(bcd+1) & 0X80) *bcd |= 0X01;

		*(bcd+1) <<= 1;
		if(*(bcd+2) & 0X80) *(bcd+1) |= 0X01;

		*(bcd+2) <<= 1;
		if(bin & 0X8000) *(bcd+2) |= 0X01;

		bin <<= 1;
	}
}
//******************************************************************************
// Returns:
// iii.dddd if bin_decimal_position [0,5]
// ii.dddd0 if bin_decimal_position [6,12]
void binary_to_ascii(unsigned short bin, char * ascii, char bin_decimal_position)
{
	char bcd[3];
	char aux = 0;
	char * paux = ascii;
	unsigned short fraction;
	unsigned short addition;

// Compute the integer part
	bin_16_a_bcd_5((bin >> bin_decimal_position), bcd);			// binary to bcd

	if(bin_decimal_position <= 5)
	{
		*ascii++ = (bcd[1] & 0X0F) + 0X30;
	}
	*ascii++ = (bcd[2] >> 4) + 0X30;
	*ascii++ = (bcd[2] & 0X0F) + 0X30;

// Decimal point
	*ascii = '.';
	ascii++;

// Compute the frational part
	bin <<= (16-bin_decimal_position);
	fraction = 20000;											//5000 * 4
	addition = 0;
	for(aux = 12; aux > 0; aux--)
	{
		if(bin & 0X8000) addition += fraction;					// Acumulate
		fraction >>= 1;											// fraction/2
		bin <<= 1;												// Rotate
	}
	addition >>= 2;												// addition/4

	// addition max val = 9999
	bin_16_a_bcd_5(addition, bcd);								// binary to bcd

	*ascii++ = (bcd[1] >> 4) + 0X30;
	*ascii++ = (bcd[1] & 0X0F) + 0X30;
	*ascii++ = (bcd[2] >> 4) + 0X30;
	*ascii++ = (bcd[2] & 0X0F) + 0X30;
	if(bin_decimal_position > 5)
	{
		*ascii++ = '0';
	}

// Clear left 0's
	if(*paux == '0') *paux = ' ';
	if(bin_decimal_position <= 5)
	{
		if(*paux == ' ')
		{
			paux++;
			if(*paux == '0') *paux = ' ';
		}
	}
}

//-------------------------------------------------------------------
// One nibble to ascii
//-------------------------------------------------------------------
char nibble_to_ascii(char data)
{
	if(data <= 9) return (data + 0x30);
	else return (data + 0x37);
}

//-------------------------------------------------------------------
// Ascii to one nibble
//-------------------------------------------------------------------
char ascii_to_nibble(char data)
{
	if(data <= 0x39) return (data - 0x30);
	else return (data - 0x37);
}

//-------------------------------------------------------------------
// Two ascii to binary (input range "00" to "99")
//-------------------------------------------------------------------
char two_ascii_to_bin(char * data)
{
	char aux;

	aux = bcd_to_bin_table[*data & 0X0F];
	data++;
	aux += (*data & 0X0F);

	return aux;
}

//-------------------------------------------------------------------
// Binary to BCD
//-------------------------------------------------------------------
unsigned short bin_to_bcd(char bin)
{
    char m;
	unsigned short bcd = 0;

   	for(m = 0; m < 8; m++)
	{
   		if((bcd & 0X000F) >= 0X0005) bcd += 0X0003;
   		if((bcd & 0X00F0) >= 0X0050) bcd += 0X0030;

   		bcd <<= 1;
   		if(bin & 0X80) bcd |= 0X0001;

   		bin <<= 1;
   	}

    return bcd;
}

//-------------------------------------------------------------------
// BCD to Binary
//-------------------------------------------------------------------
char bcd_to_bin(char data)
{
	char aux;

	aux = bcd_to_bin_table[(data & 0XF0)>>4];
	aux += (data & 0X0F);

	return aux;
}

void digit_to_ascii(char c, char * buffer)
{
	unsigned short aux;

	aux = bin_to_bcd(c);
	*buffer = 0X30 + (aux >> 8);
	buffer++;
	*buffer = 0X30 + ((aux >> 4) & 0X000F);
	buffer++;
	*buffer = 0X30 + (aux & 0X000F);
}

//-------------------------------------------------------------------
// Clear rtc alarm flag & checks battery failure
//-------------------------------------------------------------------
void rtc_pf(void)
{
	WDTCTL = WDTPW + WDTCNTCL;				// Clr WDT

	if(Flag_Main_RTC_Alarm)					// RTC Alarm Flag
	{
		RTC_Clr_Alarm_Flag();
		RTC_Get_Date(Global_Date);
		Flag_Main_RTC_Alarm = 0;
	}

	if(Flag_Main_Power_Fail)
	{
		Flag_Main_Power_Fail = 0;
		BLUE_DIS = 1;						// Turn everything off
		MODEM_ENA = 0;
		SENSORS_012_ON = 0;
		SENSORS_34_ON = 0;
		while(1);							// Wait for the watchdog to reset the uc	
	}
}

//******************************************************************************
int main( void )
{
	init_hw();
	init_TimerB7();
	init_TimerA3();
	LCDInit();
	get_flash();
	RTC_Init();
	clear_buttons();
	__enable_interrupt();

	for(;;)
	{
		rtc_pf();
		monitor();
		rtc_pf();
		ADC();
		rtc_pf();
		ui();
		rtc_pf();
		sender();
		rtc_pf();
		modem();
		rtc_pf();
		modbus_task();

		if((Ui_Sleep == 0)							// Turn timerB off and go into Low Power?
		&& ((Flags.Short & FLAG_MAIN_LP_MASK) == FLAG_MAIN_LP_MASK)
		&& !Flag_Main_RTC_Alarm
		&& !Flag_Main_Power_Fail)
		{
			TimerB_off();							// This line goes above the one below (Timer A3 capture int. still enabled)
			dco_freq_adj_stop();					// This line goes below the one above (Timer A3 int. dis.)
            WDTCTL = WDTPW + WDTHOLD;				// Hold WDT
			__low_power_mode_1();					// Excecution stops here, any interrupt will restart it
            TimerB_on();                              	// Turn timer B (10ms) on
		}
	}
}
//******************************************************************************
