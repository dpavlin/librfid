#!/bin/sh -xe
test -d dump || mkdir dump
#test -f dump/ccid || 
gcc -DTEST -Wall -Iinclude -lusb -g src/ccid/ccid-driver.c -o utils/ccid
./utils/ccid --debug --no-poll 2>&1 | tee dump/ccid
id=`head dump/debian7.ccid | cut -d: -f4 | head -1`
mv dump/ccid dump/$id.`hostname`.ccid
