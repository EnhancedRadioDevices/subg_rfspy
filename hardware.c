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

void NOP()
{
	__asm
	nop
	__endasm;
}

void configure_hardware()
{
  IP0 = 0x10; // Set Priority Group 4 to Priority Level 1 (default 0)
  P1IFG &= ~BIT6; // Clear P1_6IF
  IRCON2 &= ~BIT3; // Clear P1IF
  IEN2 |= BIT4; // Enable P1INT interrupt
  P1IEN |= BIT6; // Enable P1_6 interrupt
}

void p1_isr(void) __interrupt P1INT_VECTOR 
{
  NOP();
  led_set_state(0,0);
//    serial_tx_byte(SLEEP); // Checkpoint marker. Remove.
//  PCON &= 0xFE; // Clear Power Mode Control bit. Probably not necessary.
//    serial_tx_byte(SLEEP); // Checkpoint marker. Remove.
//    serial_tx_byte(253); // Checkpoint marker. Remove.
//    serial_tx_byte(P1IFG); // Checkpoint marker. Remove.
  P1IFG &= ~BIT6; // Clear Port1 pin6 interrupt flag. During send,receive,reset actions with sleep enabled, CC1110 does not seem to get past this step?
//    serial_tx_byte(P1IFG); // Checkpoint marker. Remove.
//    serial_tx_byte(252); // Checkpoint marker. Remove.
//    serial_tx_byte(IRCON2); // Checkpoint marker. Remove.
  IRCON2 &= 0xF7; // Clear IRCON2.P1IF (Port1 CPU interrupt flag)
//    serial_tx_byte(IRCON2); // Checkpoint marker. Remove.
//    serial_tx_byte(251); // Checkpoint marker. Remove.
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