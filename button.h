#include "def.h"

void process_buttons(void);
void clear_buttons(void);

#define T_FAST_MODE		200		// 2s = 10ms * 200

#define Flag_Button_I	Flags_Buttons.Bit._0
#define Flag_Button_M	Flags_Buttons.Bit._1
#define Flag_Button_D	Flags_Buttons.Bit._2
#define Flag_Button_R	Flags_Buttons.Bit._3


#define BUTTON_MASK		0X0F

extern volatile char Timer_Buttons;
extern volatile tREG08 Flags_Buttons;

