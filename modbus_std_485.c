#include <io430x16x.h>
#include "def.h"
#include "main.h"
#include "modbus_std_485.h"
#include "timers.h"
#include "uart_1.h"
#include "flash.h"
#include "monitor.h"
#include "adc.h"

#pragma diag_suppress = Pa082 // avoid volatile undefined behavior warnings

char Modbus_State;
volatile char Modbus_T_3_5;
volatile char Modbus_T_1_5;
volatile unsigned short Timer_Comm_Fail;
volatile unsigned short Modbus_Rx_Index;
volatile unsigned short Modbus_Counter;
char Modbus_Buffer[MB_BUF_SIZE];
unsigned short Modbus_Address;
unsigned short Modbus_Size;
unsigned short Modbus_Aux;
volatile tREG08 Modbus_Flags;

char Coils[1];				// Bit 0 = ?
char Discrete_Inputs[2];	// 0 & 1 = Flags_Monitor_Status word
unsigned short Input_Registers[REG_32FP_SIZE]; // 0=Pressure A, 1=Pressure B
unsigned short Analog_Output[1]; // Not used

//------------------------------------------------------------------------------
char chk_lrc(char *Msg, char Len)
{
	char lrc = 0;

	if(Len & 0x01) return 0;				// Len must be even

	while(Len)
	{
		lrc += (ascii_to_nibble(*Msg) << 4);// get high nibble
		Msg++;
		lrc += ascii_to_nibble(*Msg);		// get low nibble
		Msg++;
		Len -= 2;
	}
	return lrc;
}
//--------------------------------------------------------
unsigned int CRC16(char *Msg, unsigned short Len)
{
	unsigned short CRC = 0xFFFF; 			// CRC initialized
	unsigned char n;

	while(Len--)		 					// pass through message buffer
	{
		CRC = CRC ^ (unsigned short)(*Msg);	// Xor with byte
		
		for(n = 0; n < 8; n++)				// Rotate 8 times
		{
			if(CRC & 0X0001)				// Xor with poly if the bit from the left is 1
			{
				CRC = CRC >> 1;
				CRC = CRC ^ 0XA001;
			}
			else
				CRC = CRC >> 1;
		}

		Msg++;								// point to the next byte
	}
	return CRC;
}
//--------------------------------------------------------
void map_discrete_inputs(void)
{
	Discrete_Inputs[0] = Flags_Monitor_Alarm.Byte;
	Discrete_Inputs[1] = Flags_Monitor_Status.Byte;
}
//--------------------------------------------------------
short map_in(char i)
{
	switch(i)
	{
		case 0:	return (short)Sensors_Samples[0];
		case 1:	return (short)Sensors_Samples[1];
		default: return 0;
	}
}

short map_in_dp(char i)
{
	switch(i)
	{
		case 0:	return read_flash(ID_Dec_0);
		case 1:	return read_flash(ID_Dec_1);
		default: return 0;
	}
}

short map_in_dp_2(char i)
{
	if(i < ID_Alarm_Out_H)
		return read_flash(ID_Dec_0);
	else
		return read_flash(ID_Dec_1);
}

//--------------------------------------------------------
void map_coils_out(void)
{
}
//--------------------------------------------------------
void modbus_rx_int(char c)
{
	// Has the last frame been processed?
	if(Modbus_Flags_Rx_Frame)
		return;

	if(read_flash(ID_Protocol) == RTU_E)
	{	// Reload timers
		Modbus_T_3_5 = T_3_5;
		Modbus_T_1_5 = T_1_5;
	}
	else
	{	// Start?
		if(c == RTU_START) Modbus_Rx_Index = 0;
 		// Finished?
		else if(c == LF) Modbus_Flags_Rx_Frame = 1;
	}

	if(Modbus_Rx_Index < MB_BUF_SIZE)
		Modbus_Buffer[Modbus_Rx_Index++] = c;		// Get byte
}

//--------------------------------------------------------
// 500us Tick
//--------------------------------------------------------
void modbus_500us_tick(void)
{
	if(Timer_Comm_Fail) Timer_Comm_Fail--;

	if(read_flash(ID_Protocol) == RTU_E)
	{	
		if(Modbus_Rx_Index == 0) return;	// No frame has been detected yet
	
		if(Modbus_T_1_5) Modbus_T_1_5--;	// Frame already started, dec. timer
		else Modbus_Flags_Rx_Frame = 1;		// Finish
	}

	if(Modbus_T_3_5) Modbus_T_3_5--;
}
//------------------------------------------------------------------------------
void ascii_to_rtu(void)
{
	unsigned short rtu_index = 0;		// points to destiny (RTU)
	unsigned short ascii_index = 1;		// points to source (ASCII), RTU_START is not copied
	while(Modbus_Buffer[ascii_index+2] != CR) // LRC, CR & LF are not copied
	{
		Modbus_Buffer[rtu_index] = (ascii_to_nibble(Modbus_Buffer[ascii_index]) << 4);	// get high nibble
		ascii_index++;

		Modbus_Buffer[rtu_index] += ascii_to_nibble(Modbus_Buffer[ascii_index]);		// get low nibble
		ascii_index++;

		rtu_index++;
	}
}
//------------------------------------------------------------------------------
void rtu_to_ascii(unsigned short size_rtu) // size: all rtu bytes except CRC
{
	char lrc;
	unsigned short ascii_index = size_rtu << 1;	// points to destiny (ASCII)
	unsigned short i = size_rtu;

	while(i)
	{
		i--;
		Modbus_Buffer[ascii_index--] = nibble_to_ascii(Modbus_Buffer[i] & 0x0F);
		Modbus_Buffer[ascii_index--] = nibble_to_ascii(Modbus_Buffer[i] >> 4);
	}

	Modbus_Buffer[0] = RTU_START;

	// Compute checksum
	lrc = -chk_lrc(&Modbus_Buffer[1], size_rtu << 1);
	ascii_index = (size_rtu << 1) + 1;

	Modbus_Buffer[ascii_index++] = nibble_to_ascii(lrc >> 4);	// chk H
	Modbus_Buffer[ascii_index++] = nibble_to_ascii(lrc & 0x0F);// chk L
	Modbus_Buffer[ascii_index++] = CR;
	Modbus_Buffer[ascii_index++] = LF;
}
//--------------------------------------------------------
void MB_Format_Exception(char code)
{
	//Modbus_Buffer[MB_ADDR] was not modified
	Modbus_Buffer[MB_FUNCTION] |= 0X80;				// Error
	Modbus_Buffer[MB_FUNCTION + 1] = code;			// Error code

	if(read_flash(ID_Protocol) == RTU_E)
	{
		Modbus_Aux = CRC16( Modbus_Buffer, 3);		// Compute CRC
		Modbus_Buffer[MB_FUNCTION + 2] = (char)Modbus_Aux;
		Modbus_Buffer[MB_FUNCTION + 3] = (char)(Modbus_Aux >> 8);

		Modbus_Counter = 3+2;						// Bytes to be sent
	}
	else
	{
		rtu_to_ascii(3);
		Modbus_Counter = 1+(3<<1)+4;				// Bytes to be sent
	}
}
//--------------------------------------------------------
void MB_Format_Data_8(char * data, unsigned short address)
{
	char a,b;
	char r,i;
  unsigned short bit_count = Modbus_Size;

	//Modbus_Buffer[MB_ADDR] was not modified
	//Modbus_Buffer[MB_FUNCTION] was not modified
	Modbus_Buffer[MB_BYTE_COUNT] = (char)(Modbus_Size >> 3);	// Get byte count from bits
	if(Modbus_Size & 0x0007) Modbus_Buffer[MB_BYTE_COUNT]++;

	r = (address & 0x07);					// Get rotation index
	data += address >> 3;					// Compute offset

	i = 0;
	while(i < Modbus_Buffer[MB_BYTE_COUNT])
	{
		if(r != 0)
		{
			a = (*data) >> r;							// Rotate
      if(bit_count >= (8-r))
      {
        bit_count -= (8-r);
      }
      else
      {
        bit_count = 0;
      }

			if(bit_count > 0)
      {
 				b = (*(data+1)) << (8-r);		// Rotate next byte
        if(bit_count >= r)
        {
          bit_count -= r;
        }
        else
        {
          bit_count = 0;
        }
      }
			else
      {
				b = 0;											// Do not rotate
      }

			Modbus_Buffer[MB_REGISTERS + i] = a | b;			// Or bytes and copy
		}
		else
		{
			Modbus_Buffer[MB_REGISTERS + i] = *data;			// Copy byte
		}
		i++;
		data++;													// Increment counter
	}

	a = Modbus_Buffer[MB_BYTE_COUNT];

	if(read_flash(ID_Protocol) == RTU_E)
	{
		Modbus_Aux = CRC16( Modbus_Buffer, a + 3);		// Compute CRC
		Modbus_Buffer[MB_REGISTERS + i] = (char)Modbus_Aux;
		i++;
		Modbus_Buffer[MB_REGISTERS + i] = (char)(Modbus_Aux >> 8);
	
		Modbus_Counter = 3+a+2;							// Bytes to be sent
	}
	else
	{
		rtu_to_ascii(3+a);
		Modbus_Counter = 1+((3+a)<<1)+4;				// Bytes to be sent
	}
}
//--------------------------------------------------------
void MB_Format_Data_16(unsigned short * data, unsigned short size)
{
	unsigned short i = 0;

	size = size << 1;									// size * 2 (1 int = 2 bytes)
	//Modbus_Buffer[MB_ADDR] was not modified
	//Modbus_Buffer[MB_FUNCTION] was not modified
	Modbus_Buffer[MB_BYTE_COUNT] = size;				// Byte count

	while(i < size)
	{
		Modbus_Buffer[MB_REGISTERS + i] = (*data)>>8;	// Copy high byte
		i++;											// Increment counter
		Modbus_Buffer[MB_REGISTERS + i] = *data;		// Copy low byte
		i++;											// Increment counter
		data++;											// Point to the next short
	}

	if(read_flash(ID_Protocol) == RTU_E)
	{
		Modbus_Aux = CRC16( Modbus_Buffer, size + 3);	// Compute CRC
		Modbus_Buffer[MB_REGISTERS + i] = (char)Modbus_Aux;
		i++;
		Modbus_Buffer[MB_REGISTERS + i] = (char)(Modbus_Aux >> 8);
	
		Modbus_Counter = 3+size+2;						// Bytes to be sent
	}
	else
	{
		rtu_to_ascii(3+size);
		Modbus_Counter = 1+((3+size)<<1)+4;				// Bytes to be sent
	}
}
//--------------------------------------------------------
void MB_Format_Data_32fp(unsigned short address, unsigned short size)
{
	tLONG aux;
	short aux_short;
	char j, dp;
	unsigned short i = 0;

  size = size << 2;		// Byte count = size * 4 (1 32fp = 4 bytes)
	while(i < size)
	{
		// 32fp input registers?
		if(address >= FLASH_32FP_SIZE) 
		{
			dp = map_in_dp((char)(address - FLASH_32FP_SIZE)); // decimal bits

			aux_short = map_in((char)(address - FLASH_32FP_SIZE));
			if(aux_short >= 0)
				aux.Long = (unsigned long)aux_short;
			else
				aux.Long = (unsigned long)(-aux_short);
		}
		else // 32fp flash registers
		{
			dp = map_in_dp_2((char)address);
			aux_short = 0;
			aux.Long = (unsigned long)read_flash(address);
		}

		if(aux.Long != 0)
		{
			for(j = 0; j < 24; j++)
			{
				if(aux.Long & 0x00800000) break;
				aux.Long = aux.Long << 1;
			}
			aux.Byte.hl &= 0x7F; // Clear the implicit 1
			aux.Long += ((unsigned long)(127 + 23 - dp -j) << 23); // Copy exponent

			// Set negative bit?
			if(aux_short < 0)
				aux.Byte.hh |= 0x80; 
		}

		switch(read_flash(ID_Float))
		{
			case INV_NONE_E:
				Modbus_Buffer[MB_REGISTERS + i] = aux.Byte.hh;
				Modbus_Buffer[MB_REGISTERS + 1 + i] = aux.Byte.hl;
				Modbus_Buffer[MB_REGISTERS + 2 + i] = aux.Byte.lh;
				Modbus_Buffer[MB_REGISTERS + 3 + i] = aux.Byte.ll;
				break;

			case INV_WORD_E:
				Modbus_Buffer[MB_REGISTERS + i] = aux.Byte.lh;
				Modbus_Buffer[MB_REGISTERS + 1 + i] = aux.Byte.ll;
				Modbus_Buffer[MB_REGISTERS + 2 + i] = aux.Byte.hh;
				Modbus_Buffer[MB_REGISTERS + 3 + i] = aux.Byte.hl;
				break;

			case INV_BYTE_E:
				Modbus_Buffer[MB_REGISTERS + i] = aux.Byte.hl;
				Modbus_Buffer[MB_REGISTERS + 1 + i] = aux.Byte.hh;
				Modbus_Buffer[MB_REGISTERS + 2 + i] = aux.Byte.ll;
				Modbus_Buffer[MB_REGISTERS + 3 + i] = aux.Byte.lh;
				break;

			case INV_BOTH_E:
				Modbus_Buffer[MB_REGISTERS + i] = aux.Byte.ll;
				Modbus_Buffer[MB_REGISTERS + 1 + i] = aux.Byte.lh;
				Modbus_Buffer[MB_REGISTERS + 2 + i] = aux.Byte.hl;
				Modbus_Buffer[MB_REGISTERS + 3 + i] = aux.Byte.hh;
				break;
		}
		i += 4;
		address++;						// Point to the next short
	}

	Modbus_Buffer[MB_BYTE_COUNT] = size;

	if(read_flash(ID_Protocol) == RTU_E)
	{
		Modbus_Aux = CRC16( Modbus_Buffer, size + 3);	// Compute CRC
		Modbus_Buffer[MB_REGISTERS + i] = (char)Modbus_Aux;
		i++;
		Modbus_Buffer[MB_REGISTERS + i] = (char)(Modbus_Aux >> 8);
	
		Modbus_Counter = 3+size+2;						// Bytes to be sent
	}
	else
	{
		rtu_to_ascii(3+size);
		Modbus_Counter = 1+((3+size)<<1)+4;				// Bytes to be sent
	}
}
//--------------------------------------------------------
void MB_Write_Mult_Coils(unsigned short address)
{
  char buff_mask;
  char buff_index;
  unsigned short i;

	//Modbus_Buffer[MB_ADDR] was not modified
	//Modbus_Buffer[MB_FUNCTION] was not modified
  
  buff_index = 0;
  buff_mask = 0x01;

	for(i = 0; i < Modbus_Size; i++)
	{
    if(Modbus_Buffer[MB_RX_REGISTERS + buff_index] & buff_mask)
    {
      Coils[address >> 3] |= num_to_bit[address & 0x7];
    }
    else
    {
      Coils[address >> 3] &= ~num_to_bit[address & 0x7];
    }
    
    address++;

    buff_mask = buff_mask << 1;
    if(buff_mask == 0)
    {
      buff_index++;
      buff_mask = 0x01;
    }
	}
}
//--------------------------------------------------------
void MB_Format_Echo(unsigned short size)
{
	if(read_flash(ID_Protocol) == RTU_E)
	{
    Modbus_Aux = CRC16(Modbus_Buffer, size);
    Modbus_Buffer[size] = (char)(Modbus_Aux);
    Modbus_Buffer[size+1] = (char)(Modbus_Aux >> 8);
		Modbus_Counter = size+2;						// Bytes to be sent
	}
	else
	{
		rtu_to_ascii(size);
		Modbus_Counter = 1+(size<<1)+4;					// Bytes to be sent
	}
}
//--------------------------------------------------------
void float_to_flash(unsigned short address, char d[])
{
	tLONG aux;
	char exp, dp;

	dp = map_in_dp_2((char)address);	// decimal bits data

	switch(read_flash(ID_Float))
	{
		case INV_NONE_E:
			aux.Byte.hh = d[0];
			aux.Byte.hl = d[1]; 
			aux.Byte.lh = d[2];
			aux.Byte.ll = d[3];
			break;

		case INV_WORD_E:
			aux.Byte.lh = d[0];
			aux.Byte.ll = d[1]; 
			aux.Byte.hh = d[2];
			aux.Byte.hl = d[3];
			break;

		case INV_BYTE_E:
			aux.Byte.hl = d[0];
			aux.Byte.hh = d[1]; 
			aux.Byte.ll = d[2];
			aux.Byte.lh = d[3];
			break;

		case INV_BOTH_E:
			aux.Byte.ll = d[0];
			aux.Byte.lh = d[1]; 
			aux.Byte.hl = d[2];
			aux.Byte.hh = d[3];
			break;
	}

	exp = (aux.Long & 0x7F800000) >> 23;	// get exponent
	aux.Long &= 0x007FFFFF;					// leave the fraction only (supposed positive)
	aux.Byte.hl |= 0x80;					// set the implicit 1
	
	if(exp >= 127)
	{
		exp -= 127;							// remove bias
		exp = 23 - dp - exp;				// compute shift
	}
	else
	{
		exp = 127 - exp;					// remove bias
		exp = 23 - dp + exp;				// compute shift
	}

	aux.Long = aux.Long >> exp;
	if(aux.Long > 0x00000FFF) aux.Long = 0x00000FFF;

	write_flash(address, (unsigned short)aux.Long);
}
//--------------------------------------------------------
void Modbus_Exit(void)
{
	Stop_UART_1();
	Flag_Main_Modbus_LP_T10ms_off = 1; // Low power allowed
	Modbus_State = MODBUS_INIT;
}
//--------------------------------------------------------
// State machine
//--------------------------------------------------------
void modbus_task(void)
{
	switch(Modbus_State)
	{
		//--------------------------------------------------------
		// Initialize
		//--------------------------------------------------------
		case MODBUS_INIT:
			Flag_Main_Modbus_LP_T10ms_off = 1; // Low power allowed
			if(!read_flash(ID_Mod_On)) break;
			Flag_Main_Modbus_LP_T10ms_off = 0;	// Low power not allowed

			Modbus_Aux = read_flash(ID_Baud);
			if(read_flash(ID_Bits)) Modbus_Aux |= U1_BITS_8;
			if(read_flash(ID_Par) == ODD_E) Modbus_Aux |= U1_PAR_ENA;
			else if(read_flash(ID_Par) == EVEN_E) Modbus_Aux |= (U1_PAR_ENA + U1_PAR_EVEN);

			Start_UART_1((char)Modbus_Aux);
			if(Flag_Main_Power_Fail)			// This avoids atomicity problem during power fail int.
			{
				Modbus_Exit();
				break;
			}
			Modbus_State++;
			break;

		//--------------------------------------------------------
		// Start reception
		//--------------------------------------------------------
		case MODBUS_START:
			if(!read_flash(ID_Mod_On))
			{
				Modbus_Exit();
 				break;
			}

			Rx_int_disable_UART_1();
			Uart_Flag_Error = 0;
			Modbus_Rx_Index = 0;					// Point to the first byte
			Modbus_Flags_Rx_Frame = 0;
			Modbus_Flags_Broadcast = 0;
			Modbus_T_3_5= T_3_5;
			Modbus_T_1_5= T_1_5;
			Rx_int_enable_UART_1();

			Modbus_State++;
			break;

		//--------------------------------------------------------
		// Wait for a frame
		//--------------------------------------------------------
		case MODBUS_IDLE:
			if(!Modbus_Flags_Rx_Frame)
			{
				if(Timer_Comm_Fail == 0)
					Modbus_Flags_Master_Error = 1; 	// Fail safe feature
				break;
			}

			if(Uart_Flag_Error)						// Any error during reception?
			{
				Modbus_State = MODBUS_START;
				break;
			}

			if(read_flash(ID_Protocol) == RTU_E)
			{
				Modbus_Aux = ((unsigned short)Modbus_Buffer[Modbus_Rx_Index - 1] << 8)	// Get CRC
							| (unsigned short)Modbus_Buffer[Modbus_Rx_Index - 2];

				if((Modbus_Rx_Index <= 2)										// Validate Index
				|| (Modbus_Aux != CRC16( Modbus_Buffer, Modbus_Rx_Index - 2)))	// Compute and compare CRC
				{
					Modbus_State = MODBUS_START;
					break;
				}
				Modbus_T_3_5= T_3_5;								// Interframe time
			}
			else
			{
				if(chk_lrc(&Modbus_Buffer[1], Modbus_Rx_Index-3))	// Compute and compare LRC
				{
					Modbus_State = MODBUS_START;
					break;
				}

				ascii_to_rtu();
			}

			Modbus_State++;
			break;

		//--------------------------------------------------------
		// Execute
		//--------------------------------------------------------
		case MODBUS_EXE:
			if(read_flash(ID_Protocol) == RTU_E)
				if(Modbus_T_3_5) break;					// Still waiting interframe time?

			if(	((unsigned short)Modbus_Buffer[MB_ADDR] != read_flash(ID_Mod_Addr))	// Check address
			&&	((unsigned short)Modbus_Buffer[MB_ADDR] != 0) )
			{
				Modbus_State = MODBUS_START;
				break;
			}

			if(Modbus_Buffer[MB_ADDR] == 0) Modbus_Flags_Broadcast = 1;

			Timer_Comm_Fail = 0xFFFF; // =32s
			Modbus_Flags_Master_Error = 0;

			switch(Modbus_Buffer[MB_FUNCTION])
			{
				//--------------------------------------------------------------
				case MB_F_READ_COILS:
				case MB_F_READ_DISCRETE_INPUTS:

					Modbus_Size = ((unsigned short)Modbus_Buffer[MB_READ_QUANT_HI]) << 8 | Modbus_Buffer[MB_READ_QUANT_LOW]; 

					Modbus_Aux = Modbus_Size >> 3;		// Bits to bytes
					if(Modbus_Size & 0x07) Modbus_Aux++;

					if(	(Modbus_Size > 0x07D0)			// Too many required?
					||	(Modbus_Aux == 0) )				// 0 byte required?
					{
						MB_Format_Exception(0X03);
						break;
					}

					Modbus_Address = ((unsigned short)Modbus_Buffer[MB_READ_START_HI]) << 8 | Modbus_Buffer[MB_READ_START_LOW]; 

          if(Modbus_Buffer[MB_FUNCTION] == MB_F_READ_COILS)
          {
            if((Modbus_Address >= MB_FIRST_COIL) && (Modbus_Address <= MB_LAST_COIL))		// coils
            {
              if((Modbus_Address + Modbus_Size - 1) > MB_LAST_COIL)		// End address out of range?
              {
                MB_Format_Exception(0X02);
                break;
              }
              MB_Format_Data_8(Coils, (char)(Modbus_Address-MB_FIRST_COIL));
            }
          }
					else if(Modbus_Buffer[MB_FUNCTION] == MB_F_READ_DISCRETE_INPUTS)
          {
            if((Modbus_Address >= MB_FIRST_DISCRETE_INPUT) && (Modbus_Address <= MB_LAST_DISCRETE_INPUT))	// disc. inputs
            {
              if((Modbus_Address + Modbus_Size - 1) > MB_LAST_DISCRETE_INPUT)// End address out of range?
              {
                MB_Format_Exception(0X02);
                break;
              }
              map_discrete_inputs();
              MB_Format_Data_8(Discrete_Inputs, (char)(Modbus_Address-MB_FIRST_DISCRETE_INPUT));
            }
          }
					else
					{
						MB_Format_Exception(0X02);
					}

					break;

				//--------------------------------------------------------------
				case MB_F_READ_HOLDING_REG:
				case MB_F_READ_INPUT_REG:

					Modbus_Size = ((unsigned short)Modbus_Buffer[MB_READ_QUANT_HI]) << 8 | Modbus_Buffer[MB_READ_QUANT_LOW]; 
					if(	(Modbus_Size > 0X7D)	// Too many required?
					||	(Modbus_Size == 0) )	// 0 byte required?
					{
						MB_Format_Exception(0X03);
						break;
					}

					Modbus_Address = ((unsigned short)Modbus_Buffer[MB_WRITE_ADDR_HI]) << 8 | Modbus_Buffer[MB_WRITE_ADDR_LO]; 

          if(Modbus_Buffer[MB_FUNCTION] == MB_F_READ_HOLDING_REG)
          {
            if((Modbus_Address >= MB_FIRST_16_HOLD_REG) && (Modbus_Address <= MB_LAST_16_HOLD_REG))	// 16bit ints
            {
              if((Modbus_Address + Modbus_Size - 1) > MB_LAST_16_HOLD_REG)	// End address out of range?
              {
                MB_Format_Exception(0X02);
                break;
              }
              MB_Format_Data_16(&flash_copy[Modbus_Address - MB_FIRST_16_HOLD_REG + MB_16_HOLD_REG_FLASH_OFFSET], Modbus_Size);
            }
  					else if((Modbus_Address >= MB_FIRST_32FP_HOLD_REG) && (Modbus_Address <= MB_LAST_32FP_HOLD_REG))								// 32bit floats
            {
              // The requested size for a 32bit float is 2 (x16bit registers)
              if((Modbus_Address + Modbus_Size - 1) > MB_LAST_32FP_HOLD_REG)	// End address out of range?
              {
                MB_Format_Exception(0X02);
                break;
              }
              // Address and size are divided by 2 (one 32bit float are 2 16bit registers)
              MB_Format_Data_32fp(Modbus_Address/2, Modbus_Size/2);
            }
          }
					else if(Modbus_Buffer[MB_FUNCTION] == MB_F_READ_INPUT_REG)
          {
            if((Modbus_Address >= MB_FIRST_32FP_INP_REG) && (Modbus_Address <= MB_LAST_32FP_INP_REG))								// 32bit floats
            {
              // The requested size for a 32bit float is 2 (x16bit registers)
              if((Modbus_Address + Modbus_Size - 1) > MB_LAST_32FP_INP_REG)	// End address out of range?
              {
                MB_Format_Exception(0X02);
                break;
              }
              // Address and size are divided by 2 (one 32bit float are 2 16bit registers)
              MB_Format_Data_32fp(Modbus_Address/2, Modbus_Size/2);
            }
          }
					else														// Unknown address
					{
						MB_Format_Exception(0X02);
					}

					break;

				//--------------------------------------------------------------
				case MB_F_WRITE_SINGLE_COILS:

					Modbus_Address = ((unsigned short)Modbus_Buffer[MB_WRITE_ADDR_HI]) << 8 | Modbus_Buffer[MB_WRITE_ADDR_LO];

					if((Modbus_Address < MB_FIRST_COIL) || (Modbus_Address > MB_LAST_COIL))
					{
						MB_Format_Exception(0X02);
						break;
					}

					Modbus_Address -= MB_FIRST_COIL;	  // Remove offset

					if(Modbus_Buffer[MB_WRITE_REG_HI])  // Set or clear?
					{
						Coils[Modbus_Address >> 3] |= (char)num_to_bit[Modbus_Address & 0x07];
					}
					else
					{
						Coils[Modbus_Address >> 3] &= ~(char)num_to_bit[Modbus_Address & 0x07];
					}

					map_coils_out();
					MB_Format_Echo(1+1+2+2);
					break;
          
				//--------------------------------------------------------------
        case MB_F_WRITE_MULT_COILS:

					Modbus_Size = ((unsigned short)Modbus_Buffer[MB_READ_QUANT_HI]) << 8 | Modbus_Buffer[MB_READ_QUANT_LOW]; 

					Modbus_Aux = Modbus_Size >> 3;		// Bits to bytes
					if(Modbus_Size & 0x07) Modbus_Aux++;

					if(	(Modbus_Size > 0x07D0)			// Too many required?
					||	(Modbus_Aux == 0) )				// 0 byte required?
					{
						MB_Format_Exception(0X03);
						break;
					}          
          
          Modbus_Address = ((unsigned short)Modbus_Buffer[MB_WRITE_ADDR_HI]) << 8 | Modbus_Buffer[MB_WRITE_ADDR_LO];

					if((Modbus_Address >= MB_FIRST_COIL) && (Modbus_Address <= MB_LAST_COIL))
          {
              if((Modbus_Address + Modbus_Size - 1) > MB_LAST_COIL)// End address out of range?
              {
                MB_Format_Exception(0X02);
                break;
              }
              Modbus_Address -= MB_FIRST_COIL;							// Remove offset

              MB_Write_Mult_Coils((char)Modbus_Address);

              map_coils_out();
              MB_Format_Echo(1+1+2+2);
          }
					else
          {
						MB_Format_Exception(0X02);
					}
					break;
          
				//--------------------------------------------------------------
				case MB_F_WRITE_SINGLE_HOLDING_REG:

					Modbus_Address = ((unsigned short)Modbus_Buffer[MB_WRITE_ADDR_HI]) << 8 | Modbus_Buffer[MB_WRITE_ADDR_LO];		
					if((Modbus_Address >= MB_FIRST_16_HOLD_REG) && (Modbus_Address <= MB_LAST_16_HOLD_REG))		// 16bit ints
					{
						write_flash( Modbus_Address - MB_FIRST_16_HOLD_REG + MB_16_HOLD_REG_FLASH_OFFSET,
									(((unsigned short)Modbus_Buffer[MB_WRITE_REG_HI])<<8)
									| (unsigned short)Modbus_Buffer[MB_WRITE_REG_LO]);
						MB_Format_Echo(1+1+2+2);
						update_flash_safe();									// Update flash
					}
          else if((Modbus_Address >= MB_FIRST_DAC_16_HOLD_REG) && (Modbus_Address <= MB_LAST_DAC_16_HOLD_REG))		// 16bit ints
					{
						Analog_Output[Modbus_Address - MB_FIRST_DAC_16_HOLD_REG] =
									(((unsigned short)Modbus_Buffer[MB_WRITE_REG_HI])<<8)
									| (unsigned short)Modbus_Buffer[MB_WRITE_REG_LO];
						MB_Format_Echo(1+1+2+2);
					}
					else														// Unknown address
					{
						MB_Format_Exception(0X02);
					}

					break;

				//--------------------------------------------------------------
				case MB_F_WRITE_MULT_HOLDING_REG:

					Modbus_Size = ((unsigned short)Modbus_Buffer[MB_WRITE_QUANT_HI]) << 8 | Modbus_Buffer[MB_WRITE_QUANT_LOW]; 
					if(	(Modbus_Size > 0X7B)	// Too many required?
					||	(Modbus_Size == 0) )	// 0 byte required?
					{
						MB_Format_Exception(0X03);
						break;
					}

					Modbus_Address = ((unsigned short)Modbus_Buffer[MB_WRITE_ADDR_HI]) << 8 | Modbus_Buffer[MB_WRITE_ADDR_LO];		

					if((Modbus_Address >= MB_FIRST_16_HOLD_REG) && (Modbus_Address <= MB_LAST_16_HOLD_REG))		// 16bit ints
					{
						if((Modbus_Address + Modbus_Size - 1) > MB_LAST_16_HOLD_REG)	// End address out of range?
						{
							MB_Format_Exception(0X02);
							break;
						}
						
						Modbus_Address = Modbus_Address - MB_FIRST_16_HOLD_REG + MB_16_HOLD_REG_FLASH_OFFSET;
						Modbus_Aux = 0;						
						while(Modbus_Aux < (Modbus_Size*2))
						{	write_flash(Modbus_Address,
									(((unsigned short)Modbus_Buffer[MB_WRITE_DATA + Modbus_Aux])<<8)
									| (unsigned short)Modbus_Buffer[MB_WRITE_DATA + Modbus_Aux + 1]);
							Modbus_Address++;
							Modbus_Aux += 2; //each register size is 2 bytes
						}
						MB_Format_Echo(1+1+2+2);
						update_flash_safe();									// Update flash
					}

					else if((Modbus_Address >= MB_FIRST_DAC_16_HOLD_REG) && (Modbus_Address <= MB_LAST_DAC_16_HOLD_REG))		// 16bit ints
					{
						if((Modbus_Address + Modbus_Size - 1) > MB_LAST_DAC_16_HOLD_REG)	// End address out of range?
						{
							MB_Format_Exception(0X02);
							break;
						}
						
						Modbus_Address = Modbus_Address - MB_FIRST_DAC_16_HOLD_REG;
						Modbus_Aux = 0;						
						while(Modbus_Aux < (Modbus_Size*2))
						{
						    Analog_Output[Modbus_Address] =
									(((unsigned short)Modbus_Buffer[MB_WRITE_DATA + Modbus_Aux])<<8)
									| (unsigned short)Modbus_Buffer[MB_WRITE_DATA + Modbus_Aux + 1];
							Modbus_Address++;
							Modbus_Aux += 2; //each register size is 2 bytes
						}
						MB_Format_Echo(1+1+2+2);
						update_flash_safe();									// Update flash
					}

					else if((Modbus_Address >= MB_FIRST_32FP_HOLD_REG) && (Modbus_Address <= MB_LAST_32FP_HOLD_REG))// 32bit floats
					{
            // The requested size for a 32bit float is 2 (x16bit registers)
            if((Modbus_Address + Modbus_Size - 1) > MB_LAST_32FP_HOLD_REG)	// End address out of range?
						{
							MB_Format_Exception(0X02);
							break;
						}

						Modbus_Address -= MB_FIRST_32FP_HOLD_REG;
						Modbus_Size <<= 1; // one float is sent as 2 16bit registers of 2 bytes each (4bytes)
						Modbus_Aux = 0;						
						while(Modbus_Aux < Modbus_Size)
						{	
							float_to_flash(Modbus_Address/2, &Modbus_Buffer[MB_WRITE_DATA + Modbus_Aux]);
							Modbus_Address += 2;
							Modbus_Aux += 4;// one float is sent as 2 16bit registers of 2 bytes each (4bytes)
						}
						MB_Format_Echo(1+1+2+2);
						update_flash_safe();									// Update flash
					}
					else														// Unknown address
					{
						MB_Format_Exception(0X02);
					}

					break;

				//--------------------------------------------------------------
				default:
					MB_Format_Exception(0X01);
			}

			Modbus_T_3_5 = 10;									// Wait RS232/485 converter to release the line, 5ms=0.5ms*10 
			Modbus_State++;
			break;

		//--------------------------------------------------------
		// Send
		//--------------------------------------------------------
		case MODBUS_SEND_1:
			if(Modbus_Flags_Broadcast)
			{
				Modbus_State = MODBUS_START;
				break;
			}
			if(Modbus_T_3_5) break;
			RS485_1_DIR = 1;									// Set RS485 direction to Tx
			send_data_UART_1(Modbus_Buffer, Modbus_Counter);
			Modbus_State++;
			break;

		case MODBUS_SEND_2:
			if(!send_finished_UART_1()) break;							// Wait for the last byte to be sent
			RS485_1_DIR = 0;									// Set RS485 direction to Rx
			Modbus_State = MODBUS_START;
			break;

		//--------------------------------------------------------
		// Just in case
		//--------------------------------------------------------
		default:
			Modbus_State = MODBUS_START;
	}
}

