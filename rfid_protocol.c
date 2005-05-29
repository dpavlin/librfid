/* librfid - layer 3 protocol handler 
 * (C) 2005 by Harald Welte <laforge@gnumonks.org>
 */

#include <stdlib.h>
#include <unistd.h>

#include <rfid/rfid_layer2.h>
#include <rfid/rfid_protocol.h>

static struct rfid_protocol *rfid_protocol_list;

struct rfid_protocol_handle *
rfid_protocol_init(struct rfid_layer2_handle *l2h, unsigned int id)
{
	struct rfid_protocol *p;

	for (p = rfid_protocol_list; p; p = p->next)
		if (p->id == id)
			return p->fn.init(l2h);

	return NULL;
}

int
rfid_protocol_open(struct rfid_protocol_handle *ph)
{
	return ph->proto->fn.open(ph);
}

int
rfid_protocol_transcieve(struct rfid_protocol_handle *ph,
			 const unsigned char *tx_buf, unsigned int len,
			 unsigned char *rx_buf, unsigned int *rx_len,
			 unsigned int timeout, unsigned int flags)
{
	return ph->proto->fn.transcieve(ph, tx_buf, len, rx_buf, rx_len,
					timeout, flags);
}

int rfid_protocol_fini(struct rfid_protocol_handle *ph)
{
	return ph->proto->fn.fini(ph);
}

int
rfid_protocol_close(struct rfid_protocol_handle *ph)
{
	return ph->proto->fn.close(ph);
}

int
rfid_protocol_register(struct rfid_protocol *p)
{
	p->next = rfid_protocol_list;
	rfid_protocol_list = p;

	return 0;
}
