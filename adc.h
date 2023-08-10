void ADC(void);
void ADC_500us_tick(void);
void ADC_New_Sample(char);
char ADC_Done(char);

#define Flag_ADC_Get_Pressure		Flags_ADC.Bit._0
#define Flag_ADC_Get_Battery		Flags_ADC.Bit._1
#define PRESSURE	0x01
#define BATTERY		0x02
#define ADC_MASK	0x03
#define Flag_ADC_P_Init				Flags_ADC.Bit._6
#define Flag_ADC_B_Init				Flags_ADC.Bit._7
/*
struct s_cal{
	unsigned short xa;
	unsigned short xb;
	unsigned short ya;
	unsigned short yb;
	unsigned short mh;
	unsigned short ml;
	short c;
};
*/
#define BAT_A				(128*18)	// 4096=32V (128/V)
#define BAT_B				(128*24)	// 4096=32V (128/V)

enum{VR_AVcc, VR_Vref};

enum{
ADC_WAIT,
ADC_SAMPLE_PRESSURE,
ADC_SAMPLE_BATT
};

#define SIZE_AVG_BUF			8
#define LOG2_SIZE_AVG_BUF		3

#define NUM_SAMP				4
#define LOG2_NUM_SAMP			2

#define PRESSURE_SENSOR_TIME	100	// Reload the turn on timer (100*500us=50ms)
#define REF_ON_TIME				40	// Reload the turn on timer (40*500us=20ms)

#define SENSORS_012_ON			P4OUT_bit.P4
#define SENSORS_34_ON			P4OUT_bit.P3
#define SENSORS_56_ON			P4OUT_bit.P7

extern unsigned short Sensors_Samples[];
extern unsigned short Battery;
extern unsigned short Battery_Avg[];