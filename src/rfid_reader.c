/* librfid - core reader handling
 * (C) 2005-2006 by Harald Welte <laforge@gnumonks.org>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdlib.h>
#include <stdio.h>

#include <librfid/rfid.h>
#include <librfid/rfid_reader.h>
#include <librfid/rfid_reader_cm5121.h>
#include <librfid/rfid_reader_openpcd.h>
#include <librfid/rfid_reader_spidev.h>

static const struct rfid_reader *rfid_readers[] = {
#ifdef HAVE_LIBUSB
#ifdef ENABLE_CM5121
	[RFID_READER_CM5121]	= &rfid_reader_cm5121,
#endif
	[RFID_READER_OPENPCD]	= &rfid_reader_openpcd,
#endif
#ifdef ENABLE_SPIDEV
	[RFID_READER_SPIDEV]	= &rfid_reader_spidev,
#endif
};

struct rfid_reader_handle *
rfid_reader_open(void *data, unsigned int id)
{
	const struct rfid_reader *p;

	if (id >= ARRAY_SIZE(rfid_readers)) {
		DEBUGP("unable to find matching reader\n");
		return NULL;
	}

	p = rfid_readers[id];
	if (!p)
		return NULL;

	return p->open(data);
}

int
rfid_reader_transceive(struct rfid_reader_handle *rh,
			enum rfid_frametype frametype,
			 const unsigned char *tx_buf, unsigned int len,
			 unsigned char *rx_buf, unsigned int *rx_len,
			 u_int64_t timeout, unsigned int flags)
{
	return rh->reader->transceive(rh, frametype, tx_buf, len, rx_buf,
				      rx_len, timeout, flags);
}

void
rfid_reader_close(struct rfid_reader_handle *rh)
{
	rh->reader->close(rh);
}

int
rfid_reader_getopt(struct rfid_reader_handle *rh, int optname,
		   void *optval, unsigned int *optlen)
{
	return rh->reader->getopt(rh, optname, optval, optlen);
}

int rfid_reader_setopt(struct rfid_reader_handle *rh, int optname,
		       const void *optval, unsigned int optlen)
{
	return rh->reader->setopt(rh, optname, optval, optlen);
}
