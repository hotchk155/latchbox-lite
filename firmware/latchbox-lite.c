//////////////////////////////////////////////////////////////
//
// LATCHBOX
// Latch utility for accessibility switches
// hotchk155/2019
// Sixty Four Pixels Limited / OneSwitch.org
//
// This work is distibuted under terms of Creative Commons 
// License BY-NC-SA (Attribution, Non-commercial, Share-Alike)
// https://creativecommons.org/licenses/by-nc-sa/4.0/
//
//////////////////////////////////////////////////////////////

//
// HEADER FILES
//
#include <system.h>

// PIC CONFIG BITS
// - RESET INPUT DISABLED
// - WATCHDOG TIMER OFF
// - INTERNAL OSC
#pragma DATA _CONFIG1, _FOSC_INTOSC & _WDTE_OFF & _MCLRE_OFF &_CLKOUTEN_OFF
#pragma DATA _CONFIG2, _WRT_OFF & _PLLEN_OFF & _STVREN_ON & _BORV_19 & _LVP_OFF
#pragma CLOCK_FREQ 16000000

//
// TYPE DEFS
//
typedef unsigned char byte;

//
// MACRO DEFS
//

// outputs
#define P_LED_PWR	porta.1
#define P_LED_OUT	porta.0
#define P_RELAY		porta.2
#define P_POWER		porta.4
#define P_SWITCH	porta.5

// timeouts
#define AUTO_POWER_OFF_MS		(2 * 60 * 1000)		// time from last input to auto power off
#define POWER_WARN_MS			(10 * 1000)			// warning time before auto power off
#define DEBOUNCE_MS 			20					

#define TIMER_0_INIT_SCALAR		5	// Timer 0 is an 8 bit timer counting at 250kHz
									// using this init scalar means that rollover
//
// GLOBAL DATA
//

// timer stuff
volatile byte tick_flag = 0;
volatile unsigned int timer_init_scalar = 0;
volatile unsigned long systemTicks = 0; // each system tick is 1ms


////////////////////////////////////////////////////////////
// INTERRUPT HANDLER CALLED WHEN CHARACTER RECEIVED AT 
// SERIAL PORT OR WHEN TIMER 1 OVERLOWS
void interrupt( void )
{
	// timer 0 rollover ISR. Maintains the count of 
	// "system ticks" that we use for key debounce etc
	if(intcon.2)
	{
		tmr0 = TIMER_0_INIT_SCALAR;
		systemTicks++;
		tick_flag = 1;	
		intcon.2 = 0;
	}

}

////////////////////////////////////////////////////////////
// MAIN
void main()
{ 
	// osc control / 16MHz / internal
	osccon = 0b01111010;

	// configure io
	trisa = 0b11101000;              	
    trisc = 0b11111111;              
	ansela = 0b00000000;
	anselc = 0b00000000;
	porta=0;
	portc=0;
	
	// Configure timer 0 (controls systemticks)
	// 	timer 0 runs at 4MHz
	// 	prescaled 1/16 = 250kHz
	// 	rollover at 250 = 1kHz
	// 	1ms per rollover	
	option_reg.5 = 0; // timer 0 driven from instruction cycle clock
	option_reg.3 = 0; // timer 0 is prescaled
	option_reg.2 = 0; // }
	option_reg.1 = 1; // } 1/16 prescaler
	option_reg.0 = 1; // }
	intcon.5 = 1; 	  // enabled timer 0 interrrupt
	intcon.2 = 0;     // clear interrupt fired flag
	
	wpua.5=1;
	option_reg.7=0;
	
	// enable interrupts	
	intcon.7 = 1; //GIE
	intcon.6 = 1; //PEIE

	unsigned long activity_timeout = AUTO_POWER_OFF_MS;
	int debounce_timeout = 0;
	int input_state = 1;
	int output_state = 0;
	
	// quick blink of both LEDs
	P_LED_PWR = 1;
	P_LED_OUT = 1;
	delay_ms(20);
	P_LED_OUT = 0;
	P_LED_PWR = 0;
	
	// need to hold button...
	delay_s(1);
	
	// ...before power is latched on
	P_POWER = 1;
	
	
	for(;;) {		
		if(tick_flag) {	// once a ms
			tick_flag = 0;
						
			if(activity_timeout) {
				if(!--activity_timeout) {
					// turn power off
					break;
				}
			}

			if(debounce_timeout) {
				--debounce_timeout;
			}	
			
			// Power LED handling
			if(activity_timeout < POWER_WARN_MS) {
				P_LED_PWR = !!(activity_timeout & 0x80);
			}
			else {
				P_LED_PWR = !!(activity_timeout & 0x01); // 50% duty
			}
			
			// Output LED handling
			if(output_state) {
				P_LED_OUT = !!(activity_timeout & 0x01); // 50% duty
			}			
			else {
				P_LED_OUT = 0;
			}
			
		}
		
		// check switch input
		if(!debounce_timeout) { // finished debouncing 
			if(input_state) { // switch was pressed when we last looked
				if(P_SWITCH) {
					// switch is now released
					input_state = 0;
					debounce_timeout = DEBOUNCE_MS;
					activity_timeout = AUTO_POWER_OFF_MS;
				}
			}
			else {
				if(!P_SWITCH) {
					// switch is now pressed
					input_state = 1;
					debounce_timeout = DEBOUNCE_MS;
					activity_timeout = AUTO_POWER_OFF_MS;
					output_state = !output_state;
					P_RELAY = output_state;
				}
			}
		}
	}
	
	// turn power off
	P_RELAY = 0;
	P_POWER = 0;
	
	// Hang till power is properly off and we shut down
	for(;;);
}