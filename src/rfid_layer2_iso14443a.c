/* ISO 14443-3 A anticollision implementation
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
#ifdef  __MINGW32__
#include <windows.h>
#endif/*__MINGW32__*/

#include <librfid/rfid.h>
#include <librfid/rfid_layer2.h>
#include <librfid/rfid_reader.h>
#include <librfid/rfid_layer2_iso14443a.h>
#include <librfid/rfid_protocol.h>

#define TIMEOUT 1236

unsigned long randctx[4]={0x22d4a017,0x773a1f44,0xc39e1460,0x9cde8801};

/* Transceive a 7-bit short frame */
int
iso14443a_transceive_sf(struct rfid_layer2_handle *handle,
			 unsigned char cmd,
			 struct iso14443a_atqa *atqa)
{
	const struct rfid_reader *rdr = handle->rh->reader;

	return rdr->iso14443a.transceive_sf(handle->rh, cmd, atqa);
}

/* Transmit an anticollission bit frame */
static int
iso14443a_transceive_acf(struct rfid_layer2_handle *handle,
			 struct iso14443a_anticol_cmd *acf,
			 unsigned int *bit_of_col)
{
	const struct rfid_reader *rdr = handle->rh->reader;

	return rdr->iso14443a.transceive_acf(handle->rh, acf, bit_of_col);
}

/* Transmit a regular frame */
static int 
iso14443a_transceive(struct rfid_layer2_handle *handle,
		     enum rfid_frametype frametype, 
			const unsigned char *tx_buf, unsigned int tx_len,
			unsigned char *rx_buf, unsigned int *rx_len,
			u_int64_t timeout, unsigned int flags)
{
	return handle->rh->reader->transceive(handle->rh, frametype, tx_buf,
					tx_len, rx_buf, rx_len, timeout, flags);
}

static int 
iso14443a_code_nvb_bits(unsigned char *nvb, unsigned int bits)
{
	unsigned int byte_count = bits / 8;
	unsigned int bit_count = bits % 8;

	if (byte_count < 2 || byte_count > 7)
		return -1;

	*nvb = ((byte_count & 0xf) << 4) | bit_count;

	return 0;
}

static int random_bit(void)
{
	unsigned long e;

	e = randctx[0];    
	randctx[0] = randctx[1];
	randctx[1] = (randctx[2]<<19) + (randctx[2]>>13) + randctx[3];
	randctx[2] = randctx[3] ^ randctx[0];
	randctx[3] = e+randctx[1];
    
	return randctx[1]&1;
}

/* first bit is '1', second bit '2' */
static void
rnd_toggle_bit_in_field(unsigned char *bitfield, unsigned int size, unsigned int bit)
{
	unsigned int byte,rnd;

	if (bit && (bit <= (size*8))) {
		rnd = random_bit();
	
		DEBUGP("xor'ing bit %u with %u\n",bit,rnd);
		bit--;
		byte = bit/8;
		bit = rnd << (bit % 8);
		bitfield[byte] ^= bit;
	}
}


static int
iso14443a_anticol(struct rfid_layer2_handle *handle)
{
	int ret;
	unsigned int uid_size;
	struct iso14443a_handle *h = &handle->priv.iso14443a;
	struct iso14443a_atqa *atqa = &h->atqa;
	struct iso14443a_anticol_cmd acf;
	unsigned int bit_of_col;
	unsigned char sak[3];
	unsigned int rx_len = sizeof(sak);
	char *aqptr = (char *) atqa;

	memset(handle->uid, 0, sizeof(handle->uid));
	memset(sak, 0, sizeof(sak));
	memset(atqa, 0, sizeof(&atqa));
	memset(&acf, 0, sizeof(acf));

	if (handle->flags & RFID_OPT_LAYER2_WUP)
		ret = iso14443a_transceive_sf(handle, ISO14443A_SF_CMD_WUPA, atqa);
	else
		ret = iso14443a_transceive_sf(handle, ISO14443A_SF_CMD_REQA, atqa);
	if (ret < 0) {
		h->state = ISO14443A_STATE_REQA_SENT;
		DEBUGP("error during transceive_sf: %d\n", ret);
		return ret;
	}
	h->state = ISO14443A_STATE_ATQA_RCVD;
	
	DEBUGP("ATQA: 0x%02x 0x%02x\n", *aqptr, *(aqptr+1));

	if (!atqa->bf_anticol) {
		h->state = ISO14443A_STATE_NO_BITFRAME_ANTICOL;
		DEBUGP("no bitframe anticollission bits set, aborting\n");
		return -1;
	}

	if (atqa->uid_size == 2 || atqa->uid_size == 3)
		uid_size = 3;
	else if (atqa->uid_size == 1)
		uid_size = 2;
	else
		uid_size = 1;
	
	acf.sel_code = ISO14443A_AC_SEL_CODE_CL1;

	h->state = ISO14443A_STATE_ANTICOL_RUNNING;
	h->level = ISO14443A_LEVEL_CL1;

cascade:
	rx_len = sizeof(sak);
	iso14443a_code_nvb_bits(&acf.nvb, 16);

	ret = iso14443a_transceive_acf(handle, &acf, &bit_of_col);
	DEBUGP("tran_acf->%d boc: %d\n",ret,bit_of_col);
	if (ret < 0)
		return ret;
	
	while (bit_of_col != ISO14443A_BITOFCOL_NONE) {
		DEBUGP("collision at pos %u\n", bit_of_col);

		iso14443a_code_nvb_bits(&acf.nvb, bit_of_col);
		rnd_toggle_bit_in_field(acf.uid_bits, sizeof(acf.uid_bits), bit_of_col);
		DEBUGP("acf: nvb=0x%02X uid_bits=%s\n",acf.nvb,rfid_hexdump(acf.uid_bits,sizeof(acf.uid_bits)));
		ret = iso14443a_transceive_acf(handle, &acf, &bit_of_col);
		if (ret < 0)
			return ret;
	}

	iso14443a_code_nvb_bits(&acf.nvb, 7*8);

	ret = iso14443a_transceive(handle, RFID_14443A_FRAME_REGULAR,
				   (unsigned char *)&acf, 7, 
				   (unsigned char *) &sak, &rx_len,
				   TIMEOUT, 0);
	if (ret < 0)
		return ret;

	if (sak[0] & 0x04) {
		/* Cascade bit set, UID not complete */
		switch (acf.sel_code) {
		case ISO14443A_AC_SEL_CODE_CL1:
			/* cascading from CL1 to CL2 */
			DEBUGP("cascading from CL1 to CL2\n");
			if (acf.uid_bits[0] != 0x88) {
				DEBUGP("Cascade bit set, but UID0 != 0x88\n");
				return -1;
			}
			memcpy(&handle->uid[0], &acf.uid_bits[1], 3);
			acf.sel_code = ISO14443A_AC_SEL_CODE_CL2;
			h->level = ISO14443A_LEVEL_CL2;
			break;
		case ISO14443A_AC_SEL_CODE_CL2:
			/* cascading from CL2 to CL3 */
			DEBUGP("cascading from CL2 to CL3\n");
			memcpy(&handle->uid[3], &acf.uid_bits[1], 3);
			acf.sel_code = ISO14443A_AC_SEL_CODE_CL3;
			h->level = ISO14443A_LEVEL_CL3;
			break;
		default:
			DEBUGP("cannot cascade any further than CL3\n");
			h->state = ISO14443A_STATE_ERROR;
			return -1;
			break;
		}
		goto cascade;

	} else {
		switch (acf.sel_code) {
		case ISO14443A_AC_SEL_CODE_CL1:
			/* single size UID (4 bytes) */
			memcpy(&handle->uid[0], &acf.uid_bits[0], 4);
			break;
		case ISO14443A_AC_SEL_CODE_CL2:
			/* double size UID (7 bytes) */
			memcpy(&handle->uid[3], &acf.uid_bits[0], 4);
			break;
		case ISO14443A_AC_SEL_CODE_CL3:
			/* triple size UID (10 bytes) */
			memcpy(&handle->uid[6], &acf.uid_bits[0], 4);
			break;
		}
	}

	{
		if (h->level == ISO14443A_LEVEL_CL1)
			handle->uid_len = 4;
		else if (h->level == ISO14443A_LEVEL_CL2)
			handle->uid_len = 7;
		else 
			handle->uid_len = 10;

		DEBUGP("UID %s\n", rfid_hexdump(handle->uid, handle->uid_len));
	}

	h->level = ISO14443A_LEVEL_NONE;
	h->state = ISO14443A_STATE_SELECTED;
	h->sak = sak[0];

	if (sak[0] & 0x20) {
		DEBUGP("we have a T=CL compliant PICC\n");
		handle->proto_supported = 1 << RFID_PROTOCOL_TCL;
		h->tcl_capable = 1;
	} else {
		DEBUGP("we have a T!=CL PICC\n");
		handle->proto_supported = (1 << RFID_PROTOCOL_MIFARE_UL)|
					  (1 << RFID_PROTOCOL_MIFARE_CLASSIC);
		h->tcl_capable = 0;
	}

	return 0;
}

static int
iso14443a_hlta(struct rfid_layer2_handle *handle)
{
	int ret;
	unsigned char tx_buf[2] = { 0x50, 0x00 };
	unsigned char rx_buf[10];
	unsigned int rx_len = sizeof(rx_buf);

	ret = iso14443a_transceive(handle, RFID_14443A_FRAME_REGULAR,
				   tx_buf, sizeof(tx_buf),
				   rx_buf, &rx_len, 1000 /* 1ms */, 0);
	if (ret < 0) {
		/* "error" case: we don't get somethng back from the card */
		return 0;
	}
	return -1;
}

static int
iso14443a_setopt(struct rfid_layer2_handle *handle, int optname,
		 const void *optval, unsigned int optlen)
{
	int ret = -EINVAL;
	const struct rfid_reader *rdr = handle->rh->reader;
	unsigned int speed;

	switch (optname) {
	case RFID_OPT_14443A_SPEED_RX:
		if (!rdr->iso14443a.set_speed)
			return -ENOTSUP;
		speed = *(unsigned int *)optval;
		ret = rdr->iso14443a.set_speed(handle->rh, 0, speed);
		break;
	case RFID_OPT_14443A_SPEED_TX:
		if (!rdr->iso14443a.set_speed)
			return -ENOTSUP;
		speed = *(unsigned int *)optval;
		ret = rdr->iso14443a.set_speed(handle->rh, 1, speed);
		break;
	case RFID_OPT_14443A_WUPA:
		if((unsigned int*)optval)
			handle->flags |= RFID_OPT_LAYER2_WUP;
		else
			handle->flags &= ~RFID_OPT_LAYER2_WUP;
		ret = 0;
		break;
	};

	return ret;
}

static int
iso14443a_getopt(struct rfid_layer2_handle *handle, int optname,
		 void *optval, unsigned int *optlen)
{
	int ret = -EINVAL;
	struct iso14443a_handle *h = &handle->priv.iso14443a;
	struct iso14443a_atqa *atqa = optval;
	u_int8_t *opt_u8 = optval;
	int *wupa = optval;

	switch (optname) {
	case RFID_OPT_14443A_SAK:
		*opt_u8 = h->sak;
		*optlen = sizeof(*opt_u8);
		break;
	case RFID_OPT_14443A_ATQA:
		*atqa = h->atqa;
		*optlen = sizeof(*atqa);
		ret = 0;
		break;
	case RFID_OPT_14443A_WUPA:
		*wupa = ((handle->flags & RFID_OPT_LAYER2_WUP) != 0);
		*optlen = sizeof(*wupa);
		ret = 0;
		break;
	};

	return ret;
}


static struct rfid_layer2_handle *
iso14443a_init(struct rfid_reader_handle *rh)
{
	int ret;
	struct rfid_layer2_handle *h = malloc_layer2_handle(sizeof(*h));
	if (!h)
		return NULL;

	memset(h, 0, sizeof(*h));
	
#ifdef  __MINGW32__
	randctx[0] ^= GetTickCount();
#endif/*__MINGW32__*/
        for(ret=0;ret<23;ret++)
            random_bit();
	
	h->l2 = &rfid_layer2_iso14443a;
	h->rh = rh;
	h->priv.iso14443a.state = ISO14443A_STATE_NONE;
	h->priv.iso14443a.level = ISO14443A_LEVEL_NONE;

	ret = h->rh->reader->init(h->rh, RFID_LAYER2_ISO14443A);
	if (ret < 0) {
		free_layer2_handle(h);
		return NULL;
	}

	return h;
}

static int
iso14443a_fini(struct rfid_layer2_handle *handle)
{
	free_layer2_handle(handle);
	return 0;
}


const struct rfid_layer2 rfid_layer2_iso14443a = {
	.id	= RFID_LAYER2_ISO14443A,
	.name 	= "ISO 14443-3 A",
	.fn	= {
		.init 		= &iso14443a_init,
		.open 		= &iso14443a_anticol,
		.transceive 	= &iso14443a_transceive,
		.close 		= &iso14443a_hlta,
		.fini 		= &iso14443a_fini,
		.setopt		= &iso14443a_setopt,
		.getopt		= &iso14443a_getopt,
	},
};

