#ifndef __RTC_H__
#define __RTC_H__

#include <io430x16x.h>
#include <stdint.h>

enum{DAY,MONTH,YEAR,HOUR,MIN,SEC};

void clock_to_ascii(uint8_t *dataout, uint8_t *datain);
char get_max_day(char data[]);
void validate_clock(char * buffer, char index);
void RTC_Set_Date(char * buffer);
void RTC_Init(void);
void RTC_Get_Date(char * date);
void RTC_Clr_Alarm_Flag(void);
char FRAM_RDSR(void);
void FRAM_WRSR(char data);
void FRAM_RDPC(char address, char size, char * data);
void FRAM_WRPC(char address, char size, char * data);
void FRAM_READ(unsigned short address, unsigned short size, char * data);
void FRAM_WRITE(unsigned short address, unsigned short size, char * data);
void FRAM_CLR(unsigned short address, unsigned short size);

#define WREN   0X06 // Write Enable  0000 0110b
#define WRDI   0X04 // Write Disable 0000 0100b
#define RDSR   0X05 // Read Status Register 0000 0101b
#define WRSR   0X01 // Write Status Register 0000 0001b
#define READ   0X03 // Read Memory Data 0000 0011b
#define WRITE  0X02 // Write Memory Data 0000 0010b
#define RDPC   0X13 // Read Proc. Companion 0001 0011b
#define WRPC   0X12 // Write Proc. Companion 0001 0010b

#define RTC_on()  P5OUT_bit.P7 = 0	// RTC Select
#define RTC_off() P5OUT_bit.P7 = 1  // RTC Deselect

#define PC_ACS		0X10

#define PC_AL_SW	0X40
#define PC_VBC		0X08
#define PC_FC		0X04

#define PC_MATCH	0X80
#define PC_WRITE	0X02
#define PC_READ		0X01

#endif /* __RTC_H__ */
