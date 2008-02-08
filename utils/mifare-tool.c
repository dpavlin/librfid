/* mifare-tool - a small command-line tool for librfid mifare testing
 *
 * (C) 2006 by Harald Welte <laforge@gnumonks.org>
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

#include <librfid/rfid_protocol_mifare_classic.h>
#include <librfid/rfid_protocol_mifare_ul.h>

#include <librfid/rfid_access_mifare_classic.h>

#include "librfid-tool.h"

static char *program_name;

static void help(void)
{
	printf( " -h	--help		Print this help message\n"
		" -r	--read		Read a mifare sector\n"
		" -l	--loop-read	Loop reading a mifare sector\n"
		" -w	--write		Write a mifare sector\n"
		" -k	--key		Specify mifare access key (in hex tuples)\n"
		" -b    --brute-force n	Brute Force read sector n\n");
}

static struct option mifare_opts[] = {
	{ "key", 1, 0, 'k' },
	{ "read", 1, 0, 'r' },
	{ "loop-read", 1, 0, 'l' },
	{ "write", 1 ,0, 'w' },
	{ "help", 0, 0, 'h' },
	{ "brute-force", 1, 0, 'b' },
	{ 0, 0, 0, 0 }
};

static int mifare_cl_auth(unsigned char *key, int page)
{
	int rc;

	rc = mfcl_set_key(ph, key);
	if (rc < 0) {
		fprintf(stderr, "key format error\n");
		return rc;
	}
	rc = mfcl_auth(ph, RFID_CMD_MIFARE_AUTH1A, page);
	if (rc < 0) {
		fprintf(stderr, "mifare auth error\n");
		return rc;
	} else 
		printf("mifare auth succeeded!\n");
	
	return 0;
}

static void mifare_l3(void)
{
	while (l2_init(RFID_LAYER2_ISO14443A) < 0) ;

	printf("ISO14443-3A anticollision succeeded\n");

	while (l3_init(RFID_PROTOCOL_MIFARE_CLASSIC) < 0) ;

	printf("Mifare card available\n");
}

int main(int argc, char **argv)
{
	int len, rc, c, option_index = 0;
	unsigned int page,uid,uid_len;
	char key[MIFARE_CL_KEY_LEN];
	char buf[MIFARE_CL_PAGE_SIZE];

#ifdef  __MINGW32__
	program_name = argv[0];
#else
	program_name = basename(argv[0]);
#endif/*__MINGW32__*/

	memcpy(key, MIFARE_CL_KEYA_DEFAULT_INFINEON, MIFARE_CL_KEY_LEN);

	printf("%s - (C) 2006 by Harald Welte\n"
	       "This program is Free Software and has "
	       "ABSOLUTELY NO WARRANTY\n\n", program_name);

	printf("initializing librfid\n");
	rfid_init();

	if (reader_init() < 0) {
		fprintf(stderr, "error opening reader\n");
		exit(1);
	}

	while (1) {
		c = getopt_long(argc, argv, "k:r:l:w:b:h", mifare_opts,
				&option_index);
		if (c == -1)
			break;

		switch (c) {
			int i;
		case 'b':
			page = atoi(optarg);
			printf("key: %s\n", hexdump(key, MIFARE_CL_KEY_LEN));
			len = MIFARE_CL_PAGE_SIZE;
			mifare_l3();
			for (i = 0; i <= 0xff; i++) {
				key[MIFARE_CL_KEY_LEN-1]=i;
				if (mifare_cl_auth(key, page) >= 0)
					printf("KEY: %s\n",hexdump(key, MIFARE_CL_KEY_LEN));
			}

			break;
		case 'k':
			hexread(key, optarg, strlen(optarg));
			printf("key: %s\n", hexdump(key, MIFARE_CL_KEY_LEN));
			break;
		case 'r':
			page = atoi(optarg);
			printf("read(key='%s',page=%u):",
				hexdump(key, MIFARE_CL_KEY_LEN), page);
			len = MIFARE_CL_PAGE_SIZE;
			mifare_l3();
			if (mifare_cl_auth(key, page) < 0)
				exit(1);

        		uid_len=sizeof(uid);
                    	uid=0;
                    	if(rfid_layer2_getopt(l2h,RFID_OPT_LAYER2_UID,&uid,&uid_len)>=0)
                            printf("UID=%08X (len=%u)\n",uid,uid_len);
				
	                len=MIFARE_CL_PAGE_SIZE;																				    				
			rc = rfid_protocol_read(ph, page, buf, &len);
			if (rc < 0) {
				printf("\n");
				fprintf(stderr, "error during read\n");
				break;
			}
			printf("len=%u data=%s\n", len, hexdump(buf, len));

			if (page & 0x3 == 0x3) {
				struct mfcl_access_sect s;
				struct mfcl_access_exp_sect es;
				int b;
				u_int8_t recreated[4];
				mfcl_parse_access(&s, buf+6);
				printf("access b0:%u b1:%u b2:%u b3:%u\n",
					s.block[0], s.block[1],
					s.block[2], s.block[3]);
				mfcl_access_to_exp(&es, &s);
				for (b = 0; b < 3; b++)
					printf("%u: %s\n", b, mfcl_access_exp_stringify(&es.block[b]));
				printf("3: %s\n", mfcl_access_exp_acc_stringify(&es.acc));
#if 0
				mfcl_compile_access(recreated, &s);
				printf("recreated; %s\n", hexdump(recreated,4));
#endif
			}
			break;
		case 'l':
			page = atoi(optarg);
			printf("read_loop(key='%s',page=%u):\n",
				hexdump(key, MIFARE_CL_KEY_LEN), page);
			while (1) {
				mifare_l3();
				if (mifare_cl_auth(key, page) < 0)
					continue;

                    		uid_len=sizeof(uid);
                    		uid=0;
                    		if(rfid_layer2_getopt(l2h,RFID_OPT_LAYER2_UID,&uid,&uid_len)>=0)
                        	    printf("UID=%08X (len=%u)\n",uid,uid_len);

	            		len=MIFARE_CL_PAGE_SIZE;																				    				
				rc = rfid_protocol_read(ph, page, buf, &len);
				if (rc < 0) {
					printf("\n");
					fprintf(stderr, "error during read\n");
					continue;
				}
				printf("%s\n", hexdump(buf, len));
			}
			break;
		case 'w':
			page = atoi(optarg);
			len = strlen(argv[optind]);
			len = hexread(buf, argv[optind], len);
			printf("write(key='%s',page=%u):",
				hexdump(key, MIFARE_CL_KEY_LEN), page);
			printf(" '%s'(%u):", hexdump(buf, len), len);
			mifare_l3();
			if (mifare_cl_auth(key, page) < 0)
				exit(1);
			rc = rfid_protocol_write(ph, page, buf, len); 
			if (rc < 0) {
				printf("\n");
				fprintf(stderr, "error during write\n");
				break;
			}
			printf("success\n");
			break;
		case 'h':
		default:
			help();
		}
	}

#if 0
	rfid_protocol_close(ph);
	rfid_protocol_fini(ph);

	rfid_layer2_close(l2h);
	rfid_layer2_fini(l2h);
#endif
	rfid_reader_close(rh);
	exit(0);
}

