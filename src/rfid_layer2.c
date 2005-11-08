/* librfid - layer 2 protocol handler 
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
#include <errno.h>

#include <rfid/rfid.h>
#include <rfid/rfid_layer2.h>

static struct rfid_layer2 *rfid_layer2_list;

struct rfid_layer2_handle *
rfid_layer2_init(struct rfid_reader_handle *rh, unsigned int id)
{
	struct rfid_layer2 *p;

	for (p = rfid_layer2_list; p; p = p->next)
		if (p->id == id)
			return p->fn.init(rh);

	DEBUGP("unable to find matching layer2 protocol\n");
	return NULL;
}

int
rfid_layer2_open(struct rfid_layer2_handle *ph)
{
	if (!ph->l2->fn.open)
		return 0;

	return ph->l2->fn.open(ph);
}

int
rfid_layer2_transcieve(struct rfid_layer2_handle *ph,
			enum rfid_frametype frametype,
			 const unsigned char *tx_buf, unsigned int len,
			 unsigned char *rx_buf, unsigned int *rx_len,
			 u_int64_t timeout, unsigned int flags)
{
	if (!ph->l2->fn.transcieve)
		return -EIO;

	return ph->l2->fn.transcieve(ph, frametype, tx_buf, len, rx_buf,
				     rx_len, timeout, flags);
}

int rfid_layer2_fini(struct rfid_layer2_handle *ph)
{
	if (!ph->l2->fn.fini)
		return 0;

	return ph->l2->fn.fini(ph);
}

int
rfid_layer2_close(struct rfid_layer2_handle *ph)
{
	if (!ph->l2->fn.close)
		return 0;

	return ph->l2->fn.close(ph);
}

int
rfid_layer2_register(struct rfid_layer2 *p)
{
	p->next = rfid_layer2_list;
	rfid_layer2_list = p;

	return 0;
}

int
rfid_layer2_getopt(struct rfid_layer2_handle *ph, int optname,
		   void *optval, unsigned int *optlen)
{
	if (!ph->l2->fn.getopt)
		return -EINVAL;

	return ph->l2->fn.getopt(ph, optname, optval, optlen);
}

int
rfid_layer2_setopt(struct rfid_layer2_handle *ph, int optname,
		   const void *optval, unsigned int optlen)
{
	if (!ph->l2->fn.setopt)
		return -EINVAL;

	return ph->l2->fn.setopt(ph, optname, optval, optlen);
}
