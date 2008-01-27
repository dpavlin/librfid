/* ISO 15693 anticollision implementation
 *
 * (C) 2005-2008 by Harald Welte <laforge@gnumonks.org>
 * (C) 2007 by Bjoern Riemer <bjoern.riemer@web.de>
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

#define DEBUG_LIBRFID

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <librfid/rfid.h>
#include <librfid/rfid_layer2.h>
#include <librfid/rfid_reader.h>
#include <librfid/rfid_layer2_iso15693.h>

struct iso15693_request_read {
	struct iso15693_request req;
	u_int64_t uid;
	u_int8_t blocknum;
} __attribute__ ((packed));

struct iso15693_request_adressed {
	struct iso15693_request head;
	u_int64_t uid;
} __attribute__ ((packed));

#define ISO15693_BLOCK_SIZE_MAX	(256/8)
#define ISO15693_RESP_SIZE_MAX	(4+ISO15693_BLOCK_SIZE_MAX)

#define TIMEOUT 200

static int iso15693_transceive(struct rfid_layer2_handle *handle,
			       enum rfid_frametype frametype,
			       const unsigned char *tx_buf, unsigned int tx_len,
			       unsigned char *rx_buf, unsigned int *rx_len,
			       u_int64_t timeout, unsigned int flags)
{
	return handle->rh->reader->transceive(handle->rh, frametype, tx_buf,
					tx_len, rx_buf, rx_len, timeout, flags);
}

/* Transmit an anticollission frame */
static int
iso15693_transceive_acf(struct rfid_layer2_handle *handle,
			struct iso15693_anticol_cmd *acf,
			unsigned char uuid[ISO15693_UID_LEN],
			char *bit_of_col)
{
	struct rfid_reader *rdr = handle->rh->reader;
	if (!rdr->iso15693.transceive_ac)
		return -1;
	return rdr->iso15693.transceive_ac(handle->rh, acf, uuid, bit_of_col);
}

#if 0

static int
iso15693_read_block(struct rfid_layer2_handle *handle,
		    u_int8_t blocknr, u_int32_t *data)
{
	int rc;
	struct iso15693_request_read req;
	u_int8_t resp[ISO15693_RESP_SIZE_MAX];

	req.req.flags = 0;
	req.command = ISO15693_CMD_READ_BLOCK_SINGLE;
	memcpy(&req.uid, handle->..., ISO15693_UID_LEN);
	req.blocknum = blocknr;

	/* FIXME: fill CRC if required */

	rc = iso15693_transceive(... &req, ...,  );

	if (rc < 0)
		return rc;

	memcpy(data, resp+1, rc-1); /* FIXME rc-3 in case of CRC */

	return rc-1;
}

static int
iso15693_write_block()
{
	struct iso16593_request_read *rreq;
	u_int32_t buf[sizeof(req)+ISO15693_BLOCK_SIZE_MAX];

	rreq = (struct iso15693_request_read *) req;

	rreq->req.flags = ;
	rreq->req.command = ISO15693_CMD_WRITE_BLOCK_SINGLE;
	memcpy(rreq->uid, handle->, ISO15693_UID_LEN);
	rreq->blocknum = blocknr;
	memcpy(rreq->);

}

static int
iso15693_lock_block()
{
}

#endif

static int
iso15693_anticol(struct rfid_layer2_handle *handle)
{
	int i, ret;
	int rx_len = 0;
	int num_valid = 0;
	struct iso15693_anticol_cmd acf;
	char uuid[ISO15693_UID_LEN];
	char boc;

	char uuid_list[16][ISO15693_UID_LEN];
	int uuid_list_valid[16];

#define MY_NONE 0
#define MY_COLL 1
#define MY_UUID 2

	memset(uuid_list_valid, MY_NONE, 16);
	memset(uuid_list, 0, ISO15693_UID_LEN * 16);

	memset(&acf, 0, sizeof(struct iso15693_anticol_cmd));
	acf.afi = 0;
	acf.flags = RFID_15693_F5_NSLOTS_1 | /* comment out for 16 slots */
		    RFID_15693_F_INV_TABLE_5 |
		    RFID_15693_F_RATE_HIGH;
		    //RFID_15693_F_SUBC_TWO
	acf.mask_len = 0;
	//acf.mask_bits[0] = 3;
	acf.current_slot = 0;

	if (acf.flags & RFID_15693_F5_NSLOTS_1)
		i = 1;
	else
		i = 16;
	for (; i >=1; i--) {
		//acf.current_slot=0;
		ret = iso15693_transceive_acf(handle, &acf, &uuid[0], &boc);
		switch (ret) {
		case -ETIMEDOUT:
			DEBUGP("no answer from vicc in slot %d\n",
				acf.current_slot);
			uuid_list_valid[acf.current_slot] = MY_NONE;
			break;
		case -ECOLLISION:
			DEBUGP("Collision during anticol. slot %d bit %d\n",
				acf.current_slot,boc);
			uuid_list_valid[acf.current_slot] = -boc;
			memcpy(uuid_list[acf.current_slot], uuid, ISO15693_UID_LEN);
			break;
		default:
			if (ret < 0) {
				DEBUGP("ERROR ret: %d, slot %d\n", ret,
					acf.current_slot);
				uuid_list_valid[acf.current_slot] = MY_NONE;
			} else {
				DEBUGP("Slot %d ret: %d UUID: %s\n",
					acf.current_slot, ret,
					rfid_hexdump(uuid, ISO15693_UID_LEN));
				uuid_list_valid[acf.current_slot] = MY_UUID;
				memcpy(&uuid_list[acf.current_slot][0], uuid,
					ISO15693_UID_LEN);
			}
		}
		usleep(1000*200);
	}
	if (acf.flags & RFID_15693_F5_NSLOTS_1)
		i = 1;
	else
		i = 16;

	while (i) {
		if (uuid_list_valid[i] == MY_NONE) {
			DEBUGP("slot[%d]: timeout\n",i);
		} else if (uuid_list_valid[i] == MY_UUID) {
			DEBUGP("slot[%d]: VALID uuid: %s\n", i,
				rfid_hexdump(uuid_list[i], ISO15693_UID_LEN));
			num_valid++;
		} else if (uuid_list_valid[i] < 0) {
			DEBUGP("slot[%d]: collision(%d %d,%d) uuid: %s\n",
				i,uuid_list_valid[i]*-1,
				(uuid_list_valid[i]*-1)/8,
				(uuid_list_valid[i]*-1)%8,
			rfid_hexdump(uuid_list[i], ISO15693_UID_LEN));
		}
		i--;
	}
	if (num_valid == 0)
		return -1;

	return num_valid;
}

static int
iso15693_select(struct rfid_layer2_handle *handle)
{
	struct iso15693_request_adressed tx_req;
	int ret;
	unsigned int rx_len, tx_len;

	struct {
		struct iso15693_response head;
		u_int8_t error;
		unsigned char crc[2];
	} rx_buf;
	rx_len = sizeof(rx_buf);

	tx_req.head.command = ISO15693_CMD_SELECT;
	tx_req.head.flags = RFID_15693_F4_ADDRESS | RFID_15693_F_SUBC_TWO ;
	tx_req.uid = 0xE0070000020C1F18;
	//req.uid = 0x181F0C02000007E0;
	//req.uid = 0xe004010001950837;
	//req.uid = 0x37089501000104e0;
	tx_len = sizeof(tx_req);
	DEBUGP("tx_len=%u", tx_len); DEBUGPC(" rx_len=%u\n",rx_len);
	ret = iso15693_transceive(handle, RFID_15693_FRAME, (u_int8_t*)&tx_req,
				  tx_len, (u_int8_t*)&rx_buf, &rx_len, 50,0);
	DEBUGP("ret: %d, error_flag: %d error: %d\n", ret,
		rx_buf.head.flags&RFID_15693_RF_ERROR, 0);
	return -1;
}

static int
iso15693_getopt(struct rfid_layer2_handle *handle,
		int optname, void *optval, unsigned int *optlen)
{
	switch (optname) {
	case RFID_OPT_15693_MOD_DEPTH:
	case RFID_OPT_15693_VCD_CODING:
	case RFID_OPT_15693_VICC_SUBC:
	case RFID_OPT_15693_VICC_SPEED:
	default:
		return -EINVAL;
		break;
	}
	return 0;
}

static int
iso15693_setopt(struct rfid_layer2_handle *handle, int optname,
	        const void *optval, unsigned int optlen)
{
	switch (optname) {
	case RFID_OPT_15693_MOD_DEPTH:
	case RFID_OPT_15693_VCD_CODING:
	case RFID_OPT_15693_VICC_SUBC:
	case RFID_OPT_15693_VICC_SPEED:
	default:
		return -EINVAL;
		break;
	}
	return 0;
}

static int transceive_inventory(struct rfid_layer2_handle *l2h)
{
	return -1;
}

static struct rfid_layer2_handle *
iso15693_init(struct rfid_reader_handle *rh)
{
	int ret;
	struct rfid_layer2_handle *h = malloc_layer2_handle(sizeof(*h));
	if (!h)
		return NULL;

	h->l2 = &rfid_layer2_iso15693;
	h->rh = rh;
	h->priv.iso15693.state = ISO15693_STATE_NONE;
	ret = h->rh->reader->iso15693.init(h->rh);
	if (ret < 0) {
		free_layer2_handle(h);
		return NULL;
	}

	return h;
}

static int
iso15693_fini(struct rfid_layer2_handle *handle)
{
	free_layer2_handle(handle);
	return 0;
}


const struct rfid_layer2 rfid_layer2_iso15693 = {
	.id	= RFID_LAYER2_ISO15693,
	.name 	= "ISO 15693",
	.fn	= {
		.init 		= &iso15693_init,
		.open 		= &iso15693_anticol,
		//.open		= &iso15693_select,
		//.transceive 	= &iso15693_transceive,
		//.close 		= &iso14443a_hlta,
		.fini 		= &iso15693_fini,
		.setopt		= &iso15693_setopt,
		.getopt		= &iso15693_getopt,
	},
};

