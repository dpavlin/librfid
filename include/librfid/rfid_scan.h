#ifndef _RFID_SCAN_H
#define _RFID_SCAN_H

#include <librfid/rfid_reader.h>
#include <librfid/rfid_layer2.h>
#include <librfid/rfid_protocol.h>

struct rfid_layer2_handle *
rfid_layer2_scan(struct rfid_reader_handle *rh);

struct rfid_protocol_handle *
rfid_protocol_scan(struct rfid_layer2_handle *l2h);

int rfid_scan(struct rfid_reader_handle *rh,
	      struct rfid_layer2_handle **l2h,
	      struct rfid_protocol_handle **ph);

#endif /* _RFID_SCAN_H */
