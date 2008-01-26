/* Direct spi for RC632 transport layer
 * (based on openpcd reader)
 *
 * (C) 2007 by Frederic RODO <f.rodo@til-technologies.fr>
 *
 * This reader use the Linux's spidev interface, so it need a least
 * kernel 2.6.22
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

#include <librfid/rfid.h>
#include <librfid/rfid_reader.h>
#include <librfid/rfid_asic.h>
#include <librfid/rfid_asic_rc632.h>
#include <librfid/rfid_reader_spidev.h>
#include <librfid/rfid_layer2.h>
#include <librfid/rfid_protocol.h>

/* FIXME */
#include "rc632.h"
static int spidev_fd;

struct spi_ioc_transfer xfer[1];

/* 256bytes max FSD/FSC, plus 1 bytes header, plus 10 bytes reserve */
#define SENDBUF_LEN     (256+1+10)
#define RECVBUF_LEN     SENDBUF_LEN
static char snd_buf[SENDBUF_LEN];
static char rcv_buf[RECVBUF_LEN];

static int spidev_read(unsigned char reg, unsigned char len,
		       unsigned char *buf)
{
	int ret;

	if (!len)
		return -EINVAL;

	snd_buf[0] = (reg<<1) | 0x80;
	if (len > 1)
		memset(&snd_buf[1], reg<<1 , len-1);
	snd_buf[len] = 0;

	/* prepare spi buffer */
	xfer[0].tx_buf = (__u64) snd_buf;
	xfer[0].rx_buf = (__u64) rcv_buf;
	xfer[0].len = len + 1;

	ret = ioctl(spidev_fd, SPI_IOC_MESSAGE(1), xfer);
	if (ret < 0) {
		DEBUGPC("ERROR sending command\n");
		return ret;
	} else if (ret != (len + 1)) {
		DEBUGPC("ERROR sending command bad length\n");
		return -EINVAL;
	}

	memcpy(buf, &rcv_buf[1], len);

	return len;
}

static int spidev_write(unsigned char reg, unsigned char len,
		       const unsigned char *buf)
{
	int ret;

	if (!len)
		return -EINVAL;

	snd_buf[0] = (reg << 1) & 0x7E;
	memcpy(&snd_buf[1], buf, len);

	/* prepare spi buffer */
	xfer[0].tx_buf = (__u64) snd_buf;
	xfer[0].rx_buf = (__u64) NULL;
	xfer[0].len = len + 1;

	ret = ioctl(spidev_fd, SPI_IOC_MESSAGE(1), xfer);
        if (ret < 0) {
		DEBUGPC("ERROR sending command\n");
		return ret;
	}
	else if (ret != len+1)
		return -EINVAL;

	return len;
}

static int spidev_reg_read(struct rfid_asic_transport_handle *rath,
			   unsigned char reg, unsigned char *value)
{
	int ret;

	ret = spidev_read(reg, 1, value);
	if (ret < 0)
		return ret;
	DEBUGP("%s reg = 0x%02x, val = 0x%02x\n", __FUNCTION__, reg, *value);

	return 1;
}

static int spidev_reg_write(struct rfid_asic_transport_handle *rath,
		            unsigned char reg, unsigned char value)
{
	int ret;

	ret = spidev_write(reg, 1, &value);
        if (ret < 0)
		return ret;

	DEBUGP("%s reg = 0x%02x, val = 0x%02x\n", __FUNCTION__, reg, value);

	return 1;
}

static int spidev_fifo_read(struct rfid_asic_transport_handle *rath,
			    unsigned char len, unsigned char *buf)
{
	int ret;

	ret = spidev_read(2, len, buf);
	if (ret < 0)
		return ret;

	DEBUGP("%s len=%u, val=%s\n", __FUNCTION__, len,
	       rfid_hexdump(buf, len));

	return len;
}

static int spidev_fifo_write(struct rfid_asic_transport_handle *rath,
			     unsigned char len, const unsigned char *buf,
			     unsigned char flags)
{
	int ret;

	ret = spidev_write(2, len, buf);
        if (ret < 0)
		return ret;

	DEBUGP("%s len=%u, data=%s\n", __FUNCTION__, len,
	       rfid_hexdump(buf, len));

	return len;
}

struct rfid_asic_transport spidev_spi = {
	.name = "spidev",
	.priv.rc632 = {
		       .fn = {
			      .reg_write = &spidev_reg_write,
			      .reg_read = &spidev_reg_read,
			      .fifo_write = &spidev_fifo_write,
			      .fifo_read = &spidev_fifo_read,
			      },
		       },
};

static int spidev_transceive(struct rfid_reader_handle *rh,
			       enum rfid_frametype frametype,
			       const unsigned char *tx_data,
			       unsigned int tx_len, unsigned char *rx_data,
			       unsigned int *rx_len, u_int64_t timeout,
			       unsigned int flags)
{
	return rh->ah->asic->priv.rc632.fn.transceive(rh->ah, frametype,
						      tx_data, tx_len, rx_data,
						      rx_len, timeout, flags);
}

static int spidev_transceive_sf(struct rfid_reader_handle *rh,
				  unsigned char cmd,
				  struct iso14443a_atqa *atqa)
{
	return rh->ah->asic->priv.rc632.fn.iso14443a.transceive_sf(rh->ah, cmd,
								   atqa);
}

static int
spidev_transceive_acf(struct rfid_reader_handle *rh,
			struct iso14443a_anticol_cmd *cmd,
			unsigned int *bit_of_col)
{
	return rh->ah->asic->priv.rc632.fn.iso14443a.transceive_acf(rh->ah,
								    cmd,
								    bit_of_col);
}

static int spidev_14443a_init(struct rfid_reader_handle *rh)
{
	int ret;
	ret = rh->ah->asic->priv.rc632.fn.iso14443a.init(rh->ah);
	return ret;
}

static int
spidev_14443a_set_speed(struct rfid_reader_handle *rh,
			  unsigned int tx, unsigned int speed)
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

static int spidev_14443b_init(struct rfid_reader_handle *rh)
{
	return rh->ah->asic->priv.rc632.fn.iso14443b.init(rh->ah);
}

static int spidev_15693_init(struct rfid_reader_handle *rh)
{
	return rh->ah->asic->priv.rc632.fn.iso15693.init(rh->ah);
}

static int
spidev_mifare_setkey(struct rfid_reader_handle *rh, const u_int8_t * key)
{
	return rh->ah->asic->priv.rc632.fn.mifare_classic.setkey(rh->ah, key);
}

static int
spidev_mifare_auth(struct rfid_reader_handle *rh, u_int8_t cmd,
		     u_int32_t serno, u_int8_t block)
{
	return rh->ah->asic->priv.rc632.fn.mifare_classic.auth(rh->ah,
							       cmd, serno,
							       block);
}

static struct rfid_reader_handle *spidev_open(void *data)
{
	struct rfid_reader_handle *rh;
	struct rfid_asic_transport_handle *rath;
	__u32 tmp;

	/* open spi device */
	if (!data) {
		DEBUGP("No device name\n");
		return NULL;
	}
	if ((spidev_fd = open(data, O_RDWR)) < 0) {
		DEBUGP("Unable to open:\n");
		return NULL;
	}

	rh = malloc(sizeof(*rh));
	if (!rh)
		goto out_close_spi;

	memset(rh, 0, sizeof(*rh));

	rath = malloc(sizeof(*rath));
	if (!rath)
		goto out_rh;
	memset(rath, 0, sizeof(*rath));

	rath->rat = &spidev_spi;
	rh->reader = &rfid_reader_spidev;

	/* Configure spi device, MODE 0 */
	tmp = SPI_MODE_0;
	if (ioctl(spidev_fd, SPI_IOC_WR_MODE, &tmp) < 0)
		goto out_rath;

	/* MSB First */
	tmp = 0;
	if (ioctl(spidev_fd, SPI_IOC_WR_LSB_FIRST, &tmp) < 0)
		goto out_rath;

	/* 8 bits per word */
	tmp = 8;
	if (ioctl(spidev_fd, SPI_IOC_WR_BITS_PER_WORD, &tmp) < 0)
		goto out_rath;

	/* 1 MHz */
	tmp = 1e6;
	if (ioctl(spidev_fd, SPI_IOC_WR_MAX_SPEED_HZ, &tmp) < 0)
		goto out_rath;

	/* turn on rc632 */
	rh->ah = rc632_open(rath);
	if (!rh->ah)
		goto out_rath;

	/* everything is ok, returning reader handler */
	return rh;
out_rath:
	free(rath);
out_rh:
	free(rh);
out_close_spi:
	close(spidev_fd);
	return NULL;
}

static void spidev_close(struct rfid_reader_handle *rh)
{
	struct rfid_asic_transport_handle *rath = rh->ah->rath;

	if (rh->ah)
		rc632_close(rh->ah);

	if (spidev_fd > 0)
		close(spidev_fd);

	if (rath)
		free(rath);

	if (rh)
		free(rh);
}

struct rfid_reader rfid_reader_spidev = {
	.name = "spidev reader",
	.id = RFID_READER_SPIDEV,
	.open = &spidev_open,
	.close = &spidev_close,
	.transceive = &spidev_transceive,
	.l2_supported = (1 << RFID_LAYER2_ISO14443A) |
			(1 << RFID_LAYER2_ISO14443B) |
			(1 << RFID_LAYER2_ISO15693),
	.proto_supported = (1 << RFID_PROTOCOL_TCL) |
			   (1 << RFID_PROTOCOL_MIFARE_UL) |
			   (1 << RFID_PROTOCOL_MIFARE_CLASSIC),
	.iso14443a = {
		      .init = &spidev_14443a_init,
		      .transceive_sf = &spidev_transceive_sf,
		      .transceive_acf = &spidev_transceive_acf,
		      .speed = RFID_14443A_SPEED_106K
		      | RFID_14443A_SPEED_212K | RFID_14443A_SPEED_424K,
		      .set_speed = &spidev_14443a_set_speed,
	},
	.iso14443b = {
		      .init = &spidev_14443b_init,
	},
	.mifare_classic = {
		.setkey = &spidev_mifare_setkey,
		.auth = &spidev_mifare_auth,
	},
};

