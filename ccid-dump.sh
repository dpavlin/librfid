#!/bin/sh -xe
test -d dump || mkdir dump
test -f dump/ccid || gcc -DTEST -Wall -Iinclude -lusb -g src/ccid/ccid-driver.c -o dump/ccid
./dump/ccid 2>&1 | tee dump/`hostname`.ccid
