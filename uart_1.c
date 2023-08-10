#include <io430x16x.h>
#include <string.h>
#include "def.h"
#include "uart_1.h"

#pragma diag_suppress = Pa082 // avoid volatile undefined behavior warnings

volatile char *Tx1_Buf;
volatile char Tx1_Counter;
volatile tREG08 Uart_Flags;

extern void modbus_rx_int(char);

void Stop_UART_1(void)
{
	U1CTL_bit.SWRST = 1;// Stop UART  (disable ints and clear flags)
	P3DIR_bit.P6 = 0;  	// dir = in
	P3SEL_bit.P6 = 0;	// io function
	P3SEL_bit.P7 = 0;	// io function

	RS485_1_DIR = 0;	// Set RS485 direction to Rx
	RS485_1_DIS = 1;	// Disable interface
}

void Start_UART_1(char opt)
{
	RS485_1_DIS = 0;	// Enable interface
	RS485_1_DIR = 0;	// Set RS485 direction to Rx

	// Pin select function
	P3SEL_bit.P6 = 1;  // io function
	P3SEL_bit.P7 = 1;  // io function
	P3DIR_bit.P6 = 1;  // dir = out

	// Init UART
	U1CTL_bit.SWRST = 1;// Stop UART  (disable ints and clear flags)
	U1CTL = 0x01 | (opt & (U1_BITS_8 | U1_PAR_EVEN	| U1_PAR_ENA)); // SWRST + options
	U1TCTL = 0x31;		// clock->SMCLK

	switch(opt & U1_SPEED_MASK)
	{
		case U1_SPEED_9K6:
			U1BR1 = 0x02;		// 4915200Hz/9600 = 512 = 0x0200
			U1BR0 = 0x00;
			UMCTL1 = 0x00;		// 00000000
			break;

		case U1_SPEED_19K2:
			U1BR1 = 0x01;		// 4915200Hz/19200 = 256 = 0x0100
			U1BR0 = 0x00;
			UMCTL1 = 0x00;		// 00000000
			break;

		case U1_SPEED_38K4:
			U1BR1 = 0x00;		// 4915200Hz/38400 = 128 = 0x0080
			U1BR0 = 0x80;
			UMCTL1 = 0x00;		// 00000000
			break;

		case U1_SPEED_115K:
			U1BR1 = 0x00;		// 4915200Hz/115200 = 42 = 0x002A
			U1BR0 = 0x2A;
			UMCTL1 = 0x6D;		// 01101101
			break;
	}

	ME2 = 0x30;				// Enable Tx & Rx 
	U1CTL_bit.SWRST = 0;	// Start Uart
	IE2_bit.URXIE1 = 1;		// Enable Rx interrupt
}
//******************************************************************************
// UART1 TX interrupt
//******************************************************************************
#pragma vector=USART1TX_VECTOR
__interrupt void irqHandler_USART1TX_VECTOR(void)
{
	if(Tx1_Counter)							// Are there any bytes to be sent?
	{
		Tx1_Counter--;						// Dec # of bytes left
		U1TXBUF = *Tx1_Buf++;		  // Copy data
	}
	else IE2_bit.UTXIE1 = 0;				// Turn Tx interrupts off
}

//******************************************************************************
// UART1 RX interrupt
//******************************************************************************
#pragma vector=USART1RX_VECTOR
__interrupt void irqHandler_USART1RX_VECTOR(void)
{
	char aux;

	if(!U1RCTL_bit.RXERR) // No error during reception?
	{
		aux = U1RXBUF;
		modbus_rx_int(aux);
	}
	else
	{
		aux = U1RXBUF;
		U1RCTL &= ~(FE+PE+OE+BRK+RXERR);	// Clear error flags
		Uart_Flag_Error = 1;				// Signal error
	}
}
//--------------------------------------------------------
void Rx_int_enable_UART_1(void)
{
	IE2_bit.URXIE1 = 1;	// Enable Rx interrupt
}
//--------------------------------------------------------
void Rx_int_disable_UART_1(void)
{
	IE2_bit.URXIE1 = 0;	// Disable Rx interrupt
}
//--------------------------------------------------------
void send_data_UART_1(char * buffer, char size)
{
	Tx1_Buf = buffer;
	Tx1_Counter = size;

	IFG2_bit.UTXIFG1 = 1;										// Set Tx int flag to force an interrupt
	IE2_bit.UTXIE1 = 1;											// Turn Tx interrupts on (and jump to interrupt)
}
//--------------------------------------------------------
char send_finished_UART_1(void)
{
	if(!U1TCTL_bit.TXEPT) return 0;								// Free?
	if(Tx1_Counter != 0)	return 0;
	return 1;
}
//--------------------------------------------------------
