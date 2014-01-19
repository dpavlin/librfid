#!/bin/sh -xe

#dev=`lsusb | grep 076b:5.21 | awk '{ print $2 "-" $4 }' | sed -e 's/0//g' -e 's/://'`
dev=`dmesg | grep OMNIKEY | sed 's/\[.*\] //'  | cut -d" " -f2 | cut -d: -f1 | tail -1`
authorized=/sys/bus/usb/devices/$dev/authorized
test -w $authorized || exit 1
echo 0 > $authorized
dmesg | tail -2
sleep 1
echo 1 > $authorized
dmesg | tail -2
