AUTOMAKE_OPTIONS = foreign dist-bzip2 1.6

SUBDIRS = etc include src utils
LINKOPTS = -lusb

AM_CFLAGS = -std=gnu99

EXTRA_DIST = LICENSING librfid.spec openct-cm5121-librfid.patch

$(OBJECTS): libtool
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck
