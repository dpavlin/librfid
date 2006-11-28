INCLUDES = $(all_includes) -I$(top_srcdir)/include

bin_PROGRAMS = librfid-tool mifare-tool send_script 

librfid_tool_SOURCES = librfid-tool.c common.c
librfid_tool_LDADD = ../src/librfid.la
librfid_tool_LDFLAGS = -dynamic

send_script_SOURCES= send_script.c
send_script_LDADD = ../src/librfid.la
send_script_LDFLAGS = -dynamic

mifare_tool_SOURCES = mifare-tool.c common.c
mifare_tool_LDADD = ../src/librfid.la
mifare_tool_LDFLAGS = -dynamic


