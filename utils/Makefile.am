include $(top_srcdir)/Makefile.flags.am

man_MANS = librfid-tool.1 mifare-tool.1 librfid-send_script.1

EXTRA_DIST = $(man_MANS)

bin_PROGRAMS = librfid-tool mifare-tool librfid-send_script 

noinst_HEADERS = librfid-tool.h common.h

librfid_tool_SOURCES = librfid-tool.c librfid-tool.h common.c common.h
librfid_tool_LDADD = ../src/librfid.la

librfid_send_script_SOURCES= send_script.c
librfid_send_script_LDADD = ../src/librfid.la

mifare_tool_SOURCES = mifare-tool.c common.c
mifare_tool_LDADD = ../src/librfid.la

if ENABLE_WIN32
LINKOPTS = -dynamic -mno-cygwin
librfid_send_script_LDFLAGS = $(LINKOPTS)
librfid_tool_LDFLAGS = $(LINKOPTS)
mifare_tool_LDFLAGS = $(LINKOPTS)
endif
