#include <io430x16x.h>
#include "def.h"
#include "uart_0.h"

#pragma diag_suppress = Pa082 // avoid volatile undefined behavior warnings

uint8_t Tx0_Buf[TX0_BUFFER_SIZE];
uint8_t *Tx0_Ptr;
volatile char Tx0_Index;
volatile char Tx0_Counter;

char Rx0_Buf[RX0_BUFFER_SIZE];
volatile char Rx0_Queue;
volatile char Rx0_DeQueue;

uint8_t Flag_disable_RTS;

//******************************************************************************
// UART0 TX interrupt
//******************************************************************************
#pragma vector=USART0TX_VECTOR
__interrupt void irqHandler_USART0TX_VECTOR(void)
{
	if(Tx0_Counter)							// Are there any bytes to be sent?
	{
		if(U0_CTS)							// CTS is high? don´t send data
			IE1_bit.UTXIE0 = 0;				// Turn Tx interrupts off
		else
		{
			Tx0_Counter--;						// Dec # of bytes left
			U0TXBUF = *Tx0_Ptr++;		// Copy data
		}
	}
	else IE1_bit.UTXIE0 = 0;				// Turn Tx interrupts off
}

//******************************************************************************
// UART0 RX interrupt
//******************************************************************************
#pragma vector=USART0RX_VECTOR
__interrupt void irqHandler_USART0RX_VECTOR(void)
{
	char aux;

  WDTCTL = WDTPW + WDTCNTCL;				// Clr WDT
  __low_power_mode_off_on_exit();				// Disable low power mode
	
  if(!U0RCTL_bit.RXERR) // No error during reception?
	{
		aux = U0RXBUF;

		Rx0_Buf[Rx0_Queue] = aux;
		Rx0_Queue = (Rx0_Queue + 1) & RX0_BUFFER_MASK;

    if(!Flag_disable_RTS)
    {
      // If there are only FREE_SPACE bytes left set RTS high  (disable reception)					
      if(Rx0_Queue >= Rx0_DeQueue)
      {
        if( (Rx0_DeQueue + RX0_BUFFER_SIZE - Rx0_Queue) <= RX0_FREE_SPACE)
          U0_RTS = 1;
      }
      else
      {
        if( (Rx0_DeQueue - Rx0_Queue) <= RX0_FREE_SPACE)
          U0_RTS = 1;
      }
    }
	}
	else
	{
		aux = U0RXBUF;
		U0RCTL &= ~(FE+PE+OE+BRK+RXERR);	// Clear error flags
	}
}
//--------------------------------------------------------
void uart_enable_RTS(void)
{
  Flag_disable_RTS = 0;
}
//--------------------------------------------------------
void uart_disable_RTS(void)
{
  Flag_disable_RTS = 1;
}
//--------------------------------------------------------
void Stop_UART_0(void)
{
	U0CTL_bit.SWRST = 1;// Stop UART  (disable ints and clear flags)
	P3DIR_bit.P4 = 0;   // dir = in
	P3SEL_bit.P4 = 0;	// io function
	P3SEL_bit.P5 = 0;	// io function
	U0_RTS = 0;
}
//--------------------------------------------------------
void Start_UART_0(char opt)
{
	P3SEL_bit.P4 = 1;  	// UTXD0, periphereal
	P3SEL_bit.P5 = 1;  	// URXD0, periphereal
	P3DIR_bit.P4 = 1;  	// dir = out
	U0_RTS = 0;
  Flag_disable_RTS = 0;
	// Init UART
	U0CTL_bit.SWRST = 1;// Stop UART  (disable ints and clear flags)
	U0CTL = 0x01 | (opt & (U0_BITS_8 | U0_PAR_EVEN	| U0_PAR_ENA)); // SWRST + options
	U0TCTL = 0x31;		// clock->SMCLK

	switch(opt & U0_SPEED_MASK)
	{
		case U0_SPEED_9K6:
			U0BR1 = 0x02;		// 4915200Hz/9600 = 512 = 0x0200
			U0BR0 = 0x00;
			UMCTL0 = 0x00;		// 00000000
			break;

		case U0_SPEED_19K2:
			U0BR1 = 0x01;		// 4915200Hz/19200 = 256 = 0x0100
			U0BR0 = 0x00;
			UMCTL0 = 0x00;		// 00000000
			break;

		case U0_SPEED_38K4:
			U0BR1 = 0x00;		// 4915200Hz/38400 = 128 = 0x0080
			U0BR0 = 0x80;
			UMCTL0 = 0x00;		// 00000000
			break;

		case U0_SPEED_115K:
			U0BR1 = 0x00;		// 4915200Hz/115200 = 42 = 0x002A
			U0BR0 = 0x2A;
			UMCTL0 = 0x6D;		// 01101101
			break;
	}

	ME1 = 0xC0;				// Enable Tx & Rx 
	U0CTL_bit.SWRST = 0;	// Start Uart
	IE1_bit.URXIE0 = 1;		// Enable Rx interrupt
}
//--------------------------------------------------------
void Rx_int_enable_UART_0(void)
{
	IE1_bit.URXIE0 = 1;	// Enable Rx interrupt
}
//--------------------------------------------------------
void Rx_int_disable_UART_0(void)
{
	IE1_bit.URXIE0 = 0;	// Disable Rx interrupt
}
//--------------------------------------------------------
char Rx_Byte_UART_0(uint8_t *data)
{
	char aux;

	aux = Rx0_Queue;									// Save volatile var

	if(aux == Rx0_DeQueue)
	{
    if(!Flag_disable_RTS)
    {
      U0_RTS = 0;
    }
		return 0;									// If the queue is empty return 0
	}
	else
	{
		*data = Rx0_Buf[Rx0_DeQueue];					// Copy data
		Rx0_DeQueue = (Rx0_DeQueue + 1) & RX0_BUFFER_MASK;	// Update index

    if(!Flag_disable_RTS)
    {
      // If the buffer is half full set RTS low (enable reception)
      if(aux >= Rx0_DeQueue)
      {
        if( (Rx0_DeQueue + RX0_BUFFER_SIZE + aux) >= (RX0_BUFFER_SIZE/2))
          U0_RTS = 0;
      }
      else
      {
        if( (Rx0_DeQueue - aux) >= (RX0_BUFFER_SIZE/2))
          U0_RTS = 0;
      }
    }

		return 1;
	}
}
//--------------------------------------------------------
void send_data_UART_0_from_ptr(uint8_t *buf, uint8_t size)
{
	Tx0_Counter = size;
	Tx0_Ptr = buf;

	IFG1_bit.UTXIFG0 = 1;										// Set Tx int flag to force an interrupt
	IE1_bit.UTXIE0 = 1;											// Turn Tx interrupts on (and jump to interrupt)
}

void send_data_UART_0(void)
{
	Tx0_Counter = Tx0_Index;
	Tx0_Ptr = Tx0_Buf;

	IFG1_bit.UTXIFG0 = 1;										// Set Tx int flag to force an interrupt
	IE1_bit.UTXIE0 = 1;											// Turn Tx interrupts on (and jump to interrupt)
}
//--------------------------------------------------------
char send_finished_UART_0(void)
{
	if(!U0TCTL_bit.TXEPT) return 0;								// Free?
	if(Tx0_Counter != 0)
	{
		if(!IE1_bit.UTXIE0 && !U0_CTS)
		{
			IFG1_bit.UTXIFG0 = 1;								// Set Tx int flag to force an interrupt and continue
			IE1_bit.UTXIE0 = 1;									// Turn Tx interrupts on again (and jump to interrupt)
		}
		return 0;
	}
	return 1;
}
//--------------------------------------------------------
void cue_data_UART_0(uint8_t *buffer, uint8_t size)
{
	char aux;                                          

	for(aux = 0; aux < size; aux++)					// Copy data
	{
		if(Tx0_Index < TX0_BUFFER_SIZE)
			Tx0_Buf[Tx0_Index++] = buffer[aux];      		// Tx_Index could be used, using aux to avoid volatile
	}
}
//--------------------------------------------------------
void cue_byte_UART_0(char d)
{
	if(Tx0_Index < TX0_BUFFER_SIZE)
		Tx0_Buf[Tx0_Index++] = d;
}
//--------------------------------------------------------
void clr_data_UART_0(void)
{
  Tx0_Counter = 0;
  Tx0_Index = 0;
}
//--------------------------------------------------------
void cue_string_UART_0(uint8_t *s)
{
	while (Tx0_Index < TX0_BUFFER_SIZE && *s)
	  Tx0_Buf[Tx0_Index++] = *s++;
}
//--------------------------------------------------------
void send_string_UART_0(uint8_t *s)
{
	clr_data_UART_0();
	cue_string_UART_0(s);
	send_data_UART_0();
}
//--------------------------------------------------------
