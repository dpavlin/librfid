/*
 *  (C) 2005 by Harald Welte <laforge@gnumonks.org>
 *
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
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <usb.h>
#include "pegoda.h"

const char *
hexdump(const void *data, unsigned int len)
{
	static char string[1024];
	unsigned char *d = (unsigned char *) data;
	unsigned int i, left;

	string[0] = '\0';
	left = sizeof(string);
	for (i = 0; len--; i += 3) {
		if (i >= sizeof(string) -4)
			break;
		snprintf(string+i, 4, " %02x", *d++);
	}
	return string;
}

struct pegoda_handle {
	struct usb_dev_handle *handle;
	unsigned char seq;
	unsigned char snr[4];
};


struct usb_device *find_device(u_int16_t vendor, u_int16_t device)
{
	struct usb_bus *bus;

	for (bus = usb_get_busses(); bus; bus = bus->next) {
		struct usb_device *dev;
		for (dev = bus->devices; dev; dev = dev->next) {
			if (dev->descriptor.idVendor == vendor &&
			    dev->descriptor.idProduct == device) {
				return dev;
			}
		}
	}
	return NULL;
}

int pegoda_transceive(struct pegoda_handle *ph,
		      u_int8_t cmd, unsigned char *tx, unsigned int tx_len,
		      unsigned char *rx, unsigned int *rx_len)
{
	unsigned char txbuf[256];
	unsigned char rxbuf[256];
	int rc;
	unsigned int len_expected;
	struct pegoda_cmd_hdr *hdr = (struct pegoda_cmd_hdr *)txbuf;
	struct pegoda_cmd_hdr *rxhdr = (struct pegoda_cmd_hdr *)rxbuf;

	hdr->seq = ++(ph->seq);
	hdr->cmd = cmd;
	hdr->len = htons(tx_len);
	memcpy(txbuf + sizeof(*hdr), tx, tx_len);

	printf("tx [%u]: %s\n", tx_len+sizeof(*hdr),
		hexdump(txbuf, tx_len + sizeof(*hdr)));
	rc = usb_bulk_write(ph->handle, 0x02, (char *)txbuf,
			    tx_len + sizeof(*hdr), 0);
	if (rc < 0)
		return rc;

	rc = usb_bulk_read(ph->handle, 0x81, (char *)rxbuf, sizeof(rxbuf), 0);
	if (rc <= 0)
		return rc;

	if (rc != 2) {
		fprintf(stderr, "unexpected: received %u bytes as length?\n");
		return -EIO;
	}
	printf("len [%u]: %s\n", rc, hexdump(rxbuf, rc));

	len_expected = rxbuf[0];

	if (len_expected > sizeof(rxbuf))
		return -EIO;

	rc = usb_bulk_read(ph->handle, 0x81, (char *)rxbuf, len_expected, 0);
	if (rc <= 0)
		return rc;
	printf("rx [%u]: %s\n", rc, hexdump(rxbuf, rc));

	if (rc < 4)
		return -EIO;

	if (rxhdr->seq != hdr->seq)
		return -EIO;

	*rx_len = ntohs(rxhdr->len);

	memcpy(rx, rxbuf+sizeof(*rxhdr), rc-sizeof(*rxhdr));

	return rxhdr->cmd;
}

struct pegoda_handle *pegoda_open(void)
{
	struct usb_device *pegoda;
	unsigned char rbuf[16];
	unsigned int rlen = sizeof(rbuf);
	struct pegoda_handle *ph;

	usb_init();
	usb_find_busses();
	usb_find_devices();

	pegoda = find_device(USB_VENDOR_PHILIPS, USB_DEVICE_PEGODA);

	if (!pegoda)
		return NULL;

	ph = malloc(sizeof(*ph));
	if (!ph)
		return NULL;
	memset(ph, 0, sizeof(*ph));

	printf("found pegoda, %u configurations\n",
		pegoda->descriptor.bNumConfigurations);

	printf("config 2 [nr %u] has %u interfaces\n",
		pegoda->config[1].bConfigurationValue,
		pegoda->config[1].bNumInterfaces);

	printf("config 2 interface 0 has %u altsettings\n",
		pegoda->config[1].interface[0].num_altsetting);

	ph->handle = usb_open(pegoda);
	if (!ph->handle) 
		goto out_free;

	if (usb_set_configuration(ph->handle, 2))
		goto out_free;

	printf("configuration 2 successfully set\n");

	if (usb_claim_interface(ph->handle, 0))
		goto out_free;

	printf("interface 0 claimed\n");

	if (usb_set_altinterface(ph->handle, 1))
		goto out_free;

	printf("alt setting 1 selected\n");

	pegoda_transceive(ph, PEGODA_CMD_PCD_CONFIG, NULL, 0, rbuf, &rlen);

	return ph;
out_free:
	free(ph);
	return NULL;
}

/* Transform crypto1 key from generic 6byte into rc632 specific 12byte */
static int
mifare_transform_key(const u_int8_t *key6, u_int8_t *key12)
{
	int i;
	u_int8_t ln;
	u_int8_t hn;

	for (i = 0; i < 6; i++) {
		ln = key6[i] & 0x0f;
		hn = key6[i] >> 4;
		key12[i * 2 + 1] = (~ln << 4) | ln;
		key12[i * 2] = (~hn << 4) | hn;
	}
	return 0;
}

static int pegoda_auth_e2(struct pegoda_handle *ph,
			  u_int8_t keynr, u_int8_t sector)
{
	unsigned char buf[3];
	unsigned char rbuf[16];
	unsigned int rlen = sizeof(rbuf);

	buf[0] = 0x60;
	buf[1] = keynr;		/* key number */
	buf[2] = sector;	/* sector */
	rlen = sizeof(rbuf);
	pegoda_transceive(ph, PEGODA_CMD_PICC_AUTH, buf, 3, rbuf, &rlen);

	/* FIXME: check response */

	return 0;
}

static int pegoda_auth_key(struct pegoda_handle *ph,
			   u_int8_t sector, const unsigned char *key6)
{
	unsigned char buf[1+4+12+1];
	unsigned char rbuf[16];
	unsigned int rlen = sizeof(rbuf);

	buf[0] = 0x60;
	memcpy(buf+1, ph->snr, 4);
	mifare_transform_key(key6, buf+5);
	buf[17] = sector;

	pegoda_transceive(ph, PEGODA_CMD_PICC_AUTH_KEY, buf, 18, rbuf, &rlen);

	/* FIXME: check response */

	return 0;
}

static int pegoda_read16(struct pegoda_handle *ph,
			 u_int8_t page, unsigned char *rx)
{
	int rc;
	unsigned int rlen = 16;

	rc = pegoda_transceive(ph, PEGODA_CMD_PICC_READ,
				&page, 1, rx, &rlen);
	if (rlen != 16)
		return -EIO;

	return 0;
}

int main(int argc, char **argv)
{
	unsigned char buf[256];
	unsigned char rbuf[256];
	unsigned int rlen = sizeof(rbuf);
	struct pegoda_handle *ph;
	int i;

	ph = pegoda_open();
	if (!ph)
		exit(1);

	/* LED off */
	buf[0] = 0x00;
	rlen = sizeof(rbuf);
	pegoda_transceive(ph, PEGODA_CMD_SWITCH_LED, buf, 1, rbuf, &rlen);

	/* anticollision */

	buf[0] = 0x26;
	rlen = sizeof(rbuf);
	pegoda_transceive(ph, PEGODA_CMD_PICC_COMMON_REQUEST, 
			  buf, 1, rbuf, &rlen);

	buf[0] = 0x93;
	memset(buf+1, 0, 5);
	rlen = sizeof(rbuf);
	pegoda_transceive(ph, PEGODA_CMD_PICC_CASC_ANTICOLL, 
			  buf, 6, rbuf, &rlen);

	memcpy(ph->snr, rbuf, 4);

	buf[0] = 0x93;
	memcpy(buf+1, ph->snr, 4);
	rlen = sizeof(rbuf);
	pegoda_transceive(ph, PEGODA_CMD_PICC_CASC_SELECT, 
			  buf, 5, rbuf, &rlen);

	for (i = 0; i < 16; i++) {
		int j;
		pegoda_auth_key(ph, i, "\xff\xff\xff\xff\xff\xff");
		for (j = 0; j < 4; j++) {
			pegoda_read16(ph, (i*4)+j, rbuf);
			printf("read16[%u:%u] = %s\n", i,j,hexdump(rbuf, 16));
		}
	}
	
	exit(0);
}
