/* librfid - core reader handling
 * (C) 2005 by Harald Welte <laforge@gnumonks.org>
 */
#include <stdlib.h>
#include <stdio.h>

#include <rfid/rfid.h>
#include <rfid/rfid_reader.h>

static struct rfid_reader *rfid_reader_list;

struct rfid_reader_handle *
rfid_reader_open(void *data, unsigned int id)
{
	struct rfid_reader *p;

	for (p = rfid_reader_list; p; p = p->next)
		if (p->id == id)
			return p->open(data);

	DEBUGP("unable to find matching reader\n");
	return NULL;
}

int
rfid_reader_transcieve(struct rfid_reader_handle *rh,
			 const unsigned char *tx_buf, unsigned int len,
			 unsigned char *rx_buf, unsigned int *rx_len,
			 unsigned int timeout, unsigned int flags)
{
	return rh->reader->transcieve(rh, tx_buf, len, rx_buf, rx_len,
					timeout, flags);
}

int
rfid_reader_close(struct rfid_reader_handle *rh)
{
	return rh->reader->close(rh);
}

int
rfid_reader_register(struct rfid_reader *r)
{
	r->next = rfid_reader_list;
	rfid_reader_list = r;

	return 0;
}
