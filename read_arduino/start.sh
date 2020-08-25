#!/bin/bash
BD=`dirname ${BASH_SOURCE[0]}`
cd $BD
BASEDIR=`pwd`
export BASEDIR

APP=receive_serial_data_from_arduino.py

PID=`ps -ef|grep $APP|grep -v grep|awk -F\  '{print $2}'`

if [ -z "${PID:-}" ]; then
   echo "$APP is not running. Start it!"
   nohup /home/pi/read_arduino/receive_serial_data_from_arduino.py &
else
   echo "$APP is running. PID=$PID"
fi



