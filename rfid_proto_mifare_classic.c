
/* Mifare Classic implementation, PCD side.
 *
 * (C) 2005 by Harald Welte <laforge@gnumonks.org>
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <rfid/rfid.h>
#include <rfid/rfid_protocol.h>
#include <rfid/rfid_layer2.h>
//#include <rfid/rfid_layer2_iso14443b.h>

//#include <rfid/rfid_asic.h>
#include <rfid/rfid_reader.h>

#include "rfid_iso14443_common.h"


#define MIFARE_UL_CMD_WRITE	0xA2
#define MIFARE_UL_CMD_READ	0x30

/* FIXME */
#define MIFARE_UL_READ_FWT	100
#define MIFARE_UL_WRITE_FWT	100

static int
mfcl_read(struct rfid_protocol_handle *ph, unsigned int page,
	  unsigned char *rx_data, unsigned int *rx_len)
{
	unsigned char rx_buf[16];
	unsigned int real_rx_len = sizeof(rx_buf);
	unsigned char tx[2];
	int ret;

	if (page > 7)
		return -EINVAL;

	tx[0] = MIFARE_UL_CMD_READ;
	tx[1] = page & 0xff;

	ret = ph->l2h->l2->fn.transcieve(ph->l2h, tx, sizeof(tx), rx_buf,
					 &real_rx_len, MIFARE_UL_READ_FWT, 0);

	if (ret < 0)
		return ret;

	if (real_rx_len < *rx_len)
		*rx_len = real_rx_len;

	memcpy(rx_data, rx_buf, *rx_len);

	return ret;
}

static int
mfcl_write(struct rfid_protocol_handle *ph, unsigned int page,
	   unsigned char *tx_data, unsigned int tx_len)
{
	unsigned int i;
	unsigned char tx[6];
	unsigned char rx[1];
	unsigned int rx_len;
	int ret;

	if (tx_len != 4 || page > 7)
		return -EINVAL;

	tx[0] = MIFARE_UL_CMD_WRITE;
	tx[1] = page & 0xff;

	for (i = 0; i < 4; i++)
		tx[2+i] = tx_data[i];

	ret = ph->l2h->l2->fn.transcieve(ph->l2h, tx, sizeof(tx), rx,
					 &rx_len, MIFARE_UL_WRITE_FWT, 0);
					
	/* FIXME:look at RX, check for ACK/NAK */

	return ret;
}

static struct rfid_protocol_handle *
mfcl_init(struct rfid_layer2_handle *l2h)
{
	struct rfid_protocol_handle *ph;
	ph = malloc(sizeof(struct rfid_protocol_handle));
	return ph;
}

static int mfcl_fini(struct rfid_protocol_handle *ph)
{
	free(ph);
	return 0;
}

struct rfid_protocol rfid_protocol_mfcl = {
	.id	= RFID_PROTOCOL_MIFARE_CLASSIC,
	.name	= "Mifare Classic",
	.fn	= {
		.init 		= &mfcl_init,
		.read		= &mfcl_read,
		.write 		= &mfcl_write,
		.fini		= &mfcl_fini,
	},
};

int mfcl_set_key(struct rfid_protocol_handle *ph, unsigned char *key)
{
	if (!ph->l2h->rh->reader->mifare_classic.setkey)
		return -ENODEV;

	return ph->l2h->rh->reader->mifare_classic.setkey(ph->l2h->rh, key);
}

int mfcl_auth(struct rfid_protocol_handle *ph, u_int8_t cmd, u_int8_t block)
{
	u_int32_t serno = *((u_int32_t *)ph->l2h->uid);

	if (!ph->l2h->rh->reader->mifare_classic.auth)
		return -ENODEV;

	return ph->l2h->rh->reader->mifare_classic.auth(ph->l2h->rh, cmd,
						       serno, block);
}
