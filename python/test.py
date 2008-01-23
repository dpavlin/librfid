#!/usr/bin/env python
  #Python bindings test file 
  #(C) 2007 by Kushal Das <kushal@openpcd.org>

  #This program is free software; you can redistribute it and/or modify
  #it under the terms of the GNU General Public License version 2 
  #as published by the Free Software Foundation

  #This program is distributed in the hope that it will be useful,
  #but WITHOUT ANY WARRANTY; without even the implied warranty of
  #MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  #GNU General Public License for more details.

  #You should have received a copy of the GNU General Public License
  #along with this program; if not, write to the Free Software
  #Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA

import pyrfid

res = pyrfid.open()
if res == 1:
        print "No device found"
else:
        print "We found a device :)"
        while 1:
                res = pyrfid.scan()
                if res == 3:
                        print "The id of the card is %s" %(pyrfid.get_id())
        pyrfid.close()
