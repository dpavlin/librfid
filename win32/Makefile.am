LIBVERSION= 0:0:0

noinst_HEADERS = openpcd.h
noinst_DATA = openpcd.dll

INCLUDES = -I$(top_srcdir)/include
LIBRFID_DIR = $(top_builddir)/src/.libs/
CLEANFILES = $(noinst_DATA)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

.o.dll:
	$(CC) -mno-cygwin --add-stdcall-alias -shared -o $@ -L$(LIBRFID_DIR) $< -lrfid -lwinmm -Wl,-no-undefined -Wl,--entry,__cygwin_noncygwin_dll_entry@12
	$(STRIP) --strip-unneeded $@
