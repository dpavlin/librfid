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
//#define DEBUG_LIBRFID

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <librfid/rfid.h>
#include <librfid/rfid_layer2.h>
#include <librfid/rfid_reader.h>
#include <librfid/rfid_layer2_iso15693.h>

/*struct iso15693_request_read {
	struct iso15693_request head;
	u_int64_t uid;
	u_int8_t blocknum;
} __attribute__ ((packed));*/

struct iso15693_request_adressed {
	struct iso15693_request head;
	u_int64_t uid;
} __attribute__ ((packed));

struct iso15693_request_block_addressed {
	struct iso15693_request head;
	u_int64_t uid;
	u_int8_t blocknum;
	u_int8_t data[0];
} __attribute__ ((packed));

struct iso15693_request_block_selected {
	struct iso15693_request head;
	u_int8_t blocknum;
	u_int8_t data[0];
} __attribute__ ((packed));

struct iso15693_err_resp {
	struct iso15693_response head;
	u_int8_t error;
	unsigned char crc[2];
} __attribute__ ((packed));

struct iso15693_response_sec {
	struct iso15693_response head;
	u_int8_t block_sec;
	u_int8_t data[];
} __attribute__ ((packed));

#define ISO15693_BLOCK_SIZE_MAX	(256/8)
#define ISO15693_RESP_SIZE_MAX	(4+ISO15693_BLOCK_SIZE_MAX)

const unsigned int iso15693_timing[2][5] = {
	[ISO15693_T_SLOW] = {
		[ISO15693_T1]	= 1216,	/* max time after VCD EOF before VICC SOF */
		[ISO15693_T2]	= 1200,	/* min time before VCD EOF after VICC response */
		[ISO15693_T3]	= 1502,	/* min time after VCD EOF before next EOF if no VICC response */
		[ISO15693_T4]	= 1216,	/* time after wich VICC transmits after VCD EOF */
		[ISO15693_T4_WRITE]=20000,	/* time after wich VICC transmits after VCD EOF */
	},
	[ISO15693_T_FAST] = {
		[ISO15693_T1]	= 304,	/* max time after VCD EOF before VICC SOF */
		[ISO15693_T2]	= 300,	/* min time before VCD EOF after VICC response */
		[ISO15693_T3]	= 602,	/* min time after VCD EOF before next EOF if no VICC response */
		[ISO15693_T4]	= 304,	/* time after wich VICC transmits after VCD EOF */
		[ISO15693_T4_WRITE]=20000,	/* time after wich VICC transmits after VCD EOF */
	},
};

char *
iso15693_get_response_error_name(u_int8_t error){
	switch (error){
		case RFID_15693_ERR_NOTSUPP:
			return "ERR_NOTSUPP";
		case RFID_15693_ERR_INVALID: /* command not recognized */
			return "ERR_INVALID";
		case RFID_15693_ERR_UNKNOWN: /* unknown error */
			return "ERR_UNKNOWN";
        case RFID_15693_ERR_NOTSUPP_OPTION:
            return "ERR_NotSuppOpt";
		case RFID_15693_ERR_BLOCK_NA: /* block not available */
			return "ERR_BLOCK_N";
		case RFID_15693_ERR_BLOCK_LOCKED:
			return "ERR_BLOCK_LOCKE";
		case RFID_15693_ERR_BLOCK_LOCKED_CH:
			return "ERR_BLOCK_LOCKED_CH";
		case RFID_15693_ERR_BLOCK_NOTPROG:
			return "ERR_BLOCK_NOTPROG";
		case RFID_15693_ERR_BLOCK_NOTLOCK:
			return "ERR_BLOCK_NOTLOCK";
		case 0xA0: /* until 0xDF*/
			return "Custom Command error Code";
		case 0xE0:
		default:
			return "Undef.";
	}
}

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
			const struct iso15693_anticol_cmd *acf,
			unsigned int acf_len,
			struct iso15693_anticol_resp *resp,
			unsigned int *rx_len, char *bit_of_col)
{
	const struct rfid_reader *rdr = handle->rh->reader;
	if (!rdr->iso15693.transceive_ac)
		return -1;
	return rdr->iso15693.transceive_ac(handle->rh, acf, acf_len, resp, rx_len, bit_of_col);
}


int
iso15693_read_block(struct rfid_layer2_handle *handle,
		    u_int8_t blocknr, u_int32_t *data, unsigned int len, 
		    unsigned char *block_sec_out)
{
	union {
		struct iso15693_request_block_selected sel;
		struct iso15693_request_block_addressed addr;
	} tx_req;

	int ret;
	unsigned char *errstr;
	unsigned int rx_len, tx_len,timeout;
	u_int8_t resp[ISO15693_RESP_SIZE_MAX];
	struct iso15693_err_resp *rx_err;
	struct iso15693_response *rx_pkt;
	struct iso15693_response_sec *rx_pkt_sec;

	rx_pkt_sec = (struct iso15693_response_sec *)&resp[0];
	rx_pkt = (struct iso15693_response *)&resp[0];
	rx_err = (struct iso15693_err_resp *)&resp[0];

	memset(&tx_req,0,sizeof(tx_req));

	rx_len = sizeof(resp);

	tx_req.sel.head.command = ISO15693_CMD_READ_BLOCK_SINGLE;

	if (handle->priv.iso15693.vicc_fast){
		tx_req.sel.head.flags |= RFID_15693_F_RATE_HIGH;
		timeout=iso15693_timing[ISO15693_T_FAST][ISO15693_T4];
	}else
		timeout=iso15693_timing[ISO15693_T_SLOW][ISO15693_T4];

	if (handle->priv.iso15693.vicc_two_subc)
		tx_req.sel.head.flags |= RFID_15693_F_SUBC_TWO;

	if (block_sec_out!=NULL)
		tx_req.sel.head.flags |= RFID_15693_F4_CUSTOM;

	if (handle->priv.iso15693.state==RFID_15693_STATE_SELECTED) {
		tx_len = sizeof(struct iso15693_request_block_selected);
		tx_req.sel.blocknum = blocknr;
		tx_req.sel.head.flags |= RFID_15693_F4_SELECTED;
	} else {
		tx_len = sizeof(struct iso15693_request_block_addressed);
		memcpy(&tx_req.addr.uid, handle->uid, ISO15693_UID_LEN);
		tx_req.addr.head.flags |= RFID_15693_F4_ADDRESS;
		tx_req.addr.blocknum = blocknr;
	}

	//DEBUGP("sizeof: addr: %d sel:%d\n",sizeof(struct iso15693_request_read_addressed),sizeof(struct iso15693_request_read_selected));
	DEBUGP("tx_len=%u", tx_len); DEBUGPC(" rx_len=%u\n",rx_len);

	ret = iso15693_transceive(handle, RFID_15693_FRAME, (u_int8_t*)&tx_req,
				  tx_len, resp, &rx_len, timeout, 0);

	if (ret==-ETIMEDOUT)
		errstr="(TIMEOUT)";
	else if (ret==-EIO)
		errstr="(EIO)";
	else
		errstr="";
	DEBUGP("length: %d rx_len: %d ret: %d%s\n",len,rx_len,ret,errstr);

	if (ret < 0)
		return ret;

	if (rx_len > len+1)
		return -1;
	DEBUGP("error_flag: %d", rx_pkt->flags&RFID_15693_RF_ERROR);
	if (rx_pkt->flags & RFID_15693_RF_ERROR) {
		DEBUGPC(" -> error: %02x '%s'\n", rx_err->error,
			iso15693_get_response_error_name(rx_err->error));
		return -1;
	} else if (block_sec_out != NULL) {
		DEBUGPC(" block_sec_stat: 0x%02x\n",rx_pkt_sec->block_sec);
		memcpy(data, rx_pkt_sec->data, rx_len-2);
		return rx_len-2;
	} else {
		memcpy(data, rx_pkt->data, rx_len-1); /* FIXME rc-3 in case of CRC */
		return rx_len-1;
	}
}

int
iso15693_write_block(struct rfid_layer2_handle *handle,
		     u_int8_t blocknr, u_int32_t *data, unsigned int len)
{
    int ret;
	unsigned char *errstr;
	unsigned int rx_len, tx_len,timeout;

	union{
		struct iso15693_request_block_selected sel;
		struct iso15693_request_block_addressed addr;
        u_int32_t buf[sizeof(struct iso15693_request_block_addressed)+ISO15693_BLOCK_SIZE_MAX];
	} tx_req;

	u_int8_t resp[ISO15693_RESP_SIZE_MAX];
	struct iso15693_response *rx_pkt;
	struct iso15693_err_resp *rx_err;

	rx_pkt = (struct iso15693_response *)&resp[0];
	rx_err = (struct iso15693_err_resp *)&resp[0];
	rx_len = sizeof(resp);

	if (len > ISO15693_BLOCK_SIZE_MAX)
		return -1;

	//return -1;

	memset(&tx_req,0,sizeof(tx_req));
	tx_req.sel.head.command = ISO15693_CMD_WRITE_BLOCK_SINGLE;

	if (handle->priv.iso15693.vicc_fast) {
		tx_req.sel.head.flags |= RFID_15693_F_RATE_HIGH;
		timeout = iso15693_timing[ISO15693_T_FAST][ISO15693_T4_WRITE];
	} else
		timeout = iso15693_timing[ISO15693_T_SLOW][ISO15693_T4_WRITE];

	if (handle->priv.iso15693.vicc_two_subc)
		tx_req.sel.head.flags |= RFID_15693_F_SUBC_TWO;

	if (handle->priv.iso15693.state == RFID_15693_STATE_SELECTED) {
		tx_len=sizeof(struct iso15693_request_block_selected)+len;
		tx_req.sel.head.flags |= RFID_15693_F4_SELECTED;
		tx_req.sel.blocknum = blocknr;
		memcpy(&tx_req.sel.data,data,len);
	} else {
		memcpy(&tx_req.addr.uid, handle->uid, ISO15693_UID_LEN);
		tx_len=sizeof(struct iso15693_request_block_addressed)+len;
		tx_req.addr.head.flags |= RFID_15693_F4_ADDRESS;
		tx_req.addr.blocknum = blocknr;
		memcpy(&tx_req.addr.data,data,len);
	}

	//DEBUGP("sizeof: addr: %d sel:%d\n",sizeof(struct iso15693_request_read_addressed),sizeof(struct iso15693_request_read_selected));
	DEBUGP("tx_len=%u", tx_len); DEBUGPC(" rx_len=%u\n",rx_len);

	ret = iso15693_transceive(handle, RFID_15693_FRAME, (u_int8_t*)&tx_req,
				  tx_len, resp, &rx_len, timeout, 0);

	if (ret == -ETIMEDOUT)
		errstr = "(TIMEOUT)";
	else if (ret == -EIO)
		errstr = "(EIO)";
	else
		errstr = "";
	DEBUGP("length: %d rx_len: %d ret: %d%s\n",len,rx_len,ret,errstr);

	if (ret < 0)
		return ret;

	if (rx_len > len+1)
		return -1;
	DEBUGP("error_flag: %d", rx_pkt->flags & RFID_15693_RF_ERROR);
	if (rx_pkt->flags & RFID_15693_RF_ERROR) {
		DEBUGPC(" -> error: %02x '%s'\n", rx_err->error,
			iso15693_get_response_error_name(rx_err->error));
		return -1;
	} else {
		//DEBUGPC(" block_sec_stat: 0x%02x\n",rx_pkt->data[0]);
		//memcpy(data, rx_pkt->data, rx_len-1); /* FIXME rc-3 in case of CRC */
		//return rx_len-1;
        return 0;
	}

}


#if 0

static int
iso15693_lock_block()
{
}

#endif

/* Helper function to build an ISO 15693 anti collision frame */
static int
iso15693_build_acf(u_int8_t *target, u_int8_t flags, u_int8_t afi,
		   u_int8_t mask_len, u_int8_t *mask)
{
	struct iso15693_request *req = (struct iso15693_request *) target;
	int i = 0, j, mask_bytes;
	u_int8_t byte=0;
	void* mask_p;

	req->flags = flags;
	req->command = ISO15693_CMD_INVENTORY;
	if (flags & RFID_15693_F5_AFI_PRES)
		req->data[i++] = afi;
	req->data[i++] = mask_len;

	mask_bytes = mask_len/8 + (mask_len%8)?1:0;
	mask_p = &req->data[i];

	for (j = 0; j < mask_bytes; j++)
		req->data[i++] = mask[j];

	byte = 0xFF >> (8-mask_len%8);
	req->data[i-1] &= byte;

	DEBUGP("mask_len: %d mask_bytes: %d i: %d return: %d mask:%s\n",
		mask_len,mask_bytes,i,i + sizeof(*req),rfid_hexdump(mask_p,mask_bytes));
	return i + sizeof(*req);
}

static int
iso15693_anticol(struct rfid_layer2_handle *handle)
{
	int i, ret, mask_len;
	int tx_len, rx_len;
	int num_valid = 0;
	union {
		struct iso15693_anticol_cmd_afi w_afi;
		struct iso15693_anticol_cmd no_afi;
	} acf;

	struct iso15693_anticol_resp resp;

	u_int8_t boc;
#define MAX_SLOTS 16
	int num_slots = MAX_SLOTS;

	u_int8_t uuid_list[MAX_SLOTS][ISO15693_UID_LEN];
	int uuid_list_valid[MAX_SLOTS];

	u_int8_t flags;

#define MY_NONE 0
#define MY_COLL 1
#define MY_UUID 2

	memset(uuid_list_valid, MY_NONE, sizeof(uuid_list_valid));
	memset(uuid_list, 0, sizeof(uuid_list));

	//memset(&acf, 0, sizeof(acf));

	/* FIXME: we can't use multiple slots at this point, since the RC632
	 * with librfid on the host PC has too much latency between 'EOF pulse
	 * to mark start of next slot' and 'receive data' commands :( */

	flags = RFID_15693_F_INV_TABLE_5;
	if (handle->priv.iso15693.vicc_fast)
		flags |= RFID_15693_F_RATE_HIGH;
	if (handle->priv.iso15693.vicc_two_subc)
		flags |= RFID_15693_F_SUBC_TWO;
	if (handle->priv.iso15693.single_slot) {
		flags |= RFID_15693_F5_NSLOTS_1;
		num_slots = 1;
	}
	if (handle->priv.iso15693.use_afi)
		flags |= RFID_15693_F5_AFI_PRES;
#if 1
	tx_len = iso15693_build_acf((u_int8_t *)&acf, flags,
				    handle->priv.iso15693.afi, 0, NULL);
#else
	/*FIXME: testcode*/
	u_int8_t uid[8]={0x1f, 0x1e, 0x95, 0x01, 0x00, 0x01, 0x04, 0xe0};
	//u_int8_t uid[8]={0xe3, 0xe8, 0xf1, 0x01, 0x00, 0x00, 0x07, 0xe0};
	tx_len = iso15693_build_acf((u_int8_t *)&acf, flags,
				    handle->priv.iso15693.afi, 2, uid);
#endif
start_of_ac_loop:
	for (i = 0; i < num_slots; i++) {
		rx_len = sizeof(resp);
		memset(&resp, 0, rx_len);
		ret = iso15693_transceive_acf(handle, 
					      (struct iso15693_anticol_cmd *) &acf,
					      tx_len, &resp, &rx_len, &boc);

		if (ret == -ETIMEDOUT) {
			//DEBUGP("no answer from vicc in slot %d\n", i);
			DEBUGP("slot[%d]: timeout\n",i);
			uuid_list_valid[i] = MY_NONE;
		} else if (ret < 0) {
			DEBUGP("slot[%d]: ERROR ret: %d\n", i, ret);
			uuid_list_valid[i] = MY_NONE;
		} else {
			if (ret)
				DEBUGP("iso15693_transceive_acf() ret: %d\n",ret);
			if (boc) {
				DEBUGP("slot[%d]: Collision! bit:%d byte:%d,%d (UID bit:%d byte:%d,%d)\n",
					i, boc,boc/8,boc%8,
					boc-16,(boc-16)/8,(boc-16)%8);
				DEBUGP("Slot[%d]: ret: %d DSFID: %02x UUID: %s\n", i, ret,
					resp.dsfid, rfid_hexdump(resp.uuid, ISO15693_UID_LEN));

				uuid_list_valid[i]=-boc;
				memcpy(uuid_list[i], resp.uuid, ISO15693_UID_LEN);
			} else {
				DEBUGP("Slot[%d]: ret: %d DSFID: %02x UUID: %s\n", i, ret,
					resp.dsfid, rfid_hexdump(resp.uuid, ISO15693_UID_LEN));
				uuid_list_valid[i] = MY_UUID;
				memcpy(&uuid_list[i][0], resp.uuid, ISO15693_UID_LEN);

				memcpy(handle->uid,resp.uuid, ISO15693_UID_LEN);
				/* FIXME: move to init_iso15693 */
				handle->uid_len = ISO15693_UID_LEN;
				return 1;
			}
		}
	}


	for (i = 0; i < num_slots; i++) {
		if (uuid_list_valid[i] < 0) {
			boc=uuid_list_valid[i]*-1;
			if (boc>16){
				boc=boc-16;
			}
			else
				DEBUGP("slot[%d]:boc is smaller than 2 bytes (collision before uid)!!!!\n",i);

			if (boc<65){
				tx_len = iso15693_build_acf((u_int8_t *)&acf, flags,
				    handle->priv.iso15693.afi, boc+1,  resp.uuid);
				boc=0;
				// FIXME: dont use goto
				goto start_of_ac_loop;
			}else{
				DEBUGP("slot[%d]:boc is bigger than 64 (uid size)(collision after uid)\n",i);
				memcpy(handle->uid,uuid_list[i],ISO15693_UID_LEN);

				/* FIXME: move to init_iso15693 */
				handle->uid_len = ISO15693_UID_LEN;
				return 1;
			}
		}
	}
#if 0
	for (i = 0; i < num_slots; i++) {
		if (uuid_list_valid[i] == MY_NONE) {
			DEBUGP("slot[%d]: timeout\n",i);
		} else if (uuid_list_valid[i] == MY_UUID) {
			DEBUGP("slot[%d]: VALID uuid: %s\n", i,
				rfid_hexdump(uuid_list[i], ISO15693_UID_LEN));
			memcpy(handle->uid, uuid_list[i], ISO15693_UID_LEN);
			/* FIXME: move to init_iso15693 */
			handle->uid_len = ISO15693_UID_LEN;
			num_valid++;
		} else if (uuid_list_valid[i] < 0) {
 				if (boc>16){
 					boc=boc-16;
 				}
 				else
 					DEBUGP("boc is smaller than 2 bytes (collision before uid)!!!!\n");

 				uuid_list_valid[i] = -boc;
 				if (boc<65){
 					tx_len = iso15693_build_acf((u_int8_t *)&acf, flags,
 					    handle->priv.iso15693.afi, boc+1,  resp.uuid);
 					boc=0;
 					// FIXME: dont use goto
 					goto start_of_ac_loop;
 				}else{
 					DEBUGP("boc is bigger than 64 (uid size)\n");
 					uuid_list_valid[i] = MY_UUID;
 				}
		}
	}
#endif
	if (num_valid == 0)
		return -1;

	return num_valid;
}

int
iso15693_select(struct rfid_layer2_handle *l2h)
{
	struct iso15693_request_adressed tx_req;
	int ret;
	unsigned int rx_len, tx_len, timeout;

	struct {
		struct iso15693_response head;
		u_int8_t error;
		unsigned char crc[2];
	} rx_buf;
	rx_len = sizeof(rx_buf);

	if (l2h->priv.iso15693.vicc_fast) {
		tx_req.head.flags |= RFID_15693_F_RATE_HIGH;
		timeout = iso15693_timing[ISO15693_T_FAST][ISO15693_T4];
	} else
		timeout = iso15693_timing[ISO15693_T_SLOW][ISO15693_T4];

	tx_req.head.command = ISO15693_CMD_SELECT;
	tx_req.head.flags = RFID_15693_F4_ADDRESS;

	if (l2h->priv.iso15693.vicc_fast)
		tx_req.head.flags |= RFID_15693_F_RATE_HIGH;
	if (l2h->priv.iso15693.vicc_two_subc)
		tx_req.head.flags |= RFID_15693_F_SUBC_TWO;

	memcpy(&tx_req.uid, l2h->uid, ISO15693_UID_LEN);
	tx_len = sizeof(tx_req);

	DEBUGP("tx_len=%u, rx_len=%u\n", tx_len,rx_len);

	ret = iso15693_transceive(l2h, RFID_15693_FRAME, (u_int8_t*)&tx_req,
				  tx_len, (u_int8_t*)&rx_buf, &rx_len,timeout ,0);

	DEBUGP("ret: %d%s, rx_len: %d, error_flag: %d", ret,
		(ret==-ETIMEDOUT)?"(TIMEOUT)":"", rx_len,
		rx_buf.head.flags&RFID_15693_RF_ERROR);
	if (rx_buf.head.flags & RFID_15693_RF_ERROR) {
		DEBUGPC(" -> error: %02x '%s'\n", rx_buf.error,
			iso15693_get_response_error_name(rx_buf.error));
		return -1;
	} else {
		DEBUGPC(" SELECTED\n");
		l2h->priv.iso15693.state = RFID_15693_STATE_SELECTED;
		return 0;
	}
}

static int
iso15693_stay_quiet(struct rfid_layer2_handle *l2h)
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

	tx_req.head.command = ISO15693_CMD_STAY_QUIET;

	tx_req.head.flags = RFID_15693_F4_ADDRESS;
	if (l2h->priv.iso15693.vicc_fast)
		tx_req.head.flags |= RFID_15693_F_RATE_HIGH;
	if (l2h->priv.iso15693.vicc_two_subc)
		tx_req.head.flags |= RFID_15693_F_SUBC_TWO;
	memcpy(&tx_req.uid, l2h->uid, ISO15693_UID_LEN);
	tx_len = sizeof(tx_req);

	DEBUGP("tx_len=%u", tx_len); DEBUGPC(" rx_len=%u\n",rx_len);

	ret = iso15693_transceive(l2h, RFID_15693_FRAME, (u_int8_t*)&tx_req,
				  tx_len, (u_int8_t*)&rx_buf, &rx_len, 30,0);

	l2h->priv.iso15693.state = RFID_15693_STATE_QUIET;

	DEBUGP("ret: %d%s, error_flag: %d", ret,(ret==-ETIMEDOUT)?"(TIMEOUT)":"",
			rx_buf.head.flags&RFID_15693_RF_ERROR);
	if (rx_buf.head.flags&RFID_15693_RF_ERROR)
		DEBUGPC(" -> error: %02x\n", rx_buf.error);
	else
		DEBUGPC("\n");

	return 0;
}

static int
iso15693_getopt(struct rfid_layer2_handle *handle,
		int optname, void *optval, unsigned int *optlen)
{
	unsigned int *val = optval;
	u_int8_t *val_u8 = optval;

	if (!optlen || !optval || *optlen < sizeof(unsigned int))
		return -EINVAL;

	*optlen = sizeof(unsigned int);

	switch (optname) {
	case RFID_OPT_15693_MOD_DEPTH:
		if (handle->priv.iso15693.vcd_ask100)
			*val = RFID_15693_MOD_100ASK;
		else
			*val = RFID_15693_MOD_10ASK;
		break;
	case RFID_OPT_15693_VCD_CODING:
		if (handle->priv.iso15693.vcd_out256)
			*val = RFID_15693_VCD_CODING_1OUT256;
		else
			*val = RFID_15693_VCD_CODING_1OUT4;
		break;
	case RFID_OPT_15693_VICC_SUBC:
		if (handle->priv.iso15693.vicc_two_subc)
			*val = RFID_15693_VICC_SUBC_DUAL;
		else
			*val = RFID_15693_VICC_SUBC_SINGLE;
		break;
	case RFID_OPT_15693_VICC_SPEED:
		if (handle->priv.iso15693.vicc_fast)
			*val = RFID_15693_VICC_SPEED_FAST;
		else
			*val = RFID_15693_VICC_SPEED_SLOW;
		break;
	case RFID_OPT_15693_VCD_SLOTS:
		if (handle->priv.iso15693.single_slot)
			*val = 1;
		else
			*val = 16;
		break;
	case RFID_OPT_15693_USE_AFI:
		if (handle->priv.iso15693.use_afi)
			*val = 1;
		else
			*val = 0;
		break;
	case RFID_OPT_15693_AFI:
		*val_u8 = handle->priv.iso15693.afi;
		*optlen = sizeof(u_int8_t);
		break;
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
	unsigned int val;

	if (optlen < sizeof(u_int8_t) || !optval)
		return -EINVAL;

	if (optlen == sizeof(u_int8_t))
		val = *((u_int8_t *) optval);
	if (optlen == sizeof(u_int16_t))
		val = *((u_int16_t *) optval);
	if (optlen == sizeof(unsigned int))
		val = *((unsigned int *) optval);

	switch (optname) {
	case RFID_OPT_15693_MOD_DEPTH:
		switch (val) {
		case RFID_15693_MOD_10ASK:
			handle->priv.iso15693.vcd_ask100 = 0;
			break;
		case RFID_15693_MOD_100ASK:
			handle->priv.iso15693.vcd_ask100 = 1;
			break;
		default:
			return -EINVAL;
		}
		break;
	case RFID_OPT_15693_VCD_CODING:
		switch (val) {
		case RFID_15693_VCD_CODING_1OUT256:
			handle->priv.iso15693.vcd_out256 = 1;
			break;
		case RFID_15693_VCD_CODING_1OUT4:
			handle->priv.iso15693.vcd_out256 = 0;
			break;
		default:
			return -EINVAL;
		}
		break;
	case RFID_OPT_15693_VICC_SUBC:
		switch (val) {
		case RFID_15693_VICC_SUBC_SINGLE:
			handle->priv.iso15693.vicc_two_subc = 0;
			break;
		case RFID_15693_VICC_SUBC_DUAL:
			handle->priv.iso15693.vicc_two_subc = 1;
			break;
		default:
			return -EINVAL;
		}
		break;
	case RFID_OPT_15693_VICC_SPEED:
		switch (val) {
		case RFID_15693_VICC_SPEED_SLOW:
			handle->priv.iso15693.vicc_fast = 0;
			break;
		case RFID_15693_VICC_SPEED_FAST:
			handle->priv.iso15693.vicc_fast = 1;
			break;
		default:
			return -EINVAL;
		}
	case RFID_OPT_15693_VCD_SLOTS:
		switch (val) {
		case 16:
			handle->priv.iso15693.single_slot = 0;
			break;
		case 1:
			handle->priv.iso15693.single_slot = 1;
			break;
		default:
			return -EINVAL;
		}
		break;
	case RFID_OPT_15693_USE_AFI:
		if (val)
			handle->priv.iso15693.use_afi = 1;
		else
			handle->priv.iso15693.use_afi = 0;
		break;
	case RFID_OPT_15693_AFI:
		if (val > 0xff)
			return -EINVAL;
		handle->priv.iso15693.afi = val;
		break;
	default:
		return -EINVAL;
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
	h->priv.iso15693.vcd_ask100 = 1; /* 100ASK is easier to generate */
	h->priv.iso15693.vicc_two_subc = 0;
	h->priv.iso15693.vicc_fast = 1;
	h->priv.iso15693.single_slot = 1;
	h->priv.iso15693.vcd_out256 = 0;
	h->priv.iso15693.use_afi = 0;	/* not all VICC support AFI */
	h->priv.iso15693.afi = 0;

	ret = h->rh->reader->init(h->rh, RFID_LAYER2_ISO15693);
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
		.close 		= &iso15693_stay_quiet,
		.fini 		= &iso15693_fini,
		.setopt		= &iso15693_setopt,
		.getopt		= &iso15693_getopt,
	},
};

