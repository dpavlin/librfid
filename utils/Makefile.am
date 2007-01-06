INCLUDES = $(all_includes) -I$(top_srcdir)/include

if ENABLE_FIRMWARE
else
bin_PROGRAMS = librfid-tool mifare-tool librfid-send_script 

noinst_HEADERS = librfid-tool.h

librfid_tool_SOURCES = librfid-tool.c common.c
librfid_tool_LDADD = ../src/librfid.la
librfid_tool_LDFLAGS = -dynamic

librfid_send_script_SOURCES= send_script.c
librfid_send_script_LDADD = ../src/librfid.la
librfid_send_script_LDFLAGS = -dynamic

mifare_tool_SOURCES = mifare-tool.c common.c
mifare_tool_LDADD = ../src/librfid.la
mifare_tool_LDFLAGS = -dynamic
endif
