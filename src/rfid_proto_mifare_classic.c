
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

#include <librfid/rfid.h>
#include <librfid/rfid_protocol.h>
#include <librfid/rfid_layer2.h>
#include <librfid/rfid_protocol_mifare_classic.h>

#include <librfid/rfid_reader.h>

#include "rfid_iso14443_common.h"


#define MIFARE_UL_CMD_WRITE	0xA2
#define MIFARE_UL_CMD_READ	0x30

/* FIXME */
#define MIFARE_CL_READ_FWT	100
#define MIFARE_CL_WRITE_FWT	100

static int
mfcl_read(struct rfid_protocol_handle *ph, unsigned int page,
	  unsigned char *rx_data, unsigned int *rx_len)
{
	unsigned char rx_buf[16];
	unsigned int real_rx_len = sizeof(rx_buf);
	unsigned char tx[2];
	int ret;

	if (page > MIFARE_CL_PAGE_MAX)
		return -EINVAL;

	tx[0] = MIFARE_CL_CMD_READ;
	tx[1] = page & 0xff;

	ret = rfid_layer2_transceive(ph->l2h, RFID_MIFARE_FRAME, tx,
				     sizeof(tx), rx_buf, &real_rx_len,
				     MIFARE_CL_READ_FWT, 0);

	if (ret < 0)
		return ret;

	if (real_rx_len == 1 && *rx_buf == 0x04)
		return -EPERM;

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
	unsigned char tx[18];
	unsigned char rx[1];
	unsigned int rx_len;
	int ret;

	if (tx_len != 16 || page > MIFARE_CL_PAGE_MAX)
		return -EINVAL;

	tx[0] = MIFARE_CL_CMD_WRITE16;
	tx[1] = page & 0xff;

	memcpy(tx+2, tx_data, 16);

	ret = rfid_layer2_transceive(ph->l2h, RFID_MIFARE_FRAME, tx,
				     sizeof(tx), rx, &rx_len, 
				     MIFARE_CL_WRITE_FWT, 0);
					
	if (ret < 0)
		return ret;

	if (rx[0] != MIFARE_UL_RESP_ACK)
		return -EIO;

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
