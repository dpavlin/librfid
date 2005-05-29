#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <openct/openct.h>

#include <rfid/rfid.h>
#include <rfid/rfid_reader.h>
#include <rfid/rfid_layer2.h>
#include <rfid/rfid_protocol.h>
#include <rfid/rfid_reader_cm5121.h>

static int slot = 1;
static ct_handle *h;
static ct_lock_handle lock;

static struct rfid_reader_handle *rh;
static struct rfid_layer2_handle *l2h;
static struct rfid_protocol_handle *ph;


/* this is the sole function required by rfid_reader_cm5121.c */
int 
PC_to_RDR_Escape(void *handle, 
		 const unsigned char *tx_buf, unsigned int tx_len,
		 unsigned char *rx_buf, unsigned int *rx_len)
{
	ct_handle *h = (ct_handle *) handle;
	int rc;

	rc = ct_card_transact(h, 1, tx_buf, tx_len, rx_buf, *rx_len);
	if (rc >= 0) {
		*rx_len = rc;
		return 0;
	}

	return rc;
}



static int init()
{
	unsigned char buf[0x3f];
	unsigned char atr[64];
	unsigned 
	int rc;

	h = ct_reader_connect(0);
	if (!h)
		return -1;

	printf("acquiring card lock\n");
	rc = ct_card_lock(h, slot, IFD_LOCK_EXCLUSIVE, &lock);
	if (rc < 0) {
		fprintf(stderr, "error, no card lock\n");
		return -1;
	}

	rc = ct_card_reset(h, slot, atr, sizeof(atr));
	if (rc < 0) {
		fprintf(stderr, "error, can't reset virtual card\n");
		return -1;
	}

	rfid_init();

	printf("opening reader handle\n");
	rh = rfid_reader_open(h, RFID_READER_CM5121);
	if (!rh) {
		fprintf(stderr, "error, no cm5121 handle\n");
		return -1;
	}

	printf("opening layer2 handle\n");
	//l2h = rfid_layer2_init(rh, RFID_LAYER2_ISO14443A);
	l2h = rfid_layer2_init(rh, RFID_LAYER2_ISO14443B);
	if (!l2h) {
		fprintf(stderr, "error during iso14443a_init\n");
		return -1;
	}

	//rc632_register_dump(rh->ah, buf);

	printf("running layer2 anticol\n");
	rc = rfid_layer2_open(l2h);
	if (rc < 0) {
		fprintf(stderr, "error during layer2_open\n");
		return rc;
	}

	printf("running layer3 (ats)\n");
	ph = rfid_protocol_init(l2h, RFID_PROTOCOL_TCL);
	if (!ph) {
		fprintf(stderr, "error during protocol_init\n");
		return -1;
	}
	if (rfid_protocol_open(ph) < 0) {
		fprintf(stderr, "error during protocol_open\n");
		return -1;
	}

	return 0;
}


int main(int argc, char **argv)
{
	int rc;
	char buf[0x40];

	if (init() < 0)
		exit(1);

	/* we've established T=CL at this point */
 
	rfid_reader_close(rh);
	
	exit(0);
}
