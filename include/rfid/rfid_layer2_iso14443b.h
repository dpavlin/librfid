#ifndef _RFID_LAYER2_ISO14443B_H
#define _RFID_LAYER2_ISO14443B_H

struct iso14443b_atqb {
	unsigned char fifty;
	unsigned char pupi[4];
	unsigned char app_data[4];
	struct {
		unsigned char bit_rate_capability;
		unsigned char protocol_type:4,
			      max_frame_size:4;
		unsigned char fo:2,
			      adc:2,
			      fwi:4;
	} protocol_info;
};

struct iso14443b_attrib_hdr {
	unsigned char one_d;
	unsigned char identifier[4];
	struct {
		unsigned char rfu:2,
			      sof:1,
			      eof:1,
			      min_tr1:2,
			      min_tr0:2;
	} param1;
	struct {
		unsigned char fsdi:4,
			      spd_out:2,
			      spd_in:2;
	} param2;
	struct {
		unsigned char protocol_type:4,
			      rfu:4;
	} param3;
	struct {
		unsigned char cid:4,
			      rfu:4;
	} param4;
};

struct iso14443b_handle {
	unsigned int tcl_capable; /* do we support T=CL */

	unsigned int cid;	/* Card ID */

	unsigned int fsc;	/* max. frame size card */
	unsigned int fsd;	/* max. frame size reader */

	unsigned int fwt;	/* frame waiting time (in usec) */

	unsigned int mbl;	/* maximum buffer length */

	unsigned int tr0;	/* pcd-eof to picc-subcarrier-on */
	unsigned int tr1;	/* picc-subcarrier-on to picc-sof */

	unsigned int flags;
	unsigned int state;
};

enum {
	ISO14443B_CID_SUPPORTED = 0x01,
	ISO14443B_NAD_SUPPORTED = 0x02,
};

enum {
	ISO14443B_STATE_ERROR,
	ISO14443B_STATE_NONE,
	ISO14443B_STATE_REQB_SENT,
	ISO14443B_STATE_ATQB_RCVD,
	ISO14443B_STATE_ATTRIB_SENT,
	ISO14443B_STATE_SELECTED,
	ISO14443B_STATE_HLTB_SENT,
	ISO14443B_STATE_HALTED,
};


#include <rfid/rfid_layer2.h>
struct rfid_layer2 rfid_layer2_iso14443b;

#endif
