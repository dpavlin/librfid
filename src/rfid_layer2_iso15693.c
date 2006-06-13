/* ISO 15693 anticollision implementation
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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <librfid/rfid.h>
#include <librfid/rfid_layer2.h>
#include <librfid/rfid_reader.h>
#include <librfid/rfid_layer2_iso15693.h>

#if 0
/* Transceive a 7-bit short frame */
static int
iso14443a_transceive_sf(struct rfid_layer2_handle *handle,
			 unsigned char cmd,
			 struct iso14443a_atqa *atqa)
{
	struct rfid_reader *rdr = handle->rh->reader;

	return rdr->iso14443a.transceive_sf(handle->rh, cmd, atqa);
}

/* Transmit an anticollission bit frame */
static int
iso14443a_transceive_acf(struct rfid_layer2_handle *handle,
			 struct iso14443a_anticol_cmd *acf,
			 unsigned int *bit_of_col)
{
	struct rfid_reader *rdr = handle->rh->reader;

	return rdr->iso14443a.transceive_acf(handle->rh, acf, bit_of_col);
}

/* Transmit a regular frame */
static int 
iso14443a_transceive(struct rfid_layer2_handle *handle,
			const unsigned char *tx_buf, unsigned int tx_len,
			unsigned char *rx_buf, unsigned int *rx_len,
			u_int64_t, unsigned int flags)
{
	return handle->rh->reader->transceive(handle->rh, tx_buf, tx_len, 
						rx_buf, rx_len, timeout, flags);
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

/* first bit is '1', second bit '2' */
static void
set_bit_in_field(unsigned char *bitfield, unsigned int bit)
{
	unsigned int byte_count = bit / 8;
	unsigned int bit_count = bit % 8;

	DEBUGP("bitfield=%p, byte_count=%u, bit_count=%u\n",
			bitfield, byte_count, bit_count);
	DEBUGP("%p = 0x%02x\n", (bitfield+byte_count), *(bitfield+byte_count));
	*(bitfield+byte_count) |= 1 << (bit_count-1);
	DEBUGP("%p = 0x%02x\n", (bitfield+byte_count), *(bitfield+byte_count));
}

static int
iso14443a_anticol(struct rfid_layer2_handle *handle)
{
	int ret;
	unsigned int uid_size;
	struct iso14443a_atqa atqa;
	struct iso14443a_anticol_cmd acf;
	unsigned int bit_of_col;
	unsigned char sak[3];
	unsigned char uid[10];	// triple size equals 10 bytes;
	unsigned int rx_len = sizeof(sak);
	char *aqptr = (char *) &atqa;
	static int first = 0;

	memset(uid, 0, sizeof(uid));
	memset(sak, 0, sizeof(sak));
	memset(&atqa, 0, sizeof(atqa));
	memset(&acf, 0, sizeof(acf));

	if (first == 0) {
	DEBUGP("Sending REQA\n");
	ret = iso14443a_transceive_sf(handle, ISO14443A_SF_CMD_REQA, &atqa);
	first = 1;
	} else {
	DEBUGP("Sending WUPA\n");
	ret = iso14443a_transceive_sf(handle, ISO14443A_SF_CMD_WUPA, &atqa);
	}

	if (ret < 0) {
		handle->priv.iso14443a.state = ISO14443A_STATE_REQA_SENT;
		DEBUGP("error during transceive_sf: %d\n", ret);
		return ret;
	}
	handle->priv.iso14443a.state = ISO14443A_STATE_ATQA_RCVD;

	DEBUGP("ATQA: 0x%02x 0x%02x\n", *aqptr, *(aqptr+1));

	if (!atqa.bf_anticol) {
		handle->priv.iso14443a.state =ISO14443A_STATE_NO_BITFRAME_ANTICOL;
		DEBUGP("no bitframe anticollission bits set, aborting\n");
		return -1;
	}

	if (atqa.uid_size == 2 || atqa.uid_size == 3)
		uid_size = 3;
	else if (atqa.uid_size == 1)
		uid_size = 2;
	else
		uid_size = 1;
	
	acf.sel_code = ISO14443A_AC_SEL_CODE_CL1;

	handle->priv.iso14443a.state = ISO14443A_STATE_ANTICOL_RUNNING;
	handle->priv.iso14443a.level = ISO14443A_LEVEL_CL1;

cascade:
	iso14443a_code_nvb_bits(&acf.nvb, 16);

	ret = iso14443a_transceive_acf(handle, &acf, &bit_of_col);
	if (ret < 0)
		return ret;
	DEBUGP("bit_of_col = %u\n", bit_of_col);
	
	while (bit_of_col != ISO14443A_BITOFCOL_NONE) {
		set_bit_in_field(&acf.uid_bits[0], bit_of_col-16);
		iso14443a_code_nvb_bits(&acf.nvb, bit_of_col);
		ret = iso14443a_transceive_acf(handle, &acf, &bit_of_col);
		DEBUGP("bit_of_col = %u\n", bit_of_col);
		if (ret < 0)
			return ret;
	}

	iso14443a_code_nvb_bits(&acf.nvb, 7*8);
	ret = iso14443a_transceive(handle, (unsigned char *)&acf, 7, 
				   (unsigned char *) &sak, &rx_len,
				   TIMEOUT, 0);
	if (ret < 0)
		return ret;

	if (sak[0] & 0x04) {
		/* Cascade bit set, UID not complete */
		switch (acf.sel_code) {
		case ISO14443A_AC_SEL_CODE_CL1:
			/* cascading from CL1 to CL2 */
			if (acf.uid_bits[0] != 0x88) {
				DEBUGP("Cascade bit set, but UID0 != 0x88\n");
				return -1;
			}
			memcpy(&uid[0], &acf.uid_bits[1], 3);
			acf.sel_code = ISO14443A_AC_SEL_CODE_CL2;
			handle->priv.iso14443a.level = ISO14443A_LEVEL_CL2;
			break;
		case ISO14443A_AC_SEL_CODE_CL2:
			/* cascading from CL2 to CL3 */
			memcpy(&uid[3], &acf.uid_bits[1], 3);
			acf.sel_code = ISO14443A_AC_SEL_CODE_CL3;
			handle->priv.iso14443a.level = ISO14443A_LEVEL_CL3;
			break;
		default:
			DEBUGP("cannot cascade any further than CL3\n");
			handle->priv.iso14443a.state = ISO14443A_STATE_ERROR;
			return -1;
			break;
		}
		goto cascade;

	} else {
		switch (acf.sel_code) {
		case ISO14443A_AC_SEL_CODE_CL1:
			/* single size UID (4 bytes) */
			memcpy(&uid[0], &acf.uid_bits[0], 4);
			break;
		case ISO14443A_AC_SEL_CODE_CL2:
			/* double size UID (7 bytes) */
			memcpy(&uid[3], &acf.uid_bits[0], 4);
			break;
		case ISO14443A_AC_SEL_CODE_CL3:
			/* triple size UID (10 bytes) */
			memcpy(&uid[6], &acf.uid_bits[0], 4);
			break;
		}
	}

	handle->priv.iso14443a.level = ISO14443A_LEVEL_NONE;
	handle->priv.iso14443a.state = ISO14443A_STATE_SELECTED;

	{
		int uid_len;
		if (uid_size == 1)
			uid_len = 4;
		else if (uid_size == 2)
			uid_len = 7;
		else 
			uid_len = 10;

		DEBUGP("UID %s\n", rfid_hexdump(uid, uid_len));
	}

	if (sak[0] & 0x20) {
		DEBUGP("we have a T=CL compliant PICC\n");
		handle->priv.iso14443a.tcl_capable = 1;
	} else {
		DEBUGP("we have a T!=CL PICC\n");
		handle->priv.iso14443a.tcl_capable = 0;
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

	return 0;

	ret = iso14443a_transceive(handle, tx_buf, sizeof(tx_buf),
				   rx_buf, &rx_len, 1000 /* 1ms */, 0);
	if (ret < 0) {
		/* "error" case: we don't get somethng back from the card */
		return 0;
	}
	return -1;
}
#endif

static struct rfid_layer2_handle *
iso15693_init(struct rfid_reader_handle *rh)
{
	int ret;
	struct rfid_layer2_handle *h = malloc(sizeof(*h));
	if (!h)
		return NULL;

	h->l2 = &rfid_layer2_iso15693;
	h->rh = rh;
	h->priv.iso15693.state = ISO15693_STATE_NONE;

	ret = h->rh->reader->iso15693.init(h->rh);
	if (ret < 0) {
		free(h);
		return NULL;
	}

	return h;
}

static int
iso15693_fini(struct rfid_layer2_handle *handle)
{
	free(handle);
	return 0;
}


struct rfid_layer2 rfid_layer2_iso15693 = {
	.id	= RFID_LAYER2_ISO15693,
	.name 	= "ISO 15693",
	.fn	= {
		.init 		= &iso15693_init,
		//.open 		= &iso15693_anticol,
		//.transceive 	= &iso15693_transceive,
		//.close 		= &iso14443a_hlta,
		.fini 		= &iso15693_fini,
	},
};

