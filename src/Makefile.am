LIBVERSION= 0:0:0
INCLUDES = $(all_includes) -I$(top_srcdir)/include

lib_LTLIBRARIES = librfid.la

CORE=rfid.c rfid_layer2.c rfid_protocol.c rfid_reader.c
L2=rfid_layer2_iso14443a.c rfid_layer2_14443b.c rfid_layer2_iso15693.c rfid_iso14443_common.o
PROTO=rfid_proto_tcl.c rfid_proto_mifare_ul.c rfid_proto_mifare_classic.c
READER=rfid_reader_cm5121.c rfid_asic_rc632.c 

librfid_la_LDFLAGS = -Wc,-nostartfiles -version-info $(LIBVERSION)
librfid_la_SOURCES = $(CORE) $(L2) $(PROTO) $(READER)

