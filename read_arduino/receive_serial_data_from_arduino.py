#!/usr/bin/env python3
import serial
import datetime
import time
import logging

if __name__ == '__main__':
    ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)
    ser.flush()

    logging.basicConfig(filename='/var/log/arduino.log',
                    filemode='a',
                    format='%(asctime)s %(message)s',
                    datefmt='%Y-%m-%d %H:%M:%S',
                    level=logging.DEBUG)

    logging.info("Start read serial")

    while True:
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8', errors='ignore').rstrip()
            logging.info(line)
        time.sleep( 0.1 )
