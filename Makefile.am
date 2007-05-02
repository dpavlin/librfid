AUTOMAKE_OPTIONS = foreign dist-bzip2 1.6

SUBDIRS = etc include src utils

DEFAULT_AM_CFLAGS = -std=gnu99
if ENABLE_WIN32
SUBDIRS += win32
LINKOPTS = -mno-cygwin
AM_CFLAGS = $(DEFAULT_AM_CFLAGS)
else
LINKOPTS = -lusb
AM_CFLAGS = $(DEFAULT_AM_CFLAGS) -mno-cygwin
endif

EXTRA_DIST = LICENSING librfid.spec openct-cm5121-librfid.patch

$(OBJECTS): libtool
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck
