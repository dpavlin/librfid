/* ISO 14443-4 (T=CL) implementation, PCD side.
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
#include <librfid/rfid_protocol_tcl.h>
#include <librfid/rfid_protocol.h>
#include <librfid/rfid_layer2.h>
#include <librfid/rfid_layer2_iso14443b.h>

#include <librfid/rfid_asic.h>
#include <librfid/rfid_reader.h>

#include "rfid_iso14443_common.h"

#define RFID_MAX_FRAMELEN	256

#define is_s_block(x) ((x & 0xc0) == 0xc0)
#define is_r_block(x) ((x & 0xc0) == 0x80)
#define is_i_block(x) ((x & 0xc0) == 0x00)

static enum rfid_frametype l2_to_frame(unsigned int layer2)
{
	switch (layer2) {
		case RFID_LAYER2_ISO14443A:
			return RFID_14443A_FRAME_REGULAR;
			break;
		case RFID_LAYER2_ISO14443B:
			return RFID_14443B_FRAME_REGULAR;
			break;
	}
	return 0;
}

static unsigned int sfgi_to_sfgt(struct rfid_protocol_handle *h, 
				 unsigned char sfgi)
{
	unsigned int multiplier;
	unsigned int tmp;

	if (sfgi > 14)
		sfgi = 14;

	multiplier = 1 << sfgi; /* 2 to the power of sfgi */

	/* ISO 14443-4:2000(E) Section 5.2.5:
	 * (256 * 16 / h->l2h->rh->ah->fc) * (2 ^ sfgi) */
	tmp = (unsigned int) 1000000 * 256 * 16;

	return (tmp / h->l2h->rh->ah->fc) * multiplier;
}

static unsigned int fwi_to_fwt(struct rfid_protocol_handle *h, 
				unsigned char fwi)
{
	unsigned int multiplier, tmp;

	if (fwi > 14)
		fwi = 14;

	multiplier  = 1 << fwi; /* 2 to the power of fwi */

	/* ISO 14443-4:2000(E) Section 7.2.:
	 * (256*16 / h->l2h->rh->ah->fc) * (2 ^ fwi) */

	tmp = (unsigned int) 1000000 * 256 * 16;

	return (tmp / h->l2h->rh->ah->fc) * multiplier;
}

/* 4.9seconds as microseconds (4.9 billion seconds) exceeds 2^32 */
#define activation_fwt(x) (((u_int64_t)1000000 * 65536 / x->l2h->rh->ah->fc))
#define deactivation_fwt(x) activation_fwt(x)

static int
tcl_parse_ats(struct rfid_protocol_handle *h, 
		unsigned char *ats, unsigned int size)
{
	unsigned char len = ats[0];
	unsigned char t0;
	unsigned char *cur;

	if (len == 0 || size == 0) 
		return -1;

	if (size < len)
		len = size;

	h->priv.tcl.ta = 0;

	if (len == 1) {
		/* FIXME: assume some default values */
		h->priv.tcl.fsc = 32;
		h->priv.tcl.ta = 0x80;	/* 0x80 (same d for both dirs) */
		h->priv.tcl.sfgt = sfgi_to_sfgt(h, 0);
		if (h->l2h->l2->id == RFID_LAYER2_ISO14443A) {
			/* Section 7.2: fwi default for type A is 4 */
			h->priv.tcl.fwt = fwi_to_fwt(h, 4);
		} else {
			/* Section 7.2: fwi for type B is always in ATQB */
			/* Value is assigned in tcl_connect() */
			/* This function is never called for Type B, 
			 * since Type B has no (R)ATS */
		}
		return 0;
	}

	/* guarateed to be at least 2 bytes in size */

	t0 = ats[1];
	cur = &ats[2];

	iso14443_fsdi_to_fsd(&h->priv.tcl.fsc, t0 & 0x0f);
	if (h->priv.tcl.fsc > h->l2h->rh->ah->mtu)
		h->priv.tcl.fsc = h->l2h->rh->ah->mtu;

	if (t0 & (1 << 4)) {
		/* TA is transmitted */
		h->priv.tcl.ta = *cur++;
	}

	if (t0 & (1 << 5)) {
		/* TB is transmitted */
		h->priv.tcl.sfgt = sfgi_to_sfgt(h, *cur & 0x0f);
		h->priv.tcl.fwt = fwi_to_fwt(h, (*cur & 0xf0) >> 4);
		cur++;
	}

	if (t0 & (1 << 6)) {
		/* TC is transmitted */
		if (*cur & 0x01) {
			h->priv.tcl.flags |= TCL_HANDLE_F_NAD_SUPPORTED;
			DEBUGP("This PICC supports NAD\n");
		}
		if (*cur & 0x02) {
			h->priv.tcl.flags |= TCL_HANDLE_F_CID_SUPPORTED;
			DEBUGP("This PICC supports CID\n");
		}
		cur++;
	}

	h->priv.tcl.historical_len = (ats+len) - cur;
	h->priv.tcl.historical_bytes = cur;
	
	DEBUGP("ATS parsed: %s\n", rfid_hexdump(ats, size));

	return 0;
}


/* request an ATS from the PICC */
static int
tcl_request_ats(struct rfid_protocol_handle *h)
{
	int ret;
	unsigned char rats[2];
	unsigned char fsdi;

	if (h->priv.tcl.state != TCL_STATE_INITIAL)
		return -1;

	ret = iso14443_fsd_to_fsdi(&fsdi, h->priv.tcl.fsd);
	if (ret < 0) {
		DEBUGP("unable to encode FSD of %u as FSDI\n", h->priv.tcl.fsd);
		return ret;
	}

	rats[0] = 0xe0;
	rats[1] = (h->priv.tcl.cid & 0x0f) | ((fsdi << 4) & 0xf0);

	/* transceive (with CRC) */
	ret = rfid_layer2_transceive(h->l2h, RFID_14443A_FRAME_REGULAR,
				     rats, 2, h->priv.tcl.ats,
				     &h->priv.tcl.ats_len, activation_fwt(h),
				     TCL_TRANSP_F_TX_CRC);
	if (ret < 0) {
		DEBUGP("transceive of rats failed\n");
		h->priv.tcl.state = TCL_STATE_RATS_SENT;
		/* FIXME: retransmit */
		return ret;
	}
	h->priv.tcl.state = TCL_STATE_ATS_RCVD;

	ret = tcl_parse_ats(h, h->priv.tcl.ats, h->priv.tcl.ats_len);
	if (ret < 0) {
		DEBUGP("parsing of ats failed\n");
		return ret;
	}

	return 0;
}

#define ATS_TA_DIV_2	1
#define ATS_TA_DIV_4	2
#define ATS_TA_DIV_8	4

#define PPS_DIV_8	3
#define PPS_DIV_4	2
#define PPS_DIV_2	1
#define PPS_DIV_1	0
static unsigned char d_to_di(struct rfid_protocol_handle *h, unsigned char D)
{
	static char DI;
	unsigned int speed = h->l2h->rh->reader->iso14443a.speed;
	
	if ((D & ATS_TA_DIV_8) && (speed & RFID_14443A_SPEED_848K))
		DI = PPS_DIV_8;
	else if ((D & ATS_TA_DIV_4) && (speed & RFID_14443A_SPEED_424K))
		DI = PPS_DIV_4;
	else if ((D & ATS_TA_DIV_2) && (speed & RFID_14443A_SPEED_212K))
		DI = PPS_DIV_2;
	else
		DI = PPS_DIV_1;

	return DI;
}

static unsigned int di_to_speed(unsigned char DI)
{
	switch (DI) {
	case PPS_DIV_8:
		return RFID_14443A_SPEED_848K;
		break;
	case PPS_DIV_4:
		return RFID_14443A_SPEED_424K;
		break;
	case PPS_DIV_2:
		return RFID_14443A_SPEED_212K;
		break;
	case PPS_DIV_1:
		return RFID_14443A_SPEED_106K;
		break;
	}
}

/* start a PPS run (autimatically configure highest possible speed */
static int 
tcl_do_pps(struct rfid_protocol_handle *h)
{
	int ret;
	unsigned char ppss[3];
	/* FIXME: this stinks like hell. IF we reduce pps_response size to one,
	   we'll get stack corruption! */
	unsigned char pps_response[10];
	unsigned int rx_len = 1;
	unsigned char Dr, Ds, DrI, DsI;
	unsigned int speed;

	if (h->priv.tcl.state != TCL_STATE_ATS_RCVD)
		return -1;

	Dr = h->priv.tcl.ta & 0x07;
	Ds = h->priv.tcl.ta & 0x70 >> 4;
	DEBUGP("Dr = 0x%x, Ds = 0x%x\n", Dr, Ds);

	if (Dr != Ds && !(h->priv.tcl.ta & 0x80)) {
		/* device supports different divisors for rx and tx, but not
		 * really ?!? */
		DEBUGP("PICC has contradictory TA, aborting PPS\n");
		return -1;
	};

	/* ISO 14443-4:2000(E) Section 5.3. */

	ppss[0] = 0xd0 | (h->priv.tcl.cid & 0x0f);
	ppss[1] = 0x11;
	ppss[2] = 0x00;

	/* FIXME: deal with different speed for each direction */
	DrI = d_to_di(h, Dr);
	DsI = d_to_di(h, Ds);
	DEBUGP("DrI = 0x%x, DsI = 0x%x\n", DrI, DsI);

	ppss[2] = (ppss[2] & 0xf0) | (DrI | DsI << 2);

	ret = rfid_layer2_transceive(h->l2h, RFID_14443A_FRAME_REGULAR,
					ppss, 3, pps_response, &rx_len,
					h->priv.tcl.fwt, TCL_TRANSP_F_TX_CRC);
	if (ret < 0)
		return ret;

	if (pps_response[0] != ppss[0]) {
		DEBUGP("PPS Response != PPSS\n");
		return -1;
	}

	speed = di_to_speed(DrI);

	ret = rfid_layer2_setopt(h->l2h, RFID_OPT_14443A_SPEED_RX,
				 &speed, sizeof(speed));
	if (ret < 0)
		return ret;

	ret = rfid_layer2_setopt(h->l2h, RFID_OPT_14443A_SPEED_TX,
				 &speed, sizeof(speed));
	if (ret < 0)
		return ret;
	
	return 0;
}


static int
tcl_build_prologue2(struct tcl_handle *th, 
		    unsigned char *prlg, unsigned int *prlg_len, 
		    unsigned char pcb)
{
	*prlg_len = 1;

	*prlg = pcb;

	if (!is_s_block(pcb)) {
		if (th->toggle) {
			/* we've sent a toggle bit last time */
			th->toggle = 0;
		} else {
			/* we've not sent a toggle last time: send one */
			th->toggle = 1;
			*prlg |= 0x01;
		}
	}

	if (th->flags & TCL_HANDLE_F_CID_USED) {
		/* ISO 14443-4:2000(E) Section 7.1.1.2 */
		*prlg |= TCL_PCB_CID_FOLLOWING;
		(*prlg_len)++;
		prlg[*prlg_len] = th->cid & 0x0f;
	}

	/* nad only for I-block */
	if ((th->flags & TCL_HANDLE_F_NAD_USED) && is_i_block(pcb)) {
		/* ISO 14443-4:2000(E) Section 7.1.1.3 */
		/* FIXME: in case of chaining only for first frame */
		*prlg |= TCL_PCB_NAD_FOLLOWING;
		prlg[*prlg_len] = th->nad;
		(*prlg_len)++;
	}

	return 0;
}

static int
tcl_build_prologue_i(struct tcl_handle *th,
		     unsigned char *prlg, unsigned int *prlg_len)
{
	/* ISO 14443-4:2000(E) Section 7.1.1.1 */
	return tcl_build_prologue2(th, prlg, prlg_len, 0x02);
}

static int
tcl_build_prologue_r(struct tcl_handle *th,
		     unsigned char *prlg, unsigned int *prlg_len,
		     unsigned int nak)
{
	unsigned char pcb = 0xa2;
	/* ISO 14443-4:2000(E) Section 7.1.1.1 */

	if (nak)
		pcb |= 0x10;

	return tcl_build_prologue2(th, prlg, prlg_len, pcb);
}

static int
tcl_build_prologue_s(struct tcl_handle *th,
		     unsigned char *prlg, unsigned int *prlg_len)
{
	/* ISO 14443-4:2000(E) Section 7.1.1.1 */

	/* the only S-block from PCD->PICC is DESELECT,
	 * well, actually there is the S(WTX) response. */
	return tcl_build_prologue2(th, prlg, prlg_len, 0xc2);
}

/* FIXME: WTXM implementation */

static int tcl_prlg_len(struct tcl_handle *th)
{
	int prlg_len = 1;

	if (th->flags & TCL_HANDLE_F_CID_USED)
		prlg_len++;

	if (th->flags & TCL_HANDLE_F_NAD_USED)
		prlg_len++;

	return prlg_len;
}

#define max_net_tx_framesize(x)	(x->fsc - tcl_prlg_len(x))

static int
tcl_connect(struct rfid_protocol_handle *h)
{
	int ret; 

	if (h->priv.tcl.state != TCL_STATE_DESELECTED &&
	    h->priv.tcl.state != TCL_STATE_INITIAL)
		return -1;

	switch (h->l2h->l2->id) {
	case RFID_LAYER2_ISO14443A:
		/* Start Type A T=CL Activation Sequence */
		ret = tcl_request_ats(h);
		if (ret < 0)
			return ret;

		/* Only do PPS if any non-default divisors supported */
		if (h->priv.tcl.ta & 0x77) {
			ret = tcl_do_pps(h);
			if (ret < 0)
				return ret;
		}
		break;
	case RFID_LAYER2_ISO14443B:
		/* initialized T=CL state from Type B Activation Data */
		h->priv.tcl.cid = h->l2h->priv.iso14443b.cid;
		h->priv.tcl.fsc = h->l2h->priv.iso14443b.fsc;
		h->priv.tcl.fsd = h->l2h->priv.iso14443b.fsd;
		h->priv.tcl.fwt = h->l2h->priv.iso14443b.fwt;

		/* what about ta? sfgt? */

		if (h->l2h->priv.iso14443b.flags & ISO14443B_CID_SUPPORTED)
			h->priv.tcl.flags |= TCL_HANDLE_F_CID_SUPPORTED;
		if (h->l2h->priv.iso14443b.flags & ISO14443B_NAD_SUPPORTED)
			h->priv.tcl.flags |= TCL_HANDLE_F_NAD_SUPPORTED;

		switch (h->l2h->priv.iso14443b.state) {
			case ISO14443B_STATE_SELECTED:
				h->priv.tcl.state = TCL_STATE_ATS_RCVD;
				break;
			case ISO14443B_STATE_ATTRIB_SENT:
				h->priv.tcl.state = TCL_STATE_RATS_SENT;
				break;
		}

		/* PUPI will be presented as ATS/historical bytes */
		memcpy(h->priv.tcl.ats, h->l2h->uid, 4);
		h->priv.tcl.ats_len = 4;
		h->priv.tcl.historical_bytes = h->priv.tcl.ats;

		break;
	default:
		DEBUGP("unsupported l2: %u\n", h->l2h->l2->id);
		return -1;
		break;
	}

	h->priv.tcl.state = TCL_STATE_ESTABLISHED;

	return 0;
}

static int
tcl_deselect(struct rfid_protocol_handle *h)
{
	/* ISO 14443-4:2000(E) Section 8 */
	int ret;
	unsigned char frame[3];		/* 3 bytes prologue, no information */
	unsigned char rx[3];
	unsigned int rx_len = sizeof(rx);
	unsigned int prlg_len;
	struct tcl_handle *th = &h->priv.tcl;

	if (th->state != TCL_STATE_ESTABLISHED) {
		/* FIXME: not sure whether deselect is possible here,
		 * probably better send a HLTA? */
	}

	/* build DESELECT S-block */
	ret = tcl_build_prologue_s(th, frame, &prlg_len);
	if (ret < 0)
		return ret;

	ret = rfid_layer2_transceive(h->l2h, RFID_14443A_FRAME_REGULAR,
				     frame, prlg_len, rx,
				     &rx_len, deactivation_fwt(h),
				     TCL_TRANSP_F_TX_CRC);
	if (ret < 0) {
		/* FIXME: retransmit, HLT(A|B) */
		return ret;
	}

	th->state = TCL_STATE_DESELECTED;

	return 0;
}

struct fr_buff {
	unsigned int frame_len;		/* length of frame */
	unsigned int hdr_len;		/* length of header within frame */
	unsigned char data[RFID_MAX_FRAMELEN];
};

#define frb_payload(x)	(x.data + x.hdr_len)


/* RFID transceive buffer. */
struct rfid_xcvb {
	struct rfix_xcvb *next;		/* next in queue of buffers */

	u_int64_t timeout;		/* timeout to wait for reply */
	struct fr_buff tx;
	struct fr_buff rx;

	//struct rfid_protocol_handle *h;	/* connection to which we belong */
};

struct tcl_tx_context {
	const unsigned char *tx;
	unsigned char *rx;
	const unsigned char *next_tx_byte;
	unsigned char *next_rx_byte;
	unsigned int rx_len;
	unsigned int tx_len;
	struct rfid_protocol_handle *h;
};

#define tcl_ctx_todo(ctx) (ctx->tx_len - (ctx->next_tx_byte - ctx->tx))

static int 
tcl_refill_xcvb(struct rfid_xcvb *xcvb, struct tcl_tx_context *ctx)
{
	struct tcl_handle *th = &ctx->h->priv.tcl;

	if (ctx->next_tx_byte >= ctx->tx + ctx->tx_len) {
		DEBUGP("tyring to refill tx xcvb but no data left!\n");
		return -1;
	}

	if (tcl_build_prologue_i(th, xcvb->tx.data, 
				 &xcvb->tx.hdr_len) < 0)
		return -1;

	if (tcl_ctx_todo(ctx) > th->fsc - xcvb->tx.hdr_len)
		xcvb->tx.frame_len = max_net_tx_framesize(th);
	else
		xcvb->tx.frame_len = tcl_ctx_todo(ctx);

	memcpy(frb_payload(xcvb->tx), ctx->next_tx_byte,
		xcvb->tx.frame_len);

	ctx->next_tx_byte += xcvb->tx.frame_len;

	/* check whether we need to set the chaining bit */
	if (ctx->next_tx_byte < ctx->tx + ctx->tx_len)
		xcvb->tx.data[0] |= 0x10;

	/* add hdr_len after copying the net payload */
	xcvb->tx.frame_len += xcvb->tx.hdr_len;

	xcvb->timeout = th->fwt;

	return 0;
}

static void fill_xcvb_wtxm(struct tcl_handle *th, struct rfid_xcvb *xcvb,
			  unsigned char inf)
{
	/* Acknowledge WTXM */
	tcl_build_prologue_s(th, xcvb->tx.data, &xcvb->tx.hdr_len);
	/* set two bits that make this block a wtx */
	xcvb->tx.data[0] |= 0x30;
	xcvb->tx.data[xcvb->tx.hdr_len] = inf;
	xcvb->tx.frame_len = xcvb->tx.hdr_len+1;
	xcvb->timeout = th->fwt * inf;
}

static int check_cid(struct tcl_handle *th, struct rfid_xcvb *xcvb)
{
	if (xcvb->rx.data[0] & TCL_PCB_CID_FOLLOWING) {
		if (xcvb->rx.data[1] != th->cid) {
			DEBUGP("CID %u is not valid, we expected %u\n", 
				xcvb->rx.data[1], th->cid);
			return 0;
		}
	}
	return 1;
}

static int
tcl_transceive(struct rfid_protocol_handle *h,
		const unsigned char *tx_data, unsigned int tx_len,
		unsigned char *rx_data, unsigned int *rx_len,
		unsigned int timeout, unsigned int flags)
{
	int ret;

	struct rfid_xcvb xcvb;
	struct tcl_tx_context tcl_ctx;
	struct tcl_handle *th = &h->priv.tcl;

	unsigned char ack[10];
	unsigned int ack_len;

	/* initialize context */
	tcl_ctx.next_tx_byte = tcl_ctx.tx = tx_data;
	tcl_ctx.next_rx_byte = tcl_ctx.rx = rx_data;
	tcl_ctx.rx_len = *rx_len;
	tcl_ctx.tx_len = tx_len;
	tcl_ctx.h = h;

	/* initialize xcvb */
	xcvb.timeout = th->fwt;

tx_refill:
	if (tcl_refill_xcvb(&xcvb, &tcl_ctx) < 0) {
		ret = -1;
		goto out;
	}

do_tx:
	xcvb.rx.frame_len = sizeof(xcvb.rx.data);
	ret = rfid_layer2_transceive(h->l2h, l2_to_frame(h->l2h->l2->id),
				     xcvb.tx.data, xcvb.tx.frame_len,
				     xcvb.rx.data, &xcvb.rx.frame_len,
				     xcvb.timeout, 0);

	DEBUGP("l2 transceive finished\n");
	if (ret < 0)
		goto out;

	if (is_r_block(xcvb.rx.data[0])) {
		DEBUGP("R-Block\n");

		if ((xcvb.rx.data[0] & 0x01) != h->priv.tcl.toggle) {
			DEBUGP("response with wrong toggle bit\n");
			goto out;
		}

		/* Handle ACK frame in case of chaining */
		if (!check_cid(th, &xcvb))
			goto out;

		goto tx_refill;
	} else if (is_s_block(xcvb.rx.data[0])) {
		unsigned char inf;
		unsigned int prlg_len;

		DEBUGP("S-Block\n");
		/* Handle Wait Time Extension */
		
		if (!check_cid(th, &xcvb))
			goto out;

		if (xcvb.rx.data[0] & TCL_PCB_CID_FOLLOWING) {
			if (xcvb.rx.frame_len < 3) {
				DEBUGP("S-Block with CID but short len\n");
				ret = -1;
				goto out;
			}
			inf = xcvb.rx.data[2];
		} else
			inf = xcvb.rx.data[1];

		if ((xcvb.rx.data[0] & 0x30) != 0x30) {
			DEBUGP("S-Block but not WTX?\n");
			ret = -1;
			goto out;
		}
		inf &= 0x3f;	/* only lower 6 bits code WTXM */
		if (inf == 0 || (inf >= 60 && inf <= 63)) {
			DEBUGP("WTXM %u is RFU!\n", inf);
			ret = -1;
			goto out;
		}
		
		fill_xcvb_wtxm(th, &xcvb, inf);
		/* start over with next transceive */
		goto do_tx; 
	} else if (is_i_block(xcvb.rx.data[0])) {
		unsigned int net_payload_len;
		/* we're actually receiving payload data */

		DEBUGP("I-Block: ");

		if ((xcvb.rx.data[0] & 0x01) != h->priv.tcl.toggle) {
			DEBUGP("response with wrong toggle bit\n");
			goto out;
		}

		xcvb.rx.hdr_len = 1;

		if (!check_cid(th, &xcvb))
			goto out;

		if (xcvb.rx.data[0] & TCL_PCB_CID_FOLLOWING)
			xcvb.rx.hdr_len++;
		if (xcvb.rx.data[0] & TCL_PCB_NAD_FOLLOWING)
			xcvb.rx.hdr_len++;
	
		net_payload_len = xcvb.rx.frame_len - xcvb.rx.hdr_len;
		DEBUGPC("%u bytes\n", net_payload_len);
		memcpy(tcl_ctx.next_rx_byte, &xcvb.rx.data[xcvb.rx.hdr_len], 
			net_payload_len);
		tcl_ctx.next_rx_byte += net_payload_len;

		if (xcvb.rx.data[0] & 0x10) {
			/* we're not the last frame in the chain, continue rx */
			DEBUGP("not the last frame in the chain, continue\n");
			ack_len = sizeof(ack);
			tcl_build_prologue_r(th, xcvb.tx.data, &xcvb.tx.frame_len,						  0);
			xcvb.timeout = th->fwt;
			goto do_tx;
		}
	}

out:
	*rx_len = tcl_ctx.next_rx_byte - tcl_ctx.rx;
	return ret;
}

static struct rfid_protocol_handle *
tcl_init(struct rfid_layer2_handle *l2h)
{
	struct rfid_protocol_handle *th;
	unsigned int mru = l2h->rh->ah->mru;

	th = malloc_protocol_handle(sizeof(struct rfid_protocol_handle));
	if (!th)
		return NULL;

	/* FIXME: mru should be attribute of layer2 (in case it adds/removes
	 * some overhead */
	memset(th, 0, sizeof(struct rfid_protocol_handle));

	/* maximum received ats length equals mru of asic/reader */
	th->priv.tcl.state = TCL_STATE_INITIAL;
	th->priv.tcl.ats_len = mru;
	th->priv.tcl.toggle = 1;

	th->priv.tcl.fsd = iso14443_fsd_approx(mru);

	return th;
}

static int
tcl_fini(struct rfid_protocol_handle *ph)
{
	free_protocol_handle(ph);
	return 0;
}

int 
tcl_getopt(struct rfid_protocol_handle *h, int optname, void *optval,
	   unsigned int *optlen)
{
	u_int8_t *opt_str = optval;

	switch (optname) {
	case RFID_OPT_P_TCL_ATS:
		if (h->priv.tcl.ats_len < *optlen)
			*optlen = h->priv.tcl.ats_len;
		memcpy(opt_str, h->priv.tcl.ats, *optlen);
		break;
	case RFID_OPT_P_TCL_ATS_LEN:
		if (*optlen < sizeof(u_int8_t))
			return -E2BIG;
		*optlen = sizeof(u_int8_t);
		*opt_str = h->priv.tcl.ats_len & 0xff;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int
tcl_setopt(struct rfid_protocol_handle *h, int optname, const void *optval,
	   unsigned int optlen)
{
	int ret = -EINVAL;

	switch (optname) {
	default:
		break;
	}

	return ret;
}

const struct rfid_protocol rfid_protocol_tcl = {
	.id	= RFID_PROTOCOL_TCL,
	.name	= "ISO 14443-4 / T=CL",
	.fn	= {
		.init = &tcl_init,
		.open = &tcl_connect,
		.transceive = &tcl_transceive,
		.close = &tcl_deselect,
		.fini = &tcl_fini,
		.getopt = &tcl_getopt,
		.setopt = &tcl_setopt,
	},
};
