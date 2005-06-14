CFLAGS:=-Wall -g -I/usr/local/include -Iinclude
LDFLAGS:=-lopenct -lusb

all: openct-escape

openct-escape: openct-escape.o librfid.a
	$(CC) $(LDFLAGS) -o $@ $^

librfid.a: rfid_layer2.o rfid_layer2_iso14443a.o rfid_layer2_iso14443b.o rfid_layer2_iso15693.o rfid_asic_rc632.o rfid_reader_cm5121.o rfid.o rfid_protocol.o rfid_proto_tcl.o rfid_iso14443_common.o rfid_reader.o
	ar r $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $^

clean:
	rm -f *.o openct-escape
