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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <stdio.h>
#include <string.h>

#include <librfid/rfid_reader_cm5121.h>
#include <librfid/rfid_reader_openpcd.h>
#include <librfid/rfid_protocol.h>
#include <librfid/rfid_protocol_tcl.h>
#include <librfid/rfid_protocol_mifare_ul.h>
#include <librfid/rfid_protocol_mifare_classic.h>

const char *
rfid_hexdump(const void *data, unsigned int len)
{
	static char string[1024];
	unsigned char *d = (unsigned char *) data;
	unsigned int i, left;

	string[0] = '\0';
	left = sizeof(string);
	for (i = 0; len--; i += 3) {
		if (i >= sizeof(string) -4)
			break;
		snprintf(string+i, 4, " %02x", *d++);
	}
	return string;
}

int rfid_init()
{
	rfid_reader_register(&rfid_reader_cm5121);
	rfid_reader_register(&rfid_reader_openpcd);
	rfid_layer2_register(&rfid_layer2_iso14443a);
	rfid_layer2_register(&rfid_layer2_iso14443b);
	rfid_protocol_register(&rfid_protocol_tcl);
	rfid_protocol_register(&rfid_protocol_mful);
	rfid_protocol_register(&rfid_protocol_mfcl);

	return 0;
}

void rfid_fini()
{
	/* FIXME: implementation */
}
