#ifndef _SENDER_
#define _SENDER_

#include <stdint.h>
#include <stdbool.h>
#include "def.h"

#define Flag_SMS0_Ok          Alarm_Flags.Bit._0
#define Flag_SMS1_Ok          Alarm_Flags.Bit._1
#define Flag_SMS2_Ok          Alarm_Flags.Bit._2
#define Flag_SMS3_Ok          Alarm_Flags.Bit._3
#define Flag_SMS4_Ok          Alarm_Flags.Bit._4
#define Flag_SMS5_Ok          Alarm_Flags.Bit._5
#define Flag_Voice0_Ok        Alarm_Flags.Bit._6
#define Flag_Voice1_Ok        Alarm_Flags.Bit._7
#define Flag_SMS0_No_Phone    Alarm_Flags.Bit._8
#define Flag_SMS1_No_Phone    Alarm_Flags.Bit._9
#define Flag_SMS2_No_Phone    Alarm_Flags.Bit._10
#define Flag_SMS3_No_Phone    Alarm_Flags.Bit._11
#define Flag_SMS4_No_Phone    Alarm_Flags.Bit._12
#define Flag_SMS5_No_Phone    Alarm_Flags.Bit._13
#define Flag_SMS_ACK          Alarm_Flags.Bit._14

#define MSG_TXT     72
#define PHONE_SIZE   15

struct t_message
{
	uint8_t Phone[PHONE_SIZE];
	uint8_t Text[MSG_TXT];
};

#define SMS_RCV_SIZE          4// 2's power
#define SMS_RCV_SIZE_MASK     0x03
#define TEXT_SIZE             4

struct t_sms_rcv
{
	uint8_t phone[PHONE_SIZE];
	uint8_t text[TEXT_SIZE];
};

#define ALARM_SIZE				    8	// 2's power
#define ALARM_MASK 		        (ALARM_SIZE-1)

struct t_alarm
{
	char Type;
	tREG16 Alarm_Flags;
	char GlobalRet;
	char Date_Time[6];
	char Num;
	uint8_t phone[PHONE_SIZE];
};

void sender(void);
void sender_1s_tick(void);
void set_alarm(char n, uint8_t *phone);
uint8_t *alarm_to_ascii(void);
uint8_t alarm_number(void);
void queue_sms_phone(uint8_t * data);
void queue_sms_data(uint8_t * data);
bool new_sms(void);
uint8_t sender_status(void);

#define PAUSE_TIME            5 // s
#define WAIT_ANS_TIME         120 //s

enum {
  SENDER_STAT_IDLE,
  SENDER_STAT_SMS,
  SENDER_STAT_RES,
  SENDER_STAT_VOICE
};

enum
{
  ALARM_NONE,
  ALARM_IN_HIGH,
  ALARM_IN_LOW,
  ALARM_OUT_HIGH,
  ALARM_OUT_LOW,
  ALARM_LOW_BATT,
  ALARM_PERIODIC,
  ALARM_REQ
};

enum {
  SEND_SCHEDULER_0,
  SEND_SMS0,
  SEND_SMS0_WAIT,
  SEND_SMS1,
  SEND_SMS1_WAIT,
  SEND_SMS2,
  SEND_SMS2_WAIT,
  SEND_SMS3,
  SEND_SMS3_WAIT,
  SEND_VOICE0,
  SEND_VOICE0_WAIT,
  SEND_VOICE1,
  SEND_VOICE1_WAIT,
  SEND_SCHEDULER_1,
  SEND_SMS_RCV,
  SEND_SCHEDULER_2,
  SEND_SMS_EX_PHONE,
  SEND_SMS_EX_PHONE_WAIT
};


extern struct t_message Message;

#endif
