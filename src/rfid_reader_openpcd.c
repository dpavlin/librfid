/* OpenPCD specific RC632 transport layer 
 *
 * (C) 2006 by Harald Welte <laforge@gnumonks.org>
 *
 * The OpenPCD is an Atmel AT91SAM7Sxx based USB RFID reader.
 * It's CL RC632 is connected via SPI.  OpenPCD has multiple firmware
 * images.  This driver is for the "main_dumbreader" firmware.
 *
 * TODO:
 * - put hdl from static variable into asic transport or reader handle 
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

//#define DEBUG

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <usb.h>

#include <librfid/rfid.h>
#include <librfid/rfid_reader.h>
#include <librfid/rfid_asic.h>
#include <librfid/rfid_asic_rc632.h>
#include <librfid/rfid_reader_openpcd.h>
#include <librfid/rfid_layer2.h>
#include <librfid/rfid_protocol.h>

/* FIXME */
#include "rc632.h"

#define SENDBUF_LEN	(256+4+10) /* 256bytes max FSD/FSC, plus 4 bytes header,
				    plus 10 bytes reserve */
#define RECVBUF_LEN	SENDBUF_LEN

static char snd_buf[SENDBUF_LEN];
static char rcv_buf[RECVBUF_LEN];
static struct openpcd_hdr *snd_hdr;
static struct openpcd_hdr *rcv_hdr;


#ifndef LIBRFID_FIRMWARE

static struct usb_device *dev;
static struct usb_dev_handle *hdl;

static int openpcd_send_command(u_int8_t cmd, u_int8_t reg, u_int8_t val,
				u_int16_t len, const unsigned char *data)
{
	int ret;
	u_int16_t cur;

	snd_hdr->cmd = cmd;
	snd_hdr->reg = reg;
	snd_hdr->val = val;
	snd_hdr->flags = OPENPCD_FLAG_RESPOND;
	if (data && len)
		memcpy(snd_hdr->data, data, len);

	cur = sizeof(*snd_hdr) + len;

	return usb_bulk_write(hdl, OPENPCD_OUT_EP, (char *)snd_hdr, cur, 0);
}

static int openpcd_recv_reply(void)
{
	int ret;

	ret = usb_bulk_read(hdl, OPENPCD_IN_EP, rcv_buf, sizeof(rcv_buf), 1000);

	return ret;
}

static int openpcd_xcv(u_int8_t cmd, u_int8_t reg, u_int8_t val,
			u_int16_t len, const unsigned char *data)
{
	int ret;
	
	ret = openpcd_send_command(cmd, reg, val, len, data);
	if (ret < 0)
		return ret;
	if (ret < sizeof(sizeof(struct openpcd_hdr)))
		return -EINVAL;

	return openpcd_recv_reply();
}

struct usb_id {
	u_int16_t vid;
	u_int16_t pid;
};

static const struct usb_id opcd_usb_ids[] = {
	{ .vid = 0x2342, .pid = 0x0001 },	/* prototypes */
	{ .vid = 0x16c0, .pid = 0x076b },	/* first official device id */
};

static struct usb_device *find_opcd_device(void)
{
	struct usb_bus *bus;

	for (bus = usb_busses; bus; bus = bus->next) {
		struct usb_device *dev;
		for (dev = bus->devices; dev; dev = dev->next) {
			int i;
			for (i = 0; i < ARRAY_SIZE(opcd_usb_ids); i++) {
				const struct usb_id *id = &opcd_usb_ids[i];
				if (dev->descriptor.idVendor == id->vid &&
				    dev->descriptor.idProduct == id->pid)
					return dev;
			}
		}
	}
	return NULL;
}

/* RC632 access primitives for librfid inside reader firmware */

static int openpcd_reg_write(struct rfid_asic_transport_handle *rath,
			     unsigned char reg, unsigned char value)
{
	int ret;

	DEBUGP("reg=0x%02x, val=%02x: ", reg, value);

	ret = openpcd_xcv(OPENPCD_CMD_WRITE_REG, reg, value, 0, NULL);
	if (ret < 0)
		DEBUGPC("ERROR sending command\n");
	else
		DEBUGPC("OK\n");

	return ret;
}

static int openpcd_reg_read(struct rfid_asic_transport_handle *rath,
			    unsigned char reg,
			    unsigned char *value)
{
	int ret;	

	DEBUGP("reg=0x%02x, ", reg);

	ret = openpcd_xcv(OPENPCD_CMD_READ_REG, reg, 0, 0, NULL);
	if (ret < 0) {
		DEBUGPC("ERROR sending command\n");
		return ret;
	}

	if (ret < sizeof(struct openpcd_hdr)) {
		DEBUGPC("ERROR: short packet\n");
		return ret;
	}

	*value = rcv_hdr->val;
	DEBUGPC("val=%02x: OK\n", *value);

	return ret;
}

static int openpcd_fifo_read(struct rfid_asic_transport_handle *rath,
			     unsigned char num_bytes,
			     unsigned char *buf)
{
	int ret;

	DEBUGP(" ");

	ret = openpcd_xcv(OPENPCD_CMD_READ_FIFO, 0x00, num_bytes, 0, NULL);
	if (ret < 0) {
		DEBUGPC("ERROR sending command\n");
		return ret;
	}
	DEBUGPC("ret = %d\n", ret);

	memcpy(buf, rcv_hdr->data, ret - sizeof(struct openpcd_hdr));
	DEBUGPC("len=%d val=%s: OK\n", ret - sizeof(struct openpcd_hdr),
		rfid_hexdump(rcv_hdr->data, ret - sizeof(struct openpcd_hdr)));

	return ret;
}

static int openpcd_fifo_write(struct rfid_asic_transport_handle *rath,
			     unsigned char len,
			     const unsigned char *bytes,
			     unsigned char flags)
{
	int ret;

	DEBUGP("len=%u, data=%s\n", len, rfid_hexdump(bytes, len));
	ret = openpcd_xcv(OPENPCD_CMD_WRITE_FIFO, 0, 0, len, bytes);

	return ret;
}

const struct rfid_asic_transport openpcd_rat = {
	.name = "OpenPCD Dumb USB Protocol",
	.priv.rc632 = {
		.fn = {
			.reg_write 	= &openpcd_reg_write,
			.reg_read 	= &openpcd_reg_read,
			.fifo_write	= &openpcd_fifo_write,
			.fifo_read	= &openpcd_fifo_read,
		},
	},
};

#else
/* RC632 access primitives for librfid inside reader firmware */

static int openpcd_reg_write(struct rfid_asic_transport_handle *rath,
			     unsigned char reg, unsigned char value)
{
	return rc632_reg_write(rath, reg, value);
}

static int openpcd_reg_read(struct rfid_asic_transport_handle *rath,
			    unsigned char reg,
			    unsigned char *value)
{
	return rc632_reg_read(rath, reg, value);
}


static int openpcd_fifo_read(struct rfid_asic_transport_handle *rath,
			     unsigned char num_bytes,
			     unsigned char *buf)
{
	return rc632_fifo_read(rath, num_bytes, buf);
}

static int openpcd_fifo_write(struct rfid_asic_transport_handle *rath,
			     unsigned char len,
			     const unsigned char *bytes,
			     unsigned char flags)
{
	return rc632_fifo_write(rath, len, bytes, flags);
}

const struct rfid_asic_transport openpcd_rat = {
	.name = "OpenPCD Firmware RC632 Access",
	.priv.rc632 = {
		.fn = {
			.reg_write 	= &openpcd_reg_write,
			.reg_read 	= &openpcd_reg_read,
			.fifo_write	= &openpcd_fifo_write,
			.fifo_read	= &openpcd_fifo_read,
		},
	},
};

#endif /* LIBRFID_FIRMWARE */

static int openpcd_transceive(struct rfid_reader_handle *rh,
			     enum rfid_frametype frametype,
			     const unsigned char *tx_data, unsigned int tx_len,
			     unsigned char *rx_data, unsigned int *rx_len,
			     u_int64_t timeout, unsigned int flags)
{
	return rh->ah->asic->priv.rc632.fn.transceive(rh->ah, frametype,
						      tx_data, tx_len, 
						      rx_data, rx_len,
						      timeout, flags);
}

static int openpcd_transceive_sf(struct rfid_reader_handle *rh,
			       unsigned char cmd, struct iso14443a_atqa *atqa)
{
	return rh->ah->asic->priv.rc632.fn.iso14443a.transceive_sf(rh->ah,
								   cmd,
								   atqa);
}

static int
openpcd_transceive_acf(struct rfid_reader_handle *rh,
		      struct iso14443a_anticol_cmd *cmd,
		      unsigned int *bit_of_col)
{
	return rh->ah->asic->priv.rc632.fn.iso14443a.transceive_acf(rh->ah,
							 cmd, bit_of_col);
}

static int
openpcd_14443a_init(struct rfid_reader_handle *rh)
{
	return rh->ah->asic->priv.rc632.fn.iso14443a.init(rh->ah);
}

static int
openpcd_14443a_set_speed(struct rfid_reader_handle *rh, 
			unsigned int tx,
			unsigned int speed)
{
	u_int8_t rate;
	
	DEBUGP("setting rate: ");
	switch (speed) {
	case RFID_14443A_SPEED_106K:
		rate = 0x00;
		DEBUGPC("106K\n");
		break;
	case RFID_14443A_SPEED_212K:
		rate = 0x01;
		DEBUGPC("212K\n");
		break;
	case RFID_14443A_SPEED_424K:
		rate = 0x02;
		DEBUGPC("424K\n");
		break;
	case RFID_14443A_SPEED_848K:
		rate = 0x03;
		DEBUGPC("848K\n");
		break;
	default:
		return -EINVAL;
		break;
	}
	return rh->ah->asic->priv.rc632.fn.iso14443a.set_speed(rh->ah,
								tx, rate);
}

static int
openpcd_14443b_init(struct rfid_reader_handle *rh)
{
	return rh->ah->asic->priv.rc632.fn.iso14443b.init(rh->ah);
}

static int
openpcd_15693_init(struct rfid_reader_handle *rh)
{
	return rh->ah->asic->priv.rc632.fn.iso15693.init(rh->ah);
}

static int
openpcd_mifare_setkey(struct rfid_reader_handle *rh, const u_int8_t *key)
{
	return rh->ah->asic->priv.rc632.fn.mifare_classic.setkey(rh->ah, key);
}

static int
openpcd_mifare_auth(struct rfid_reader_handle *rh, u_int8_t cmd, 
		   u_int32_t serno, u_int8_t block)
{
	return rh->ah->asic->priv.rc632.fn.mifare_classic.auth(rh->ah, 
							cmd, serno, block);
}

static struct rfid_reader_handle *
openpcd_open(void *data)
{
	struct rfid_reader_handle *rh;
	struct rfid_asic_transport_handle *rath;

	snd_hdr = (struct openpcd_hdr *)snd_buf;
	rcv_hdr = (struct openpcd_hdr *)rcv_buf;

#ifndef LIBRFID_FIRMWARE
	usb_init();
	if (usb_find_busses() < 0)
		return NULL;
	if (usb_find_devices() < 0) 
		return NULL;
	
	dev = find_opcd_device();
	if (!dev) {
		DEBUGP("No matching USB device found\n");
		return NULL;
	}

	hdl = usb_open(dev);
	if (!hdl) {
		DEBUGP("Can't open USB device\n");
		return NULL;
	}

	if (usb_claim_interface(hdl, 0) < 0) {
		DEBUGP("Can't claim interface\n");
		usb_close(hdl);
		return NULL;
	}
#endif

	rh = malloc_reader_handle(sizeof(*rh));
	if (!rh)
		return NULL;
	memset(rh, 0, sizeof(*rh));

	rath = malloc_rat_handle(sizeof(*rath));
	if (!rath)
		goto out_rh;
	memset(rath, 0, sizeof(*rath));

	rath->rat = &openpcd_rat;
	rh->reader = &rfid_reader_openpcd;

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
openpcd_close(struct rfid_reader_handle *rh)
{
	struct rfid_asic_transport_handle *rath = rh->ah->rath;

	rc632_close(rh->ah);
	free_rat_handle(rath);
	free_reader_handle(rh);

#ifndef LIBRFID_FIRMWARE
	usb_close(hdl);
#endif
}

const struct rfid_reader rfid_reader_openpcd = {
	.name 	= "OpenPCD RFID Reader",
	.id = RFID_READER_OPENPCD,
	.open = &openpcd_open,
	.close = &openpcd_close,
	.transceive = &openpcd_transceive,
	.l2_supported = (1 << RFID_LAYER2_ISO14443A) |
			(1 << RFID_LAYER2_ISO14443B) |
			(1 << RFID_LAYER2_ISO15693),
	.proto_supported = (1 << RFID_PROTOCOL_TCL) |
			(1 << RFID_PROTOCOL_MIFARE_UL) |
			(1 << RFID_PROTOCOL_MIFARE_CLASSIC),
	.iso14443a = {
		.init = &openpcd_14443a_init,
		.transceive_sf = &openpcd_transceive_sf,
		.transceive_acf = &openpcd_transceive_acf,
		.speed = RFID_14443A_SPEED_106K | RFID_14443A_SPEED_212K |
			 RFID_14443A_SPEED_424K, //| RFID_14443A_SPEED_848K,
		.set_speed = &openpcd_14443a_set_speed,
	},
	.iso14443b = {
		.init = &openpcd_14443b_init,
	},
	.iso15693 = {
		.init = &openpcd_15693_init,
	},
	.mifare_classic = {
		.setkey = &openpcd_mifare_setkey,
		.auth = &openpcd_mifare_auth,
	},
};
