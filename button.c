#include <io430x16x.h>
#include "def.h"
#include "button.h"

#pragma diag_suppress = Pa082 // avoid volatile undefined behavior warnings

// Locales variables
volatile tREG08 Flags_Buttons;
volatile char Timer_Buttons;
volatile char Aux_Buttons[3];

//******************************************************************************
void process_buttons(void)
{
	Aux_Buttons[0] = (P1IN & BUTTON_MASK);

	if(Aux_Buttons[1] != Aux_Buttons[0])		// Any change between real buttons and aux?
	{
		Aux_Buttons[1] = Aux_Buttons[0];		// Get changes into an aux
	}
	else
	{
		if(Aux_Buttons[2] != Aux_Buttons[1])									// Any change between aux and final flags?
		{
			Aux_Buttons[2] = Aux_Buttons[1];									// Get aux into flags
			Timer_Buttons = T_FAST_MODE;										// Reload fast timer (slow down)
		}
		else																	// No change
		{	
			if(Timer_Buttons) Timer_Buttons--;									// Dec fast timer			
		}
	}

	Flags_Buttons.Byte = Aux_Buttons[2];
}
//******************************************************************************
void clear_buttons(void)
{
	Aux_Buttons[0] = P1IN & BUTTON_MASK;
	Aux_Buttons[1] = Aux_Buttons[0];
	Aux_Buttons[2] = Aux_Buttons[1];
	Flags_Buttons.Byte = Aux_Buttons[2];
}