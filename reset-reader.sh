#!/bin/sh -xe

dev=`lsusb | grep 076b:5.21 | awk '{ print $2 "-" $4 }' | sed -e 's/0//g' -e 's/://'`
authorized=/sys/bus/usb/devices/$dev/authorized
echo 0 > $authorized
dmesg
echo 1 > $authorized
dmesg
