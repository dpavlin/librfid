#!/bin/sh -xe
test -d dump || mkdir dump
#test -f dump/ccid || 
gcc -DTEST -Wall -Iinclude -lusb -g src/ccid/ccid-driver.c -o utils/ccid
sudo ./utils/ccid --debug --no-poll 2>&1 | tee dump/`hostname`.ccid
