/* librfid core 
 *  (C) 2005-2006 by Harald Welte <laforge@gnumonks.org>
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
#include <string.h>

#include <librfid/rfid.h>
#include <librfid/rfid_reader.h>
#include <librfid/rfid_protocol.h>
#include <librfid/rfid_protocol_tcl.h>
#include <librfid/rfid_protocol_mifare_ul.h>
#include <librfid/rfid_protocol_mifare_classic.h>

#ifdef LIBRFID_STATIC
struct rfid_asic_handle rfid_ah;
struct rfid_layer2_handle rfid_l2h;
struct rfid_protocol_handle rfid_ph;
struct rfid_asic_transport_handle rfid_ath;
struct rfid_reader_handle rfid_rh;
#endif

#ifndef LIBRFID_FIRMWARE
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
#else
#define rfid_hexdump(x, y) hexdump(x, y)
#endif

#if 0
int rfid_setopt(struct rfid_handle *rh, unsigned int level,
		unsigned int optname,
		const void *opt, unsigned int *optlen)
{
	switch (level) {
	case RFID_LEVEL_READER:
		return rfid_reader_setopt(optname, opt, optlen);
		break;
	case RFID_LEVEL_LAYER2:
		return rfid_layer2_setopt(optname, opt, optlen);
		break;
	case RFID_LEVEL_LAYER3:
		return rfid_layer3_setopt(optname, opt, optlen);
		break;
	case RFID_LEVEL_ASIC:
	default:
		return -EINVAL;
		break;
	}

	return 0;
}

int rfid_getopt(struct rfid_handle *rh, unsigned int level,
		unsigned int optname,
		void *opt, unsigned int *optlen)
{
	switch (level) {
	case RFID_LEVEL_READER:
		return rfid_reader_getopt(optname, opt, optlen);
		break;
	case RFID_LEVEL_LAYER2:
		return rfid_layer2_getopt(optname, opt, optlen);
		break;
	case RFID_LEVEL_LAYER3:
		return rfid_layer3_getopt(optname, opt, optlen);
		break;
	case RFID_LEVEL_ASIC:
	default:
		return -EINVAL;
		break;
	}

	return 0;
}
#endif

int rfid_init()
{
	return 0;
}

void rfid_fini()
{
	/* FIXME: implementation */
}
