
/* Mifare Classic implementation, PCD side.
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
#include <librfid/rfid_protocol_mifare_classic.h>

#include <librfid/rfid_reader.h>

#include "rfid_iso14443_common.h"


#define MIFARE_UL_CMD_WRITE	0xA2
#define MIFARE_UL_CMD_READ	0x30

#define MIFARE_CL_READ_FWT	250
#define MIFARE_CL_WRITE_FWT	600

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
	unsigned char tx[2];
	unsigned char rx[1];
	unsigned int rx_len = sizeof(rx);
	int ret;

	if (page > MIFARE_CL_PAGE_MAX)
		return -EINVAL;

	if (tx_len != 16)
		return -EINVAL;
	
	tx[0] = MIFARE_CL_CMD_WRITE16;
	tx[1] = page & 0xff;

	ret = rfid_layer2_transceive(ph->l2h, RFID_MIFARE_FRAME, tx, 2, rx,
				     &rx_len, MIFARE_CL_WRITE_FWT, 0);
	if (ret < 0)
		return ret;

	ret = rfid_layer2_transceive(ph->l2h, RFID_MIFARE_FRAME, tx_data,
				     tx_len, rx, &rx_len,
				     MIFARE_CL_WRITE_FWT, 0);
	if (ret < 0)
		return ret;

	if (rx[0] != MIFARE_UL_RESP_ACK)
		return -EIO;

	return ret;
}

static int 
mfcl_getopt(struct rfid_protocol_handle *ph, int optname, void *optval,
	    unsigned int *optlen)
{
	int ret = -EINVAL;
	u_int8_t atqa[2];
	u_int8_t sak;
	unsigned int atqa_size = sizeof(atqa);
	unsigned int sak_size = sizeof(sak);
	unsigned int *size = optval;

	switch (optname) {
	case RFID_OPT_PROTO_SIZE:
		if (*optlen < sizeof(*size))
			return -EINVAL;
		*optlen = sizeof(*size);
		ret = 0;
		rfid_layer2_getopt(ph->l2h, RFID_OPT_14443A_ATQA,
				   atqa, &atqa_size);
		rfid_layer2_getopt(ph->l2h, RFID_OPT_14443A_SAK,
				   &sak, &sak_size);
		if (atqa[0] == 0x04 && atqa[1] == 0x00) {
			if (sak == 0x09) {
				/* mifare mini */
				*size = 320;
			} else
				*size = 1024;
		} else if (atqa[0] == 0x02 && atqa[1] == 0x00)
			*size = 4096;
		else
			ret = -EIO;
		break;
	}

	return ret;
}

static struct rfid_protocol_handle *
mfcl_init(struct rfid_layer2_handle *l2h)
{
	struct rfid_protocol_handle *ph;

	if (l2h->l2->id != RFID_LAYER2_ISO14443A)
		return NULL;

	if (l2h->uid_len != 4)
		return NULL;

	ph = malloc_protocol_handle(sizeof(struct rfid_protocol_handle));
	return ph;
}

static int mfcl_fini(struct rfid_protocol_handle *ph)
{
	free_protocol_handle(ph);
	return 0;
}

const struct rfid_protocol rfid_protocol_mfcl = {
	.id	= RFID_PROTOCOL_MIFARE_CLASSIC,
	.name	= "Mifare Classic",
	.fn	= {
		.init 		= &mfcl_init,
		.read		= &mfcl_read,
		.write 		= &mfcl_write,
		.fini		= &mfcl_fini,
		.getopt		= &mfcl_getopt,
	},
};

int mfcl_set_key(struct rfid_protocol_handle *ph, unsigned char *key)
{
	if (!ph->l2h->rh->reader->mifare_classic.setkey)
		return -ENODEV;

	return ph->l2h->rh->reader->mifare_classic.setkey(ph->l2h->rh, key);
}

int mfcl_set_key_ee(struct rfid_protocol_handle *ph, unsigned int addr)
{
	if (!ph->l2h->rh->reader->mifare_classic.setkey_ee)
		return -ENODEV;

	return ph->l2h->rh->reader->mifare_classic.setkey_ee(ph->l2h->rh, addr);
}

int mfcl_auth(struct rfid_protocol_handle *ph, u_int8_t cmd, u_int8_t block)
{
	u_int32_t serno = *((u_int32_t *)ph->l2h->uid);

	if (!ph->l2h->rh->reader->mifare_classic.auth)
		return -ENODEV;

	return ph->l2h->rh->reader->mifare_classic.auth(ph->l2h->rh, cmd,
						       serno, block);
}

int mfcl_block2sector(u_int8_t block)
{
	if (block < MIFARE_CL_SMALL_SECTORS * MIFARE_CL_BLOCKS_P_SECTOR_1k)
		return block/MIFARE_CL_BLOCKS_P_SECTOR_1k;
	else
		return (block - MIFARE_CL_SMALL_SECTORS * MIFARE_CL_BLOCKS_P_SECTOR_1k)
					/ MIFARE_CL_BLOCKS_P_SECTOR_4k;
}

int mfcl_sector2block(u_int8_t sector)
{
	if (sector < MIFARE_CL_SMALL_SECTORS)
		return sector * MIFARE_CL_BLOCKS_P_SECTOR_1k;
	else if (sector < MIFARE_CL_SMALL_SECTORS + MIFARE_CL_LARGE_SECTORS)
		return MIFARE_CL_SMALL_SECTORS * MIFARE_CL_BLOCKS_P_SECTOR_1k + 
			(sector - MIFARE_CL_SMALL_SECTORS) * MIFARE_CL_BLOCKS_P_SECTOR_4k; 
	else
		return -EINVAL;
}

int mfcl_sector_blocks(u_int8_t sector)
{
	if (sector < MIFARE_CL_SMALL_SECTORS)
		return MIFARE_CL_BLOCKS_P_SECTOR_1k;
	else if (sector < MIFARE_CL_SMALL_SECTORS + MIFARE_CL_LARGE_SECTORS)
		return MIFARE_CL_BLOCKS_P_SECTOR_4k;
	else
		return -EINVAL;
}
