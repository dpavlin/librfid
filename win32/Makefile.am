include $(top_srcdir)/Makefile.flags.am

noinst_HEADERS = openpcd.h
noinst_DATA = openpcd.dll

LIBRFID_DIR = $(top_builddir)/src/.libs/
CLEANFILES = $(noinst_DATA)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

.o.dll:
	$(CC) -mno-cygwin --add-stdcall-alias -shared -o $@ -L$(LIBRFID_DIR) $< -lrfid -lwinmm -Wl,-no-undefined -Wl,--entry,__cygwin_noncygwin_dll_entry@12
	$(STRIP) --strip-unneeded $@
