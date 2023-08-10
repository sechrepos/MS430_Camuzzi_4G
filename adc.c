#include <io430x16x.h>
#include "def.h"
#include "adc.h"
#include "main.h"
#include "flash.h"

char ADC_State;
volatile short ADC_Timer;
unsigned short ADC_Aux;
tREG08 Flags_ADC;
unsigned short Sensors_Samples[2];
unsigned short Sensors_Samples_0_Avg[SIZE_AVG_BUF];
unsigned short Sensors_Samples_1_Avg[SIZE_AVG_BUF];
unsigned short Battery;
unsigned short Battery_Avg[SIZE_AVG_BUF];

//******************************************************************************
unsigned short sample_math(unsigned short data, char fid)
{
	unsigned long Mult32;

	if(data > read_flash(fid)) data -= read_flash(fid);		// Remove Offset
	else data = 0x0000;

	fid += (ID_Span_0 - ID_Offset_0);						// Compute Span flash_copy Id

	Mult32 = (unsigned long)data * (unsigned long)read_flash(fid);	// Multiply by span (000E,FFFFFFFFFFFF)
	data = (unsigned short)(Mult32 >> 12);					// Recover decimal point (0001,000000000000)

	if(data > 0x0FFF) data = 0X0FFF;

	return data;
}
//******************************************************************************
void init_ADC12(void)
{
	// SHT1			15-12	1111	=1024clks
	// SHT0			11-8	1111	=1024clks
	// MSC			7		x
	// REF2_5V		6		1		=2.5V ref
	// REFON		5		1		=ref on
	// ADC12ON		4		1		=ADC12 on
	// ADC12OVIE	3		0		=OV int. disabled
	// ADC12TOVIE	2		0		=time OV int. disabled
	// ENC			1		0		=ADC12 disabled
	// ADC12SC		0		0		=No samp. and conversion start
	ADC12CTL0 = 0xFF00 + ADC12ON + REFON + REF2_5V;

	// CSTARTADDx	15-12	0000	=1st start address
	// SHSx			11-10	00		=start with ADC12SC bit
	// SHP			9		1		=pulsed mode
	// ISSH			8		0		=not inverted
	// ADC12DIVx	7-5		000		=/1
	// ADC12SSELx	4-3		10		=MCLK
	// CONSEQx		2-1		00		=single channel, single conversion
	// ADC12BUSY	0		x
	ADC12CTL1 = 0x0210;
}

void start_sampling(char channel, char ref)
{
	ADC12CTL0_bit.ENC = 0;
	// EOS			7		x
	// INCHx		3-0		0000	0000=A0, 1010=temp. sensor
	if(ref)
		ADC12MCTL0 = 0x10 | channel;	// SREFx		6-4		001		= VR+=Vref+ & VR-=AVSS
	else
		ADC12MCTL0 = channel;			// SREFx		6-4		000		= VR+=AVCC & VR-=AVSS

	ADC12CTL0 |= ENC + ADC12SC;		// Enable conversions + Get sample
}

unsigned short finish_sampling(void)
{
	while(!(ADC12IFG & 0X0001));	// Wait							
	ADC12CTL0 &= ~(ADC12ON | REFON);// Turn off ADC y Ref
	return ADC12MEM0;
}

void ADC_500us_tick(void)
{
	if(ADC_Timer) ADC_Timer--;
}

void ADC_New_Sample(char ch)
{
	Flags_ADC.Byte |= ch;
}

char ADC_Done(char ch)
{
	if(Flags_ADC.Byte & ch) return 0;
	return 1;
}
//******************************************************************************
void ADC(void)
{
	switch(ADC_State)
	{
		//  --------------------------------------------------------------
		case ADC_WAIT:
			//  --------------------------------------------------------------
			// Get new sample?
			Flag_Main_ADC_LP_T10ms_off = 1;								// Enable low power
			if((Flags_ADC.Byte & ADC_MASK) == 0) break;					// Get sample?
			Flag_Main_ADC_LP_T10ms_off = 0;								// Low power not allowed
			init_ADC12();

			if(Flag_ADC_Get_Pressure)
			{
				ADC_Timer = PRESSURE_SENSOR_TIME;						// Load the turn on timer
				SENSORS_012_ON = 1;										// Enable pressure sensors
				ADC_State = ADC_SAMPLE_PRESSURE;
				break;
			}

			if(Flag_ADC_Get_Battery)
			{
				ADC_Timer = REF_ON_TIME;								// Wait ADC turn on time
				ADC_State = ADC_SAMPLE_BATT;
				break;
			}

			Flags_ADC.Byte &= ~ADC_MASK; // Error, clr all
			ADC_Aux = finish_sampling(); // Turn off ADC
			break;

		case ADC_SAMPLE_BATT:
			if(ADC_Timer) break;											// Wait ADC to be ready
			// Battery ---------------------------------------------------------
	
			// Shift buffer
			for(ADC_Aux = (SIZE_AVG_BUF-1) ; ADC_Aux > 0 ; ADC_Aux--)
			{
				Battery_Avg[ADC_Aux] = Battery_Avg[ADC_Aux-1];
			}

			// Get n consecutive samples and average them
			Battery_Avg[0] = 0;
			for(ADC_Aux = 0; ADC_Aux < NUM_SAMP; ADC_Aux++)
			{
				start_sampling(0x07,VR_AVcc);						// Channel 7 (battery)
				Battery_Avg[0] += finish_sampling();				// Get sample
			}
			Battery_Avg[0] >>= LOG2_NUM_SAMP;						// Sample/8

			if(!Flag_ADC_B_Init)
			{
				// Set the buffer with the very first sample
				for(ADC_Aux = 1; ADC_Aux < SIZE_AVG_BUF; ADC_Aux++)
				{
					Battery_Avg[ADC_Aux] = Battery_Avg[0];
				}
			}
	
			// Average the last m samples
			Battery = 0;
			for(ADC_Aux = 0; ADC_Aux < SIZE_AVG_BUF; ADC_Aux++)
			{
				Battery += Battery_Avg[ADC_Aux];
			}
			Battery >>= LOG2_SIZE_AVG_BUF;

			// Do the math
			Battery = sample_math(Battery, ID_Offset_Bat);


			//------------------------------------------------------------------
			// Set init flag
			if(!Flag_ADC_B_Init) Flag_ADC_B_Init = 1;
			Flag_ADC_Get_Battery = 0;
			ADC_State = ADC_WAIT;
			break;

		//  --------------------------------------------------------------
		case ADC_SAMPLE_PRESSURE:
			if(ADC_Timer) break;											// Wait sensors and ADC to be ready

			// SENSOR 0 --------------------------------------------------------

			// Shift buffer
			for(ADC_Aux = (SIZE_AVG_BUF-1) ; ADC_Aux > 0 ; ADC_Aux--)
			{
				Sensors_Samples_0_Avg[ADC_Aux] = Sensors_Samples_0_Avg[ADC_Aux-1];
			}

			// Get n consecutive samples and average them
			Sensors_Samples_0_Avg[0] = 0;
			for(ADC_Aux = 0; ADC_Aux < NUM_SAMP; ADC_Aux++)
			{
				start_sampling(0x00,VR_Vref);										// Channel 0
				Sensors_Samples_0_Avg[0] += finish_sampling();				// Get sample
			}
			Sensors_Samples_0_Avg[0] >>= LOG2_NUM_SAMP;						// Sample/8

			if(!Flag_ADC_P_Init)
			{	// Set the buffer with the very first sample
				for(ADC_Aux = 1; ADC_Aux < SIZE_AVG_BUF; ADC_Aux++)
				{
					Sensors_Samples_0_Avg[ADC_Aux] = Sensors_Samples_0_Avg[0];
				}
			}			

			// Average the last m samples
			Sensors_Samples[0] = 0;
			for(ADC_Aux = 0; ADC_Aux < SIZE_AVG_BUF; ADC_Aux++)
			{
				Sensors_Samples[0] += Sensors_Samples_0_Avg[ADC_Aux];
			}
			Sensors_Samples[0] >>= LOG2_SIZE_AVG_BUF;

			// Do the math
			Sensors_Samples[0] = sample_math(Sensors_Samples[0], ID_Offset_0);

			// SENSOR 1 --------------------------------------------------------

			// Shift buffer
			for(ADC_Aux = (SIZE_AVG_BUF-1) ; ADC_Aux > 0 ; ADC_Aux--)
			{
				Sensors_Samples_1_Avg[ADC_Aux] = Sensors_Samples_1_Avg[ADC_Aux-1];
			}

			// Get n consecutive samples and average them
			Sensors_Samples_1_Avg[0] = 0;
			for(ADC_Aux = 0; ADC_Aux < NUM_SAMP; ADC_Aux++)
			{
				start_sampling(0x01,1);										// Channel 1
				Sensors_Samples_1_Avg[0] += finish_sampling();				// Get sample
			}
			Sensors_Samples_1_Avg[0] >>= LOG2_NUM_SAMP;						// Sample/8

			if(!Flag_ADC_P_Init)
			{	// Set the buffer with the very first sample
				for(ADC_Aux = 1; ADC_Aux < SIZE_AVG_BUF; ADC_Aux++)
				{
					Sensors_Samples_1_Avg[ADC_Aux] = Sensors_Samples_1_Avg[0];
				}
			}

			// Average the last m samples
			Sensors_Samples[1] = 0;
			for(ADC_Aux = 0; ADC_Aux < SIZE_AVG_BUF; ADC_Aux++)
			{
				Sensors_Samples[1] += Sensors_Samples_1_Avg[ADC_Aux];
			}
			Sensors_Samples[1] >>= LOG2_SIZE_AVG_BUF;

			// Do the math
			Sensors_Samples[1] = sample_math(Sensors_Samples[1], ID_Offset_1);

			//------------------------------------------------------------------

			// Set init flag
			if(!Flag_ADC_P_Init) Flag_ADC_P_Init = 1;

			// Done
			SENSORS_012_ON = 0;												// Disable sensors
			Flag_ADC_Get_Pressure = 0;										// Clr flag
			ADC_State = ADC_WAIT;
			break;

		// Default -------------------------------------------------------------
		default:
			ADC_State = ADC_WAIT;
			break;
	}
}