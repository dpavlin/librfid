CC=gcc
PYTHON=python
PREFIX=/usr
PYTHON_VER=2.5
PYTHON_INC=$(PREFIX)/include/python$(PYTHON_VER)
PYTHON_LIB=$(PREFIX)/lib/python$(PYTHON_VER)/site-packages/
LIBRFID_DIR=../src/.libs/librfid.la
LIBUSB_DIR=/usr/local/lib

SOURCE_MAIN=pyrfid.c
SOURCES=$(SOURCE_MAIN) ../utils/common.c
INCLUDES=-I$(PYTHON_INC) -I../include/ -I../utils/
CFLAGS=-O3 -Wall $(INCLUDES)
LDFLAGS=-shared -L$(LIBRFID_DIR) -lrfid -L$(LIBUSB_DIR) -lusb  -Wl,--rpath -Wl,/usr/local/lib $(LIBS)
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

