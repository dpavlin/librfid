
/* Mifare Ultralight implementation, PCD side.
 *
 * (C) 2005-2006 by Harald Welte <laforge@gnumonks.org>
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
#include <librfid/rfid_protocol_mifare_ul.h>

#include "rfid_iso14443_common.h"


/* FIXME */
#define MIFARE_UL_READ_FWT	100
#define MIFARE_UL_WRITE_FWT	100

static int
mful_read(struct rfid_protocol_handle *ph, unsigned int page,
	  unsigned char *rx_data, unsigned int *rx_len)
{
	unsigned char rx_buf[16];
	unsigned int real_rx_len = sizeof(rx_buf);
	unsigned char tx[2];
	int ret;

	if (page > MIFARE_UL_PAGE_MAX)
		return -EINVAL;

	tx[0] = MIFARE_UL_CMD_READ;
	tx[1] = page & 0xff;

	ret = rfid_layer2_transceive(ph->l2h, RFID_14443A_FRAME_REGULAR,
				     tx, sizeof(tx), rx_buf, 
				     &real_rx_len, MIFARE_UL_READ_FWT, 0);

	if (ret < 0)
		return ret;

	if (real_rx_len < *rx_len)
		*rx_len = real_rx_len;

	memcpy(rx_data, rx_buf, *rx_len);

	return ret;
}

static int
mful_write(struct rfid_protocol_handle *ph, unsigned int page,
	   unsigned char *tx_data, unsigned int tx_len)
{
	unsigned int i;
	unsigned char tx[6];
	unsigned char rx[10];
	unsigned int rx_len = sizeof(rx);
	int ret;

	if (tx_len != 4 || page > MIFARE_UL_PAGE_MAX)
		return -EINVAL;

	tx[0] = MIFARE_UL_CMD_WRITE;
	tx[1] = page & 0xff;

	for (i = 0; i < 4; i++)
		tx[2+i] = tx_data[i];

	ret = rfid_layer2_transceive(ph->l2h, RFID_14443A_FRAME_REGULAR,
				     tx, sizeof(tx), rx, &rx_len, 
				     MIFARE_UL_WRITE_FWT, 0);
					
	if (ret < 0)
		return ret;

	if (rx[0] != MIFARE_UL_RESP_ACK)
		return -EIO;

	return ret;
}

static int 
mful_getopt(struct rfid_protocol_handle *ph, int optname, void *optval,
	    unsigned int *optlen)
{
	int ret = -EINVAL;
	unsigned int *size = optval;

	switch (optname) {
	case RFID_OPT_PROTO_SIZE:
		ret = 0;
		/* we have to return the size in bytes, not bits */
		*size = 512/8;
		break;
	}

	return ret;
}


static struct rfid_protocol_handle *
mful_init(struct rfid_layer2_handle *l2h)
{
	struct rfid_protocol_handle *ph;
	u_int8_t atqa[2];
	unsigned int atqa_len = sizeof(atqa);

	if (l2h->l2->id != RFID_LAYER2_ISO14443A)
		return NULL;
	
	/* According to "Type Identification Procedure Rev. 1.3" */
	rfid_layer2_getopt(l2h, RFID_OPT_14443A_ATQA,
			   atqa, &atqa_len);
	if (atqa[0] != 0x44 || atqa[1] != 0x00)
		return NULL;

	/* according to "Functional Specification Rev. 3.0 */
	if (l2h->uid_len != 7)
		return NULL;

	ph = malloc_protocol_handle(sizeof(struct rfid_protocol_handle));
	return ph;
}

static int mful_fini(struct rfid_protocol_handle *ph)
{
	free_protocol_handle(ph);
	return 0;
}

const struct rfid_protocol rfid_protocol_mful = {
	.id	= RFID_PROTOCOL_MIFARE_UL,
	.name	= "Mifare Ultralight",
	.fn	= {
		.init 		= &mful_init,
		.read		= &mful_read,
		.write 		= &mful_write,
		.fini		= &mful_fini,
		.getopt		= &mful_getopt,
	},
};

/* Functions below are not (yet? covered in the generic librfid api */


/* lock a certain page */
int rfid_mful_lock_page(struct rfid_protocol_handle *ph, unsigned int page)
{
	unsigned char buf[4] = { 0x00, 0x00, 0x00, 0x00 };

	if (ph->proto != &rfid_protocol_mful)
		return -EINVAL;

	if (page < 3 || page > 15)
		return -EINVAL;

	if (page > 8)
		buf[2] = (1 << page);
	else
		buf[3] = (1 << (page - 8));

	return mful_write(ph, MIFARE_UL_PAGE_LOCK, buf, sizeof(buf));
}

/* convenience wrapper to lock the otp page */
int rfid_mful_lock_otp(struct rfid_protocol_handle *ph)
{
	return rfid_mful_lock_page(ph, MIFARE_UL_PAGE_OTP);
}
