/* Omnikey CardMan 5121 specific RC632 transport layer 
 *
 * (C) 2005-2008 by Harald Welte <laforge@gnumonks.org>
 *
 * The 5121 is an Atmel AT89C5122 based USB CCID reader (probably the same
 * design like the 3121).  It's CL RC632 is connected via address/data bus,
 * not via SPI.
 *
 * The vendor-supplied reader firmware provides some undocumented extensions 
 * to CCID (via PC_to_RDR_Escape) that allow access to registers and FIFO of
 * the RC632.
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

#ifndef LIBRFID_FIRMWARE


#include <librfid/rfid_reader.h>
#include <librfid/rfid_asic.h>
#include <librfid/rfid_asic_rc632.h>
#include <librfid/rfid_reader_cm5121.h>
#include <librfid/rfid_layer2.h>
#include <librfid/rfid_protocol.h>

#include "rfid_reader_rc632_common.h"

#include "cm5121_source.h"

/* FIXME */
#include "rc632.h"

#define SENDBUF_LEN	256+7+10 /* 256bytes max FSD/FSC, plus 7 bytes header,
				    plus 10 bytes reserve */
#define RECVBUF_LEN	SENDBUF_LEN

#ifdef DEBUG_REGISTER
#define DEBUGRC DEBUGPC
#define DEBUGR DEBUGP
#else
#define DEBUGRC(x, args ...)	do {} while(0)
#define DEBUGR(x, args ...)	do {} while(0)
#endif

static
int Write1ByteToReg(struct rfid_asic_transport_handle *rath,
		    unsigned char reg, unsigned char value)
{
	unsigned char sndbuf[SENDBUF_LEN];
	unsigned char rcvbuf[RECVBUF_LEN];
	size_t retlen = RECVBUF_LEN;

	sndbuf[0] = 0x20;
	sndbuf[1] = 0x00;
	sndbuf[2] = 0x01;
	sndbuf[3] = 0x00;
	sndbuf[4] = 0x00;
	sndbuf[5] = 0x00;
	sndbuf[6] = reg;
	sndbuf[7] = value;

	DEBUGR("reg=0x%02x, val=%02x: ", reg, value);

	if (PC_to_RDR_Escape(rath->data, sndbuf, 8, rcvbuf, 
			     &retlen) == 0) {
		DEBUGRC("OK\n");
		return 0;
	}

	DEBUGRC("ERROR\n");
	return -1;
}

static int Read1ByteFromReg(struct rfid_asic_transport_handle *rath,
			    unsigned char reg,
			    unsigned char *value)
{
	unsigned char sndbuf[SENDBUF_LEN];
	unsigned char recvbuf[RECVBUF_LEN];
	size_t retlen = sizeof(recvbuf);

	sndbuf[0] = 0x20;
	sndbuf[1] = 0x00;
	sndbuf[2] = 0x00;
	sndbuf[3] = 0x00;
	sndbuf[4] = 0x01;
	sndbuf[5] = 0x00;
	sndbuf[6] = reg;

	if (PC_to_RDR_Escape(rath->data, sndbuf, 7, recvbuf, 
			     &retlen) == 0) {
		*value = recvbuf[1];
		DEBUGR("reg=0x%02x, val=%02x: ", reg, *value);
		DEBUGRC("OK\n");
		return 0;
	}

	DEBUGRC("ERROR\n");
	return -1;
}

static int ReadNBytesFromFIFO(struct rfid_asic_transport_handle *rath,
			      unsigned char num_bytes,
			      unsigned char *buf)
{
	unsigned char sndbuf[SENDBUF_LEN];
	unsigned char recvbuf[0x7f];
	size_t retlen = sizeof(recvbuf);

	sndbuf[0] = 0x20;
	sndbuf[1] = 0x00;
	sndbuf[2] = 0x00;
	sndbuf[3] = 0x00;
	sndbuf[4] = num_bytes;
	sndbuf[5] = 0x00;
	sndbuf[6] = 0x02;

	DEBUGR("num_bytes=%u: ", num_bytes);
	if (PC_to_RDR_Escape(rath->data, sndbuf, 7, recvbuf, &retlen) == 0) {
		DEBUGRC("%u [%s]\n", retlen,
			rfid_hexdump(recvbuf+1, num_bytes));
		memcpy(buf, recvbuf+1, num_bytes); // len == 0x7f
		return 0;
	}

	DEBUGRC("ERROR\n");
	return -1;
}

static int WriteNBytesToFIFO(struct rfid_asic_transport_handle *rath,
			     unsigned char len,
			     const unsigned char *bytes,
			     unsigned char flags)
{
	unsigned char sndbuf[SENDBUF_LEN];
	unsigned char recvbuf[0x7f];
	size_t retlen = sizeof(recvbuf);

	sndbuf[0] = 0x20;
	sndbuf[1] = 0x00;
	sndbuf[2] = len;
	sndbuf[3] = 0x00;
	sndbuf[4] = 0x00;
	sndbuf[5] = flags;
	sndbuf[6] = 0x02;

	DEBUGR("%u [%s]: ", len, rfid_hexdump(bytes, len));

	memcpy(sndbuf+7, bytes, len);

	if (PC_to_RDR_Escape(rath->data, sndbuf, len+7, recvbuf, &retlen) == 0) {
		DEBUGRC("OK (%u [%s])\n", retlen, rfid_hexdump(recvbuf, retlen));
		return 0;
	}

	DEBUGRC("ERROR\n");
	return -1;
}

struct rfid_asic_transport cm5121_ccid = {
	.name = "CM5121 OpenCT",
	.priv.rc632 = {
		.fn = {
			.reg_write 	= &Write1ByteToReg,
			.reg_read 	= &Read1ByteFromReg,
			.fifo_write	= &WriteNBytesToFIFO,
			.fifo_read	= &ReadNBytesFromFIFO,
		},
	},
};

static int cm5121_enable_rc632(struct rfid_asic_transport_handle *rath)
{
	unsigned char tx_buf[1] = { 0x01 };	
	unsigned char rx_buf[64];
	size_t rx_len = sizeof(rx_buf);

	PC_to_RDR_Escape(rath->data, tx_buf, 1, rx_buf, &rx_len);

	return 0;
}

static struct rfid_reader_handle *
cm5121_open(void *data)
{
	struct rfid_reader_handle *rh;
	struct rfid_asic_transport_handle *rath;

	rh = malloc_reader_handle(sizeof(*rh));
	if (!rh)
		return NULL;
	memset(rh, 0, sizeof(*rh));

	rath = malloc_rat_handle(sizeof(*rath));
	if (!rath)
		goto out_rh;
	memset(rath, 0, sizeof(*rath));

	rath->rat = &cm5121_ccid;
	rh->reader = &rfid_reader_cm5121;

	if (cm5121_source_init(rath) < 0)
		goto out_rath;

	if (cm5121_enable_rc632(rath) < 0)
		goto out_rath;

	rh->ah = rc632_open(rath);
	if (!rh->ah) 
		goto out_rath;

	DEBUGP("returning %p\n", rh);
	return rh;

out_rath:
	free_rat_handle(rath);
out_rh:
	free_reader_handle(rh);

	return NULL;
}

static void
cm5121_close(struct rfid_reader_handle *rh)
{
	struct rfid_asic_transport_handle *rath = rh->ah->rath;
	rc632_close(rh->ah);
	free_rat_handle(rath);
	free_reader_handle(rh);
}

const struct rfid_reader rfid_reader_cm5121 = {
	.name 	= "Omnikey CardMan 5121 RFID",
	.open = &cm5121_open,
	.close = &cm5121_close,
	.l2_supported = (1 << RFID_LAYER2_ISO14443A) |
			(1 << RFID_LAYER2_ISO14443B) |
			(1 << RFID_LAYER2_ISO15693),
	.proto_supported = (1 << RFID_PROTOCOL_TCL) |
			(1 << RFID_PROTOCOL_MIFARE_UL) |
			(1 << RFID_PROTOCOL_MIFARE_CLASSIC),
	.getopt = &_rdr_rc632_getopt,
	.setopt = &_rdr_rc632_setopt,
	.transceive = &_rdr_rc632_transceive,
	.init = &_rdr_rc632_l2_init,
	.iso14443a = {
		.transceive_sf = &_rdr_rc632_transceive_sf,
		.transceive_acf = &_rdr_rc632_transceive_acf,
		.speed = RFID_14443A_SPEED_106K | RFID_14443A_SPEED_212K |
			 RFID_14443A_SPEED_424K, //| RFID_14443A_SPEED_848K,
		.set_speed = &_rdr_rc632_14443a_set_speed,
	},
	.iso15693 = {
		.transceive_ac = &_rdr_rc632_iso15693_transceive_ac,
	},
	.mifare_classic = {
		.setkey = &_rdr_rc632_mifare_setkey,
		.setkey_ee = &_rdr_rc632_mifare_setkey_ee,
		.auth = &_rdr_rc632_mifare_auth,
	},
};

#endif /* LIBRFID_FIRMWARE */
