
#include <stdint.h>
#include "hardware.h"
#include "serial.h"
#include "commands.h"
#include "delay.h"
#include "timer.h"

#define MAX_PACKET_LEN 192
volatile uint8_t __xdata radio_tx_buf[MAX_PACKET_LEN];
volatile uint8_t radio_tx_buf_len = 0;
volatile uint8_t radio_tx_buf_idx = 0;
volatile uint8_t __xdata radio_rx_buf[MAX_PACKET_LEN];
volatile uint8_t radio_rx_buf_len = 0;
volatile uint8_t packet_count = 1;
volatile uint8_t underflow_count = 0;
volatile uint8_t packet_length_signifier = 0;
volatile enum MODE {
	FOOTER,
	FIXED,
	VARIABLE
};
volatile enum MODE packet_mode = FOOTER;

void configure_radio()
{
  /* RF settings SoC: CC1110 */
  SYNC1     = 0xFF; // sync word, high byte
  SYNC0     = 0x00; // sync word, low byte
  PKTLEN    = 0xFF; // packet length
  PKTCTRL1  = 0x00; // packet automation control
  PKTCTRL0  = 0x00; // packet automation control
  ADDR      = 0x00;
  // CHANNR: See the locale-specific section below
  FSCTRL1   = 0x06; // frequency synthesizer control
  FSCTRL0   = 0x00;
  // FREQ0/FREQ1/FREQ2: See the locale-specific section below
  MDMCFG4   = 0x99; // 150.5 kHz rx filter bandwidth. Narrower can improve range,
                    // but then freq must be dialed in more tightly, which does not
                    // allow for variation we see with pump in free space vs on body.
  MDMCFG3   = 0x66; // modem configuration
  MDMCFG2   = 0x33; // modem configuration
  MDMCFG1   = 0x61; // modem configuration
  MDMCFG0   = 0x7E; // modem configuration
  DEVIATN   = 0x15; // modem deviation setting
  MCSM2     = 0x07;
  MCSM1     = 0x30;
  MCSM0     = 0x18; // main radio control state machine configuration
  FOCCFG    = 0x17; // frequency offset compensation configuration BSCFG     = 0x6C;
  FREND1    = 0xB6; // front end tx configuration
  FREND0    = 0x11; // front end tx configuration
  FSCAL3    = 0xE9; // frequency synthesizer calibration
  FSCAL2    = 0x2A; // frequency synthesizer calibration
  FSCAL1    = 0x00; // frequency synthesizer calibration
  FSCAL0    = 0x1F; // frequency synthesizer calibration
  TEST1     = 0x31; // various test settings
  TEST0     = 0x09; // various test settings
  PA_TABLE0 = 0x00; // needs to be explicitly set!
  // PA_TABLE1: See the locale-specific section below

  AGCCTRL2 = 0x07; // 0x03 to 0x07 - default: 0x03
  AGCCTRL1 = 0x00; // 0x00         - default: 0x40
  AGCCTRL0 = 0x91; // 0x91 or 0x92 - default: 0x91

#if US_RADIO_LOCALE
  FREQ2     = 0x26; // 916.541MHz is midpoint between freq of pump in free space,
  FREQ1     = 0x30; // and pump held close to the body.
  FREQ0     = 0x70; //
  CHANNR    = 0x02; // channel number
  PA_TABLE1 = 0xC0; // pa power setting 10 dBm
#else
  FREQ2     = 0x24; // frequency control word, high byte
  FREQ1     = 0x2E; // frequency control word, middle byte
  FREQ0     = 0x38; // frequency control word, low byte
  CHANNR    = 0x00; // channel number
  PA_TABLE1 = 0xC2; // Max configurable power output at this frequency is 0xC2
#endif

  IEN2 |= IEN2_RFIE;
  RFTXRXIE = 1;
}

void rftxrx_isr(void) __interrupt RFTXRX_VECTOR {
  uint8_t d_byte;
  if (MARCSTATE==MARC_STATE_RX) {
    d_byte = RFD;
	switch(packet_mode) {
      case FOOTER:
        if (radio_rx_buf_len == 0) {
          radio_rx_buf[0] = RSSI; 
          if (radio_rx_buf[0] == packet_length_signifier) {
            radio_rx_buf[0] = packet_length_signifier + 1; // Prevent RSSI of pls from triggering end-of-packet
          }
          radio_rx_buf[1] = packet_count; 
          packet_count++;
	      if (packet_count == packet_length_signifier) {
            packet_count = packet_count + 1;
          }
          radio_rx_buf_len = 2;
        }
	
        if (radio_rx_buf_len < MAX_PACKET_LEN) {
          radio_rx_buf[radio_rx_buf_len] = d_byte;
          radio_rx_buf_len++;
        } else {
          // Overflow
        }
        if (d_byte == packet_length_signifier) {
          RFST = RFST_SIDLE;
          while(MARCSTATE!=MARC_STATE_IDLE);
        }
        break;
      case FIXED:
	    if (radio_rx_buf_len == 0) {
          radio_rx_buf[0] = RSSI; 
          radio_rx_buf[1] = packet_count; 
          packet_count++;
          radio_rx_buf_len = 2;
        }
	
        if (radio_rx_buf_len < packet_length_signifier + 2) {
          radio_rx_buf[radio_rx_buf_len] = d_byte;
          radio_rx_buf_len++;
        } else {
          RFST = RFST_SIDLE;
		  while(MARCSTATE!=MARC_STATE_IDLE);
        }
        break;
	  case VARIABLE:
		if (radio_rx_buf_len == 0) {
		  radio_rx_buf[0] = RSSI;
		  radio_rx_buf[1] = packet_count;
		  packet_count++;
		  radio_rx_buf[2] = d_byte;
		  radio_rx_buf_len = 3;
		} else if (radio_rx_buf_len < radio_rx_buf[2] + 2) {
		  radio_rx_buf[radio_rx_buf_len] = d_byte;
		  radio_rx_buf_len++;
		} else {
		  RFST = RFST_SIDLE;
		  while(MARCSTATE!=MARC_STATE_IDLE);
		}
		break;
	  default:
	    RFST = RFST_SIDLE;
        while(MARCSTATE!=MARC_STATE_IDLE);
	    break;
    }
  }
  else if (MARCSTATE==MARC_STATE_TX) {
    switch (packet_mode) { 
      case FOOTER:
        if (radio_tx_buf_len > radio_tx_buf_idx) {
          d_byte = radio_tx_buf[radio_tx_buf_idx++];
          RFD = d_byte;
        } else {
          RFD = packet_length_signifier; // Something must be written to RFD whenever the ISR is called.
          underflow_count++;
          // We wait a few counts to make sure the radio has sent the last bytes
          // before turning it off.
          if (underflow_count == 4) {
            RFST = RFST_SIDLE;
          }
        }
        break;
      case FIXED:
        if (radio_tx_buf_len > radio_tx_buf_idx) {
          d_byte = radio_tx_buf[radio_tx_buf_idx++];
          RFD = d_byte;
        } else {
		  RFD = 0; // Something must be written to RFD whenever the ISR is called.
          underflow_count++;
          // We wait a few counts to make sure the radio has sent the last bytes
          // before turning it off.
          if (underflow_count == 4) {
            RFST = RFST_SIDLE;
          }
        }
        break;
	  case VARIABLE:
		if (radio_tx_buf_len > radio_tx_buf_idx) {
		  d_byte = radio_tx_buf[radio_tx_buf_idx++];
		  RFD = d_byte;
		} else {
		  RFD = 0; // Something must be written to RFD whenever the ISR is called.
		  underflow_count++;
		  // We wait a few counts to make sure the radio has sent the last bytes
          // before turning it off.
		  if (underflow_count == 4) {
			RFST = RFST_SIDLE;
		  }
		}
		break;
	  default:
		RFD = 0; // Something must be written to RFD whenever the ISR is called.
	    RFST = RFST_SIDLE;
	    break;
    }
  }
}

void rf_isr(void) __interrupt RF_VECTOR {
  S1CON &= ~0x03; // Clear CPU interrupt flag
  if(RFIF & 0x80) // TX underflow
  {
    // Underflow
    RFST = RFST_SIDLE;
    RFIF &= ~0x80; // Clear module interrupt flag
  }
  else if(RFIF & 0x40) // RX overflow
  {
    RFIF &= ~0x40; // Clear module interrupt flag
  }
  else if(RFIF & 0x20) // RX timeout
  {
    RFIF &= ~0x20; // Clear module interrupt flag
  }
  // Use ”else if” to check and handle other RFIF flags

}

void send_packet_from_serial(uint8_t channel, uint8_t repeat_count, uint8_t delay_ms) {
  uint8_t s_byte;
  uint8_t variable_len;
  
  radio_tx_buf_len = 0;
  radio_tx_buf_idx = 0;
  underflow_count = 0;

  RFST = RFST_SIDLE;
  while(MARCSTATE!=MARC_STATE_IDLE);

  CHANNR = channel;
  //led_set_state(1,1);
  
  switch (packet_mode) {
    case FOOTER:
      while (SERIAL_DATA_AVAILABLE) {
		s_byte = serial_rx_byte(); 
        if (radio_tx_buf_len == (MAX_PACKET_LEN - 1)) {
          s_byte = packet_length_signifier;
        }
        radio_tx_buf[radio_tx_buf_len++] = s_byte;
        if (s_byte == packet_length_signifier) {
		  // End of packet
          break;
        }

        if (radio_tx_buf_len == 2) { 
          // Turn on radio
          RFST = RFST_STX;
        }
      }
	  break;
    case FIXED:
	  while (radio_tx_buf_len < packet_length_signifier) {
        s_byte = serial_rx_byte();
        radio_tx_buf[radio_tx_buf_len++] = s_byte;
		
		if (radio_tx_buf_len == packet_length_signifier) {
		  // End of packet
		  break;
		}
        if (radio_tx_buf_len == 2) {
          // Turn on radio
          RFST = RFST_STX;
        }
		
      }
      break;
	case VARIABLE:
	  variable_len = serial_rx_byte();
	  while (radio_tx_buf_len < variable_len) {
		s_byte = serial_rx_byte();
        radio_tx_buf[radio_tx_buf_len++] = s_byte;
		
		if (radio_tx_buf_len == variable_len) {
		  // End of packet
		  break;
		}
        if (radio_tx_buf_len == 2) {
          // Turn on radio
          RFST = RFST_STX;
        }
	  }
	default:
	  serial_tx_byte(244);
      break;
  }
  
  // wait for sending to finish
  while(MARCSTATE!=MARC_STATE_IDLE);

  while(repeat_count > 0) {
    // Reset idx to beginning of buffer
    radio_tx_buf_idx = 0;
    underflow_count = 0;

    // delay 
    if (delay_ms > 0) {
      delay(delay_ms);
    }
    
    // Turn on radio (interrupts should start again)
    RFST = RFST_STX;
    while(MARCSTATE!=MARC_STATE_TX);

    // wait for sending to finish
    while(MARCSTATE!=MARC_STATE_IDLE);
    repeat_count--;
  }
  //led_set_state(1,0);
}

void resend_from_tx_buf(uint8_t channel) {

  RFST = RFST_SIDLE;
  while(MARCSTATE!=MARC_STATE_IDLE);

  CHANNR = channel;

  // Reset idx to beginning of buffer
  radio_tx_buf_idx = 0;
  underflow_count = 0;

  // Turn on radio (interrupts should start again)
  RFST = RFST_STX;
  while(MARCSTATE!=MARC_STATE_TX);

  // wait for sending to finish
  while(MARCSTATE!=MARC_STATE_IDLE);
}

uint8_t get_packet_and_write_to_serial(uint8_t channel, uint32_t timeout_ms) {

  uint8_t read_idx = 0;
  uint8_t d_byte = 0;
  uint8_t rval = packet_length_signifier;

  reset_timer();

  RFST = RFST_SIDLE;
  while(MARCSTATE!=MARC_STATE_IDLE);

  CHANNR = channel;

  radio_rx_buf_len = 0;

  RFST = RFST_SRX;
  while(MARCSTATE!=MARC_STATE_RX);
  
  switch(packet_mode) {
	case FOOTER:
	  while(1) {
		// Waiting for isr to put radio bytes into radio_rx_buf
		if (radio_rx_buf_len > read_idx) {
		  //led_set_state(0,1);

		  if (read_idx == 0 && radio_rx_buf_len > 2 && radio_rx_buf[2] == packet_length_signifier) {
			rval = ERROR_ZERO_DATA;
			break;
		  }
		  d_byte = radio_rx_buf[read_idx];
		  serial_tx_byte(d_byte);
		  read_idx++;
		  if (read_idx > 1 && d_byte == packet_length_signifier) {
			// End of packet.
			break;
		  }
		}

		if (timeout_ms > 0 && timerCounter > timeout_ms) {
		  rval = ERROR_RX_TIMEOUT;
		  break;
		}
	  
		// Also going to watch serial in case the client wants to interrupt rx
		if (SERIAL_DATA_AVAILABLE) {
		  // Received a byte from uart while waiting for radio packet
		  // We will interrupt the RX and go handle the command.
		  interrupting_cmd = serial_rx_byte();
		  rval = ERROR_CMD_INTERRUPTED;
		  break;
		}
	  }
	  break;
	case FIXED:
	  while(read_idx < packet_length_signifier + 2) {
		// Waiting for isr to put radio bytes into radio_rx_buf
		if (radio_rx_buf_len > read_idx) {
		  //led_set_state(0,1);
		  d_byte = radio_rx_buf[read_idx];
		  serial_tx_byte(d_byte);
		  read_idx++;
		}

		if (timeout_ms > 0 && timerCounter > timeout_ms) {
		  rval = ERROR_RX_TIMEOUT;
		  break;
		}
	  
		// Also going to watch serial in case the client wants to interrupt rx
		if (SERIAL_DATA_AVAILABLE) {
		  // Received a byte from uart while waiting for radio packet
		  // We will interrupt the RX and go handle the command.
		  interrupting_cmd = serial_rx_byte();
		  rval = ERROR_CMD_INTERRUPTED;
		  break;
		}
	  }
	  break;
	case VARIABLE:
	  while(read_idx < radio_rx_buf[2] + 2) {
		// Waiting for isr to put radio bytes into radio_rx_buf
		if (radio_rx_buf_len > read_idx) {
		  //led_set_state(0,1);
		  d_byte = radio_rx_buf[read_idx];
		  serial_tx_byte(d_byte);
		  read_idx++;
		}
		
		if (timeout_ms > 0 && timerCounter > timeout_ms) {
		  rval = ERROR_RX_TIMEOUT;
		  break;
		}
	  
		// Also going to watch serial in case the client wants to interrupt rx
		if (SERIAL_DATA_AVAILABLE) {
		  // Received a byte from uart while waiting for radio packet
		  // We will interrupt the RX and go handle the command.
		  interrupting_cmd = serial_rx_byte();
		  rval = ERROR_CMD_INTERRUPTED;
		  break;
		}
	  }
	  break;
	default:
	  serial_tx_byte(244);
	  break;
  }
  RFST = RFST_SIDLE;
  //led_set_state(0,0);
  return rval;
}

