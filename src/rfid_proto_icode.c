
/* Philips/NXP I*Code implementation, PCD side.
 *
 * (C) 2005-2008 by Harald Welte <laforge@gnumonks.org>
 *
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <librfid/rfid.h>
#include <librfid/rfid_protocol.h>
#include <librfid/rfid_layer2.h>
#include <librfid/rfid_layer2_icode1.h>
#include <librfid/rfid_protocol_icode.h>

static int
icode_read(struct rfid_protocol_handle *ph, unsigned int page,
	   unsigned char *rx_data, unsigned int *rx_len)
{
	unsigned char rx_buf[16];
	unsigned int real_rx_len = sizeof(rx_buf);
	unsigned char tx[2];
	int ret;

	/* FIXME */
	return -EINVAL;
}

static int
icode_write(struct rfid_protocol_handle *ph, unsigned int page,
	    unsigned char *tx_data, unsigned int tx_len)
{
	unsigned int i;
	unsigned char tx[6];
	unsigned char rx[10];
	unsigned int rx_len = sizeof(rx);
	int ret;

	return -EINVAL;
}

static int
icode_getopt(struct rfid_protocol_handle *ph, int optname, void *optval,
	    unsigned int *optlen)
{
	int ret = -EINVAL;
	unsigned int *size = optval;

	switch (optname) {
	case RFID_OPT_PROTO_SIZE:
		ret = 0;
		/* we have to return the size in bytes, not bits */
		/* FIXME: implement this */
		*size = 12345;
		break;
	}

	return ret;
}


static struct rfid_protocol_handle *
icode_init(struct rfid_layer2_handle *l2h)
{
	struct rfid_protocol_handle *ph;
	u_int8_t uid[6];
	int uid_len = sizeof(uid);

	if (l2h->l2->id != RFID_LAYER2_ICODE1)
		return NULL;

	/* According to "SCBU002 - November 2005 */
	rfid_layer2_getopt(l2h, RFID_OPT_LAYER2_UID,
			   uid, &uid_len);
	if (uid[5] != 0xe0 || uid[4] != 0x07)
		return NULL;
	if (l2h->uid_len != 6)
		return NULL;

	ph = malloc_protocol_handle(sizeof(struct rfid_protocol_handle));
	return ph;
}

static int icode_fini(struct rfid_protocol_handle *ph)
{
	free_protocol_handle(ph);
	return 0;
}

const struct rfid_protocol rfid_protocol_icode = {
	.id	= RFID_PROTOCOL_ICODE_SLI,
	.name	= "I*Code SLI",
	.fn	= {
		.init 		= &icode_init,
		.read		= &icode_read,
		.write 		= &icode_write,
		.fini		= &icode_fini,
		.getopt		= &icode_getopt,
	},
};

/* Functions below are not (yet? covered in the generic librfid api */
