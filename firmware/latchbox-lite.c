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
// V1 - JUL19
// V2 - 25OCT19 - change to auto power off handling
// V3 - 27NOV19 - prevent accidental immediate power off
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

#define FIRMWARE_VERSION 2

//
// TYPE DEFS
//
typedef unsigned char byte;

//
// MACRO DEFS
//

/*
PIN ASSIGNMENTS
Control Input/I	RA5		RA0	LED2/USB Power detect/IO
Power Control/O	RA4		RA1 LED1/0
Power Switch/I	RA3		RA2 Control Output/O
*/
#define O_LED1		lata.1
#define O_LED2		lata.0
#define O_OUTPUT	lata.2
#define O_PWR_CTRL	lata.4

#define LED_OUT		O_LED2
#define LED_PWR		O_LED1

#define I_SWITCH	porta.3
#define I_INPUT		porta.5

//                    76543210
#define P_TRISA		0b11101000
#define P_WPUA		0b00101000


// timeouts
#define AUTO_POWER_OFF_MS		(10 * 60 * 1000)		// time from last input to auto power off
#define POWER_ON_DELAY_MS		200						// delay after power on before we can power off
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
	trisa = P_TRISA;              	
	ansela = 0b00000000;
	porta=0;
	
	wpua=P_WPUA;
	option_reg.7=0;

	
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
	
	
	// enable interrupts	
	intcon.7 = 1; //GIE
	intcon.6 = 1; //PEIE

	unsigned long activity_timeout = AUTO_POWER_OFF_MS;
	int debounce_timeout = 0;
	int input_state = 1;
	int output_state = 0;
	int is_battery_power = 0;
	unsigned char count = 0;
	

	// if the switch is held at start up we will assume we're running 
	// on battery power
	if(!I_SWITCH) {	
		is_battery_power  = 1;
	
		// need to hold button for 1s before power is latched on
		// button release during this time will kill power
		delay_s(1);
		O_PWR_CTRL = 1;

		// now we need to wait for the switch to be released so
		// we dont just switch off again. during this time power
		// LED is on 50 duty
		while(!I_SWITCH) {
			if(tick_flag) {	
				tick_flag = 0;
				LED_PWR = !!(count & 0x01); // 50% duty
				++count;
			}
		}

		// long debounce delay after power on prevents
		// accidental immediate power off 
		for(int i=0; i<POWER_ON_DELAY_MS; ++i) {
			while(!tick_flag);
			tick_flag = 0;
			LED_PWR = !!(count & 0x01); // 50% duty
			++count;
		}
	}
	
	// loop forever (until power off)
	for(;;) {	

		// once per ms
		if(tick_flag) {	// once a ms
			tick_flag = 0;
			++count;
			
			// auto power off
			if(is_battery_power && activity_timeout) {
				if(!--activity_timeout) {
					// turn power off
					O_PWR_CTRL = 0;				
				}
			}
			
			// Output LED handling
			if(output_state) {
				LED_OUT = !!(count & 0x01); // 50% duty
			}			
			else {
				LED_OUT = 0;
			}

			// power LED
			LED_PWR = !!(count & 0x01); // 50% duty

			// debouncing timing
			if(debounce_timeout) {
				--debounce_timeout;
			}	
		}
		
		// check input
		if(!debounce_timeout) { // finished debouncing 
			if(input_state) { // switch was pressed when we last looked
				if(I_INPUT) {
					// input has gone off
					input_state = 0;
					debounce_timeout = DEBOUNCE_MS;
					activity_timeout = AUTO_POWER_OFF_MS;
				}
			}
			else {
				if(!I_INPUT) {
					// input has come on
					input_state = 1;
					debounce_timeout = DEBOUNCE_MS;
					activity_timeout = AUTO_POWER_OFF_MS;
					output_state = !output_state;
					O_OUTPUT = output_state;
				}
			}
		}
		if(!I_SWITCH) {
			// if we are running from battery power, switch
			// will turn us off
			O_PWR_CTRL = 0;
		}
	}
}
