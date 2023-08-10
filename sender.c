#include <string.h>
#include "sender.h"
#include "monitor.h"
#include "main.h"
#include "modem.h"
#include "flash.h"
#include "ui.h"
#include "rtc.h"

uint8_t Sender_State;
uint8_t Sender_Status;
uint8_t Sender_Aux;
uint8_t Sender_Retry;
volatile uint16_t Sender_Timer;
uint16_t Sender_Periodic_Day_Count;
uint8_t Sender_Periodic_Last_Day;
uint8_t Sender_Periodic_State;
struct t_message Message;
struct t_alarm alarm[ALARM_SIZE];
uint8_t alarm_queue;
uint8_t alarm_dequeue;
struct t_sms_rcv sms_rcv[SMS_RCV_SIZE];
uint8_t sms_rcv_queue;
uint8_t sms_rcv_dequeue;
uint8_t GlobalRet;

const uint8_t alarm_ui_table[][7] = {
  "Ok    ",
  "E.Alta",
  "E.Baja",
  "S.Alta",
  "S.Baja",
  "Bat.  ",
  "Rep.  ",
  "Pedido"
};


uint8_t sender_status(void)
{
  return Sender_Status;
}
//******************************************************************************
void queue_sms_phone(uint8_t * data)
{
  uint8_t i, aux;

  aux = (sms_rcv_queue + 1) & SMS_RCV_SIZE_MASK;
  // Buffer full?
  if(aux == sms_rcv_dequeue) {
    return;
  }
  // Set it to 0
  memset(sms_rcv[sms_rcv_queue].phone, 0,PHONE_SIZE);
  // Copy phone number
  i=0;
  while(i<PHONE_SIZE)
  {
    // Finished?
    if(*data == '"')
      break;

    // Numbers only
    if((*data >= '0') && (*data <= '9'))
    {
      sms_rcv[sms_rcv_queue].phone[i] = *data;
      i++;
    }

    data++;
  }
  // Pointer will be updated by queue_sms_data()
}
//******************************************************************************
void queue_sms_data(uint8_t * data)
{
  uint8_t i, aux;

  aux = (sms_rcv_queue + 1) & SMS_RCV_SIZE_MASK;
  // Buffer full?
  if(aux == sms_rcv_dequeue) {
    return;
  }

  // Set it to 0
  memset(sms_rcv[sms_rcv_queue].text, 0, TEXT_SIZE);

  for(i=0; i<TEXT_SIZE; i++)
  {
    if(*data == 0x0D)
      break;
    sms_rcv[sms_rcv_queue].text[i] = *data;
    data++;
  }

  // Update pointer  
  sms_rcv_queue = aux;
}
//******************************************************************************
void set_alarm(char n, uint8_t *phone)
{
  char aux;
  static char alarm_count;

  aux = (alarm_queue + 1) & ALARM_MASK;
  // Buffer full?
  if(aux == alarm_dequeue) return;

  if((n != ALARM_PERIODIC)
  && (n != ALARM_REQ))
  {
    alarm_count++;
    if((alarm_count < 0x30) || (alarm_count > 0x39))
      alarm_count = 0x30;
  }

  alarm[alarm_queue].Type = n;
  alarm[alarm_queue].GlobalRet = read_flash(ID_Retries);
  alarm[alarm_queue].Alarm_Flags.Short = 0;

  memcpy(alarm[alarm_queue].Date_Time, Global_Date, 6);
  alarm[alarm_queue].Num = alarm_count;

  if((n == ALARM_PERIODIC) || (n == ALARM_REQ))
  {	// No voice call with periodic alarms, set them as already sent
    alarm[alarm_queue].Flag_Voice0_Ok = 1;
    alarm[alarm_queue].Flag_Voice1_Ok = 1;
    // No need to answer periodic alarms, set it as already answered
    alarm[alarm_queue].Flag_SMS_ACK = 1;
  }

  if(phone != NULL)
  {
    memcpy(alarm[alarm_queue].phone, phone, PHONE_SIZE);
  }
  else
  {
    memset(alarm[alarm_queue].phone, 0, PHONE_SIZE);
  }

  alarm_queue = aux;
}

bool new_sms(void)
{
  if(sms_rcv_dequeue != sms_rcv_queue)
    return 1;

  return 0;
}

//******************************************************************************
void sms_parser(void)
{
  // New SMS?
  if(sms_rcv_dequeue != sms_rcv_queue)
  {
    // SMS Response?
    if((sms_rcv[sms_rcv_dequeue].text[0] >= '0')
    && (sms_rcv[sms_rcv_dequeue].text[0] <= '9'))
    {
      // Waiting SMS Response?
      if((alarm_dequeue != alarm_queue)
      && !alarm[alarm_dequeue].Flag_SMS_ACK)
      {
        // Numbers match?
        if((alarm[alarm_dequeue].Num) == sms_rcv[sms_rcv_dequeue].text[0])
        {
          alarm[alarm_dequeue].Flag_SMS_ACK = true;
        }
      }
      sms_rcv[sms_rcv_dequeue].text[0] = 0;
    }
    // Asynch command?
    else
    {
      // Request?
      if(sms_rcv[sms_rcv_dequeue].text[0] == 'R')
      {
        set_alarm(ALARM_REQ, NULL);
      }
      else if(sms_rcv[sms_rcv_dequeue].text[0] == 'P')
      {
        set_alarm(ALARM_REQ, sms_rcv[sms_rcv_dequeue].phone);
      }
    }

    // Update dequeue
    sms_rcv_dequeue = (sms_rcv_dequeue + 1) & ALARM_MASK;
  }
}
//******************************************************************************
void clr_alarm(void)
{
  alarm[alarm_dequeue].Type = ALARM_NONE;
  alarm_dequeue = (alarm_dequeue + 1) & ALARM_MASK;
}
//******************************************************************************
uint8_t *alarm_to_ascii(void)
{
  if(alarm_dequeue == alarm_queue) return (uint8_t *)&alarm_ui_table[0][0];
  return (uint8_t *)&alarm_ui_table[alarm[alarm_dequeue].Type][0];
}
//******************************************************************************
uint8_t alarm_number(void)
{
  if(alarm_dequeue == alarm_queue) return ' ';
  return alarm[alarm_dequeue].Num + '0';
}
//******************************************************************************
void sender_1s_tick(void)
{
  if(Sender_Timer) Sender_Timer--;
}
//******************************************************************************
void sender_fill_msg(void)
{
  char aux;

  for(aux=0; aux < TAG_SIZE; aux++)
  {
    Message.Text[aux] = (char)read_flash(ID_Tag_0 + aux);
  }
  Message.Text[TAG_SIZE] = 0x0A; // New line
  
  clock_to_ascii(&Message.Text[11], (uint8_t *)alarm[alarm_dequeue].Date_Time);
  Message.Text[25] = 0x0A; // New line
  
  if((alarm[alarm_dequeue].Type != ALARM_PERIODIC)
  && (alarm[alarm_dequeue].Type != ALARM_REQ))
  {
    Message.Text[26] = alarm[alarm_dequeue].Num;
  }
  else
  {
    Message.Text[26] = ' ';
  }
  Message.Text[27] = 0x0A; // New line
  
  var_to_ascii(DISP_P0, &Message.Text[28]);
  Message.Text[37] = 0x0A; // New line
  var_to_ascii(DISP_P1, &Message.Text[38]);
  Message.Text[47] = 0x0A; // New line
  var_to_ascii(DISP_BATT, &Message.Text[48]);
  Message.Text[55] = 0x0A; // New line

  switch(alarm[alarm_dequeue].Type)
  {
  case ALARM_IN_HIGH:
    strcpy((char *)&Message.Text[56], "Entrada Alta");
    break;
  case ALARM_IN_LOW:
    strcpy((char *)&Message.Text[56], "Entrada Baja");
    break;
  case ALARM_OUT_HIGH:
    strcpy((char *)&Message.Text[56], "Salida Alta");
    break;
  case ALARM_OUT_LOW:
    strcpy((char *)&Message.Text[56], "Salida Baja");
    break;
  case ALARM_LOW_BATT:
    strcpy((char *)&Message.Text[56], "Bateria Baja");
    break;
  case ALARM_PERIODIC:
    strcpy((char *)&Message.Text[56], "Reporte");
    break;

  default: strcpy((char *)&Message.Text[56], ""); break;
  }
}
//******************************************************************************
void periodic_alarm(void)
{
  // Queue periodic report alarm?
  if((read_flash(ID_Report_Period) > 0)	            // Periodic report active?
      && (Global_Date[DAY] != Sender_Periodic_Last_Day)	// Day changed?
      && (Global_Date[HOUR] == 12))			                // Report hour?
  {
    if(Sender_Periodic_Day_Count >= (read_flash(ID_Report_Period)-1))
    {
      switch(Sender_Periodic_State)
      {
      case 0:
        if(Global_Date[MIN] == 0)
        {
          set_alarm(ALARM_PERIODIC, NULL);
          Sender_Periodic_State = 1;
        }
        break;
      case 1:
        if(Global_Date[MIN] == 10)
        {
          set_alarm(ALARM_PERIODIC, NULL);
          Sender_Periodic_State = 2;
        }
        break;
      case 2:
      default:
        Sender_Periodic_State = 0;
        Sender_Periodic_Day_Count = 0; // Reload day counter
        Sender_Periodic_Last_Day = Global_Date[DAY];	// Store new day
        break;
      }
    }
    else
    {
      Sender_Periodic_Day_Count++;
      Sender_Periodic_Last_Day = Global_Date[DAY];	// Store new day
    }
  }
}
//******************************************************************************
void sender(void)
{
  // Modem not registered
  if(modem_status() == MODEM_STAT_INI)
  {
    // Init sender
    Flag_Main_Sender_LP_T10ms_off = 1;	// Low power allowed
    Sender_State = SEND_SCHEDULER_0;
    Sender_Status = SENDER_STAT_IDLE;
    Sender_Timer = 0;
    return;
  }

  sms_parser();

  periodic_alarm();

  switch (Sender_State)
  {
  case SEND_SCHEDULER_0:
    Flag_Main_Sender_LP_T10ms_off = 1;	// Low power allowed
    Sender_Status = SENDER_STAT_IDLE;

    // Alarm to send ?
    if(alarm_dequeue != alarm_queue)
    {
      // First time or retry time?
      if(Sender_Timer == 0)
      {
        Flag_Main_Sender_LP_T10ms_off = 0;	// Low power not allowed

        // Alarm
        if(alarm[alarm_dequeue].Type != ALARM_REQ)
        {
          Sender_State = SEND_SMS0;
        }
        // Special request
        else
        {
          if(alarm[alarm_dequeue].phone[0] == 0)
            Sender_State = SEND_SMS0;
          else
            Sender_State = SEND_SMS_EX_PHONE;
        }
      }
    }
    break;

    //------------------------------------------------------
  case SEND_SMS0:
    Sender_Status = SENDER_STAT_SMS;

    // Valid phone?
    phone_flash_to_ascii(Message.Phone, ID_Cel_0);
    if(Message.Phone[0] != 0)
    {
      sender_fill_msg();
      modem_command(MODEM_CMD_SMS);
      Sender_State++;
    }
    else
    {
      alarm[alarm_dequeue].Flag_SMS0_No_Phone = 1;
      Sender_Timer = 0;
      Sender_State = SEND_SMS1;
    }
    break;

  case SEND_SMS0_WAIT:
    // Read response (wait)
    Sender_Aux = modem_result();
    if(Sender_Aux == MODEM_RES_WAIT) break; // Keep waiting
    if(Sender_Aux == MODEM_RES_OK) alarm[alarm_dequeue].Flag_SMS0_Ok = 1;
    Sender_Timer = PAUSE_TIME;
    Sender_State = SEND_SMS1;
    break;

    //------------------------------------------------------
  case SEND_SMS1:
    if(Sender_Timer) break;
    // Valid phone?
    phone_flash_to_ascii(Message.Phone, ID_Cel_1);
    if(Message.Phone[0] != 0)
    {
      sender_fill_msg();
      modem_command(MODEM_CMD_SMS);
      Sender_State++;
    }
    else
    {
      alarm[alarm_dequeue].Flag_SMS1_No_Phone = 1;
      Sender_Timer = 0;
      Sender_State = SEND_SMS2;
    }
    break;

  case SEND_SMS1_WAIT:
    // Read response (wait)
    Sender_Aux = modem_result();
    if(Sender_Aux == MODEM_RES_WAIT) break; // Keep waiting
    if(Sender_Aux == MODEM_RES_OK) alarm[alarm_dequeue].Flag_SMS1_Ok = 1;
    Sender_Timer = PAUSE_TIME;
    Sender_State = SEND_SMS2;
    break;

    //------------------------------------------------------
  case SEND_SMS2:
    if(Sender_Timer) break;
    // Valid phone?
    phone_flash_to_ascii(Message.Phone, ID_Cel_2);
    if(Message.Phone[0] != 0)
    {
      sender_fill_msg();
      modem_command(MODEM_CMD_SMS);
      Sender_State++;
    }
    else
    {
      alarm[alarm_dequeue].Flag_SMS2_No_Phone = 1;
      Sender_Timer = 0;
      Sender_State = SEND_SMS3;
    }
    break;

  case SEND_SMS2_WAIT:
    // Read response (wait)
    Sender_Aux = modem_result();
    if(Sender_Aux == MODEM_RES_WAIT) break; // Keep waiting
    if(Sender_Aux == MODEM_RES_OK) alarm[alarm_dequeue].Flag_SMS2_Ok = 1;
    Sender_Timer = PAUSE_TIME;
    Sender_State = SEND_SMS3;
    break;

    //------------------------------------------------------
  case SEND_SMS3:
    if(Sender_Timer) break;
    // Valid phone?
    phone_flash_to_ascii(Message.Phone, ID_Cel_3);
    if(Message.Phone[0] != 0)
    {
      sender_fill_msg();
      modem_command(MODEM_CMD_SMS);
      Sender_State++;
    }
    else
    {
      alarm[alarm_dequeue].Flag_SMS3_No_Phone = 1;
      Sender_Timer = 0;
      Sender_State = SEND_VOICE0;
    }
    break;

  case SEND_SMS3_WAIT:
    // Read response (wait)
    Sender_Aux = modem_result();
    if(Sender_Aux == MODEM_RES_WAIT) break; // Keep waiting
    if(Sender_Aux == MODEM_RES_OK) alarm[alarm_dequeue].Flag_SMS3_Ok = 1;
    Sender_Timer = PAUSE_TIME;
    Sender_State = SEND_VOICE0;
    break;

  case SEND_VOICE0:
    if(Sender_Timer) break;

    Sender_Status = SENDER_STAT_VOICE;

    if(alarm[alarm_dequeue].Flag_Voice0_Ok)
    {
      Sender_Timer = 0;
      Sender_State = SEND_VOICE1;
      break;
    }

    // Valid phone?
    phone_flash_to_ascii(Message.Phone, ID_VCel_0);
    if(Message.Phone[0] != 0)
    {
      modem_command(MODEM_CMD_VOICE);
      Sender_State++;
    }
    else
    {
      alarm[alarm_dequeue].Flag_Voice0_Ok = 1;
      Sender_Timer = 0;
      Sender_State = SEND_VOICE1;
    }
    break;

  case SEND_VOICE0_WAIT:
    // Read response (wait)
    Sender_Aux = modem_result();
    if(Sender_Aux == MODEM_RES_WAIT) break; // Keep waiting
    if(Sender_Aux == MODEM_RES_OK) alarm[alarm_dequeue].Flag_Voice0_Ok = 1;
    Sender_Timer = PAUSE_TIME;
    Sender_State = SEND_VOICE1;
    break;

    //------------------------------------------------------
  case SEND_VOICE1:
    if(Sender_Timer) break;

    if(alarm[alarm_dequeue].Flag_Voice1_Ok)
    {
      Sender_State = SEND_SCHEDULER_1;
      break;
    }

    // Valid phone?
    phone_flash_to_ascii(Message.Phone, ID_VCel_1);
    if(Message.Phone[0] != 0)
    {
      modem_command(MODEM_CMD_VOICE);
      Sender_State++;
    }
    else
    {
      alarm[alarm_dequeue].Flag_Voice1_Ok = 1;
      Sender_State = SEND_SCHEDULER_1;
    }
    break;

  case SEND_VOICE1_WAIT:
    // Read response (wait)
    Sender_Aux = modem_result();
    if(Sender_Aux == MODEM_RES_WAIT) break; // Keep waiting
    if(Sender_Aux == MODEM_RES_OK) alarm[alarm_dequeue].Flag_Voice1_Ok = 1;
    Sender_State++;
    break;
    //------------------------------------------------------
  case SEND_SCHEDULER_1:
    // If at least one SMS was sent then check response
    if(alarm[alarm_dequeue].Flag_SMS0_Ok
        || alarm[alarm_dequeue].Flag_SMS1_Ok
        || alarm[alarm_dequeue].Flag_SMS2_Ok
        || alarm[alarm_dequeue].Flag_SMS3_Ok)
    {
      Sender_Timer = WAIT_ANS_TIME;
      Sender_State++;
      break;
    }

    Sender_State = SEND_SCHEDULER_2;
    break;

    //------------------------------------------------------
  case SEND_SMS_RCV:
    Sender_Status = SENDER_STAT_RES;
    Flag_Main_Sender_LP_T10ms_off = 1;	// Low power allowed

    if(Sender_Timer)
    {
      if(!alarm[alarm_dequeue].Flag_SMS_ACK)
        break;
    }

    Flag_Main_Sender_LP_T10ms_off = 0;	// Low power not allowed
    Sender_State = SEND_SCHEDULER_2;
    break;

    //------------------------------------------------------
  case SEND_SCHEDULER_2:
    if(!alarm[alarm_dequeue].Flag_SMS_ACK	// Alarm not acknowledged?
    && (alarm[alarm_dequeue].GlobalRet > 0))// Global retries left?
    {
      alarm[alarm_dequeue].GlobalRet--;
      Sender_Timer = read_flash(ID_Retries_Time);
      Sender_State = SEND_SCHEDULER_0;
      break;
    }

		// Clear and finish
		clr_alarm();
	  Sender_Timer = 0;
    Sender_State = SEND_SCHEDULER_0;
    break;

    //------------------------------------------------------
  case SEND_SMS_EX_PHONE:
    Sender_Status = SENDER_STAT_SMS;
    memcpy((uint8_t *)Message.Phone, alarm[alarm_dequeue].phone, PHONE_SIZE);
    sender_fill_msg();
    modem_command(MODEM_CMD_SMS);
    Sender_State++;
    break;

  case SEND_SMS_EX_PHONE_WAIT:
    // Read response (wait)
    Sender_Aux = modem_result();
    if(Sender_Aux == MODEM_RES_WAIT) break; // Keep waiting
    
    if(Sender_Aux == MODEM_RES_OK)  // Done
    {
      clr_alarm();
      Sender_Timer = 0;
    }
    else if(alarm[alarm_dequeue].GlobalRet > 0)  // Global retries left?
    {
      alarm[alarm_dequeue].GlobalRet--;
      Sender_Timer = PAUSE_TIME;
    }
    else // No more retries
    {
      clr_alarm();
      Sender_Timer = 0;
    }

    Sender_State = SEND_SCHEDULER_0;
    break;

    //------------------------------------------------------
  default:
    Sender_State = SEND_SCHEDULER_0;
  }
}
