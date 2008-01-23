/* RFID scanning implementation
 *
 * (C) 2006 by Harald Welte <laforge@gnumonks.org>
 *
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

#include <librfid/rfid.h>
#include <librfid/rfid_layer2.h>
#include <librfid/rfid_reader.h>
#include <librfid/rfid_protocol.h>

static struct rfid_layer2_handle *
rfid_layer2_scan1(struct rfid_reader_handle *rh, int l2)
{
	struct rfid_layer2_handle *l2h;

	if (rh->reader->l2_supported & (1 << l2)) {
		l2h = rfid_layer2_init(rh, l2);
		if (!l2h)
			return NULL;
		if (rfid_layer2_open(l2h) < 0) {
			rfid_layer2_fini(l2h);
			return NULL;
		} else 
			return l2h;
	}

	return NULL;
}

struct rfid_layer2_handle *
rfid_layer2_scan(struct rfid_reader_handle *rh)
{
	struct rfid_layer2_handle *l2h;
	int i;

#define RFID_LAYER2_MAX 16
	for (i = 0; i < RFID_LAYER2_MAX; i++) {
		DEBUGP("testing l2 %u\n", i);
		l2h = rfid_layer2_scan1(rh, i);
		if (l2h)
			return l2h;
	}

	return NULL;
}

static struct rfid_protocol_handle *
rfid_protocol_scan1(struct rfid_layer2_handle *l2h, int proto)
{
	struct rfid_protocol_handle *ph;

	if (l2h->rh->reader->proto_supported & (1 << proto) &&
	    l2h->proto_supported & (1 << proto)) {
		ph = rfid_protocol_init(l2h, proto);
		if (!ph)
			return NULL;
		if (rfid_protocol_open(ph) < 0) {
			rfid_protocol_fini(ph);
			return NULL;
		} else
			return ph;
	}

	return NULL;
}

struct rfid_protocol_handle *
rfid_protocol_scan(struct rfid_layer2_handle *l2h)
{
	struct rfid_protocol_handle *ph;
	int i;

#define RFID_PROTOCOL_MAX 16
	for (i = 0; i < RFID_PROTOCOL_MAX; i++) {
		DEBUGP("testing proto %u\n", i);
		ph = rfid_protocol_scan1(l2h, i);
		if (ph)
			return ph;
	}

	return NULL;
}

/* Scan for any RFID transponder within range of the given reader.
 * Abort after the first successfully found transponder. */
int rfid_scan(struct rfid_reader_handle *rh,
	      struct rfid_layer2_handle **l2h,
	      struct rfid_protocol_handle **ph)
{
	*l2h = rfid_layer2_scan(rh);
	if (!*l2h)
		return 0;
	
	*ph = rfid_protocol_scan(*l2h);
	if (!*ph)
		return 2;

	return 3;
}
