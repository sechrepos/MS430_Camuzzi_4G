#ifndef __FLASH_H__
#define __FLASH_H__

unsigned short read_flash(char index);
void write_flash(char index, unsigned short val);
void inc_flash(char index, char add);
void dec_flash(char index, char sub);
void get_flash(void);
void update_flash(void);
void update_flash_safe(void);

#define FLASH_PAGE_SIZE     64 // words
#define FLASH_INFO_START    0x1000
#define FLASH_INFO_START_2  0x1080

enum{SP_9600_E, SP_19200_E, SP_38400_E};
enum{NONE_E, EVEN_E, ODD_E};
enum{BITS_7_E, BITS_8_E};
enum{INV_NONE_E, INV_WORD_E, INV_BYTE_E, INV_BOTH_E};
enum{RTU_E, ASCII_E};

enum{
ID_Alarm_In_H, //1
ID_Alarm_In_H_Hist, //2
ID_Alarm_In_L, //3
ID_Alarm_In_L_Hist, //4
ID_Alarm_Out_H, //5
ID_Alarm_Out_H_Hist, //6
ID_Alarm_Out_L, //7
ID_Alarm_Out_L_Hist, //8
ID_Dec_0, //9
ID_Offset_0, //10
ID_Span_0, //11
ID_Dec_1, //12
ID_Offset_1, //13
ID_Span_1, //14
ID_Offset_Bat, //15
ID_Span_Bat, //16
ID_T_Mask, //17
ID_Alarm_In_H_Mode, //18
ID_Alarm_In_L_Mode, //19
ID_Alarm_Out_H_Mode, //20
ID_Alarm_Out_L_Mode, //21
ID_VCel_0, ID_VCel_0_a, ID_VCel_0_b, //24
ID_VCel_1, ID_VCel_1_a, ID_VCel_1_b, //27
ID_Cel_0, ID_Cel_0_a, ID_Cel_0_b, //30
ID_Cel_1, ID_Cel_1_a, ID_Cel_1_b, //33
ID_Cel_2, ID_Cel_2_a, ID_Cel_2_b, //36
ID_Cel_3, ID_Cel_3_a, ID_Cel_3_b, //39
ID_Report_Period, //40
ID_Tag_0, ID_T1, ID_T2, ID_T3, ID_T4, ID_T5, ID_T6, ID_T7, ID_T8, ID_T9, //50
ID_Pass_0, ID_P1, ID_P2, ID_P3, ID_P4, ID_P5, ID_P6, ID_P7, //58
ID_Baud, //59
ID_Par, //60
ID_Bits, //61
ID_Float, //62
ID_Protocol, //63
ID_Mod_Addr, //64
ID_Mod_On, //65
ID_Retries, //66
ID_Retries_Time, //67
FLASH_INFO_SIZE // Always last
};

extern unsigned short flash_copy[];

#endif /* __FLASH_H__ */
