#include "rtc.h"
#include "main.h"

const char days_per_month[]={0,31,28,31,30,31,30,31,31,30,31,30,31};
const char date_range_min[] = {1, 1 ,0 ,0 ,0 };
const char date_range_max[] = {31,12,99,23,59};

//------------------------------------------------------------------------------
char softSpiByte(char data)
{
	char i;
	WDTCTL = WDTPW + WDTCNTCL;				// Clr WDT
	P3OUT_bit.P3 = 0;						// Clock low
	// Clock data out
	for(i=0; i<8; i++)
	{
		if(data & 0X80) P3OUT_bit.P1 = 1;	// Set data bit
		else P3OUT_bit.P1 = 0;
		P3OUT_bit.P3 = 1;					// Clock high
		data = data << 1;					// Shift data one bit to the left
		if(P3IN_bit.P2) data |= 0x01;		// Read data bit
		P3OUT_bit.P3 = 0;					// Clock low
	}
	return data;
}
//------------------------------------------------------------------------------
void clock_to_ascii(uint8_t *dataout, uint8_t *datain)
{
	char aux, i;
	const char sep[]={'/','/',' ',':',0x00};

	for(i=DAY; i<SEC; i++)
	{
		aux = (char)bin_to_bcd(datain[i]);
		*dataout = 0X30 + (aux >> 4);
		dataout++;
		*dataout = 0X30 + (aux & 0X0F);
		dataout++;
		*dataout = sep[i];
		dataout++;
	}
}
//------------------------------------------------------------------------------
char get_max_day(char data[])
{
	if(data[MONTH] == 2) // February?
	{
		if((data[YEAR] & 0X03) == 0X00) return 29;// Leap year? (Biciesto, divisible por 4)
		else return 28;
	}
	else return (days_per_month[data[MONTH]]);
}
//------------------------------------------------------------------------------
void validate_clock(char * buffer, char index)
{
	char temp;

	if(buffer[index] < date_range_min[index]) buffer[index] = date_range_min[index];		// Min

	if(index == DAY) temp = get_max_day(buffer);			// If we compare against the day, we get it from here
	else temp = date_range_max[index];

	if(buffer[index] > temp) buffer[index] = temp;		// Max
}
//------------------------------------------------------------------------------
char FRAM_RDSR(void)
{
   char data;

   RTC_on();
   softSpiByte(RDSR);		// Write
   data = softSpiByte(0x00);// Read
   RTC_off();
   return data;
}
//------------------------------------------------------------------------------
void FRAM_WRSR(char data)
{
   RTC_on();
   softSpiByte(WREN);
   RTC_off();
   
   RTC_on();
   softSpiByte(WRSR);
   softSpiByte(data);
   RTC_off();
}
//------------------------------------------------------------------------------
void FRAM_RDPC(char address, char size, char * data)
{
   RTC_on();
   softSpiByte(RDPC);
   softSpiByte(address);

   while(size>0)
   {
      *data = softSpiByte(0x00);
      data++;
      size--;
   }

   RTC_off();
}
//------------------------------------------------------------------------------
void FRAM_WRPC(char address, char size, char * data)
{
   RTC_on();
   softSpiByte(WREN);
   RTC_off();
   
   RTC_on();
   softSpiByte(WRPC);
   softSpiByte(address);

   while(size>0)
   {
      softSpiByte(*data);
      data++;
      size--;
   }

   RTC_off();
}
//------------------------------------------------------------------------------
void FRAM_READ(unsigned short address, unsigned short size, char * data)
{
   RTC_on();
   softSpiByte(READ);
   softSpiByte((char)(address>>8));
   softSpiByte((char)(address & 0X00FF));

   while(size>0)
   {
      *data = softSpiByte(0x00);
      data++;
      size--;
   }

   RTC_off();
}
//------------------------------------------------------------------------------
void FRAM_WRITE(unsigned short address, unsigned short size, char * data)
{
   RTC_on();
   softSpiByte(WREN);
   RTC_off();

   RTC_on();
   softSpiByte(WRITE);
   softSpiByte((char)(address>>8));
   softSpiByte((char)(address & 0X00FF));

   while(size>0)
   {
      softSpiByte(*data);
      data++;
      size--;
   }

   RTC_off();
}
//------------------------------------------------------------------------------
void FRAM_CLR(unsigned short address, unsigned short size)
{
   RTC_on();
   softSpiByte(WREN);
   RTC_off();

   RTC_on();
   softSpiByte(WRITE);
   softSpiByte((char)(address>>8));
   softSpiByte((char)(address & 0X00FF));

   while(size>0)
   {
      softSpiByte(0x00);
      size--;
   }

   RTC_off();
}
//------------------------------------------------------------------------------
void RTC_Clr_Alarm_Flag(void)
{
   char data;

   data = PC_ACS;
   FRAM_WRPC(0X00, 1, &data); // If the clock is disabled, enable it; enable ACS alarm; Clr AF flag
}
//------------------------------------------------------------------------------
void RTC_Set_Date(char * buffer)
{
   char data[7];

   data[0] = PC_ACS | PC_WRITE;
   FRAM_WRPC(0X00, 1, data);  // Stop clock

   data[6] = (char)bin_to_bcd(buffer[YEAR]);
   data[5] = (char)bin_to_bcd(buffer[MONTH]);
   data[4] = (char)bin_to_bcd(buffer[DAY]);
   data[3] = 0X01;			// Day   01-07
   data[2] = (char)bin_to_bcd(buffer[HOUR]);
   data[1] = (char)bin_to_bcd(buffer[MIN]);
   data[0] = (char)bin_to_bcd(buffer[SEC]);

   FRAM_WRPC(0X02, 7, data);  // Write clock

   data[0] = PC_ACS;
   FRAM_WRPC(0X00, 1, data);  // Start clock
}
//------------------------------------------------------------------------------
void RTC_Get_Date(char * buffer)
{
   char data[7];

   data[0] = PC_ACS | PC_READ;
   FRAM_WRPC(0X00, 1, data);  // Copy time to registers

   FRAM_RDPC(0X02, 7, data);   // Read sec, min, hour, day, date, month & year

   buffer[YEAR] = bcd_to_bin(data[6]);
   buffer[MONTH] = bcd_to_bin(data[5]);
   buffer[DAY] = bcd_to_bin(data[4]);

   buffer[HOUR] = bcd_to_bin(data[2]);
   buffer[MIN] = bcd_to_bin(data[1]);
   buffer[SEC] = bcd_to_bin(data[0]);

   data[0] = PC_ACS;
   FRAM_WRPC(0X00, 1, data);  // Release registers      
}
//------------------------------------------------------------------------------
void RTC_Init(void)
{
	char data;
	
	FRAM_RDPC(0X09, 1, &data);	// Check Backup battery/Capacitor status bit
	if(data & 0x10)
	{
		RTC_Set_Date("\x1E\x0A\x0E\x0B\x3A\x00"); // init the clock: 30/10/14 11:58:00
		data = 0x44; // and write the magic number
		FRAM_WRPC(0X10, 1, &data);
	}
	else
	{
		// Look for the magic number at FRAM's serial #0
		FRAM_RDPC(0X10, 1, &data);
		if(data != 0x44)
		{	// If it is not there...
			RTC_Set_Date("\x1E\x0A\x0E\x0B\x3A\x00"); // init the clock: 30/10/14 11:58:00
			data = 0x44; // and write the magic number
			FRAM_WRPC(0X10, 1, &data);
		}
	}

	data = PC_MATCH;
	FRAM_WRPC(0X19, 1, &data); // Match = 1
	FRAM_WRPC(0X1A, 1, &data); // Match = 1
	FRAM_WRPC(0X1B, 1, &data); // Match = 1
	FRAM_WRPC(0X1C, 1, &data); // Match = 1
	FRAM_WRPC(0X1D, 1, &data); // Match = 1
	
	data = PC_AL_SW + PC_VBC + PC_FC;
	FRAM_WRPC(0X18, 1, &data); // ACS pin used with the alarm + Vcharge + Fast charge
	
	data = PC_ACS;
	FRAM_WRPC(0X00, 1, &data); // If the clock is disabled, enable it; enable ACS alarm; Clr AF flag

	data = 0X00;
	FRAM_WRPC(0X09, 1, &data); // Clear flags (CHECK BAT low voltage before doing this!!!)
}
//------------------------------------------------------------------------------
