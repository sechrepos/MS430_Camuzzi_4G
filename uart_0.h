#ifndef __UART_0_H__
#define __UART_0_H__

#include <stdint.h>

void Stop_UART_0(void);
void Start_UART_0(char opt);
void Rx_int_enable_UART_0(void);
void Rx_int_disable_UART_0(void);
char Rx_Byte_UART_0(uint8_t *data);
void send_data_UART_0(void);
void send_data_UART_0_from_ptr(uint8_t *buf, uint8_t size);
char send_finished_UART_0(void);
void cue_data_UART_0(uint8_t *buffer, uint8_t size);
void cue_byte_UART_0(char d);
void clr_data_UART_0(void);
void cue_string_UART_0(uint8_t *s);
void send_string_UART_0(uint8_t *s);
void uart_enable_RTS(void);
void uart_disable_RTS(void);

#define TX0_BUFFER_SIZE		32
// Buffer size
// 1,2,4,8,16,32,64,128 or 256 bytes are allowed buffer sizes
#define RX0_BUFFER_SIZE		32
#define RX0_BUFFER_MASK		(RX0_BUFFER_SIZE - 1)
#define RX0_FREE_SPACE		12 // 9 bytes can be received after setting RTS (@9600bps) 

enum {
  U0_SPEED_9K6,
  U0_SPEED_19K2,
  U0_SPEED_38K4,
  U0_SPEED_115K
};

#define U0_SPEED_MASK		0x03
#define U0_BITS_8			  0x10
#define U0_PAR_EVEN			0x40
#define U0_PAR_ENA			0x80

#define U0_RTS 				P4OUT_bit.P0
#define U0_CTS 				P4OUT_bit.P1

#endif /* __UART_0_H__ */
