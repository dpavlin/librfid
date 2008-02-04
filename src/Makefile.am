include $(top_srcdir)/Makefile.flags.am

AM_CFLAGS += -D__LIBRFID__ 
INCLUDES += @OPENCT_CFLAGS@

if ENABLE_FIRMWARE
INCLUDES += -I$(FIRMWARE_PATH)/include -I$(FIRMWARE_PATH)/src -I$(GNUARM_PATH)/arm-elf/include
AM_CFLAGS += -DLIBRFID_FIRMWARE -DLIBRFID_STATIC -D__MS_types__
endif

if ENABLE_STATIC
AM_CFLAGS += -DLIBRFID_STATIC
endif

noinst_HEADERS = rfid_iso14443_common.h rc632.h libusb_dyn.h usleep.h cm5121_source.h \
		 rfid_reader_rc632_common.h

CORE = rfid.c rfid_layer2.c rfid_protocol.c rfid_reader.c rfid_scan.c
L2 = rfid_layer2_iso14443a.c rfid_layer2_iso14443b.c rfid_iso14443_common.c \
     rfid_layer2_iso15693.c
PROTO = rfid_proto_tcl.c rfid_proto_mifare_ul.c rfid_proto_mifare_classic.c \
	rfid_proto_icode.c rfid_proto_tagit.c
ASIC = rfid_asic_rc632.c rfid_reader_rc632_common.c
MISC = rfid_access_mifare_classic.c

if ENABLE_WIN32
WIN32=usleep.c libusb_dyn.c
AM_LDFLAGS_WIN32=-mno-cygwin -lwinmm
AM_CFLAGS_WIN32=-mno-cygwin
librfid_la_LIBADD = -lwinmm
endif

if HAVE_LIBUSB
READER_OPENPCD=rfid_reader_openpcd.c
AM_CFLAGS += -DENABLE_OPENPCD
endif

if ENABLE_SPIDEV
READER_SPIDEV=rfid_reader_spidev.c
AM_CFLAGS += -DENABLE_SPIDEV
endif

if ENABLE_CCID
READER_CM5121=rfid_reader_cm5121.c ccid/rfid_reader_cm5121_ccid_direct.c ccid/ccid-driver.c ccid/ccid-driver.h
AM_CFLAGS += -DENABLE_CM5121
endif

if HAVE_OPENCT
READER_CM5121=rfid_reader_cm5121.c rfid_reader_cm5121_openct.c
AM_CFLAGS += -DENABLE_CM5121
endif


lib_LTLIBRARIES = librfid.la
librfid_la_LDFLAGS = -Wc,-nostartfiles -version-info $(LIBVERSION) $(AM_LDFLAGS_WIN32) @OPENCT_LIBS@
librfid_la_SOURCES = $(CORE) $(L2) $(PROTO) $(ASIC) $(MISC) $(WIN32) \
		     $(READER_OPENPCD) $(READER_CM5121) $(READER_SPIDEV)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = librfid.pc
