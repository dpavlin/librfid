/*                                                 -*- linux-c -*-
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <rfid/rfid.h>
#include <rfid/rfid_reader.h>
#include <rfid/rfid_layer2.h>
#include <rfid/rfid_protocol.h>
#include <rfid/rfid_reader_cm5121.h>
#include <rfid/rfid_protocol_mifare_classic.h>

static struct rfid_reader_handle *rh;
static struct rfid_layer2_handle *l2h;
static struct rfid_protocol_handle *ph;

static int init()
{
	unsigned char buf[0x3f];
	int rc;

	printf("initializing librfid\n");
	rfid_init();

	printf("opening reader handle\n");
	rh = rfid_reader_open(NULL, RFID_READER_CM5121);
	if (!rh) {
		fprintf(stderr, "error, no cm5121 handle\n");
		return -1;
	}

	sleep(2);

	printf("opening layer2 handle\n");
	l2h = rfid_layer2_init(rh, RFID_LAYER2_ISO14443A);
	//l2h = rfid_layer2_init(rh, RFID_LAYER2_ISO14443B);
	if (!l2h) {
		fprintf(stderr, "error during iso14443a_init\n");
		return -1;
	}

	//rc632_register_dump(rh->ah, buf);

	printf("running layer2 anticol\n");
	rc = rfid_layer2_open(l2h);
	if (rc < 0) {
		fprintf(stderr, "error during layer2_open\n");
		return rc;
	}

	return 0;
}

static int l3(int protocol)
{
	printf("running layer3 (ats)\n");
	ph = rfid_protocol_init(l2h, protocol);
	if (!ph) {
		fprintf(stderr, "error during protocol_init\n");
		return -1;
	}
	if (rfid_protocol_open(ph) < 0) {
		fprintf(stderr, "error during protocol_open\n");
		return -1;
	}

	printf("we now have layer3 up and running\n");

	return 0;
}

static int select_mf(void)
{
	unsigned char cmd[] = { 0x00, 0xa4, 0x00, 0x00, 0x02, 0x3f, 0x00, 0x00 };
	unsigned char ret[256];
	unsigned int rlen = sizeof(ret);

	int rv;

	rv = rfid_protocol_transcieve(ph, cmd, sizeof(cmd), ret, &rlen, 0, 0);
	if (rv < 0)
		return rv;

	printf("%s\n", rfid_hexdump(ret, rlen));

	return 0;
}


static int iso7816_get_challenge(unsigned char len)
{
	unsigned char cmd[] = { 0x00, 0x84, 0x00, 0x00, 0x08 };
	unsigned char ret[256];
	unsigned int rlen = sizeof(ret);

	cmd[4] = len;

	int rv;

	rv = rfid_protocol_transcieve(ph, cmd, sizeof(cmd), ret, &rlen, 0, 0);
	if (rv < 0)
		return rv;

	printf("%d: [%s]\n", rlen, rfid_hexdump(ret, rlen));

	return 0;
}

int
iso7816_select_application(void)
{
	char cmd[] = { 0x00, 0xa4, 0x04, 0x0c, 0x07,
		       0xa0, 0x00, 0x00, 0x02, 0x47, 0x10, 0x01 };
	char resp[7];
	unsigned int rlen = sizeof(resp);

	int rv;

	rv = rfid_protocol_transcieve(ph, cmd, sizeof(cmd), resp, &rlen, 0, 0);
	if (rv < 0)
		return rv;

	/* FIXME: parse response */
	printf("%s\n", rfid_hexdump(resp, rlen));

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

	rv = rfid_protocol_transcieve(ph, cmd, sizeof(cmd), resp, &rlen, 0, 0);
	if (rv < 0)
		return rv;

	/* FIXME: parse response */
	printf("%s\n", rfid_hexdump(resp, rlen));

	return 0;
}

int
iso7816_read_binary(unsigned char *buf, unsigned int *len)
{
	unsigned char cmd[] = { 0x00, 0xb0, 0x00, 0x00, 0x00 };
	unsigned char resp[256];
	unsigned int rlen = sizeof(resp);
	
	int rv;

	rv = rfid_protocol_transcieve(ph, cmd, sizeof(cmd), resp, &rlen, 0, 0);
	if (rv < 0)
		return rv;

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

		printf("Page 0x%x: %s\n", i, rfid_hexdump(buf, 4));
	}
	return 0;
}

/* mifare classic helpers */
static int
mifare_classic_read(struct rfid_protocol_handle *ph)
{
	unsigned char buf[20];
	unsigned int len = sizeof(buf);
	int ret;
	int i;

	for (i = 0; i <= MIFARE_CL_PAGE_MAX; i++) {
		ret = rfid_protocol_read(ph, i, buf, &len);
		if (ret < 0)
			return ret;

		printf("Page 0x%x: %s\n", i, rfid_hexdump(buf, 4));
	}
	return 0;
}


int main(int argc, char **argv)
{
	int rc;
	char buf[0x40];
	int i, protocol;

#if 0
        if (argc) {
                argc--;
                argv++;
        }
        
        while (argc) {
                if ( !strcmp (*argv, "--list")) {
                        char *p;
                        p = ccid_get_reader_list ();
                        if (!p)
                                return 1;
                        fputs (p, stderr);
                        free (p);
                        return 0;
                }
                else if ( !strcmp (*argv, "--debug")) {
                        ccid_set_debug_level (ccid_set_debug_level (-1) + 1);
                        argc--; argv++;
                }
                else
                        break;
        }
#endif

	if (init() < 0)
		exit(1);

	//protocol = RFID_PROTOCOL_MIFARE_UL;
	//protocol = RFID_PROTOCOL_MIFARE_CLASSIC;
	protocol = RFID_PROTOCOL_TCL;

	if (l3(protocol) < 0)
		exit(1);

	switch (protocol) {
	case RFID_PROTOCOL_TCL:
		/* we've established T=CL at this point */
		select_mf();

		iso7816_select_application();
		iso7816_select_ef(0x011e);
		iso7816_select_ef(0x0101);
#if 1
		for (i = 0; i < 4; i++)
			iso7816_get_challenge(0x60);
#endif
		break;
	case RFID_PROTOCOL_MIFARE_UL:
		mifare_ulight_read(ph);
#if 0
		mifare_ulight_blank(ph);
		mifare_ulight_write(ph);
		mifare_ulight_read(ph);
#endif
		break;
	case RFID_PROTOCOL_MIFARE_CLASSIC:
		rc = mfcl_set_key(ph, MIFARE_CLASSIC_KEYB_DEFAULT);
		if (rc < 0) {
			printf("key format error\n");
			exit(1);
		}
		rc = mfcl_auth(ph, RFID_CMD_MIFARE_AUTH1B, 10);
		if (rc < 0) {
			printf("mifare auth error\n");
			exit(1);
		} else 
			printf("mifare authe succeeded!\n");
		mifare_classic_read(ph);
		break;
	}

	rfid_reader_close(rh);
	
	exit(0);
}
