/* CM5121 backend for 'internal' CCID driver */
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <librfid/rfid.h>

#ifndef LIBRFID_FIRMWARE

#include <librfid/rfid_asic.h>

#include "ccid-driver.h"

/* this is the sole function required by rfid_reader_cm5121.c */
int 
PC_to_RDR_Escape(void *handle, 
		 const unsigned char *tx_buf, size_t tx_len,
		 unsigned char *rx_buf, size_t *rx_len)
{
	int rc;
        ccid_driver_t ch = handle;
        size_t maxrxlen = *rx_len;

        rc = ccid_transceive_escape (ch, tx_buf, tx_len,
                                     rx_buf, maxrxlen, rx_len);

	return rc;
}

int cm5121_source_init(struct rfid_asic_transport_handle *rath)
{
	int rc;

        rc = ccid_open_reader(&rath->data, NULL);
        if (rc) {
                fprintf (stderr, "failed to open CCID reader: %#x\n", rc);
                return -1;
        }
	return 0;
}

#endif /* LIBRFID_FIRMWARE */
