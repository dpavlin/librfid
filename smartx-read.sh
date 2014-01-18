#!/bin/sh -xe

./utils/librfid-tool -s

./utils/mifare-tool -k a0a1a2a3a4a5 -r 0
./utils/mifare-tool -k a0a1a2a3a4a5 -r 1
./utils/mifare-tool -k a0a1a2a3a4a5 -r 2

./utils/mifare-tool -k a0a1a2a3a4a5 -r 128
./utils/mifare-tool -k a0a1a2a3a4a5 -r 129
./utils/mifare-tool -k a0a1a2a3a4a5 -r 130

