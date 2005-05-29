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
	/* ISO 14443-4:2000(E) Section 5.2.5. */
	return (256 * 16 / h->l2h->rh->ah->fc) * (2 ^ sfgi);
}

static unsigned int fwi_to_fwt(struct rfid_protocol_handle *h, 
				unsigned char fwi)
{
	/* ISO 14443-4:2000(E) Section 7.2. */
	return (256*16 / h->l2h->rh->ah->fc) * (2 ^ fwi);
}

#define activation_fwt(x) (65536 / x->l2h->rh->ah->fc)
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

	if (len == 1) {
		/* FIXME: assume some default values */
		h->priv.tcl.fsc = 32;
		h->priv.tcl.ta = 0;
		h->priv.tcl.sfgt = sfgi_to_sfgt(h, 0);
		if (1 /* FIXME: is_iso14443a */) {
			/* Section 7.2: fwi default for type A is 4 */
			h->priv.tcl.fwt = fwi_to_fwt(h, 4);
		} else {
			/* Section 7.2: fwi for type B is always in ATQB */
			/* FIXME */
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
/* start a PSS run (autimatically configure highest possible speed */
static int 
tcl_do_pss(struct rfid_protocol_handle *h)
{
	unsigned char ppss[3];
	unsigned char pps_response[1];

	if (h->priv.tcl.state != TCL_STATE_ATS_RCVD)
		return -1;

	/* ISO 14443-4:2000(E) Section 5.3. */

	ppss[0] = 0xd0 & (h->priv.tcl.cid & 0x0f);
	ppss[1] = 0x11;

	//ppss[2] = 0x00 & foo;

	// FIXME: finish
	
	return -1;
	
	h->priv.tcl.state = TCL_STATE_ESTABLISHED;
}


static int
tcl_build_prologue2(struct tcl_handle *th, 
		    unsigned char *prlg, unsigned int *prlg_len, 
		    unsigned char pcb)
{
	*prlg_len = 1;

	*prlg = pcb;

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

	/* the only S-block from PCD->PICC is DESELECT: */
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

		if (0 /* FIXME */) {
			ret = tcl_do_pss(h);
			if (ret < 0)
				return -1;
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

	ret = h->l2h->l2->fn.transcieve(h->l2h, tx_buf, tx_len+prlg_len, 
				     rx_buf, rx_len, th->fwt, 0);
	if (ret < 0)
		goto out_rxb;


	memcpy(rx_data, rx_buf, *rx_len);

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
