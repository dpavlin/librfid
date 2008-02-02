/* librfid-tool - a small command-line tool for librfid testing
 *
 * (C) 2005-2008 by Harald Welte <laforge@gnumonks.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 
 *  as published by the Free Software Foundation
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifndef __MINGW32__
#include <libgen.h>
#endif

#define _GNU_SOURCE
#include <getopt.h>

#include <librfid/rfid.h>
#include <librfid/rfid_scan.h>
#include <librfid/rfid_reader.h>
#include <librfid/rfid_layer2.h>
#include <librfid/rfid_protocol.h>

#include <librfid/rfid_layer2_iso14443a.h>
#include <librfid/rfid_layer2_iso15693.h>

#include <librfid/rfid_protocol_mifare_classic.h>
#include <librfid/rfid_protocol_mifare_ul.h>
#include <librfid/rfid_protocol_tagit.h>
#include <librfid/rfid_protocol_icode.h>
#include <librfid/rfid_protocol_tcl.h>

#include "librfid-tool.h"


static int select_mf(void)
{
	unsigned char cmd[] = { 0x00, 0xa4, 0x00, 0x00, 0x02, 0x3f, 0x00, 0x00 };
	unsigned char ret[256];
	unsigned int rlen = sizeof(ret);

	int rv;

	rv = rfid_protocol_transceive(ph, cmd, sizeof(cmd), ret, &rlen, 0, 0);
	if (rv < 0)
		return rv;

	printf("%d: [%s]\n", rlen, hexdump(ret, rlen));

	return 0;
}


static int iso7816_get_challenge(unsigned char len)
{
	unsigned char cmd[] = { 0x00, 0x84, 0x00, 0x00, 0x08 };
	unsigned char ret[256];
	unsigned int rlen = sizeof(ret);

	cmd[4] = len;

	int rv;

	rv = rfid_protocol_transceive(ph, cmd, sizeof(cmd), ret, &rlen, 0, 0);
	if (rv < 0)
		return rv;

	printf("%d: [%s]\n", rlen, hexdump(ret, rlen));

	return 0;
}

int
iso7816_select_application(void)
{
	unsigned char cmd[] = { 0x00, 0xa4, 0x04, 0x0c, 0x07,
		       0xa0, 0x00, 0x00, 0x02, 0x47, 0x10, 0x01 };
	unsigned char resp[7];
	unsigned int rlen = sizeof(resp);

	int rv;

	rv = rfid_protocol_transceive(ph, cmd, sizeof(cmd), resp, &rlen, 0, 0);
	if (rv < 0)
		return rv;

	/* FIXME: parse response */
	printf("%s\n", hexdump(resp, rlen));

	return 0;
}

int
iso7816_select_ef(u_int16_t fid)
{
	unsigned char cmd[7] = { 0x00, 0xa4, 0x02, 0x0c, 0x02, 0x00, 0x00 };
	unsigned char resp[7];
	unsigned int rlen = sizeof(resp);

	int rv;

	cmd[5] = (fid >> 8) & 0xff;
	cmd[6] = fid & 0xff;

	rv = rfid_protocol_transceive(ph, cmd, sizeof(cmd), resp, &rlen, 0, 0);
	if (rv < 0)
		return rv;

	/* FIXME: parse response */
	printf("%s\n", hexdump(resp, rlen));

	return 0;
}

int
iso7816_read_binary(unsigned char *buf, unsigned int *len)
{
	unsigned char cmd[] = { 0x00, 0xb0, 0x00, 0x00, 0x00 };
	unsigned char resp[256];
	unsigned int rlen = sizeof(resp);
	
	int rv;

	rv = rfid_protocol_transceive(ph, cmd, sizeof(cmd), resp, &rlen, 0, 0);
	if (rv < 0)
		return rv;

	printf("%s\n", hexdump(resp, rlen));

	/* FIXME: parse response, determine whether we need additional reads */

	/* FIXME: copy 'len' number of response bytes to 'buf' */
	return 0;
}

/* wrapper function around SELECT EF and READ BINARY */
int
iso7816_read_ef(u_int16_t fid, unsigned char *buf, unsigned int *len)
{
	int rv;

	rv = iso7816_select_ef(fid);
	if (rv < 0)
		return rv;

	return iso7816_read_binary(buf, len);
}

/* mifare ultralight helpers */
int
mifare_ulight_write(struct rfid_protocol_handle *ph)
{
	unsigned char buf[4] = { 0xa1, 0xa2, 0xa3, 0xa4 };

	return rfid_protocol_write(ph, 10, buf, 4);
}

int
mifare_ulight_blank(struct rfid_protocol_handle *ph)
{
	unsigned char buf[4] = { 0x00, 0x00, 0x00, 0x00 };
	int i, ret;

	for (i = 4; i <= MIFARE_UL_PAGE_MAX; i++) {
		ret = rfid_protocol_write(ph, i, buf, 4);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int
mifare_ulight_read(struct rfid_protocol_handle *ph)
{
	unsigned char buf[20];
	unsigned int len = sizeof(buf);
	int ret;
	int i;

	for (i = 0; i <= MIFARE_UL_PAGE_MAX; i++) {
		ret = rfid_protocol_read(ph, i, buf, &len);
		if (ret < 0)
			return ret;

		printf("Page 0x%x: %s\n", i, hexdump(buf, 4));
	}
	return 0;
}

/* mifare classic helpers */
static int
mifare_classic_read_sector(struct rfid_protocol_handle *ph, int sector)
{
	unsigned char buf[20];
	unsigned int len = sizeof(buf);
	int ret;
	int block, blocks_per_sector, first_block;

	printf("Reading sector %u\n", sector);

	first_block = mfcl_sector2block(sector);
	blocks_per_sector = mfcl_sector_blocks(sector);

	if (first_block < 0 || blocks_per_sector < 0)
		return -EINVAL;

	for (block = first_block; block < first_block + blocks_per_sector;
	     block++) {
		printf("Reading block %u: ", block);
		ret = rfid_protocol_read(ph, block, buf, &len);
		if (ret == -ETIMEDOUT)
			fprintf(stderr, "TIMEOUT\n");
		if (ret < 0) {
			printf("Error %d reading\n", ret);
			return ret;
		}

		printf("Page 0x%x: %s\n", block, hexdump(buf, len));
	}
	return 0;
}

static int
mifare_classic_dump(struct rfid_protocol_handle *ph)
{
	unsigned int size;
	unsigned int size_len = sizeof(size);
	int sector, num_sectors;

	if (rfid_protocol_getopt(ph, RFID_OPT_PROTO_SIZE, 
				 &size, &size_len) == 0) {
		printf("Size: %u bytes\n", size);
	} else {
		printf("Size: unknown ?!?\n");
		return -EINVAL;
	}

	switch (size) {
	case 320:
		num_sectors = 5;
		break;
	case 1024:
		num_sectors = 16;
		break;
	case 4096:
		num_sectors = 40;
		break;
	default:
		return -EINVAL;
	}

	for (sector = 0; sector < num_sectors; sector++) {
		int rc;

		printf("Authenticating sector %u: ", sector);
		fflush(stdout);

		rc = mfcl_set_key(ph, MIFARE_CL_KEYA_DEFAULT_INFINEON);
		if (rc < 0) {
			printf("key format error\n");
			exit(1);
		}

		rc = mfcl_auth(ph, RFID_CMD_MIFARE_AUTH1A,
			       mfcl_sector2block(sector));
		if (rc < 0) {
			printf("mifare auth error\n");
			exit(1);
		} else 
			printf("mifare auth succeeded!\n");

		mifare_classic_read_sector(ph, sector);
	}
}

static char *proto_names[] = {
	[RFID_PROTOCOL_TCL] = "tcl",
	[RFID_PROTOCOL_MIFARE_UL] = "mifare-ultralight",
	[RFID_PROTOCOL_MIFARE_CLASSIC] = "mifare-classic",
	[RFID_PROTOCOL_ICODE_SLI] = "icode",
	[RFID_PROTOCOL_TAGIT] = "tagit",
};

static int proto_by_name(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(proto_names); i++) {
		if (proto_names[i] == NULL)
			continue;
		if (!strcasecmp(name, proto_names[i]))
			return i;
	}
	return -1;
}

static char *l2_names[] = {
	[RFID_LAYER2_ISO14443A] = "iso14443a",
	[RFID_LAYER2_ISO14443B] = "iso14443b",
	[RFID_LAYER2_ISO15693] = "iso15693",
	[RFID_LAYER2_ICODE1] = "icode1",
};

static int l2_by_name(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(l2_names); i++) {
		if (l2_names[i] == NULL)
			continue;
		if (!strcasecmp(name, l2_names[i]))
			return i;
	}
	return -1;
}

static int do_scan(int first)
{
	int rc;
	unsigned int size;
	unsigned int size_len = sizeof(size);
	char *data;
	unsigned int data_len;

	if (first) {
		unsigned int opt;
		unsigned int optlen = sizeof(opt);

		/* turn off RF */
		opt = 1;
		rfid_reader_setopt(rh, RFID_OPT_RDR_RF_KILL, &opt, optlen);

		usleep(10*1000);

		/* turn on RF */
		opt = 0;
		rfid_reader_setopt(rh, RFID_OPT_RDR_RF_KILL, &opt, optlen);
	}
	printf("scanning for RFID token...\n");
	rc = rfid_scan(rh, &l2h, &ph);
	if (rc >= 2) {
		unsigned char uid_buf[16];
		unsigned int uid_len = sizeof(uid_buf);
		rfid_layer2_getopt(l2h, RFID_OPT_LAYER2_UID, &uid_buf,
				   &uid_len);
		printf("Layer 2 success (%s): %s\n", rfid_layer2_name(l2h),
			hexdump(uid_buf, uid_len));
	}
	if (rc >= 3) {
		printf("Protocol success (%s)\n", rfid_protocol_name(ph));

		if (rfid_protocol_getopt(ph, RFID_OPT_PROTO_SIZE, 
					 &size, &size_len) == 0)
			printf("Size: %u bytes\n", size);
		size_len = sizeof(size);
		size = 0;
		if (rfid_protocol_getopt(ph, RFID_OPT_P_TCL_ATS_LEN,
					 &size, &size_len) == 0) {
			data_len = size + 1;
			data = malloc(data_len);
			if (data) {
				if (rfid_protocol_getopt(ph, RFID_OPT_P_TCL_ATS,
							 data, &data_len) == 0) {
					printf("Got ATS of %u bytes: %s\n", size,
					       hexdump(data, data_len));
				}
			}
		}
	}

	return rc;
}

static void do_endless_scan()
{
	int rc;
	int first = 1;

	while (1) {
		if (first)
			putc('\n', stdout);
		printf("==> doing %s scan\n", first ? "first" : "successive");
		rc = do_scan(first);
		if (rc >= 3) {
			printf("closing proto\n");
			rfid_protocol_close(ph);
		}
		if (rc >= 2) {
			printf("closing layer2\n");
			rfid_layer2_close(l2h);
			first = 0;
		} else
			first = 1;
	}
}

static void do_regdump(void)
{
	u_int8_t buffer[0xff];
	int i;

	printf("dumping rc632 regs...\n");

	rc632_register_dump(rh->ah, buffer);

	printf("\n     ");
	for (i=0; i<=0x0f; i++)
		printf(" 0x_%01X",i);
	printf("\n-----------------------------------------------------------------------------------\n");

	for (i=0; i <= 0x3f; i++) {
		if ((i % 0x10) == 0)
			printf("0x%01X_:",i/0x10);
		printf(" 0x%02X", buffer[i]);
		if ((i% 0x10) == 0x0f)
			printf("\n");
	}

	/* print regdump as c-style array*/
	printf("u_int8_t rc632_regs[] = {");
	for (i = 0; i <= 0x3f; i++) {
		if (((i+1) % 0x08) == 1) {
			if (i > 7)
				printf("//%2d..%2d",i-8,i-1);
			printf("\n\t");
		}
		printf(" 0x%02X, ",buffer[i]);
	}
	printf("//%2d..%2d\n\t 0 };\n",i-8,i-1);

}

static void do_enum(int layer2)
{
	int rc;
	//unsigned int size;
	//unsigned int size_len = sizeof(size);
	unsigned char uid_buf[16];
	unsigned int uid_len;

	printf("scanning for RFID token on layer %s...\n", l2_names[layer2]);

	if (rh->reader->l2_supported & (1 << layer2)) {
		l2h = rfid_layer2_init(rh, layer2);
		rc = rfid_layer2_open(l2h);
	} else {
		printf("error during layer2_open\n");
		return ;
	}

	while (rc>=0) {
		if (l2h) {
			uid_len = sizeof(uid_buf);
			rfid_layer2_getopt(l2h, RFID_OPT_LAYER2_UID, &uid_buf, &uid_len);
			printf("Layer 2 success (%s)[%d]: %s\n", rfid_layer2_name(l2h), uid_len, hexdump(uid_buf, uid_len));
		}

       	/*
		ph = rfid_protocol_scan(l2h);
		if (ph) {
			printf("Protocol success (%s)\n", rfid_protocol_name(ph));

			if (rfid_protocol_getopt(ph, RFID_OPT_PROTO_SIZE,
					     &size, &size_len) == 0)
			printf("Size: %u bytes\n", size);
		} else
			printf("##############\n");
		*/

		if (rc >= 0) {
			rfid_layer2_close(l2h);
		}
		rc = rfid_layer2_open(l2h);
	}
}

#define OPTION_OFFSET 256

static struct option original_opts[] = {
	{ "help", 0, 0, 'h' },
	{ "layer2", 1, 0, 'l' },
	{ "protocol", 1, 0, 'p' },
	{ "scan", 0, 0, 's' },
	{ "scan-loop", 0, 0, 'S' },
	{ "dump", 0, 0, 'd' },
	{ "enum", 0, 0, 'e' },
	{0, 0, 0, 0}
};

/* module / option merging code */
static struct option *opts = original_opts;
static unsigned int global_option_offset = 0;

static char *program_name;
static char *program_version = LIBRFID_TOOL_VERSION;

static void free_opts(int reset_offset)
{
	if (opts != original_opts) {
		free(opts);
		opts = original_opts;
		if (reset_offset)
			global_option_offset = 0;
	}
}

static struct option *
merge_options(struct option *oldopts, const struct option *newopts,
	      unsigned int *option_offset)
{
	unsigned int num_old, num_new, i;
	struct option *merge;

	for (num_old = 0; oldopts[num_old].name; num_old++);
	for (num_new = 0; oldopts[num_new].name; num_new++);

	global_option_offset += OPTION_OFFSET;
	*option_offset = global_option_offset;

	merge = malloc(sizeof(struct option) * (num_new + num_old + 1));
	memcpy(merge, oldopts, num_old * sizeof(struct option));
	free_opts(0); /* Release previous options merged if any */
	for (i = 0; i < num_new; i++) {
		merge[num_old + i] = newopts[i];
		merge[num_old + i].val += *option_offset;
	}
	memset(merge + num_old + num_new, 0, sizeof(struct option));

	return merge;
}

struct rfidtool_module *find_module(const char *name)
{
	return NULL;
}

void register_module(struct rfidtool_module *me)
{
	struct rfidtool_module *old;

	if (strcmp(me->version, program_version) != 0) {
		fprintf(stderr, "%s: target `%s' v%s (I'm v%s).\n",
			program_name, me->name, me->version, program_version);
		exit(1);
	}

	old = find_module(me->name);
	if (old) {
		fprintf(stderr, "%s: target `%s' already registered.\n",
			program_name, me->name);
		exit(1);
	}
}

static void help(void)
{
	printf( " -s	--scan		scan until first RFID tag is found\n"
		" -S	--scan-loop	endless scanning loop\n" 
		" -p	--protocol	{tcl,mifare-ultralight,mifare-classic,tagit}\n"
		" -l	--layer2	{iso14443a,iso14443b,iso15693}\n"
		" -d	--dump		dump rc632 registers\n"
		" -e	--enum		enumerate all tag's in field (iso14443a)\n"
		" -h	--help\n");
}

int main(int argc, char **argv)
{
	int rc;
	char buf[0x100];
	int i, len, protocol = -1, layer2 = -1;

#ifdef  __MINGW32__
	program_name = argv[0];
#else /*__MINGW32__*/
	program_name = basename(argv[0]);
#endif/*__MINGW32__*/
	
	printf("%s - (C) 2005-2008 by Harald Welte\n"
	       "This program is Free Software and has "
	       "ABSOLUTELY NO WARRANTY\n\n", program_name);

	printf("initializing librfid\n");
	rfid_init();

	while (1) {
		int c, option_index = 0;
		c = getopt_long(argc, argv, "hp:l:sSde", opts, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'e':
			if (reader_init() < 0)
				exit(1);
			layer2 = RFID_LAYER2_ISO14443A;
			do_enum(layer2);
			exit(0);
			break;
		case 'd':
			if (reader_init() < 0)
				exit(1);
			do_regdump();
			break;
		case 's':
			if (reader_init() < 0)
				exit(1);
			do_scan(0);
			exit(0);
			break;
		case 'S':
			if (reader_init() < 0)
				exit(1);
			do_endless_scan();
			exit(0);
			break;
		case 'p':
			protocol = proto_by_name(optarg);
			if (protocol < 0) {
				fprintf(stderr, "unknown protocol `%s'\n", 
					optarg);
				exit(2);
			}
			break;
		case 'l':
			layer2 = l2_by_name(optarg);
			if (layer2 < 0) {
				fprintf(stderr, "unknown layer2 `%s'\n",
					optarg);
				exit(2);
			}
			break;
		case 'h':
			help();
			exit(0);
			break;
		}
	}

	switch (protocol) {
	case RFID_PROTOCOL_MIFARE_UL:
	case RFID_PROTOCOL_MIFARE_CLASSIC:
		layer2 = RFID_LAYER2_ISO14443A;
		break;
	case -1:
		fprintf(stderr, "you have to specify --protocol\n");
		exit(2);
	}

	if (layer2 < 0) {
		fprintf(stderr, "you have to specify --layer2\n");
		exit(2);
	}
	
	if (reader_init() < 0)
		exit(1);


	if (l2_init(layer2) < 0) {
		rfid_reader_close(rh);
		exit(1);
	}

	if (l3_init(protocol) < 0) {
		rfid_reader_close(rh);
		exit(1);
	}

	switch (protocol) {

	case RFID_PROTOCOL_TCL:
		printf("Protocol T=CL\n");
		/* we've established T=CL at this point */
		printf("selecting Master File\n");
		rc = select_mf();
		if (rc < 0) {
			printf("error selecting MF\n");
			break;
		}

		printf("Getting random challenge, length 255\n");
		rc = iso7816_get_challenge(0xff);
		if (rc < 0) {
			printf("error getting random challenge\n");
			break;
		}

		printf("selecting Passport application\n");
		rc = iso7816_select_application();
		if (rc < 0) {
			printf("error selecting passport application\n");
			break;
		}

		printf("selecting EF 0x1e\n");
		rc = iso7816_select_ef(0x011e);
		if (rc < 0) {
			printf("error selecting EF 0x1e\n");
			break;
		}

		printf("selecting EF 0x01\n");
		rc = iso7816_select_ef(0x0101);
		if (rc < 0) {
			printf("error selecting EF 0x01\n");
			break;
		}

		while (1) {
			printf("reading EF1\n");
			len = sizeof(buf);
			printf("reading ef\n");
			rc = iso7816_read_binary(buf, &len);
			if (rc < 0) {
				printf("error reading EF\n");
				break;
			}
		}
#if 0
		for (i = 0; i < 4; i++)
			iso7816_get_challenge(0xff);
#endif
		break;
	case RFID_PROTOCOL_MIFARE_UL:
		printf("Protocol Mifare Ultralight\n");
		mifare_ulight_read(ph);
#if 0
		mifare_ulight_blank(ph);
		mifare_ulight_write(ph);
		mifare_ulight_read(ph);
#endif
		break;
	case RFID_PROTOCOL_MIFARE_CLASSIC:
		printf("Protocol Mifare Classic\n");
		mifare_classic_dump(ph);
		break;
	default:
		printf("unknown protocol %u\n", protocol);
		exit(1);
		break;
	}

	rfid_reader_close(rh);
	
	exit(0);
}
