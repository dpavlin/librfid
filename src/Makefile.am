LIBVERSION= 0:0:0

if ENABLE_FIRMWARE
INCLUDES_FIRMWARE = -I$(FIRMWARE_PATH)/include -I$(FIRMWARE_PATH)/src -I$(GNUARM_PATH)/arm-elf/include
AM_CFLAGS_FIRMWARE = -DLIBRFID_FIRMWARE -DLIBRFID_STATIC -D__MS_types__
endif

if ENABLE_STATIC
AM_CFLAGS_STATIC = -DLIBRFID_STATIC
endif

noinst_HEADERS=rfid_iso14443_common.h rc632.h libusb_dyn.h usleep.h

CORE=rfid.c rfid_layer2.c rfid_protocol.c rfid_reader.c rfid_scan.c
L2=rfid_layer2_iso14443a.c rfid_layer2_iso14443b.c rfid_layer2_iso15693.c rfid_iso14443_common.c
PROTO=rfid_proto_tcl.c rfid_proto_mifare_ul.c rfid_proto_mifare_classic.c
ASIC=rfid_asic_rc632.c
READER=rfid_reader_openpcd.c
MISC=rfid_access_mifare_classic.c

if ENABLE_WIN32
WIN32=usleep.c libusb_dyn.c
AM_LDLAGS_WIN32=-mno-cygwin -lwinmm
AM_CFLAGS_WIN32=-mno-cygwin
endif

if ENABLE_CCID
READER_CM5121=rfid_reader_cm5121.c ccid/rfid_reader_cm5121_ccid_direct.c ccid/ccid-driver.c
AM_CFLAGS_CM5121= -DENABLE_CM5121
endif

if HAVE_OPENCT
READER_CM5121=rfid_reader_cm5121.c rfid_reader_cm5121_openct.c
AM_CFLAGS_CM5121= -DENABLE_CM5121
endif

INCLUDES = $(all_includes) -I$(top_srcdir)/include $(INCLUDES_FIRMWARE) \
	   $(INCLUDES_STATIC) @OPENCT_CFLAGS@
AM_CFLAGS = -std=gnu99 -D__LIBRFID__ $(AM_CFLAGS_FIRMWARE) $(AM_CFLAGS_STATIC) \
	    $(AM_CFLAGS_CM5121) $(AM_CFLAGS_WIN32)

lib_LTLIBRARIES = librfid.la
librfid_la_LDFLAGS = -Wc,-nostartfiles -version-info $(LIBVERSION) $(AM_LDFLAGS_WIN32) @OPENCT_LIBS@
librfid_la_SOURCES = $(CORE) $(L2) $(PROTO) $(ASIC) $(MISC) \
		     $(READER) $(READER_CM5121) $(WIN32)

if ENABLE_WIN32
librfid_la_LIBADD = -lwinmm
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = librfid.pc
