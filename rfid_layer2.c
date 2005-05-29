/* librfid - layer 2 protocol handler 
 * (C) 2005 by Harald Welte <laforge@gnumonks.org>
 */
#include <stdlib.h>
#include <stdio.h>

#include <rfid/rfid.h>
#include <rfid/rfid_layer2.h>

static struct rfid_layer2 *rfid_layer2_list;

struct rfid_layer2_handle *
rfid_layer2_init(struct rfid_reader_handle *rh, unsigned int id)
{
	struct rfid_layer2 *p;

	for (p = rfid_layer2_list; p; p = p->next)
		if (p->id == id)
			return p->fn.init(rh);

	DEBUGP("unable to find matching layer2 protocol\n");
	return NULL;
}

int
rfid_layer2_open(struct rfid_layer2_handle *ph)
{
	return ph->l2->fn.open(ph);
}

int
rfid_layer2_transcieve(struct rfid_layer2_handle *ph,
			 const unsigned char *tx_buf, unsigned int len,
			 unsigned char *rx_buf, unsigned int *rx_len,
			 unsigned int timeout, unsigned int flags)
{
	return ph->l2->fn.transcieve(ph, tx_buf, len, rx_buf, rx_len,
					timeout, flags);
}

int rfid_layer2_fini(struct rfid_layer2_handle *ph)
{
	return ph->l2->fn.fini(ph);
}

int
rfid_layer2_close(struct rfid_layer2_handle *ph)
{
	return ph->l2->fn.close(ph);
}

int
rfid_layer2_register(struct rfid_layer2 *p)
{
	p->next = rfid_layer2_list;
	rfid_layer2_list = p;

	return 0;
}
