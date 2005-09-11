/* ISO 14443-4 (T=CL) implementation, PCD side.
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
#include <rfid/rfid_protocol_tcl.h>
#include <rfid/rfid_protocol.h>
#include <rfid/rfid_layer2.h>
#include <rfid/rfid_layer2_iso14443b.h>

#include <rfid/rfid_asic.h>
#include <rfid/rfid_reader.h>

#include "rfid_iso14443_common.h"


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
			/* This function is never called for Type B, since it has no (R)ATS */
		}
		return 0;
	}

	/* guarateed to be at least 2 bytes in size */

	t0 = ats[1];
	cur = &ats[2];

	iso14443_fsdi_to_fsd(&h->priv.tcl.fsc, t0 & 0x0f);

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
		if (*cur & 0x01)
			h->priv.tcl.flags |= TCL_HANDLE_F_NAD_SUPPORTED;
		if (*cur & 0x02)
			h->priv.tcl.flags |= TCL_HANDLE_F_CID_SUPPORTED;
		cur++;
	}

	h->priv.tcl.historical_len = (ats+len) - cur;
	h->priv.tcl.historical_bytes = cur;

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

	/* transcieve (with CRC) */
	ret = h->l2h->l2->fn.transcieve(h->l2h, rats, 2, h->priv.tcl.ats,
				       &h->priv.tcl.ats_len, activation_fwt(h),
				       TCL_TRANSP_F_TX_CRC);
	if (ret < 0) {
		DEBUGP("transcieve of rats failed\n");
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
	
	if ((D & ATS_TA_DIV_8) && (speed & RFID_READER_SPEED_848K))
		DI = PPS_DIV_8;
	else if ((D & ATS_TA_DIV_4) && (speed & RFID_READER_SPEED_424K))
		DI = PPS_DIV_4;
	else if ((D & ATS_TA_DIV_2) && (speed & RFID_READER_SPEED_212K))
		DI = PPS_DIV_2;
	else
		DI = PPS_DIV_1;

	return DI;
}


/* start a PSS run (autimatically configure highest possible speed */
static int 
tcl_do_pps(struct rfid_protocol_handle *h)
{
	int ret;
	unsigned char ppss[3];
	unsigned char pps_response[1];
	unsigned int rx_len = 1;
	unsigned char Dr, Ds, DrI, DsI;

	if (h->priv.tcl.state != TCL_STATE_ATS_RCVD)
		return -1;

	Dr = h->priv.tcl.ta & 0x07;
	Ds = h->priv.tcl.ta & 0x70 >> 4;

	if (Dr != Ds && !(h->priv.tcl.ta & 0x80)) {
		/* device supports different divisors for rx and tx, but not ?!? */
		DEBUGP("PICC has contradictory TA, aborting PPS\n");
		return -1;
	};

	/* ISO 14443-4:2000(E) Section 5.3. */

	ppss[0] = 0xd0 & (h->priv.tcl.cid & 0x0f);
	ppss[1] = 0x11;

	/* FIXME: deal with different speed for each direction */
	DrI = d_to_di(h, Dr);
	DsI = d_to_di(h, Ds);

	ppss[2] = (ppss[2] & 0xf0) | (DrI | DsI << 2);

	ret = h->l2h->l2->fn.transcieve(h->l2h, ppss, 3, pps_response,
					&rx_len, h->priv.tcl.fwt,
					TCL_TRANSP_F_TX_CRC);
	if (ret < 0)
		return ret;

	if (pps_response[0] != ppss[0]) {
		DEBUGP("PPS Response != PPSS\n");
		return -1;
	}
	
	h->priv.tcl.state = TCL_STATE_ESTABLISHED;

	return 0;
}


static int
tcl_build_prologue2(struct tcl_handle *th, 
		    unsigned char *prlg, unsigned int *prlg_len, 
		    unsigned char pcb)
{
	*prlg_len = 1;

	*prlg = pcb;

	if (th->toggle) {
		/* we've sent a toggle bit last time */
		th->toggle = 0;
	} else {
		/* we've not sent a toggle last time: send one */
		th->toggle = 1;
		*prlg |= 0x01;
	}

	if (th->flags & TCL_HANDLE_F_CID_USED) {
		/* ISO 14443-4:2000(E) Section 7.1.1.2 */
		*prlg |= TCL_PCB_CID_FOLLOWING;
		(*prlg_len)++;
		prlg[*prlg_len] = th->cid & 0x0f;
	}

	/* nad only for I-block (0xc0 == 00) */
	if ((th->flags & TCL_HANDLE_F_NAD_USED) &&
	    ((pcb & 0xc0) == 0x00)) {
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
		memcpy(h->priv.tcl.ats, h->l2h->priv.iso14443b.pupi, 4);
		h->priv.tcl.ats_len = 4;
		h->priv.tcl.historical_bytes = h->priv.tcl.ats;

		break;
	default:
		DEBUGP("unsupported l2: %u\n", h->l2h->l2->id);
		return -1;
		break;
	}

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

	ret = h->l2h->l2->fn.transcieve(h->l2h, frame, prlg_len, rx,
				     &rx_len, deactivation_fwt(h),
				     TCL_TRANSP_F_TX_CRC);
	if (ret < 0) {
		/* FIXME: retransmit, HLT(A|B) */
		return ret;
	}

	th->state = TCL_STATE_DESELECTED;

	return 0;
}

#define is_s_block(x) ((x & 0xc0) == 0xc0)
#define is_r_block(x) ((x & 0xc0) == 0x80)
#define is_i_block(x) ((x & 0xc0) == 0x00)

static int
tcl_transcieve(struct rfid_protocol_handle *h,
		const unsigned char *tx_data, unsigned int tx_len,
		unsigned char *rx_data, unsigned int *rx_len,
		unsigned int timeout, unsigned int flags)
{
	int ret;
	unsigned char *tx_buf, *rx_buf;
	unsigned int prlg_len;
	struct tcl_handle *th = &h->priv.tcl;

	unsigned char *_tx;
	unsigned int _tx_len, _timeout;
	unsigned char wtx_resp[3];

	if (tx_len > max_net_tx_framesize(th)) {
		/* slow path: we need to use chaining */
		return -1;
	}

	tx_buf = malloc(tcl_prlg_len(th) + tx_len);
	if (!tx_buf) {
		ret = -ENOMEM;
		goto out;
	}
	rx_buf = malloc(tcl_prlg_len(th) + *rx_len);
	if (!rx_buf) {
		ret = -ENOMEM;
		goto out_txb;
	}

	if (tcl_build_prologue_i(th, tx_buf, &prlg_len) < 0) {
		ret = -1;
		goto out_rxb;
	}
	memcpy(tx_buf + prlg_len, tx_data, tx_len);

	/* intialize to data-to-be-transferred */
	_tx = tx_buf;
	_tx_len = tx_len+prlg_len;
	_timeout = th->fwt;

do_tx:
	ret = h->l2h->l2->fn.transcieve(h->l2h, _tx, _tx_len,
					rx_buf, rx_len, _timeout, 0);
	if (ret < 0)
		goto out_rxb;

	if ((*rx_buf & 0x01) != h->priv.tcl.toggle) {
		DEBUGP("response with wrong toggle bit\n");
		goto out_rxb;
	}

	//if (*rx_len )
	//

	if (is_r_block(*rx_buf)) {
		unsigned int txed = _tx - tx_buf;
		/* Handle ACK frame in case of chaining */
		if (*rx_buf & TCL_PCB_CID_FOLLOWING) {
			if (*(rx_buf+1) != h->priv.tcl.cid) {
				DEBUGP("CID %u is not valid\n", *(rx_buf)+1);
				goto out_rxb;
			}
		}
		/* set up parameters for next frame in chain */
		if (txed < tx_len) {
			/* move tx pointer by the amount of bytes transferred
			 * in last frame */
			_tx += _tx_len;
			_tx_len = (tx_len - txed);
			if (_tx_len > max_net_tx_framesize(th)) {
				/* not last frame in chain */
				_tx_len = max_net_tx_framesize(th);
			} else {
				/* last frame in chain */
			}
			goto do_tx;
		} else {
			DEBUGP("Received ACK in response to last frame in "
			       "chain?!? Expected I-frame.\n");
			ret = -1;
			goto out_rxb;
		}
	} else if (is_s_block(*rx_buf)) {
		unsigned char inf;
		unsigned int prlg_len;

		/* Handle Wait Time Extension */
		if (*rx_buf & TCL_PCB_CID_FOLLOWING) {
			if (*rx_len < 3) {
				DEBUGP("S-Block with CID but short len\n");
				ret = -1;
				goto out_rxb;
			}
			if (*(rx_buf+1) != h->priv.tcl.cid) {
				DEBUGP("CID %u is not valid\n", *(rx_buf)+1);
				goto out_rxb;
			}
			inf = *(rx_buf+2);
		} else
			inf = *(rx_buf+1);

		if ((*rx_buf & 0x30) != 0x30) {
			DEBUGP("S-Block but not WTX?\n");
			ret = -1;
			goto out_rxb;
		}
		inf &= 0x3f;	/* only lower 6 bits code WTXM */
		if (inf == 0 || (inf >= 60 && inf <= 63)) {
			DEBUGP("WTXM %u is RFU!\n", inf);
			ret = -1;
			goto out_rxb;
		}
		
		/* Acknowledge WTXM */
		tcl_build_prologue_s(&h->priv.tcl, wtx_resp, &prlg_len);
		/* set two bits that make this block a wtx */
		wtx_resp[0] |= 0x30;
		wtx_resp[prlg_len] = inf;
		_tx = wtx_resp;
		_tx_len = prlg_len+1;
		_timeout = th->fwt * inf;

		/* start over with next transcieve */
		goto do_tx; /* FIXME: do transcieve locally since we use
				totally different buffer */

	} else if (is_i_block(*rx_buf)) {
		unsigned char *inf = rx_buf+1;
		/* we're actually receiving payload data */

		if (*rx_buf & TCL_PCB_CID_FOLLOWING) {
			if (*(rx_buf+1) != h->priv.tcl.cid) {
				DEBUGP("CID %u is not valid\n", *(rx_buf)+1);
				goto out_rxb;
			}
			inf++;
		}
		if (*rx_buf & TCL_PCB_NAD_FOLLOWING) {
			inf++;
		}
		memcpy(rx_data, inf, *rx_len - (inf - rx_buf));

		if (*rx_buf & 0x10) {
			/* we're not the last frame in the chain, continue */
			goto do_tx;
		}
	}

out_rxb:
	free(rx_buf);
out_txb:
	free(tx_buf);
out:
	return ret;
}

#if 0
int
tcl_send(struct tcl_handle *th)
{
	return -1;
}

int
tcl_recv()
{
	return -1;
}
#endif

static struct rfid_protocol_handle *
tcl_init(struct rfid_layer2_handle *l2h)
{
	struct rfid_protocol_handle *th;
	unsigned int mru = l2h->rh->ah->mru;

	th = malloc(sizeof(struct rfid_protocol_handle) + mru);
	if (!th)
		return NULL;

	/* FIXME: mru should be attribute of layer2 (in case it adds/removes
	 * some overhead */
	memset(th, 0, sizeof(struct rfid_protocol_handle) + mru);

	/* maximum received ats length equals mru of asic/reader */
	th->priv.tcl.state = TCL_STATE_INITIAL;
	th->priv.tcl.ats_len = mru;
	th->priv.tcl.toggle = 1;
	th->l2h = l2h;
	th->proto = &rfid_protocol_tcl;

	th->priv.tcl.fsd = iso14443_fsd_approx(mru);

	return th;
}

static int
tcl_fini(struct rfid_protocol_handle *ph)
{
	free(ph);
	return 0;
}

struct rfid_protocol rfid_protocol_tcl = {
	.id	= RFID_PROTOCOL_TCL,
	.name	= "ISO 14443-4 / T=CL",
	.fn	= {
		.init = &tcl_init,
		.open = &tcl_connect,
		.transcieve = &tcl_transcieve,
		.close = &tcl_deselect,
		.fini = &tcl_fini,
	},
};
