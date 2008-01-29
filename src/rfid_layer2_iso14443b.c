/* ISO 14443-3 B anticollision implementation
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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <librfid/rfid.h>
#include <librfid/rfid_layer2.h>
#include <librfid/rfid_reader.h>
#include <librfid/rfid_layer2_iso14443b.h>
#include <librfid/rfid_protocol.h>

#include "rfid_iso14443_common.h"

#define ATQB_TIMEOUT 	((256*10e6/ISO14443_FREQ_SUBCARRIER)		\
			 +(200*10e6/ISO14443_FREQ_SUBCARRIER))

#undef ATQB_TIMEOUT
#define ATQB_TIMEOUT	1

static inline int
fwi_to_fwt(struct rfid_layer2_handle *h, unsigned int *fwt, unsigned int fwi)
{
	unsigned int multiplier, tmp;

	/* 15 is RFU */
	if (fwi > 14)
		return -1;

	/* According to ISO 14443-3:200(E), Chapter 7.9.4.3, the forumala is
	 * (256 * 16 / fC) * 2^fwi We avoid floating point computations by
	 * shifting everything into the microsecond range.  In integer
	 * calculations 1000000*256*16/13560000 evaluates to 302 (instead of
	 * 302.064897), which provides sufficient precision, IMHO.  The max
	 * result is 302 * 16384 (4947968), which fits well within the 31/32
	 * bit range of an integer */

	multiplier = 1 << fwi;		/* 2 to the power of fwi */

	tmp = (unsigned int) 1000000 * 256 * 16;

	return (tmp / h->rh->ah->asic->fc) * multiplier;
}

static int
parse_atqb(struct rfid_layer2_handle *h, struct iso14443b_atqb *atqb)
{
	int ret;

	if (atqb->fifty != 0x50)
		return -1; 

	if (atqb->protocol_info.fo & 0x01)
		h->priv.iso14443b.flags |= ISO14443B_CID_SUPPORTED;
	if (atqb->protocol_info.fo & 0x02)
		h->priv.iso14443b.flags |= ISO14443B_NAD_SUPPORTED;

	ret = fwi_to_fwt(h, &h->priv.iso14443b.fwt, atqb->protocol_info.fwi);
	if (ret < 0) {
		DEBUGP("invalid fwi %u\n", atqb->protocol_info.fwi);
		return ret;
	}

	if (atqb->protocol_info.protocol_type == 0x1) {
		DEBUGP("we have a T=CL compliant PICC\n");
		h->priv.iso14443b.tcl_capable = 1;
		h->proto_supported = (1 << RFID_PROTOCOL_TCL);
	} else {
		DEBUGP("we have a T!=CL PICC\n");
		h->priv.iso14443b.tcl_capable = 0;
		/* FIXME: what protocols do we support? */
	}

	iso14443_fsdi_to_fsd(&h->priv.iso14443b.fsc, 
			     atqb->protocol_info.max_frame_size);

	/* FIXME: speed capability */

	memcpy(h->uid, atqb->pupi, sizeof(atqb->pupi));
	h->uid_len = sizeof(atqb->pupi);

	return 0;
}

static int
send_reqb(struct rfid_layer2_handle *h, unsigned char afi,
	  unsigned int is_wup, unsigned int num_initial_slots)
{
	int ret;
	unsigned char reqb[3];
	struct iso14443b_atqb atqb;
	unsigned int atqb_len = sizeof(atqb);
	unsigned int num_slot_idx = num_initial_slots;

	reqb[0] = 0x05;
	reqb[1] = afi;

	for (num_slot_idx = num_initial_slots; num_slot_idx <= 4;
	     num_slot_idx++) {
		reqb[2] = num_slot_idx & 0x07;
		if (is_wup)
			reqb[2] |= 0x08;

		ret = h->rh->reader->transceive(h->rh, RFID_14443B_FRAME_REGULAR,
						reqb, sizeof(reqb),
						 (unsigned char *)&atqb, 
						 &atqb_len, ATQB_TIMEOUT, 0);
		h->priv.iso14443b.state = ISO14443B_STATE_REQB_SENT;
		if (ret < 0) {
			DEBUGP("error during transceive of REQB/WUBP\n");
			continue;
		}

		/* FIXME: send N-1 slot marker frames */
	
		if (atqb_len != sizeof(atqb)) {
			DEBUGP("error: atqb_len = %u instead of %Zu\n",
				atqb_len, sizeof(atqb));
			continue;
		}

		/* FIXME: how to detect a collission at 14443B ?  I guess we
		 * can only rely on the CRC checking (CRCErr in ErrorFlag
		 * register?) */

		if (parse_atqb(h, &atqb) >= 0) {
			h->priv.iso14443b.state = ISO14443B_STATE_ATQB_RCVD;
			return 0;
		}
	}

	return -1;
}

static inline unsigned int mbli_to_mbl(struct rfid_layer2_handle *h,
					unsigned int mbli)
{
	return (h->priv.iso14443b.fsc * 2 ^ (mbli-1));
}

static int
transceive_attrib(struct rfid_layer2_handle *h, const unsigned char *inf,
	    unsigned int inf_len, unsigned char *rx_data, unsigned int *rx_len)
{
	struct {
		struct iso14443b_attrib_hdr attrib;
		char buf[256-3];
	} _attrib_buf;

	struct iso14443b_attrib_hdr *attrib = &_attrib_buf.attrib;
	unsigned char rx_buf[256];
	unsigned char fsdi;
	int ret = 0;
	
	DEBUGP("fsd is %u\n", h->priv.iso14443b.fsd);
	if (rx_len >= rx_len-1)
		return -EINVAL;

	/* initialize attrib frame */
	memset(&_attrib_buf, 0, sizeof(_attrib_buf));
	if (inf_len)
		memcpy((unsigned char *)attrib+sizeof(*attrib), inf, inf_len);

	attrib->one_d = 0x1d;
	memcpy(attrib->identifier, h->uid, 4);

	/* FIXME: do we want to change TR0/TR1 from its default ? */
	/* FIXME: do we want to change SOF/EOF from its default ? */

	ret = iso14443_fsd_to_fsdi(&fsdi, h->priv.iso14443b.fsd);
	if (ret < 0) {
		DEBUGP("unable to map FSD(%u) to FSDI\n",
			h->priv.iso14443b.fsd);
		goto out_rx;
	}
	attrib->param2.fsdi = fsdi;

	/* FIXME: spd_in / spd_out */
	if (h->priv.iso14443b.tcl_capable == 1)
		attrib->param3.protocol_type = 0x1;

	attrib->param4.cid = h->priv.iso14443b.cid & 0xf;

	*rx_len = *rx_len + 1;
	ret = h->rh->reader->transceive(h->rh, RFID_14443B_FRAME_REGULAR,
					(unsigned char *) attrib,
					sizeof(*attrib)+inf_len,
					rx_buf, rx_len, h->priv.iso14443b.fwt,
					0);
	h->priv.iso14443b.state = ISO14443B_STATE_ATTRIB_SENT;
	if (ret < 0) {
		DEBUGP("transceive problem\n");
		goto out_rx;
	}

	if ((rx_buf[0] & 0x0f) != h->priv.iso14443b.cid) {
		DEBUGP("ATTRIB response with invalid CID %u (should be %u)\n",
			rx_buf[0] & 0x0f, h->priv.iso14443b.cid);
		ret = -1;
		goto out_rx;
	}

	h->priv.iso14443b.state = ISO14443B_STATE_SELECTED;
	
	h->priv.iso14443b.mbl = mbli_to_mbl(h, (rx_data[0] & 0xf0) >> 4);

	*rx_len = *rx_len - 1;
	memcpy(rx_data, rx_buf+1, *rx_len);

out_rx:
out_attrib:

	return ret;
}

static int
iso14443b_hltb(struct rfid_layer2_handle *h)
{
	int ret;
	unsigned char hltb[5];
	unsigned char hltb_resp[1];
	unsigned int hltb_len = 1;

	hltb[0] = 0x50;
	memcpy(hltb+1, h->uid, 4);

	ret = h->rh->reader->transceive(h->rh, RFID_14443B_FRAME_REGULAR,
					hltb, 5,
					hltb_resp, &hltb_len,
					h->priv.iso14443b.fwt, 0);
	h->priv.iso14443b.state = ISO14443B_STATE_HLTB_SENT;
	if (ret < 0) {
		DEBUGP("transceive problem\n");
		return ret;
	}

	if (hltb_len != 1 || hltb_resp[0] != 0x00) {
		DEBUGP("bad HLTB response\n");
		return -1;
	}
	h->priv.iso14443b.state = ISO14443B_STATE_HALTED;
						
	return 0;
}

static int
iso14443b_anticol(struct rfid_layer2_handle *handle)
{
	unsigned char afi = 0; /* FIXME */
	int ret;
	unsigned char buf[255];
	unsigned int buf_len = sizeof(buf);

	ret = send_reqb(handle, afi, 0, 0);
	if (ret < 0)
		return ret;

	ret = transceive_attrib(handle, NULL, 0, buf, &buf_len);
	if (ret < 0)
		return ret;

	return 0;
}

static struct rfid_layer2_handle *
iso14443b_init(struct rfid_reader_handle *rh)
{
	int ret;
	struct rfid_layer2_handle *h = malloc_layer2_handle(sizeof(*h));
	if (!h)
		return NULL;

	h->l2 = &rfid_layer2_iso14443b;
	h->rh = rh;
	h->priv.iso14443b.state = ISO14443B_STATE_NONE;

	/* FIXME: if we want to support multiple PICC's, we need some
	 * fancy allocation scheme for CID's */
	h->priv.iso14443b.cid = 0;

	h->priv.iso14443b.fsd = iso14443_fsd_approx(rh->ah->mru);
	DEBUGP("fsd is %u\n", h->priv.iso14443b.fsd);

	/* 14443-3 Section 7.1.6 */
	h->priv.iso14443b.tr0 = (256/ISO14443_FREQ_SUBCARRIER)*10e6;
	h->priv.iso14443b.tr1 = (200/ISO14443_FREQ_SUBCARRIER)*10e6;

	ret = h->rh->reader->init(h->rh, RFID_LAYER2_ISO14443B);
	if (ret < 0) {
		DEBUGP("error during reader 14443b init\n");
		free_layer2_handle(h);
		return NULL;
	}

	return h;
}

static int
iso14443b_fini(struct rfid_layer2_handle *handle)
{
	free_layer2_handle(handle);
	return 0;
}

static int
iso14443b_transceive(struct rfid_layer2_handle *handle,
		     enum rfid_frametype frametype,
		     const unsigned char *tx_buf, unsigned int tx_len,
		     unsigned char *rx_buf, unsigned int *rx_len,
		     u_int64_t timeout, unsigned int flags)
{
	DEBUGP("transcieving %u bytes, expecting max %u\n", tx_len, *rx_len);
	return handle->rh->reader->transceive(handle->rh, frametype,
					      tx_buf, tx_len,
					      rx_buf, rx_len, timeout, flags);
}

static int
iso14443b_getopt(struct rfid_layer2_handle *handle,
		 int optname, void *optval, unsigned int *optlen)
{
	unsigned int *opt_ui = optval;

	switch (optname) {
	case RFID_OPT_14443B_CID:
		*opt_ui = handle->priv.iso14443b.cid;
		break;
	case RFID_OPT_14443B_FSC:
		*opt_ui = handle->priv.iso14443b.fsc;
		break;
	case RFID_OPT_14443B_FSD:
		*opt_ui = handle->priv.iso14443b.fsd;
		break;
	case RFID_OPT_14443B_FWT:
		*opt_ui = handle->priv.iso14443b.fwt;
		break;
	case RFID_OPT_14443B_TR0:
		*opt_ui = handle->priv.iso14443b.tr0;
		break;
	case RFID_OPT_14443B_TR1:
		*opt_ui = handle->priv.iso14443b.tr1;
		break;
	default:
		return -EINVAL;
		break;
	}
	return 0;
}

static int
iso14443b_setopt(struct rfid_layer2_handle *handle,
		 int optname, const void *optval, unsigned int optlen)
{
	const unsigned int *opt_ui = optval;

	switch (optname) {
	case RFID_OPT_14443B_CID:
		handle->priv.iso14443b.cid = (*opt_ui & 0xf);
		break;
	defaukt:
		return -EINVAL;
		break;
	}
	return 0;
}


const struct rfid_layer2 rfid_layer2_iso14443b = {
	.id	= RFID_LAYER2_ISO14443B,
	.name 	= "ISO 14443-3 B",
	.fn	= {
		.init 		= &iso14443b_init,
		.open 		= &iso14443b_anticol,
		.transceive 	= &iso14443b_transceive,
		.close 		= &iso14443b_hltb,
		.fini 		= &iso14443b_fini,
		.getopt		= &iso14443b_getopt,
		.setopt		= &iso14443b_setopt,
	},
};
