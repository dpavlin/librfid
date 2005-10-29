
CFLAGS:=-Wall -g -I/usr/local/include -Iinclude
LDFLAGS:= -lusb

LIBRFID_OBJS=rfid_layer2.o rfid_layer2_iso14443a.o rfid_layer2_iso14443b.o rfid_layer2_iso15693.o rfid_asic_rc632.o rfid_reader_cm5121.o rfid.o rfid_protocol.o rfid_proto_tcl.o rfid_proto_mifare_ul.o rfid_proto_mifare_classic.o rfid_iso14443_common.o rfid_reader.o

# uncomment this if you want to use OpenCT
LDFLAGS+=-lopenct
LIBRFID_OBJS+=rfid_reader_cm5121_openct.o

# uncomment this if you want to use our internal CCID driver
#LIBRFID_OBJS+=rfid_reader_cm5121_ccid_direct.o ccid/ccid-driver.o

all: openct-escape

openct-escape: openct-escape.o librfid.a
	$(CC) $(LDFLAGS) -o $@ $^

librfid.a: $(LIBRFID_OBJS)
	ar r $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -DHAVE_LIBUSB -DUSE_INTERNAL_CCID_DRIVER  -o $@ -c $^

clean:
	rm -f *.o openct-escape librfid.a
