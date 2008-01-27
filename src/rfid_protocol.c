/* librfid - layer 4 protocol handler 
 * (C) 2005-2006 by Harald Welte <laforge@gnumonks.org>
 */

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

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <librfid/rfid_layer2.h>
#include <librfid/rfid_protocol.h>

static const struct rfid_protocol *rfid_protocols[] = {
	[RFID_PROTOCOL_MIFARE_CLASSIC]	= &rfid_protocol_mfcl,
	[RFID_PROTOCOL_MIFARE_UL] 	= &rfid_protocol_mful,
	[RFID_PROTOCOL_TCL]		= &rfid_protocol_tcl,
	[RFID_PROTOCOL_TAGIT]		= &rfid_protocol_tagit,
};

struct rfid_protocol_handle *
rfid_protocol_init(struct rfid_layer2_handle *l2h, unsigned int id)
{
	const struct rfid_protocol *p;
	struct rfid_protocol_handle *ph = NULL;

	if (id >= ARRAY_SIZE(rfid_protocols))
		return NULL;

	p = rfid_protocols[id];

	ph = p->fn.init(l2h);
	if (!ph)
		return NULL;

	ph->proto = p;
	ph->l2h = l2h;

	return ph;
}

int
rfid_protocol_open(struct rfid_protocol_handle *ph)
{
	if (ph->proto->fn.open)
		return ph->proto->fn.open(ph);
	return 0;
}

int
rfid_protocol_transceive(struct rfid_protocol_handle *ph,
			 const unsigned char *tx_buf, unsigned int len,
			 unsigned char *rx_buf, unsigned int *rx_len,
			 unsigned int timeout, unsigned int flags)
{
	return ph->proto->fn.transceive(ph, tx_buf, len, rx_buf, rx_len,
					timeout, flags);
}

int
rfid_protocol_read(struct rfid_protocol_handle *ph,
	 	   unsigned int page,
		   unsigned char *rx_data,
		   unsigned int *rx_len)
{
	if (ph->proto->fn.read)
		return ph->proto->fn.read(ph, page, rx_data, rx_len);
	else
		return -EINVAL;
}

int
rfid_protocol_write(struct rfid_protocol_handle *ph,
	 	   unsigned int page,
		   unsigned char *tx_data,
		   unsigned int tx_len)
{
	if (ph->proto->fn.write)
		return ph->proto->fn.write(ph, page, tx_data, tx_len);
	else
		return -EINVAL;
}

int rfid_protocol_fini(struct rfid_protocol_handle *ph)
{
	return ph->proto->fn.fini(ph);
}

int
rfid_protocol_close(struct rfid_protocol_handle *ph)
{
	if (ph->proto->fn.close)
		return ph->proto->fn.close(ph);
	return 0;
}

int
rfid_protocol_getopt(struct rfid_protocol_handle *ph, int optname,
		     void *optval, unsigned int *optlen)
{
	if (optname >> 16 == 0) {
		unsigned char *optchar = optval;

		switch (optname) {
		default:
			return -EINVAL;
			break;
		}
	} else {
		if (!ph->proto->fn.getopt)
			return -EINVAL;

		return ph->proto->fn.getopt(ph, optname, optval, optlen);
	}
	return 0;
}

int
rfid_protocol_setopt(struct rfid_protocol_handle *ph, int optname,
		     const void *optval, unsigned int optlen)
{
	if (optname >> 16 == 0) {
		switch (optname) {
		default:
			return -EINVAL;
			break;
		}
	} else {
		if (!ph->proto->fn.setopt)
			return -EINVAL;

		return ph->proto->fn.setopt(ph, optname, optval, optlen);
	}
	return 0;
}

char *rfid_protocol_name(struct rfid_protocol_handle *ph)
{
	return ph->proto->name;
}
