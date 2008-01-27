/* librfid - layer 2 protocol handler 
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
#include <stdio.h>
#include <errno.h>
#include <string.h> /* for memcpy */

#include <librfid/rfid.h>
#include <librfid/rfid_layer2.h>

static const struct rfid_layer2 *rfid_layer2s[] = {
	[RFID_LAYER2_ISO14443A]	= &rfid_layer2_iso14443a,
	[RFID_LAYER2_ISO14443B]	= &rfid_layer2_iso14443b,
	[RFID_LAYER2_ISO15693]	= &rfid_layer2_iso15693,
};

struct rfid_layer2_handle *
rfid_layer2_init(struct rfid_reader_handle *rh, unsigned int id)
{
	struct rfid_layer2 *p;

	if (id >= ARRAY_SIZE(rfid_layer2s)) {
		DEBUGP("unable to find matching layer2 protocol\n");
		return NULL;
	}

	p = rfid_layer2s[id];
	return p->fn.init(rh);
}

int
rfid_layer2_open(struct rfid_layer2_handle *ph)
{
	if (!ph->l2->fn.open)
		return 0;

	return ph->l2->fn.open(ph);
}

int
rfid_layer2_transceive(struct rfid_layer2_handle *ph,
			enum rfid_frametype frametype,
			 const unsigned char *tx_buf, unsigned int len,
			 unsigned char *rx_buf, unsigned int *rx_len,
			 u_int64_t timeout, unsigned int flags)
{
	if (!ph->l2->fn.transceive)
		return -EIO;

	return ph->l2->fn.transceive(ph, frametype, tx_buf, len, rx_buf,
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
rfid_layer2_getopt(struct rfid_layer2_handle *ph, int optname,
		   void *optval, unsigned int *optlen)
{
	if (optname >> 16 == 0) {
		unsigned char *optchar = optval;

		switch (optname) {
		case RFID_OPT_LAYER2_UID:
			if (ph->uid_len < *optlen)
				*optlen = ph->uid_len;
			memcpy(optchar, ph->uid, *optlen);
			break;
		default:
			return -EINVAL;
			break;
		}
	} else {
		if (!ph->l2->fn.getopt)
			return -EINVAL;

		return ph->l2->fn.getopt(ph, optname, optval, optlen);
	}
	return 0;
}

int
rfid_layer2_setopt(struct rfid_layer2_handle *ph, int optname,
		   const void *optval, unsigned int optlen)
{
	if (optname >> 16 == 0) {
		switch (optname) {
		case RFID_OPT_LAYER2_UID:
		printf("----> sizeof(ph->uid): %d\n",sizeof(ph->uid));
			if ((ph->uid_len < sizeof(ph->uid)) && (optlen<=sizeof(ph->uid))) {
				//(ph->uid_len<optlen)
				ph->uid_len = optlen;
				memcpy(ph->uid, optval, optlen);
			} else
				return -EINVAL;
		break;
		default:
			return -EINVAL;
			break;
		}
	} else {
		if (!ph->l2->fn.setopt)
			return -EINVAL;

		return ph->l2->fn.setopt(ph, optname, optval, optlen);
	}
	return 0;
}

char *rfid_layer2_name(struct rfid_layer2_handle *l2h)
{
	return l2h->l2->name;
}
