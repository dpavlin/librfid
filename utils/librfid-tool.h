#ifndef _RFIDTOOL_H
#define _RFIDTOOL_H

#define _GNU_SOURCE
#include <getopt.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))


extern const char *
hexdump(const void *data, unsigned int len);

extern int
hexread(unsigned char *result, const unsigned char *in, unsigned int len);

extern struct rfid_reader_handle *rh;
extern struct rfid_layer2_handle *l2h;
extern struct rfid_protocol_handle *ph;

extern int reader_init(void);
extern int l2_init(int layer2);
extern int l3_init(int protocol);

#define LIBRFID_TOOL_VERSION	"0.1"

struct rfidtool_module {
	struct rfidtool_module *next;
	char *name;
	char *version;
	const struct option *extra_opts;

	unsigned int option_offset;
};

#endif /* _REFIDTOOL_H */
