/*
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
#include <ctype.h>

#define _GNU_SOURCE
#include <getopt.h>

#include <librfid/rfid.h>
#include <librfid/rfid_reader.h>
#include <librfid/rfid_layer2.h>
#include <librfid/rfid_protocol.h>

#include <librfid/rfid_protocol_mifare_classic.h>
#include <librfid/rfid_protocol_mifare_ul.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static const char *
hexdump(const void *data, unsigned int len)
{
	static char string[2048];
	unsigned char *d = (unsigned char *) data;
	unsigned int i, left, llen = len;

	string[0] = '\0';
	left = sizeof(string);
	for (i = 0; llen--; i += 3) {
		if (i >= sizeof(string) -4)
			break;
		snprintf(string+i, 4, " %02x", *d++);
	} return string;
	
	if (i >= sizeof(string) -2)
		return string;
	snprintf(string+i, 2, " ");
	i++; llen = len;
	
	d = (unsigned char *) data;
	for (; llen--; i += 1) {
		if (i >= sizeof(string) -2)
			break;
		snprintf(string+i, 2, "%c", isprint(*d) ? *d : '.');
		d++;
	}
	return string;
}

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

static int send_command(char* sbuf, int slen, char* rbuf, int rlen)
{
	int rv;
	static int doit;
	int answer, c = 0;

	if(doit == 0) {
		fprintf(stderr, "?? %s (%i bytes)\n", hexdump(sbuf, slen), slen);
		fprintf(stderr, "Execute? (Yes/No/All/Exit) ");
		answer = getc(stdin);
		if(answer != '\n') do {
			c = getc(stdin);
		} while(c != '\n' && c != EOF);
		switch(answer) {
		case 'y': // Fall-through
		case 'Y':
		case '\n':
			// Do nothing
			break;
		case 'n': // Fall-through
		case 'N':
			return 0;
			break;
		case 'a': // Fall-through
		case 'A':
			doit = 1;
			break;
		case 'e': // Fall-through
		case 'E':
			return -1;
			break;
		default:
			return 0; // Default to 'n'
			break;
		}
	}

	printf(">> %s (%i bytes)\n", hexdump(sbuf, slen), slen);

	rv = rfid_protocol_transceive(ph, sbuf, slen, rbuf, &rlen, 0, 0);
	if (rv < 0) {
		fprintf(stderr, "Error from transceive: %i\n", rv);
		return rv;
	}

	printf("<< %s (%i bytes)\n", hexdump(rbuf, rlen), rlen);

	if(rlen < 2 || rbuf[rlen-2] != (char)0x90 || rbuf[rlen-1] != 0x00) {
		fprintf(stderr, "SW is not 90 00. Ignore (i) or Abort (a)? ");
		answer = getc(stdin);
		if(answer != '\n') do {
			c = getc(stdin);
		} while(c != '\n' && c != EOF);
		switch(answer) {
		case 'i': // Fall-through
		case 'I':
		case '\n':
			// Do nothing
			break;
		case 'a': // Fall-through
		case 'A':
			return -1;
			break;
		default:
			return -1; // Default to 'a'
		}
	}

	return rlen;
}

static char *nextline(FILE* fh)
{
	int buflen = 1024; // FIXME Might want to increase dynamically?
	char *buffer = malloc(buflen);
	if (!buffer) {
		perror("malloc"); 
		return 0;
	}

	if (!fgets(buffer, buflen, fh)) {
		perror("fgets");
		free(buffer);
		return 0;
	}

	return buffer;
}

static int unhexchar(char c) {
	if ((c - '0') >= 0 && (c - '0') < 10) 
		return c-'0';
	if ((c - 'a') >= 0 && (c - 'a') < 6)
		return 10 + c-'a';
	if ((c - 'A') >= 0 && (c - 'A') < 6)
		return 10 + c-'A';
	return -1;
}

int make_command( const char *line, char **buffer, int *blen ) 
{
	int len = strlen(line), pos;
	*buffer = malloc( len );
	*blen = 0;
	if(!*buffer) {
		perror("malloc");
		return 0;
	}

	for(pos = 0; pos < len; pos++) {
		if(!isxdigit(line[pos]))
			continue;
		if(! (pos+1 < len) )
			continue;

		(*buffer)[*blen] = unhexchar(line[pos]) * 16 + unhexchar(line[pos+1]);
		pos += 1;
		*blen += 1;
	}
	return 1;
}

int main(int argc, char **argv)
{
	FILE *fh;
	int slen, rlen, retval = 0;
	char *next, *sbuf, *rbuf;

	if (argc != 2) {
		fprintf(stderr, "Syntax: %s scriptfile\n", argv[0]);
		exit(1);
	}

	fh = fopen(argv[1], "r");
	if (!fh) {
		perror("fopen");
		exit(1);
	}

	if (init() < 0)
		exit(1);

	if (l3(RFID_PROTOCOL_TCL) < 0)
		exit(1);

	printf("Protocol T=CL\n");
	/* we've established T=CL at this point */

	while (next = nextline(fh)) {
		if (!(strlen(next) >= 2 && strncmp(next, "//", 2) == 0)) {
			if (make_command(next, &sbuf, &slen)) {
				rlen = 1024;
				rbuf = calloc(rlen, 1);
				
				retval = send_command(sbuf, slen, rbuf, rlen);

				free(sbuf);
				free(rbuf);
			}
		}
		free(next);

		if (retval < 0)
			break;
	}

	rfid_reader_close(rh);
	
	exit(0);
}
