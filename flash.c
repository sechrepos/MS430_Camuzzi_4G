#include <io430x16x.h>
#include <intrinsics.h>
#include "flash.h"

// 0x1000 to 0x10FF = information memory (two 128bytes sectors), erase size = 128bytes
// Initial flash values
__root const unsigned short config_flash[] @ 0x1000 =
{
0x0E00,	// Alarm in high						// 7001
0x0010,	// Alarm in high hist
0x0900,	// Alarm in low
0x0010,	// Alarm in low hist
0x0800,	// Alarm out high
0x0010,	// Alarm out high hist
0x0600,	// Alarm out low
0x0010,	// Alarm out low hist					// 7008 (7009 to 7010 are the input, read only, pressures)
7,		  // Decimals 0 (10=4bar, 9=8bar, 8=16bar, 7=32bar, 6=64bar, 5=128bar)
0x01A7,	// Offset 0
0x14CD,	// Span 0
10,		  // Decimals 1 (10=4bar, 9=8bar, 8=16bar, 7=32bar, 6=64bar, 5=128bar)
0x01FA,	// Offset 1
0x170C,	// Span 1
0x0000,	// Offset Bat (not used)
0x1580,	// Span Bat
60,		// Mask									// 3001
0,		// Alarm in high mode
1,		// Alarm in low mode
0,		// Alarm out high mode
1,		// Alarm out low mode	// 3005
0x1130,	// Cel phone 1 (voice)
0x4515,
0x30FF,
0xFFFF,	// Cel phone 2 (voice)
0xFFFF,
0xFFFF,
0x1128,	// Cel phone 3 (sms)
0x9766,
0x99FF,
0x1130,	// Cel phone 4 (sms)
0x4515,
0x30FF,
0x1128,	// Cel phone 5 (sms)
0x4679,
0x07FF,
0x1150,	// Cel phone 6 (sms)
0x6028,
0x18FF,
7,		// Periodic report period
'C','a','c','h','a','r','i',' ',' ',' ',	// Tag
1,1,1,1,1,1,1,1,	// Password
SP_9600_E,// Modbus
NONE_E,
BITS_8_E,
INV_WORD_E,
RTU_E,
1,		// Modbus Address
0,		// Modbus Off
2,		// SMS & Voice call retries
30,		// SMS & Voice call time between retries
};

// Flash min. & max. values
__root const unsigned short config_min[] =
{
0,		// Alarm in high min
0,		// Alarm in high hist min
0,		// Alarm in low min
0,		// Alarm in low hist min
0,		// Alarm out high min
0,		// Alarm out high hist min
0,		// Alarm out low min
0,		// Alarm out low hist min
4,		// Decimals 0 min
0X0000,	// Offset 0 min
0X0000,	// Span 0 min
4,		// Decimals 1 min
0X0000,	// Offset 1 min
0X0000,	// Span 1 min
0x0000,	// Offset Bat min (not used)
0x0001,	// Span Bat min
0,		// Mask min
0,		// Alarm in high mode min
0,		// Alarm in low mode min
0,		// Alarm out high mode min
0,		// Alarm out low mode min
0, 0, 0, 	// Cell min
0, 0, 0, 	// Cell min
0, 0, 0, 	// Cell min
0, 0, 0, 	// Cell min
0, 0, 0, 	// Cell min
0, 0, 0, 	// Cell min
0,		// Periodic report period min
32, 32, 32, 32, 32, 32, 32, 32, 32, 32, // Tag min (first ascii)
0, 0, 0, 0, 0, 0, 0, 0,	// Password min
SP_9600_E,	// Modbus min
NONE_E,
BITS_7_E,
INV_NONE_E,
RTU_E,
1,		// Modbus Address min
0,		// Modbus Off min
0,		// SMS & Voice call retries min
30,		// SMS & Voice call time between retries min
};

// Flash min. & max. values
__root const unsigned short config_max[] =
{
0X0FFF,	// Alarm in high max
0X0FFF,	// Alarm in high hist max
0X0FFF,	// Alarm in low max
0X0FFF,	// Alarm in low hist max
0X0FFF,	// Alarm out high max
0X0FFF,	// Alarm out high hist max
0X0FFF,	// Alarm out low max
0X0FFF,	// Alarm out low hist max
12,		// Decimals 0 max
0X0FFF,	// Offset 0 max
0X1FFF,	// Span 0 max
12,		// Decimals 1 max
0X0FFF,	// Offset 1 max
0X1FFF,	// Span 1 max
0x0000,	// Offset Bat max (not used)
0x1FFF,	// Span Bat max
0X0FFF,	// Mask max
1,		// Alarm in high mode max
1,		// Alarm in low mode max
1,		// Alarm out high mode max
1,		// Alarm out low mode max
0xFFFF, 0xFFFF, 0xFFFF,// Cell max
0xFFFF, 0xFFFF, 0xFFFF,	// Cell max
0xFFFF, 0xFFFF, 0xFFFF,	// Cell max
0xFFFF, 0xFFFF, 0xFFFF,	// Cell max
0xFFFF, 0xFFFF, 0xFFFF,// Cell max
0xFFFF, 0xFFFF, 0xFFFF,// Cell max
30,	// Periodic report period max
126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 	// Tag max (last ascii - 1)
1, 1, 1, 1, 1, 1, 1, 1,	// Password max
SP_38400_E,	// Modbus max
ODD_E,
BITS_8_E,
INV_BOTH_E,
ASCII_E,
247,	// Modbus Address max
1,		// Modbus Off max
100,	// SMS & Voice call retries max
600,	// SMS & Voice call time between retries max
};

unsigned short flash_copy[FLASH_INFO_SIZE];

// -----------------------------------------------------------------------------
unsigned short read_flash(char index)
{
	return flash_copy[index];
}
// -----------------------------------------------------------------------------
void write_flash(char index, unsigned short val)
{
	if(index >= FLASH_INFO_SIZE) return;
	if(val > config_max[index]) val = config_max[index];
	else if(val < config_min[index]) val = config_min[index];
	flash_copy[index] = val;
}
// -----------------------------------------------------------------------------
void inc_flash(char index, char add)
{
	if((config_max[index] - flash_copy[index]) >= add ) flash_copy[index] += add;
	else flash_copy[index] = config_max[index];
}
// -----------------------------------------------------------------------------
void dec_flash(char index, char sub)
{
	if((flash_copy[index] - config_min[index]) >= sub) flash_copy[index] -= sub;
	else flash_copy[index] = config_min[index];
}
// -----------------------------------------------------------------------------
void get_flash(void)
{
	unsigned short * flash_ptr;
	unsigned short i;

	flash_ptr = (unsigned short *)FLASH_INFO_START;
	for(i=0; i<FLASH_INFO_SIZE; i++)
	{   
		flash_copy[i] = *flash_ptr++;
		if((flash_copy[i] > config_max[i])
		|| (flash_copy[i] < config_min[i]))
			flash_copy[i] = config_min[i];
	}
}
// -----------------------------------------------------------------------------
void update_flash(void)
{
	unsigned short * flash_ptr;
	unsigned short i;
    unsigned short limit;

	__disable_interrupt();														// Disable int.

	WDTCTL = WDTPW + WDTHOLD;					                				// Stop watchdog timer
	FCTL2 = FWKEY + FSSEL_1 + 10;             									// 4.9M(MCLK) / 476kHz = 11


    
  // Low page
if(FLASH_INFO_SIZE > FLASH_PAGE_SIZE)
  limit = FLASH_PAGE_SIZE;
else
  limit = FLASH_INFO_SIZE;

	flash_ptr = (unsigned short *)FLASH_INFO_START;
	FCTL1 = FWKEY + ERASE;												// Set Erase bit
	FCTL3 = FWKEY;																// Clear Lock bit
	*flash_ptr = 0;																// Erase flash writing at any address in the segment

	FCTL1 = FWKEY + WRT;									        // Set WRT bit before writing
	for (i=0; i<limit; i++)
	{
		(*flash_ptr) = flash_copy[i];								// Write flash
		flash_ptr++;
	}
	FCTL1 = FWKEY;																// Clear WRT bit
	FCTL3 = FWKEY + LOCK;													// Set LOCK bit

  // High page
if( FLASH_INFO_SIZE > FLASH_PAGE_SIZE)
{
  limit = FLASH_INFO_SIZE - FLASH_PAGE_SIZE;
  flash_ptr = (unsigned short *)FLASH_INFO_START_2;
  FCTL1 = FWKEY + ERASE;												// Set Erase bit
  FCTL3 = FWKEY;																// Clear Lock bit
  *flash_ptr = 0;																// Erase flash writing at any address in the segment

  FCTL1 = FWKEY + WRT;									            // Set WRT bit before writing
  for (i=0; i<limit; i++)
  {
    (*flash_ptr) = flash_copy[FLASH_PAGE_SIZE + i];// Write flash
    flash_ptr++;
  }
  FCTL1 = FWKEY;																// Clear WRT bit
  FCTL3 = FWKEY + LOCK;														// Set LOCK bit
}

	WDTCTL = WDTPW + WDTCNTCL;					            // Start watchdog timer

	__enable_interrupt();														// Enable int.
}
// -----------------------------------------------------------------------------
void update_flash_safe(void)
{
	unsigned short * flash_ptr;
	unsigned short i;

	// look for changes
	flash_ptr = (unsigned short *)FLASH_INFO_START;
	for(i=0; i<FLASH_INFO_SIZE; i++)
	{
		if(flash_copy[i] != *flash_ptr++)
		{
			update_flash();
			break;
		}
	}
}
