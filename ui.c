#include <intrinsics.h>
#include <string.h>
#include "def.h"
#include "ui.h"
#include "lcd.h"
#include "button.h"
#include "flash.h"
#include "main.h"
#include "monitor.h"
#include "rtc.h"
#include "adc.h"
#include "sender.h"
#include "modem.h"

// Local Variables
char Ui_State;
volatile char Ui_Timer;
volatile char Ui_Sleep = T_LOW_POWER;
char Ui_Buffer[PASS_SIZE]; // Password (PW_SIZE) or Clock (6) 
char Ui_Index;
char Ui_Disp_Buffer[15]; // Display + NULL size
char Ui_Menu_Index;
char Ui_Config_Var;

const char flash_to_pass[] = {'-','+'};
const char master_pass[] = {0,0,1,0,1,0,1,0}; //42

//******************************************************************************
void ui_10ms_tick(void)
{
	if(Ui_Timer) Ui_Timer--;
}
//******************************************************************************
void ui_1s_tick(void)
{
	if(Ui_Sleep) Ui_Sleep--;
	else process_buttons();						// Refresh switches here too (used to leave the low power mode)
}
//******************************************************************************
void var_to_ascii(uint8_t var, uint8_t aux_buffer[])
{
	switch(var)
	{
		case DISP_P0:
			aux_buffer[0] = 'E';
			aux_buffer[1] = ':';
			// Format: 0000EEEEEEE,FFFFF
			binary_to_ascii(Sensors_Samples[0], (char *)&aux_buffer[2], read_flash(ID_Dec_0)); // Careful, writes 8 bytes!
			aux_buffer[8] = 'b';
			aux_buffer[9] = 0X00;
			break;

		case DISP_P1:
			aux_buffer[0] = 'S';
			aux_buffer[1] = ':';
			// Format: 0000EEEEEEE,FFFFF
			binary_to_ascii(Sensors_Samples[1], (char *)&aux_buffer[2], read_flash(ID_Dec_1)); // Careful, writes 8 bytes!
			aux_buffer[8] = 'b';
			aux_buffer[9] = 0X00;
			break;

		case DISP_BATT:
			// Format: 0000EEEEE,FFFFFFF
			binary_to_ascii(Battery, (char *)&aux_buffer[2], 7); // Careful, writes 8 bytes!
			aux_buffer[0] = 'B';
			aux_buffer[1] = ':';
			aux_buffer[6] = 'V';
			aux_buffer[7] = 0X00;
			break;
	}
}
//******************************************************************************
void display_var(char var, char line, char x, char size)
{
	char aux_buffer[15]; // This buffer is needed to avoid overwritting Ui_Disp_Buffer

	var_to_ascii((uint8_t)var, (uint8_t *)aux_buffer);
	if(size) LCDStr_2(x, line, aux_buffer);
	else LCDStr(x, line, aux_buffer);
}
//******************************************************************************
void ui_init(void)
{
	Flag_Main_Ui_Var_Changed = 0;
	Flag_Main_Logged_In = 0;
	Ui_State = 0;
}
//------------------------------------------------------------------------------
void phone_flash_to_ascii(uint8_t *dataout, uint8_t flash_id)
{
	char i,j;
	short aux;
	const char rot[]={12,8,4,0};

	for(i=0; i<=2; i++)
	{
		aux = read_flash(flash_id + i);
		for(j=0; j<=3; j++)
		{
			*dataout = 0x30 + ((char)(aux >> rot[j]) & 0X000F);
			if(*dataout > 0x39) *dataout = 0x00;
			dataout++;
		}
	}
}
//------------------------------------------------------------------------------
void phone_ascii_to_flash(char * datain, char flash_id)
{
	char i,j;
	short aux;
	const char rot[]={12,8,4,0};

	// Remove spaces
	for(i=0; i<CELL_SIZE;)
	{
		if(*(datain+i) == 0x00) break;

		if(*(datain+i) == ' ')
		{
			for(j=i; j<CELL_SIZE; j++)
			{
				*(datain+j) = *(datain+j+1);
			}
		}
		else i++;
	}

	// ASCII to flash
	for(i=0; i<=2; i++)
	{
		aux = 0;
		for(j=0; j<=3; j++)
		{
			if(*datain == 0x00)
				aux |= (0x000F << rot[j]);
			else
				aux |= (((short)(*datain & 0x0F)) << rot[j]);
			datain++;
		}
		write_flash(flash_id + i, aux);
	}
}
//******************************************************************************
void validate_phone(char data[])
{
	char i;

	for(i=0; i<CELL_SIZE; i++)
	{
		if((data[i] < '0') || (data[i] > '9')) data[i] = ' ';
	}
}
//******************************************************************************
void ui(void)
{
	// Low power mode ----------------------------------------------------------
	if(Ui_Sleep == 0)
	{
		Flag_Main_Logged_In = 0;

		// Turn display off if it was on
		if(Flag_LCD_State)
		{
			LCDClearLines(L4,L5);
			LCDSleep();
			Flag_LCD_State = 0;
		}

		// Exit Low Power?
		if(!Flag_Button_M)
		{	// Yes
			Flag_Main_Ui_Var_Changed = 0;
			Flag_Main_Logged_In = 0;
			Ui_State = 0;	
			Ui_Sleep = T_LOW_POWER;	// Reload Low Power timer
		}

		return;
	}


	// Normal mode -------------------------------------------------------------
	// Any button pressed?
	if((Flags_Buttons.Byte & BUTTON_MASK) != BUTTON_MASK) Ui_Sleep = T_LOW_POWER;	// Reload low power timer

	// 250ms tick
	if(Ui_Timer == 0)
	{
		Ui_Timer = 25; // 25*10ms=250ms

		// Turn display on it was off
		if(!Flag_LCD_State)
		{
			LCDWakeup();
			Flag_LCD_State = 1;
			Flag_LCD_Update = 1;
		}

		// Not in configuration?
		if((Ui_State==UI_MAIN_BUTTON_A) || (Ui_State==UI_MAIN_BUTTON_B))
		{
			// Display P0 pressure
			display_var(DISP_P0, L0, 0, LARGE);
			// P0 Status line
			if(Flag_Monitor_Alarm_Pending_In_H) LCDChrXY(13, L0, 'H');
			else if(Flag_Monitor_Alarm_Status_In_H) LCDChrXY(13, L0, 'h');
			else LCDChrXY(13, L0, ' ');
	
			if(Flag_Monitor_Alarm_Pending_In_L) LCDChrXY(13, L1, 'L');
			else if(Flag_Monitor_Alarm_Status_In_L) LCDChrXY(13, L1, 'l');
			else LCDChrXY(13, L1, ' ');
	
			// Display P1 pressure
			display_var(DISP_P1, L2, 0, LARGE);
			// P1 Status line
			if(Flag_Monitor_Alarm_Pending_Out_H) LCDChrXY(13, L2, 'H');
			else if(Flag_Monitor_Alarm_Status_Out_H) LCDChrXY(13, L2, 'h');
			else LCDChrXY(13, L2, ' ');
	
			if(Flag_Monitor_Alarm_Pending_Out_L) LCDChrXY(13, L3, 'L');
			else if(Flag_Monitor_Alarm_Status_Out_L) LCDChrXY(13, L3, 'l');
			else LCDChrXY(13, L3, ' ');

			LCDClearLines(L4, L5);
			// Status line
			// Alarm number
			LCDStr(0, L4, (char *)alarm_to_ascii());

			// Modem
      // Status & signal strength
      switch(modem_status())
      {
        case MODEM_STAT_INI:
          LCDStr(11, L4, "Ini" );
          break;

        case MODEM_STAT_IDLE:
          LCDStr(11, L4, "Idl" );
          break;

        case MODEM_STAT_CHECK:
          LCDStr(11, L4, "Chk" );
          break;

        case MODEM_STAT_SLEEP:
          LCDStr(11, L4, "Slp" );
          break;

        case MODEM_STAT_SMS_TXRX:
          LCDStr(11, L4, "Sms" );
          break;
          
        case MODEM_STAT_VOICE:
          LCDStr(11, L4, "Voz" );
          break;
      }

      LCDStr(8, L4, (char *)MODEM_Signal);

			if(Global_Date[SEC] & 0x08)
			{
				// Display Batt 
				display_var(DISP_BATT, L5, 0, SMALL);

				// Modbus state
				if(read_flash(ID_Mod_On)) LCDChrXY(13, L5, 'M');
				else LCDChrXY(13, L5, ' ');
			}
			else
			{
				// Display Clock
				clock_to_ascii((uint8_t *)Ui_Disp_Buffer, (uint8_t *)Global_Date);
				LCDStr(0, L5, Ui_Disp_Buffer);
			}

		}
		else
		{
			// Display large
			LCDClearLines(L0, L3);
      // Show release
      if((Ui_State==UI_LOGIN_BUTTON_A)
      || (Ui_State==UI_LOGIN_BUTTON_B)
      || (Ui_State==UI_ENTER_BUTTON_A)
      || (Ui_State==UI_ENTER_BUTTON_B))
      {
        LCDStr(0, L0, "MEP7000 4G" );
        LCDStr(0, L1, "Camuzzi v4.5" );
      }
			// Conf. Display pressure 0
			else if((Ui_Config_Var == ID_Offset_0) || (Ui_Config_Var == ID_Span_0)) display_var(DISP_P0, L0, 0, LARGE);
			// Conf. Display pressure 1
			else if((Ui_Config_Var == ID_Offset_1) || (Ui_Config_Var == ID_Span_1)) display_var(DISP_P1, L0, 0, LARGE);
			// Conf. Display battery
			else if((Ui_Config_Var == ID_Offset_Bat) || (Ui_Config_Var == ID_Span_Bat)) display_var(DISP_BATT, L0, 0, LARGE);
		}
	}

	switch(Ui_State)
	{
		//-------------------------------------------------------------------
		case UI_MAIN_BUTTON_B:
			Flag_Main_Config = 0;
			if(!Flag_Button_M)
			{	// Logged in?
				if(Flag_Main_Logged_In)
				{
					Flag_Main_Config = 1;
					Ui_Menu_Index = 0;
					Ui_State = UI_ENTER_SCREEN;
				}
				else
				{
					// Clear password
					for(Ui_Index = 0; Ui_Index < PASS_SIZE; Ui_Index++)
						Ui_Buffer[Ui_Index] = 0x00;
					Ui_Index=0;
					LCDStr(0, L4, "Ingrese clave " );
					LCDStr(0, L5, "              " );
					Ui_State = UI_LOGIN_BUTTON_A;
				}
				break;
			}
			break;

		//-------------------------------------------------------------------
		case UI_LOGIN_BUTTON_B:
			if((!Flag_Button_I)
			&& (Ui_Index < PASS_SIZE))
			{
				Ui_Buffer[Ui_Index] = 1;
				LCDChrXY (Ui_Index, L5, '*');
				Flag_LCD_Update = 1;
				Ui_Index++;
				Ui_State--;
				break;
			}

			if((!Flag_Button_D)
			&& (Ui_Index < PASS_SIZE))
			{
				Ui_Buffer[Ui_Index] = 0;
				LCDChrXY (Ui_Index, L5, '*');
				Flag_LCD_Update = 1;
				Ui_Index++;
				Ui_State--;
				break;
			}

			if(!Flag_Button_M)
			{
				// Password ok?
				for(Ui_Index = 0; Ui_Index < PASS_SIZE; Ui_Index++)				
				{
					if(Ui_Buffer[Ui_Index] != (char)read_flash(ID_Pass_0 + Ui_Index)) break;
				}

				if(Ui_Index == PASS_SIZE)
				{
					Flag_Main_Config = 1;
					Flag_Main_Logged_In = 1;			// Logged in
					Flag_Main_Master_Pass = 0;
					Ui_Menu_Index = 0;
					Ui_State = UI_ENTER_SCREEN;
					break;
				}

				// Master Password ok?
				for(Ui_Index = 0; Ui_Index < PASS_SIZE; Ui_Index++)
				{
					if(Ui_Buffer[Ui_Index] != master_pass[Ui_Index]) break;
				}

				if(Ui_Index == PASS_SIZE)
				{
					Flag_Main_Config = 1;
					Flag_Main_Logged_In = 1;			// Logged in
					Flag_Main_Master_Pass = 1;
					Ui_Menu_Index = 0;
					Ui_State = UI_ENTER_SCREEN;
					break;
				}

				Ui_State = UI_MAIN_BUTTON_A;
				break;
			}

			break;
		//-------------------------------------------------------------------
		case UI_ENTER_SCREEN:

			switch(Ui_Menu_Index)
			{
				case MS_ALARM:	LCDStr(0, L4, "Alarmas       "); break;
				case MS_SYS:	LCDStr(0, L4, "Sistema       "); break;
				case MS_CELL:	LCDStr(0, L4, "Celular       "); break;
				case MS_COM:	LCDStr(0, L4, "Modbus        "); break;
				case MS_SENSOR:	LCDStr(0, L4, "Transductores "); break;
			}
			LCDStr(0, L5, "              ");
			Ui_State++;
			break;

		//-------------------------------------------------------------------
		case UI_ENTER_BUTTON_B:
			// Menu button?
			if(!Flag_Button_M)
			{
				LCDClearLines(L4,L5);
				Ui_Menu_Index++;
				if(Ui_Menu_Index > MS_SENSOR)
				{
					LCDClearLines(L4,L5);
					Ui_Config_Var = ID_T_Mask; // Ui_Config_Var needs to be != Offset_Bat, Span_Bat, ... for the display
					Ui_State = UI_MAIN_BUTTON_A;
				}
				else Ui_State = UI_ENTER_SCREEN;
				break;
			}

			// Enter button?
			if(!Flag_Button_R)
			{
				LCDClearLines(L4,L5);

				switch(Ui_Menu_Index)
				{
					case MS_ALARM:
						Ui_State = ALARM_IN_H_0;
						break;

					case MS_SYS:
						Ui_State = SYS_CLOCK_0;
						break;

					case MS_CELL:
						Ui_State = CELL_REP_0;
						break;

					case MS_COM:
						Ui_State = COM_BAUD_0;
						break;

					case MS_SENSOR:
						Ui_State = TRANS_DEC_S0_0;
						break;

					default:
						Ui_State = UI_MAIN_BUTTON_A;
				}
			}
			break;
		//-------------------------------------------------------------------
		// Setup: In High Alarm
		case ALARM_IN_H_0:
			Ui_Config_Var = ID_Alarm_In_H;
			LCDScrollOn( L4, "> Nivel de alarma de entrada alta" );
			Ui_State++;
			break;

		case ALARM_IN_H_1:
		case ALARM_IN_H_HIST_1:
		case ALARM_IN_L_1:
		case ALARM_IN_L_HIST_1:
			// Format: 0000EEEEEEE,FFFFF
			binary_to_ascii(read_flash(Ui_Config_Var), Ui_Disp_Buffer, read_flash(ID_Dec_0)); // Careful, writes 8 bytes!
			strcpy(&Ui_Disp_Buffer[6], "bar");
			LCDStr(5, L5, Ui_Disp_Buffer);
			Ui_State++;
			break;

		case ALARM_OUT_H_1:
		case ALARM_OUT_H_HIST_1:
		case ALARM_OUT_L_1:
		case ALARM_OUT_L_HIST_1:
			// Format: 0000EEEEEEE,FFFFF
			binary_to_ascii(read_flash(Ui_Config_Var), Ui_Disp_Buffer, read_flash(ID_Dec_1)); // Careful, writes 8 bytes!
			strcpy(&Ui_Disp_Buffer[6], "bar");
			LCDStr(5, L5, Ui_Disp_Buffer);
			Ui_State++;
			break;
		//-------------------------------------------------------------------
		// Setup: In High Alarm Histeresis
		case ALARM_IN_H_HIST_0:
			Ui_Config_Var = ID_Alarm_In_H_Hist;
			LCDScrollOn( L4, "> Histeresis de alarma de entrada alta" );
			Ui_State++;
			break;
		//-------------------------------------------------------------------
		// Setup: In Low Alarm
		case ALARM_IN_L_0:
			Ui_Config_Var = ID_Alarm_In_L;
			LCDScrollOn( L4, "> Nivel de alarma de entrada baja" );
			Ui_State++;
			break;
		//-------------------------------------------------------------------
		// Setup: In Low Alarm Histeresis
		case ALARM_IN_L_HIST_0:
			Ui_Config_Var = ID_Alarm_In_L_Hist;
			LCDScrollOn( L4, "> Histeresis de alarma de entrada baja" );
			Ui_State++;
			break;
		//-------------------------------------------------------------------
		// Setup: Out High Alarm
		case ALARM_OUT_H_0:
			Ui_Config_Var = ID_Alarm_Out_H;
			LCDScrollOn( L4, "> Nivel de alarma de salida alta" );
			Ui_State++;
			break;
		//-------------------------------------------------------------------
		// Setup: Out High Alarm Histeresis
		case ALARM_OUT_H_HIST_0:
			Ui_Config_Var = ID_Alarm_Out_H_Hist;
			LCDScrollOn( L4, "> Histeresis de alarma de salida alta" );
			Ui_State++;
			break;
		//-------------------------------------------------------------------
		// Setup: Out Low Alarm
		case ALARM_OUT_L_0:
			Ui_Config_Var = ID_Alarm_Out_L;
			LCDScrollOn( L4, "> Nivel de alarma de salida baja" );
			Ui_State++;
			break;
		//-------------------------------------------------------------------
		// Setup: Out Low Alarm Histeresis
		case ALARM_OUT_L_HIST_0:
			Ui_Config_Var = ID_Alarm_Out_L_Hist;
			LCDScrollOn( L4, "> Histeresis de alarma de salida baja" );
			Ui_State++;
			break;
		//-------------------------------------------------------------------
		// Setup: In High Alarm mode
		case ALARM_IN_H_MODE_0:
			Ui_Config_Var = ID_Alarm_In_H_Mode;
			LCDScrollOn( L4, "> Estado de alarma de entrada alta" );
			Ui_State++;
			break;

		case ALARM_IN_H_MODE_1:
		case ALARM_IN_L_MODE_1:
		case ALARM_OUT_H_MODE_1:
		case ALARM_OUT_L_MODE_1:
			switch(read_flash(Ui_Config_Var))
			{
				case 0: LCDStr(0, L5, "Desactivada" ); break;
				case 1: LCDStr(0, L5, "Activada   " ); break;
			}
			Ui_State++;
			break;
		//-------------------------------------------------------------------
		// Setup: In Low Alarm mode
		case ALARM_IN_L_MODE_0:
			Ui_Config_Var = ID_Alarm_In_L_Mode;
			LCDScrollOn( L4, "> Estado de alarma de entrada baja" );
			Ui_State++;
			break;
		//-------------------------------------------------------------------
		// Setup: Out High Alarm mode
		case ALARM_OUT_H_MODE_0:
			Ui_Config_Var = ID_Alarm_Out_H_Mode;
			LCDScrollOn( L4, "> Estado de alarma de salida alta" );
			Ui_State++;
			break;
		//-------------------------------------------------------------------
		// Setup: Out Low Alarm mode
		case ALARM_OUT_L_MODE_0:
			Ui_Config_Var = ID_Alarm_Out_L_Mode;
			LCDScrollOn( L4, "> Estado de alarma de salida baja" );
			Ui_State++;
			break;
		//-------------------------------------------------------------------
		// Setup: Mask time
		case ALARM_MASK_0:
			Ui_Config_Var = ID_T_Mask;
			LCDScrollOn( L4, "> Tiempo para Alarma" );
			Ui_State++;
			break;

		case ALARM_MASK_1:
			// Format: 00000000EEEEEEEE
		  	binary_to_ascii(read_flash(ID_T_Mask), Ui_Disp_Buffer, 0); // Careful, writes 8 bytes!
			Ui_Disp_Buffer[3] = 's';
			Ui_Disp_Buffer[4] = 0X00;
			LCDStr(10, L5, Ui_Disp_Buffer);
			Ui_State++;
			break;
		//-------------------------------------------------------------------
		case ALARM_END:
			Ui_State = UI_ENTER_SCREEN;
			break;
		//-------------------------------------------------------------------
		//-------------------------------------------------------------------
		// Setup: Date & Hour
		case SYS_CLOCK_0:
			memcpy(Ui_Buffer, Global_Date, 6);
			LCDScrollOn( L4, "> Fecha y Hora" );
			Ui_Index = DAY;
			Ui_State++;
			break;

		case SYS_CLOCK_1:
			validate_clock(Ui_Buffer, Ui_Index);
			clock_to_ascii((uint8_t *)Ui_Disp_Buffer, (uint8_t *)Ui_Buffer);
			LCDStr(0, L5, Ui_Disp_Buffer);
			LCDBlinkOn(Ui_Index*3, L5, &Ui_Disp_Buffer[Ui_Index*3], 2);
			Ui_State++;
			break;

		case SYS_CLOCK_BUTTON_B:
			if(!Flag_Button_I)
			{
				if(Ui_Buffer[Ui_Index] < 0xFF)									// Ver si dio la vuelta e inc.
				{
					Ui_Buffer[Ui_Index]++;
					Flag_Main_Ui_Var_Changed = 1;
					Ui_State -= 2;
					break;
				}
			}

			if(!Flag_Button_D)
			{
				if(Ui_Buffer[Ui_Index] > 0x00)									// Ver si dio la vuelta y dec.
				{
					Ui_Buffer[Ui_Index]--;
					Flag_Main_Ui_Var_Changed = 1;
					Ui_State -= 2;
					break;
				}
			}

			if(!Flag_Button_M)
			{
				if(Ui_Index < MIN)
				{
					Ui_Index++;
					Ui_State -= 2;
				}
				else
				{
					if(Flag_Main_Ui_Var_Changed)
					{
						Flag_Main_Ui_Var_Changed = 0;
						Ui_Buffer[SEC]=0;										// Save clock
						RTC_Set_Date(Ui_Buffer);
					}
					LCDClearLines(L4,L5);
					Ui_State++;
				}
				break;
			}

			if(!Flag_Button_R)
			{
				if(Ui_Index > DAY)
				{
					Ui_Index--;
					Ui_State -= 2;
					break;
				}
			}

			break;
		//-------------------------------------------------------------------
		// Setup: Tag
		case SYS_TAG_0:
			LCDScrollOn( L4, "> Identificador" );
			Ui_Config_Var = ID_Tag_0;
			Ui_State++;
			break;

		case SYS_TAG_1:
			for(Ui_Index = 0; Ui_Index < TAG_SIZE; Ui_Index++)
			{
				Ui_Disp_Buffer[Ui_Index] = (char)read_flash(ID_Tag_0 + Ui_Index);
			}
			Ui_Disp_Buffer[TAG_SIZE] = 0X00;
			LCDStr(0, L5, Ui_Disp_Buffer);
			LCDBlinkOn(Ui_Config_Var - ID_Tag_0, L5, &Ui_Disp_Buffer[Ui_Config_Var - ID_Tag_0], 1);
			Ui_State++;
			break;
		//-------------------------------------------------------------------
		// Setup: Password
		case SYS_PASS_0:
			LCDStr(0, L4, "> Clave       ");
			Ui_Config_Var = ID_Pass_0;
			Ui_State++;
			break;

		case SYS_PASS_1:
			for(Ui_Index = 0; Ui_Index < PASS_SIZE; Ui_Index++)
			{
				Ui_Disp_Buffer[Ui_Index] = flash_to_pass[(char)read_flash(ID_Pass_0 + Ui_Index)];
			}
			Ui_Disp_Buffer[PASS_SIZE] = 0X00;
			LCDStr(0, L5, Ui_Disp_Buffer);
			LCDBlinkOn(Ui_Config_Var - ID_Pass_0, L5, &Ui_Disp_Buffer[Ui_Config_Var - ID_Pass_0], 1);
			Ui_State++;
			break;
		//-------------------------------------------------------------------
		case SYS_END:
			Ui_State = UI_ENTER_SCREEN;
			break;
		//-------------------------------------------------------------------
		//-------------------------------------------------------------------
		// Setup: Report period
		case CELL_REP_0:
			LCDScrollOn( L4, "> Periodo de reporte" );
			Ui_Config_Var = ID_Report_Period;
			Ui_State++;
			break;

		case CELL_REP_1:
			// Format: 00000000EEEEEEEE
		  	binary_to_ascii(read_flash(ID_Report_Period), Ui_Disp_Buffer, 0); // Careful, writes 8 bytes!
			strcpy(&Ui_Disp_Buffer[3], "dias");
			LCDStr(7, L5, Ui_Disp_Buffer);
			Ui_State++;
			break;
		//-------------------------------------------------------------------
		// Setup: Global SMS & Voice call retries
		case CELL_RETRIES_0:
			LCDScrollOn( L4, "> Reintentos SMS y llamada de voz" );
			Ui_Config_Var = ID_Retries;
			Ui_State++;
			break;

		case CELL_RETRIES_1:
			binary_to_ascii(read_flash(ID_Retries), Ui_Disp_Buffer, 0); // Careful, writes 8 bytes!
			Ui_Disp_Buffer[3] = 0X00;
			LCDStr(11, L5, Ui_Disp_Buffer);
			Ui_State++;
			break;
		//-------------------------------------------------------------------
		// Setup: Global SMS & Voice call retries
		case CELL_RETRIES_T_0:
			LCDScrollOn( L4, "> Tiempo entre reintentos SMS y llamada de voz" );
			Ui_Config_Var = ID_Retries_Time;
			Ui_State++;
			break;

		case CELL_RETRIES_T_1:
			binary_to_ascii(read_flash(ID_Retries_Time), Ui_Disp_Buffer, 0); // Careful, writes 8 bytes!
			Ui_Disp_Buffer[3] = 0X00;
			LCDStr(11, L5, Ui_Disp_Buffer);
			Ui_State++;
			break;
		//-------------------------------------------------------------------
		// Setup: Cell number
		case CELL_0_0:
			LCDStr(0, L4, "> Celular Voz1" );
			Ui_Config_Var = ID_VCel_0;
			goto CELL_X_0;

		case CELL_1_0:
			LCDStr(0, L4, "> Celular Voz2" );
			Ui_Config_Var = ID_VCel_1;
			goto CELL_X_0;

		case CELL_2_0:
			LCDStr(0, L4, "> Celular SMS1" );
			Ui_Config_Var = ID_Cel_0;
			goto CELL_X_0;

		case CELL_3_0:
			LCDStr(0, L4, "> Celular SMS2" );
			Ui_Config_Var = ID_Cel_1;
			goto CELL_X_0;

		case CELL_4_0:
			LCDStr(0, L4, "> Celular SMS3" );
			Ui_Config_Var = ID_Cel_2;
			goto CELL_X_0;

		case CELL_5_0:
			LCDStr(0, L4, "> Celular SMS4" );
			Ui_Config_Var = ID_Cel_3;

CELL_X_0:
			Ui_Index = 0;
			memset(Ui_Disp_Buffer, 0x00, sizeof(Ui_Disp_Buffer));
			phone_flash_to_ascii((uint8_t *)Ui_Disp_Buffer, (uint8_t)Ui_Config_Var);
			Ui_State++;
			break;

		case CELL_0_1:
		case CELL_1_1:
		case CELL_2_1:
		case CELL_3_1:
		case CELL_4_1:
		case CELL_5_1:
			validate_phone(Ui_Disp_Buffer);
			LCDStr(0, L5, Ui_Disp_Buffer);
			LCDBlinkOn(Ui_Index, L5, &Ui_Disp_Buffer[Ui_Index], 1);
			Ui_State++;
			break;

		case CELL_0_BUTTON_B:
		case CELL_1_BUTTON_B:
		case CELL_2_BUTTON_B:
		case CELL_3_BUTTON_B:
		case CELL_4_BUTTON_B:
		case CELL_5_BUTTON_B:
			if(!Flag_Button_I)
			{
				if(Ui_Disp_Buffer[Ui_Index] == '9') Ui_Disp_Buffer[Ui_Index] = ' ';
				else if(Ui_Disp_Buffer[Ui_Index] == ' ') Ui_Disp_Buffer[Ui_Index] = '0';
				else Ui_Disp_Buffer[Ui_Index]++;
				
				Flag_Main_Ui_Var_Changed = 1;
				Ui_State -= 2;
				break;
			}

			if(!Flag_Button_D)
			{
				if(Ui_Disp_Buffer[Ui_Index] == '0') Ui_Disp_Buffer[Ui_Index] = ' ';
				else if(Ui_Disp_Buffer[Ui_Index] == ' ') Ui_Disp_Buffer[Ui_Index] = '9';
				else Ui_Disp_Buffer[Ui_Index]--;
				
				Flag_Main_Ui_Var_Changed = 1;
				Ui_State -= 2;
				break;
			}

			if(!Flag_Button_M)
			{
				if(Ui_Index < (CELL_SIZE-1))
				{
					Ui_Index++;
					Ui_State -= 2;
				}
				else
				{
					if(Flag_Main_Ui_Var_Changed)
					{
						Flag_Main_Ui_Var_Changed = 0;
						phone_ascii_to_flash(Ui_Disp_Buffer,Ui_Config_Var);		// Save cell
						update_flash_safe();
					}
					LCDClearLines(L4,L5);
					Ui_State++;
				}
				break;
			}

			if(!Flag_Button_R)
			{
				if(Ui_Index >= 1)
				{
					Ui_Index--;
					Ui_State -= 2;
					break;
				}
			}

			break;

		//-------------------------------------------------------------------
		case CELL_END:
			Ui_State = UI_ENTER_SCREEN;
			break;

		//-------------------------------------------------------------------
		//-------------------------------------------------------------------
		// Setup: Baud rate
		case COM_BAUD_0:
			Ui_Config_Var = ID_Baud;
			LCDScrollOn( L4, "> Velocidad (Modbus)" );
			Ui_State++;
			break;

		case COM_BAUD_1:
			switch(read_flash(Ui_Config_Var))
			{
				case SP_9600_E: LCDStr(0, L5, "      9600 bps" ); break;
				case SP_19200_E: LCDStr(0, L5, "     19200 bps" ); break;
				case SP_38400_E: LCDStr(0, L5, "     38400 bps" ); break;
			}
			Ui_State++;
			break;

		//-------------------------------------------------------------------
		// Setup: Parity
		case COM_PAR_0:
			Ui_Config_Var = ID_Par;
			LCDScrollOn( L4, "> Paridad (Modbus)" );
			Ui_State++;
			break;

		case COM_PAR_1:
			switch(read_flash(Ui_Config_Var))
			{
				case NONE_E: LCDStr(0, L5, "Sin paridad   " ); break;
				case EVEN_E: LCDStr(0, L5, "Par           " ); break;
				case ODD_E: LCDStr(0, L5, "Impar         " ); break;
			}
			Ui_State++;
			break;

		//-------------------------------------------------------------------
		// Setup: Bits #
		case COM_BITS_0:
			Ui_Config_Var = ID_Bits;
			LCDScrollOn( L4, "> Numero de bits (Modbus)" );
			Ui_State++;
			break;

		case COM_BITS_1:
			if(read_flash(Ui_Config_Var)==BITS_7_E)	LCDStr(0, L5, "             7" );
			else LCDStr(0, L5, "             8" );
			Ui_State++;
			break;

		//-------------------------------------------------------------------
		// Setup: Swap word & bytes
		case COM_SWAP_0:
			Ui_Config_Var = ID_Float;
			LCDScrollOn( L4, "> Formato punto flotante (Modbus)" );
			Ui_State++;
			break;

		case COM_SWAP_1:
			switch(read_flash(Ui_Config_Var))
			{
				case INV_NONE_E: LCDStr(0, L5, "No invierte   " ); break;
				case INV_WORD_E: LCDStr(0, L5, "Invierte Words" ); break;
				case INV_BYTE_E: LCDStr(0, L5, "Invierte Bytes" ); break;
				case INV_BOTH_E: LCDStr(0, L5, "Invierte ambos" ); break;
			}
			Ui_State++;
			break;

		//-------------------------------------------------------------------
		// Setup: RTU/ASCII
		case COM_MODE_0:
			Ui_Config_Var = ID_Protocol;
			LCDScrollOn( L4, "> Protocolo (Modbus)" );
			Ui_State++;
			break;

		case COM_MODE_1:
			if(read_flash(Ui_Config_Var) == RTU_E) LCDStr(0, L5, "RTU           " );
			else LCDStr(0, L5, "ASCII         " );
			Ui_State++;
			break;
		//-------------------------------------------------------------------
		// Setup: Modbus Address
		case COM_MOD_ADDR_0:
			Ui_Config_Var = ID_Mod_Addr;
			LCDScrollOn( L4, "> Direccion (Modbus)" );
			Ui_State++;
			break;

		case COM_MOD_ADDR_1:
			binary_to_ascii(read_flash(ID_Mod_Addr), Ui_Disp_Buffer, 0); // Careful, writes 8 bytes!
			Ui_Disp_Buffer[3] = 0X00;
			LCDStr(11, L5, Ui_Disp_Buffer);
			Ui_State++;
			break;
		//-------------------------------------------------------------------
		// Setup: Modbus on/off
		case COM_MOD_ON_0:
			Ui_Config_Var = ID_Mod_On;
			LCDScrollOn( L4, "> Activar (Modbus)" );
			Ui_State++;
			break;

		case COM_MOD_ON_1:
			if(read_flash(Ui_Config_Var)) LCDStr(0, L5, "Activado      " );
			else LCDStr(0, L5, "Desactivado   " );
			Ui_State++;
			break;
		//-------------------------------------------------------------------
		case COM_END:
			Ui_State = UI_ENTER_SCREEN;
			break;

		//-------------------------------------------------------------------
		// Setup: Sensor decimal point
		case TRANS_DEC_S0_0:
			Ui_Config_Var = ID_Dec_0;
			LCDScrollOn( L4, "> Rango Transmisor Entrada" );
			Ui_State++;
			break;

		case TRANS_DEC_S0_1:
		case TRANS_DEC_S1_1:
			switch(read_flash(Ui_Config_Var))
			{
				case 4: LCDStr(0, L5, "        256bar" ); break;
				case 5: LCDStr(0, L5, "        128bar" ); break;
				case 6: LCDStr(0, L5, "         64bar" ); break;
				case 7: LCDStr(0, L5, "         32bar" ); break;
				case 8: LCDStr(0, L5, "         16bar" ); break;
				case 9: LCDStr(0, L5, "          8bar" ); break;
				case 10: LCDStr(0, L5, "          4bar" ); break;
				case 11: LCDStr(0, L5, "          2bar" ); break;
				case 12: LCDStr(0, L5, "          1bar" ); break;
			}
			Ui_State++;
			break;
		//-------------------------------------------------------------------
		// Setup: Sensor decimal point
		case TRANS_DEC_S1_0:
			Ui_Config_Var = ID_Dec_1;
			LCDScrollOn( L4, "> Rango Transmisor Salida" );
			Ui_State++;
			break;

		//-------------------------------------------------------------------
		// Setup: Offset
		case TRANS_OFFSET_S0_0:
			Ui_Config_Var = ID_Offset_0;
			LCDScrollOn( L4, "> Calibrar Cero: Presion Entrada" );
			Ui_State++;
			break;

		case TRANS_OFFSET_S0_1:
			binary_to_ascii(read_flash(Ui_Config_Var), Ui_Disp_Buffer, read_flash(ID_Dec_0)); // Careful, writes 8 bytes!
			Ui_Disp_Buffer[8] = 0X00;
			LCDStr(6, L5, Ui_Disp_Buffer);
			Ui_State++;
			break;

		case TRANS_OFFSET_S1_1:
			binary_to_ascii(read_flash(Ui_Config_Var), Ui_Disp_Buffer, read_flash(ID_Dec_1)); // Careful, writes 8 bytes!
			Ui_Disp_Buffer[8] = 0X00;
			LCDStr(6, L5, Ui_Disp_Buffer);
			Ui_State++;
			break;

		//-------------------------------------------------------------------
		// Setup: Span
		case TRANS_SPAN_S0_0:
			Ui_Config_Var = ID_Span_0;
			LCDScrollOn( L4, "> Calibrar Span: Presion Entrada" );
			Ui_State++;
			break;

		case TRANS_SPAN_S0_1:
		case TRANS_SPAN_S1_1:
		case TRANS_SPAN_BAT_1:
			// Format: 000E,FFFFFFFFFFFF
			binary_to_ascii(read_flash(Ui_Config_Var), Ui_Disp_Buffer, 12); // Careful, writes 8 bytes!
			Ui_Disp_Buffer[8] = 0X00;
			LCDStr(6, L5, Ui_Disp_Buffer);
			Ui_State++;
			break;
		//-------------------------------------------------------------------
		case TRANS_OFFSET_S1_0:
			Ui_Config_Var = ID_Offset_1;
			LCDScrollOn( L4, "> Calibrar Cero: Presion Salida" );
			Ui_State++;
			break;

		case TRANS_SPAN_S1_0:
			Ui_Config_Var = ID_Span_1;
			LCDScrollOn( L4, "> Calibrar Span: Presion Salida" );
			Ui_State++;
			break;

		//-------------------------------------------------------------------
		case TRANS_SPAN_BAT_0:
			if(!Flag_Main_Master_Pass)
			{
				Ui_State = TRANS_END;
				break;
			}
			Ui_Config_Var = ID_Span_Bat;
			LCDScrollOn( L4, "> Calibrar Span: Bateria" );
			Ui_State++;

			break;
            
		//-------------------------------------------------------------------
		case TRANS_END:
			Ui_Config_Var = ID_T_Mask; // Ui_Config_Var needs to be != Offset_Bat, Span_Bat, ... for the display
			Ui_State = UI_ENTER_SCREEN;
			break;

		//-------------------------------------------------------------------
		case UI_MAIN_BUTTON_A:
		case UI_LOGIN_BUTTON_A:
		case UI_ENTER_BUTTON_A:
		case ALARM_IN_H_BUTTON_A:
		case ALARM_IN_H_HIST_BUTTON_A:
		case ALARM_IN_H_MODE_BUTTON_A:
		case ALARM_IN_L_BUTTON_A:
		case ALARM_IN_L_HIST_BUTTON_A:
		case ALARM_IN_L_MODE_BUTTON_A:
		case ALARM_OUT_H_BUTTON_A:
		case ALARM_OUT_H_HIST_BUTTON_A:
		case ALARM_OUT_H_MODE_BUTTON_A:
		case ALARM_OUT_L_BUTTON_A:
		case ALARM_OUT_L_HIST_BUTTON_A:
		case ALARM_OUT_L_MODE_BUTTON_A:
		case ALARM_MASK_BUTTON_A:
		case CELL_0_BUTTON_A:
		case CELL_1_BUTTON_A:
		case CELL_2_BUTTON_A:
		case CELL_3_BUTTON_A:
		case CELL_4_BUTTON_A:
		case CELL_5_BUTTON_A:
		case CELL_REP_BUTTON_A:
		case CELL_RETRIES_BUTTON_A:
		case CELL_RETRIES_T_BUTTON_A:
		case COM_BAUD_BUTTON_A:
		case COM_PAR_BUTTON_A:
		case COM_BITS_BUTTON_A:
		case COM_SWAP_BUTTON_A:
		case COM_MODE_BUTTON_A:
		case COM_MOD_ADDR_BUTTON_A:
		case COM_MOD_ON_BUTTON_A:
		case TRANS_DEC_S0_BUTTON_A:
		case TRANS_OFFSET_S0_BUTTON_A:
		case TRANS_SPAN_S0_BUTTON_A:
		case TRANS_DEC_S1_BUTTON_A:
		case TRANS_OFFSET_S1_BUTTON_A:
		case TRANS_SPAN_S1_BUTTON_A:
		case TRANS_SPAN_BAT_BUTTON_A:
		case SYS_CLOCK_BUTTON_A:
		case SYS_TAG_BUTTON_A:
		case SYS_PASS_BUTTON_A:
			if((Flags_Buttons.Byte & BUTTON_MASK) == BUTTON_MASK) Ui_State++;	// Buttons released?
			else if(!Timer_Buttons) Ui_State++;							// Fast mode?
			break;
		//-------------------------------------------------------------------
		case ALARM_IN_H_BUTTON_B:
		case ALARM_IN_H_HIST_BUTTON_B:
		case ALARM_IN_H_MODE_BUTTON_B:
		case ALARM_IN_L_BUTTON_B:
		case ALARM_IN_L_HIST_BUTTON_B:
		case ALARM_IN_L_MODE_BUTTON_B:
		case ALARM_OUT_H_BUTTON_B:
		case ALARM_OUT_H_HIST_BUTTON_B:
		case ALARM_OUT_H_MODE_BUTTON_B:
		case ALARM_OUT_L_BUTTON_B:
		case ALARM_OUT_L_HIST_BUTTON_B:
		case ALARM_OUT_L_MODE_BUTTON_B:
		case ALARM_MASK_BUTTON_B:
		case COM_BAUD_BUTTON_B:
		case COM_PAR_BUTTON_B:
		case COM_BITS_BUTTON_B:
		case COM_SWAP_BUTTON_B:
		case COM_MODE_BUTTON_B:
		case COM_MOD_ADDR_BUTTON_B:
		case COM_MOD_ON_BUTTON_B:
		case TRANS_DEC_S0_BUTTON_B:
		case TRANS_OFFSET_S0_BUTTON_B:
		case TRANS_SPAN_S0_BUTTON_B:
		case TRANS_DEC_S1_BUTTON_B:
		case TRANS_OFFSET_S1_BUTTON_B:
		case TRANS_SPAN_S1_BUTTON_B:
		case TRANS_SPAN_BAT_BUTTON_B:
		case CELL_REP_BUTTON_B:
		case CELL_RETRIES_BUTTON_B:
		case CELL_RETRIES_T_BUTTON_B:
			if(!Flag_Button_I)
			{
				inc_flash(Ui_Config_Var, 1);				
				// Fast mode?				
				if(Timer_Buttons == 0) inc_flash(Ui_Config_Var, 4);
				Ui_State -= 2;
				Flag_Main_Ui_Var_Changed = 1;
				break;
			}

			if(!Flag_Button_D)
			{
				dec_flash(Ui_Config_Var, 1);				
				// Fast mode?				
				if(Timer_Buttons == 0) dec_flash(Ui_Config_Var, 4);
				Ui_State -= 2;
				Flag_Main_Ui_Var_Changed = 1;
				break;
			}

			if(!Flag_Button_M)
			{
				if(Flag_Main_Ui_Var_Changed)
				{
					Flag_Main_Ui_Var_Changed = 0;
					update_flash_safe();
				}
				LCDClearLines(L4,L5);
				Ui_State++;
				break;
			}

			break;
		//-------------------------------------------------------------------
		case SYS_TAG_BUTTON_B:
			if(!Flag_Button_M)
			{
				Ui_Config_Var++;
				if((Ui_Config_Var - ID_Tag_0) >= TAG_SIZE)
				{
					if(Flag_Main_Ui_Var_Changed)
					{
						Flag_Main_Ui_Var_Changed = 0;
						update_flash_safe();
					}
					LCDClearLines(L4,L5);
					Ui_State++;
				}
				else Ui_State -= 2;
			}

			if(!Flag_Button_I)
			{
				inc_flash(Ui_Config_Var, 1);				
				// Fast mode?				
				if(Timer_Buttons == 0) inc_flash(Ui_Config_Var, 4);
				Ui_State -= 2;
				Flag_Main_Ui_Var_Changed = 1;
				break;
			}

			if(!Flag_Button_D)
			{
				dec_flash(Ui_Config_Var, 1);				
				// Fast mode?				
				if(Timer_Buttons == 0) dec_flash(Ui_Config_Var, 4);
				Ui_State -= 2;
				Flag_Main_Ui_Var_Changed = 1;
				break;
			}

			if(!Flag_Button_R)
			{
				if(Ui_Config_Var > ID_Tag_0)
				{
					Ui_Config_Var--;
					Ui_State -=2;
				}
			}
			break;

		//-------------------------------------------------------------------
		case SYS_PASS_BUTTON_B:
			if(!Flag_Button_M)
			{
				Ui_Config_Var++;
				if((Ui_Config_Var - ID_Pass_0) >= PASS_SIZE)
				{
					if(Flag_Main_Ui_Var_Changed)
					{
						Flag_Main_Ui_Var_Changed = 0;
						update_flash_safe();
					}
					LCDClearLines(L4,L5);
					Ui_State++;
				}
				else Ui_State -= 2;
			}

			if(!Flag_Button_I)
			{
				inc_flash(Ui_Config_Var, 1);				
				Ui_State -= 2;
				Flag_Main_Ui_Var_Changed = 1;
				break;
			}

			if(!Flag_Button_D)
			{
				dec_flash(Ui_Config_Var, 1);				
				Ui_State -= 2;
				Flag_Main_Ui_Var_Changed = 1;
				break;
			}

			if(!Flag_Button_R)
			{
				if(Ui_Config_Var > ID_Pass_0)
				{
					Ui_Config_Var--;
					Ui_State -=2;
				}
			}
			break;

		//-------------------------------------------------------------------
		default:
			Ui_State = 0;
	}

	LCDUpdate();
}
//******************************************************************************

