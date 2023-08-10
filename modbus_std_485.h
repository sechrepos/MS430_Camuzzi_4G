#include "main.h"

// Forward declarations
void modbus_task(void);
void modbus_500us_tick(void);
void modbus_init(void);

// States
enum{
MODBUS_INIT,
MODBUS_START,
MODBUS_IDLE,
MODBUS_EXE,
MODBUS_SEND_1,
MODBUS_SEND_2
};

#define RTU_START	':'
#define CR			0x0D
#define LF			0x0A

// Flags
#define Modbus_Flags_Broadcast		Modbus_Flags.Bit._0
#define Modbus_Flags_Rx_Frame		Modbus_Flags.Bit._1
#define Modbus_Flags_Rx_Error		Modbus_Flags.Bit._2
#define Modbus_Flags_Master_Error	Modbus_Flags.Bit._3

// Buffer size
#define MB_BUF_SIZE			513

// Modbus protocol bytes
#define MB_ADDR				0
#define MB_FUNCTION			1
// Modbus protocol bytes (rx)
#define MB_READ_START_HI	2
#define MB_READ_START_LOW	3
#define MB_READ_QUANT_HI	4
#define MB_READ_QUANT_LOW	5

#define MB_WRITE_ADDR_HI	2
#define MB_WRITE_ADDR_LO	3
#define MB_WRITE_QUANT_HI	4
#define MB_WRITE_QUANT_LOW	5
#define MB_WRITE_BYTE_COUNT	6
#define MB_WRITE_DATA		7

#define MB_WRITE_REG_HI		4
#define MB_WRITE_REG_LO		5

#define MB_EXT_FUNC			2

// Modbus protocol bytes (tx)
#define MB_BYTE_COUNT		2
#define MB_REGISTERS		3

// Modbus protocol bytes (rx)
#define MB_RX_REGISTERS		7


// Modbus functions
#define MB_F_READ_COILS					0X01
#define MB_F_READ_DISCRETE_INPUTS		0X02
#define MB_F_READ_HOLDING_REG			0X03
#define MB_F_READ_INPUT_REG				0X04
#define MB_F_WRITE_SINGLE_COILS			0X05
#define MB_F_WRITE_MULT_COILS			0X0F
#define MB_F_WRITE_SINGLE_HOLDING_REG	0X06
#define MB_F_WRITE_MULT_HOLDING_REG		0X10
#define MB_F_EXTENDED					0X12


#define FLASH_32FP_SIZE		8			// 7001 to 7xxx
#define MB_16_HOLD_REG_FLASH_OFFSET	16	// First flash ID to 3001's ID
#define REG_32FP_SIZE		2			// 7xxx+1 (read only)
#define FLASH_16_SIZE		5			// 3001 to 3xxx
#define DAC_16_SIZE		              1		// 3101 to 31xx (read & write) (not use)

// Modbus (others)
/*
Registers	Type		Description
COIL		Digital or Discrete, 1 bit
DISC.INPUT	Digital or Discrete, 1 bit
INTEGER		16 Bit integers
LONG		32 Bit integers
FLOAT		32 Bit IEEE floating point
*/
#define MB_FIRST_COIL			        0
#define MB_LAST_COIL              7

#define MB_FIRST_DISCRETE_INPUT	  0
#define MB_LAST_DISCRETE_INPUT	  15

#define MB_FIRST_16_HOLD_REG	    (unsigned short)(100-1)
#define MB_LAST_16_HOLD_REG		    (unsigned short)(MB_FIRST_16_HOLD_REG + FLASH_16_SIZE - 1)
#define MB_FIRST_DAC_16_HOLD_REG	(unsigned short)(200-1)
#define MB_LAST_DAC_16_HOLD_REG		(unsigned short)(MB_FIRST_DAC_16_HOLD_REG + DAC_16_SIZE - 1)

//#define MB_FIRST_32_HOLD_RER    0
//#define MB_LAST_32_HOLD_REG     0

// The size for a 32bit float is 2x16bits
#define MB_FIRST_32FP_HOLD_REG    0
#define MB_LAST_32FP_HOLD_REG     (MB_FIRST_32FP_HOLD_REG + (2*FLASH_32FP_SIZE) - 1)
#define MB_FIRST_32FP_INP_REG     (MB_LAST_32FP_HOLD_REG + 1)
#define MB_LAST_32FP_INP_REG      (MB_FIRST_32FP_INP_REG + (2*REG_32FP_SIZE) - 1)

// Timeouts
#define T_3_5			8	// 3.5ms=0.5ms*7 (+1 for safety)
#define T_1_5			4	// 1.5ms=0.5ms*3 (+1 for safety)
