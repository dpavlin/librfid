#!/bin/sh -xe

sudo apt-get -y install autoconf automake libtool gcc make libusb-dev
./autogen.sh
./configure --enable-ccid
make
