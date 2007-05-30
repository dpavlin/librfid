#!/usr/bin/env python

import pyrfid

res = openpcd.open()
if res == 1:
        print "No device found"
else:
        print "We found a device :)"
        while 1:
                res = openpcd.scan()
                if res == 3:
                        print "The id of the card is %s" %(openpcd.get_id())
openpcd.close()
