#!/bin/bash
BD=`dirname ${BASH_SOURCE[0]}`
cd $BD
BASEDIR=`pwd`
export BASEDIR

APP=receive_serial_data_from_arduino

if [ ! -f "/etc/cron.d/watchdog-$APP" ] ; then
	echo "install $APP watchdog"
	echo "*/5 * * * * root $BASEDIR/start.sh" > /etc/cron.d/watchdog-$APP
fi

