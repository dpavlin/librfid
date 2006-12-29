LIBVERSION= 0:0:0

if ENABLE_FIRMWARE
INCLUDES_FIRMWARE = -I$(FIRMWARE_PATH)/include -I$(GNUARM_PATH)/arm-elf/include
AM_CFLAGS_FIRMWARE = -DLIBRFID_FIRMWARE -DLIBRFID_STATIC -D__MS_types__
endif

if ENABLE_STATIC
AM_CFLAGS_STATIC = -DLIBRFID_STATIC
endif

noinst_HEADERS=rfid_iso14443_common.h rc632.h

CORE=rfid.c rfid_layer2.c rfid_protocol.c rfid_reader.c rfid_scan.c
L2=rfid_layer2_iso14443a.c rfid_layer2_iso14443b.c rfid_layer2_iso15693.c rfid_iso14443_common.c
PROTO=rfid_proto_tcl.c rfid_proto_mifare_ul.c rfid_proto_mifare_classic.c
ASIC=rfid_asic_rc632.c
READER=rfid_reader_openpcd.c
MISC=rfid_access_mifare_classic.c

if ENABLE_CM5121
READER_CM5121=rfid_reader_cm5121.c
AM_CFLAGS_CM5121= -DENABLE_CM5121

if ENABLE_CCID
READER_LOW_CCID=ccid/rfid_reader_cm5121_ccid_direct.c ccid/ccid-driver.c
endif

if ENABLE_OPENCT
READER_LOW_OPENCT=rfid_reader_cm5121_openct.c
LDFLAGS_OPENCT=-lopenct
endif

endif

INCLUDES = $(all_includes) -I$(top_srcdir)/include $(INCLUDES_FIRMWARE) \
	   $(INCLUDES_STATIC)
AM_CFLAGS = -std=gnu99 -D__LIBRFID__ $(AM_CFLAGS_FIRMWARE) $(AM_CFLAGS_STATIC) \
	    $(AM_CFLAGS_CM5121)

lib_LTLIBRARIES = librfid.la
librfid_la_LDFLAGS = -Wc,-nostartfiles -version-info $(LIBVERSION) $(LDFLAGS_OPENCT)
librfid_la_SOURCES = $(CORE) $(L2) $(PROTO) $(ASIC) $(MISC) \
		     $(READER) \
		     $(READER_CM5121) $(READER_LOW_CCID) $(READER_LOW_OPENCT)

