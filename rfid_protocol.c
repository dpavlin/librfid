/* librfid - layer 3 protocol handler 
 * (C) 2005 by Harald Welte <laforge@gnumonks.org>
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdlib.h>
#include <unistd.h>

#include <rfid/rfid_layer2.h>
#include <rfid/rfid_protocol.h>

static struct rfid_protocol *rfid_protocol_list;

struct rfid_protocol_handle *
rfid_protocol_init(struct rfid_layer2_handle *l2h, unsigned int id)
{
	struct rfid_protocol *p;

	for (p = rfid_protocol_list; p; p = p->next)
		if (p->id == id)
			return p->fn.init(l2h);

	return NULL;
}

int
rfid_protocol_open(struct rfid_protocol_handle *ph)
{
	return ph->proto->fn.open(ph);
}

int
rfid_protocol_transcieve(struct rfid_protocol_handle *ph,
			 const unsigned char *tx_buf, unsigned int len,
			 unsigned char *rx_buf, unsigned int *rx_len,
			 unsigned int timeout, unsigned int flags)
{
	return ph->proto->fn.transcieve(ph, tx_buf, len, rx_buf, rx_len,
					timeout, flags);
}

int rfid_protocol_fini(struct rfid_protocol_handle *ph)
{
	return ph->proto->fn.fini(ph);
}

int
rfid_protocol_close(struct rfid_protocol_handle *ph)
{
	return ph->proto->fn.close(ph);
}

int
rfid_protocol_register(struct rfid_protocol *p)
{
	p->next = rfid_protocol_list;
	rfid_protocol_list = p;

	return 0;
}
