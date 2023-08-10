#include <io430x16x.h>
#include <intrinsics.h>
#include "def.h"
#include "timers.h"
#include "monitor.h"
#include "button.h"
#include "main.h"
#include "lcd.h"
#include "ui.h"
#include "modem.h"
#include "modbus_std_485.h"
#include "uart_0.h"
#include "adc.h"
#include "sender.h"

#pragma diag_suppress = Pa082 // avoid volatile undefined behavior warnings

// Local Variables
volatile char dco_freq_state;
volatile uint16_t timerB_tick;

//******************************************************************************
// Timer A3
//******************************************************************************
void init_TimerA3(void)
{
	// Not used		15-10			000000
	// TASSELx		9-8				10	=SMCLK
	// IDx			7-6				00	=div. (/1)
	// MCx			5-4				00	=stop mode
	// Not used		3				0
	// TACLR		2				0	=clear all
	// TAIE			1				0	=int. enabled
	// TAIFG		0				0	=int. flag
	TACTL = 0x0200;

	// CMx			15-14			01	=Capture on rising edge
	// CCISx		13-12			01	=CCIxB (CCI2B=ACLK)
	// SCS			11				1	=Synchronize capture
	// SCCI			10				0	=Synchronized capture/compare input
	// Not used		9				0
	// CAP			8				1	=Capture mode
	// OUTMODx		7-5				001	=Set
	// CCIE			4				0	=Capture/compare interrupt enable
	// CCI			3				0	=Capture/compare (input bit)
	// OUT			2				0	=For output mode 0, this bit directly controls the state of the output.
	// COV			1				0	=Capture overflow. This bit indicates a capture overflow occurred.
	// CCIFG		0				0	=Capture/compare interrupt flag
	TACCTL2 = 0x5920;
}
//******************************************************************************
// Timer A3 capture interrupt
#pragma vector = TIMERA1_VECTOR
__interrupt void irqHandler_TIMERA1_VECTOR(void)
{
	static unsigned short new_cap;
	static unsigned short old_cap;
	static unsigned short cap_diff;

	if(TAIV != TAIV_TACCR2) return;

	new_cap = TACCR2;
	if(dco_freq_state == 0)		// the first interrupt is used to start fresh
	{							// the system was sleeping, then old_cap is not valid
		dco_freq_state = 1;
		old_cap = new_cap;
	}
	else
	{
		TACCTL2_bit.CCIE = 0;

		cap_diff = new_cap - old_cap;

		if(cap_diff == DCO_FSET) return;

		if(cap_diff < DCO_FSET)
		{
			if(DCOCTL < 255) DCOCTL++; 	// increase DCO by one
		}
		else
		{
			if(DCOCTL > 0) DCOCTL--;	// decrease DCO by one
		}
	}
}
//******************************************************************************
void dco_freq_adj(void)
{
	if(TACCTL2_bit.CCIE) return; // Capture going on

	dco_freq_state = 0;		// Two captures will be made, one to synch & one to capture
	TACTL = 0x0224;			// Clearing the counter ensures no wraparound (and no handling soft)
							// 10 = continuous mode
	TACCTL2_bit.CCIFG = 0;
	TACCTL2_bit.CCIE = 1;
}
//******************************************************************************
void dco_freq_adj_stop(void)
{
	TACCTL2_bit.CCIE = 0;
	TACTL = 0x0200;			// 00 = stop mode
}
//******************************************************************************

//******************************************************************************
// Timer B7
//******************************************************************************
void init_TimerB7(void)
{
	// Load value to be compared (16 / 32768Hz = 500us)
	TBCCR0 = 16;

	// Not used		15				0
	// Group		14-13			00	=indep.
	// Length		12-11			00	=16bits
	// Sin uso		10				0
	// TBSSELx		9-8				01	=ACLK
	// IDx Bits		7-6				00	=div. (/1)
	// MCx Bits		5-4				01	=up mode
	// Not used		3				0
	// TBCLR		2				1	=clear all
	// TBIE			1				0	=int. enabled
	// TBIFG		0				0	=int. flag
	TBCTL = 0x0114;

	// TBCCTL0 as default
	//Enable interrupt "Timer B7 compare 0"
	TBCCTL0_bit.CCIE = 1;
}

//******************************************************************************
void TimerB_off(void)
{
  TBCTL &= (~0X30); // 00 = stop mode
}

//******************************************************************************
void TimerB_on(void)
{
  TBCTL |= 0X10; // 01 = up mode
}

uint16_t timerB_get_tick(void)
{
  return timerB_tick;
}
//******************************************************************************
// Timer B7 compare 0 interrupt
#pragma vector = TIMERB0_VECTOR
__interrupt void irqHandler_TIMERB0_VECTOR(void)
{
	static char subtimer;
	static uint8_t ms_timer;

	// 500us tick 
	ADC_500us_tick();
	modbus_500us_tick();

	if(subtimer) subtimer--;
	else
	{	// 10ms tick
		subtimer = 20; // 10ms/500us = 20

		ui_10ms_tick();
		process_buttons();
		LCD_tick();

		dco_freq_adj();
	}

	/* 1 millisecond system tick */
	if (ms_timer)
	  ms_timer--;
	else {
	  timerB_tick++; // inc system tick
	  ms_timer = 1; // 1ms/500us = 2
	}
}

//******************************************************************************
#pragma vector = PORT2_VECTOR
__interrupt void irqHandler_PORT2_VECTOR(void)
{
      WDTCTL = WDTPW + WDTCNTCL;				// Clr WDT
	__low_power_mode_off_on_exit();				// Disable low power mode on each tick or during power fail

	// RTC 1s interrupt (might wake uC from sleep)
	if(P2IFG_bit.P2)
	{
		P2IFG_bit.P2 = 0;							// Clear int flag
		Flag_Main_RTC_Alarm = 1;					// Signal that the RTC Alarm Flag needs to be cleared
	
		monitor_1s_tick();
		ui_1s_tick();
		sender_1s_tick();
		modem_1s_tick();
	}
	// Power fail interrupt
	else
	{
		P2IE_bit.P3 = 0;							// Disable interrupt on pin
		P2IFG_bit.P3 = 0;							// Clear int flag
		Flag_Main_Power_Fail = 1;					// Signal powerfail
	}
}
//******************************************************************************
