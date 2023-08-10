#ifndef __UART_1_H__
#define __UART_1_H__

void Stop_UART_1(void);
void Start_UART_1(char opt);
void Rx_int_enable_UART_1(void);
void Rx_int_disable_UART_1(void);
void send_data_UART_1(char * buffer, char size);
char send_finished_UART_1(void);

// Buffer size
// 1,2,4,8,16,32,64,128 or 256 bytes are allowed buffer sizes

#define Uart_Flag_Error		Uart_Flags.Bit._0

enum {
  U1_SPEED_9K6,
  U1_SPEED_19K2,
  U1_SPEED_38K4,
  U1_SPEED_115K
};

#define U1_SPEED_MASK		0x03
#define U1_BITS_8			  0x10
#define U1_PAR_EVEN			0x40
#define U1_PAR_ENA			0x80

#define RS485_1_DIS			P4OUT_bit.P6
#define RS485_1_DIR			P1OUT_bit.P7

extern volatile tREG08 Uart_Flags;

#endif /* __UART_1_H__ */
