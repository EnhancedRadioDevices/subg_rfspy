/* Control a cc1110 for sub-ghz RF comms over uart. */

#include <stdint.h>
#include "hardware.h"
#include "serial.h"
#include "radio.h"
#include "timer.h"
#include "commands.h"
#include "delay.h"

// SDCC needs prototypes of all ISR functions in main. not sure why, but described in section 3.8.1
void t1_isr(void) __interrupt T1_VECTOR;
void rftxrx_isr(void) __interrupt RFTXRX_VECTOR;
void rf_isr(void) __interrupt RF_VECTOR;
void p1_isr(void) __interrupt P1INT_VECTOR; //Required for external wakeups on port one while in sleep mode

#ifdef USES_USART1_RX_ISR
void rx1_isr(void) __interrupt URX1_VECTOR;
#endif 

#ifdef USES_USART1_TX_ISR
void tx1_isr(void) __interrupt UTX1_VECTOR;
#endif

#if TI_DONGLE || SRF_STICK
void usb_isr() __interrupt 6;
#endif

int main(void)
{

  // Set the system clock source to HS XOSC and max CPU speed,
  // ref. [clk]=>[clk_xosc.c]
  SLEEP &= ~SLEEP_OSC_PD;
  while( !(SLEEP & SLEEP_XOSC_S) );
  CLKCON = (CLKCON & ~(CLKCON_CLKSPD | CLKCON_OSC)) | CLKSPD_DIV_1;
  while (CLKCON & CLKCON_OSC);
  SLEEP |= SLEEP_OSC_PD;

  // init LEDS
  HARDWARE_LED_INIT;       // see hardware.h
//  led_set_state(0, 1); //GREEN_LED = 1;
  led_set_state(0, 0); //GREEN_LED = 0;
  led_set_state(1, 0); //BLUE_LED = 0;

  // Global interrupt enable
  init_timer();
  EA = 1;

  configure_radio();
  configure_serial();
  configure_hardware();

  while(1) {
    //led_set_state(0,2);
//    go to sleep
//    serial_tx_byte(242); // Checkpoint marker. Remove.
//    serial_tx_byte(SLEEP); // Checkpoint marker. Remove.
//    IEN0 &= ~BIT0; //rftxrxie
//    IEN1 &= ~BIT1; //t1ie
//    IEN2 &= ~BIT3; //utx1ie
//    IEN2 &= ~BIT0; //rfie
//    URX1IE = 0; //urx1ie
//    IEN2 |= BIT4; // Enable P1INT interrupt
//    serial_tx_byte(IEN0);
//    serial_tx_byte(IEN1);
//    serial_tx_byte(IEN2);
//    serial_tx_byte(IP0);
//    serial_tx_byte(IP1);
//    serial_tx_byte(TCON);
//    serial_tx_byte(S0CON);
//    serial_tx_byte(S1CON);
//    serial_tx_byte(IRCON);
//    serial_tx_byte(IRCON2);
    SLEEP = (SLEEP & 0xFC) | 0x01; // Power Mode 1
//    serial_tx_byte(SLEEP); // Checkpoint marker. Remove.
    NOP();
    NOP();
    NOP(); // Three NOPs while sleep registers are set
//    serial_tx_byte(SLEEP); // Checkpoint marker. Remove.
    if (SLEEP & 0x03) {
//     serial_tx_byte(SLEEP); // Checkpoint marker. Remove.
//     serial_tx_byte(243); // Checkpoint marker. Remove.
      led_set_state(0,1);
      PCON |= 0x01;
      NOP(); // First command after waking up should always be NOP
//      led_set_state(0,0);
//      IEN2 &= ~BIT4; // Disable P1INT interrupt
//      IEN0 |= BIT0; //rftxrxie
//      IEN1 |= BIT1; //t1ie
//      IEN2 |= BIT3; //utx1ie
//      IEN2 |= BIT0; //rfie
//      URX1IE = 1; //urx1ie
    }
//    serial_tx_byte(244); // Checkpoint marker. Remove.
    get_command();
  }
}
