LIBVERSION= 0:0:0
INCLUDES = $(all_includes) -I$(top_srcdir)/include
AM_CFLAGS = -std=gnu99 -D__LIBRFID__

if ENABLE_FIRMWARE
INCLUDES += -I$(FIRMWARE_PATH)/include -I$(GNUARM_PATH)/arm-elf/include
AM_CFLAGS += -DLIBRFID_FIRMWARE -DLIBRFID_STATIC -D__MS_types__
endif

if ENABLE_STATIC
AM_CFLAGS += -DLIBRFID_STATIC
endif

lib_LTLIBRARIES = librfid.la

CORE=rfid.c rfid_layer2.c rfid_protocol.c rfid_reader.c rfid_scan.c
L2=rfid_layer2_iso14443a.c rfid_layer2_iso14443b.c rfid_layer2_iso15693.c rfid_iso14443_common.c
PROTO=rfid_proto_tcl.c rfid_proto_mifare_ul.c rfid_proto_mifare_classic.c
ASIC=rfid_asic_rc632.c
READER=rfid_reader_openpcd.c

if ENABLE_CM5121
READER += rfid_reader_cm5121.c
AM_CFLAGS += -DENABLE_CM5121

if ENABLE_CCID
READER_LOW=ccid/rfid_reader_cm5121_ccid_direct.c ccid/ccid-driver.c
endif

if ENABLE_OPENCT
READER_LOW=rfid_reader_cm5121_openct.c
endif

endif

librfid_la_LDFLAGS = -Wc,-nostartfiles -version-info $(LIBVERSION) -lopenct
librfid_la_SOURCES = $(CORE) $(L2) $(PROTO) $(READER) $(READER_LOW) $(ASIC)

