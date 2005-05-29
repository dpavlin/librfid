CFLAGS:=-Wall -g -I/usr/local/include -Iinclude
LDFLAGS:=-lopenct -lusb

all: openct-escape

openct-escape: openct-escape.o rfid_layer2.o rfid_layer2_iso14443a.o rfid_layer2_iso14443b.o rfid_asic_rc632.o rfid_reader_cm5121.o rfid.o rfid_protocol.o rfid_proto_tcl.o rfid_iso14443_common.o rfid_reader.o
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $^

clean:
	rm -f *.o openct-escape
