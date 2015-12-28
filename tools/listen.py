#!/usr/bin/env python

import binascii
from datetime import datetime
import sys
from serial_rl import SerialRL

def print_packet(p):
  rssi_dec = p[0]
  rssi_offset = 73;
  if rssi_dec >= 128:
    rssi = (( rssi_dec - 256) / 2) - rssi_offset
  else:
    rssi = (rssi_dec / 2) - rssi_offset;

  packet_number = p[1];

  p = p[2:]
  t = datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]
  print "%s - (%d) (%d) - %s" % (t,  rssi, packet_number, binascii.hexlify(p))
  sys.stdout.flush()


if len(sys.argv) < 2:
  print "usage: listen.py serialport [channel]"
  sys.exit(2)

channel = 2

if len(sys.argv) == 3:
  channel = int(sys.argv[2])

rl = SerialRL(sys.argv[1])
rl.sync()
rl.do_command(SerialRL.CMD_SET_CHANNEL, chr(channel))
while 1:
  timeout = 0
  rl.send_command(SerialRL.CMD_GET_PACKET, chr(timeout))
  packet = rl.get_response()
  if len(packet) > 2:
    print_packet(packet)
  else:
    print "Timeout"
    sys.exit(0)