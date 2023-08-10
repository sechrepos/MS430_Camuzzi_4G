#include <io430x16x.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "main.h"
#include "flash.h"
#include "modem.h"
#include "sender.h"
#include "uart_0.h"
#include "timers.h"
uint8_t Modem_Rx_Data[2][MODEM_RX_DATA_SIZE];

uint8_t Modem_Rx_Buffer[MODEM_RX_BUFFER_SIZE];
uint8_t Modem_Rx_Index;

uint16_t Modem_Tickstart;
uint16_t Modem_Timeout;
uint16_t Modem_Periodic_Data_Timer;
uint8_t Modem_Keep_Awake_Timer;
uint32_t Modem_Timeout_Reset;

uint8_t Modem_State;
uint8_t Modem_Status;
uint8_t Modem_Retries;
uint8_t MODEM_Aux;
uint8_t MODEM_Voice_Call_Status;
uint8_t MODEM_Cmd;
uint8_t MODEM_Res;

uint8_t parser_ini;

uint8_t sms_id[SMS_ID_SIZE];
uint8_t sms_id_queue;
uint8_t sms_id_dequeue;

uint8_t MODEM_Signal[3];
uint8_t MODEM_Reg_Retries;
uint8_t MODEM_Voice_Retries;

uint8_t parser_state;
// Timeouts and intermessages in ms
const uint32_t modem_timeout[] =
{
2000,	        //MODEM_INIT_0,
1000,	        //MODEM_INIT_1,
20000,	        //MODEM_INIT_2,
0,	        //MODEM_INIT_3,
//20000,	          //MODEM_INIT_4,
//    0,	          //MODEM_INIT_5
17000,	        //MODEM_ECHO_OFF,
10000,	        //MODEM_ECHO_OFF_ECHO_OR_OK,
INTER_MSG,      //MODEM_ECHO_OFF_OK,
INTER_MSG,      //MODEM_FLOW_C,
STD_TIMEOUT,	//MODEM_FLOW_C_OK,
INTER_MSG,  //MODEM_KSLEEP,
STD_TIMEOUT,		//MODEM_KSLEEP_OK,
INTER_MSG,	//MODEM_KVGT,
STD_TIMEOUT,    //MODEM_KVGT_OK,
INTER_MSG,      //MODEM_VGT,
STD_TIMEOUT,    //MODEM_VGT_OK,
INTER_MSG,      //MODEM_INIT_GPIO4,
STD_TIMEOUT,	//MODEM_INIT_GPIO4_OK,
INTER_MSG,	//MODEM_INIT2_GPIO4,
STD_TIMEOUT,    //MODEM_INIT2_GPIO4_OK,
INTER_MSG,      //MODEM_INIT_GPIO1,
STD_TIMEOUT,	//MODEM_INIT_GPIO1_OK,    //10
INTER_MSG,      //MODEM_INIT_GPIO2,
STD_TIMEOUT,	//MODEM_INIT_GPIO2_OK,
INTER_MSG,	//MODEM_INIT2_GPIO2,
STD_TIMEOUT,    //MODEM_INIT2_GPIO2_OK,
INTER_MSG,      //MODEM_INIT_GPIO3,
STD_TIMEOUT,	//MODEM_INIT_GPIO3_OK,
INTER_MSG,	//MODEM_INIT2_GPIO3,
STD_TIMEOUT,	//MODEM_INIT2_GPIO3_OK,    //20
INTER_MSG,	//MODEM_REG_MODE,
STD_TIMEOUT,	//MODEM_REG_MODE_OK,
INTER_MSG,	//MODEM_MODE,
STD_TIMEOUT,	//MODEM_MODE_OK,
INTER_MSG,	//MODEM_EVENT,
STD_TIMEOUT,	//MODEM_EVENT_OK,
0,		//MODEM_INIT_DONE

INTER_MSG,		//MODEM_CMD_WAIT,
//STD_TIMEOUT,  //MODEM_FUN,
//INTER_MSG,		//MODEM_FUN_OK,
0,		        //MODEM_SLEEPING_WAIT,
0,            //MODEM_WAKE_0,
0,            //MODEM_WAKE_1,

10000,	      //SMS,                  //40
10000,	      //SMS_PROMPT,
5000,         //SMS_STORE_LOCATION,
0,		        //SMS_OK,

60000,	      //VOICE_DIAL,
0,		        //VOICE_STATUS,
STD_TIMEOUT,	//VOICE_PLAY,
200,		      //VOICE_PLAY_OK,	// PLAY low pulse duration
STD_TIMEOUT,	//VOICE_PLAY_2,
INTER_MSG,		//VOICE_PLAY_2_OK,
STD_TIMEOUT,	//VOICE_READY           //50
STD_TIMEOUT,  //VOICE_READY_2,
INTER_MSG,		//VOICE_READY_2_OK,
STD_TIMEOUT,	//VOICE_HANG,

3000,       	//MODEM_REG,
3000,	        //MODEM_REG_RESPONSE,
INTER_MSG,		//MODEM_REG_OK,
3000,         //MODEM_SIG,
3000,         //MODEM_SIG_RESPONSE,
INTER_MSG,    //MODEM_SIG_OK,
3000,         //MODEM_SMS_DEL_ALL,
INTER_MSG,    //MODEM_SMS_DEL_ALL_OK, 
STD_TIMEOUT,  //MODEM_PERIODIC_END,    //60

3000,	         //MODEM_RESET_0
0,            //MODEM_RESET_1
};

//******************************************************************************
uint8_t modem_status(void)
{
  return Modem_Status;
}

void modem_command(uint8_t cmd)
{
  MODEM_Cmd = cmd;
  MODEM_Res = MODEM_RES_WAIT;
}

uint8_t modem_result(void)
{
  return MODEM_Res;
}

void modem_1s_tick(void)
{
  if(Modem_Periodic_Data_Timer)
    Modem_Periodic_Data_Timer--;
  if(Modem_Keep_Awake_Timer)
    Modem_Keep_Awake_Timer--;
  if (Modem_Timeout_Reset)
    Modem_Timeout_Reset--;
}
//******************************************************************************

//--------------------------------------------------------
// Aux routines
//--------------------------------------------------------
void clr_signal(void)
{
  MODEM_Signal[0] = '-';
  MODEM_Signal[1] = '-';
  MODEM_Signal[2] =  0x00;
}

void MODEM_Error(void)
{
  clr_signal();
  MODEM_Cmd = MODEM_CMD_NONE;
  MODEM_Res = MODEM_RES_ERR;
  Modem_State = MODEM_RESET_0;
}

void MODEM_Back(uint8_t back)
{
  Modem_Retries--;
  Modem_State -= back;
}

//--------------------------------------------------------
void modem_parse_data(void)
{
  uint8_t data;

  while(Rx_Byte_UART_0(&data))
  {	// Avoid newline
    if(data == 0X0A) continue;

    // Store data
    if(Modem_Rx_Index < MODEM_RX_BUFFER_SIZE)
    {
      Modem_Rx_Buffer[Modem_Rx_Index] = data;
    }

    // Normal data reception -------------------------------------------------
    if(parser_state == 0)
    {
      // SMS send prompt exception
      if(Modem_Rx_Buffer[0] == '>' && Modem_Rx_Buffer[1] == ' ')
      {
        Modem_Rx_Data[0][0] = Modem_Rx_Buffer[0];
        Modem_Rx_Data[0][1] = Modem_Rx_Buffer[1];
        Modem_Rx_Index = 0;
        return;
      }
      
      // If Enter is received, finish
      if(data == 0X0D)
      {	// Avoid blank lines
        if(Modem_Rx_Index == 0)
          return;
        else
        {
          // Async exceptions
          // New SMS: "+CMT: "+54911...	
          if(strncmp ( "+CMT:", (const char *)Modem_Rx_Buffer, 5 ) == 0)
          {
            // queue phone
            queue_sms_phone(Modem_Rx_Buffer+7);
            parser_state = 1;
            Modem_Rx_Index = 0;
            return;
          }
          
          if(strncmp ( "+PBREADY", (const char *)Modem_Rx_Buffer, 8 ) == 0)
          {                       
            parser_ini = 1;
            Modem_Rx_Index = 0;
            return;
          }
          if(strncmp ( "+SIM: 1", (const char *)Modem_Rx_Buffer, 7) == 0)
          {                       
            parser_ini = 2;
            Modem_Rx_Index = 0;
            return;
          }
           if(strncmp ( "+KSUP: 0", (const char *)Modem_Rx_Buffer, 8) == 0)
          {                       
            parser_ini = 3;
            Modem_Rx_Index = 0;
            return;
          }
          
          // Voice call status
          /*
				BUSY (MO in progress)*
				RING (remote ring)*
				CONNECT (remote call accepted)*
				RELEASED (after ATH)
        NO ANSWER *
				//DISCONNECTED (remote hang-up)
				Note: In case a BUSY tone is received and at the same time ATX0
				will return NO CARRIER instead of DISCONNECTED.*
          */
          if(Modem_State==VOICE_STATUS)
      {
          if((Modem_Rx_Buffer[0]=='O') && (Modem_Rx_Buffer[1]=='K')) //OK ** DIALING
          {
            MODEM_Voice_Call_Status = VOICE_STATE_DIALING;
            Modem_Rx_Index = 0;
            return;
          }
          else if((Modem_Rx_Buffer[0]=='R') && (Modem_Rx_Buffer[1]=='I') && (Modem_Rx_Buffer[3]=='G')) //RING ** RINGING
          {
            MODEM_Voice_Call_Status = VOICE_STATE_RINGING;
            Modem_Rx_Index = 0;
            return;
          }
          else if((Modem_Rx_Buffer[0]=='C') && (Modem_Rx_Buffer[1]=='O') && (Modem_Rx_Buffer[6]=='T')) //CONNECT ** CONNECTED
          {
            MODEM_Voice_Call_Status = VOICE_STATE_CONNECTED;
            Modem_Rx_Index = 0;
            return;
          }
          else if((Modem_Rx_Buffer[0]=='R') && (Modem_Rx_Buffer[1]=='E') && (Modem_Rx_Buffer[5]=='S')) // RELEASED
          {
            MODEM_Voice_Call_Status = VOICE_STATE_RELEASED;
            Modem_Rx_Index = 0;
            return;
          }
          else if((Modem_Rx_Buffer[0]=='D') && (Modem_Rx_Buffer[1]=='I') && (Modem_Rx_Buffer[2]=='S') && (Modem_Rx_Buffer[9]=='T')) // DISCONNECTED
          {
            MODEM_Voice_Call_Status = VOICE_STATE_DISCONNECTED;
            Modem_Rx_Index = 0;
            return;
          }
          else if((Modem_Rx_Buffer[0]=='N') && (Modem_Rx_Buffer[1]=='O') && (Modem_Rx_Buffer[9]=='R')) // NO CARRIER
          {
            MODEM_Voice_Call_Status = VOICE_STATE_NO_CARRIER;
            Modem_Rx_Index = 0;
            return;
          }
          else if((Modem_Rx_Buffer[0]=='N') && (Modem_Rx_Buffer[1]=='O') && (Modem_Rx_Buffer[8]=='R')) // NO ANSWER
          {
            MODEM_Voice_Call_Status = VOICE_STATE_NO_CARRIER;
            Modem_Rx_Index = 0;
            return;
          }
          else if((Modem_Rx_Buffer[0]=='B') && (Modem_Rx_Buffer[1]=='U') && (Modem_Rx_Buffer[3]=='Y')) // BUSY
          {
            MODEM_Voice_Call_Status = VOICE_STATE_NO_CARRIER;
            Modem_Rx_Index = 0;
            return;
          }
          else if((Modem_Rx_Buffer[0]=='E') && (Modem_Rx_Buffer[1]=='R') && (Modem_Rx_Buffer[4]=='R')) // ERROR
          {
            MODEM_Voice_Call_Status = VOICE_STATE_NO_CARRIER;
            Modem_Rx_Index = 0;
            return;
          }
      }
          // Data received
          // First buffer free?
          if(Modem_Rx_Data[0][0] == 0)
            memcpy(&Modem_Rx_Data[0][0], Modem_Rx_Buffer, Modem_Rx_Index);
          else
            memcpy(&Modem_Rx_Data[1][0], Modem_Rx_Buffer, Modem_Rx_Index);
          
          Modem_Rx_Index = 0;
          return;
        }
      }
      else
      {
        Modem_Rx_Index++;
      }
    }
    // SMS payload reception -------------------------------------------------
    else
    {
      // If Enter is received, finish
      if(data == 0X0D)
      {
        // queue data
        queue_sms_data(Modem_Rx_Buffer);
        parser_state = 0;
        Modem_Rx_Index = 0;
         
        Modem_Status = MODEM_STAT_SMS_TXRX;
        Modem_Keep_Awake_Timer = MODEM_AWAKE_TIME;
         
        return;
      }
      else
      {
        Modem_Rx_Index++;
      }
    }
  }
}

//--------------------------------------------------------
// Reads a package
//--------------------------------------------------------
bool modem_read_data(void)
{
  if(Modem_Rx_Data[0][0] != 0)
    return true;
  
  return false;
}

bool modem_read_data_2(void)
{
  if(Modem_Rx_Data[1][0] != 0)
    return true;
  
  return false;
}

void clr_rx_data(void)
{
  Modem_Rx_Index = 0;
  parser_state = 0;
  memset(&Modem_Rx_Data[0][0], 0, MODEM_RX_DATA_SIZE);
  memset(&Modem_Rx_Data[1][0], 0, MODEM_RX_DATA_SIZE);
}
bool modem_timer_expired(void)
{
  return ((timerB_get_tick() - Modem_Tickstart) >= Modem_Timeout);
}

//------------------------------------------------------
void modem(void)
{
  modem_parse_data();
  switch(Modem_State)
  {
  //------------------------------------------------------
  case MODEM_INIT_0:
    Flag_Main_Uart0_LP_T10ms_off = 0; // Low power not allowed
    parser_ini = 0;
    MODEM_ENA = 1;
    PIN_ENABLE = 1;
    Start_UART_0(U0_BITS_8 + /* U0_SPEED_9K6 */ U0_SPEED_115K);
    Modem_Status = MODEM_STAT_INI;
    Modem_Periodic_Data_Timer = 0;
    clr_signal();
    // Wait power supply + modem to be ready
    Modem_Tickstart = timerB_get_tick();
    Modem_Timeout = modem_timeout[MODEM_INIT_0];
    Modem_State++;
    break;

  case MODEM_INIT_1:
    //Wakeup
    if(!modem_timer_expired()) break;
    //uart_disable_RTS();
    //U0_RTS = 1;
    Modem_Tickstart = timerB_get_tick();
    Modem_Timeout = WAKE_PULSE_TIME;
    Modem_State++;
    break;

  case MODEM_INIT_2:
    if(!modem_timer_expired()) break;
    U0_RTS = 0;
    uart_enable_RTS();
    Modem_Timeout = modem_timeout[MODEM_INIT_2];
    Modem_State++;
    break;

  case MODEM_INIT_3:
    //Exit prompt
    if(!modem_timer_expired())
    {
    
      if(parser_ini==3)
      {
      Modem_Rx_Index = 0;
      Modem_Retries = 2;
      Modem_Tickstart = timerB_get_tick();
      Modem_Timeout = modem_timeout[MODEM_INIT_3];
        Modem_State++;
         break;
      }
    
      break;
    }  
    
    MODEM_Error();
    break;

    //------------------------------------------------------
  case MODEM_ECHO_OFF:
    if(!send_finished_UART_0()) break;
    if(!modem_timer_expired()) break;
    clr_rx_data();
    send_string_UART_0("ATE0\x0D");
    Modem_Tickstart = timerB_get_tick();
    Modem_Timeout = modem_timeout[MODEM_ECHO_OFF];
    Modem_State++;
    break;

  case MODEM_ECHO_OFF_ECHO_OR_OK:
    if(!send_finished_UART_0()) break;
    if(modem_timer_expired())
    {
      if (Modem_Retries)
        MODEM_Back(1);
      else
        MODEM_Error();
      break;
    }
    if(!modem_read_data()) break;		// Wait echo or OK
    // wait for "ATE0\x0D"
    if(strncmp ( "ATE0", (const char *)&Modem_Rx_Data[0][0], 4 ) == 0)
    {
      Modem_State++;
      break;
    }
		// wait for "OK\x0D"			
    else if(strncmp ( "OK", (const char *)&Modem_Rx_Data[0][0], 2 ) == 0)
    {
      Modem_Retries = 2;
      Modem_Tickstart = timerB_get_tick();
      Modem_Timeout = modem_timeout[MODEM_ECHO_OFF_ECHO_OR_OK];
      Modem_State = MODEM_FLOW_C;
      break;
    }
    else
    {
      if(Modem_Retries) MODEM_Back(1);
      else MODEM_Error();
    }
    break;

  case MODEM_ECHO_OFF_OK:
    if(modem_timer_expired())
    {
      if(Modem_Retries) MODEM_Back(2);
      else MODEM_Error();
      break;
    }
    if(!modem_read_data_2()) break;		// Wait echo or OK

    // wait for "OK\x0D"			
    if(strncmp ( "OK", (const char *)&Modem_Rx_Data[1][0], 2 ) == 0)
    {
      Modem_Retries = 2;
      Modem_Tickstart = timerB_get_tick();
      Modem_Timeout = modem_timeout[MODEM_ECHO_OFF_OK];
      Modem_State = MODEM_FLOW_C;
      break;
    }
    else
    {
      if(Modem_Retries) MODEM_Back(2);
      else MODEM_Error();
    }
    break;
    
    case MODEM_FLOW_C:
			if(!modem_timer_expired()) break;
			clr_rx_data();
			send_string_UART_0("AT&K3\x0D");
      Modem_Tickstart = timerB_get_tick();
      Modem_Timeout = modem_timeout[MODEM_FLOW_C];
			Modem_State++;
			break;
      
      case MODEM_KSLEEP:
			if(!modem_timer_expired()) break;
			clr_rx_data();
			send_string_UART_0("AT+KSLEEP=2\x0D");
      Modem_Tickstart = timerB_get_tick();
      Modem_Timeout = modem_timeout[MODEM_KSLEEP];
			Modem_State++;
			break;   

  case MODEM_KVGT:
			if(!modem_timer_expired()) break;
			clr_rx_data();
			send_string_UART_0("AT+KVGT=4\x0D");
      Modem_Tickstart = timerB_get_tick();
      Modem_Timeout = modem_timeout[MODEM_KVGT];
			Modem_State++;
			break;
      
  case MODEM_VGT:
			if(!modem_timer_expired()) break;
			clr_rx_data();
			send_string_UART_0("AT+VGT=140\x0D");
      Modem_Tickstart = timerB_get_tick();
      Modem_Timeout = modem_timeout[MODEM_VGT];
			Modem_State++;
			break;
    
    //------------------------------------------------------
  case MODEM_INIT_GPIO4:
    if(!modem_timer_expired()) break;
    clr_rx_data();
    send_string_UART_0("AT+KGPIOCFG=5,0,2\x0D"); // Set GPIO4 as an output, idle = 0 (PLAY)
    Modem_Tickstart = timerB_get_tick();
    Modem_Timeout = modem_timeout[MODEM_INIT_GPIO4];
    Modem_State++;
    break;
    
  case MODEM_INIT2_GPIO4:
    if(!modem_timer_expired()) break;
    clr_rx_data();
    send_string_UART_0("AT+KGPIO=5,0\x0D"); // Set GPIO4 as an output, idle = 0 (PLAY)
    Modem_Tickstart = timerB_get_tick();
    Modem_Timeout = modem_timeout[MODEM_INIT2_GPIO4];
    Modem_State++;
    break;

  case MODEM_INIT_GPIO1:
    if(!modem_timer_expired()) break;
    clr_rx_data();
    send_string_UART_0("AT+KGPIOCFG=4,1,1\x0D"); // Set GPIO1 as an input, idle = 1 (RDY)
    Modem_Tickstart = timerB_get_tick();
    Modem_Timeout = modem_timeout[MODEM_INIT_GPIO1];
    Modem_State++;
    break;

  case MODEM_INIT_GPIO2:
    if(!modem_timer_expired()) break;
    clr_rx_data();
    send_string_UART_0("AT+KGPIOCFG=15,0,2\x0D"); // Set GPIO2 as an output, idle = 0 (RESET)
    Modem_Tickstart = timerB_get_tick();
    Modem_Timeout = modem_timeout[MODEM_INIT_GPIO2];
    Modem_State++;
    break;
    
  case MODEM_INIT2_GPIO2:
    if(!modem_timer_expired()) break;
    clr_rx_data();
    send_string_UART_0("AT+KGPIO=15,0\x0D"); // Set GPIO2 as an output, idle = 0 (RESET)
    Modem_Tickstart = timerB_get_tick();
    Modem_Timeout = modem_timeout[MODEM_INIT2_GPIO2];
    Modem_State++;
    break;

  case MODEM_INIT_GPIO3:
    if(!modem_timer_expired()) break;
    clr_rx_data();
    send_string_UART_0("AT+KGPIOCFG=1,0,2\x0D"); // Set GPIO3 as an output, idle = 0 (FWD)
    Modem_Tickstart = timerB_get_tick();
    Modem_Timeout = modem_timeout[MODEM_INIT_GPIO3];
    Modem_State++;
    break;
    
  case MODEM_INIT2_GPIO3:
    if(!modem_timer_expired()) break;
    clr_rx_data();
    send_string_UART_0("AT+KGPIO=1,0\x0D"); // Set GPIO3 as an output, idle = 0 (FWD)
    Modem_Tickstart = timerB_get_tick();
    Modem_Timeout = modem_timeout[MODEM_INIT2_GPIO3];
    Modem_State++;
    break;

    //------------------------------------------------------
  /*case MODEM_AUTOBAND:
    if(!modem_timer_expired()) break;
    clr_rx_data();
    send_string_UART_0("AT#AUTOBND=0\x0D");
    Modem_Tickstart = timerB_get_tick();
    Modem_Timeout = modem_timeout[MODEM_AUTOBAND];
    Modem_State++;
    break;

  case MODEM_BAND:
    if(!modem_timer_expired()) break;
    clr_rx_data();
    send_string_UART_0("AT#BND=3\x0D");
    Modem_Tickstart = timerB_get_tick();
    Modem_Timeout = modem_timeout[MODEM_BAND];
    Modem_State++;
    break;*/

  case MODEM_REG_MODE:
    if(!modem_timer_expired()) break;
    clr_rx_data();
    send_string_UART_0("AT+COPS=0\x0D");
    Modem_Tickstart = timerB_get_tick();
    Modem_Timeout = modem_timeout[MODEM_REG_MODE];
    Modem_State++;
    break;

  case MODEM_MODE:
    if(!modem_timer_expired()) break;
    clr_rx_data();
    send_string_UART_0("AT+CMGF=1\x0D");
    Modem_Tickstart = timerB_get_tick();
    Modem_Timeout = modem_timeout[MODEM_MODE];
    Modem_State++;
    break; 

    //------------------------------------------------------
  case MODEM_EVENT:
    if(!modem_timer_expired()) break;
    clr_rx_data();
    send_string_UART_0("AT+CNMI=2,2,0,0,0\x0D");// Incoming SMSs are sent
    Modem_Tickstart = timerB_get_tick();
    Modem_Timeout = modem_timeout[MODEM_EVENT];
    Modem_State++;
    break;

    //------------------------------------------------------
  case MODEM_INIT_GPIO4_OK:
  case MODEM_INIT2_GPIO4_OK:
  case MODEM_INIT_GPIO1_OK:
  case MODEM_INIT_GPIO2_OK:
  case MODEM_INIT2_GPIO2_OK:
  case MODEM_INIT_GPIO3_OK:
  case MODEM_INIT2_GPIO3_OK:
  //case MODEM_AUTOBAND_OK:
  //case MODEM_BAND_OK:
  case MODEM_REG_MODE_OK:
  case MODEM_MODE_OK:
  case MODEM_FLOW_C_OK:
  case MODEM_KSLEEP_OK:
  case MODEM_KVGT_OK:
  case MODEM_VGT_OK:
  case MODEM_EVENT_OK:
  //case VOICE_GAIN_OK:
  //case VOICE_TYPE_OK:
  //case VOICE_MODE_OK:
  //case MODEM_FUN_OK:
  case VOICE_PLAY_OK:
  case VOICE_PLAY_2_OK:
    if(!send_finished_UART_0()) break;
    if(modem_timer_expired())
    {
      if(Modem_Retries) MODEM_Back(1);
      else MODEM_Error();
      break;
    }
    if(!modem_read_data()) break;
    // wait for "OK\x0D\x0A"
    if(strncmp ( "OK", (const char *)&Modem_Rx_Data[0][0], 2 ) == 0)
    {
      Modem_Retries = 2;
      Modem_Tickstart = timerB_get_tick();
      Modem_Timeout = modem_timeout[Modem_State];
      Modem_State++;
      break;
    }
    else
    {
      if(Modem_Retries) MODEM_Back(1);
      else MODEM_Error();
    }
    break;
    //------------------------------------------------------
 /* case VOICE_GAIN:
    if(!modem_timer_expired()) break;
    clr_rx_data();
    send_string_UART_0("AT#HFMICG=2\x0D");
    Modem_Tickstart = timerB_get_tick();
    Modem_Timeout = modem_timeout[VOICE_GAIN];
    Modem_State++;
    break;

  case VOICE_TYPE:
    if(!modem_timer_expired()) break;
    clr_rx_data();
    send_string_UART_0("AT+FCLASS=8\x0D");
    Modem_Tickstart = timerB_get_tick();
    Modem_Timeout = modem_timeout[VOICE_TYPE];
    Modem_State++;
    break;

  case VOICE_MODE:
    if(!modem_timer_expired()) break;
    clr_rx_data();
    send_string_UART_0("AT#DIALMODE=2\x0D");
    Modem_Tickstart = timerB_get_tick();
    Modem_Timeout = modem_timeout[VOICE_MODE];
    Modem_State++;
    break;*/

    //------------------------------------------------------
  case MODEM_INIT_DONE:
    MODEM_Reg_Retries = MAX_REG_RETRIES;
    MODEM_Cmd = MODEM_CMD_NONE;
    MODEM_Res = MODEM_RES_OK;
    Modem_Timeout_Reset = TIME_RESET;
    Modem_State++;
    break;

    //***********************************************************************************
    //------------------------------------------------------
  case MODEM_CMD_WAIT:
    // Sender commands
    if(MODEM_Cmd == MODEM_CMD_SMS)
    {
      Modem_Status = MODEM_STAT_SMS_TXRX;
      Modem_Keep_Awake_Timer = MODEM_AWAKE_TIME;
      Modem_Tickstart = timerB_get_tick();
      Modem_Timeout = modem_timeout[MODEM_CMD_WAIT];
      Modem_Retries = 2;
      Modem_State = SMS;
      break;
    }

    if(MODEM_Cmd == MODEM_CMD_VOICE)
    {
      Modem_Status = MODEM_STAT_VOICE;
      Modem_Keep_Awake_Timer = MODEM_AWAKE_TIME;
      Modem_Tickstart = timerB_get_tick();
      Modem_Timeout = modem_timeout[MODEM_CMD_WAIT];
      Modem_Retries = 2;
      MODEM_Voice_Retries = DISCONNECTED_RETRIES;
      Modem_State = VOICE_DIAL;
      break;
    }

    // Periodic data request?
    if(Modem_Periodic_Data_Timer == 0)
    {
      Modem_Status = MODEM_STAT_CHECK;
      Modem_Periodic_Data_Timer = MODEM_PERIODIC_CHK_TIME;
      Modem_Keep_Awake_Timer = MODEM_AWAKE_TIME;
      Modem_Retries = 2;
      Modem_Tickstart = timerB_get_tick();
      Modem_Timeout = INTER_MSG;
      Modem_State = MODEM_REG;
      break;
    }

    // Sleep modem
    if(Modem_Keep_Awake_Timer == 0)
    {
      Modem_Status = MODEM_STAT_SLEEP;
      Modem_Retries = 2;
      Modem_Tickstart = timerB_get_tick();
      Modem_Timeout = INTER_MSG;
      Modem_State = MODEM_SLEEPING_WAIT;
      break;
    }
    
   

    Modem_Status = MODEM_STAT_IDLE;
    break;

        //------------------------------------------------------
 /* case MODEM_FUN:
    if(!modem_timer_expired()) break;
    uart_disable_RTS();
    clr_rx_data();
    send_string_UART_0("AT+CFUN=9\x0D");
    Modem_Tickstart = timerB_get_tick();
    Modem_Timeout = modem_timeout[MODEM_FUN];
    Modem_State++;
    break;*/

    //------------------------------------------------------
  case MODEM_SLEEPING_WAIT:
    Flag_Main_Uart0_LP_T10ms_off = 1; // Low power allowed

    // Periodic data request?
    if((Modem_Periodic_Data_Timer == 0)
    // Command?
    || (MODEM_Cmd != MODEM_CMD_NONE))
    {
      Flag_Main_Uart0_LP_T10ms_off = 0; // Low power not allowed
      Modem_State++;
    }
    // SMS received?
    else if(new_sms())
    {
      Flag_Main_Uart0_LP_T10ms_off = 0; // Low power not allowed
      //uart_enable_RTS();
      Modem_State = MODEM_CMD_WAIT;
    }
    
     if(Modem_Timeout_Reset == 0)
    {
      Modem_Status = MODEM_STAT_IDLE;
      Modem_Retries = 2;
      Modem_Tickstart = timerB_get_tick();
      Modem_Timeout = INTER_MSG;
      Modem_State = MODEM_RESET_0;
      break;
    }
    
    break;

    //------------------------------------------------------
  case MODEM_WAKE_0:
    //U0_RTS = 1;
    Modem_Tickstart = timerB_get_tick();
    Modem_Timeout = WAKE_PULSE_TIME;
    Modem_State++;
    break;

  case MODEM_WAKE_1:
    if(!modem_timer_expired()) break;
    //U0_RTS = 0;
    //uart_enable_RTS();
    Modem_State = MODEM_CMD_WAIT;
    break;
    
    //------------------------------------------------------
  case SMS:
    if(!send_finished_UART_0()) break; // needed after sending 
    if(!modem_timer_expired()) break;
    clr_rx_data();

    clr_data_UART_0();
    cue_data_UART_0("AT+CMGS=\"", 9);
    cue_string_UART_0((uint8_t *)Message.Phone);
    cue_data_UART_0("\"\x0D", 2);
    send_data_UART_0();
    Modem_Tickstart = timerB_get_tick();
    Modem_Timeout = modem_timeout[SMS];
    Modem_State++;
    break;

  case SMS_PROMPT:
    if(!send_finished_UART_0()) break;

    if(modem_timer_expired())
    {
      if(Modem_Retries)
      {
        //send_string_UART_0("\x1B");
        //Modem_Timeout = INTER_MSG;
        MODEM_Back(1);
      }
      else MODEM_Error();
      break;
    }
    if(!modem_read_data()) break;
    // wait for "> "
    if(Modem_Rx_Data[0][0] == '>' && Modem_Rx_Data[0][1] == ' ')
    {
      clr_rx_data();

      // Append 0x1A
      strcat((char *)Message.Text,"\x1A");
      send_data_UART_0_from_ptr(Message.Text, strlen((char *)Message.Text));

      Modem_Tickstart = timerB_get_tick();
      Modem_Timeout = modem_timeout[SMS_PROMPT];
      Modem_State++;
    }
    /*else
    { 
      if(Modem_Retries)
      {
        send_string_UART_0("\x1B");
        Modem_Timeout = INTER_MSG;
        MODEM_Back(1);
      }
      else MODEM_Error();
    }*/
    break;

    //------------------------------------------------------
  case SMS_STORE_LOCATION:
    if(!send_finished_UART_0()) break;
    
    if(modem_timer_expired())
    {
      if(Modem_Retries) MODEM_Back(2);
      else MODEM_Error();
      break;
    }
    if(!modem_read_data()) break;
    // wait for "+CMGS: n\x0D\x0A"
    if((strncmp ( "+CMGS:", (const char *)&Modem_Rx_Data[0][0], 6 ) == 0) ||(strncmp ( "OK", (const char *)&Modem_Rx_Data[0][0], 2 ) == 0))
    {
      Modem_State++;
      break;
    }			
    else
    {
      if(Modem_Retries) MODEM_Back(2);
      else MODEM_Error();
    }
    break;

  case SMS_OK:
    if(!send_finished_UART_0()) break;
    
    if(modem_timer_expired())
    {
      if(Modem_Retries) MODEM_Back(3);
      else MODEM_Error();
      break;
    }
    if(!modem_read_data_2()) break;

    // wait for "OK\x0D"
    if(strncmp ( "OK", (const char *)&Modem_Rx_Data[1][0], 2 ) == 0)
    {
      MODEM_Cmd = MODEM_CMD_NONE;
      MODEM_Res = MODEM_RES_OK;
      Modem_State = MODEM_CMD_WAIT;
    }			
    else
    {
      if(Modem_Retries) MODEM_Back(3);
      else MODEM_Error();
    }
    break;

  //------------------------------------------------------
  case VOICE_DIAL:
    if(!modem_timer_expired()) break;

    if(MODEM_Voice_Retries == 0)
    {
      MODEM_Cmd = MODEM_CMD_NONE;
      MODEM_Res = MODEM_RES_ERR;
      Modem_State = MODEM_CMD_WAIT;
      break;
    }
    MODEM_Voice_Retries--;
    MODEM_Aux = 0;// Enable retries when "DISCONNECTED" is received just after dialing
    MODEM_Voice_Call_Status = VOICE_STATE_NONE;
    
    clr_rx_data();
    clr_data_UART_0();
    cue_data_UART_0("ATD",3);
    cue_string_UART_0((uint8_t *)Message.Phone);
    cue_data_UART_0(";\x0D",2);
    send_data_UART_0();
    Modem_Tickstart = timerB_get_tick();
    Modem_Timeout = modem_timeout[VOICE_DIAL];
    Modem_State++;
    break;

  case VOICE_STATUS:
    if(!send_finished_UART_0()) break;
    //if(modem_timer_expired())
    //{	// Timeout?
     // Modem_State = VOICE_DIAL;
      //break;
    //}

    /*
			DIALING (MO in progress)
			RINGING (remote ring)
			CONNECTED (remote call accepted)
			RELEASED (after ATH)
			DISCONNECTED (remote hang-up)
			Note: In case a BUSY tone is received and at the same time ATX0
			will return NO CARRIER instead of DISCONNECTED.
     */
    switch(MODEM_Voice_Call_Status)
    {
      case VOICE_STATE_DIALING:
        MODEM_Voice_Call_Status = VOICE_STATE_DIALING2;
        Modem_Tickstart = timerB_get_tick();
        Modem_Timeout = INTER_MSG*20;
        break;

    case VOICE_STATE_DIALING2:
      if(modem_timer_expired())
      {
        MODEM_Voice_Call_Status = VOICE_STATE_CONNECTED;
      }          
      break;
      case VOICE_STATE_RINGING:
        MODEM_Aux = 1; // User can hang, "DISCONNECTED" enabled
        break;

      case VOICE_STATE_CONNECTED:
        MODEM_Voice_Call_Status = VOICE_STATE_NONE;
        Modem_Tickstart = timerB_get_tick();
        Modem_Timeout = INTER_MSG*2;
        Modem_State = VOICE_PLAY;
        break;

      case VOICE_STATE_RELEASED:
        MODEM_Voice_Call_Status = VOICE_STATE_NONE;
        MODEM_Cmd = MODEM_CMD_NONE;
        MODEM_Res = MODEM_RES_OK;
        Modem_State = MODEM_CMD_WAIT;
        break;

      case VOICE_STATE_DISCONNECTED:
        if(MODEM_Aux)
        {
          MODEM_Voice_Call_Status = VOICE_STATE_NONE;
          MODEM_Cmd = MODEM_CMD_NONE;
          MODEM_Res = MODEM_RES_OK;
          Modem_State = MODEM_CMD_WAIT;
        }
        else
        {
          Modem_Tickstart = timerB_get_tick();
          Modem_Timeout = INTER_MSG;
          Modem_State = VOICE_DIAL;
        }
        break;

      case VOICE_STATE_NO_CARRIER:
        Modem_Tickstart = timerB_get_tick();
        Modem_Timeout = INTER_MSG;
        MODEM_Voice_Call_Status = VOICE_STATE_NONE;
        //Modem_State = VOICE_DIAL;
        MODEM_Cmd = MODEM_CMD_NONE;
        MODEM_Res = MODEM_RES_OK;
        Modem_State = MODEM_CMD_WAIT;
        break;
    }
    break;

    case VOICE_PLAY:
      if(!modem_timer_expired()) break;
      clr_rx_data();
      send_string_UART_0("AT+KGPIO=5,1\x0D"); // Set GPIO4 = 1 (PLAY)
      Modem_Tickstart = timerB_get_tick();
      Modem_Timeout = modem_timeout[VOICE_PLAY];
      Modem_State++;
      break;

    case VOICE_PLAY_2:
      if(!modem_timer_expired()) break;
      clr_rx_data();
      send_string_UART_0("AT+KGPIO=5,0\x0D"); // Set GPIO4 = 0 (STOP PLAY)
      Modem_Tickstart = timerB_get_tick();
      Modem_Timeout = modem_timeout[VOICE_PLAY_2];
      Modem_State++;
      break;

    case VOICE_READY:
      if(!modem_timer_expired()) break;
      clr_rx_data();
      send_string_UART_0("AT+KGPIO=4,2\x0D"); // Read GPIO1 (RDY)
      Modem_Tickstart = timerB_get_tick();
      Modem_Timeout = modem_timeout[VOICE_READY];
      Modem_State++;
      break;

    case VOICE_READY_2:
      if(!send_finished_UART_0()) break;
      if(modem_timer_expired())
      {
        if(Modem_Retries) MODEM_Back(1);
        else MODEM_Error();
        break;
      }

      if(!modem_read_data())
        break;

      // wait for "#GPIO: 0,1\x0D\x0A"
      if(strncmp ( "+KGPIO:", (const char *)&Modem_Rx_Data[0][0], 7 ) == 0)
      {
        Modem_State++;
        break;
      }

      if(Modem_Retries) MODEM_Back(1);
      else MODEM_Error();
      break;
      
    case VOICE_READY_2_OK:
      if(!send_finished_UART_0()) break;
      if(modem_timer_expired())
      {
        if(Modem_Retries) MODEM_Back(2);
        else MODEM_Error();
        break;
      }
      // wait for "OK\x0D\x0A"
      if(!modem_read_data_2())
        break;

      // OK?
      if(strncmp ( "OK", (const char *)&Modem_Rx_Data[1][0], 2 ) == 0)
      {
        // Done?
        if(Modem_Rx_Data[0][10]=='1')
        {
          Modem_Retries = 2;
          Modem_State++;
        }
        else
        {
          Modem_State = VOICE_READY;
        }
        Modem_Tickstart = timerB_get_tick();
        Modem_Timeout = INTER_MSG;
        break;
      }

      if(Modem_Retries) MODEM_Back(2);
      else MODEM_Error();
      break;

    case VOICE_HANG:
      if(!modem_timer_expired()) break;
      clr_rx_data();
      send_string_UART_0("AT+CHUP\x0D");
      Modem_Tickstart = timerB_get_tick();
      Modem_Timeout = modem_timeout[VOICE_HANG];
      MODEM_Voice_Call_Status = VOICE_STATE_NONE;
      MODEM_Cmd = MODEM_CMD_NONE;
      MODEM_Res = MODEM_RES_OK;
      Modem_State = MODEM_CMD_WAIT;
      break;

      //------------------------------------------------------
    case MODEM_REG:
      if(!modem_timer_expired()) break;
      clr_rx_data();
      send_string_UART_0("AT+CREG?\x0D");
      Modem_Tickstart = timerB_get_tick();
      Modem_Timeout = modem_timeout[MODEM_REG];
      Modem_State++;
      break;

    case MODEM_REG_RESPONSE:
      if(!send_finished_UART_0()) break;
      if(modem_timer_expired())
      {
        if(Modem_Retries) MODEM_Back(1);
        else MODEM_Error();
        break;
      }
      if(!modem_read_data()) break;
      // wait for "+CREG: 0,1\x0D\x0A"
      if((Modem_Rx_Data[0][0]=='+') && (Modem_Rx_Data[0][4]=='G'))
      {
        // '0'&'2' wait, '3' error, '1' || '5'  ok
        if(Modem_Rx_Data[0][9] == '1' || Modem_Rx_Data[0][9] == '5')
        {
          MODEM_Reg_Retries = MAX_REG_RETRIES;
          Modem_State++;
        }
        else
        {
          if(MODEM_Reg_Retries == 0)
          {
            MODEM_Error();
            break;
          }
          else
          {
            MODEM_Reg_Retries--;
            Modem_Periodic_Data_Timer = MODEM_PERIODIC_NOT_REG_TIME;
            Modem_State = MODEM_PERIODIC_END;
          }
        }

        Modem_Tickstart = timerB_get_tick();
        Modem_Timeout = modem_timeout[MODEM_REG_RESPONSE];
      }
      else
      {
        if(Modem_Retries) MODEM_Back(1);
        else MODEM_Error();
      }
      break;

    case MODEM_REG_OK:
    case MODEM_SIG_OK:
      if(!send_finished_UART_0()) break;
      if(modem_timer_expired())
      {
        if(Modem_Retries) MODEM_Back(2);
        else MODEM_Error();
        break;
      }
      if(!modem_read_data_2()) break;
      // wait for "OK\x0D\x0A"
      if(strncmp ( "OK", (const char *)&Modem_Rx_Data[1][0], 2 ) == 0)
      {
        Modem_Tickstart = timerB_get_tick();
        Modem_Timeout = INTER_MSG;
        Modem_State++;
      }
      else
      {
        if(Modem_Retries) MODEM_Back(2);
        else MODEM_Error();
      }
      break;
      
      //------------------------------------------------------
    case MODEM_SIG:
      if(!modem_timer_expired()) break;
      clr_rx_data();
      Modem_Tickstart = timerB_get_tick();
      Modem_Timeout = modem_timeout[MODEM_SIG];
      send_string_UART_0("AT+CSQ\x0D");
      Modem_State++;
      break;

    case MODEM_SIG_RESPONSE:
      if(!send_finished_UART_0()) break;
      if(modem_timer_expired())
      {
        if(Modem_Retries) MODEM_Back(1);
        else MODEM_Error();
        break;
      }
      if(!modem_read_data()) break;
      // wait for "+CSQ: 17,0\x0D\x0A"
      if((Modem_Rx_Data[0][0]=='+') && (Modem_Rx_Data[0][3]=='Q'))
      {
        if(Modem_Rx_Data[0][7]==',')
        {
          MODEM_Signal[0] = '0';
          MODEM_Signal[1] = Modem_Rx_Data[0][6];
        }
        else
        {
          MODEM_Signal[0] = Modem_Rx_Data[0][6];
          MODEM_Signal[1] = Modem_Rx_Data[0][7];
          if(MODEM_Signal[0]=='9' && MODEM_Signal[1]=='9') clr_signal();
        }
        MODEM_Signal[2] = 0x00;
        Modem_Tickstart = timerB_get_tick();
        Modem_Timeout = modem_timeout[MODEM_SIG_RESPONSE];
        Modem_State++;
      }
      else
      {
        if(Modem_Retries) MODEM_Back(1);
        else MODEM_Error();
      }
      break;

  case MODEM_SMS_DEL_ALL:
      if(!modem_timer_expired()) break;
      clr_rx_data();
      Modem_Tickstart = timerB_get_tick();
      Modem_Timeout = modem_timeout[MODEM_SMS_DEL_ALL];
      send_string_UART_0("AT+CMGD=1,4\x0D");
      Modem_State++;
      break;
 
      case MODEM_SMS_DEL_ALL_OK:
      if(!send_finished_UART_0()) break;
      if(modem_timer_expired())
      {
        if(Modem_Retries) MODEM_Back(1);
        else MODEM_Error();
        break;
      }
      if(!modem_read_data()) break;
      // wait for "OK\x0D\x0A"
      if(strncmp ( "OK", (const char *)&Modem_Rx_Data[0][0], 2 ) == 0)
      {
        Modem_Retries = 2;
        Modem_Tickstart = timerB_get_tick();
        Modem_Timeout = INTER_MSG;
        Modem_State++;
      }
      else
      {
        if(Modem_Retries) MODEM_Back(1);
        else MODEM_Error();
      }
      break;
      
      //------------------------------------------------------
    case  MODEM_PERIODIC_END:
      Modem_State = MODEM_CMD_WAIT;
      break;

      //-----------------------------------------------------
    case MODEM_RESET_0:
      Modem_Tickstart = timerB_get_tick();
      Modem_Timeout = modem_timeout[MODEM_RESET_0];
      //send_string_UART_0("ATZ0\x0D");
      PIN_ENABLE = 0;
      //MODEM_ENA = 0;
      Stop_UART_0();
      Modem_State++;
      break;

    case MODEM_RESET_1:
      //if(!send_finished_UART_0()) break;
      if(!modem_timer_expired()) break;        
      Modem_State = MODEM_INIT_0;
      break;

      //------------------------------------------------------
    default:
      Modem_State = MODEM_INIT_0;
  }
}

