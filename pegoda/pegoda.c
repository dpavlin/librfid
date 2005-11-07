#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <usb.h>

#define USB_VENDOR_PHILIPS	0x0742
#define USB_DEVICE_PEGODA	0xff01

/* header of a pegoda usb command packet */
struct pegoda_cmd_hdr {
	u_int8_t seq;
	u_int8_t cmd;
	u_int16_t len;
};

enum {
	PEGODA_CMD_WRITE_RC			= 0x01,
	PEGODA_CMD_READ_RC			= 0x02,
	PEGODA_CMD_EXCHANGE_BYTESTREAM		= 0x03,
	PEGODA_CMD_WRITE_MULTIPLE		= 0x04,
	PEGODA_CMD_READ_MULTIPLE		= 0x05,
	
	PEGODA_CMD_PCD_CONFIG			= 0x10,
	PEGODA_CMD_PICC_REQUEST			= 0x11,
	PEGODA_CMD_PICC_ANTICOLL		= 0x12,
	PEGODA_CMD_PICC_SELECT			= 0x13,
	PEGODA_CMD_PICC_AUTH			= 0x14,
	PEGODA_CMD_PICC_AUTH_E2			= 0x15,
	PEGODA_CMD_LOAD_KEY_E2			= 0x17,
	PEGODA_CMD_PICC_AUTH_KEY		= 0x18,
	PEGODA_CMD_PICC_READ			= 0x19,
	PEGODA_CMD_PICC_WRITE			= 0x1a,
	PEGODA_CMD_PICC_VALUE			= 0x1b,
	PEGODA_CMD_PICC_VALUE_DEBIT		= 0x1c,
	PEGODA_CMD_PICC_HALT			= 0x1d,
	PEGODA_CMD_PICC_WRITE4			= 0x1e,
	PEGODA_CMD_PICC_COMMON_WRITE		= 0x1f,

	PEGODA_CMD_PCD_RF_RESET			= 0x21,
	PEGODA_CMD_PCD_RESET			= 0x21,
	PEGODA_CMD_PCD_GET_SNR			= 0x22,
	PEGODA_CMD_PCD_READ_E2			= 0x23,
	PEGODA_CMD_PCD_SET_TMO			= 0x27,
	PEGODA_CMD_PICC_COMMON_READ		= 0x28,
	PEGODA_CMD_ACTIVE_ANTENNA_MASTER	= 0x2a,
	PEGODA_CMD_ACTIVE_ANTENNA_SLAVE		= 0x2b,

	PEGODA_CMD_PICC_COMMON_REQUEST		= 0x40,
	PEGODA_CMD_PICC_CASC_ANTICOLL		= 0x41,
	PEGODA_CMD_PICC_CASC_SELECT		= 0x42,
	PEGODA_CMD_PICC_ACTIVATE_IDLE		= 0x43,
	PEGODA_CMD_ACTIVATE_WAKEUP		= 0x44,

	PEGODA_CMD_PICC_ACTIVATE_WAKEUP		= 0x44,
	PEGODA_CMD_PCD_SET_DEFAULT_ATTRIB	= 0x45,
	PEGODA_CMD_PCD_SET_ATTRIB		= 0x46,
	PEGODA_CMD_PCD_GET_ATTRIB		= 0x47,
	PEGODA_CMD_PICC_EXCHANGE_BLOCK		= 0x48,
	PEGODA_CMD_PICC_ACTIVATE_IDLE_LOOP	= 0x49,
	PEGODA_CMD_PICC_ACTTIVATION		= 0x4a,


	PEGODA_CMD_GET_FW_VERSION		= 0x63,
	PEGODA_CMD_GET_RIC_VERSION		= 0x64,
	PEGODA_CMD_ENABLE_DEBUG_PINS		= 0x65,
};

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
