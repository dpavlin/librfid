# Makefile for Python bindings 
# (C) 2007-2008 by Kushal Das <kushal@openpcd.org>

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 
# as published by the Free Software Foundation

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA



CC=gcc
PYTHON=python
PREFIX=/usr
PYTHON_VER=2.5
PYTHON_INC=$(PREFIX)/include/python$(PYTHON_VER)
PYTHON_LIB=$(PREFIX)/lib/python$(PYTHON_VER)/site-packages/
LIBRFID_DIR=../src/.libs

SOURCE_MAIN=pyrfid.c
SOURCES=$(SOURCE_MAIN)  openpcd.c
INCLUDES=-I$(PYTHON_INC) -I../include/ -I../utils/
CFLAGS=-O3 -Wall $(INCLUDES)
LDFLAGS=-shared -L$(LIBRFID_DIR) -lrfid -lusb  -Wl,--rpath -Wl,/usr/local/lib $(LIBS)
TARGET=$(SOURCE_MAIN:.c=.so)
OBJECTS=$(SOURCES:.c=.o)

all: $(SOURCE_MAIN) $(TARGET)

test:
	$(PYTHON) $@.py

install: $(TARGET)
	install $(TARGET) $(PYTHON_LIB)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

