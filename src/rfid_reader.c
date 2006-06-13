/* librfid - core reader handling
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
#include <stdio.h>

#include <librfid/rfid.h>
#include <librfid/rfid_reader.h>

static struct rfid_reader *rfid_reader_list;

struct rfid_reader_handle *
rfid_reader_open(void *data, unsigned int id)
{
	struct rfid_reader *p;

	for (p = rfid_reader_list; p; p = p->next)
		if (p->id == id)
			return p->open(data);

	DEBUGP("unable to find matching reader\n");
	return NULL;
}

int
rfid_reader_transceive(struct rfid_reader_handle *rh,
			enum rfid_frametype frametype,
			 const unsigned char *tx_buf, unsigned int len,
			 unsigned char *rx_buf, unsigned int *rx_len,
			 u_int64_t timeout, unsigned int flags)
{
	return rh->reader->transceive(rh, frametype, tx_buf, len, rx_buf,
				      rx_len, timeout, flags);
}

void
rfid_reader_close(struct rfid_reader_handle *rh)
{
	rh->reader->close(rh);
}

int
rfid_reader_register(struct rfid_reader *r)
{
	r->next = rfid_reader_list;
	rfid_reader_list = r;

	return 0;
}
