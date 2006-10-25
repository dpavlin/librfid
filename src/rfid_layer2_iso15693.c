/* ISO 15693 anticollision implementation
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <librfid/rfid.h>
#include <librfid/rfid_layer2.h>
#include <librfid/rfid_reader.h>
#include <librfid/rfid_layer2_iso15693.h>

struct iso15693_request_read {
	struct iso15693_request req;
	u_int64_t uid;
	u_int8_t blocknum;
} __attribute__ ((packed));

#define ISO15693_BLOCK_SIZE_MAX	(256/8)
#define ISO15693_RESP_SIZE_MAX	(4+ISO15693_BLOCK_SIZE_MAX)

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
		//.transceive 	= &iso15693_transceive,
		//.close 		= &iso14443a_hlta,
		.fini 		= &iso15693_fini,
	},
};

