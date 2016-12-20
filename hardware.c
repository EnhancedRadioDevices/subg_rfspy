/* Control a cc1110 for sub-ghz RF comms over uart. */

#include <stdint.h>
#include "hardware.h"
#include "serial.h"
#include "radio.h"
#include "timer.h"
#include "commands.h"
#include "delay.h"

uint8_t green_mode = 2;
uint8_t blue_mode = 2;

void configure_hardware()
{  
  IRCON2 &= ~BIT3; // Clear P1IF
  IEN2 |= BIT4;    // Enable P1INT interrupt
  P1IEN |= BIT4;    // Enable P1_4 interrupt
}

void p1_isr(void) __interrupt P1INT_VECTOR {
	P1IFG = 0xEF; // Clear Port1 pin4 interrupt flag
	IRCON2 &= 0xF7; // Clear IRCON2.P1IF (Port1 CPU interrupt flag)
	SLEEP &= 0xFC; 
}

void led_set_mode(uint8_t led, uint8_t new_mode)
{
	if(led == 0){
		green_mode = new_mode;
		if(new_mode != 2){
			GREEN_LED = new_mode;
		}
	}
	else if(led == 1){
		blue_mode = new_mode;
		if(new_mode != 2){
			BLUE_LED = new_mode;
		}
	}
}

void led_set_state(uint8_t led, uint8_t command)
{
	if(led == 0){
		if(green_mode == 2){
			if(command < 2){
				GREEN_LED = command;
			}
		}
	}
	else if(led == 1){
		if(blue_mode == 2){
			if(command < 2){
				BLUE_LED = command;
			}
		}
	} 
}