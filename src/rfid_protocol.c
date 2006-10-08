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
#include <errno.h>

#include <librfid/rfid_layer2.h>
#include <librfid/rfid_protocol.h>

static struct rfid_protocol *rfid_protocol_list;

struct rfid_protocol_handle *
rfid_protocol_init(struct rfid_layer2_handle *l2h, unsigned int id)
{
	struct rfid_protocol *p;
	struct rfid_protocol_handle *ph = NULL;

	for (p = rfid_protocol_list; p; p = p->next) {
		if (p->id == id) {
			ph = p->fn.init(l2h);
			break;
		}
	}

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
rfid_protocol_register(struct rfid_protocol *p)
{
	p->next = rfid_protocol_list;
	rfid_protocol_list = p;

	return 0;
}

char *rfid_protocol_name(struct rfid_protocol_handle *ph)
{
	return ph->proto->name;
}
