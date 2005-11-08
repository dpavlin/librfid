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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <usb.h>
#include "pegoda.h"

const char *
rfid_hexdump(const void *data, unsigned int len)
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


struct usb_device *find_device(u_int16_t vendor, u_int16_t device)
{
	struct usb_bus *bus;

	for (bus = usb_get_busses(); bus; bus = bus->next) {
		struct usb_device *dev;
		for (dev = bus->devices; dev; dev = dev->next) {
			printf("vend 0x%x dev 0x%x\n",
				dev->descriptor.idVendor, dev->descriptor.idProduct);
			if (dev->descriptor.idVendor == vendor &&
			    dev->descriptor.idProduct == device) {
				return dev;
			}
		}
	}
	return NULL;
}

static unsigned char seq = 0x00;
static struct usb_dev_handle *pegoda_handle;

int pegoda_transcieve(u_int8_t cmd, unsigned char *tx, unsigned int tx_len,
		      unsigned char *rx, unsigned int *rx_len)
{
	unsigned char txbuf[256];
	unsigned char rxbuf[256];
	int rc;
	unsigned int len_expected;
	struct pegoda_cmd_hdr *hdr = txbuf;

	hdr->seq = ++seq;
	hdr->cmd = cmd;
	hdr->len = htons(tx_len);
	memcpy(txbuf + sizeof(*hdr), tx, tx_len);

	printf("tx [%u]: %s\n", tx_len+sizeof(*hdr), rfid_hexdump(txbuf, tx_len + sizeof(*hdr)));
	rc = usb_bulk_write(pegoda_handle, 0x02, (char *)txbuf,
			    tx_len + sizeof(*hdr), 0);
	if (rc < 0)
		return rc;

	rc = usb_bulk_read(pegoda_handle, 0x81, (char *)rxbuf, sizeof(rxbuf), 0);
	if (rc <= 0)
		return rc;

	if (rc != 2) {
		fprintf(stderr, "unexpected: received %u bytes as length?\n");
		return -EIO;
	}
	printf("len [%u]: %s\n", rc, rfid_hexdump(rxbuf, rc));

	len_expected = rxbuf[0];

	if (len_expected > sizeof(rxbuf))
		return -EIO;

	rc = usb_bulk_read(pegoda_handle, 0x81, (char *)rxbuf, len_expected, 0);
	if (rc <= 0)
		return rc;
	printf("rx [%u]: %s\n", rc, rfid_hexdump(rxbuf, rc));

	memcpy(rx, rxbuf+1, rc-1);
	*rx_len = rc - 1;

	return 0;
}

/* Transform crypto1 key from generic 6byte into rc632 specific 12byte */
static int
rc632_mifare_transform_key(const u_int8_t *key6, u_int8_t *key12)
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


int main(int argc, char **argv)
{
	struct usb_device *pegoda;
	unsigned char buf[256];
	unsigned char rbuf[256];
	unsigned int rlen = sizeof(rbuf);
	unsigned char snr[4];

	usb_init();
	usb_find_busses();
	usb_find_devices();

	pegoda = find_device(USB_VENDOR_PHILIPS, USB_DEVICE_PEGODA);

	if (!pegoda)
		exit(2);

	printf("found pegoda, %u configurations\n",
		pegoda->descriptor.bNumConfigurations);

	printf("config 2 [nr %u] has %u interfaces\n",
		pegoda->config[1].bConfigurationValue,
		pegoda->config[1].bNumInterfaces);

	printf("config 2 interface 0 has %u altsettings\n",
		pegoda->config[1].interface[0].num_altsetting);

	pegoda_handle = usb_open(pegoda);
	if (!pegoda_handle)
		exit(1);

	if (usb_set_configuration(pegoda_handle, 2))
		exit(1);

	printf("configuration 2 successfully set\n");

	if (usb_claim_interface(pegoda_handle, 0))
		exit(1);

	printf("interface 0 claimed\n");

	if (usb_set_altinterface(pegoda_handle, 1))
		exit(1);

	printf("alt setting 1 selected\n");

	pegoda_transcieve(PEGODA_CMD_PCD_CONFIG, NULL, 0, rbuf, &rlen);

	buf[0] = 0x26;
	rlen = sizeof(rbuf);
	pegoda_transcieve(PEGODA_CMD_PICC_COMMON_REQUEST, buf, 1, rbuf, &rlen);

	buf[0] = 0x93;
	memset(buf+1, 0, 5);
	rlen = sizeof(rbuf);
	pegoda_transcieve(PEGODA_CMD_PICC_CASC_ANTICOLL, buf, 6, rbuf, &rlen);

	memcpy(snr, rbuf+3, 4);

	buf[0] = 0x93;
	memcpy(buf+1, snr, 4);
	rlen = sizeof(rbuf);
	pegoda_transcieve(PEGODA_CMD_PICC_CASC_SELECT, buf, 5, rbuf, &rlen);
	
	buf[0] = 0x60;
#if 0
	buf[1] = 0x00;	/* key number */
	buf[2] = 0x00;	/* sector */
	rlen = sizeof(rbuf);
	pegoda_transcieve(PEGODA_CMD_PICC_AUTH, buf, 3, rbuf, &rlen);
#else
	memcpy(buf+1, snr, 4);
	{ 
		u_int8_t key6[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
		//u_int8_t key6[6] = { 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6 };
		u_int8_t key12[12];

		rc632_mifare_transform_key(key6, key12);

		memcpy(buf+5, key12, 12);
		buf[17] = 0x00; /* sector */
	}
	pegoda_transcieve(PEGODA_CMD_PICC_AUTH_KEY, buf, 18, rbuf, &rlen);
#endif

	buf[0] = 0x00; /* sector */
	pegoda_transcieve(PEGODA_CMD_PICC_READ, buf, 1, rbuf, &rlen);

	exit(0);
}
