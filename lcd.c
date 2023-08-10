#include <io430x16x.h>
#include <intrinsics.h>
#include "lcd.h"
#include "main.h"

#define LCD_REVB

unsigned short LcdMemIdx;// LCD memory index
char LcdMemory[LCD_CACHE_SIZE];// represent LCD matrix

volatile tREG08 Flags_LCD;

volatile char Timer_LCD_Blink;
volatile char Timer_LCD_Scroll;
char blink_x;
char blink_y;
char blink_size;
char * blink_ptr;
char scroll_x;
char scroll_y;
char * scroll_index;
char * scroll_start;
char * scroll_ptr;

extern volatile tREG16 Flags;

/****************************************************************************/
void LCDSleep(void)
{
	LCDEnable();
	LCDSend( 0x08, SEND_CMD );  // Blank display

//(PD bit is not working, supply current increases instead lowering)
//	LCDSend( 0x24, SEND_CMD );  // Power down, Horizontal addressing mode, Standard Commands
	LCDDisable();
}

/****************************************************************************/
void LCDWakeup(void)
{
	LCDEnable();

//(PD bit is not working, supply current increases instead lowering)
//	LCDSend( 0x20, SEND_CMD );  // Wake up, Horizontal addressing mode, Standard Commands
	LCDSend( 0x0C, SEND_CMD );  // Normal mode
	LCDDisable();
}

/****************************************************************************/
/*
void LCDBar(char data)
{
	unsigned short index;

	for(index = (LCD_X_RES*3)+GRAPH_START; index < ((LCD_X_RES*4)-GRAPH_END); index++)// Move
	{
		LcdMemory[index] = LcdMemory[index+1];
	}

	for(index = (LCD_X_RES*4)+GRAPH_START; index < ((LCD_X_RES*5)-GRAPH_END); index++)
	{
		LcdMemory[index] = LcdMemory[index+1];
	}

	LcdMemory[(LCD_X_RES*4)-GRAPH_END] = BarTable[data][1];						// High
	LcdMemory[(LCD_X_RES*5)-GRAPH_END] = BarTable[data][0];						// Low
}
*/
/****************************************************************************/
/*
void LCDBarAxis(void)
{
	char data;
	unsigned short index;

	for(data = 0; data < 16; data += 4)
	{
		for(index = 0; index < 4; index++)
		{
			LcdMemory[index + (LCD_X_RES*3)] |= DotTable[data][1];	// High
			LcdMemory[index + (LCD_X_RES*4)] |= DotTable[data][0];	// Low
		}
	}

	LcdMemory[LCD_X_RES*3] |= BarTable[15][1];	// High
	LcdMemory[LCD_X_RES*4] |= BarTable[15][0];	// Low
}
*/
/****************************************************************************/
/*
void LCDAlarm(char data)
{
	unsigned short index;

	for(index = 0; index < 4; index++)
	{
		LcdMemory[index + (LCD_X_RES*4) - GRAPH_END + 2] |= DotTable[data][1];	// High
		LcdMemory[index + (LCD_X_RES*5) - GRAPH_END + 2] |= DotTable[data][0];	// Low
	}
}
*/
/****************************************************************************/
/*
void LCDClrAlarm(void)
{
	unsigned short index;

	for(index = 0; index < 4; index++)
	{
		LcdMemory[index + (LCD_X_RES*4) - GRAPH_END + 2] = 0;					// High
		LcdMemory[index + (LCD_X_RES*5) - GRAPH_END + 2] = 0;					// Low
	}
}
*/
/****************************************************************************/
/*  Init LCD Controler                                                      */
/****************************************************************************/
void LCDInit(void)
{
	//  Toggle display reset pin.
	P5OUT_bit.P4 = 0;
	Delay(10000);
	P5OUT_bit.P4 = 1;
	Delay(100);

	// Send sequence of command
	LCDEnable();
	LCDSend( 0x21, SEND_CMD );  // LCD Extended Commands.
#ifdef LCD_REVB
	LCDSend( 0x80 + 90, SEND_CMD );  // Set LCD Vop (Contrast).
    LCDSend( 0x45, SEND_CMD );   // New LCD Correction (Y offset +5)
#else
	LCDSend( 0x80 + 70, SEND_CMD );  // Set LCD Vop (Contrast).
#endif
	LCDSend( 0x13, SEND_CMD );  // LCD bias mode 1:48.
	LCDSend( 0x06, SEND_CMD );  // Set Temp coefficent.
	LCDSend( 0x20, SEND_CMD );  // LCD Standard Commands, Horizontal addressing mode.
	LCDSend( 0x08, SEND_CMD );  // LCD blank
	LCDSend( 0x0C, SEND_CMD );  // LCD in normal mode.
	LCDDisable();
	
	// Clear and Update
	LCDClearLines(0,5);
	LCDUpdate();
}

/****************************************************************************/
/*  Send to LCD                                                             */
/*  Function : LCDSend one char (a cmd or data byte)                        */
/*      Parameters                                                          */
/*          Input   :  data and SEND_CHR or SEND_CMD                        */
/****************************************************************************/
// STEO0 pin 0
// SIMO0 pin 1
// SOMI0 (D/S) pin 2
// ULCK pin 3

void LCDEnable(void)
{
	P3DIR_bit.P2 = 1;	// Set LCD Cmd or data pin as an output
	P3OUT_bit.P0 = 0;	// Enable display controller (active low)
}

void LCDDisable(void)
{
	P3OUT_bit.P0 = 1;	// STE0 (CE) (has R1 pullup)	// Disable display controller
	P3DIR_bit.P2 = 0;	// Set LCD Cmd or data pin as an input
}

void LCDSend(char data, char cd)
{
	char i;
	WDTCTL = WDTPW + WDTCNTCL;				// Clr WDT

	// Cmd or data?
	if(cd == SEND_CHR) P3OUT_bit.P2 = 1;	// SOMI0 (D/S)
	else P3OUT_bit.P2 = 0;

	// Clock data out
	for(i=0;  i<8; i++)
	{
		P3OUT_bit.P3 = 0;  // Clock low
		if(data & 0X80) P3OUT_bit.P1 = 1; // Data
		else P3OUT_bit.P1 = 0;
		P3OUT_bit.P3 = 1;  // Clock high
		data = data << 1; // Shift data one bit to the left
	}
}

/****************************************************************************/
/*  Clear LCD                                                               */
/*  Function : LCDClear                                                     */
/****************************************************************************/
void LCDClearLines(unsigned short a, unsigned short b)
{
	unsigned short i;
  
	if((a <= blink_y) && (b >= blink_y)) Flag_LCD_Blink_On = 0;
	if((a <= scroll_y) && (b >= scroll_y)) Flag_LCD_Scroll_On = 0;

	b++;
	for(i = (LCD_X_RES * a); i < (LCD_X_RES * b); i++) LcdMemory[i] = 0;

	Flag_LCD_Update = 1;
}


/****************************************************************************/
/*  Change LCD Pixel mode                                                   */
/*  Function :                                                 */
/*      Parameters                                                          */
/*          Input   :                                               */
/****************************************************************************/

void LCDPixel (char x, char y, char mode )
{
	unsigned int    index   = 0;
	char   offset  = 0;
	char   data    = 0;
	
	index = ((y / 8) * 84) + x;
	offset  = y - ((y / 8) * 8);
	
	data = LcdMemory[index];
	
	if ( mode == PIXEL_OFF )
	{
		data &= (~(0x01 << offset));
	}
	else if ( mode == PIXEL_ON )
	{
		data |= (0x01 << offset);
	}
	else if ( mode  == PIXEL_XOR )
	{
		data ^= (0x01 << offset);
	}
	
	LcdMemory[index] = data;
}


/****************************************************************************/
/*  Function : Place one char at X,Y                                        */
/*      Parameters                                                          */
/*          Input   :  x,y,char                                             */
/****************************************************************************/
void LCDChrXY (char x, char y, char ch )
{
	unsigned int index;
	char i;
	
	index = (x + y*14)*6;//(x*48+y*48*14)/8
	
	for ( i = 0; i < 5; i++ )
	{
	  LcdMemory[index] = FontLookup[ch - 32][i] << 1;
	  index++;
	}
}

void LCDNotChrXY (char x, char y, char ch )
{
	unsigned int index;
	char i;
	
	index = x*6 + y*84 ;
	
	for ( i = 0; i < 5; i++ )
	{
	  LcdMemory[index] = (~FontLookup[ch - 32][i]) << 1;
	  index++;
	}
}

void LCDChrXY_2 (char x, char y, char ch )
{
	unsigned int index;
	char i;
	
	index = x*8 + y*84;
	
	for ( i = 0; i < 8; i++ )
	{
		LcdMemory[index] = (FontLookup_2[ch - 32][i] & 0x00FF) << 3;
		LcdMemory[index+84] = FontLookup_2[ch - 32][i] >> 5;
		index++;
	}
}

void LCDStr_2(char x, char y, char *dataPtr )
{
	// If the desired line equals the one being blinked or scrolled, disable these functions
	if(y == blink_y) Flag_LCD_Blink_On = 0;
	if(y == scroll_y) Flag_LCD_Scroll_On = 0;

	// Loop to the until the end of the string
	while( *dataPtr && (x < LINE_SIZE_2))
	{
		LCDChrXY_2( x, y, (*dataPtr));
		x++;
		dataPtr++;
	}

	Flag_LCD_Update = 1;
}

/****************************************************************************/
/*  Set LCD Contrast                                                        */
/*  Function : LcdContrast                                                  */
/*      Parameters                                                          */
/*          Input   :  contrast                                             */
/****************************************************************************/
void LCDContrast(char contrast)
{
	LCDEnable();
	//  LCD Extended Commands.
	LCDSend( 0x21, SEND_CMD );
	// Set LCD Vop (Contrast).
	LCDSend( 0x80 | contrast, SEND_CMD );
	//  LCD Standard Commands, horizontal addressing mode.
	LCDSend( 0x20, SEND_CMD );
	LCDDisable();
}


/****************************************************************************/
/*  Send string to LCD                                                      */
/*  Function : LCDStr                                                       */
/*      Parameters                                                          */
/*          Input   :  row, text                                            */
/****************************************************************************/
void LCDStr(char x, char y, char *dataPtr )
{

	// If the desired line equals the one being blinked or scrolled, disable these functions
	if(y == blink_y) Flag_LCD_Blink_On = 0;
	if(y == scroll_y) Flag_LCD_Scroll_On = 0;

	// Loop to the until the end of the string
	while( *dataPtr && (x < LINE_SIZE))
	{
		LCDChrXY( x, y, (*dataPtr));
		x++;
		dataPtr++;
	}

	Flag_LCD_Update = 1;
}

// Scroll routines
void LCDScrollOn(char y, char *dataPtr )
{
	// If the desired line equals the one being blinked, disable this function
	if(y == blink_y) Flag_LCD_Blink_On = 0;

	scroll_y = y;
	scroll_start = dataPtr;
	scroll_ptr = dataPtr;
	Flag_LCD_Scroll_On = 1;
}

void LCDScrollOff(void)
{
	Flag_LCD_Scroll_On = 0;
}

void LCDScroll(void)
{
	char i;

	scroll_index = scroll_start;
	for(i=0; i<LINE_SIZE; i++)
	{
		if(*scroll_index == 0)
		{
			LCDChrXY(i, scroll_y, ' ');
			scroll_index = scroll_ptr;
		}
		else
		{
			LCDChrXY(i, scroll_y, *scroll_index);
			scroll_index++;
		}
	}

	if(*scroll_start == 0) scroll_start = scroll_ptr;
	else scroll_start++;

	Flag_LCD_Update = 1;
}


// Blink routines
// This function will print a string but will reverse the bytes# reverse and reverse+1
void LCDBlinkOn(char x, char y, char *dataPtr, char size )
{
	// If the desired line equals the one being scrolled, disable this function
	if(y == scroll_y) Flag_LCD_Scroll_On = 0;

	blink_x = x;
	blink_y = y;
	blink_size = size;
	blink_ptr = dataPtr;
	Flag_LCD_Blink_On = 1;
}

void LCDBlinkOff(void)
{
	Flag_LCD_Blink_On = 0;
}

void LCDBlink(void)
{
	char size;

	if(Flag_LCD_Blink)
	{
		Flag_LCD_Blink = 0;
		for(size = 0; size < blink_size; size++)
		{
			LCDChrXY(blink_x + size, blink_y, *(blink_ptr + size));
		}
	}
	else
	{
		Flag_LCD_Blink = 1;
		for(size = 0; size < blink_size; size++)
		{
			LCDNotChrXY(blink_x + size, blink_y, *(blink_ptr + size));
		}

	}
	Flag_LCD_Update = 1;
}

void LCD_tick(void)
{
	if(Timer_LCD_Blink) Timer_LCD_Blink--;
	if(Timer_LCD_Scroll) Timer_LCD_Scroll--;
}
/****************************************************************************/
/*  Update LCD memory                                                       */
/*  Function : LCDUpdate (send all the array)                               */
/****************************************************************************/
/*
void LCDUpdate ( void )
{
	int i;

	if(Timer_LCD_Blink == 0)
	{
		Timer_LCD_Blink = 50; 			// 10ms*50 = 0.5sec
		if(Flag_LCD_Blink_On) LCDBlink();
	}

	if(Timer_LCD_Scroll == 0)
	{
		Timer_LCD_Scroll = 33; 			// 10ms*33 = 0.33sec
		if(Flag_LCD_Scroll_On) LCDScroll();
	}

	if(!Flag_LCD_Update) return;	// Update LCD if needed only
	Flag_LCD_Update = 0;

	LCDEnable();	
	//  Set base address X=0 Y=0
	LCDSend(0x80, SEND_CMD );
	LCDSend(0x40, SEND_CMD );
	
	//  Serialize the video buffer.
	for (i=0; i<LCD_CACHE_SIZE; i++)
	{
		LCDSend( LcdMemory[i], SEND_CHR );
	}

	LCDDisable();
}
*/

void LCDUpdate ( void )
{
 	int col, row;

	if(Timer_LCD_Blink == 0)
	{
		Timer_LCD_Blink = 50; 			// 10ms*50 = 0.5sec
		if(Flag_LCD_Blink_On) LCDBlink();
	}

	if(Timer_LCD_Scroll == 0)
	{
		Timer_LCD_Scroll = 33; 			// 10ms*33 = 0.33sec
		if(Flag_LCD_Scroll_On) LCDScroll();
	}

	if(!Flag_LCD_Update) return;	// Update LCD if needed only
	Flag_LCD_Update = 0;

	LCDEnable();	

	for(row = 0; row < 6; row++)
	{
#ifdef LCD_REVB
		LCDSend(0x40 | (row+1), SEND_CMD );	// Row
#else
		LCDSend(0x40 | row, SEND_CMD );	// Row
#endif

		LCDSend(0x80, SEND_CMD );	// Column

		for(col = 0; col < LCD_X_RES; col++)
		{
			LCDSend( LcdMemory[(LCD_X_RES * row) + col], SEND_CHR );
		}
	}

	LCDDisable();
}