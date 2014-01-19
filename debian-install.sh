#!/bin/sh -xe

sudo apt-get -y install autoconf automake libtool gcc make libusb-dev
./autogen.sh
./configure --enable-ccid
make
sudo make install
sudo cp etc/udev/librfid.rules /etc/udev/rules.d/
sudo /etc/init.d/udev restart
