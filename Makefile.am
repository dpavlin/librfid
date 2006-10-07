AUTOMAKE_OPTIONS = foreign dist-bzip2 1.6

SUBDIRS = include src utils
LINKOPTS = -lusb

AM_CFLAGS = -std=gnu99

$(OBJECTS): libtool
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck
