/* CM5121 backend for OpenCT virtual slot */

#include <stdio.h>

#include <librfid/rfid_asic.h>
#include <openct/openct.h>

/* FIXME: get rid of this global crap.  In fact this needs to become part of
 * struct rfid_reader_handle  */
static ct_lock_handle lock;
static int slot = 1;

/* this is the sole function required by rfid_reader_cm5121.c */
int 
PC_to_RDR_Escape(void *handle, 
		 const unsigned char *tx_buf, size_t tx_len,
		 unsigned char *rx_buf, size_t *rx_len)
{
	int rc;
	ct_handle *h = (ct_handle *) handle;

	rc = ct_card_transact(h, 1, tx_buf, tx_len, rx_buf, *rx_len);
	if (rc >= 0) {
		*rx_len = rc;
		return 0;
	}

	return rc;
}


int cm5121_source_init(struct rfid_asic_transport_handle *rath)
{
	struct ct_handle *h;
	int rc;
	unsigned char atr[64];

	h = ct_reader_connect(0);
	if (!h)
		return -1;

	rc = ct_card_lock(h, slot, IFD_LOCK_EXCLUSIVE, &lock);
	if (rc < 0)
		return -1;

	rc = ct_card_reset(h, slot, atr, sizeof(atr));
	if (rc < 0)
		return -1;

	rath->data = h;

	return 0;
}

